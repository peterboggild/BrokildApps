/*  1984 — engine

    Eight voices, each of two complete ranks (the CS-80 layout): oscillator,
    resonant high-pass, resonant low-pass (12 dB state-variable or a 24 dB
    ladder), a filter envelope with the initial-level / attack-level shape,
    an amplifier ADSR, level and pan. Per voice: a ring modulator, a
    sub-oscillator LFO, touch. Between the ranks: hard sync and poly-mod.
    After the voices, in the oversampled domain: drive, a three-phase
    ensemble, a formant choir and a tape (wow, flutter, saturation, age,
    dropouts, hiss, VHS). After decimation: an eight-line hall with a
    budgeted shimmer. Then the Brokild World FX rack, outside this file.

    What makes it sound like the machines it is after
    -------------------------------------------------
      * The oscillator gives saw, pulse, triangle and sine AT ONCE from one
        phase, each with its own level — the CS-80 brass is saw plus a little
        pulse plus sine, and a single-shape oscillator cannot make it.
      * The filters saturate in their state, so a resonance pushed to
        self-oscillation stays a note. The ladder's linear loop is solved
        before its saturator, so its tuning is exact (Black Rider).
      * The filter envelope has no sustain. Its two levels ARE the shape.
      * Every voice has its own component tolerances and its own slow drift,
        scaled by one VINTAGE knob, so eight voices are eight machines.
      * The chain is oversampled up to and including the tape, so nothing
        that distorts can alias.
      * The hall's shimmer is budgeted against its decay so that the loop
        gain cannot exceed one in any phase: bounded is not stable.

    All numbers quoted in comments are from test/bench.cpp.
*/
#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>

namespace n84
{

static constexpr int MAX_VOICES = 8;
static constexpr int NUM_RANKS  = 2;
static constexpr int MAX_OS     = 4;
static constexpr int SCOPE_N    = 2048;
static constexpr int CTRL       = 8;     // control-rate divider, in oversampled samples

//==============================================================================
inline float clamp01 (float v)                  { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline float xmap (float v, float a, float b)   { return a * std::pow (b / a, clamp01 (v)); }
inline bool  bad (float v)                      { return ! (v > -1.0e9f && v < 1.0e9f); }
inline float clean (float v)                    { return bad (v) ? 0.0f : v; }
inline float midiHz (float n)                   { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }

/*  A ceiling that is transparent well below the limit and asymptotic at it. */
inline float ceilSoft (float x, float c)
{
    const float a = std::abs (x);
    const float knee = c * 0.7f;
    if (a <= knee) return x;
    const float y = knee + (c - knee) * std::tanh ((a - knee) / (c - knee));
    return x < 0.0f ? -y : y;
}

/*  tanh, rational approximation good to ~1e-6 where it matters. */
inline float ftanh (float x)
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float p = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float q = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return p / q;
}

/*  2^x for the pitch path. */
inline float fexp2 (float x)
{
    x = clampf (x, -60.0f, 60.0f);
    const int   i = (int) std::floor (x);
    const float f = x - (float) i;
    const float p = 1.0f + f * (0.69314718f + f * (0.24022652f + f * (0.05550411f
                        + f * (0.00961813f + f * 0.00133336f))));
    union { uint32_t u; float fl; } bits;
    bits.u = (uint32_t) ((i + 127) << 23);
    return p * bits.fl;
}

inline float fsin (float ph)   // ph in cycles; a 5th-order odd polynomial, ~1e-4
{
    ph -= std::floor (ph);
    float x = ph * 4.0f;               // 0..4
    if (x > 2.0f) x -= 4.0f;           // -2..2, triangle-folded below
    if (x > 1.0f) x = 2.0f - x; else if (x < -1.0f) x = -2.0f - x;
    const float x2 = x * x;
    return x * (1.5707963f + x2 * (-0.6459641f + x2 * 0.0796926f));
}

//==============================================================================
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; }
    inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }
    inline float bi()  { return uni() * 2.0f - 1.0f; }
};

struct OnePole
{
    float z = 0.0f, a = 0.1f;
    void setHz (float hz, double sr)
    {
        a = 1.0f - std::exp (-6.2831853f * clampf (hz, 0.001f, (float) sr * 0.45f) / (float) sr);
    }
    inline float lp (float x) { z += (x - z) * a; return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, a = 0.9995f;
    void setHz (float hz, double sr) { a = std::exp (-6.2831853f * hz / (float) sr); }
    inline float operator() (float x) { const float y = x - x1 + a * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float operator() (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }
    void set (float B0, float B1, float B2, float A0, float A1, float A2)
    {
        const float ia = 1.0f / A0;
        b0 = B0 * ia; b1 = B1 * ia; b2 = B2 * ia; a1 = A1 * ia; a2 = A2 * ia;
    }
    void lowpass (float f, float q, double sr)
    {
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void highpass (float f, float q, double sr)
    {
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    // constant 0 dB peak gain band-pass, bandwidth in Hz
    void bandpass (float f, float bwHz, double sr)
    {
        f = clampf (f, 10.0f, (float) sr * 0.45f);
        const float q = f / std::max (1.0f, bwHz);
        const float w = 6.2831853f * f / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (al, 0.0f, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (float f, float gainDb, float q, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float f, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float sq = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float f, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float sq = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

//==============================================================================
struct Delay
{
    std::vector<float> b;
    int w = 0, n = 0;
    void prepare (int maxSamples)
    {
        n = std::max (16, maxSamples + 8);
        b.assign ((size_t) n, 0.0f);
        w = 0;
    }
    void reset() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; if (++w >= n) w = 0; }
    inline float readInt (int d) const
    {
        int r = w - 1 - (d < 0 ? 0 : (d > n - 2 ? n - 2 : d));
        while (r < 0) r += n;
        return b[(size_t) r];
    }
    inline float readHermite (float d) const
    {
        const float dd = clampf (d, 1.0f, (float) (n - 4));
        const int i = (int) dd;
        const float f = dd - (float) i;
        const float y0 = readInt (i - 1), y1 = readInt (i), y2 = readInt (i + 1), y3 = readInt (i + 2);
        const float c0 = y1, c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
};

//==============================================================================
/*  Halfband decimator, 47-tap Kaiser polyphase FIR, stopband below -90 dB.
    Two in, one out. For 4x it runs twice in cascade. (Black Rider.) */
struct HalfDown
{
    static constexpr int TAPS = 47;
    static constexpr int NEV  = 24;
    static constexpr int C    = (TAPS - 1) / 2;
    std::array<float, NEV> he {};
    std::array<float, 32> ev {}, od {};
    int p = 0;
    float centre = 0.5f;

    void prepare()
    {
        std::array<double, TAPS> t {};
        double sum = 0.0;
        const double beta = 9.0;
        auto bessel0 = [] (double x)
        {
            double s = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); s += term; if (term < 1e-12 * s) break; }
            return s;
        };
        const double ib = bessel0 (beta);
        for (int i = 0; i < TAPS; ++i)
        {
            const double x = (double) (i - C);
            const double s = (x == 0.0) ? 0.5 : std::sin (3.14159265358979 * 0.5 * x) / (3.14159265358979 * x);
            const double r = 2.0 * i / (TAPS - 1) - 1.0;
            const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / ib;
            t[(size_t) i] = s * w;
            sum += t[(size_t) i];
        }
        for (auto& v : t) v /= sum;
        for (int k = 0; k < NEV; ++k) he[(size_t) k] = (float) t[(size_t) (2 * k)];
        centre = (float) t[(size_t) C];
        reset();
    }
    void reset() { ev.fill (0.0f); od.fill (0.0f); p = 0; }
    inline float operator() (float a, float b)
    {
        ev[(size_t) p] = a; od[(size_t) p] = b;
        float acc = 0.0f;
        for (int k = 0; k < NEV; ++k) acc += he[(size_t) k] * ev[(size_t) ((p - k) & 31)];
        acc += centre * od[(size_t) ((p - 12) & 31)];
        p = (p + 1) & 31;
        return acc;
    }
};

//==============================================================================
/*  The parameter table — the single source of truth. */
enum PKind
{
    KP_PCT = 0,   // 0..100 %
    KP_SW,        // ON/OFF
    KP_HZ,        // xmap(lo,hi) Hz
    KP_MS,        // xmap(lo,hi) ms
    KP_LIST,      // integer 0..hi, names from listNames(id)
    KP_SEMI,      // (v-0.5)*24 semitones, rounded
    KP_CENT,      // (v-0.5)*lo cents
    KP_CENTU,     // v*lo cents, unipolar
    KP_BIPOL,     // (v-0.5)*200 %
    KP_GLIDE,     // 0 = off, else xmap(lo,hi) ms
    KP_VOL,       // volume in dB
    KP_PW,        // pulse width 50..95 %
    KP_INT,       // plain integer 0..hi
    KP_SEC        // xmap(lo,hi) seconds
};

struct RankParams
{
    float saw = 0.8f, pulse = 0.0f, pw = 0.0f, pwm = 0.0f, tri = 0.0f, sine = 0.0f, noise = 0.0f;
    float oct = 2.0f, semi = 0.5f, fine = 0.5f;
    float hpf = 0.0f, hpq = 0.0f, lpf = 0.75f, lpq = 0.15f, fmode = 0.0f;
    float il = 0.5f, al = 0.75f, fa = 0.2f, fd = 0.5f, fr = 0.5f;
    float va = 0.15f, vd = 0.5f, vs = 0.8f, vr = 0.4f;
    float lvl = 0.8f, pan = 0.5f, vel = 0.5f, velb = 0.3f, sync = 0.0f;
};

struct Params
{
    // ---- global / performance
    float mode = 0, glide = 0, gliss = 0, legato = 1, tune = 0.5f, fine = 0.5f, bend = 2,
          vintage = 0.3f, os = 1, volume = 0.5f, brill = 0.5f, reso = 0.5f, ktrack = 0.5f,
          spread = 0.6f, unidet = 0.3f, patch = 0;
    RankParams rk[NUM_RANKS];
    // ---- ring
    float ring_mode = 0, ring_speed = 0.5f, ring_key = 0, ring_depth = 0, ring_a = 0.2f, ring_d = 0.5f, ring_mod = 0.5f;
    // ---- poly-mod
    float pm_o2pitch = 0, pm_o2pw = 0, pm_o2filt = 0, pm_envpitch = 0.5f, pm_envpw = 0.5f;
    // ---- sub-oscillator (per voice)
    float lfo_wave = 0, lfo_rate = 0.5f, lfo_pitch = 0, lfo_pw = 0, lfo_vcf = 0, lfo_vca = 0, lfo_delay = 0, lfo_mode = 0;
    // ---- wheel and touch
    float wheel_lfo = 0.4f, wheel_brill = 0.5f, wheel_rate = 0.5f;
    float at_pitch = 0, at_brill = 0.3f, at_lvl = 0, at_lfo = 0.3f;
    // ---- chain
    float drv_mode = 0, drv_amt = 0.3f, drv_tone = 0.5f;
    float ens_mode = 0, ens_rate = 0.5f, ens_depth = 0.6f, ens_mix = 0.5f;
    float choir_mix = 0, choir_vowel = 0.5f, choir_reg = 0.5f, choir_air = 0.2f;
    float tape_mode = 0, tape_wow = 0.3f, tape_wowrate = 0.4f, tape_flut = 0.3f, tape_sat = 0.3f,
          tape_age = 0.3f, tape_drop = 0.0f, tape_hiss = 0.2f;
    float hall_mix = 0.25f, hall_pre = 0.3f, hall_size = 0.6f, hall_decay = 0.45f, hall_damp = 0.6f,
          hall_mod = 0.3f, hall_shim = 0.0f;
    // host tempo (not a table param)
    double bpm = 120.0;
};

struct PSpec
{
    const char* id;
    const char* name;
    float def;
    int   kind;
    float lo, hi;
    int   rank;                       // -1 global, else rank index
    float Params::*gp;
    float RankParams::*rp;
    float& ref (Params& p) const { return rank < 0 ? p.*gp : (p.rk[rank].*rp); }
};

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);
const char* const* listNames (const char* id, int& count);
float        paramMax (const PSpec& s);

// knob -> real value, shared by engine, host formatting and bench
float glideMs (float v);
float msOf (const PSpec& s, float v);
float hzOf (const PSpec& s, float v);
float volGain (float v);

//==============================================================================
/*  One band-limited discontinuity inserter (Black Rider). Every oscillator
    output is delayed one sample so the half of a polyBLEP that belongs to
    the sample BEFORE an edge can still be applied to it. */
struct BlepQueue
{
    float prev = 0.0f, cur = 0.0f;
    inline void step (float delta, float f)
    {
        const float g = 1.0f - f;
        prev += 0.5f * delta * f * f;
        cur  -= 0.5f * delta * g * g;
    }
    inline void ramp (float ds, float f)
    {
        const float g = 1.0f - f;
        prev += ds * f * f * f * (1.0f / 6.0f);
        cur  += ds * g * g * g * (1.0f / 6.0f);
    }
};

/*  One phase, four band-limited waves at once. Saw and pulse are stepped,
    the triangle is ramped (polyBLAMP) — never integrated. Hard sync resets
    the phase 'syncF' samples ago and band-limits the actual jump. */
struct MultiOsc
{
    float ph = 0.0f, inc = 0.0f;
    float pendS = 0.0f, pendP = 0.0f, pendT = 0.0f, pendSin = 0.0f;
    BlepQueue qs, qp, qt;
    float saw = 0.0f, pulse = 0.0f, tri = 0.0f, sine = 0.0f;   // outputs (previous sample)
    float wrapF = -1.0f;                                          // >= 0: wrapped this sample, samples ago

    void reset (float phase)
    {
        ph = phase; pendS = pendP = pendT = pendSin = 0.0f;
        qs = {}; qp = {}; qt = {};
        saw = pulse = tri = sine = 0.0f; wrapF = -1.0f;
    }
    static inline float nSaw (float p)            { return 2.0f * p - 1.0f; }
    static inline float nPulse (float p, float pw){ return p < pw ? 1.0f : -1.0f; }
    static inline float nTri (float p)            { return p < 0.5f ? -1.0f + 4.0f * p : 3.0f - 4.0f * p; }

    inline void tick (float frequencyNorm, float pw, float syncF)
    {
        inc = clampf (frequencyNorm, 0.0f, 0.45f);
        pw = clampf (pw, 0.03f, 0.97f);
        const float phPrev = ph;
        float u = ph + inc;
        wrapF = -1.0f;

        float ownWrapF = -1.0f;
        if (u >= 1.0f) { u -= 1.0f; ownWrapF = inc > 0.0f ? u / inc : 0.0f; }
        const bool doSync = syncF >= 0.0f;
        if (doSync && ownWrapF >= 0.0f && ownWrapF < syncF) ownWrapF = -1.0f;

        if (ownWrapF >= 0.0f)
        {
            wrapF = ownWrapF;
            qs.step (-2.0f, ownWrapF);
            qp.step (2.0f, ownWrapF);
            qt.ramp (8.0f * inc, ownWrapF);
        }
        // pulse falling edge and triangle apex, on the unwrapped phase
        {
            const float uu = phPrev + inc;
            auto edgeAt = [&] (float edge) -> float
            {
                if (phPrev < edge && uu >= edge) return (uu - edge) / std::max (inc, 1.0e-9f);
                if (uu - 1.0f >= edge && phPrev >= edge) return (uu - 1.0f - edge) / std::max (inc, 1.0e-9f);
                return -1.0f;
            };
            const float ep = edgeAt (pw);
            if (ep >= 0.0f && ! (doSync && ep < syncF)) qp.step (-2.0f, ep);
            const float et = edgeAt (0.5f);
            if (et >= 0.0f && ! (doSync && et < syncF)) qt.ramp (-8.0f * inc, et);
        }
        ph = u;

        if (doSync)
        {
            float q = phPrev + (1.0f - syncF) * inc; if (q >= 1.0f) q -= 1.0f;
            const float phEdge = q;
            ph = syncF * inc;
            const float dS = nSaw (0.0f) - nSaw (phEdge);
            const float dP = nPulse (0.0f, pw) - nPulse (phEdge, pw);
            const float dT = nTri (0.0f) - nTri (phEdge);
            if (std::abs (dS) > 1.0e-6f) qs.step (dS, syncF);
            if (std::abs (dP) > 1.0e-6f) qp.step (dP, syncF);
            if (std::abs (dT) > 1.0e-6f) qt.step (dT, syncF);
            const float slopeBefore = phEdge < 0.5f ? 4.0f * inc : -4.0f * inc;
            qt.ramp (4.0f * inc - slopeBefore, syncF);
            wrapF = syncF;
        }

        saw   = pendS + qs.prev;  pendS = nSaw (ph) + qs.cur;
        pulse = pendP + qp.prev;  pendP = nPulse (ph, pw) + qp.cur;
        tri   = pendT + qt.prev;  pendT = nTri (ph) + qt.cur;
        sine  = pendSin;          pendSin = fsin (ph) + 0.02f * fsin (2.0f * ph);
        qs.prev = qs.cur = 0.0f; qp.prev = qp.cur = 0.0f; qt.prev = qt.cur = 0.0f;
    }
};

//==============================================================================
/*  ADSR with the analogue shapes: attack charges towards 1.3 and is clamped,
    decay and release are exponential, retrigger continues from the level. */
struct Env
{
    enum Stage { IDLE = 0, ATT, DEC, SUS, REL };
    int stage = IDLE;
    float v = 0.0f;
    float ka = 0.01f, kd = 0.001f, kr = 0.001f, sus = 0.5f;

    void set (float attMs, float decMs, float susLvl, float relMs, double fs)
    {
        ka = 1.0f - std::exp (-1.466f / std::max (1.0f, (float) (attMs * 0.001 * fs)));
        kd = 1.0f - std::exp (-4.0f   / std::max (1.0f, (float) (decMs * 0.001 * fs)));
        kr = 1.0f - std::exp (-4.0f   / std::max (1.0f, (float) (relMs * 0.001 * fs)));
        sus = clamp01 (susLvl);
    }
    void gate (bool on) { if (on) stage = ATT; else if (stage != IDLE) stage = REL; }
    inline float tick()
    {
        switch (stage)
        {
            case ATT: v += (1.3f - v) * ka; if (v >= 1.0f) { v = 1.0f; stage = DEC; } break;
            case DEC: v += (sus - v) * kd; if (v <= sus + 1.0e-4f) { v = sus; stage = SUS; } break;
            case SUS: v = sus; break;
            case REL: v += (0.0f - v) * kr; if (v < 1.0e-5f) { v = 0.0f; stage = IDLE; } break;
            default: break;
        }
        return v;
    }
    bool active() const { return stage != IDLE; }
    void reset() { stage = IDLE; v = 0.0f; }
};

/*  The CS-80 filter envelope: jump to IL at the gate, charge to AL over the
    attack, decay to zero, release to zero. No sustain — the levels are the
    shape. Bipolar, in units of the modulation depth. */
struct FEnv
{
    enum Stage { IDLE = 0, ATT, DEC, REL };
    int stage = IDLE;
    float v = 0.0f;
    float il = 0.0f, al = 0.5f;
    float ka = 0.01f, kd = 0.001f, kr = 0.001f;
    bool  fresh = false;      // gated, not yet told this block's levels

    void set (float ilv, float alv, float attMs, float decMs, float relMs, double fs)
    {
        il = ilv; al = alv;
        if (fresh) { v = il; fresh = false; }
        ka = 1.0f - std::exp (-1.466f / std::max (1.0f, (float) (attMs * 0.001 * fs)));
        kd = 1.0f - std::exp (-4.0f   / std::max (1.0f, (float) (decMs * 0.001 * fs)));
        kr = 1.0f - std::exp (-4.0f   / std::max (1.0f, (float) (relMs * 0.001 * fs)));
    }
    void gate (bool on)
    {
        if (on) { stage = ATT; v = il; fresh = true; }
        else if (stage != IDLE) { stage = REL; fresh = false; }
    }
    inline float tick()
    {
        switch (stage)
        {
            case ATT:
            {
                const float target = al + (al - il) * 0.3f;      // charge past, clamp at AL
                v += (target - v) * ka;
                if ((al >= il && v >= al) || (al < il && v <= al)) { v = al; stage = DEC; }
                break;
            }
            case DEC: v += (0.0f - v) * kd; if (std::abs (v) < 1.0e-4f) { v = 0.0f; stage = IDLE; } break;
            case REL: v += (0.0f - v) * kr; if (std::abs (v) < 1.0e-4f) { v = 0.0f; stage = IDLE; } break;
            default: break;
        }
        return v;
    }
    void reset() { stage = IDLE; v = 0.0f; }
};

//==============================================================================
/*  Topology-preserving state-variable filter with a soft limit on the
    band-pass state, so a resonance pushed to self-oscillation is a bounded
    note. Resonance k = 2(1-q)^1.2 — q = 1 is the edge. */
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f, g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void setG (float gg, float res)
    {
        g = gg;
        /*  A linear SVF with k > 0 only rings; self-oscillation needs the
            loop to be marginally unstable and the state limiter to hold it.
            So the last five per cent of the knob take k just below zero. */
        res = clampf (res, 0.0f, 1.0f);
        if (res <= 0.95f) k = 2.0f * std::pow (1.0f - res, 1.2f);
        else              k = lerp (2.0f * std::pow (0.05f, 1.2f), -0.06f, (res - 0.95f) / 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float lowpass (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.4f * ftanh ((2.0f * v1 - ic1) * (1.0f / 2.4f));
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    inline float highpass (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.4f * ftanh ((2.0f * v1 - ic1) * (1.0f / 2.4f));
        ic2 = 2.0f * v2 - ic2;
        return v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

/*  The four-pole transistor ladder (Black Rider): linear loop solved exactly,
    then the differential pair at the summing node as a tanh. Tuning exact. */
struct Ladder
{
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float G = 0.1f;
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }
    void setG (float g) { G = g / (1.0f + g); }
    inline float tick (float x, float k)
    {
        const float G2 = G * G, G4 = G2 * G2, oneG = 1.0f - G;
        const float S = G2 * G * oneG * s1 + G2 * oneG * s2 + G * oneG * s3 + oneG * s4;
        const float y4lin = (G4 * x + S) / (1.0f + k * G4);
        float u = x - k * y4lin;
        u = 1.3f * ftanh (u * (1.0f / 1.3f));
        auto stage = [this] (float in, float& s) { const float v = G * (in - s); const float y = v + s; s = y + v; return y; };
        auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
        const float y1 = soft (stage (u,  s1));
        const float y2 = soft (stage (y1, s2));
        const float y3 = soft (stage (y2, s3));
        return stage (y3, s4);
    }
};

//==============================================================================
struct RankTol       // component tolerances, fixed per voice per rank
{
    float cents = 0;    // tuning
    float cut = 0;      // octaves
    float env = 1;      // time scale
    float driftPh = 0;
};

struct Rank
{
    MultiOsc osc;
    SVF hp, lp;
    Ladder lad;
    FEnv feg;
    Env  aeg;
    RankTol tol;
    float lastPre = 0.0f;         // pre-filter mix, last OS sample (poly-mod, ring)
    float lastOut = 0.0f;         // post-VCA, last OS sample
    float driftA = 0.0f, driftB = 0.0f;   // slow OU walk, cents
    float jit = 0.0f;
    float gHp = 0.1f, gLp = 0.1f, kLad = 0.0f, resLp = 0.0f, resHp = 0.0f;   // held coefficients
    float pitchNorm = 0.0f;       // frequency / fsOs, held between control ticks
    float pwHeld = 0.5f, ampHeld = 0.0f;
    void reset (float phase)
    {
        osc.reset (phase); hp.reset(); lp.reset(); lad.reset(); feg.reset(); aeg.reset();
        lastPre = lastOut = 0.0f; driftA = driftB = jit = 0.0f;
    }
};

struct Voice
{
    Rank rk[NUM_RANKS];
    Rng  rng;
    int   note = -1, lastNote = 60;
    float vel = 0.8f;
    bool  gate = false;
    int   order = 0;                 // note-on serial, for stealing
    int   slot = -1, member = 0;     // which allocation slot, and which member of it
    uint64_t onAt = 0;
    float pitch = 60.0f;             // glided note number
    float pitchTarget = 60.0f;
    float pan = 0.0f, panTarget = 0.0f;
    float detCents = 0.0f;           // unison/duo fan
    float pressure = 0.0f;           // smoothed aftertouch
    float lfoPh = 0.0f, lfoVal = 0.0f, lfoSH = 0.0f, lfoNz = 0.0f, lfoEnv = 0.0f;
    float ringPh = 0.0f, ringEnv = 0.0f; int ringStage = 0;
    float wmGate = 0.0f;
    float noiseHold = 0.0f;
    int   ctrlPhase = 0;
    DcBlock outDc;
    void reset();
    bool sounding() const { return gate || rk[0].aeg.active() || rk[1].aeg.active(); }
};

//==============================================================================
/*  The drive, in the oversampled domain. A fixed trim per mode and amount,
    measured at prepare over a reference tone — never a tracker. */
struct Drive
{
    static constexpr int NMODE = 5, NPTS = 17;
    std::array<std::array<float, NPTS>, NMODE> trim {};
    OnePole tiltL, tiltR, fuzzHpL, fuzzHpR;
    DcBlock dcL, dcR;
    static inline float shape (int mode, float x, float amt)
    {
        const float g = 1.0f + amt * amt * 24.0f;
        switch (mode)
        {
            case 1: { /*  VALVE. A square wave with unequal tops is DC plus odd
                          harmonics, so asymmetric AMPLITUDE alone gives no
                          warmth; the halves need different curvature. */
                      const float b = x * g * 0.7f;
                      const float u = b + 0.45f * b * b / (1.0f + std::abs (b));
                      return ftanh (u); }
            case 2: { const float u = x * g * 0.6f; return u / std::sqrt (1.0f + u * u); }   // TAPE: soft knee
            case 3: { return ftanh (x * g * 2.2f); }                                          // FUZZ
            case 4: { return fsin (x * g * 0.22f) ; }                                        // FOLD (fsin takes cycles)
            default: return x;
        }
    }
    void prepare (double fsOs);
    void reset() { tiltL.reset(); tiltR.reset(); fuzzHpL.reset(); fuzzHpR.reset(); dcL.reset(); dcR.reset(); }
    inline float trimAt (int mode, float amt) const
    {
        const float x = clamp01 (amt) * (NPTS - 1);
        const int i = std::min (NPTS - 2, (int) x);
        const float f = x - (float) i;
        return lerp (trim[(size_t) mode][(size_t) i], trim[(size_t) mode][(size_t) i + 1], f);
    }
    inline void tick (int mode, float amt, float tone, float& l, float& r)
    {
        if (mode <= 0) return;
        if (mode == 3) { l = fuzzHpL.hp (l); r = fuzzHpR.hp (r); }
        const float t = trimAt (mode, amt);
        l = dcL (shape (mode, l, amt)) * t;
        r = dcR (shape (mode, r, amt)) * t;
        // tilt: 0.5 flat, low = dark, high = bright
        const float tt = (tone - 0.5f) * 2.0f;
        const float ll = tiltL.lp (l), rl = tiltR.lp (r);
        l = l + tt * (l - 2.0f * ll) * 0.5f;
        r = r + tt * (r - 2.0f * rl) * 0.5f;
    }
};

/*  Three modulated taps per channel, two LFOs (slow and fast), phases 120°
    apart, the right channel 60° on: the Solina / VP-330 circuit. */
struct EnsembleFx
{
    Delay dL, dR;
    OnePole bbdL, bbdR;
    float phS = 0.0f, phF = 0.0f;
    float fsOs = 96000.0f;
    void prepare (double fs);
    void reset() { dL.reset(); dR.reset(); bbdL.reset(); bbdR.reset(); phS = phF = 0.0f; }
    void tick (int mode, float rate, float depth, float mix, float& l, float& r);
};

/*  Five band-pass sections at the formants of a vowel, morphing U-O-A-E-I. */
struct ChoirFx
{
    std::array<Biquad, 5> bL, bR;
    std::array<float, 5> amp {};
    Biquad airL, airR;
    OnePole darkL, darkR;
    float fsOs = 96000.0f;
    int   ctr = 0;
    void prepare (double fs);
    void reset() { for (auto& b : bL) b.reset(); for (auto& b : bR) b.reset(); airL.reset(); airR.reset(); darkL.reset(); darkR.reset(); ctr = 0; }
    void update (float vowel, float reg, float air);
    inline void tick (float mix, float& l, float& r)
    {
        if (mix <= 0.0005f) return;
        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < 5; ++i) { wl += bL[(size_t) i] (l) * amp[(size_t) i]; wr += bR[(size_t) i] (r) * amp[(size_t) i]; }
        wl = airL (darkL.lp (wl)) * 2.4f; wr = airR (darkR.lp (wr)) * 2.4f;
        l = lerp (l, wl, mix); r = lerp (r, wr, mix);
    }
};

/*  Tape: a modulated delay carrying wow and flutter, a biased saturator with
    a head bump, an age loss, dropouts, hiss added AFTER everything. VHS mode
    swaps the flutter for head-switching and caps the bandwidth. */
struct TapeFx
{
    Delay dL, dR;
    Rng   rng;
    float fsOs = 96000.0f;
    float wowPh = 0.0f, walk = 0.0f, walkTgt = 0.0f, walkSm = 0.0f;
    float fl1 = 0.0f, fl2 = 0.0f, fl3 = 0.0f, flOu = 0.0f, flOuTgt = 0.0f;
    float vhsPh = 0.0f;
    float dropEnv = 0.0f, dropTarget = 0.0f; int dropLeft = 0; float dropPos = 0.0f, dropLen = 1.0f;
    float satTrim[17] {};
    Biquad bumpL, bumpR, vhsPkL, vhsPkR, chromaL, chromaR;
    OnePole ageL, ageR, ageL2, ageR2, hissLp, dropLpL, dropLpR, wowLp;
    float envF = 0.0f, envAtk = 0.01f, envRel = 0.0001f;   // the signal follower that gates the hiss
    DcBlock dcL, dcR;
    float tickEnv = 0.0f;
    int   walkCtr = 0;
    // for the panel
    float uiWow = 0.0f; bool uiDrop = false;
    void prepare (double fs);
    void reset();
    void tick (int mode, float wow, float wowRate, float flut, float sat, float age, float drop, float hiss, float& l, float& r);
};

/*  Reads its own delay line at twice the rate through two crossfaded
    windows: an octave up with a soft seam. */
struct OctaveUp
{
    Delay d;
    float ph = 0.0f, win = 2400.0f;
    void prepare (double sr) { win = (float) sr * 0.05f; d.prepare ((int) win * 2 + 8); ph = 0.0f; }
    inline float tick (float x)
    {
        d.push (x);
        ph += 1.0f; if (ph >= win) ph -= win;
        float p2 = ph + win * 0.5f; if (p2 >= win) p2 -= win;
        const float w1 = 0.5f - 0.5f * std::cos (6.2831853f * ph / win);
        const float w2 = 0.5f - 0.5f * std::cos (6.2831853f * p2 / win);
        return d.readHermite (win - ph) * w1 + d.readHermite (win - p2) * w2;
    }
    void reset() { d.reset(); ph = 0.0f; }
};

/*  Eight-line FDN behind diffusers; decay from an RT60; shimmer BEFORE the
    damping and budgeted against the decay so that g + L < 1 on every line. */
struct HallFx
{
    static constexpr int N = 8;
    Delay line[N], apL[2], apR[2], pre[2];
    OnePole damp[N];
    float apLen[4] { 142.0f, 379.0f, 107.0f, 277.0f };
    float baseLen[N] { 1116.0f, 1188.0f, 1277.0f, 1356.0f, 1422.0f, 1491.0f, 1557.0f, 1617.0f };
    float len[N] {}, g[N] {}, shimG[N] {};
    float srScale = 1.0f, fs = 48000.0f;
    float modPh = 0.0f;
    int   preSamples = 0;
    OctaveUp shimmer;
    OnePole shHp;
    float mixSm = 0.0f;
    void prepare (double sr);
    void reset();
    void set (float size, float rt60, float dampHz, float shim);
    void tick (float inL, float inR, float mod, float mix, float& outL, float& outR);
};

//==============================================================================
class Engine
{
public:
    Params p;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* L, float* R, int n);

    void noteOn (int note, float vel);
    void noteOff (int note);
    void allNotesOff();
    void setBend (float v)         { bendIn = clampf (v, -1.0f, 1.0f); }
    void setWheel (float v)        { wheelIn = clamp01 (v); }
    void setAftertouch (float v)   { atIn = clamp01 (v); }
    void setPolyAftertouch (int note, float v) { if (note >= 0 && note < 128) polyAt[(size_t) note] = clamp01 (v); }
    void setSustain (bool on);

    // Brokild World FX world-mod bus; a NEUTRAL bus is skipped entirely
    void setWorldMod (float detCents, float panSpread, float tremDepth,
                      float tremRateHz, float sagSemis, float filterMul)
    {
        wmIn[0].store (detCents,   std::memory_order_relaxed);
        wmIn[1].store (panSpread,  std::memory_order_relaxed);
        wmIn[2].store (tremDepth,  std::memory_order_relaxed);
        wmIn[3].store (tremRateHz, std::memory_order_relaxed);
        wmIn[4].store (sagSemis,   std::memory_order_relaxed);
        wmIn[5].store (filterMul,  std::memory_order_relaxed);
    }

    // ---- for the panel
    float outRms = 0.0f, outPeak = 0.0f;
    std::array<float, NUM_RANKS> uiFeg {}, uiAeg {}, uiCut {};
    float uiLfo = 0.0f, uiWow = 0.0f;
    bool  uiDrop = false;
    std::array<float, SCOPE_N> scope {};
    int   scopeWrite = 0;
    std::array<int, MAX_VOICES> uiNotes { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<float, MAX_VOICES> uiLevel {};
    int   heldCount = 0;

    double sr = 48000.0;
    int    osf = 2;
    double fsOs = 96000.0;

    // debug accessors for the bench
    int  voicesSounding() const;
    int  newestVoice() const;
    float voicePitch (int vi) const { return voices[(size_t) vi].pitch; }

private:
    void configureRate();
    void deriveBlock();
    void renderVoice (Voice& v, int vi, float* outL, float* outR, int nOs);
    void controlTick (Voice& v, int vi, int tick);
    int  allocSlot (int note);
    void startVoice (Voice& v, int note, float vel, int slot, int member, int vpn, bool retrigger, float fromPitch);
    int  voicesPerNote() const;

    std::array<Voice, MAX_VOICES> voices;
    int   noteSerial = 0;
    uint64_t samplesDone = 0;
    std::array<int, 32> heldStack {}; int heldN = 0;
    bool  sustain = false;
    std::array<bool, 128> heldKeys {};
    std::array<bool, 128> susKeys {};
    std::array<float, 128> polyAt {};
    float lastMonoPitch = 60.0f;

    float bendIn = 0.0f, wheelIn = 0.0f, atIn = 0.0f;
    float bendSm = 0.0f, wheelSm = 0.0f, atSm = 0.0f;
    std::vector<float> bendBuf, wheelBuf, atBuf;   // per control tick, filled once per block
    float perfK = 0.01f;                            // the 8 ms one-pole, per control tick
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFmul = 1;
    bool  wmActive = false;
    double wmT = 0;

    // global lfo (the ONE mode, and the wheel vibrato)
    float gLfoPh = 0.0f, gLfoVal = 0.0f, gLfoSH = 0.0f, gLfoNz = 0.0f;
    float wheelLfoPh = 0.0f;
    Rng   lfoRng;

    // per-block derived values
    struct RankBlock
    {
        float octMul = 1, semis = 0, hpfHz = 10, lpfHz = 4000, hpq = 0, lpq = 0;
        bool  ladder = false;
        float il = 0, al = 0, fa = 10, fd = 500, fr = 500, va = 10, vd = 500, vs = 0.8f, vr = 500;
        float lvl = 0.8f, pan = 0, vel = 0.5f, velb = 0.3f, sync = false, pw = 0.5f, pwm = 0;
        float saw = 0.8f, pulse = 0, tri = 0, sine = 0, noise = 0;
    };
    RankBlock rb[NUM_RANKS];
    float bendRange = 2, glideK = 0, tuneSemis = 0, lfoInc = 0, lfoPitchCents = 0, ringHz = 1, wheelLfoInc = 0;
    float ringRatio = 1.0f;
    int   lfoWave = 0, lfoMode = 0, ringMode = 0, modeI = 0;
    bool  gliss = false;
    float lfoDelayK = 0.0f;

    std::vector<float> osL, osR;
    HalfDown dnL1, dnR1, dnL2, dnR2;

    Drive      drive;
    EnsembleFx ens;
    ChoirFx    choir;
    TapeFx     tape;
    HallFx     hall;

    Rng   hissRng;
    DcBlock outDcL, outDcR;
    float rmsAcc = 0.0f, peakAcc = 0.0f; int rmsN = 0;
    int   scopeHold = 0; float scopePrev = 0.0f;
    int   maxBlock = 512;
    int   lastOsParam = -1;
    int   fxCtr = 0;
    std::array<float, MAX_VOICES> ringIncHeld {};
    float panG[MAX_VOICES][NUM_RANKS][2] {};
    float hallLast[4] { -1.0f, -1.0f, -1.0f, -1.0f };
};

} // namespace n84
