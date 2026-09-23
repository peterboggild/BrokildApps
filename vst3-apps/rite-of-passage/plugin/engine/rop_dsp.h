#pragma once

// RITE OF PASSAGE — shared DSP bricks.
//
// Plain C++17, JUCE-free. Design authority: RITE-OF-PASSAGE-DESIGN.md at the
// BrokildApps repo root. Nothing here allocates; everything is real-time safe
// once prepared.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "rop_loudness.h"

namespace rop
{

constexpr int   kSlots    = 6;
constexpr int   kSubBlock = 32;     // control rate, the BWFX granularity
/*  Per effect. Raised 10 -> 16 on 2026-09-22 to match bwfx::kMaxParams
    exactly, so a wrapped World module can never have more knobs than a slot
    can carry (KIERANATOR has 10, STRIP 9 — it fitted, with nothing to
    spare, and "it fitted" is not a contract). It costs six floats twice and
    six bytes per slot in SlotState, and nothing else: the state writes
    numParams entries, not kMaxParams. */
constexpr int   kMaxParams = 16;

inline float clampf (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf  (float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------------------
// The five curves of the score. x and the result are both 0..1.
enum Curve { CurveLinear = 0, CurveAccel, CurveDecel, CurveS, CurvePeak, kNumCurves };

inline float applyCurve (float x, int shape)
{
    x = clampf (x, 0.0f, 1.0f);
    switch (shape)
    {
        case CurveAccel: return x * x * x;                       // late, then a rush
        case CurveDecel: { const float i = 1.0f - x; return 1.0f - i * i * i; }
        case CurveS:     return x * x * (3.0f - 2.0f * x);
        case CurvePeak:  return 1.0f - std::abs (2.0f * x - 1.0f);  // out and back
        default:         return x;
    }
}

// ---------------------------------------------------------------------------
// One-pole smoother. tau in seconds; advance by n samples toward the target.
struct Smooth
{
    float v = 0.0f, target = 0.0f, a = 0.0f;
    void setTau (double fs, double tau) { a = (float) (1.0 - std::exp (-1.0 / std::max (1.0, tau * fs))); }
    void snap (float x) { v = target = x; }
    float step()          { v += a * (target - v); return v; }
    float step (int n)    { for (int i = 0; i < n; ++i) v += a * (target - v); return v; }
};

// ---------------------------------------------------------------------------
// Running mean-square with a one-pole, for the measured compensations of §8.
struct PowerFollower
{
    double v = 0.0, a = 0.0;
    void setTau (double fs, double tau) { a = 1.0 - std::exp (-1.0 / std::max (1.0, tau * fs)); }
    void reset() { v = 0.0; }
    void push (float x)      { v += a * ((double) x * x - v); }
    void push (float l, float r) { v += a * (0.5 * ((double) l * l + (double) r * r) - v); }
    double rms() const { return std::sqrt (std::max (0.0, v)); }
};

// ---------------------------------------------------------------------------
/*  TPT / zero-delay-feedback state-variable filter (Zavalishin).

    The determining choice of the whole plugin (§7). A direct-form biquad swept
    fast at high resonance mistunes, zippers and can go unstable; this takes new
    coefficients every sample and stays well behaved all the way into
    self-oscillation, which is where a build wants to end up.

    RESONANCE MAKEUP (§8.1) is computed, not guessed: at every coefficient
    update the filter integrates its own |H|^2 over a pink-weighted log-
    frequency grid twice — once at the current Q and once at Q = 1/sqrt(2) —
    and takes the ratio. That removes exactly the loudness the resonance added
    and leaves the passband loss alone, which is what makes CLIMB SPECTRAL
    rather than NEUTRAL: a lowpass sweeping down is supposed to thin out. */
struct SvfTPT
{
    enum Mode { LP = 0, BP, HP };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
        buildGrid();
        set (1000.0f, 0.707f, LP);
    }

    void reset() { ic1 = ic2 = 0.0f; }

    void set (float cutoffHz, float q, int m)
    {
        mode = m;
        const double fc = std::min (std::max (10.0, (double) cutoffHz), fs * 0.45);
        q = clampf (q, 0.35f, 40.0f);
        g = (float) std::tan (M_PI * fc / fs);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        makeup = computeMakeup (fc, q);
    }

    inline float process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float y = (mode == LP) ? v2 : (mode == BP ? v1 : (x - k * v1 - v2));
        return y * makeup;
    }

    float gain() const { return makeup; }

private:
    //  |H(f)|^2 of the analogue prototype the TPT structure realises, which is
    //  what the makeup integral needs. w = f/fc.
    static double magSq (double w, double q, int mode)
    {
        const double w2 = w * w;
        const double den = (1.0 - w2) * (1.0 - w2) + (w / q) * (w / q);
        switch (mode)
        {
            case LP: return 1.0 / den;
            case BP: return (w / q) * (w / q) / den;
            default: return w2 * w2 / den;
        }
    }

    void buildGrid()
    {
        /*  A log grid (equal weight per octave) K-WEIGHTED, so the integral
            below is in the same domain the loudness contract is stated in.
            A flat log grid left 2.4 dB of resonance gain on the table because
            it counted 30 Hz as heavily as 3 kHz and the meter does not. */
        for (int i = 0; i < kGrid; ++i)
        {
            gridHz[i] = 20.0 * std::pow (1000.0, (double) i / (kGrid - 1));   // 20 Hz .. 20 kHz
            gridW[i] = kWeightMagSq (gridHz[i], fs);
        }
    }

    float computeMakeup (double fc, double q) const
    {
        double pRes = 0.0, pRef = 0.0;
        for (int i = 0; i < kGrid; ++i)
        {
            if (gridHz[i] > fs * 0.5) break;
            const double w = gridHz[i] / fc;
            pRes += gridW[i] * magSq (w, q, mode);
            pRef += gridW[i] * magSq (w, M_SQRT1_2, mode);
        }
        if (pRes <= 1e-12 || pRef <= 1e-12) return 1.0f;
        const double m = std::sqrt (pRef / pRes);
        //  never let the makeup itself become a gain stage
        return (float) std::min (1.0, std::max (0.02, m));
    }

    static constexpr int kGrid = 96;
    double fs = 48000.0;
    double gridHz[kGrid] {}, gridW[kGrid] {};
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, makeup = 1.0f;
    float ic1 = 0, ic2 = 0;
    int mode = LP;
};

// ---------------------------------------------------------------------------
/*  MEASURED makeup, §8.1. Follows the power going into a stage and the power
    coming out of it and applies the ratio, slowly and with a ceiling.

    This is the one place a gain is allowed to move on its own, and it exists
    because saturation's loudness gain depends on the material and cannot be
    computed from the knob. It is NOT a leveller: the time constant is long,
    it is wrapped around one stage rather than the signal path, and it never
    sees a gesture — a lowpass sweeping down is compensated by the SVF's own
    computed makeup for the resonance only, never by this. */
struct MeasuredMakeup
{
    void prepare (double fs, double tau = 0.120, float maxDb = 12.0f)
    {
        pin.setTau (fs, tau); pout.setTau (fs, tau);
        hi = std::pow (10.0f, maxDb / 20.0f);
        lo = 1.0f / hi;
        g = 1.0f;
        gs.setTau (fs, 0.030);
        gs.snap (1.0f);
    }
    void reset() { pin.reset(); pout.reset(); g = 1.0f; gs.snap (1.0f); }

    //  call once per sample with the stage's input and output
    inline float update (float inL, float inR, float outL, float outR)
    {
        pin.push (inL, inR);
        pout.push (outL, outR);
        if (pout.v > 1e-12 && pin.v > 1e-12)
            g = clampf ((float) std::sqrt (pin.v / pout.v), lo, hi);
        gs.target = g;
        return gs.step();
    }

private:
    PowerFollower pin, pout;
    Smooth gs;
    float g = 1.0f, hi = 4.0f, lo = 0.25f;
};

// ---------------------------------------------------------------------------
// Tempo. Divisions ordered by SPEED so that interpolating the index is an
// accelerando — a STUTTER travelling 1/4 -> 1/32 passes through everything
// between, which is the gesture.
enum Div { Div4 = 0, Div8, Div8T, Div16, Div16T, Div32, kNumDivs };
inline double beatsForDiv (int d)
{
    switch (d)
    {
        case Div4:   return 1.0;
        case Div8:   return 0.5;
        case Div8T:  return 1.0 / 3.0;
        case Div16:  return 0.25;
        case Div16T: return 1.0 / 6.0;
        default:     return 0.125;
    }
}
inline const char* kDivChoices = "1/4|1/8|1/8T|1/16|1/16T|1/32";

// ---------------------------------------------------------------------------
/*  Equal-power dry/wet, §8.1. A linear crossfade between two DECORRELATED
    signals dips 3 dB in the middle — measured on STUTTER at DEPTH 50 as a
    3.0 dB hole. Most wet paths in this plugin are decorrelated from the dry
    by construction (a repeated slice, a reversed fragment, a cloud), so
    equal power is the default and linear is the exception.  */
inline void equalPowerMix (float& dry, float& wet, float t)
{
    const float a = 0.5f * (float) M_PI * clampf (t, 0.0f, 1.0f);
    dry = std::cos (a);
    wet = std::sin (a);
}

// ---------------------------------------------------------------------------
// Orthonormal mid/side. Energy preserving and exactly invertible, which the
// /2 form is not — and §8 requires every stereo operation to preserve energy.
inline void toMS (float l, float r, float& m, float& s)
{
    m = (float) (M_SQRT1_2) * (l + r);
    s = (float) (M_SQRT1_2) * (l - r);
}
inline void fromMS (float m, float s, float& l, float& r)
{
    l = (float) (M_SQRT1_2) * (m + s);
    r = (float) (M_SQRT1_2) * (m - s);
}

// ---------------------------------------------------------------------------
/*  Energy-preserving width. w = 1 leaves the signal alone, w = 0 is mono.

    The static compensation sqrt(2/(1+w^2)) is only right for uncorrelated
    material; on a correlated source it becomes a boost. So the gain is
    MEASURED from the running mid and side powers — a slow follower, not a
    compressor — which is the §8 rule: computed compensation, never a leveller. */
struct WidthStage
{
    void prepare (double fs) { pm.setTau (fs, 0.050); ps.setTau (fs, 0.050); }
    void reset() { pm.reset(); ps.reset(); }

    void process (float& l, float& r, float w)
    {
        float m, s;
        toMS (l, r, m, s);
        pm.push (m); ps.push (s);
        const double before = pm.v + ps.v;
        const double after  = pm.v + (double) w * w * ps.v;
        const float g = (float) std::sqrt ((before + 1e-12) / (after + 1e-12));
        fromMS (m * g, s * w * g, l, r);
    }

private:
    PowerFollower pm, ps;
};

// ---------------------------------------------------------------------------
// Field rotation. A rotation matrix is energy preserving by construction —
// this moves the room, where a pan moves a fader (§6).
inline void rotate (float& l, float& r, float radians)
{
    const float c = std::cos (radians), s = std::sin (radians);
    const float nl = l * c - r * s;
    const float nr = l * s + r * c;
    l = nl; r = nr;
}

// ---------------------------------------------------------------------------
// Linkwitz-Riley-ish mono-maker for the bass, §6: below the corner the two
// channels are summed, above it they are left alone.
struct BassMono
{
    void prepare (double fs) { sr = fs; reset(); setCorner (120.0f); }
    void reset() { for (auto& z : zl) z = 0; for (auto& z : zr) z = 0; }
    void setCorner (float hz)
    {
        const double x = std::exp (-2.0 * M_PI * std::max (20.0f, hz) / sr);
        a = (float) x;
    }
    void process (float& l, float& r)
    {
        //  two cascaded one-poles per side: gentle, phase-benign, enough
        zl[0] += (1.0f - a) * (l - zl[0]); zl[1] += (1.0f - a) * (zl[0] - zl[1]);
        zr[0] += (1.0f - a) * (r - zr[0]); zr[1] += (1.0f - a) * (zr[0] - zr[1]);
        const float lowL = zl[1], lowR = zr[1];
        const float mono = 0.5f * (lowL + lowR);
        l = (l - lowL) + mono;
        r = (r - lowR) + mono;
    }
private:
    double sr = 48000.0;
    float a = 0.99f, zl[2] {}, zr[2] {};
};

// ---------------------------------------------------------------------------
// Deterministic per-instance noise. Never seeded from the clock: the bench
// requires byte-identical output across runs and block sizes.
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t x) { s = x ? x : 1u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni()  { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bip()  { return uni() * 2.0f - 1.0f; }
};

// ---------------------------------------------------------------------------
// Four-point Catmull-Rom from a power-of-two ring — the house read
// (bwfx_dsp.h): linear smears the top octave on a moving tap, and every
// moving tap in this plugin is a gesture somebody will hear.
inline float catmullRead (const float* buf, int mask, int writePos, float delaySamples)
{
    const float ri = (float) writePos - delaySamples;
    const int   i0 = (int) std::floor (ri);
    const float t  = ri - (float) i0;
    const float ym1 = buf[(i0 - 1) & mask], y0 = buf[i0 & mask];
    const float y1  = buf[(i0 + 1) & mask], y2 = buf[(i0 + 2) & mask];
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

} // namespace rop
