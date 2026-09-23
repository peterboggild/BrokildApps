/*  MARTIAN GAIN — engine

    A multiband distortion. One to five bands, split by Linkwitz-Riley
    crossovers with the all-pass compensation that keeps the sum flat, each
    band running its own shaper out of sixteen, its own limiter and its own
    trim.

    The thing that makes it usable rather than merely loud is the automatic
    gain match: every band measures its own RMS either side of the shaper and
    corrects the difference. Turning DRIVE up then changes the sound without
    changing how loud it is, so an A/B is honest and the ear is not simply
    bribed by level.
*/
#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace mw
{

static constexpr int MAX_BANDS  = 5;
static constexpr int NUM_ALGOS  = 16;
static constexpr int SCOPE_BINS = 64;
static constexpr int MAX_CABLES = 8;

/*  The patch bay. A cable takes something a band is doing and points it at
    something another band has. Sources numbered from S_AUD1 carry actual
    audio; everything below that is a control value. */
enum Source
{
    S_NONE = 0,
    S_ENV1, S_ENV2, S_ENV3, S_ENV4, S_ENV5,      // band envelopes
    S_INENV, S_OUTENV,                           // in and out
    S_AUD1, S_AUD2, S_AUD3, S_AUD4, S_AUD5,      // band distorted output
    NUM_SOURCES
};

enum Dest
{
    D_NONE = 0,
    D_DRIVE1, D_DRIVE2, D_DRIVE3, D_DRIVE4, D_DRIVE5,
    D_LEVEL1, D_LEVEL2, D_LEVEL3, D_LEVEL4, D_LEVEL5,
    D_CHAR1,  D_CHAR2,  D_CHAR3,  D_CHAR4,  D_CHAR5,
    D_CEIL1,  D_CEIL2,  D_CEIL3,  D_CEIL4,  D_CEIL5,
    D_XOVER1, D_XOVER2, D_XOVER3, D_XOVER4,
    D_IN1, D_IN2, D_IN3, D_IN4, D_IN5,           // audio into that band's shaper
    NUM_DESTS
};

const char* sourceName (int s);
const char* destName (int d);
inline bool isAudioSource (int s) { return s >= S_AUD1 && s <= S_AUD5; }
inline bool isAudioDest   (int d) { return d >= D_IN1  && d <= D_IN5;  }

//==============================================================================
inline float clamp01 (float v)                     { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf  (float v, float a, float b)   { return v < a ? a : (v > b ? b : v); }
inline float lerpf   (float a, float b, float t)   { return a + (b - a) * t; }
inline float xmap    (float t, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (t)); }
inline float flushDenorm (float x)                 { return (std::abs (x) < 1.0e-20f) ? 0.0f : x; }
inline float sgnf (float x)                        { return x < 0.0f ? -1.0f : 1.0f; }

/*  Triangle wavefolder: identity inside |x| <= 1 and bounded everywhere, so a
    fold knob does nothing until it is turned up and can never run away. */
inline float wavefold (float x)
{
    float t = std::fmod (x + 1.0f, 4.0f);
    if (t < 0.0f) t += 4.0f;
    t -= 1.0f;
    return (t > 1.0f) ? (2.0f - t) : t;
}

struct Rng
{
    uint32_t s = 0x2545f491u;
    inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float    uni() { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }
    inline float    bi()  { return uni() * 2.0f - 1.0f; }
};

//==============================================================================
struct DCBlock
{
    float x1 = 0, y1 = 0;
    inline float operator() (float x)
    {
        const float y = x - x1 + 0.9997f * y1;
        x1 = x; y1 = flushDenorm (y); return y1;
    }
    void clear() { x1 = y1 = 0; }
};

/*  Transposed direct form II. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = flushDenorm (b1 * x - a1 * y + z2);
        z2 = flushDenorm (b2 * x - a2 * y);
        return y;
    }
    void clear() { z1 = z2 = 0; }

    void setLP (float f, double sr, float q);
    void setHP (float f, double sr, float q);
    void setAP (float f, double sr, float q);
    void setLowShelf  (float f, double sr, float q, float dB);
    void setHighShelf (float f, double sr, float q, float dB);
    void copyCoeffs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
};

/*  Linkwitz-Riley 4th order: two Butterworth sections in cascade. Its low and
    high outputs sum to a second-order all-pass rather than to unity, which is
    why every band split off earlier is run through a matching all-pass. */
struct LR4
{
    Biquad lp1, lp2, hp1, hp2;
    void set (float f, double sr)
    {
        lp1.setLP (f, sr, 0.70710678f); lp2.copyCoeffs (lp1);
        hp1.setHP (f, sr, 0.70710678f); hp2.copyCoeffs (hp1);
    }
    inline void process (float x, float& lo, float& hi)
    {
        lo = lp2.process (lp1.process (x));
        hi = hp2.process (hp1.process (x));
    }
    void clear() { lp1.clear(); lp2.clear(); hp1.clear(); hp2.clear(); }
};

//==============================================================================
/*  Polyphase half-band resampler, 63-tap windowed sinc. Shapers alias badly;
    this is what keeps the mild ones sounding mild. Two of them in cascade
    give 4x. */
struct HalfBand
{
    static constexpr int TAPS = 63;
    static constexpr int PH   = 32;            // taps per polyphase branch
    static constexpr int MASK = PH - 1;

    std::array<float, PH> ge {}, go {};        // kernel[2k] and kernel[2k+1]
    std::array<float, PH> upLine {}, dnE {}, dnO {};
    int upW = 0, dnW = 0;

    void design();
    void clear() { upLine.fill (0.0f); dnE.fill (0.0f); dnO.fill (0.0f); upW = dnW = 0; }

    // one input sample -> two output samples
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

    // two input samples (even phase, odd phase) -> one output sample
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

//==============================================================================
enum Algo
{
    A_WARM = 0, A_VALVE, A_OVERDRIVE, A_TAPE,
    A_FUZZ, A_RAZOR, A_FOLD, A_SINEFOLD,
    A_RECTIFY, A_BITCRUSH, A_DECIMATE, A_SLEW,
    A_RINGMOD, A_CHEBY, A_SHRED, A_ANNIHILATE
};

const char* algoName (int a);
const char* algoCharName (int a);              // what this algorithm's CHARACTER knob is

/*  Everything expensive about a shaper's settings — powers, exponentials,
    phase increments — is worked out once per control tick and handed to the
    per-sample code as this. */
struct ShapeCo
{
    float k = 1, a = 0, b = 0;
    int   n = 1;
};
ShapeCo makeCo (int algo, float drive, float chr, double sr);

/*  Per-band state the stateful shapers need between samples. */
struct ShaperState
{
    float hold = 0;   int holdCount = 0;       // decimator
    float last = 0;                            // slew / decimator smoothing
    float phase = 0;                           // ring modulator
    float tapeMem = 0;                         // tape hysteresis
    void clear() { hold = 0; holdCount = 0; last = 0; phase = 0; tapeMem = 0; }
};

float shapeSample (int algo, float x, const ShapeCo& co, ShaperState& st, Rng& rng);

//==============================================================================
struct Params
{
    struct Band
    {
        float on = 1, solo = 0;
        int   algo = 0;
        float drive = 0.35f, bias = 0.5f, chr = 0.5f;
        float tone = 0.5f, mix = 1.0f, level = 0.5f, ceil = 0.9f;
    };
    std::array<Band, MAX_BANDS> band;

    struct Cable { int src = 0, dst = 0; float amt = 0.0f; };   // amt is bipolar
    std::array<Cable, MAX_CABLES> cable;

    std::array<float, MAX_BANDS - 1> xover { { 0.47f, 0.62f, 0.74f, 0.85f } };  // normalised
    int   bands = 3;
    int   os = 1;                              // 0 = 1x, 1 = 2x, 2 = 4x
    float inGain = 0.5f, outGain = 0.5f, dryWet = 1.0f;
    float autoGain = 1.0f, masterLim = 1.0f;
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int blockSize);
    void reset();
    void process (float* left, float* right, int numSamples);

    static float xoverHz (float norm) { return xmap (norm, 20.0f, 20000.0f); }

    Params p;

    // --- what the panel is shown -------------------------------------------
    std::array<float, MAX_BANDS> bandRms {}, bandGr {}, bandAuto {};
    std::array<float, NUM_DESTS> modOut {};       // what the cables are doing, for the panel
    std::array<float, MAX_BANDS - 1> xoverHzNow { { 200, 900, 3000, 8000 } };
    float inRms = 0, outRms = 0, masterGr = 1;

private:
    void applyDerived();
    inline void core (float l, float r, float& ol, float& orr);

    double sr = 48000.0, osSr = 96000.0;
    int    osFactor = 2;
    Rng    rng;

    static constexpr int CTRL = 32;
    int ctrlCount = 0;

    // --- crossover tree, per channel
    std::array<std::array<LR4, MAX_BANDS - 1>, 2> split;
    std::array<std::array<std::array<Biquad, MAX_BANDS - 1>, MAX_BANDS>, 2> comp;
    std::array<int, MAX_BANDS> compCount {};

    struct BandState
    {
        std::array<ShaperState, 2> shaper;
        std::array<Biquad, 2> tiltLo, tiltHi;
        std::array<DCBlock, 2> dc;
        float preSq = 0, postSq = 0, autoG = 1;
        float limEnv = 0, gr = 1, rms = 0;
        float env = 0;                            // faster follower, for the cables
    };
    std::array<BandState, MAX_BANDS> bs;

    std::array<std::array<HalfBand, 2>, 2> hb;   // [stage][channel]

    struct Derived
    {
        std::array<float, MAX_BANDS> lvl   { { 1, 1, 1, 1, 1 } };
        std::array<float, MAX_BANDS> ceil  { { 0.9f, 0.9f, 0.9f, 0.9f, 0.9f } };
        std::array<float, MAX_BANDS> mix   { { 1, 1, 1, 1, 1 } };
        std::array<float, MAX_BANDS> bias  { { 0, 0, 0, 0, 0 } };
        std::array<float, MAX_BANDS> gate  { { 0, 0, 0, 0, 0 } };
        std::array<int,   MAX_BANDS> algo  { { 0, 0, 0, 0, 0 } };
        std::array<ShapeCo, MAX_BANDS> co;
        float inG = 1, outG = 1, wet = 1;
        float limAtk = 0.4f, limRel = 0.002f, autoK = 0.0004f, autoSlew = 0.0015f;
    } d;

    // ---- patch bay
    float srcControl (int s) const;
    std::array<float, MAX_BANDS> lastOut {};      // previous sample, per band
    std::array<float, MAX_BANDS> inject {};       // audio arriving through a cable
    float inEnvF = 0, outEnvF = 0;
    float envAtk = 0.02f, envRel = 0.002f;

    DCBlock outDcL, outDcR;
    float masterEnv = 0, masterRel = 0.002f;
    int   nb = 3;

    /*  Changing the band count or the oversampling rebuilds the whole filter
        tree, which cannot be done between two samples without a click. The
        wet path is faded down, rebuilt, and faded back up — about twelve
        milliseconds, heard as a shift rather than a bang. */
    int   nbWant = 3, osWant = 2, structPhase = 0;
    float structFade = 1.0f, structStep = 0.0004f;

    /*  Straight after a rebuild every band is measuring a different slice
        of the spectrum, so the gain match is briefly wrong. It is allowed
        to converge about ten times faster until it has caught up, which
        keeps the settle inside the fade instead of clipping after it. */
    int   settle = 0;
};

} // namespace mw
