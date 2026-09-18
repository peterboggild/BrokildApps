#pragma once

// RITE OF PASSAGE — the DSP blocks more than one effect file needs.
//
// These three were written for the second six (rop_effects_more.cpp) and are
// MOVED here verbatim now that the third six want them too. They are not in
// rop_dsp.h because rop_dsp.h is the house vocabulary every effect speaks;
// these are heavier machinery that only some of them reach for.
//
//   HalfBand   2x oversampling, so a tap read faster than it was written
//              cannot fold. The reason the whole thing exists is in its own
//              comment — read it before removing an oversampler anywhere.
//   KMakeup    a measured gain match in the K-weighted domain the loudness
//              contract is stated in, with a HOLD for when there is nothing
//              to match (a frozen wash, a shift that left the band).
//   GrainBank  equal-power or equal-amplitude windowed grains over a
//              power-of-two ring; the window choice is a property of whether
//              the grains are correlated, not a preference.

#include "rop_effect.h"

#include <cmath>
#include <vector>

namespace rop
{

// ---------------------------------------------------------------------------
/*  2x polyphase half-band resampler, 63-tap windowed sinc. Ported from
    BrokildWorldFX's bwfx_dsp.h (itself Martian Gain's), where it has been
    measured; COPIED rather than included, because the Rite engine and its
    bench are deliberately free of every dependency.

    THIS IS WHAT KEEPS THE FAST READS HI-FI. Reading a delay line faster than
    it was written DECIMATES it, and everything above the new Nyquist folds
    back into the band as a spurious tone — the sound of a cheap sampler. No
    filter cheap enough exists to hold a 12 kHz corner 60 dB down by 14 kHz;
    the transition is far too narrow. Oversampling turns that impossible
    filter into an easy one: at 96 kHz a 14 kHz tone shifted an octave up
    lands at 28 kHz, which is below the new Nyquist and CANNOT fold, and the
    decimator's own stopband then removes it. The bench's ALIASING section
    measured +0.0 dB of fold before this and the noise floor after.

    Group delay is 31 samples at the base rate. Both users are delay lines
    already, so there is nothing to compensate — it is simply 0.65 ms more
    delay on an effect whose whole job is delay. */
struct HalfBand
{
    static constexpr int TAPS = 63;
    static constexpr int PH   = 32;
    static constexpr int MASK = PH - 1;

    float ge[PH] {}, go[PH] {};
    float upLine[PH] {}, dnE[PH] {}, dnO[PH] {};
    int upW = 0, dnW = 0;

    void design()
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
        for (auto& v : g) v /= sum;          //  unity at DC, whatever the window
        for (int k = 0; k < PH; ++k)
        {
            ge[k] = (2 * k     < TAPS) ? (float) g[2 * k]     : 0.0f;
            go[k] = (2 * k + 1 < TAPS) ? (float) g[2 * k + 1] : 0.0f;
        }
    }

    void clear()
    {
        for (auto& v : upLine) v = 0.0f;
        for (auto& v : dnE) v = 0.0f;
        for (auto& v : dnO) v = 0.0f;
        upW = dnW = 0;
    }

    inline void up (float x, float& o0, float& o1)
    {
        upLine[upW] = x;
        float s0 = 0.0f, s1 = 0.0f;
        for (int k = 0; k < PH; ++k)
        {
            const float v = upLine[(upW - k) & MASK];
            s0 += ge[k] * v;
            s1 += go[k] * v;
        }
        o0 = 2.0f * s0; o1 = 2.0f * s1;
        upW = (upW + 1) & MASK;
    }

    inline float down (float a, float b)
    {
        dnE[dnW] = a; dnO[dnW] = b;
        float s = 0.0f;
        for (int k = 0; k < PH; ++k)
            s += ge[k] * dnE[(dnW - k) & MASK] + go[k] * dnO[(dnW - k - 1) & MASK];
        dnW = (dnW + 1) & MASK;
        return s;
    }
};

// ---------------------------------------------------------------------------
/*  K-weighted measured makeup: MeasuredMakeup with BS.1770 pre-filters on
    both taps, so a stage that darkens or brightens its wet path is matched
    in loudness rather than in raw energy. Long window, clamped, and it can
    be HELD (a frozen reverb must not have its level chased). */
struct KMakeup
{
    void prepare (double fs, double tau, float maxDb)
    {
        kin.prepare (fs); kout.prepare (fs);
        pin.setTau (fs, tau); pout.setTau (fs, tau);
        hi = std::pow (10.0f, maxDb / 20.0f); lo = 1.0f / hi;
        gs.setTau (fs, 0.050);
        reset();
    }
    void reset() { pin.reset(); pout.reset(); g = 1.0f; gs.snap (1.0f); }

    inline float update (float inL, float inR, float outL, float outR, bool hold)
    {
        float a, b, c, d;
        kin.process (inL, inR, a, b);
        kout.process (outL, outR, c, d);
        if (! hold)
        {
            pin.push (a, b); pout.push (c, d);
            if (pin.v > 1.0e-9 && pout.v > 1.0e-9)
                g = clampf ((float) std::sqrt (pin.v / pout.v), lo, hi);
        }
        gs.target = g;
        return gs.step();
    }

private:
    KWeight kin, kout;
    PowerFollower pin, pout;
    Smooth gs;
    float g = 1.0f, hi = 4.0f, lo = 0.25f;
};

// ---------------------------------------------------------------------------
/*  A bank of equal-power grains reading a power-of-two ring. The window is
    sin(pi*phase): with k grains overlapping at equal spacing the windows'
    powers sum to k/2, so scaling the sum by sqrt(2/k) preserves the power of
    what was read — a computed makeup, not a measured one (§8.1).

    A grain at rate exactly 1 whose start is an integer reads with t = 0 in
    the Catmull-Rom, which returns y0 exactly: the exact path of §8.2 costs
    nothing and needs no special case. */
struct GrainBank
{
    static constexpr int kMax = 24;
    struct Grain { bool on = false; double pos = 0.0, rate = 1.0; int len = 0, phase = 0; };

    void prepare (double fs, double seconds)
    {
        sr = fs;
        size = nextPow2 ((int) (fs * seconds) + 16);
        mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        w = 0; nextLaunch = 0;
        for (auto& g : grains) g.on = false;
    }

    inline void write (float l, float r)
    {
        buf[0][(size_t) (w & mask)] = l;
        buf[1][(size_t) (w & mask)] = r;
    }

    //  start a grain whose read head sits `back` samples behind the write
    //  head and moves at `rate`; a rate above 1 needs extra room ahead
    void launch (double back, double rate, int len)
    {
        for (auto& g : grains)
            if (! g.on)
            {
                const double room = (double) len * std::max (0.0, rate - 1.0);
                g.on = true; g.rate = rate; g.len = std::max (8, len); g.phase = 0;
                g.pos = (double) w - 2.0 - back - room;
                return;
            }
    }

    //  sum every live grain at the current write position (call BEFORE ++w)
    inline void read (float& l, float& r, double rateNow, bool followRate)
    {
        l = 0.0f; r = 0.0f;
        for (auto& g : grains)
        {
            if (! g.on) continue;
            const double rate = followRate ? rateNow : g.rate;
            const double delay = (double) w - g.pos;
            if (delay < 1.0 || delay > (double) (size - 8)) { g.on = false; continue; }
            float win = std::sin ((float) M_PI * (float) g.phase / (float) g.len);
            if (raisedCosine) win *= win;
            l += win * catmullRead (buf[0].data(), mask, (int) (w & mask) + size, (float) delay);
            r += win * catmullRead (buf[1].data(), mask, (int) (w & mask) + size, (float) delay);
            g.pos += rate;
            if (++g.phase >= g.len) g.on = false;
        }
    }

    inline void advance() { ++w; }

    /*  the window. A cloud of grains at RANDOM positions is decorrelated,
        so its windows must be power-complementary (sin, normalised by
        sqrt(2/overlap)); two grains reading the SAME material a fraction
        apart are correlated, so theirs must be amplitude-complementary
        (sin^2 at 50 % overlap sums to exactly 1). Getting this wrong costs
        3 dB of ripple at the splice rate and a splice that radiates,
        measured as 10 dB of extra rubbish in the aliasing check. */
    bool raisedCosine = false;

    double sr = 48000.0;
    int size = 0, mask = 0, nextLaunch = 0;
    long long w = 0;
    std::vector<float> buf[2];
    Grain grains[kMax];
};

inline double semisToRate (float st) { return std::pow (2.0, (double) st / 12.0); }

} // namespace rop
