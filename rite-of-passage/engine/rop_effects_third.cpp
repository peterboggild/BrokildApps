// RITE OF PASSAGE — the third six: SWIRL, MANGLE, SWARM, DUST, ORBIT, CHANT.
//
// Peter's list, 2026-09-18: "a heavily modulated verb / a distortion with 4
// different distortion engines from Martian Gain / heavy chorus + detune /
// lofi / super stereo panner, making sounds swirl in 3D, near - right - back
// (weaker, filtered, small delay) - left / vocoder". Eighteen effects in all;
// APPENDED to the registry, so every existing type index and every saved rite
// keeps naming exactly what it named before.
//
// FOUR DECISIONS, stated here because each had a worse alternative:
//
//  1. CHANT MAKES ITS OWN CARRIER. A vocoder wants two signals and this is an
//     insert with one, so the audio arriving is the MODULATOR and the carrier
//     is generated inside, with its own PITCH so the score can sweep it. The
//     alternative — a side-chain input — would change the plugin's bus layout
//     and every host's idea of what it is, to serve one effect.
//
//  2. SWIRL IS NOT BLOOM WITH A BIGGER KNOB. BLOOM is a room that GROWS and
//     can be frozen. SWIRL is a fixed room whose delay lines are chorused
//     twenty times as deep and whose wet field rotates — seasick rather than
//     large. Two reverbs earn their place by being different animals.
//
//  3. MANGLE TAKES FOUR OF MARTIAN GAIN'S SIXTEEN, chosen to be a journey and
//     not a menu: VALVE (warm, asymmetric), FUZZ (ugly, lopsided clip), SINE
//     FOLD (the wave turned inside out), ANNIHILATE (fold, quantise, ring).
//     Warm → ugly → folded → destroyed is a build in one knob. The shapers are
//     ported from b/MarsWars/Source/Engine.cpp, where they were measured.
//
//  4. ORBIT'S ANGLE IS A LEVEL KNOB, and deliberately. The brief asks for the
//     back of the circle to be WEAKER, and a source behind you being quieter
//     is the cue that tells you it is behind you. Compensating that would
//     delete the effect, so it is declared instead (§8.1's per-knob rule).
//
// Level and pitch contracts, as declared below and as the bench holds them:
//   SWIRL   NEUTRAL except DECAY and MIX. A longer tail carries more energy.
//   MANGLE  NEUTRAL except MIX — and DRIVE is NOT excused. A saturator's
//           loudness depends on the material, so it is MEASURED (Martian
//           Gain's own rule: measured, never modelled) in the K-weighted
//           domain the contract is stated in.
//   SWARM   NEUTRAL except MIX; computed, because N decorrelated voices sum
//           in power and 1/sqrt(N) is exact.
//   DUST    NEUTRAL except NOISE and MIX. NOISE defaults to 0 or silence in
//           stops being silence out (the BWFX GRIT lesson).
//   ORBIT   NEUTRAL except ANGLE, see 4 above.
//   CHANT   NEUTRAL except MIX; measured, because the output level is the sum
//           of whatever the band envelopes happened to find.
//
// Real-time rules as everywhere: process() allocates nothing, locks nothing.

#include "rop_effects_third.h"
#include "rop_blocks.h"

#include <cstring>
#include <vector>

namespace rop { namespace third {

namespace
{

// ---------------------------------------------------------------------------
/*  A plain TPT state-variable bandpass. NOT SvfTPT: that one integrates its
    own K-weighted response over a 96-point grid every time it is re-tuned, to
    make CLIMB's resonance loudness-neutral — which is right there and far too
    expensive here, where CHANT re-tunes up to 72 filters. A vocoder band does
    not want a resonance makeup anyway; it wants to be a window. */
struct Bp2
{
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { ic1 = ic2 = 0.0f; }

    void set (float hz, float q)
    {
        const double fc = std::min (std::max (20.0, (double) hz), fs * 0.45);
        const float g = (float) std::tan (M_PI * fc / fs);
        k = 1.0f / std::max (0.30f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v1;                       // bandpass, unity at centre
    }

private:
    double fs = 48000.0;
    float k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

//  a one-pole, for tone tilts and envelope followers
struct OnePole
{
    void setHz (double fs, double hz) { a = (float) (1.0 - std::exp (-2.0 * M_PI * std::max (0.1, hz) / fs)); }
    void setTau (double fs, double tau) { a = (float) (1.0 - std::exp (-1.0 / std::max (1.0, tau * fs))); }
    void reset() { z = 0.0f; }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { return x - lp (x); }
    float z = 0.0f, a = 0.1f;
};

/*  The triangle fold Martian Gain's FOLD and ANNIHILATE are built on: identity
    on [-1,1], reflecting beyond it, period 4. Written arithmetically rather
    than as asin(sin(x)) because it runs per sample at 2x. */
inline float wavefold (float x)
{
    const float u = x * 0.25f + 0.25f;
    return 1.0f - 4.0f * std::abs (u - std::floor (u) - 0.5f);
}

inline double semitones (float st) { return std::pow (2.0, (double) st / 12.0); }

// ===========================================================================
// 13 · SWIRL — the room itself will not hold still.
//
// A four-line FDN whose delays are modulated some TWENTY TIMES as deep as
// BLOOM's (BLOOM wobbles 0.3 ms to stop the tail ringing metallic; SWIRL
// reaches 8 ms, which is a chorus, not a de-metalliser) and whose wet field
// is ROTATED by the same motion — so the wash does not merely shimmer, it
// moves around the head. Fixed size, so it is never BLOOM: this one is
// seasick where BLOOM is large.
const ParamDesc SWIRL_P[] = {
    { "size",  "SIZE",   45.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    //  a longer tail carries more energy; that is the knob, not a fault
    { "decay", "DECAY",  55.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "warp",  "WARP",   60.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "rate",  "RATE",    0.6f, 0.02f, 8.0f,  0.0f, "Hz", Warp::Log },
    //  a tone control is allowed to change the loudness of the tone —
    //  the same exemption CLIMB's CUTOFF has
    { "tone",  "TONE",   50.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "mix",   "MIX",    40.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
//  movesPitch: a delay line modulated 8 ms deep is a vibrato, and the bench
//  measured 55 cents of it. That is the brief, not a fault.
const EffectDesc SWIRL_D_ { "swirl", "SWIRL", "the room will not hold still",
                            SWIRL_P, 6, Level::Neutral, true, false, false };

class Swirl : public Effect
{
public:
    static constexpr int kLines = 4;
    const EffectDesc& desc() const override { return SWIRL_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 0.30) + 16);
        mask = size - 1;
        for (auto& b : line) b.assign ((size_t) size, 0.0f);
        for (auto& f : damp) f.setHz (fs, 8000.0);
        //  §8.1 measured, K-weighted: a room that changes level when its
        //  SIZE changes is a gain fault, not a bigger room
        mk.prepare (fs, 0.400, 12.0f);
        reset();
    }
    void reset() override
    {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& f : damp) f.reset();
        for (int i = 0; i < kLines; ++i) ph[i] = (double) i * 0.5 * M_PI;
        rot = 0.0; w = 0; fed = true;
        mk.reset();
    }
    void arm() override { fed = true; }
    void release (Tail t) override
    {
        fed = (t == Tail::Spill);
        if (t == Tail::Clear) { for (auto& b : line) std::fill (b.begin(), b.end(), 0.0f); }
    }
    bool ringing() const override
    {
        float e = 0.0f;
        for (const auto& f : damp) e += std::abs (f.z);
        return e > 1.0e-5f;
    }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        //  prime numbers of milliseconds, so the lines do not agree
        static const double baseMs[kLines] = { 23.3, 31.7, 41.9, 53.5 };
        static const double lfoMul[kLines] = { 1.00, 0.73, 1.31, 0.57 };

        const float size01  = clampf (pL[0] * 0.01f, 0.0f, 1.0f);
        const float decay01 = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const float warp01  = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
        const double rateHz = std::max (0.01, (double) pL[3]);
        const float tone01  = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix     = clampf (pL[5] * 0.01f, 0.0f, 1.0f);

        const double scale = 0.35 + 1.65 * size01;
        const double rt60  = 0.25 * std::pow (60.0, (double) decay01);      // 0.25 .. 15 s
        //  THE WHOLE POINT: 8 ms of travel, where BLOOM uses 0.3
        const double depth = 0.0080 * warp01 * sr;
        const double fc    = 16000.0 * std::pow (0.05, (double) (1.0f - tone01));
        for (auto& f : damp) f.setHz (sr, fc);

        double lenS[kLines]; float gain[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            lenS[i] = baseMs[i] * 0.001 * scale * sr;
            gain[i] = (float) std::pow (10.0, -3.0 * (lenS[i] / sr) / rt60);
        }
        float gd, gw; equalPowerMix (gd, gw, mix);

        for (int s = 0; s < n; ++s)
        {
            const float inL = fed ? L[s] : 0.0f;
            const float inR = fed ? R[s] : 0.0f;

            float v[kLines]; float sum = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                ph[i] += 2.0 * M_PI * rateHz * lfoMul[i] / sr;
                if (ph[i] > 2.0 * M_PI) ph[i] -= 2.0 * M_PI;
                const double d = lenS[i] + depth * std::sin (ph[i]);
                const float y = catmullRead (line[i].data(), mask, (int) (w & mask) + size,
                                             clampf ((float) d, 4.0f, (float) (size - 8)));
                v[i] = damp[i].lp (y) * gain[i];
                sum += v[i];
            }
            //  Householder: orthogonal, so only gain[] takes anything out
            const float hs = sum * (2.0f / (float) kLines);
            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                const float in = (i & 1) ? inR : inL;
                line[i][(size_t) (w & mask)] = in + (v[i] - hs);
                if (i & 1) wetR += v[i]; else wetL += v[i];
            }
            wetL *= 0.5f; wetR *= 0.5f;
            ++w;

            //  and the field turns with the same motion — energy preserving,
            //  so the rotation is a move and never a gain
            rot += 2.0 * M_PI * rateHz * 0.37 / sr;
            if (rot > 2.0 * M_PI) rot -= 2.0 * M_PI;
            rotate (wetL, wetR, (float) (warp01 * 0.7 * std::sin (rot)));

            const float m = mk.update (L[s], R[s], wetL, wetR, ! fed);
            L[s] = L[s] * gd + wetL * m * gw;
            R[s] = R[s] * gd + wetR * m * gw;
        }
    }

private:
    double sr = 48000.0, ph[kLines] {}, rot = 0.0;
    int size = 0, mask = 0;
    long long w = 0;
    std::vector<float> line[kLines];
    OnePole damp[kLines];
    KMakeup mk;
    bool fed = true;
};

// ===========================================================================
// 14 · MANGLE — four ways to break the same signal.
//
// The shapers are Martian Gain's, ported from b/MarsWars/Source/Engine.cpp
// where they were measured, and chosen to be a JOURNEY rather than a menu:
// VALVE flatters, FUZZ ruins, SINE FOLD turns the wave inside out, ANNIHILATE
// leaves gravel. Travelling ENGINE from A to B across the build walks that
// road — and ENGINE is stepped, so it changes on a musical boundary rather
// than mid-phrase (the rack's stepped-parameter grid does that for free).
//
// 2x OVERSAMPLED. A shaper generates harmonics without limit; at 48 kHz they
// fold immediately, which is the difference between a saturator and a mess.
// Martian Gain oversamples for exactly this reason and so does this.
const char* kEngines = "VALVE|FUZZ|SINE FOLD|ANNIHILATE";
const ParamDesc MANGLE_P[] = {
    { "engine", "ENGINE", 0.0f, 0.0f,   3.0f, 1.0f, "",  Warp::Linear, kEngines },
    { "drive",  "DRIVE", 25.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "bias",   "BIAS",   0.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "char",   "CHAR",  50.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "tone",   "TONE",  50.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear },
    { "mix",    "MIX",  100.0f, 0.0f, 100.0f, 0.0f, "%", Warp::Linear, nullptr, true },
};
const EffectDesc MANGLE_D_ { "mangle", "MANGLE", "four ways to break it",
                             MANGLE_P, 6, Level::Neutral, false, false, false };

class Mangle : public Effect
{
public:
    const EffectDesc& desc() const override { return MANGLE_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        for (auto& h : hb) h.design();
        for (auto& f : tilt) f.setHz (fs, 1200.0);
        //  measured, K-weighted, and slow: this is a gain match, not a leveller
        mk.prepare (fs, 0.300, 12.0f);
        reset();
    }
    void reset() override
    {
        for (auto& h : hb) h.clear();
        for (auto& f : tilt) f.reset();
        for (auto& f : dcb) f.reset();
        mk.reset();
    }
    void release (Tail t) override { if (t != Tail::Spill) reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const int   eng   = (int) clampf (pL[0] + 0.5f, 0.0f, 3.0f);
        const float drive = clampf (pL[1] * 0.01f, 0.0f, 1.0f);
        const float bias  = clampf (pL[2] * 0.01f, 0.0f, 1.0f) * 0.6f;
        const float chr   = clampf (pL[3] * 0.01f, 0.0f, 1.0f);
        const float tone  = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix   = clampf (pL[5] * 0.01f, 0.0f, 1.0f);

        //  pre-gain per engine, scaled so the knob spans each one's useful road
        float k = 1.0f;
        switch (eng)
        {
            case 0: k = 1.0f + drive * 8.0f;  break;   // VALVE
            case 1: k = 1.0f + drive * 14.0f; break;   // FUZZ
            case 2: k = 1.0f + drive * 6.0f;  break;   // SINE FOLD
            default: k = 1.0f + drive * 14.0f; break;  // ANNIHILATE
        }
        for (auto& f : tilt) f.setHz (sr, 400.0 + 6000.0 * tone);
        //  a correlated dry/wet crossfades at equal AMPLITUDE, never equal
        //  power — the BWFX TUBE lesson, and a saturator's wet is correlated
        const float gd = 1.0f - mix, gw = mix;

        for (int i = 0; i < n; ++i)
        {
            float a0, a1, b0, b1;
            hb[0].up (L[i], a0, a1);
            hb[1].up (R[i], b0, b1);

            const float y0l = shape (eng, a0 * k + bias, chr, 0);
            const float y1l = shape (eng, a1 * k + bias, chr, 0);
            const float y0r = shape (eng, b0 * k + bias, chr, 1);
            const float y1r = shape (eng, b1 * k + bias, chr, 1);

            float wl = hb[0].down (y0l, y1l);
            float wr = hb[1].down (y0r, y1r);

            //  BIAS puts a DC step in by construction; take it back out
            wl = dcb[0].hp (wl);
            wr = dcb[1].hp (wr);
            //  TONE is a tilt on the wet only, so the dry stays the dry
            wl = lerpf (tilt[0].lp (wl), wl, tone);
            wr = lerpf (tilt[1].lp (wr), wr, tone);

            const float m = mk.update (L[i], R[i], wl, wr, false);
            L[i] = L[i] * gd + wl * m * gw;
            R[i] = R[i] * gd + wr * m * gw;
        }
    }

private:
    //  Martian Gain's four, verbatim in behaviour
    inline float shape (int eng, float u, float chr, int ch)
    {
        switch (eng)
        {
            case 0:     // VALVE — asymmetric tanh; the lopsidedness IS the 2nd harmonic
            {
                const float a = 0.25f + 0.75f * chr;
                return u >= 0.0f ? std::tanh (u)
                                 : std::tanh (u * a) * (0.6f + 0.4f * a);
            }
            case 1:     // FUZZ — different clip levels either way up
            {
                const float hi = 0.35f + 0.65f * chr, lo = 0.35f + 0.65f * (1.0f - chr);
                return clampf (u, -lo, hi) / hi;
            }
            case 2:     // SINE FOLD — the wave turned inside out
                return std::sin (u * (0.6f + 2.4f * chr));
            default:    // ANNIHILATE — fold, quantise to 16 steps, then ring it
            {
                float y = wavefold (u);
                y = std::floor (y * 8.0f + 0.5f) * 0.125f;
                y = clampf (y * 1.6f, -1.0f, 1.0f);
                return y * (1.0f - chr * 0.5f) + chr * 0.5f * std::sin (11.0f * y);
            }
        }
        (void) ch;
    }

    double sr = 48000.0;
    HalfBand hb[2];
    OnePole tilt[2], dcb[2];
    KMakeup mk;
};

// ===========================================================================
// 15 · SWARM — one voice becomes a crowd that never quite agrees.
//
// Up to eight copies, each at its OWN constant pitch offset (a true detune, a
// grain pair reading the line at a fixed rate) and its own slow wander (the
// chorus). The crowd is spread across the field so it widens as it thickens.
//
// NO OVERSAMPLING, and that is measured rather than assumed: at the full
// 50 cents the read rate is 1.029, so content above 0.971 of Nyquist is all
// that can fold — 23.3 kHz and up, where there is nothing. DIVE is the effect
// that moves pitch far enough to need a half-band; this one is not.
const ParamDesc SWARM_P[] = {
    { "voices", "VOICES",  4.0f,  2.0f,   8.0f, 1.0f, "",   Warp::Linear },
    { "detune", "DETUNE", 18.0f,  0.0f,  50.0f, 0.0f, "c",  Warp::Linear },
    { "depth",  "DEPTH",   6.0f,  0.0f,  30.0f, 0.0f, "ms", Warp::Linear },
    { "rate",   "RATE",    0.4f,  0.02f,  6.0f, 0.0f, "Hz", Warp::Log },
    { "spread", "SPREAD", 70.0f,  0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "mix",    "MIX",    70.0f,  0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
const EffectDesc SWARM_D_ { "swarm", "SWARM", "one voice becomes a crowd",
                            SWARM_P, 6, Level::Neutral, true, false, false, true };

class Swarm : public Effect
{
public:
    static constexpr int kMaxVoices = 8;
    const EffectDesc& desc() const override { return SWARM_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            bank[v].prepare (fs, 0.25);
            bank[v].raisedCosine = true;   // two grains off ONE source: correlated
        }
        reset();
    }
    void reset() override
    {
        for (auto& b : bank) b.reset();
        for (int v = 0; v < kMaxVoices; ++v) ph[v] = (double) v * 0.7853981;
    }
    void release (Tail t) override { if (t != Tail::Spill) reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const int   nv     = (int) clampf (pL[0] + 0.5f, 2.0f, (float) kMaxVoices);
        const float cents  = clampf (pL[1], 0.0f, 50.0f);
        const double depth = (double) pL[2] * 0.001 * sr;
        const double rate  = std::max (0.01, (double) pL[3]);
        const float spread = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix    = clampf (pL[5] * 0.01f, 0.0f, 1.0f);

        const int len = std::max (64, (int) (0.060 * sr));      // 60 ms grains
        const int interval = std::max (1, len / 2);
        //  §8.1 computed: nv decorrelated copies sum in POWER
        const float norm = 1.0f / std::sqrt ((float) nv);
        float gd, gw; equalPowerMix (gd, gw, mix);

        for (int i = 0; i < n; ++i)
        {
            float wetL = 0.0f, wetR = 0.0f;
            for (int v = 0; v < nv; ++v)
            {
                //  spread the detune symmetrically, so the crowd has no centre
                //  of gravity away from the note
                const float f = (nv > 1) ? ((float) v / (float) (nv - 1)) * 2.0f - 1.0f : 0.0f;
                const double rateV = semitones (cents * f * 0.01f);

                ph[v] += 2.0 * M_PI * rate * (0.6 + 0.35 * (double) v) / sr;
                if (ph[v] > 2.0 * M_PI) ph[v] -= 2.0 * M_PI;
                /*  EVERY VOICE ITS OWN BASE DELAY, and the bench is what
                    insisted: sharing one meant that at DEPTH 0 all eight read
                    the same sample and summed COHERENTLY, while the 1/sqrt(N)
                    normalisation below is a POWER sum — so the crowd came out
                    sqrt(N) too loud exactly where it was least spread, and
                    DEPTH read as a 2.6 dB level control. A real ensemble
                    staggers its voices; so does this. */
                const double baseV = 0.0020 * sr + 0.0022 * sr * (double) v;
                const double back = baseV + depth * (0.5 + 0.5 * std::sin (ph[v]));

                auto& b = bank[v];
                b.write (L[i], R[i]);
                if (--b.nextLaunch <= 0) { b.nextLaunch = interval; b.launch (back, rateV, len); }
                float l, r;
                b.read (l, r, rateV, true);
                b.advance();

                //  each voice takes its own seat in the field
                const float pan = spread * f;
                const float gl = std::sqrt (0.5f * (1.0f - pan));
                const float gr = std::sqrt (0.5f * (1.0f + pan));
                const float mono = 0.5f * (l + r);
                wetL += mono * gl * (float) M_SQRT2;
                wetR += mono * gr * (float) M_SQRT2;
            }
            L[i] = L[i] * gd + wetL * norm * gw;
            R[i] = R[i] * gd + wetR * norm * gw;
        }
    }

private:
    double sr = 48000.0, ph[kMaxVoices] {};
    GrainBank bank[kMaxVoices];
};

// ===========================================================================
// 16 · DUST — the recording is older than the record.
//
// Quantise the amplitude, hold the samples, wobble the transport, add the room
// the tape was in. DELIBERATELY NOT OVERSAMPLED: a sample-and-hold folds its
// images back into the band, and that fold IS the sound of a cheap converter.
// Every other fast read in this plugin is oversampled precisely so it does not
// do this; here it is the effect, which is why it is worth saying out loud.
const ParamDesc DUST_P[] = {
    //  floored at 2: one bit is a sign function, which is a square wave at
    //  full scale whatever went in — a different effect wearing this name
    { "bits",  "BITS",   16.0f,  2.0f,   16.0f, 0.0f, "",   Warp::Linear },
    { "rate",  "RATE", 48000.0f, 400.0f, 48000.0f, 0.0f, "Hz", Warp::Log },
    { "wow",   "WOW",    20.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear },
    //  0 by default, or silence in stops being silence out
    { "noise", "NOISE",   0.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    //  a tone control may move the loudness of the tone
    { "tone",  "TONE",   70.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
    { "mix",   "MIX",   100.0f,  0.0f,  100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
const EffectDesc DUST_D_ { "dust", "DUST", "older than the record",
                           DUST_P, 6, Level::Neutral, true, false, false };

class Dust : public Effect
{
public:
    const EffectDesc& desc() const override { return DUST_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 0.06) + 16); mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f);
        //  §8.1 measured: crushing to two bits squares the wave up and was
        //  measured 11.5 dB louder. Degradation is not a level control.
        mk.prepare (fs, 0.300, 12.0f);
        rng.seed (0x2B7E1516u);
        reset();
    }
    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        for (auto& f : tone) f.reset();
        hold[0] = hold[1] = 0.0f; acc = 0.0; w = 0; ph = 0.0; slow = 0.0f;
        mk.reset();
        rng.seed (0x2B7E1516u);
    }
    void release (Tail t) override { if (t != Tail::Spill) reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const float bits  = clampf (pL[0], 1.0f, 16.0f);
        const double rate = std::min ((double) pL[1], sr);
        const float wow   = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
        const float noise = clampf (pL[3] * 0.01f, 0.0f, 1.0f);
        const float tone01 = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix   = clampf (pL[5] * 0.01f, 0.0f, 1.0f);

        const float steps = std::pow (2.0f, bits - 1.0f);
        const double inc  = rate / sr;                  // hold advance per sample
        const double wowDepth = 0.0060 * wow * sr;      // up to 6 ms of travel
        for (auto& f : tone) f.setHz (sr, 800.0 + 15000.0 * tone01);
        //  the wet is a mangled copy of the dry: correlated, so linear
        const float gd = 1.0f - mix, gw = mix;

        for (int i = 0; i < n; ++i)
        {
            //  the transport wanders first, so everything after it is late
            buf[0][(size_t) (w & mask)] = L[i];
            buf[1][(size_t) (w & mask)] = R[i];
            ph += 2.0 * M_PI * 0.7 / sr;
            if (ph > 2.0 * M_PI) ph -= 2.0 * M_PI;
            //  a wander that STEPS reads as a fault, so the random part is
            //  slewed (the Black Rider tape lesson)
            slow += 0.0006f * (rng.bip() - slow);
            const double d = 4.0 + wowDepth * (0.5 + 0.5 * std::sin (ph) + 0.6 * slow);
            float xl = catmullRead (buf[0].data(), mask, (int) (w & mask) + size,
                                    clampf ((float) d, 2.0f, (float) (size - 8)));
            float xr = catmullRead (buf[1].data(), mask, (int) (w & mask) + size,
                                    clampf ((float) d, 2.0f, (float) (size - 8)));
            ++w;

            //  sample and hold at the reduced rate
            acc += inc;
            if (acc >= 1.0)
            {
                acc -= std::floor (acc);
                hold[0] = xl; hold[1] = xr;
            }
            xl = hold[0]; xr = hold[1];

            //  quantise, with the dither a real converter would have
            if (bits < 15.9f)
            {
                const float d0 = rng.bip() * (0.5f / steps);
                xl = std::floor ((clampf (xl + d0, -1.0f, 1.0f)) * steps + 0.5f) / steps;
                xr = std::floor ((clampf (xr + d0, -1.0f, 1.0f)) * steps + 0.5f) / steps;
            }

            xl = tone[0].lp (xl);
            xr = tone[1].lp (xr);

            if (noise > 0.0f)
            {
                const float h = noise * 0.02f;
                xl += rng.bip() * h; xr += rng.bip() * h;
            }

            const float m = mk.update (L[i], R[i], xl, xr, false);
            L[i] = L[i] * gd + xl * m * gw;
            R[i] = R[i] * gd + xr * m * gw;
        }
    }

private:
    double sr = 48000.0, acc = 0.0, ph = 0.0;
    int size = 0, mask = 0;
    long long w = 0;
    std::vector<float> buf[2];
    OnePole tone[2];
    KMakeup mk;
    float hold[2] {}, slow = 0.0f;
    Rng rng;
};

// ===========================================================================
// 17 · ORBIT — the sound goes round the head.
//
// Peter's brief exactly: near, right, back, left. ANGLE is where on that
// circle the source is, and the SCORE sweeps it, so a build can walk the sound
// round behind the listener and bring it back to the front on the drop.
//
// Three cues, because one is not convincing:
//   SIDE   an equal-power pan, which is the only cue a pan pot gives you.
//   BACK   quieter, darker and LATE. Behind the head there is no direct path:
//          the sound arrives round the skull, so it loses its top and a few
//          hundred microseconds. Moving that delay as the angle sweeps is a
//          doppler, and it is the cue that sells the circle.
//   NEAR   the front of the orbit is closest, so it is loudest and brightest.
//
// ANGLE IS DECLARED A LEVEL KNOB. Something behind you IS quieter; that is
// what tells you it is behind you, and compensating it would delete the whole
// effect (§8.1's per-parameter rule exists for exactly this).
const ParamDesc ORBIT_P[] = {
    { "angle", "ANGLE",   0.0f, 0.0f, 360.0f, 0.0f, "d",  Warp::Linear, nullptr, true },
    //  a level knob for the SAME one reason ANGLE is: the circle has a quiet
    //  side by construction, and both of these move the source round it
    { "spin",  "SPIN",    0.0f, -4.0f,  4.0f, 0.0f, "Hz", Warp::Linear, nullptr, true },
    { "width", "WIDTH",  85.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "rear",  "REAR",   70.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "shade", "SHADE",  60.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "mix",   "MIX",   100.0f, 0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
//  movesPitch: the rear delay MOVES as the angle sweeps, and a moving delay
//  is a doppler — 6.3 cents of it, measured. It is the cue, not a leak.
const EffectDesc ORBIT_D_ { "orbit", "ORBIT", "round the head, and behind it",
                            ORBIT_P, 6, Level::Neutral, true, false, false };

class Orbit : public Effect
{
public:
    const EffectDesc& desc() const override { return ORBIT_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        size = nextPow2 ((int) (fs * 0.02) + 16); mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f);
        reset();
    }
    void reset() override
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        for (auto& f : shade) f.reset();
        spinPh = 0.0; w = 0;
    }
    void release (Tail t) override { if (t != Tail::Spill) reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const double angle0 = (double) pL[0] * M_PI / 180.0;
        const double spin   = (double) pL[1];
        const float width   = clampf (pL[2] * 0.01f, 0.0f, 1.0f);
        const float rear    = clampf (pL[3] * 0.01f, 0.0f, 1.0f);
        const float shade01 = clampf (pL[4] * 0.01f, 0.0f, 1.0f);
        const float mix     = clampf (pL[5] * 0.01f, 0.0f, 1.0f);
        const float gd = 1.0f - mix, gw = mix;

        for (int i = 0; i < n; ++i)
        {
            spinPh += 2.0 * M_PI * spin / sr;
            if (spinPh > 2.0 * M_PI) spinPh -= 2.0 * M_PI;
            if (spinPh < 0.0) spinPh += 2.0 * M_PI;
            const double a = angle0 + spinPh;

            const float side = (float) std::sin (a) * width;      // -1 left .. +1 right
            const float back = 0.5f * (1.0f - (float) std::cos (a));   // 0 front .. 1 behind

            buf[0][(size_t) (w & mask)] = L[i];
            buf[1][(size_t) (w & mask)] = R[i];

            //  behind the head the path is longer: up to 0.7 ms, and it MOVES
            const float dly = clampf (2.0f + back * rear * (float) (0.0007 * sr),
                                      2.0f, (float) (size - 8));
            float xl = catmullRead (buf[0].data(), mask, (int) (w & mask) + size, dly);
            float xr = catmullRead (buf[1].data(), mask, (int) (w & mask) + size, dly);
            ++w;

            //  and it loses its top going round the skull
            const double fc = 18000.0 * std::pow (0.05, (double) (back * shade01));
            shade[0].setHz (sr, fc); shade[1].setHz (sr, fc);
            xl = shade[0].lp (xl); xr = shade[1].lp (xr);

            //  the pan, and the level the distance costs
            const float gl = std::sqrt (0.5f * (1.0f - side));
            const float gr = std::sqrt (0.5f * (1.0f + side));
            const float lvl = 1.0f - 0.45f * back * rear;
            const float mono = 0.5f * (xl + xr);
            const float wl = mono * gl * (float) M_SQRT2 * lvl;
            const float wr = mono * gr * (float) M_SQRT2 * lvl;

            L[i] = L[i] * gd + wl * gw;
            R[i] = R[i] * gd + wr * gw;
        }
    }

private:
    double sr = 48000.0, spinPh = 0.0;
    int size = 0, mask = 0;
    long long w = 0;
    std::vector<float> buf[2];
    OnePole shade[2];
};

// ===========================================================================
// 18 · CHANT — the track is made to speak.
//
// A vocoder with ONE input: what arrives is the MODULATOR, and the CARRIER is
// generated here, because an insert has no second signal and giving the plugin
// a side-chain bus to serve one effect would change what every host thinks it
// is. So the carrier has its own PITCH, and the score can sweep that — a build
// where the drums learn to talk and the talking rises.
//
// Analysis: a bank of bandpasses on the input sum, each followed by an
// envelope follower. Synthesis: the same bank, optionally SHIFTED by FORMANT,
// applied to the carrier and scaled by the matching envelope. The bands are
// re-tuned only when something moved, because re-tuning 72 filters per sample
// is how a vocoder eats a core.
const char* kCarriers = "SAW|PULSE|NOISE|SUB";
const ParamDesc CHANT_P[] = {
    { "carrier", "CARRIER",  0.0f,  0.0f,   3.0f, 1.0f, "",   Warp::Linear, kCarriers },
    { "pitch",   "PITCH",    0.0f, -24.0f, 24.0f, 0.0f, "st", Warp::Linear },
    { "bands",   "BANDS",    1.0f,  0.0f,   2.0f, 1.0f, "",   Warp::Linear, "8|16|24" },
    { "formant", "FORMANT",  0.0f, -12.0f, 12.0f, 0.0f, "st", Warp::Linear },
    { "resp",    "RESPONSE", 12.0f, 1.0f, 200.0f, 0.0f, "ms", Warp::Log },
    { "tone",    "TONE",    50.0f,  0.0f, 100.0f, 0.0f, "%",  Warp::Linear },
    { "mix",     "MIX",    100.0f,  0.0f, 100.0f, 0.0f, "%",  Warp::Linear, nullptr, true },
};
//  movesPitch: the output is the CARRIER, opened by the modulator's bands.
//  Keeping the input's pitch would mean it was not a vocoder.
const EffectDesc CHANT_D_ { "chant", "CHANT", "the track is made to speak",
                            CHANT_P, 7, Level::Neutral, true, false, false };

class Chant : public Effect
{
public:
    static constexpr int kMaxBands = 24;
    const EffectDesc& desc() const override { return CHANT_D_; }

    void prepare (double fs, int) override
    {
        sr = fs;
        for (int b = 0; b < kMaxBands; ++b)
        {
            ana[b].prepare (fs);
            syn[0][b].prepare (fs);
            syn[1][b].prepare (fs);
        }
        mk.prepare (fs, 0.250, 12.0f);
        rng.seed (0x3C6EF372u);
        reset();
    }
    void reset() override
    {
        for (int b = 0; b < kMaxBands; ++b)
        {
            ana[b].reset(); syn[0][b].reset(); syn[1][b].reset();
            env[b].reset();
        }
        mk.reset();
        phase = 0.0; lastN = -1; lastFormant = 1.0e9f; lastBands = -1;
        rng.seed (0x3C6EF372u);
    }
    void release (Tail t) override { if (t != Tail::Spill) reset(); }

    void process (float* L, float* R, int n, const float* pL, const float*, const Ctx&) override
    {
        const int   carrier = (int) clampf (pL[0] + 0.5f, 0.0f, 3.0f);
        const float pitch   = pL[1];
        const int   bandSel = (int) clampf (pL[2] + 0.5f, 0.0f, 2.0f);
        const float formant = pL[3];
        const double respMs = std::max (0.5, (double) pL[4]);
        const float tone    = clampf (pL[5] * 0.01f, 0.0f, 1.0f);
        const float mix     = clampf (pL[6] * 0.01f, 0.0f, 1.0f);

        const int nb = (bandSel == 0) ? 8 : (bandSel == 1 ? 16 : 24);

        //  re-tune only when something actually moved: 72 filter updates per
        //  sample is what makes a vocoder expensive, and nothing here changes
        //  per sample
        if (nb != lastBands || std::abs (formant - lastFormant) > 1.0e-4f)
        {
            retune (nb, formant);
            lastBands = nb; lastFormant = formant;
        }
        if (nb != lastN) { for (auto& e : env) e.setTau (sr, respMs * 0.001); lastN = nb; }
        for (int b = 0; b < nb; ++b) env[b].setTau (sr, respMs * 0.001);

        //  the carrier's note. 65.4 Hz is C2, low enough to have harmonics for
        //  every band to find.
        const double f0 = 65.406 * semitones (pitch);
        const double inc = f0 / sr;
        const float gd = 1.0f - mix, gw = mix;

        for (int i = 0; i < n; ++i)
        {
            const float modIn = 0.5f * (L[i] + R[i]);

            phase += inc;
            if (phase >= 1.0) phase -= std::floor (phase);
            float car = 0.0f;
            switch (carrier)
            {
                case 0: car = (float) (2.0 * phase - 1.0); break;                  // SAW
                case 1: car = phase < 0.45 ? 1.0f : -1.0f; break;                  // PULSE
                case 2: car = rng.bip(); break;                                    // NOISE
                default: car = (float) std::sin (2.0 * M_PI * phase)
                             + 0.35f * (float) std::sin (4.0 * M_PI * phase); break; // SUB
            }
            car *= 0.5f;

            float wet = 0.0f;
            for (int b = 0; b < nb; ++b)
            {
                const float a = ana[b].process (modIn);
                const float e = env[b].lp (std::abs (a));
                //  the same band on the carrier, opened by what the voice had
                wet += syn[0][b].process (car) * e * 4.0f;
            }
            //  TONE tilts the result, so a dark carrier can still be made to cut
            wet = lerpf (tilt.lp (wet), wet, tone);

            const float m = mk.update (L[i], R[i], wet, wet, false);
            const float v = wet * m;
            L[i] = L[i] * gd + v * gw;
            R[i] = R[i] * gd + v * gw;
        }
    }

private:
    void retune (int nb, float formant)
    {
        //  log-spaced from 150 Hz to 7 kHz; Q rises with the band count so the
        //  windows keep overlapping rather than leaving gaps
        const double lo = 150.0, hi = 7000.0;
        const float q = 2.0f + 0.22f * (float) nb;
        const double shift = semitones (formant);
        for (int b = 0; b < nb; ++b)
        {
            const double t = (nb > 1) ? (double) b / (double) (nb - 1) : 0.0;
            const double f = lo * std::pow (hi / lo, t);
            ana[b].set ((float) f, q);
            syn[0][b].set ((float) std::min (f * shift, sr * 0.45), q);
            syn[1][b].set ((float) std::min (f * shift, sr * 0.45), q);
        }
        tilt.setHz (sr, 1400.0);
    }

    double sr = 48000.0, phase = 0.0;
    Bp2 ana[kMaxBands], syn[2][kMaxBands];
    OnePole env[kMaxBands], tilt;
    KMakeup mk;
    Rng rng;
    int lastN = -1, lastBands = -1;
    float lastFormant = 1.0e9f;
};

} // namespace

const EffectDesc& swirlDesc()  { return SWIRL_D_; }
const EffectDesc& mangleDesc() { return MANGLE_D_; }
const EffectDesc& swarmDesc()  { return SWARM_D_; }
const EffectDesc& dustDesc()   { return DUST_D_; }
const EffectDesc& orbitDesc()  { return ORBIT_D_; }
const EffectDesc& chantDesc()  { return CHANT_D_; }

Effect* makeSwirl()  { return new Swirl; }
Effect* makeMangle() { return new Mangle; }
Effect* makeSwarm()  { return new Swarm; }
Effect* makeDust()   { return new Dust; }
Effect* makeOrbit()  { return new Orbit; }
Effect* makeChant()  { return new Chant; }

} } // namespace rop::third
