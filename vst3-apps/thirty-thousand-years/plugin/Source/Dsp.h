/*  Thirty Thousand Years — DSP primitives.

    Plain C++, no JUCE: the bench compiles this without a framework. Every
    block here is a building brick used by more than one stratum, so it is
    written once and measured once (test/bench.cpp).

    Rules that hold everywhere in this file:
      * nothing allocates after prepare(); std::vector is sized once;
      * every recursive element has a reset() and clean()s its state on a
        non-finite value, because a feedback instrument must recover;
      * frequencies are clamped below Nyquist before a tan() or a cos().
*/
#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <atomic>

namespace tty
{

static constexpr float PI  = 3.14159265358979f;
static constexpr float TAU = 6.28318530717959f;

//==============================================================================
inline float clamp01 (float v)                  { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline int   clampi (int v, int a, int b)       { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline float xmap (float v, float a, float b)   { return a * std::pow (b / a, clamp01 (v)); }
inline float xunmap (float x, float a, float b) { return std::log (clampf (x, a, b) / a) / std::log (b / a); }
inline bool  bad (float v)                      { return ! (v > -1.0e9f && v < 1.0e9f); }
inline float clean (float v)                    { return bad (v) ? 0.0f : v; }
inline float midiHz (float n)                   { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }
inline float hzMidi (float f)                   { return 69.0f + 12.0f * std::log2 (std::max (1.0e-3f, f) / 440.0f); }
inline float dbGain (float db)                  { return std::pow (10.0f, db / 20.0f); }
inline float gainDb (float g)                   { return 20.0f * std::log10 (std::max (1.0e-9f, g)); }

/*  A ceiling that is transparent below 70 % of the limit and asymptotic at it. */
inline float ceilSoft (float x, float c)
{
    const float a = std::abs (x);
    const float knee = c * 0.7f;
    if (a <= knee) return x;
    const float y = knee + (c - knee) * std::tanh ((a - knee) / (c - knee));
    return x < 0.0f ? -y : y;
}

/*  A hard knee: identity below the knee, tanh above, C1 at the join. Used
    where transparency below the threshold is the whole point. */
inline float kneeSat (float x, float knee)
{
    const float a = std::abs (x);
    if (a <= knee) return x;
    const float y = knee + std::tanh (a - knee);
    return x < 0.0f ? -y : y;
}

inline float ftanh (float x)   // rational approximation, ~1e-6 inside +-5
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float p = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float q = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return p / q;
}

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

inline float fsin (float ph)   // ph in cycles; 7th-order odd polynomial, peak error ~5e-5 (the 5th-order one overshot 1.0 by 0.45 %)
{
    ph -= std::floor (ph);
    float x = ph * 4.0f;
    if (x > 2.0f) x -= 4.0f;
    if (x > 1.0f) x = 2.0f - x; else if (x < -1.0f) x = -2.0f - x;
    const float x2 = x * x;
    return x * (1.5707963f + x2 * (-0.6459641f + x2 * (0.0796926f - x2 * 0.0046817f)));
}
inline float fcos (float ph) { return fsin (ph + 0.25f); }

//==============================================================================
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; for (int i = 0; i < 4; ++i) u32(); }
    inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }
    inline float bi()  { return uni() * 2.0f - 1.0f; }
    inline float gauss() { return (bi() + bi() + bi()) * 0.7071f; }   // near enough, bounded
};

/*  A stateless hash so an event at (cycle, step, salt) is the same on a bounce
    as it was on playback. */
inline uint32_t hash32 (uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
inline float hash01 (uint32_t a, uint32_t b, uint32_t c) { return (float) (hash32 (a, b, c) >> 8) * (1.0f / 16777216.0f); }

//==============================================================================
struct OnePole
{
    float z = 0.0f, a = 0.1f;
    void setHz (float hz, double sr) { a = 1.0f - std::exp (-TAU * clampf (hz, 0.0001f, (float) sr * 0.45f) / (float) sr); }
    void setTau (float seconds, double sr) { a = 1.0f - std::exp (-1.0f / std::max (1.0e-6f, seconds * (float) sr)); }
    inline float lp (float x) { z += (x - z) * a; return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset (float v = 0.0f) { z = v; }
};

struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, a = 0.9995f;
    void setHz (float hz, double sr) { a = std::exp (-TAU * hz / (float) sr); }
    inline float operator() (float x) { const float y = x - x1 + a * y1; x1 = x; y1 = clean (y); return y1; }
    void reset() { x1 = y1 = 0.0f; }
};

/*  A slew that follows exponentially: one call per control tick. */
struct Smooth
{
    float z = 0.0f, a = 0.05f;
    void setTau (float seconds, double rate) { a = 1.0f - std::exp (-1.0f / std::max (1.0e-6f, seconds * (float) rate)); }
    inline float to (float target) { z += (target - z) * a; return z; }
    void set (float v) { z = v; }
};

//==============================================================================
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
    void cleanState() { if (bad (z1) || bad (z2)) reset(); }
    void set (float B0, float B1, float B2, float A0, float A1, float A2)
    {
        const float ia = 1.0f / A0;
        b0 = B0 * ia; b1 = B1 * ia; b2 = B2 * ia; a1 = A1 * ia; a2 = A2 * ia;
    }
    void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void lowpass (float f, float q, double sr)
    {
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void highpass (float f, float q, double sr)
    {
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (float f, float q, double sr)   // 0 dB peak
    {
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (al, 0.0f, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (float f, float gainDb, float q, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float f, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float sq = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float f, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
        const float sq = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void allpass (float f, float q, double sr)
    {
        const float w = TAU * clampf (f, 5.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
};

//==============================================================================
/*  A 2-pole resonator for the modal bank: y = x*g + 2 r cos(w) y1 - r² y2.
    Decay given as T60. g = sin(w)/4, so an IMPULSE rings at about a quarter
    of its size whatever the decay; the steady-state gain at resonance is then
    g/(1-r), enormous for a long decay — CONTINUOUS excitation is scaled down
    by the caller with `contScale` (STRUCTURE), which the first version of
    this file got backwards (strikes were inaudible). */
struct Resonator
{
    float a1 = 0, a2 = 0, g = 0, y1 = 0, y2 = 0, r = 0.999f;
    void set (float hz, float t60, double sr)
    {
        hz = clampf (hz, 5.0f, (float) sr * 0.47f);
        r = std::exp (-6.9078f / std::max (1.0e-3f, t60 * (float) sr));
        const float w = TAU * hz / (float) sr;
        a1 = 2.0f * r * std::cos (w);
        a2 = -r * r;
        g  = 0.25f * std::sin (w);
    }
    inline float operator() (float x)
    {
        const float y = g * x + a1 * y1 + a2 * y2;
        y2 = y1; y1 = y;
        return y;
    }
    void reset() { y1 = y2 = 0.0f; }
    void cleanState() { if (bad (y1) || bad (y2)) reset(); }
    float energy() const { return y1 * y1 + y2 * y2; }
};

//==============================================================================
/*  TPT state-variable filter with a tanh on the band-pass state, so a
    resonance driven to self-oscillation stays a note (1984). */
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    void set (float hz, float q, double sr)
    {
        g  = std::tan (PI * clampf (hz, 5.0f, (float) sr * 0.45f) / (float) sr);
        k  = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    /*  mode 0 LP, 1 BP, 2 HP, 3 NOTCH.
        sat is a DRIVE, not a level: ftanh(v*sat)/sat is the identity for small
        sat and a saturator for large, so a filter at rest must be given a
        small one. It used to be handed 0.6 at zero drive and coloured every
        note (measured -35 dB THD on a sine). */
    inline float operator() (float x, int mode, float sat = 1.0f)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = ftanh (sat * (2.0f * v1 - ic1)) / sat;
        ic2 = 2.0f * v2 - ic2;
        switch (mode)
        {
            case 0:  return v2;
            case 1:  return v1;
            case 2:  return x - k * v1 - v2;
            default: return x - k * v1;
        }
    }
    void reset() { ic1 = ic2 = 0.0f; }
    void cleanState() { if (bad (ic1) || bad (ic2)) reset(); }
};

/*  ZDF four-pole ladder with tanh at the summing node: the linear loop is
    solved before the saturator so the tuning stays exact (Black Rider). */
struct Ladder
{
    float s[4] = { 0, 0, 0, 0 };
    float G = 0.1f, k = 0.0f;
    /*  The saturator in the summing node is what gives the ladder its
        character, but it has to be REACHED rather than always on: at drive 0
        the loop is linear to a thousandth. */
    void set (float hz, float res, double sr)
    {
        const float g = std::tan (PI * clampf (hz, 5.0f, (float) sr * 0.45f) / (float) sr);
        G = g / (1.0f + g);
        k = clampf (res, 0.0f, 1.05f) * 4.0f;
    }
    inline float operator() (float x, float drive = 1.0f)
    {
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = G3 * s[0] + G2 * s[1] + G * s[2] + s[3];   // the ladder's state sum
        // yes: y4 = G^4 * u + S where u = tanh(x - k*y4); solve linearly for the loop, then saturate
        const float u = (x - k * S) / (1.0f + k * G4);
        /*  Two jobs, kept apart. DRIVE colours the loop: ftanh(u*d)/d is the
            identity for small d, so at rest this is a clean filter. The soft
            ceiling bounds it: transparent below 1.4, asymptotic at 2.0, which
            is what stops a self-oscillating resonance from running away
            without colouring every note on the way. */
        float v = ceilSoft (ftanh (u * drive) / drive, 2.0f);
        for (int i = 0; i < 4; ++i)
        {
            const float y = G * (v - s[i]) + s[i];
            s[i] = 2.0f * y - s[i];
            v = y;
        }
        return v;
    }
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
    void cleanState() { for (float& v : s) if (bad (v)) { reset(); return; } }
};

//==============================================================================
/*  polyBLEP oscillator: sine, triangle (integrated square), saw, pulse. */
struct Blep
{
    float ph = 0.0f, inc = 0.0f, tri = 0.0f;
    static inline float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    inline void setHz (float hz, double sr) { inc = clampf (hz / (float) sr, 0.0f, 0.49f); }
    // returns the sample and advances; wave 0 sine, 1 tri, 2 saw, 3 pulse
    inline float tick (int wave, float pw, bool& wrapped)
    {
        float out;
        const float dt = std::max (1.0e-6f, inc);
        switch (wave)
        {
            case 0: out = fsin (ph); break;
            case 2: out = 2.0f * ph - 1.0f - blep (ph, dt); break;
            case 3:
            {
                float t2 = ph + pw; t2 -= std::floor (t2);
                out = (ph < pw ? 1.0f : -1.0f) + blep (ph, dt) - blep (t2, dt);
                break;
            }
            default:
            {
                float t2 = ph + 0.5f; t2 -= std::floor (t2);
                const float sq = (ph < 0.5f ? 1.0f : -1.0f) + blep (ph, dt) - blep (t2, dt);
                tri += 4.0f * dt * sq;              // integrate the square about its mean
                tri -= tri * dt * 2.0f;             // leak
                out = clampf (tri, -1.0f, 1.0f);
                break;
            }
        }
        ph += inc;
        wrapped = ph >= 1.0f;
        if (wrapped) ph -= 1.0f;
        return out;
    }
    void reset (float p = 0.0f) { ph = p; tri = 0.0f; }
};

//==============================================================================
struct Delay
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    inline void write (float v) { buf[(size_t) w] = v; w = (w + 1) & mask; }
    inline float readInt (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    inline float read (float d) const
    {
        d = clampf (d, 1.0f, (float) mask - 3.0f);
        const int i = (int) d; const float f = d - (float) i;
        const int p0 = (w - 1 - i) & mask;
        const float y0 = buf[(size_t) ((p0 + 1) & mask)], y1 = buf[(size_t) p0],
                    y2 = buf[(size_t) ((p0 - 1) & mask)], y3 = buf[(size_t) ((p0 - 2) & mask)];
        // Hermite
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    inline float readLin (float d) const
    {
        d = clampf (d, 0.0f, (float) mask - 2.0f);
        const int i = (int) d; const float f = d - (float) i;
        return lerp (readInt (i), readInt (i + 1), f);
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int size() const { return mask + 1; }
};

/*  Schroeder allpass on a delay line. */
struct Allpass
{
    Delay d; int len = 1; float g = 0.5f;
    void prepare (int maxLen) { d.prepare (maxLen); }
    inline float operator() (float x)
    {
        const float v = d.readInt (len - 1);
        const float u = x + g * v;
        d.write (u);
        return v - g * u;
    }
};

//==============================================================================
/*  A polyphase half-band FIR for 2x up and down. 47 taps, Kaiser-windowed,
    transition centred on fs/4 of the higher rate. Measured (test/probe.cpp
    halfband): passband flat, 32 kHz at 96 k down 77 dB, 36 kHz down 87 dB.

    The interpolator is polyphase over the LOW-rate history: the odd-n taps of
    a half-band are zero except the centre, so the aligned output is a pure
    delay and only the other branch is a real FIR. The first version stepped
    both branches through the history one tap per sample and was worse than
    no oversampling at all (measured). */
struct HalfBand
{
    static constexpr int NT = 47;
    float h[NT];
    float zlo[64]; int wlo = 0;          // low-rate history for up()
    std::vector<float> zd;               // high-rate history for down()
    int wd = 0;
    HalfBand()
    {
        const float beta = 7.0f;
        auto bessel0 = [] (float x) { float s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0f * k)) * (x / (2.0f * k)); s += t; } return s; };
        const int M = NT / 2;
        for (int i = 0; i < NT; ++i)
        {
            const int n = i - M;
            float s = n == 0 ? 0.5f : std::sin (PI * 0.5f * n) / (PI * n);
            const float r = (float) n / (float) M;
            const float wnd = bessel0 (beta * std::sqrt (std::max (0.0f, 1.0f - r * r))) / bessel0 (beta);
            h[i] = s * wnd;
        }
        std::fill (std::begin (zlo), std::end (zlo), 0.0f);
        zd.assign (NT * 2, 0.0f);
    }
    void reset() { std::fill (std::begin (zlo), std::end (zlo), 0.0f); std::fill (zd.begin(), zd.end(), 0.0f); wlo = wd = 0; }
    // one input sample -> two output samples (at 2x): the interpolated one first, then the aligned one
    inline void up (float x, float& o0, float& o1)
    {
        zlo[wlo & 63] = x;
        float a = 0.0f;
        for (int j = 0; j < (NT + 1) / 2; ++j) a += h[2 * j] * zlo[(wlo - j) & 63];   // even i = odd n: the real FIR
        o0 = 2.0f * a;
        o1 = zlo[(wlo - NT / 4) & 63];                                                // the centre tap (0.5 * 2) at its delay
        ++wlo;
    }
    // two input samples (at 2x) -> one output sample
    inline float down (float x0, float x1)
    {
        zd[(size_t) wd] = zd[(size_t) (wd + NT)] = x0; wd = (wd + 1) % NT;
        zd[(size_t) wd] = zd[(size_t) (wd + NT)] = x1; wd = (wd + 1) % NT;
        const float* z = &zd[(size_t) (wd + NT - 1)];
        float a = 0.0f;
        for (int i = 0; i < NT; ++i) a += h[i] * z[-i];
        return a;
    }
    static int latency() { return NT / 2; }   // in 2x samples per direction
};

//==============================================================================
/*  Hilbert pair for the frequency shifter: two 4-section allpass cascades
    (Niemitalo) whose outputs are 90 degrees apart across 20 Hz–20 kHz. */
struct Hilbert
{
    static constexpr float A0[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    static constexpr float A1[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
    float x0[4][2] = {}, y0[4][2] = {}, x1[4][2] = {}, y1[4][2] = {}, d1 = 0.0f;
    static inline float sec (float a, float in, float* xs, float* ys)
    {
        const float a2 = a * a;
        const float y = a2 * (in + ys[1]) - xs[1];
        xs[1] = xs[0]; xs[0] = in; ys[1] = ys[0]; ys[0] = y;
        return y;
    }
    inline void operator() (float in, float& re, float& im)
    {
        float v = in;
        for (int i = 0; i < 4; ++i) v = sec (A0[i], v, x0[i], y0[i]);
        re = v;
        float u = d1; d1 = in;    // one-sample delay on the second branch
        for (int i = 0; i < 4; ++i) u = sec (A1[i], u, x1[i], y1[i]);
        im = u;
    }
    void reset() { std::memset (x0, 0, sizeof (x0)); std::memset (y0, 0, sizeof (y0)); std::memset (x1, 0, sizeof (x1)); std::memset (y1, 0, sizeof (y1)); d1 = 0.0f; }
    void cleanState() { for (int i = 0; i < 4; ++i) if (bad (y0[i][0]) || bad (y1[i][0])) { reset(); return; } }
};

/*  Single-sideband frequency shifter. The delayed branch of the Niemitalo
    network LEADS by 90 degrees (measured: re*cos - im*sin shifted a 440 Hz
    sine DOWN to 340), so the up-shift is re*cos + im*sin. */
struct FreqShifter
{
    Hilbert hb; float ph = 0.0f, inc = 0.0f;
    void setHz (float hz, double sr) { inc = clampf (hz / (float) sr, -0.49f, 0.49f); }
    inline float operator() (float x)
    {
        float re, im; hb (x, re, im);
        const float c = fcos (ph), s = fsin (ph);
        ph += inc; ph -= std::floor (ph);
        return re * c + im * s;
    }
    void reset() { hb.reset(); ph = 0.0f; }
};

//==============================================================================
/*  Radix-2 complex FFT, in place, precomputed twiddles. Real signals are
    packed by the caller. Sizes are powers of two. */
struct Fft
{
    int n = 0, log2n = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
    void prepare (int size)
    {
        n = size; log2n = 0; while ((1 << log2n) < n) ++log2n;
        cosT.resize ((size_t) n / 2); sinT.resize ((size_t) n / 2); rev.resize ((size_t) n);
        for (int i = 0; i < n / 2; ++i) { cosT[(size_t) i] = std::cos (TAU * i / n); sinT[(size_t) i] = std::sin (TAU * i / n); }
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < log2n; ++b) if (i & (1 << b)) r |= 1 << (log2n - 1 - b); rev[(size_t) i] = r; }
    }
    void forward (float* re, float* im) const { run (re, im, false); }
    void inverse (float* re, float* im) const { run (re, im, true); const float s = 1.0f / (float) n; for (int i = 0; i < n; ++i) { re[i] *= s; im[i] *= s; } }
    void run (float* re, float* im, bool inv) const
    {
        for (int i = 0; i < n; ++i) { const int j = rev[(size_t) i]; if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float c = cosT[(size_t) (j * step)], s = inv ? sinT[(size_t) (j * step)] : -sinT[(size_t) (j * step)];
                    const float ur = re[i + j], ui = im[i + j];
                    const float vr = re[i + j + half] * c - im[i + j + half] * s;
                    const float vi = re[i + j + half] * s + im[i + j + half] * c;
                    re[i + j] = ur + vr; im[i + j] = ui + vi;
                    re[i + j + half] = ur - vr; im[i + j + half] = ui - vi;
                }
        }
    }
};

//==============================================================================
struct Adsr
{
    enum { IDLE, ATT, DEC, SUS, REL };
    int st = IDLE; float v = 0.0f, aC = 0.0f, dC = 0.0f, rC = 0.0f, sus = 0.7f, vel = 1.0f;
    void set (float aMs, float dMs, float s, float rMs, double rate)
    {
        // RC-shaped: coefficient such that 63 % of the distance is covered in the time
        aC = 1.0f - std::exp (-1.0f / std::max (1.0f, aMs * 0.001f * (float) rate));
        dC = 1.0f - std::exp (-1.0f / std::max (1.0f, dMs * 0.001f * (float) rate));
        rC = 1.0f - std::exp (-1.0f / std::max (1.0f, rMs * 0.001f * (float) rate));
        sus = clamp01 (s);
    }
    void on (float velocity = 1.0f) { st = ATT; vel = velocity; }
    void off() { if (st != IDLE) st = REL; }
    void kill() { st = IDLE; v = 0.0f; }
    bool active() const { return st != IDLE; }
    inline float tick()
    {
        switch (st)
        {
            case ATT: v += (1.2f - v) * aC; if (v >= 1.0f) { v = 1.0f; st = DEC; } break;
            case DEC: v += (sus - v) * dC; if (v <= sus + 1.0e-4f) { v = sus; st = SUS; } break;
            case SUS: v = sus; break;
            case REL: v += (0.0f - v) * rC; if (v <= 1.0e-4f) { v = 0.0f; st = IDLE; } break;
            default: break;
        }
        return v;
    }
};

//==============================================================================
/*  Ornstein–Uhlenbeck walk in time-constant form: the same wander at any
    tick rate (the BWFX 1.3 rule). amp is the standard deviation. */
struct OuWalk
{
    float v = 0.0f;
    inline float tick (Rng& r, float tau, float amp, float dt)
    {
        const float a = std::exp (-dt / std::max (1.0e-3f, tau));
        v = v * a + r.gauss() * amp * std::sqrt (std::max (0.0f, 1.0f - a * a));
        return v;
    }
    void reset() { v = 0.0f; }
};

//==============================================================================
/*  Lookahead ceiling limiter: the required gain is taken over a window of L
    samples with a box filter, so it has arrived at the peak by the time the
    delayed peak arrives. Release exponential. */
struct Limiter
{
    Delay dl, dr;
    std::vector<float> gbox; int gw = 0; float gsum = 0.0f;
    int L = 96; float genv = 1.0f, rel = 0.999f, ceiling = 0.98f, reduction = 0.0f;
    void prepare (double sr, float lookMs = 2.0f)
    {
        L = std::max (8, (int) (sr * lookMs * 0.001));
        dl.prepare (L + 8); dr.prepare (L + 8);
        gbox.assign ((size_t) L, 1.0f); gsum = (float) L; gw = 0; genv = 1.0f;
        rel = std::exp (-1.0f / (0.12f * (float) sr));
    }
    int latency() const { return L; }
    inline void tick (float& l, float& r)
    {
        const float pk = std::max (std::abs (l), std::abs (r));
        const float want = pk > ceiling ? ceiling / pk : 1.0f;
        genv = std::min (want, genv);                  // attack: instantaneous
        genv = std::min (1.0f, genv / rel);            // release toward 1 (rel < 1)
        if (genv > 1.0f) genv = 1.0f;
        gsum += genv - gbox[(size_t) gw]; gbox[(size_t) gw] = genv; gw = (gw + 1) % L;
        const float g = gsum / (float) L;
        dl.write (l); dr.write (r);
        l = dl.readInt (L - 1) * g; r = dr.readInt (L - 1) * g;
        reduction = 1.0f - g;
    }
    void reset() { dl.clear(); dr.clear(); std::fill (gbox.begin(), gbox.end(), 1.0f); gsum = (float) L; genv = 1.0f; }
};

//==============================================================================
/*  A measured RMS tracker pair for level-matching a nonlinear stage (the
    Martian Gain / BWFX TUBE rule: measure, do not model). Frozen in silence,
    bounded 0.35..2.8x. */
struct LevelMatch
{
    float ein = 1.0e-4f, eout = 1.0e-4f, a = 0.001f;
    void prepare (double sr, float tauS = 0.3f) { a = 1.0f - std::exp (-1.0f / (tauS * (float) sr)); ein = eout = 1.0e-4f; }
    inline void feed (float in, float out)
    {
        if (in * in > 1.0e-8f) { ein += (in * in - ein) * a; eout += (out * out - eout) * a; }
    }
    inline float gain() const { return clampf (std::sqrt (ein / std::max (1.0e-9f, eout)), 0.12f, 2.8f); }
};

} // namespace tty
