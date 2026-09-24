#include "legion_leveller.h"

#include <algorithm>
#include <cmath>

namespace legion
{

namespace
{
    inline int nextPow2 (int v)
    {
        int p = 1;
        while (p < v) p <<= 1;
        return p;
    }

    constexpr double kPi = 3.14159265358979323846;
}

// ---------------------------------------------------------------------------
float Leveller::curveDb (const LevellerParams& p, float L)
{
    const float T = p.topDb;
    const float slope = 1.0f - 1.0f / std::max (1.0f, p.ratio);

    //  downward, soft knee centred on TOP
    const float over = L - T;
    float down = 0.0f;
    if (over >= 0.5f * kKneeDb)       down = -slope * over;
    else if (over > -0.5f * kKneeDb)
    {
        const float x = over + 0.5f * kKneeDb;
        down = -slope * x * x / (2.0f * kKneeDb);
    }

    //  upward: a 2:1 pull toward TOP, never more than LIFT, faded in over
    //  the 12 dB above FLOOR so the lift cannot reach the noise between
    //  phrases
    float up = 0.0f;
    if (L < T) up = std::min (std::max (0.0f, p.liftDb), (T - L) * (1.0f - 1.0f / kUpRatio));
    float u = (L - p.floorDb) / kFloorFade;
    u = std::min (1.0f, std::max (0.0f, u));
    up *= u * u * (3.0f - 2.0f * u);

    return down + up;
}

// ---------------------------------------------------------------------------
void Leveller::prepare (double sampleRate, int maxLatency)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    W   = std::max (8, (int) std::lround (kWindowMs * 0.001 * fs));
    //  two equal one-poles delay a slow envelope by 2 tau
    lag = (int) std::lround (2.0 * kDetTauMs * 0.001 * fs);

    //  RBJ high-pass, 80 Hz, Butterworth Q — in the DETECTOR only. Plosives
    //  and handling rumble should not pull the voice down.
    {
        const double w0 = 2.0 * kPi * 80.0 / fs, c = std::cos (w0), s = std::sin (w0);
        const double al = s / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + al;
        hpB0 = (1.0 + c) * 0.5 / a0;
        hpB1 = -(1.0 + c) / a0;
        hpB2 = (1.0 + c) * 0.5 / a0;
        hpA1 = -2.0 * c / a0;
        hpA2 = (1.0 - al) / a0;
    }
    aDet = 1.0 - std::exp (-1.0 / (kDetTauMs * 0.001 * fs));
    aAmt = 1.0f - (float) std::exp (-1.0 / (0.015 * fs));

    const int dq = nextPow2 (W + 4);
    dqVal.assign ((size_t) dq, 0.0f);
    dqIdx.assign ((size_t) dq, 0);
    dqMask = dq - 1;

    box.assign ((size_t) W, 0.0f);

    const int d = nextPow2 (std::max (maxLatency, 1) + 4);
    dly.assign ((size_t) d, 0.0f);
    dlyMask = d - 1;

    reset();
}

void Leveller::reset()
{
    hz1[0] = hz1[1] = hz2[0] = hz2[1] = 0.0;
    e1 = e2 = 0.0;
    dqHead = dqTail = tick = 0;
    rel = 0.0f;
    std::fill (box.begin(), box.end(), 0.0f);
    boxPos = 0; boxSum = 0.0; boxRecalc = 0;
    std::fill (dly.begin(), dly.end(), 0.0f);
    dlyPos = 0;
    meter.store (0.0f, std::memory_order_relaxed);
    //  `amount` survives a reset on purpose: it follows the switch, and a
    //  reset is not a reason to fade in again
}

// ---------------------------------------------------------------------------
void Leveller::process (const LevellerParams& p, const float* inL, const float* inR, int n,
                        int latency, float* const* bufs, int numBufs)
{
    if (box.empty()) return;

    const double relMs = 800.0 * std::pow (60.0 / 800.0, (double) std::min (1.0f, std::max (0.0f, p.speed)));
    const float aRel = (float) (1.0 - std::exp (-1.0 / (relMs * 0.001 * fs)));
    const float target = p.on ? 1.0f : 0.0f;

    //  where the gain for the bus sample leaving now was produced
    int E = latency - (W - 1) - lag;
    E = std::max (0, std::min (E, dlyMask - 1));

    const float invW = 1.0f / (float) W;
    const float k = 0.11512925464970228f;   // ln(10) / 20

    float g = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // ---- detector: high-passed, stereo-linked mean power, two poles --
        const double x[2] = { (double) inL[i], (double) inR[i] };
        double pw = 0.0;
        for (int c = 0; c < 2; ++c)
        {
            //  transposed direct form II
            const double y = hpB0 * x[c] + hz1[c];
            hz1[c] = hpB1 * x[c] - hpA1 * y + hz2[c];
            hz2[c] = hpB2 * x[c] - hpA2 * y;
            pw += y * y;
        }
        pw *= 0.5;
        e1 += aDet * (pw - e1);
        e2 += aDet * (e1 - e2);

        const float levelDb = (float) (10.0 * std::log10 (e2 + 1.0e-14));
        const float raw = curveDb (p, levelDb);

        // ---- sliding minimum over W ---------------------------------------
        while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] >= raw) --dqTail;
        dqVal[(size_t) (dqTail & dqMask)] = raw;
        dqIdx[(size_t) (dqTail & dqMask)] = tick;
        ++dqTail;
        while (dqIdx[(size_t) (dqHead & dqMask)] <= tick - W) ++dqHead;
        const float m = dqVal[(size_t) (dqHead & dqMask)];
        ++tick;

        // ---- drops pass straight on, rises go through the release --------
        rel = m < rel ? m : rel + aRel * (m - rel);

        // ---- the boxcar turns each drop into a ramp that ends on time ----
        boxSum += (double) rel - (double) box[(size_t) boxPos];
        box[(size_t) boxPos] = rel;
        if (++boxPos >= W) boxPos = 0;
        if (++boxRecalc >= 8192)
        {
            boxRecalc = 0;
            double s = 0.0;
            for (float b : box) s += (double) b;
            boxSum = s;
        }
        const float gs = (float) boxSum * invW;

        // ---- line up with the buses ---------------------------------------
        dly[(size_t) dlyPos] = gs;
        g = dly[(size_t) ((dlyPos - E) & dlyMask)];
        dlyPos = (dlyPos + 1) & dlyMask;

        // ---- apply ---------------------------------------------------------
        if (amount != target)
        {
            amount += aAmt * (target - amount);
            if (std::abs (amount - target) < 1.0e-5f) amount = target;
        }
        if (amount > 0.0f)
        {
            const float lin = std::exp (k * amount * g);
            for (int b = 0; b < numBufs; ++b) bufs[b][i] *= lin;
        }
    }

    meter.store (amount * g, std::memory_order_relaxed);
}

} // namespace legion
