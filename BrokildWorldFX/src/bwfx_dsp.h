#pragma once

// Shared DSP bricks for BWFX modules. All plain C++17, ported from proven
// Brokild engines (Photo-Synth 2, Mars Wars) — keep behaviour, drop JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bwfx
{

inline float  clampf (float x, float a, float b)  { return x < a ? a : (x > b ? b : x); }
inline double clampd (double x, double a, double b) { return x < a ? a : (x > b ? b : x); }

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// ---------------------------------------------------------------------------
// One-pole parameter smoother, WebAudio setTargetAtTime-style: advance by n
// samples toward the target with time constant tau. Kills zipper without a
// second code path for automation.
struct Smooth
{
    float current = 0, target = 0;
    void  init (float v)      { current = target = v; }
    void  set (float v)       { target = v; }
    float tick (double fsr, int n, float tau)
    {
        if (tau <= 0.0f) { current = target; return current; }
        const float a = 1.0f - std::exp ((float) (-(double) n / ((double) tau * fsr)));
        current += (target - current) * a;
        return current;
    }
};

// ---------------------------------------------------------------------------
// Biquad — WebAudio spec (port of Photo-Synth 2's): Q is in dB for
// low/highpass, linear otherwise; peaking gain in dB.
struct Biquad
{
    enum Type { lowpass = 0, highpass, bandpass, notch, allpass, peaking };
    Type type = lowpass;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1L = 0, z2L = 0, z1R = 0, z2R = 0;

    void reset() { z1L = z2L = z1R = z2R = 0; }

    void set (Type t, double freq, double Q, double gainDb, double fsr)
    {
        type = t;
        const double f = clampd (freq, 0.0, fsr * 0.5);
        const double w = kTwoPi * f / fsr;
        const double cw = std::cos (w), sw = std::sin (w);
        double b0n = 1, b1n = 0, b2n = 0, a0 = 1, a1n = 0, a2n = 0;
        const double eps = 1e-9;
        switch (t)
        {
            case lowpass:
            {
                const double q = std::pow (10.0, Q / 20.0);
                const double alpha = sw / (2.0 * std::max (eps, q));
                b0n = (1 - cw) / 2; b1n = 1 - cw; b2n = b0n;
                a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
                break;
            }
            case highpass:
            {
                const double q = std::pow (10.0, Q / 20.0);
                const double alpha = sw / (2.0 * std::max (eps, q));
                b0n = (1 + cw) / 2; b1n = -(1 + cw); b2n = b0n;
                a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
                break;
            }
            case bandpass:
            {
                const double alpha = sw / (2.0 * std::max (eps, Q));
                b0n = alpha; b1n = 0; b2n = -alpha;
                a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
                break;
            }
            case notch:
            {
                const double alpha = sw / (2.0 * std::max (eps, Q));
                b0n = 1; b1n = -2 * cw; b2n = 1;
                a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
                break;
            }
            case allpass:
            {
                const double alpha = sw / (2.0 * std::max (eps, Q));
                b0n = 1 - alpha; b1n = -2 * cw; b2n = 1 + alpha;
                a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
                break;
            }
            case peaking:
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                const double alpha = sw / (2.0 * std::max (eps, Q));
                b0n = 1 + alpha * A; b1n = -2 * cw; b2n = 1 - alpha * A;
                a0 = 1 + alpha / A; a1n = -2 * cw; a2n = 1 - alpha / A;
                break;
            }
        }
        b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
        a1 = a1n / a0; a2 = a2n / a0;
    }

    inline float processL (float x) { const double y = b0 * x + z1L; z1L = b1 * x - a1 * y + z2L; z2L = b2 * x - a2 * y; return (float) y; }
    inline float processR (float x) { const double y = b0 * x + z1R; z1R = b1 * x - a1 * y + z2R; z2R = b2 * x - a2 * y; return (float) y; }
};

// ---------------------------------------------------------------------------
// 4-point Catmull-Rom read from a power-of-two ring buffer. A moving tap read
// with linear interpolation smears the top octave and zippers; cubic is
// ~8 dB cleaner on bright material (measured on Photo-Synth 2).
inline float catmullRead (const std::vector<float>& buf, int mask, int writePos, double delaySamples)
{
    const double ri = (double) writePos - delaySamples;
    const int i0 = (int) std::floor (ri);
    const double t = ri - i0;
    const double ym1 = buf[(size_t) ((i0 - 1) & mask)], y0 = buf[(size_t) (i0 & mask)];
    const double y1  = buf[(size_t) ((i0 + 1) & mask)], y2 = buf[(size_t) ((i0 + 2) & mask)];
    const double c1 = 0.5 * (y1 - ym1);
    const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return (float) (((c3 * t + c2) * t + c1) * t + y0);
}

// ---------------------------------------------------------------------------
// Polyphase half-band resampler, 63-tap windowed sinc (port of Mars Wars').
// Shapers alias badly; this is what keeps the mild ones sounding mild.
// Two in cascade give 4x.
struct HalfBand
{
    static constexpr int TAPS = 63;
    static constexpr int PH   = 32;
    static constexpr int MASK = PH - 1;

    std::array<float, PH> ge {}, go {};
    std::array<float, PH> upLine {}, dnE {}, dnO {};
    int upW = 0, dnW = 0;

    void design()
    {
        constexpr int C = TAPS / 2;
        std::array<double, TAPS> g {};
        double sum = 0.0;
        for (int n = 0; n < TAPS; ++n)
        {
            const double t = n - C;
            const double s = (t == 0.0) ? 0.5 : std::sin (kPi * 0.5 * t) / (kPi * t);
            const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * n / (TAPS - 1))
                                  + 0.08 * std::cos (4.0 * kPi * n / (TAPS - 1));
            g[(size_t) n] = s * w;
            sum += g[(size_t) n];
        }
        for (auto& v : g) v /= sum;
        for (int k = 0; k < PH; ++k)
        {
            ge[(size_t) k] = (2 * k     < TAPS) ? (float) g[(size_t) (2 * k)]     : 0.0f;
            go[(size_t) k] = (2 * k + 1 < TAPS) ? (float) g[(size_t) (2 * k + 1)] : 0.0f;
        }
    }
    void clear() { upLine.fill (0.0f); dnE.fill (0.0f); dnO.fill (0.0f); upW = dnW = 0; }

    inline void up (float x, float& o0, float& o1)
    {
        upLine[(size_t) upW] = x;
        float s0 = 0.0f, s1 = 0.0f;
        for (int k = 0; k < PH; ++k)
        {
            const float v = upLine[(size_t) ((upW - k) & MASK)];
            s0 += ge[(size_t) k] * v;
            s1 += go[(size_t) k] * v;
        }
        o0 = 2.0f * s0;
        o1 = 2.0f * s1;
        upW = (upW + 1) & MASK;
    }

    inline float down (float a, float b)
    {
        dnE[(size_t) dnW] = a;
        dnO[(size_t) dnW] = b;
        float s = 0.0f;
        for (int k = 0; k < PH; ++k)
        {
            s += ge[(size_t) k] * dnE[(size_t) ((dnW - k) & MASK)]
               + go[(size_t) k] * dnO[(size_t) ((dnW - k - 1) & MASK)];
        }
        dnW = (dnW + 1) & MASK;
        return s;
    }
};

} // namespace bwfx
