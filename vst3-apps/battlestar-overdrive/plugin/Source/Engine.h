/*  BATTLESTAR OVERDRIVE - engine

    Plain C++ with no JUCE in it, so the offline bench can measure the whole
    signal path without a host. The DSP infrastructure (half-band resampler,
    biquads, the shaper bank) is ported from Martian Gain, which is bench-proven;
    everything above it - ANTITHRUST, SPACE, SPECTRUM and the fuel tank - is new
    here and is measured by test/bench.cpp.
*/
#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bo
{

//==============================================================================
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf  (float a, float b, float t)   { return a + (b - a) * t; }

inline float flushDenorm (float x)
{
    return (std::abs (x) < 1.0e-20f) ? 0.0f : x;
}

/*  A soft ceiling: exactly transparent below `knee` of the limit, asymptotic
    above it, so nothing can exceed `lim` however the loops are set and there is
    no hard corner to hear on the way. A hard clamp was measured being reached
    in 116 of 300 random settings - routine, not a corner case - which is the
    one kind of distortion nobody wants on top of the intended one. */
inline float ceilSoft (float x, float lim = 1.0f, float knee = 0.70f)
{
    const float t = knee * lim;
    const float a = std::abs (x);
    if (a <= t) return x;                       // IEEE-exact identity below the knee
    const float over = (a - t) / (lim - t);
    const float y = t + (lim - t) * std::tanh (over);
    return (x < 0.0f) ? -y : y;
}

/*  Triangle fold of period 4 about zero: the reflecting boundary a wavefolder
    needs, and unlike fmod-and-clip it is continuous at every fold. */
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

/*  One-pole lowpass, set by coefficient rather than by frequency so the caller
    can move it per sample without a trig call. */
struct OnePole
{
    float a = 0.5f, z = 0.0f;
    inline float process (float x) { z = flushDenorm (z + a * (x - z)); return z; }
    void setHz (float f, double sr)
    {
        const float w = (float) (6.283185307 * clampf (f, 1.0f, (float) sr * 0.49f) / sr);
        a = clampf (w / (1.0f + w), 0.0f, 1.0f);
    }
    void clear() { z = 0.0f; }
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

    void setLP   (float f, double sr, float q);
    void setHP   (float f, double sr, float q);
    void setPeak (float f, double sr, float q, float dB);
    void setLowShelf  (float f, double sr, float q, float dB);
    void setHighShelf (float f, double sr, float q, float dB);
    void copyCoeffs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
};

/*  Linkwitz-Riley 4th order, for the harmonic tremolo's crossover. Its two
    outputs sum to an all-pass, which is exactly what is wanted here: the
    tremolo wants the halves to move against each other, not to cancel. */
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
/*  Polyphase half-band resampler, 63-tap windowed sinc. Two in cascade give 4x,
    which is what every engine here runs at: there is no quality switch on the
    metal, so the plugin does not get to ask. */
struct HalfBand
{
    static constexpr int TAPS = 63;
    static constexpr int PH   = 32;
    static constexpr int MASK = PH - 1;

    std::array<float, PH> ge {}, go {};
    std::array<float, PH> upLine {}, dnE {}, dnO {};
    int upW = 0, dnW = 0;

    void design();
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
            s += ge[(size_t) k] * dnE[(size_t) ((dnW - k) & MASK)]
               + go[(size_t) k] * dnO[(size_t) ((dnW - k - 1) & MASK)];
        dnW = (dnW + 1) & MASK;
        return s;
    }
};

//==============================================================================
/*  A fractional delay line with a Catmull-Rom read. Cubic and not linear
    because a modulated tap read linearly loses its top end audibly - measured
    at about 8 dB of interpolation error in Photo Synth, which is why every
    modulated tap in the house has been cubic since.  */
struct Delay
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void setMaxSamples (int n)
    {
        int cap = 16;
        while (cap < n + 8) cap <<= 1;
        buf.assign ((size_t) cap, 0.0f);
        mask = cap - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    inline float readFrac (float d) const
    {
        d = clampf (d, 1.0f, (float) mask - 3.0f);
        const int   i = (int) d;
        const float f = d - (float) i;
        const int   b = (w - i) & mask;
        const float y0 = buf[(size_t) ((b + 1) & mask)];
        const float y1 = buf[(size_t) b];
        const float y2 = buf[(size_t) ((b - 1) & mask)];
        const float y3 = buf[(size_t) ((b - 2) & mask)];
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 =         y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0              + 0.5f * y2;
        return ((a0 * f + a1) * f + a2) * f + y1;
    }

    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
};

/*  Schroeder allpass. Its magnitude response is exactly flat - only the phase
    moves - which is what makes it the right tool for widening: it decorrelates
    without colouring, where a comb would leave notches all over the tone. */
struct APDelay
{
    Delay line;
    float c = 0.5f;
    int   D = 64;

    void setup (int d, float coef) { D = std::max (1, d); line.setMaxSamples (D + 8); c = coef; }
    void clear() { line.clear(); }

    inline float process (float x)
    {
        const float v = line.readInt (D + 1);
        const float y = -c * x + v;
        line.write (flushDenorm (x + c * y));
        return y;
    }
};

//==============================================================================
enum EngineId
{
    E_IDLE = 0,     // IDLE BURN    soft asymmetric tube
    E_ION,          // ION DRIVE    the classic soft knee
    E_PLASMA,       // PLASMA COIL  tape saturation with memory
    E_AFTERBURNER,  // AFTERBURNER  asymmetric fuzz
    E_RAZOR,        // RAZOR WING   tanh into hard clip
    E_WARPFOLD,     // WARP FOLD    wavefolder
    E_HYPERDRIVE,   // HYPERDRIVE   Chebyshev harmonic injection
    E_SUPERNOVA,    // SUPERNOVA    fold, crush and chaos
    NUM_ENGINES
};

const char* engineName (int e);
const char* engineBlurb (int e);
float       engineRabid (int e);

/*  Everything expensive about an engine's settings is worked out once per
    control tick and handed to the per-sample code as this. */
struct ShapeCo
{
    float k = 1.0f;     // pre-gain into the shaper
    float a = 0.0f;     // per-engine character
    float b = 0.0f;
    float trim = 1.0f;  // measured output match
};

struct ShaperState
{
    float tapeMem = 0.0f;
    float last = 0.0f;
    void clear() { tapeMem = last = 0.0f; }
};

ShapeCo  engineCoeffs (int engine, float thrust, float fuelSag);
float    shapeSample  (int engine, float x, const ShapeCo& co, ShaperState& st);

//==============================================================================
/*  SPACE SYNC divisions, as a fraction of a whole note. Index 0 is FREE and
    is the default: with it, and with no host clock, the delay behaves exactly
    as it always did. */
enum { SYNC_FREE = 0, NUM_SYNC = 8 };
extern const float SYNC_BEATS[NUM_SYNC];      // in quarter notes
extern const char* SYNC_NAMES;                // pipe separated, for the host

struct Params
{
    float mix        = 1.00f;
    float thrust     = 0.35f;
    float antithrust = 0.00f;
    float space      = 0.00f;
    float spectrum   = 0.50f;
    int   engine     = E_ION;
    bool  autorefill = true;

    /*  Hidden: not on the metal, but the host can see both.

        spaceWet is the overall FX level, and it is BIPOLAR about its centre:
        0.5 is exactly today's sound (an IEEE-exact x1.0, so a patch that
        never touches it is bit-identical), 0 turns every SPACE effect off,
        and 1 goes further than today. */
    float spaceWet   = 0.50f;
    int   spaceSync  = SYNC_FREE;
    double bpm       = 0.0;              // from the host; 0 means no clock
};

//==============================================================================
/*  An FDN-8 hall with a shimmer path folded into its own feedback, which is
    the arrangement already proven in Blade Ruiner and BWFX SHIMMER. */
struct Hall
{
    static constexpr int N = 8;
    std::array<Delay, N> line;
    std::array<OnePole, N> damp;
    std::array<float, N> len {};
    DCBlock dcL, dcR;

    // the octave-up path: two taps reading at double rate, crossfaded
    Delay shiftBuf;
    float shiftPhase = 0.0f;
    float shiftWin = 0.0f;

    void prepare (double sr);
    void clear();
    void process (float inL, float inR, float rt60, float damping,
                  float shimmer, double sr, float& outL, float& outR);
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& newParams) { pending = newParams; }

    /*  In place, stereo. */
    void process (float* left, float* right, int numSamples);

    int   latencySamples() const { return latency; }

    // --- meters, for the panel -------------------------------------------
    float fuelLevel()  const { return fuel; }
    float outLevel()   const { return meterOut; }
    float outPeak()    const { return meterPeak; }
    float inLevel()    const { return meterIn; }
    float driveAmount()const { return meterDrive; }
    bool  misfiring()  const { return misfireGate < 0.9f; }

    /*  The tank has run dry and AUTOREFILL is off: the engine is fizzling out
        or already dead, and the panel should be blinking FUEL EMPTY. */
    bool  fuelEmpty()  const { return emptyLatched; }
    /*  0 at the moment it runs dry, 1 once the five seconds are up. */
    float fizzleAmount() const { return clampf (fizzleT / FIZZLE_SECS, 0.0f, 1.0f); }

    // --- for the bench ----------------------------------------------------
    void  setFuelForTest (float f) { fuel = clampf (f, 0.0f, 1.0f); }
    float spaceDelayMs() const { return dlTimeMs; }

    // One gauge step every five seconds of playing; the tube has 15 frames, so
    // a full tank is 70 seconds of playing. Peter's spec, exactly.
    static constexpr float FUEL_STEPS   = 14.0f;
    static constexpr float SECS_PER_STEP = 5.0f;
    static constexpr float FIZZLE_SECS  = 5.0f;
    // Below this the tank is "next to bottom" - one step from empty - which is
    // where AUTOREFILL tops it up.
    static constexpr float REFILL_AT    = 1.0f / FUEL_STEPS;

private:
    void  updateDerived();
    void  buildTrimTable();

    float trimFor (int engine, float thrust) const;

    float spectrumGainNorm (float s) const;

    double sr = 48000.0;
    int    latency = 0;

    /*  Per-engine output match, MEASURED at prepare by running each engine's
        own shaper over a reference tone at 17 drive settings - not a table of
        numbers typed in, which would drift the first time a shaper is retuned.
        One reference amplitude cannot be right for every input (a static
        compensation calibrated at one level never holds for a saturator), so
        the slow auto-gain mops up what is left. */
    static constexpr int TRIM_PTS = 17;
    std::array<std::array<float, TRIM_PTS>, NUM_ENGINES> trimTable {};



    Params p, pending;
    bool   first = true;

    // --- oversampling ------------------------------------------------------
    std::array<std::array<HalfBand, 2>, 2> hb;   // [channel][stage]

    // --- drive -------------------------------------------------------------
    std::array<ShaperState, 2> shaper;
    std::array<DCBlock, 2> dcPre, dcPost;
    ShapeCo co;

    // --- ANTITHRUST: width, and the choke ----------------------------------
    /*  Worked in MID/SIDE. The knob ADDS decorrelated side derived from the
        mid, and never touches the mid itself, so the mono sum is exactly the
        mono sum it always was - no cancellation at any setting, whatever the
        source. It also means an incoming stereo image is preserved and widened
        rather than replaced, which a sum-to-mono-and-respread widener cannot
        do. At zero the side is untouched and the path is an exact identity. */
    static constexpr int NAP = 5;
    std::array<APDelay, NAP> widthAp;
    Delay widthComb;
    Biquad sideShelf;
    float widthDelaySm = 0.0f;
    float chokeEnv = 0.0f;
    std::array<OnePole, 2> chokeLp;

    // --- SPECTRUM ----------------------------------------------------------
    std::array<Biquad, 2> specMid, specLo, specHi, specRoll;

    // --- SPACE -------------------------------------------------------------
    std::array<Delay, 2> tape;
    std::array<OnePole, 2> tapeLp;
    std::array<float, 2> tapeFbZ { 0.0f, 0.0f };
    float dlTimeMs = 30.0f, dlTimeSm = 30.0f;
    bool  syncWasOn = false;
    float wowPhase = 0.0f, wowVal = 0.0f, wowTarget = 0.0f;
    Hall hall;
    LR4 tremSplit[2];
    float tremPhase = 0.0f;
    std::array<Delay, 2> vib;

    // --- fuel --------------------------------------------------------------
    float fuel = 1.0f;
    float misfireGate = 1.0f;
    float misfireHold = 0.0f;
    float sagSm = 1.0f;
    float fizzleT = 0.0f;           // seconds since the tank ran dry
    bool  emptyLatched = false;
    bool  refilling = false;

    // --- level -------------------------------------------------------------
    // There is NO signal-derived auto-gain. The per-engine trim is measured at
    // prepare and depends only on the parameters, so the compensation moves
    // only when a knob moves and cannot breathe. A tracker that chased the
    // running level measured 3.65 dB of drift in the two seconds after an
    // input step - audible, and a compressor nobody asked for.
    float rmsIn = 0.0f;
    float meterIn = 0.0f, meterOut = 0.0f, meterDrive = 0.0f;
    // Fast attack, slow release: what the panel needs to show the tube
    // overloading, which is a peak event and invisible in an average.
    float meterPeak = 0.0f;

    // --- dry path ----------------------------------------------------------
    std::array<Delay, 2> dry;

    // smoothed control values
    float sMix = 1.0f, sThrust = 0.35f, sAnti = 0.0f, sSpace = 0.0f, sSpec = 0.5f;

    Rng rng;
};

} // namespace bo
