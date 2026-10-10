#include "Engine.h"

#include <algorithm>
#include <cstring>

#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif

namespace mt
{

namespace
{
    inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }

    const char* SYNC_LABELS[NUM_SYNC] =
        { "OFF", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1/1", "2/1" };

    // Catmull-Rom segment through p0..p3, evaluated / integrated over t in [0,1].
    struct CR
    {
        double c0, c1, c2, c3;
        CR (double p0, double p1, double p2, double p3)
            : c0 (p1),
              c1 (0.5 * (p2 - p0)),
              c2 (0.5 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3)),
              c3 (0.5 * (3.0 * (p1 - p2) + p3 - p0)) {}
        double at (double t) const { return c0 + t * (c1 + t * (c2 + t * c3)); }
        double prim (double t) const   // antiderivative from 0
        {
            return t * (c0 + t * (c1 * 0.5 + t * (c2 / 3.0 + t * c3 * 0.25)));
        }
    };
}

const char* syncName (int i) { return (i >= 0 && i < NUM_SYNC) ? SYNC_LABELS[i] : "?"; }

//==============================================================================
void Biquad::shelf (bool high, double fs, double f0, double gainDb)
{
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * M_PI * f0 / fs;
    const double cs = std::cos (w0), sn = std::sin (w0);
    const double alpha = sn * 0.5 * std::sqrt (2.0);      // S = 1: steepest with no overshoot
    const double sa = 2.0 * std::sqrt (A) * alpha;
    double a0;
    if (! high)
    {
        b0 = A * ((A + 1) - (A - 1) * cs + sa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cs);
        b2 = A * ((A + 1) - (A - 1) * cs - sa);
        a0 = (A + 1) + (A - 1) * cs + sa;
        a1 = -2 * ((A - 1) + (A + 1) * cs);
        a2 = (A + 1) + (A - 1) * cs - sa;
    }
    else
    {
        b0 = A * ((A + 1) + (A - 1) * cs + sa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        b2 = A * ((A + 1) + (A - 1) * cs - sa);
        a0 = (A + 1) - (A - 1) * cs + sa;
        a1 = 2 * ((A - 1) - (A + 1) * cs);
        a2 = (A + 1) - (A - 1) * cs - sa;
    }
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
}

//==============================================================================
void HalfBand::design()
{
    constexpr int C = TAPS / 2;
    double g[TAPS] {}, sum = 0.0;
    for (int n = 0; n < TAPS; ++n)
    {
        const double t = n - C;
        const double sc = (t == 0.0) ? 0.5 : std::sin (M_PI * 0.5 * t) / (M_PI * t);
        const double wn = 0.42 - 0.5 * std::cos (2.0 * M_PI * n / (TAPS - 1))
                               + 0.08 * std::cos (4.0 * M_PI * n / (TAPS - 1));
        g[n] = sc * wn;
        sum += g[n];
    }
    for (auto& x : g) x /= sum;
    for (int k = 0; k < PH; ++k)
    {
        ge[k] = (2 * k     < TAPS) ? (float) g[2 * k]     : 0.0f;
        go[k] = (2 * k + 1 < TAPS) ? (float) g[2 * k + 1] : 0.0f;
    }
}

void HalfBand::clear()
{
    std::memset (upLine, 0, sizeof (upLine));
    std::memset (dnE, 0, sizeof (dnE));
    std::memset (dnO, 0, sizeof (dnO));
    upW = dnW = 0;
}

void HalfBand::up (float x, float& o0, float& o1)
{
    upLine[upW] = x;
    float s0 = 0.0f, s1 = 0.0f;
    for (int k = 0; k < PH; ++k)
    {
        const float val = upLine[(upW - k) & MASK];
        s0 += ge[k] * val;
        s1 += go[k] * val;
    }
    o0 = 2.0f * s0; o1 = 2.0f * s1;
    upW = (upW + 1) & MASK;
}

float HalfBand::down (float a, float b)
{
    dnE[dnW] = a; dnO[dnW] = b;
    float s = 0.0f;
    for (int k = 0; k < PH; ++k)
        s += ge[k] * dnE[(dnW - k) & MASK] + go[k] * dnO[(dnW - k - 1) & MASK];
    dnW = (dnW + 1) & MASK;
    return s;
}

//==============================================================================
Engine::Engine() = default;

Engine::~Engine()
{
    delete store;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
}

int64_t Engine::cellsNeeded (int m, double loopMeters) const
{
    if (m == 0) return (int64_t) std::llround (REEL_WINDOW_CM * CELLS_PER_CM);
    return (int64_t) std::llround (loopMeters * 100.0 * CELLS_PER_CM);
}

void Engine::prepare (double hostSampleRate, int)
{
    fsHost = hostSampleRate;
    os = fsHost < 80000.0 ? 2 : 1;
    fsInt = fsHost * os;
    dt = 1.0 / fsInt;

    for (auto& h : up) { h.design(); h.clear(); }
    for (auto& h : dn) { h.design(); h.clear(); }

    // The half-band pair's delay, MEASURED with an impulse rather than taken
    // from a comment: a symmetric FIR's centroid is its exact group delay.
    double hbDelay = 0.0;
    if (os == 2)
    {
        HalfBand u, d; u.design(); d.design(); u.clear(); d.clear();
        double num = 0.0, den = 0.0;
        for (int n = 0; n < 256; ++n)
        {
            float a, b;
            u.up (n == 0 ? 1.0f : 0.0f, a, b);
            const double y = d.down (a, b);
            num += n * y; den += y;
        }
        hbDelay = num / den;               // host samples
    }
    // + half an internal sample: a head's output is its average over the span it
    // crossed during the sample, which is centred half a sample in the past.
    // + one more for the 3-tap FIR that undoes that box (its centre tap).
    // Each half-band is a centred FIR, so the measured pair splits evenly: the
    // up half is paid at the record head (inLag), the down half here.
    inLag = (int) std::lround (0.5 * hbDelay * os);
    leadS = 0.5 * hbDelay / fsHost + 0.5 * dt + dt;
    {
        // Fit the FIR to 1/sinc at 16 kHz: within 0.05 dB of exact to 20 kHz at 96 kHz.
        const double x = 16000.0 * dt;
        const double inv = (M_PI * x) / std::sin (M_PI * x);
        const double a = (inv - 1.0) / (2.0 * (1.0 - std::cos (2.0 * M_PI * x)));
        for (auto& f : comp) { f.a = a; f.reset(); }
    }

    aGain  = 1.0 - std::exp (-dt / 0.020);
    aMotor = 1.0 - std::exp (-dt / MOTOR_TAU);
    aHost  = 1.0 - std::exp (-1.0 / (fsHost * 0.020));

    // A fresh tape for the transport as it stands.
    delete store; store = nullptr;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
    wantCells = 0;

    mode  = params.mode > 0.5f ? 1 : 0;
    loopM = std::clamp (std::round (params.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    pendingLoopM = loopM; loopSettle = 1.0;

    // Controls start where they are, with no glide from nowhere.
    bool clampedNow = false, syncNow = false;
    v = vTarget = targetSpeedCms (clampedNow, syncNow) * CELLS_PER_CM;
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        const float* pos[NUM_HEADS] = { &params.h1Pos, &params.h2Pos, &params.h3Pos };
        hPos[i] = std::clamp ((double) *pos[i], HEAD_MIN_CM, HEAD_MAX_CM);
        hVel[i] = 0.0;
    }
    {
        // A session saved with two heads inside each other: open them apart.
        double start[NUM_HEADS];
        for (int i = 0; i < NUM_HEADS; ++i) start[i] = hPos[i] - 1.0e-9 * (NUM_HEADS - i);
        separateHeads (start);
        for (int i = 0; i < NUM_HEADS; ++i) hPos[i] = std::clamp (hPos[i], HEAD_MIN_CM, HEAD_MAX_CM);
    }

    adoptStore (new TapeStore (cellsNeeded (mode, loopM)), mode, cellsNeeded (mode, loopM));
    spliceState = 0;
    wetFade = 1.0;
    recFade = 1.0;                     // the first tape needs no fade

    for (auto& b : lowS)  b.reset();
    for (auto& b : highS) b.reset();
    bassDb = params.bass; trebDb = params.treble;
    for (int c = 0; c < 2; ++c)
    {
        lowS[c].shelf (false, fsInt, 200.0, bassDb);
        highS[c].shelf (true, fsInt, 3000.0, trebDb);
    }

    // Snap every smoothed control to its target.
    process (nullptr, nullptr, nullptr, nullptr, 0);
    gIn = dbToLin (params.drive);
    gDry = (params.killDry > 0.5f) ? 0.0 : std::cos (params.mix * M_PI * 0.5);
    gWet = std::sin (params.mix * M_PI * 0.5);
    gOut = dbToLin (params.output);
}

void Engine::adoptStore (TapeStore* s, int newMode, int64_t cells)
{
    if (store != nullptr)
    {
        // Hand the old tape back for the message thread to free.
        TapeStore* prev = retired.exchange (store);
        delete prev;   // only reachable from prepare(), where nothing else runs
    }
    store = s;
    viewStore.store (s, std::memory_order_release);
    tr0 = s->t0.data();
    tr1 = s->t1.data();
    N = cells;
    mode = newMode;

    T = 0.0;
    const double lead = v * leadS;
    for (int i = 0; i < NUM_HEADS; ++i)
        uPrev[i] = T - hPos[i] * CELLS_PER_CM + lead;
    Tlin = 0.0; kTick = 0;
    for (double& x : Thist) x = 0.0;
    wPrevU1 = T;
    ePos = T + ERASE_CM * CELLS_PER_CM;
    vPos = T + SUPPLY_CM * CELLS_PER_CM;
    for (int c = 0; c < 2; ++c) { sHist[c][0] = sHist[c][1] = sHist[c][2] = 0.0; wAcc[c] = 0.0; wLen[c] = 0.0; }
    recFade = 0.0;                     // recording onto a new tape starts from silence

    tele.mode = mode;
    tele.loopLenM = loopM;
}

//==============================================================================
void Engine::service()
{
    if (TapeStore* r = retired.exchange (nullptr, std::memory_order_acq_rel))
        delete r;

    const int64_t want = wantCells.load (std::memory_order_acquire);
    if (want > 0 && pending.load (std::memory_order_acquire) == nullptr)
    {
        pending.store (new TapeStore (want), std::memory_order_release);
        wantCells.store (0, std::memory_order_release);
    }
}

//==============================================================================
double Engine::readPoint (const float* tr, double u) const
{
    const double x = u - 0.5;                 // cell i's value sits at u = i + 0.5
    const double fl = std::floor (x);
    const int64_t i = (int64_t) fl;
    const double t = x - fl;
    CR seg (tr[wrapIndex (i - 1)], tr[wrapIndex (i)], tr[wrapIndex (i + 1)], tr[wrapIndex (i + 2)]);
    return seg.at (t);
}

/*  Mean of the Catmull-Rom interpolant over material [a, b] - the head's output
    integrated over the sample. The integral is closed-form per segment; a
    full segment is (-p0 + 13 p1 + 13 p2 - p3) / 24. */
double Engine::readAvg (const float* tr, double a, double b) const
{
    if (b < a) std::swap (a, b);
    const double w = b - a;
    if (w < 1.0e-3) return readPoint (tr, 0.5 * (a + b));

    const double xa = a - 0.5, xb = b - 0.5;
    const double fa = std::floor (xa), fb = std::floor (xb);
    const int64_t ia = (int64_t) fa, ib = (int64_t) fb;
    const double ta = xa - fa, tb = xb - fb;

    auto P = [&] (int64_t i) { return (double) tr[wrapIndex (i)]; };

    if (ia == ib)
    {
        CR s (P (ia - 1), P (ia), P (ia + 1), P (ia + 2));
        return (s.prim (tb) - s.prim (ta)) / w;
    }

    double sum = 0.0;
    {
        CR s (P (ia - 1), P (ia), P (ia + 1), P (ia + 2));
        sum += s.prim (1.0) - s.prim (ta);
    }
    for (int64_t i = ia + 1; i < ib; ++i)
        sum += (-P (i - 1) + 13.0 * P (i) + 13.0 * P (i + 1) - P (i + 2)) * (1.0 / 24.0);
    {
        CR s (P (ib - 1), P (ib), P (ib + 1), P (ib + 2));
        sum += s.prim (tb);
    }
    return sum / w;
}

/*  The record head passed material u0 -> u1 during one sample, and the record
    signal over that sample is the Catmull-Rom cubic through p0..p3 (the sample
    before, the two ends, the sample after). Position is linear in time within
    a sample, so each cell's share is an exact integral of the cubic, and each
    cell gets the signal's mean over its own transit.

    Joining the samples with straight lines instead - the first version - is a
    triangle filter in time: 0.8 dB down at 16 kHz at 96 kHz, at every speed. */
void Engine::writeSpan (float* tr, int c, double u0, double u1, double p0, double p1, double p2, double p3)
{
    const double du = u1 - u0;
    if (du <= 0.0) return;
    const CR seg (p0, p1, p2, p3);
    double pos = u0, ta = 0.0, Pa = 0.0;
    for (;;)
    {
        const double boundary = std::floor (pos) + 1.0;
        if (boundary > u1) break;
        const double tb = (boundary - u0) / du;
        const double Pb = seg.prim (tb);
        wAcc[c] += du * (Pb - Pa);
        wLen[c] += boundary - pos;
        if (wLen[c] > 0.0)
            tr[wrapIndex ((int64_t) std::floor (pos))] = (float) (wAcc[c] / wLen[c]);
        wAcc[c] = 0.0; wLen[c] = 0.0;
        pos = boundary; ta = tb; Pa = Pb;
    }
    wAcc[c] += du * (seg.prim (1.0) - Pa);
    wLen[c] += u1 - pos;
    (void) ta;
}

/*  Return every cell that has passed `newPos` to zero (the erase head, at full
    field in stage 1; or the supply side of a REEL, where virgin tape enters). */
void Engine::clearSpan (double& prevPos, double newPos)
{
    const int64_t a = (int64_t) std::floor (prevPos);
    const int64_t b = (int64_t) std::floor (newPos);
    for (int64_t i = a; i < b; ++i)
    {
        const int64_t k = wrapIndex (i);
        tr0[k] = 0.0f; tr1[k] = 0.0f;
    }
    prevPos = newPos;
}

//==============================================================================
/*  Three heads on one rail touch the tape on the same line, so they cannot pass
    through each other. A carriage that runs into its neighbour STOPS against it
    (its control keeps asking; the head's delay readout shows where it really
    is). The neighbour is not shoved: it holds its own position. Order along the
    rail is the order the heads were in before this step. */
void Engine::separateHeads (const double* before)
{
    int ord[NUM_HEADS] = { 0, 1, 2 };
    std::sort (ord, ord + NUM_HEADS, [&] (int a, int b) { return before[a] < before[b]; });
    for (int k = 0; k + 1 < NUM_HEADS; ++k)
    {
        const int a = ord[k], b = ord[k + 1];
        if (hPos[b] - hPos[a] >= HEAD_SPACING_CM) continue;
        const double ma = hPos[a] - before[a], mb = hPos[b] - before[b];
        if (ma > mb) { hPos[a] = hPos[b] - HEAD_SPACING_CM; hVel[a] = 0.0; }   // a ran into b
        else         { hPos[b] = hPos[a] + HEAD_SPACING_CM; hVel[b] = 0.0; }   // b ran into a
    }
}

//==============================================================================
void Engine::tick (float xL, float xR, float& yL, float& yR)
{
    // ---- splice fade
    if (spliceState == 1)
    {
        wetFade -= dt / SPLICE_FADE_S;
        if (wetFade <= 0.0)
        {
            wetFade = 0.0;
            spliceState = 2;
            wantCells.store (cellsNeeded (spliceMode, spliceLoopM), std::memory_order_release);
        }
    }
    else if (spliceState == 2)
    {
        const int64_t need = cellsNeeded (spliceMode, spliceLoopM);
        TapeStore* s = pending.load (std::memory_order_acquire);
        if (s != nullptr && retired.load (std::memory_order_acquire) == nullptr)
        {
            pending.store (nullptr, std::memory_order_release);
            if (s->n == need)
            {
                // The panel must never be left looking at a tape that is about
                // to be freed: publish the new one before retiring the old.
                viewStore.store (s, std::memory_order_release);
                TapeStore* old = store;
                store = nullptr;
                retired.store (old, std::memory_order_release);
                loopM = spliceLoopM;
                adoptStore (s, spliceMode, need);
                spliceState = 3;
            }
            else
            {
                // A tape cut for an earlier request: send it back, ask again.
                retired.store (s, std::memory_order_release);
                wantCells.store (need, std::memory_order_release);
            }
        }
    }
    else if (spliceState == 3)
    {
        wetFade += dt / SPLICE_FADE_S;
        if (wetFade >= 1.0) { wetFade = 1.0; spliceState = 0; }
    }

    // ---- motor and carriages
    v += (vTarget - v) * aMotor;

    const float* tgtP[NUM_HEADS] = { &params.h1Pos, &params.h2Pos, &params.h3Pos };
    const double headMax = (mode == 1)
        ? std::min (HEAD_MAX_CM, 0.85 * (loopM * 100.0 - ERASE_CM)) : HEAD_MAX_CM;
    constexpr double W = 25.0;                         // rad/s, critically damped
    double before[NUM_HEADS];
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        before[i] = hPos[i];
        const double tgt = std::clamp ((double) *tgtP[i], HEAD_MIN_CM, headMax);
        hVel[i] += (W * W * (tgt - hPos[i]) - 2.0 * W * hVel[i]) * dt;
        hVel[i] = std::clamp (hVel[i], -CARRIAGE_MAX_CMS, CARRIAGE_MAX_CMS);
        hPos[i] += hVel[i] * dt;
    }
    separateHeads (before);

    // ---- the tape moves
    T += v * dt;
    Tlin += v * dt;
    ++kTick;
    Thist[kTick & (HIST - 1)] = Tlin;
    if (Tlin > 1.0e9) { Tlin -= 1.0e9; for (double& x : Thist) x -= 1.0e9; }
    if (T >= (double) N)
    {
        const double n = (double) N;
        T -= n; wPrevU1 -= n; ePos -= n; vPos -= n;
        for (double& u : uPrev) u -= n;
    }
    const double lead = v * leadS;

    // ---- playback: each head reads the material it crossed this sample
    double outL = 0.0, outR = 0.0, fbL = 0.0, fbR = 0.0;
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        const double u1 = T - hPos[i] * CELLS_PER_CM + lead;
        const double rL = readAvg (tr0, uPrev[i], u1);
        const double rR = readAvg (tr1, uPrev[i], u1);
        uPrev[i] = u1;

        const double hL = rL / FLUX_PER_UNIT, hR = rR / FLUX_PER_UNIT;
        const double g = gHead[i];
        outL += g * panL[i] * hL;
        outR += g * panR[i] * hR;
        fbL += g * hL;
        fbR += g * hR;
        const float pk = (float) (g * std::max (std::abs (hL), std::abs (hR)));
        if (pk > pkHead[i]) pkHead[i] = pk;
    }

    // ---- undo the read integration's box (exact: it is a box in time)
    outL = comp[0].tick (outL); outR = comp[1].tick (outR);
    fbL  = comp[2].tick (fbL);  fbR  = comp[3].tick (fbR);

    // ---- the feedback wire, through the tone stage
    fbL = highS[0].tick (lowS[0].tick (fbL)) * fbEff;
    fbR = highS[1].tick (lowS[1].tick (fbR)) * fbEff;

    // ---- record
    if (recFade < 1.0) recFade = std::min (1.0, recFade + dt / SPLICE_FADE_S);
    const double sL = (xL * gIn + fbL) * FLUX_PER_UNIT * recFade;
    const double sR = (xR * gIn + fbR) * FLUX_PER_UNIT * recFade;
    // one sample late: the segment between k-2 and k-1, with k-3 and k around it
    {
        // The segment k-2 .. k-1, placed where the tape was when those samples
        // entered the box (inLag samples further back). Contiguous with the last
        // write by construction; the end is recomputed from the history.
        const double u0 = wPrevU1;
        const double u1 = posAgo (1 + inLag);
        writeSpan (tr0, 0, u0, u1, sHist[0][0], sHist[0][1], sHist[0][2], sL);
        writeSpan (tr1, 1, u0, u1, sHist[1][0], sHist[1][1], sHist[1][2], sR);
        wPrevU1 = u1;
    }
    for (int k = 0; k < 2; ++k) { sHist[k][0] = sHist[k][1]; sHist[k][1] = sHist[k][2]; }
    sHist[0][2] = sL; sHist[1][2] = sR;

    // ---- erase head (full field in stage 1), and virgin tape onto a REEL
    clearSpan (ePos, T + ERASE_CM * CELLS_PER_CM);
    if (mode == 0) clearSpan (vPos, T + SUPPLY_CM * CELLS_PER_CM);

    yL = (float) (outL * wetFade);
    yR = (float) (outR * wetFade);
}

//==============================================================================
double Engine::targetSpeedCms (bool& clamped, bool& syncOn) const
{
    const Params& p = params;
    double vt = std::clamp ((double) p.speed, 4.75, 76.0);
    const int si = std::clamp ((int) std::lround (p.sync), 0, NUM_SYNC - 1);
    clamped = false; syncOn = false;
    if (si > 0 && bpm > 0.0)
    {
        const float* pos[NUM_HEADS] = { &p.h1Pos, &p.h2Pos, &p.h3Pos };
        const int th = std::clamp ((int) std::lround (p.syncHead), 0, NUM_HEADS - 1);
        const double h = std::clamp ((double) *pos[th], HEAD_MIN_CM, HEAD_MAX_CM);
        const double dur = SYNC_BEATS[si] * 60.0 / bpm;
        const double want = h / dur;
        vt = std::clamp (want, 4.75, 76.0);
        clamped = std::abs (vt - want) > 1.0e-6;
        syncOn = true;
    }
    return vt;
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const Params& p = params;

    // ---- transport requests: a mode change splices at once, a new loop length
    //      once the control has been still for 150 ms (loops are cut to 5 cm).
    const int wantMode = p.mode > 0.5f ? 1 : 0;
    const double q = std::clamp (std::round (p.loopLen / 0.05f) * 0.05, 0.5, 6.0);
    if (std::abs (q - pendingLoopM) > 1.0e-9) { pendingLoopM = q; loopSettle = 0.0; }
    else loopSettle += n / fsHost;
    if (spliceState == 0)
    {
        const bool lenChange = wantMode == 1 && mode == 1 && loopSettle > 0.15
                               && std::abs (pendingLoopM - loopM) > 1.0e-9;
        if (wantMode != mode || lenChange)
        {
            spliceState = 1;
            spliceMode = wantMode;
            spliceLoopM = pendingLoopM;
        }
    }

    // ---- motor target: the speed control, or SYNC on the tempo head
    bool clamped = false, syncOn = false;
    vTarget = targetSpeedCms (clamped, syncOn) * CELLS_PER_CM;

    // ---- control targets
    const float* on[NUM_HEADS]  = { &p.h1On, &p.h2On, &p.h3On };
    const float* lvl[NUM_HEADS] = { &p.h1Lvl, &p.h2Lvl, &p.h3Lvl };
    const float* pan[NUM_HEADS] = { &p.h1Pan, &p.h2Pan, &p.h3Pan };
    double tHead[NUM_HEADS], tPL[NUM_HEADS], tPR[NUM_HEADS], sumHead = 0.0;
    for (int i = 0; i < NUM_HEADS; ++i)
    {
        tHead[i] = (*on[i] > 0.5f) ? dbToLin (*lvl[i]) : 0.0;
        sumHead += tHead[i];
        const double pn = std::clamp ((double) *pan[i], -1.0, 1.0);
        tPL[i] = std::min (1.0, 1.0 - pn);
        tPR[i] = std::min (1.0, 1.0 + pn);
    }
    const double gRec = dbToLin (p.recLevel);
    const double tIn = dbToLin (p.drive) * gRec;
    const double toneMax = std::max ({ 1.0, dbToLin (p.bass), dbToLin (p.treble) });
    // STAGE 1: the loop cannot exceed unity gain - containment belongs to tape
    // saturation (stage 3), never to a limiter. The worst case is every head in
    // phase at the tone stage's largest boost.
    const double loopMax = toneMax * gRec * sumHead;
    const double tFb = p.feedback * 0.97 / std::max (1.0, loopMax) * gRec;

    const double tDry = (p.killDry > 0.5f) ? 0.0 : std::cos (p.mix * M_PI * 0.5);
    const double tWet = std::sin (p.mix * M_PI * 0.5);
    const double tOut = dbToLin (p.output);
    const bool mono = p.inMode > 0.5f;

    if (n == 0)
    {
        // prepare(): snap everything.
        for (int i = 0; i < NUM_HEADS; ++i) { gHead[i] = tHead[i]; panL[i] = tPL[i]; panR[i] = tPR[i]; }
        fbEff = tFb; gIn = tIn;
        return;
    }

    for (int k = 0; k < n; ++k)
    {
        // smoothed controls, stepped at the host rate (20 ms)
        for (int i = 0; i < NUM_HEADS; ++i)
        {
            gHead[i] += (tHead[i] - gHead[i]) * aHost;
            panL[i]  += (tPL[i] - panL[i]) * aHost;
            panR[i]  += (tPR[i] - panR[i]) * aHost;
        }
        fbEff += (tFb - fbEff) * aHost;
        gIn   += (tIn - gIn) * aHost;
        gDry  += (tDry - gDry) * aHost;
        gWet  += (tWet - gWet) * aHost;
        gOut  += (tOut - gOut) * aHost;

        if ((toneTick++ & 63) == 0)
        {
            bassDb += (p.bass - bassDb) * 0.1;
            trebDb += (p.treble - trebDb) * 0.1;
            for (int c = 0; c < 2; ++c)
            {
                // keep the state, change only the coefficients
                Biquad nl = lowS[c], nh = highS[c];
                nl.shelf (false, fsInt, 200.0, bassDb);
                nh.shelf (true, fsInt, 3000.0, trebDb);
                nl.z1 = lowS[c].z1; nl.z2 = lowS[c].z2;
                nh.z1 = highS[c].z1; nh.z2 = highS[c].z2;
                lowS[c] = nl; highS[c] = nh;
            }
        }

        const float dl = inL ? inL[k] : 0.0f;
        const float dr = inR ? inR[k] : dl;
        float xl = dl, xr = dr;
        if (mono) xl = xr = 0.5f * (dl + dr);
        pkIn = std::max (pkIn, std::max (std::abs (xl), std::abs (xr)));

        float wl, wr;
        if (os == 2)
        {
            float a0, a1, b0, b1, y0l, y0r, y1l, y1r;
            up[0].up (xl, a0, a1);
            up[1].up (xr, b0, b1);
            tick (a0, b0, y0l, y0r);
            tick (a1, b1, y1l, y1r);
            wl = dn[0].down (y0l, y1l);
            wr = dn[1].down (y0r, y1r);
        }
        else
        {
            tick (xl, xr, wl, wr);
        }

        const float ol = (float) ((dl * gDry + wl * gWet) * gOut);
        const float orr = (float) ((dr * gDry + wr * gWet) * gOut);
        pkOut = std::max (pkOut, std::max (std::abs (ol), std::abs (orr)));
        if (outL) outL[k] = ol;
        if (outR) outR[k] = orr;
    }

    // ---- telemetry
    const double distM = v * n * os * dt * CELL_UM * 1.0e-6;   // m of tape this block
    capAng += distM / CAPSTAN_R_M;
    if (mode == 0)
    {
        playedM += distM;
        if (playedM >= REEL_PACK_M) playedM -= REEL_PACK_M;       // a fresh pack
        const double rs = std::sqrt (REEL_HUB_M * REEL_HUB_M + (REEL_PACK_M - playedM) * TAPE_THICK_M / M_PI);
        const double rt = std::sqrt (REEL_HUB_M * REEL_HUB_M + playedM * TAPE_THICK_M / M_PI);
        supAng += distM / rs;
        takAng += distM / rt;
        tele.supplyR = rs; tele.takeupR = rt;
    }
    else
    {
        loopAng += distM / 0.03;
    }
    viewT.store (T, std::memory_order_relaxed);
    tele.speedCms = v / CELLS_PER_CM;
    tele.targetCms = vTarget / CELLS_PER_CM;
    tele.capstanAngle = std::fmod (capAng, 2.0 * M_PI);
    tele.supplyAngle = std::fmod (supAng, 2.0 * M_PI);
    tele.takeupAngle = std::fmod (takAng, 2.0 * M_PI);
    tele.loopAngle = std::fmod (loopAng, 2.0 * M_PI);
    tele.tapeOdometerM = playedM;
    for (int i = 0; i < NUM_HEADS; ++i) { tele.headCm[i] = hPos[i]; tele.headVelCms[i] = hVel[i]; }
    tele.syncClamped = clamped;
    tele.syncActive = syncOn;
    tele.splicing = spliceState != 0;

    // peaks, held for the panel's 30 Hz read, then decayed
    tele.inPeak = std::max (tele.inPeak.load(), pkIn);
    tele.outPeak = std::max (tele.outPeak.load(), pkOut);
    for (int i = 0; i < NUM_HEADS; ++i)
        tele.headPeak[i] = std::max (tele.headPeak[i].load(), pkHead[i]);
    pkIn = pkOut = 0.0f;
    for (float& x : pkHead) x = 0.0f;
}

//==============================================================================
void Engine::tapeView (float* out, int n, double fromCm, double toCm) const
{
    // Read-only and unlocked: a display may tear, audio never waits.
    const TapeStore* vs = viewStore.load (std::memory_order_acquire);
    if (vs == nullptr) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
    const float* a = vs->t0.data();
    const float* b = vs->t1.data();
    const int64_t ring = vs->n;
    const double t = std::fmod (viewT.load (std::memory_order_relaxed), (double) ring);
    if (a == nullptr || ring <= 1 || n <= 0) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }

    const double peakRef = REF_FLUX * std::sqrt (2.0);
    const bool reel = tele.mode.load() == 0;
    for (int j = 0; j < n; ++j)
    {
        const double x0 = fromCm + (toCm - fromCm) * j / n;
        const double x1 = fromCm + (toCm - fromCm) * (j + 1) / n;
        double m = 0.0;
        // On a REEL, tape upstream of the supply guide is still on the supply
        // reel: virgin. (The ring cells there hold tape 40 cm on, never read.)
        // On a LOOP the same stretch is the tape coming back round - shown.
        if (reel && x1 <= -SUPPLY_CM) { out[j] = 0.0f; continue; }
        constexpr int S = 24;
        for (int s = 0; s < S; ++s)
        {
            const double x = x0 + (x1 - x0) * (s + 0.5) / S;
            int64_t i = (int64_t) std::floor (t - x * CELLS_PER_CM);
            i %= ring; if (i < 0) i += ring;
            m = std::max (m, (double) std::max (std::abs (a[i]), std::abs (b[i])));
        }
        out[j] = (float) (m / peakRef);
    }
}

} // namespace mt
