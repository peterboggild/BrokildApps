#include "ks_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace ks
{

namespace
{
    constexpr double PI  = 3.14159265358979323846;
    constexpr float  LN60 = 6.9077553f;         // ln(1000): -60 dB

    inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
    inline float lerpf  (float a, float b, float t)   { return a + (b - a) * t; }
    inline float db2lin (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float smooth01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }

    /*  sin(2 pi x), any x. Folded to a quarter wave and summed to the y^11
        term: the error is under 4e-8, about -148 dB, so the oscillators are
        cleaner than the float they are stored in. std::sin costs several
        times as much and a kick at 4x runs up to eight of these. */
    inline float sin2pi (double x)
    {
        double r = x - std::floor (x + 0.5);        // [-0.5, 0.5)
        if (r > 0.25)       r = 0.5 - r;
        else if (r < -0.25) r = -0.5 - r;
        const double y = 2.0 * PI * r, y2 = y * y;
        return (float) (y * (1.0 + y2 * (-1.0 / 6.0 + y2 * (1.0 / 120.0 + y2 * (-1.0 / 5040.0
                         + y2 * (1.0 / 362880.0 + y2 * (-1.0 / 39916800.0)))))));
    }

    /*  A soft ceiling: IEEE-exact identity below `knee` of the limit and
        asymptotic above it (Battlestar's, and the fleet's), so nothing can
        leave the plug-in over full scale and nothing under 0.9 is touched. */
    inline float ceilSoft (float x, float lim = 1.0f, float knee = 0.90f)
    {
        const float t = knee * lim;
        const float a = std::abs (x);
        if (a <= t) return x;
        const float over = (a - t) / (lim - t);
        const float y = t + (lim - t) * std::tanh (over);
        return x < 0.0f ? -y : y;
    }

    inline float wavefold (float x)
    {
        float t = std::fmod (x + 1.0f, 4.0f);
        if (t < 0.0f) t += 4.0f;
        t -= 1.0f;
        return t > 1.0f ? 2.0f - t : t;
    }

    //  circular membrane modes, relative to the (0,1) mode: the Bessel zeros
    //  j(1,1) j(2,1) j(0,2) j(3,1) j(1,2) j(4,1) j(2,2) over j(0,1)
    constexpr float kModeRatio[kModes] = { 1.5933f, 2.1355f, 2.2954f, 2.6531f, 2.9173f, 3.1555f, 3.5001f };

    //  Battlestar Overdrive's engine constants (driveMax, character), for the
    //  four engines Kickstart carries
    struct DriveDef { const char* name; float driveMax; float character; };
    constexpr DriveDef kDrive[NUM_DRIVE_ENGINES] =
    {
        { "IDLE BURN",  10.0f, 0.55f },
        { "HYPERDRIVE",  8.0f, 0.45f },
        { "RAZOR WING", 24.0f, 0.70f },
        { "SUPERNOVA",  14.0f, 0.50f },
    };

    //  0.38 * (k+1)^-0.7: the higher modes speak less
    constexpr float kModeAmp[kModes] = { 0.3800f, 0.2340f, 0.1759f, 0.1441f, 0.1235f, 0.1088f, 0.0977f };

    //  ---- first-order antiderivative anti-aliasing (ADAA) ----------------
    //  y = (G(k x) - G(k x')) / (k (x - x')), G the antiderivative of the
    //  shaper. A hard clip at 4x still folds its highest harmonics back;
    //  this takes a large part of that away for the three smooth engines.
    //  SUPERNOVA quantises on purpose - its aliasing is its sound - so it
    //  runs plain.
    inline double logcosh (double u) { u = std::abs (u); return u + std::log1p (std::exp (-2.0 * u)) - 0.6931471805599453; }

    constexpr int TRIM_PTS = 17;

    struct TrimTable
    {
        float t[NUM_DRIVE_ENGINES][TRIM_PTS];
        /*  Measured, never typed: each engine's own shaper run over a sine at
            three levels (kick levels, so higher than Battlestar's guitar
            levels), averaged in the log domain, so a turned-up DRIVE changes
            the character and not the loudness. */
        TrimTable()
        {
            const float REF[3] = { 0.20f, 0.45f, 0.80f };
            for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
                for (int d = 0; d < TRIM_PTS; ++d)
                {
                    const float drive = (float) d / (TRIM_PTS - 1);
                    const float k = 1.0f + drive * kDrive[e].driveMax;
                    double logSum = 0.0;
                    for (float a : REF)
                    {
                        double acc = 0.0;
                        const int N = 4800;
                        for (int n = 0; n < N; ++n)
                        {
                            const float x = a * sin2pi (100.0 * n / 48000.0);
                            const float y = driveShape (e, x, k);
                            acc += (double) y * y;
                        }
                        const double r = std::sqrt (acc / N);
                        logSum += std::log (std::max (1.0e-4, a * 0.70710678 / std::max (1.0e-6, r)));
                    }
                    t[e][d] = clampf ((float) std::exp (logSum / 3.0), 0.02f, 8.0f);
                }
        }
    };

    const TrimTable& trims()
    {
        static const TrimTable tt;     // thread-safe static init
        return tt;
    }
}

//==============================================================================
namespace
{
    double antiderivative (int e, double u, double a)
    {
        switch (e)
        {
            case ENG_RAZOR:
                return (1.0 - a) * logcosh (u) + a * (std::abs (u) <= 1.0 ? 0.5 * u * u : std::abs (u) - 0.5);
            case ENG_IDLE:
            {
                const double s = 0.6 + 0.4 * a;
                const double ft = u >= 0.0 ? logcosh (u) : s / a * logcosh (a * u);
                const double fc = std::abs (u) <= 1.0 ? 1.5 * (0.5 * u * u - u * u * u * u / 12.0)
                                                      : 0.625 + std::abs (u) - 1.0;
                return 0.3 * fc + 0.7 * ft;
            }
            case ENG_HYPER:
            {
                const double s = a * 3.0;
                const double w2 = std::max (0.0, 1.0 - std::abs (s)), w3 = std::max (0.0, 1.0 - std::abs (s - 1.0));
                const double w4 = std::max (0.0, 1.0 - std::abs (s - 2.0)), w5 = std::max (0.0, 1.0 - std::abs (s - 3.0));
                const double g = 0.9 / (w2 + w3 + w4 + w5 + 1.0e-6);
                auto P = [&] (double c)
                {
                    const double c2 = c * c, c3 = c2 * c, c4 = c2 * c2;
                    return 0.175 * c2 + g * (w2 * (2.0 / 3.0) * c3 + w3 * (c4 - 1.5 * c2)
                                             + w4 * (1.6 * c4 * c - (8.0 / 3.0) * c3)
                                             + w5 * ((8.0 / 3.0) * c4 * c2 - 5.0 * c4 + 2.5 * c2));
                };
                auto p = [&] (double c)
                {
                    const double c2 = c * c;
                    return 0.35 * c + g * (w2 * 2.0 * c2 + w3 * c * (4.0 * c2 - 3.0) + w4 * (8.0 * c2 * c2 - 8.0 * c2)
                                           + w5 * c * (16.0 * c2 * c2 - 20.0 * c2 + 5.0));
                };
                if (u > 1.0)  return P (1.0) + p (1.0) * (u - 1.0);
                if (u < -1.0) return P (-1.0) + p (-1.0) * (u + 1.0);
                return P (u);
            }
            default: return 0.0;
        }
    }
}

float driveADAA (int engine, float x, double xPrev, float k)
{
    if (engine == ENG_NOVA) return driveShape (engine, x, k);
    const double dx = (double) x - xPrev;
    if (std::abs (dx) < 1.0e-6)
        return driveShape (engine, (float) (0.5 * ((double) x + xPrev)), k);
    const double a = kDrive[engine].character;
    return (float) ((antiderivative (engine, (double) k * x, a) - antiderivative (engine, (double) k * xPrev, a)) / ((double) k * dx));
}

float driveMaxFor (int e) { return kDrive[std::clamp (e, 0, NUM_DRIVE_ENGINES - 1)].driveMax; }
const char* driveEngineName (int e) { return kDrive[std::clamp (e, 0, NUM_DRIVE_ENGINES - 1)].name; }

float Engine::driveTrimFor (int engine, float drive)
{
    const auto& T = trims();
    const int e = std::clamp (engine, 0, NUM_DRIVE_ENGINES - 1);
    const float x = clampf (drive, 0.0f, 1.0f) * (TRIM_PTS - 1);
    const int i = std::min ((int) x, TRIM_PTS - 2);
    return lerpf (T.t[e][i], T.t[e][i + 1], x - (float) i);
}

/*  The four shapers, exactly as Battlestar Overdrive runs them. */
float driveShape (int engine, float x, float k)
{
    const float u = x * k;
    switch (engine)
    {
        case ENG_IDLE:
        {
            const float a = kDrive[ENG_IDLE].character;
            const float t = u >= 0.0f ? std::tanh (u) : std::tanh (u * a) * (0.6f + 0.4f * a);
            const float c = clampf (u, -1.0f, 1.0f);
            const float cub = 1.5f * (c - c * c * c / 3.0f);
            return lerpf (cub, t, 0.7f);
        }
        case ENG_HYPER:
        {
            const float a = kDrive[ENG_HYPER].character;
            const float c = clampf (u, -1.0f, 1.0f), c2 = c * c;
            const float t2 = 2.0f * c2;
            const float t3 = c * (4.0f * c2 - 3.0f);
            const float t4 = 8.0f * c2 * c2 - 8.0f * c2;
            const float t5 = c * (16.0f * c2 * c2 - 20.0f * c2 + 5.0f);
            const float s  = a * 3.0f;
            const float w2 = std::max (0.0f, 1.0f - std::abs (s));
            const float w3 = std::max (0.0f, 1.0f - std::abs (s - 1.0f));
            const float w4 = std::max (0.0f, 1.0f - std::abs (s - 2.0f));
            const float w5 = std::max (0.0f, 1.0f - std::abs (s - 3.0f));
            const float nn = w2 + w3 + w4 + w5 + 1.0e-6f;
            return c * 0.35f + 0.9f * (w2 * t2 + w3 * t3 + w4 * t4 + w5 * t5) / nn;
        }
        case ENG_RAZOR:
            return lerpf (std::tanh (u), clampf (u, -1.0f, 1.0f), kDrive[ENG_RAZOR].character);
        case ENG_NOVA:
        {
            const float a = kDrive[ENG_NOVA].character;
            float y = wavefold (u);
            y = std::floor (y * 8.0f + 0.5f) * 0.125f;
            y = clampf (y * 1.6f, -1.0f, 1.0f);
            return y * (1.0f - a * 0.5f) + a * 0.5f * std::sin (11.0f * y);
        }
        default: break;
    }
    return x;
}

const char* noteName (float hz, char* buf, int bufLen)
{
    static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double m = 69.0 + 12.0 * std::log2 (std::max (1.0, (double) hz) / 440.0);
    const int r = (int) std::lround (m);
    const int c = (int) std::lround ((m - r) * 100.0);
    const int oct = r / 12 - 1;
    if (c == 0) std::snprintf (buf, (size_t) bufLen, "%s%d", N[((r % 12) + 12) % 12], oct);
    else        std::snprintf (buf, (size_t) bufLen, "%s%d %+dc", N[((r % 12) + 12) % 12], oct, c);
    return buf;
}

//==============================================================================
void HalfBand::design()
{
    constexpr int C = TAPS / 2;
    std::array<double, TAPS> g {};
    double sum = 0.0;
    for (int n = 0; n < TAPS; ++n)
    {
        const double t = n - C;
        const double s = t == 0.0 ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                              + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
        g[(size_t) n] = s * w;
        sum += g[(size_t) n];
    }
    for (auto& v : g) v /= sum;
    for (int k = 0; k < PH; ++k)
    {
        ge[(size_t) k] = 2 * k     < TAPS ? (float) g[(size_t) (2 * k)]     : 0.0f;
        go[(size_t) k] = 2 * k + 1 < TAPS ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
    }
}

void Svf::set (float fc, float fsr, float q)
{
    fc = clampf (fc, 5.0f, 0.49f * fsr);
    g = (float) std::tan (PI * fc / fsr);
    k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

//==============================================================================
void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate > 1000.0 ? sampleRate : 48000.0;
    fo = fs * kOS;

    hb1.design(); hb2.design();
    (void) trims();

    const double ms = fo * 0.001;
    const float roomMs[kLines] = { 7.1f, 9.7f, 12.3f, 15.9f };
    for (int i = 0; i < kLines; ++i)
    {
        len[(size_t) i] = std::max (8, (int) std::lround (roomMs[i] * ms));
        line[(size_t) i].assign ((size_t) len[(size_t) i], 0.0f);
    }

    //  the transient detectors run at the OS rate
    auto co = [&] (double msT) { return (float) (1.0 - std::exp (-1.0 / (msT * ms))); };
    aFA = co (0.1); aSA = co (12.0); aRelA = co (20.0);
    aFR = co (8.0); aSR = co (200.0);

    //  look-ahead 1.5 ms; the detector's window spans it plus 10 ms of hold,
    //  longer than half a period of any kick fundamental, so a steady tone
    //  is seen at its peak and the gain does not ripple
    laN = std::max (1, (int) std::lround (0.0015 * fs));
    laBuf.assign ((size_t) laN, 0.0f);
    holdN = laN + std::max (1, (int) std::lround (0.010 * fs));
    int dq = 1; while (dq < holdN + 4) dq <<= 1;
    dqVal.assign ((size_t) dq, 0.0f);
    dqIdx.assign ((size_t) dq, 0);
    dqMask = dq - 1;

    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice();
    hitSeed = 0;
    gritPh = 1.0; gritHeld = 0.0f;
    tFA = tSA = tFR = tSR = 0.0f;
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    pos.fill (0); dampZ.fill (0.0f);
    roomLive = false;
    driveSm = 0.0f; colourSm = 1.0f; roomSm = 0.0f;
    adaaPrev = 0.0;
    std::fill (laBuf.begin(), laBuf.end(), 0.0f); laPos = 0;
    fresh = true;
    colourLp.clear(); coefTick = 0;
    hb1.clear(); hb2.clear();
    dqHead = dqTail = cTick = 0;
    compG = 0.0f; levelSm = 1.0f;
    pendingNote = -1;
    mPeak.store (0.0f); mGr.store (0.0f);
}

void Engine::noteOn (int note, float velocity01)
{
    pendingNote = note;
    pendingVel  = clampf (velocity01, 0.0f, 1.0f);
}

//==============================================================================
void Engine::process (const Params& p, float* out, int n)
{
    P = p;
    if (fresh)
    {
        //  a preset loaded before the first audio should not ramp in
        driveSm = p.drive; roomSm = p.room;
        colourSm = p.colour >= 0.999f ? 1.0f : p.colour;
        levelSm = p.level == 0.0f ? 1.0f : db2lin (p.level);
        fresh = false;
    }

    if (pendingNote >= 0)
    {
        //  the hit before it chokes over 3 ms rather than being cut, which
        //  would click; a third hit takes the slot that is further gone
        Voice* slot = &voices[0];
        for (auto& v : voices) if (! v.on) { slot = &v; break; }
        if (slot->on)
            slot = voices[0].choke < voices[1].choke ? &voices[0] : &voices[1];
        for (auto& v : voices)
            if (v.on && &v != slot && v.chokeStep == 0.0f)
                v.chokeStep = (float) (1.0 / (0.003 * fo));

        Voice& v = *slot;
        v = Voice();
        v.on = true;
        v.vel = pendingVel;
        const float V = clampf (P.velo, 0.0f, 1.0f);
        v.velGain  = (1.0f - V) + V * std::pow (v.vel, 1.3f);
        v.clickVel = (1.0f - V) + V * v.vel * v.vel;
        v.sweepVel = 1.0f - 0.35f * V + 0.35f * V * v.vel;
        v.keyMul = P.key > 0.5f ? (float) std::pow (2.0, (pendingNote - 36) / 12.0) : 1.0f;

        //  the beater, fixed at the moment of the hit
        const float t = clampf (P.tone, 0.0f, 1.0f);
        v.cfc  = 180.0f * std::pow (2.0f, t * 6.0f);                 // 180 Hz .. 11.5 kHz
        const float lenMs = 14.0f * std::pow (2.0f, -2.6f * t);        // 14 .. 2.3 ms
        v.ceK  = std::exp (-1.0 / (lenMs / 2.5 * 0.001 * fo));
        v.cnf  = 0.1f + 0.6f * 4.0f * t * (1.0f - t);                  // noise: most in the middle
        v.cph0 = t * 0.25f;                                            // sine start -> cosine tick
        v.cbp.set (v.cfc, (float) fo, 0.7f);
        const double bw = v.cfc / 0.7;
        v.cnorm = (float) std::min (60.0, 0.6 / (0.57735 * std::sqrt (PI * bw / fo)));
        for (auto& m : v.me) m = 1.0;
        v.rng = 0x9E3779B9u * (++hitSeed) ^ 0x2545F491u;
        if (v.rng == 0) v.rng = 1;
        pendingNote = -1;
        hits.fetch_add (1, std::memory_order_relaxed);
    }

    float peak = 0.0f;
    const bool compOn = P.comp > 0.0f;
    const float amt = clampf (P.comp, 0.0f, 1.0f);
    const float T = -30.0f * amt, R = 1.0f + 7.0f * amt, W = 6.0f;
    auto gc = [&] (float L)
    {
        const float over = L - T, s = 1.0f - 1.0f / R;
        if (over <= -0.5f * W) return 0.0f;
        if (over >=  0.5f * W) return -s * over;
        const float x = over + 0.5f * W;
        return -s * x * x / (2.0f * W);
    };
    //  make-up restores most of what the compressor takes from a signal at
    //  -6 dBFS: enough that turning COMP up does not turn the kick down
    const float makeup = compOn ? -gc (-6.0f) * 0.7f : 0.0f;
    const float sp = clampf (P.speed, 0.0f, 1.0f);
    const double attMs = 30.0 * std::pow (0.3 / 30.0, sp);
    const double relMs = 400.0 * std::pow (40.0 / 400.0, sp);
    const float aA = (float) (1.0 - std::exp (-1.0 / (attMs * 0.001 * fs)));
    const float aR = (float) (1.0 - std::exp (-1.0 / (relMs * 0.001 * fs)));
    const float levelT = P.level == 0.0f ? 1.0f : db2lin (P.level);
    const float aLev = (float) (1.0 - std::exp (-1.0 / (0.01 * fs)));

    float os[kOS];
    for (int i = 0; i < n; ++i)
    {
        renderOS (P, os);
        const float a = hb1.down (os[0], os[1]);
        const float b = hb1.down (os[2], os[3]);
        float y = hb2.down (a, b);

        // ---- COMP -----------------------------------------------------------
        //  the audio is always delayed by the look-ahead, so the latency is
        //  the same with the compressor in or out
        const float ynow = y;
        y = laBuf[(size_t) laPos];
        laBuf[(size_t) laPos] = ynow;
        if (++laPos >= laN) laPos = 0;
        if (compOn)
        {
            const float ax = std::abs (ynow);
            while (dqTail > dqHead && dqVal[(size_t) ((dqTail - 1) & dqMask)] <= ax) --dqTail;
            dqVal[(size_t) (dqTail & dqMask)] = ax;
            dqIdx[(size_t) (dqTail & dqMask)] = cTick;
            ++dqTail;
            while (dqIdx[(size_t) (dqHead & dqMask)] <= cTick - holdN) ++dqHead;
            ++cTick;
            const float L = 20.0f * std::log10 (dqVal[(size_t) (dqHead & dqMask)] + 1.0e-9f);
            const float target = gc (L);
            compG += (target < compG ? aA : aR) * (target - compG);
            y *= db2lin (compG + makeup);
        }
        else if (compG != 0.0f) { compG = 0.0f; dqHead = dqTail = 0; }

        // ---- LEVEL + ceiling -------------------------------------------------
        if (levelSm != levelT)
        {
            levelSm += aLev * (levelT - levelSm);
            if (std::abs (levelSm - levelT) < 1.0e-6f) levelSm = levelT;
        }
        y = ceilSoft (y * levelSm);
        out[i] = y;
        peak = std::max (peak, std::abs (y));
    }

    mPeak.store (peak, std::memory_order_relaxed);
    mGr.store (compG, std::memory_order_relaxed);
}

//==============================================================================
void Engine::renderOS (const Params& p, float* o4)
{
    //  per-call constants (cheap next to the four samples they serve)
    const double foMs = fo * 0.001;
    const double invD = 1.0 / (std::max (10.0f, p.decay) * foMs);
    const float  cp   = 1.0f + 7.0f * clampf (p.curve, 0.0f, 1.0f);
    const double peK  = std::exp (-1.0 / (std::max (0.5f, p.bend) * foMs));
    const float  skin = clampf (p.skin, 0.0f, 1.0f);
    const float  wave = clampf (p.wave, 0.0f, 1.0f);
    double meK[kModes];
    if (skin > 0.0f)
        for (int k = 0; k < kModes; ++k)
        {
            const double tauMs = std::max (8.0, (p.decay / LN60) * 0.45 / std::pow (kModeRatio[k], 1.3));
            meK[k] = std::exp (-1.0 / (tauMs * foMs));
        }
    const float clickAmt = clampf (p.click, 0.0f, 1.0f) * 1.2f;
    const float nyq = (float) (0.45 * fo);

    const float g = clampf (p.grit, 0.0f, 1.0f);
    const double gritRate = 44100.0 * std::pow (2.0, -2.5 * g);
    const float  gritQ = std::pow (2.0f, (16.0f - 12.0f * g) - 1.0f);

    const float att = clampf (p.attack, -1.0f, 1.0f), sus = clampf (p.sustain, -1.0f, 1.0f);

    const int eng = std::clamp ((int) std::lround (p.engine), 0, NUM_DRIVE_ENGINES - 1);
    const float aSm = (float) (1.0 - std::exp (-1.0 / (0.02 * fo)));

    const float rm = clampf (p.room, 0.0f, 1.0f);
    //  room coefficients, once per four samples (roomSm moves slowly)
    float roomG[kLines];
    const float rtNow = 0.12f + 0.9f * std::pow (roomSm, 1.5f);
    for (int i = 0; i < kLines; ++i)
        roomG[i] = std::pow (10.0f, -3.0f * (float) len[(size_t) i] / (rtNow * (float) fo));
    const float dampA = 1.0f - std::exp (-2.0f * (float) PI * (7000.0f - 4000.0f * roomSm) / (float) fo);

    for (int j = 0; j < kOS; ++j)
    {
        // ================= VOICES ============================================
        float x = 0.0f, env = 0.0f;
        for (auto& v : voices)
        {
            if (! v.on) continue;

            const double xd = v.t * invD;
            float ae = cp == 1.0f ? (float) std::exp (-LN60 * xd)
                                  : (float) std::exp (-LN60 * std::pow (xd, (double) cp));
            //  the last 20 % fades to an exact zero so a voice ENDS rather
            //  than being cut at some tiny level
            if (xd > 1.3) ae *= 1.0f - smooth01 ((float) ((xd - 1.3) / 0.2));

            // pitch: electronic sweep + the drum head's tension glide
            const float semis = p.sweep * v.sweepVel * (float) v.pe
                              + skin * 2.5f * v.velGain * ae;
            float f = p.pitch * v.keyMul * std::exp2 (semis / 12.0f);
            f = std::min (f, nyq);
            lastPitch = f;

            v.ph += f / fo;
            if (v.ph >= 1.0) v.ph -= std::floor (v.ph);
            const float s = sin2pi (v.ph);
            float body = s;
            if (wave > 0.0f)
            {
                const double q = v.ph;
                const float tri = (float) (q < 0.25 ? 4.0 * q : (q < 0.75 ? 2.0 - 4.0 * q : 4.0 * q - 4.0));
                if (wave <= 0.5f) body = lerpf (s, tri, wave * 2.0f);
                else
                {
                    const float sq = std::tanh (4.0f * s) * 1.0006711504f;   // 1/tanh(4)
                    body = lerpf (tri, sq, (wave - 0.5f) * 2.0f);
                }
            }
            body *= ae;

            if (skin > 0.0f)
            {
                float m = 0.0f;
                for (int k = 0; k < kModes; ++k)
                {
                    const float fk = f * kModeRatio[k];
                    if (fk < nyq)
                    {
                        v.phM[k] += fk / fo;
                        if (v.phM[k] >= 1.0) v.phM[k] -= std::floor (v.phM[k]);
                        m += sin2pi (v.phM[k]) * (float) v.me[k] * kModeAmp[k];
                    }
                    v.me[k] *= meK[k];
                }
                body += skin * m * std::min (1.0f, ae * 4.0f);
            }

            // the beater
            float clk = 0.0f;
            if (clickAmt > 0.0f && v.ce > 1.0e-7)
            {
                v.cph += v.cfc / fo;
                const float burst = sin2pi (v.cph + v.cph0);
                v.rng ^= v.rng << 13; v.rng ^= v.rng >> 17; v.rng ^= v.rng << 5;
                const float white = (float) (int32_t) v.rng * (1.0f / 2147483648.0f);
                float lp, bp;
                v.cbp.tick (white, lp, bp);
                const float nz = bp * v.cbp.k * v.cnorm;
                clk = ((1.0f - v.cnf) * burst + v.cnf * nz) * (float) v.ce * clickAmt * v.clickVel;
                v.ce *= v.ceK;
            }

            const float gain = 0.75f * v.velGain;
            float vo = body * gain + clk;
            float ve = ae * gain + (float) v.ce * clickAmt * v.clickVel;

            if (v.chokeStep > 0.0f)
            {
                v.choke -= v.chokeStep;
                if (v.choke <= 0.0f) { v.on = false; continue; }
                vo *= v.choke; ve *= v.choke;
            }
            x += vo; env += ve;

            v.pe *= peK;
            v.t += 1.0;
            if (xd > 1.5 && v.ce <= 1.0e-7) v.on = false;
        }

        // ================= GRIT ==============================================
        if (g > 0.0f)
        {
            gritPh += gritRate / fo;
            if (gritPh >= 1.0)
            {
                gritPh -= std::floor (gritPh);
                gritHeld = std::round (x * gritQ) / gritQ;
            }
            x = gritHeld;
        }

        // ================= TRANSIENT =========================================
        //  SPL's two differences, computed on the voices' exact envelopes
        tFA += (env > tFA ? aFA : aRelA) * (env - tFA);
        tSA += (env > tSA ? aSA : aRelA) * (env - tSA);
        tFR += (env > tFR ? 1.0f : aFR) * (env - tFR);
        tSR += (env > tSR ? 1.0f : aSR) * (env - tSR);
        if (att != 0.0f || sus != 0.0f)
        {
            const float dA = clampf (20.0f * std::log10 ((tFA + 1.0e-6f) / (tSA + 1.0e-6f)), 0.0f, 24.0f);
            const float dS = clampf (20.0f * std::log10 ((tSR + 1.0e-6f) / (tFR + 1.0e-6f)), 0.0f, 30.0f);
            const float gdb = clampf (att * 0.5f * dA + sus * (sus > 0.0f ? 0.4f : 0.8f) * dS, -30.0f, 12.0f);
            x *= db2lin (gdb);
        }

        // ================= ROOM ==============================================
        roomSm += aSm * (rm - roomSm);
        if (rm > 0.0f || roomSm > 1.0e-5f)
        {
            roomLive = true;
            float o[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                const float r = line[(size_t) i][(size_t) pos[(size_t) i]];
                dampZ[(size_t) i] += dampA * (r - dampZ[(size_t) i]);
                o[i] = dampZ[(size_t) i];
            }
            const float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]);
            const float h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
            const float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]);
            const float h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
            const float hh[kLines] = { h0, h1, h2, h3 };
            const float sgn[kLines] = { 1.0f, -1.0f, 1.0f, -1.0f };
            for (int i = 0; i < kLines; ++i)
            {
                line[(size_t) i][(size_t) pos[(size_t) i]] = x * 0.35f * sgn[i] + hh[i] * roomG[i];
                if (++pos[(size_t) i] >= len[(size_t) i]) pos[(size_t) i] = 0;
            }
            x += 0.5f * (o[0] + o[1] + o[2] + o[3]) * roomSm * 0.9f;
        }
        else if (roomLive)
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
            dampZ.fill (0.0f);
            roomLive = false;
        }

        // ================= DRIVE =============================================
        const float xIn = x;
        driveSm += aSm * (p.drive - driveSm);
        if (p.drive <= 0.0f && driveSm < 1.0e-6f) driveSm = 0.0f;
        if (driveSm > 0.0f)
        {
            const float wet = smooth01 (driveSm / 0.12f);
            const float k = 1.0f + driveSm * kDrive[eng].driveMax;
            const float yv = driveADAA (eng, x, adaaPrev, k) * driveTrimFor (eng, driveSm);
            x = x + (yv - x) * wet;
        }

        adaaPrev = xIn;
        /*  NO DC blocker. A hit that starts at zero phase carries some DC by
            nature, and a 2 Hz high-pass turned it into a subsonic hump about
            30 dB down that outlived the kick by a second - measured, and it
            made DECAY read twice as long as it is. A kick is a transient; its
            low end is left alone. */

        // ================= COLOUR ============================================
        colourSm += aSm * (p.colour - colourSm);
        if (p.colour >= 0.999f && colourSm > 0.998f) colourSm = 1.0f;
        if (colourSm < 1.0f)
        {
            if (--coefTick <= 0)
            {
                coefTick = 16;
                colourLp.set (900.0f * std::pow (2.0f, 4.5f * colourSm), (float) fo, 0.7071f);
            }
            float lp, bp;
            colourLp.tick (x, lp, bp);
            x = lp;
        }
        else { colourLp.clear(); coefTick = 0; }

        o4[j] = x;
    }
}

//==============================================================================
void Engine::renderHit (const Params& p, double sampleRate, int note, float vel,
                        std::vector<float>& out, int n)
{
    auto e = std::make_unique<Engine>();
    e->prepare (sampleRate, 512);
    e->noteOn (note, vel);
    out.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += 512)
        e->process (p, out.data() + i, std::min (512, n - i));
}

} // namespace ks
