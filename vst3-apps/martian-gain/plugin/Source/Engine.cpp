#include "Engine.h"

namespace mw
{

static constexpr double PI = 3.14159265358979323846;

//==============================================================================
// RBJ cookbook, normalised by a0.
void Biquad::setLP (float f, double sr, float q)
{
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 - c) * 0.5) / a0);
    b1 = (float) ((1.0 - c) / a0);
    b2 = b0;
    a1 = (float) ((-2.0 * c) / a0);
    a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setHP (float f, double sr, float q)
{
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) (((1.0 + c) * 0.5) / a0);
    b1 = (float) ((-(1.0 + c)) / a0);
    b2 = b0;
    a1 = (float) ((-2.0 * c) / a0);
    a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setAP (float f, double sr, float q)
{
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) ((1.0 - al) / a0);
    b1 = (float) ((-2.0 * c) / a0);
    b2 = 1.0f;
    a1 = b1;
    a2 = b0;
}

void Biquad::setLowShelf (float f, double sr, float q, float dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w);
    const double al = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1.0) + (A - 1.0) * c + sq;
    b0 = (float) ((A * ((A + 1.0) - (A - 1.0) * c + sq)) / a0);
    b1 = (float) ((2.0 * A * ((A - 1.0) - (A + 1.0) * c)) / a0);
    b2 = (float) ((A * ((A + 1.0) - (A - 1.0) * c - sq)) / a0);
    a1 = (float) ((-2.0 * ((A - 1.0) + (A + 1.0) * c)) / a0);
    a2 = (float) (((A + 1.0) + (A - 1.0) * c - sq) / a0);
}

void Biquad::setHighShelf (float f, double sr, float q, float dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * PI * clampf (f, 10.0f, (float) sr * 0.49f) / sr;
    const double c = std::cos (w), s = std::sin (w);
    const double al = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
    b0 = (float) ((A * ((A + 1.0) + (A - 1.0) * c + sq)) / a0);
    b1 = (float) ((-2.0 * A * ((A - 1.0) + (A + 1.0) * c)) / a0);
    b2 = (float) ((A * ((A + 1.0) + (A - 1.0) * c - sq)) / a0);
    a1 = (float) ((2.0 * ((A - 1.0) - (A + 1.0) * c)) / a0);
    a2 = (float) (((A + 1.0) - (A - 1.0) * c - sq) / a0);
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
        const double s = (t == 0.0) ? 0.5 : std::sin (PI * 0.5 * t) / (PI * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * PI * n / (TAPS - 1))
                              + 0.08 * std::cos (4.0 * PI * n / (TAPS - 1));
        g[(size_t) n] = s * w;
        sum += g[(size_t) n];
    }
    for (auto& v : g) v /= sum;                 // unity at DC

    for (int k = 0; k < PH; ++k)
    {
        ge[(size_t) k] = (2 * k     < TAPS) ? (float) g[(size_t) (2 * k)]     : 0.0f;
        go[(size_t) k] = (2 * k + 1 < TAPS) ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
    }
}

//==============================================================================
namespace
{
    const char* ALGO_NAMES[NUM_ALGOS] =
    {
        "WARM", "VALVE", "OVERDRIVE", "TAPE",
        "FUZZ", "RAZOR", "FOLD", "SINE FOLD",
        "RECTIFY", "BITCRUSH", "DECIMATE", "SLEW",
        "RING MOD", "CHEBYSHEV", "SHRED", "ANNIHILATE"
    };

    const char* ALGO_CHAR[NUM_ALGOS] =
    {
        "ROUND", "GRID", "KNEE", "DULL",
        "SQUASH", "EDGE", "OFFSET", "FOLDS",
        "OCTAVE", "DITHER", "SMOOTH", "SKEW",
        "FREQ", "HARMONIC", "CHOKE", "CHAOS"
    };
}

namespace
{
    const char* SOURCE_NAMES[NUM_SOURCES] =
    {
        "-", "ENV 1", "ENV 2", "ENV 3", "ENV 4", "ENV 5", "INPUT ENV", "OUTPUT ENV",
        "AUDIO 1", "AUDIO 2", "AUDIO 3", "AUDIO 4", "AUDIO 5"
    };
    const char* DEST_NAMES[NUM_DESTS] =
    {
        "-",
        "1 DRIVE", "2 DRIVE", "3 DRIVE", "4 DRIVE", "5 DRIVE",
        "1 LEVEL", "2 LEVEL", "3 LEVEL", "4 LEVEL", "5 LEVEL",
        "1 CHAR",  "2 CHAR",  "3 CHAR",  "4 CHAR",  "5 CHAR",
        "1 CEIL",  "2 CEIL",  "3 CEIL",  "4 CEIL",  "5 CEIL",
        "XOVER 1", "XOVER 2", "XOVER 3", "XOVER 4",
        "1 IN", "2 IN", "3 IN", "4 IN", "5 IN"
    };
}

const char* sourceName (int s) { return SOURCE_NAMES[(s < 0 || s >= NUM_SOURCES) ? 0 : s]; }
const char* destName   (int d) { return DEST_NAMES  [(d < 0 || d >= NUM_DESTS)   ? 0 : d]; }

const char* algoName (int a)     { return ALGO_NAMES[(a < 0 || a >= NUM_ALGOS) ? 0 : a]; }
const char* algoCharName (int a) { return ALGO_CHAR [(a < 0 || a >= NUM_ALGOS) ? 0 : a]; }

//==============================================================================
ShapeCo makeCo (int algo, float drive, float chr, double sr)
{
    ShapeCo c;
    switch (algo)
    {
        case A_WARM:       c.k = 1.0f + drive * 10.0f; c.a = chr; break;
        case A_VALVE:      c.k = 1.0f + drive * 14.0f; c.a = 0.30f + (1.0f - chr) * 0.65f; break;
        case A_OVERDRIVE:  c.k = 1.0f + drive * 20.0f; c.a = chr; break;
        case A_TAPE:       c.k = 1.0f + drive * 12.0f;
                           c.a = 0.10f + (1.0f - chr) * 0.85f;
                           c.b = chr * 0.6f; break;
        case A_FUZZ:       c.k = 1.0f + drive * 26.0f; c.a = 0.20f + chr * 0.55f; c.b = 0.90f; break;
        case A_RAZOR:      c.k = 1.0f + drive * 24.0f; c.a = chr; break;
        case A_FOLD:       c.k = 1.0f + drive * 11.0f; c.a = chr * 0.9f; break;
        case A_SINEFOLD:   c.k = 1.0f;
                           c.a = 1.5707963f * (1.0f + drive * 7.0f) * (0.6f + chr * 1.4f); break;
        case A_RECTIFY:    c.k = 1.0f + drive * 8.0f;  c.a = 0.2f + chr * 0.8f; break;
        case A_BITCRUSH:   c.k = 1.0f + drive * 3.0f;
                           c.a = std::pow (2.0f, 1.0f + (1.0f - drive) * 13.0f);
                           c.b = chr; break;
        case A_DECIMATE:   c.k = 1.0f + drive * 3.0f;
                           c.n = 1 + (int) (drive * drive * 70.0f);
                           c.a = 1.0f - chr; break;
        case A_SLEW:       c.k = 1.0f + drive * 3.0f;
                           c.a = xmap (1.0f - drive, 0.9f, 0.0015f);
                           c.b = c.a * (0.2f + chr * 3.5f); break;
        case A_RINGMOD:    c.k = 1.4f;
                           c.a = xmap (chr, 12.0f, 4000.0f) / (float) sr;
                           c.b = drive; break;
        case A_CHEBY:      c.k = 1.0f + drive * 3.0f;  c.a = chr; break;
        case A_SHRED:      c.k = 1.0f + drive * 30.0f;
                           c.a = 0.18f + (1.0f - drive) * 0.6f;
                           c.b = chr * 0.3f; break;
        case A_ANNIHILATE: c.k = 1.0f + drive * 14.0f; c.a = chr; break;
        default: break;
    }
    return c;
}

float shapeSample (int algo, float x, const ShapeCo& co, ShaperState& st, Rng& rng)
{
    const float u = x * co.k;

    switch (algo)
    {
        case A_WARM:
        {
            const float c = clampf (u, -1.0f, 1.0f);
            const float cub = 1.5f * (c - c * c * c / 3.0f);
            return lerpf (cub, std::tanh (u), co.a);
        }

        case A_VALVE:
            // A triode conducts differently either side of zero. Even at zero
            // bias this is lopsided, which is where the second harmonic comes
            // from and why it flatters almost anything.
            return u >= 0.0f ? std::tanh (u)
                             : std::tanh (u * co.a) * (0.6f + 0.4f * co.a);

        case A_OVERDRIVE:
        {
            const float soft = u / (1.0f + std::abs (u));
            return lerpf (soft * 1.6f, clampf (u, -1.0f, 1.0f), co.a);
        }

        case A_TAPE:
        {
            const float t = std::tanh (u);
            st.tapeMem = flushDenorm (st.tapeMem + (t - st.tapeMem) * co.a);
            return lerpf (t, st.tapeMem, co.b);
        }

        case A_FUZZ:
            // Different clip levels either way up: the ugliest kind of honest.
            return clampf (u, -co.b, co.a) / co.a;

        case A_RAZOR:
            return lerpf (std::tanh (u), clampf (u, -1.0f, 1.0f), co.a);

        case A_FOLD:
            return wavefold (u + co.a) - wavefold (co.a);

        case A_SINEFOLD:
            return std::sin (u * co.a);

        case A_RECTIFY:
        {
            const float rect = clampf (2.0f * std::abs (u) - 1.0f, -1.0f, 1.0f);
            return lerpf (clampf (u, -1.0f, 1.0f), rect, co.a);
        }

        case A_BITCRUSH:
        {
            const float dither = co.b * rng.bi() * 0.5f / co.a;
            const float v = clampf (u + dither, -1.0f, 1.0f);
            return std::floor (v * co.a + 0.5f) / co.a;
        }

        case A_DECIMATE:
        {
            if (--st.holdCount <= 0) { st.holdCount = co.n; st.hold = clampf (u, -1.0f, 1.0f); }
            st.last = flushDenorm (st.last + (st.hold - st.last) * (0.12f + co.a * 0.88f));
            return st.last;
        }

        case A_SLEW:
        {
            const float target = clampf (u, -1.0f, 1.0f);
            const float dmax = (target > st.last) ? co.a : co.b;
            st.last = flushDenorm (st.last + clampf (target - st.last, -dmax, dmax));
            return st.last;
        }

        case A_RINGMOD:
        {
            st.phase += co.a;
            if (st.phase >= 1.0f) st.phase -= 1.0f;
            const float m = std::sin (6.2831853f * st.phase);
            return clampf (u, -1.5f, 1.5f) * lerpf (1.0f, m, co.b);
        }

        case A_CHEBY:
        {
            // Straight harmonic injection: the knob slides the emphasis from
            // the second partial up to the fifth.
            const float c  = clampf (u, -1.0f, 1.0f);
            const float c2 = c * c;
            const float t2 = 2.0f * c2 - 1.0f;
            const float t3 = c * (4.0f * c2 - 3.0f);
            const float t4 = 8.0f * c2 * c2 - 8.0f * c2 + 1.0f;
            const float t5 = c * (16.0f * c2 * c2 - 20.0f * c2 + 5.0f);
            const float s  = co.a * 3.0f;
            const float w2 = std::max (0.0f, 1.0f - std::abs (s));
            const float w3 = std::max (0.0f, 1.0f - std::abs (s - 1.0f));
            const float w4 = std::max (0.0f, 1.0f - std::abs (s - 2.0f));
            const float w5 = std::max (0.0f, 1.0f - std::abs (s - 3.0f));
            const float n  = w2 + w3 + w4 + w5 + 1.0e-6f;
            return c * 0.35f + 0.9f * (w2 * t2 + w3 * t3 + w4 * t4 + w5 * t5) / n;
        }

        case A_SHRED:
        {
            float y = std::tanh (u);
            y = clampf (y, -co.a, co.a) / co.a;
            if (std::abs (y) < co.b) y *= 0.12f;     // anything small gets swallowed
            return y;
        }

        case A_ANNIHILATE:
        {
            float y = wavefold (u);
            y = std::floor (y * 8.0f + 0.5f) * 0.125f;
            y = clampf (y * 1.6f, -1.0f, 1.0f);
            return y * (1.0f - co.a * 0.5f) + co.a * 0.5f * std::sin (11.0f * y);
        }

        default: break;
    }
    return x;
}

//==============================================================================
Engine::Engine()
{
    rng.s = 0x9e3779b9u;
}

void Engine::prepare (double sampleRate, int /*blockSize*/)
{
    sr = sampleRate > 8000.0 ? sampleRate : 48000.0;
    for (auto& stage : hb) for (auto& f : stage) f.design();
    reset();
}

void Engine::reset()
{
    for (auto& ch : split) for (auto& s : ch) s.clear();
    for (auto& ch : comp) for (auto& band : ch) for (auto& f : band) f.clear();
    for (auto& b : bs)
    {
        for (auto& s : b.shaper) s.clear();
        for (auto& f : b.tiltLo) f.clear();
        for (auto& f : b.tiltHi) f.clear();
        for (auto& f : b.dc) f.clear();
        b.preSq = b.postSq = 0; b.autoG = 1; b.limEnv = 0; b.gr = 1; b.rms = 0;
    }
    for (auto& stage : hb) for (auto& f : stage) f.clear();
    outDcL.clear(); outDcR.clear();
    lastOut.fill (0.0f); inject.fill (0.0f);
    modOut.fill (0.0f);
    inEnvF = outEnvF = 0.0f;
    for (auto& b : bs) b.env = 0.0f;
    masterEnv = 0; masterGr = 1;
    inRms = outRms = 0;
    ctrlCount = 0;
    nb = std::max (1, std::min (MAX_BANDS, p.bands));
    osFactor = (p.os <= 0) ? 1 : (p.os == 1 ? 2 : 4);
    osSr = sr * osFactor;
    structPhase = 0; structFade = 1.0f; settle = 0;
    structStep = 1.0f / (float) (sr * 0.006);
    applyDerived();
}

//==============================================================================
/*  What a cable carries when it lands on a control. An audio source pointed at
    a knob hands over its envelope instead, so a mis-patch still does something
    sensible rather than nothing at all. */
float Engine::srcControl (int s) const
{
    if (s >= S_ENV1 && s <= S_ENV5) return bs[(size_t) (s - S_ENV1)].env;
    if (s == S_INENV)  return inEnvF;
    if (s == S_OUTENV) return outEnvF;
    if (s >= S_AUD1 && s <= S_AUD5) return bs[(size_t) (s - S_AUD1)].env;
    return 0.0f;
}

//==============================================================================
void Engine::applyDerived()
{
    nbWant = std::max (1, std::min (MAX_BANDS, p.bands));
    osWant = (p.os <= 0) ? 1 : (p.os == 1 ? 2 : 4);
    if ((nbWant != nb || osWant != osFactor) && structPhase == 0) structPhase = 1;
    osSr = sr * osFactor;

    // ---- crossovers: ordered, and never allowed to cross each other -------
    std::array<float, MAX_BANDS - 1> hz {};
    float prev = 20.0f;
    for (int k = 0; k < nb - 1; ++k)
    {
        float f = xoverHz (clamp01 (p.xover[(size_t) k]
                                    + modOut[(size_t) (D_XOVER1 + k)] * 0.25f));
        f = std::max (f, prev * 1.15f);                  // keep a semitone-ish gap
        f = std::min (f, (float) osSr * 0.45f);
        hz[(size_t) k] = f;
        prev = f;
        xoverHzNow[(size_t) k] = f;
        split[0][(size_t) k].set (f, osSr);
        split[1][(size_t) k].set (f, osSr);
    }

    /*  Band k was split off before every later split, so it has to travel
        through the all-pass those later splits leave behind, or the sum stops
        being flat and the crossovers start notching. */
    for (int k = 0; k < MAX_BANDS; ++k) compCount[(size_t) k] = 0;
    for (int k = 0; k < nb - 1; ++k)
    {
        int n = 0;
        for (int j = k + 1; j < nb - 1; ++j)
        {
            comp[0][(size_t) k][(size_t) n].setAP (hz[(size_t) j], osSr, 0.70710678f);
            comp[1][(size_t) k][(size_t) n].copyCoeffs (comp[0][(size_t) k][(size_t) n]);
            ++n;
        }
        compCount[(size_t) k] = n;
    }

    // ---- what the cables are doing ---------------------------------------
    {
        std::array<float, NUM_DESTS> acc {};
        for (const auto& c : p.cable)
        {
            if (c.src <= S_NONE || c.dst <= D_NONE) continue;
            if (isAudioDest (c.dst)) continue;              // handled per sample
            acc[(size_t) c.dst] += srcControl (c.src) * c.amt;
        }
        for (size_t i = 0; i < acc.size(); ++i)
            modOut[i] += (acc[i] - modOut[i]) * 0.08f;
    }

    // ---- per band ---------------------------------------------------------
    bool anySolo = false;
    for (int k = 0; k < nb; ++k) if (p.band[(size_t) k].solo > 0.5f) anySolo = true;

    for (int k = 0; k < MAX_BANDS; ++k)
    {
        const auto& b = p.band[(size_t) k];
        const bool live = (k < nb) && (b.on > 0.5f) && (! anySolo || b.solo > 0.5f);
        d.gate[(size_t) k] += (live ? 1.0f - d.gate[(size_t) k] : -d.gate[(size_t) k]) * 0.05f;

        const float mDrive = clamp01 (b.drive + modOut[(size_t) (D_DRIVE1 + k)]);
        const float mChar  = clamp01 (b.chr   + modOut[(size_t) (D_CHAR1  + k)]);
        const float mLevel = clamp01 (b.level + modOut[(size_t) (D_LEVEL1 + k)]);
        const float mCeil  = clamp01 (b.ceil  + modOut[(size_t) (D_CEIL1  + k)]);

        d.algo[(size_t) k] = std::max (0, std::min (NUM_ALGOS - 1, b.algo));
        d.co[(size_t) k]   = makeCo (d.algo[(size_t) k], mDrive, mChar, osSr);
        d.bias[(size_t) k] = (clamp01 (b.bias) - 0.5f) * 0.9f;
        d.mix[(size_t) k]  = clamp01 (b.mix);
        d.lvl[(size_t) k]  = mLevel * mLevel * 4.0f;                         // -inf .. +12 dB
        d.ceil[(size_t) k] = xmap (mCeil, 0.02f, 1.0f);

        const float dB = (clamp01 (b.tone) - 0.5f) * 24.0f;
        for (int c = 0; c < 2; ++c)
        {
            bs[(size_t) k].tiltLo[(size_t) c].setLowShelf  (700.0f, osSr, 0.5f, -dB);
            bs[(size_t) k].tiltHi[(size_t) c].setHighShelf (700.0f, osSr, 0.5f,  dB);
        }
    }

    d.inG  = clamp01 (p.inGain)  * clamp01 (p.inGain)  * 4.0f;
    d.outG = clamp01 (p.outGain) * clamp01 (p.outGain) * 4.0f;
    d.wet  = clamp01 (p.dryWet);

    d.limAtk  = 1.0f - std::exp (-1.0f / (float) (osSr * 0.0008));   // 0.8 ms
    d.limRel  = 1.0f - std::exp (-1.0f / (float) (osSr * 0.090));    // 90 ms
    d.autoK   = 1.0f - std::exp (-1.0f / (float) (osSr * 0.060));    // RMS window
    d.autoSlew= 1.0f - std::exp (-1.0f / (float) (osSr * 0.150));    // how fast it corrects
    masterRel = 1.0f - std::exp (-1.0f / (float) (sr * 0.120));
    envAtk    = 1.0f - std::exp (-1.0f / (float) (osSr * 0.012));   // 12 ms up
    envRel    = 1.0f - std::exp (-1.0f / (float) (osSr * 0.090));   // 90 ms down

    for (int k = 0; k < MAX_BANDS; ++k)
    {
        bandRms[(size_t) k]  = std::min (1.0f, bs[(size_t) k].rms * 3.0f);
        bandGr[(size_t) k]   = bs[(size_t) k].gr;
        bandAuto[(size_t) k] = bs[(size_t) k].autoG;
    }
}

//==============================================================================
inline void Engine::core (float l, float r, float& ol, float& orr)
{
    float bl[MAX_BANDS], br[MAX_BANDS];
    float restL = l, restR = r;

    for (int k = 0; k < nb - 1; ++k)
    {
        float lo, hi;
        split[0][(size_t) k].process (restL, lo, hi); bl[k] = lo; restL = hi;
        split[1][(size_t) k].process (restR, lo, hi); br[k] = lo; restR = hi;
    }
    bl[nb - 1] = restL;
    br[nb - 1] = restR;

    for (int k = 0; k < nb - 1; ++k)
        for (int j = 0; j < compCount[(size_t) k]; ++j)
        {
            bl[k] = comp[0][(size_t) k][(size_t) j].process (bl[k]);
            br[k] = comp[1][(size_t) k][(size_t) j].process (br[k]);
        }

    /*  Audio cables. They read the previous sample, and every band output has
        already been clamped to its own ceiling, so even a ring of them cannot
        run away — it can only sit there howling, which is the point. */
    inject.fill (0.0f);
    for (const auto& c : p.cable)
    {
        if (! isAudioSource (c.src) || ! isAudioDest (c.dst)) continue;
        inject[(size_t) (c.dst - D_IN1)] += lastOut[(size_t) (c.src - S_AUD1)] * c.amt;
    }

    float sumL = 0.0f, sumR = 0.0f;

    for (int k = 0; k < nb; ++k)
    {
        auto& b = bs[(size_t) k];
        const float fed = inject[(size_t) k] * 0.9f;
        const float inL = bl[k] + fed, inR = br[k] + fed;

        const float det = 0.5f * (inL + inR);
        b.preSq = flushDenorm (b.preSq + (det * det - b.preSq) * d.autoK);

        const int a = d.algo[(size_t) k];
        /*  BIAS offsets the shaper's operating point, which is where the even
            harmonics come from, and the DC blocker below removes the offset it
            leaves behind. RING MOD is the one exception: it MULTIPLIES, so an
            offset on the input comes out as bias * carrier — a tone rather
            than DC, which no DC blocker can touch. The result was a band that
            sang to itself at -39.5 dBFS with nothing plugged in, which on a
            muted track is simply a fault. A multiplier has no operating point
            to offset, so it does not get one. */
        const float bias = (a == A_RINGMOD) ? 0.0f : d.bias[(size_t) k];
        float yl = shapeSample (a, inL + bias, d.co[(size_t) k], b.shaper[0], rng);
        float yr = shapeSample (a, inR + bias, d.co[(size_t) k], b.shaper[1], rng);

        yl = b.tiltHi[0].process (b.tiltLo[0].process (yl));
        yr = b.tiltHi[1].process (b.tiltLo[1].process (yr));
        yl = b.dc[0] (yl);                       // BIAS leaves an offset behind
        yr = b.dc[1] (yr);

        const float dt = 0.5f * (yl + yr);
        b.postSq = flushDenorm (b.postSq + (dt * dt - b.postSq) * d.autoK);

        /*  Match the loudness the shaper changed. Measured rather than
            modelled, so it works for all sixteen without a table — and held
            where it is when the band goes quiet, so silence cannot ramp the
            gain up to meet a noise floor. */
        float want = 1.0f;
        if (p.autoGain > 0.5f)
        {
            if (b.preSq > 1.0e-9f && b.postSq > 1.0e-12f)
                want = clampf (std::sqrt (b.preSq / b.postSq), 0.06f, 8.0f);
            else
                want = b.autoG;
        }
        const float slew = (settle > 0) ? std::min (1.0f, d.autoSlew * 10.0f) : d.autoSlew;
        b.autoG = flushDenorm (b.autoG + (want - b.autoG) * slew);
        yl *= b.autoG;
        yr *= b.autoG;

        // ---- the band's own limiter --------------------------------------
        const float ceil = d.ceil[(size_t) k];
        const float pk = std::max (std::abs (yl), std::abs (yr));
        b.limEnv = flushDenorm (b.limEnv + (pk - b.limEnv) * (pk > b.limEnv ? d.limAtk : d.limRel));
        const float g = (b.limEnv > ceil) ? ceil / b.limEnv : 1.0f;
        b.gr = flushDenorm (b.gr + (g - b.gr) * 0.35f);
        yl = clampf (yl * b.gr, -ceil, ceil);
        yr = clampf (yr * b.gr, -ceil, ceil);

        // what the cables will see next sample, and the follower they read
        lastOut[(size_t) k] = 0.5f * (yl + yr);
        {
            const float a = std::abs (lastOut[(size_t) k]);
            b.env = flushDenorm (b.env + (a - b.env) * (a > b.env ? envAtk : envRel));
        }

        const float mix = d.mix[(size_t) k], lvl = d.lvl[(size_t) k] * d.gate[(size_t) k];
        const float ml = lerpf (inL, yl, mix) * lvl;
        const float mr = lerpf (inR, yr, mix) * lvl;
        b.rms = flushDenorm (b.rms + (0.5f * (std::abs (ml) + std::abs (mr)) - b.rms) * 0.0006f);

        sumL += ml;
        sumR += mr;
    }

    if (settle > 0) --settle;
    ol = sumL;
    orr = sumR;
}

//==============================================================================
void Engine::process (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (ctrlCount == 0) applyDerived();
        ctrlCount = (ctrlCount + 1) % CTRL;

        // ---- restructure fade -------------------------------------------
        if (structPhase == 1)
        {
            structFade -= structStep;
            if (structFade <= 0.0f)
            {
                structFade = 0.0f;
                nb = nbWant;
                osFactor = osWant;
                osSr = sr * osFactor;
                for (auto& ch : split) for (auto& s : ch) s.clear();
                for (auto& ch : comp) for (auto& band : ch) for (auto& f : band) f.clear();
                for (auto& b : bs)
                {
                    for (auto& s : b.shaper) s.clear();
                    for (auto& f : b.tiltLo) f.clear();
                    for (auto& f : b.tiltHi) f.clear();
                    for (auto& f : b.dc) f.clear();
                    /*  The limiter deliberately keeps its detector. Clearing
                        it let the band run wide open for the millisecond its
                        envelope took to catch up again, and that overshoot was
                        louder than the click the fade was there to hide. */
                }
                for (auto& stage : hb) for (auto& f : stage) f.clear();
                applyDerived();
                settle = (int) (osSr * 0.12);
                structPhase = 2;
            }
        }
        else if (structPhase == 2)
        {
            structFade += structStep;
            if (structFade >= 1.0f) { structFade = 1.0f; structPhase = 0; }
        }

        const float dryL = left[i], dryR = right[i];
        const float xl = dryL * d.inG, xr = dryR * d.inG;
        inRms = flushDenorm (inRms + (0.5f * (std::abs (xl) + std::abs (xr)) - inRms) * 0.0006f);
        {
            const float a = 0.5f * (std::abs (xl) + std::abs (xr));
            inEnvF = flushDenorm (inEnvF + (a - inEnvF) * (a > inEnvF ? envAtk : envRel));
        }

        float wl = 0.0f, wr = 0.0f;

        if (osFactor == 1)
        {
            core (xl, xr, wl, wr);
        }
        else if (osFactor == 2)
        {
            float a0, a1, b0, b1, y0, y1, z0, z1;
            hb[0][0].up (xl, a0, a1);
            hb[0][1].up (xr, b0, b1);
            core (a0, b0, y0, z0);
            core (a1, b1, y1, z1);
            wl = hb[0][0].down (y0, y1);
            wr = hb[0][1].down (z0, z1);
        }
        else
        {
            float a0, a1, b0, b1;
            hb[0][0].up (xl, a0, a1);
            hb[0][1].up (xr, b0, b1);

            float p0, p1, q0, q1, y0, y1, z0, z1;
            hb[1][0].up (a0, p0, p1); hb[1][1].up (b0, q0, q1);
            core (p0, q0, y0, z0);
            core (p1, q1, y1, z1);
            const float m0L = hb[1][0].down (y0, y1), m0R = hb[1][1].down (z0, z1);

            hb[1][0].up (a1, p0, p1); hb[1][1].up (b1, q0, q1);
            core (p0, q0, y0, z0);
            core (p1, q1, y1, z1);
            const float m1L = hb[1][0].down (y0, y1), m1R = hb[1][1].down (z0, z1);

            wl = hb[0][0].down (m0L, m1L);
            wr = hb[0][1].down (m0R, m1R);
        }

        wl *= structFade;
        wr *= structFade;
        float ol  = lerpf (dryL, wl, d.wet) * d.outG;
        float orr = lerpf (dryR, wr, d.wet) * d.outG;
        ol  = outDcL (ol);
        orr = outDcR (orr);

        if (p.masterLim > 0.5f)
        {
            const float pk = std::max (std::abs (ol), std::abs (orr));
            if (pk > masterEnv) masterEnv = pk;
            else masterEnv = flushDenorm (masterEnv + (pk - masterEnv) * masterRel);
            const float g = (masterEnv > 0.95f) ? 0.95f / masterEnv : 1.0f;
            masterGr += (g - masterGr) * 0.4f;
            ol *= masterGr;
            orr *= masterGr;
        }
        else masterGr += (1.0f - masterGr) * 0.05f;

        ol  = clampf (ol,  -1.0f, 1.0f);
        orr = clampf (orr, -1.0f, 1.0f);
        outRms = flushDenorm (outRms + (0.5f * (std::abs (ol) + std::abs (orr)) - outRms) * 0.0006f);
        {
            const float a = 0.5f * (std::abs (ol) + std::abs (orr));
            outEnvF = flushDenorm (outEnvF + (a - outEnvF) * (a > outEnvF ? envAtk : envRel));
        }

        left[i]  = ol;
        right[i] = orr;
    }
}

} // namespace mw
