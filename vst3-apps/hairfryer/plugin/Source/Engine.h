/*  HAIRFRYER — engine

    A vocal strip whose middle section tries to do something that is normally
    a physical skill: turn a loud clean singing voice into a metal vocal.

    Why the usual approach fails
    ---------------------------
    Put a distortion on a voice and you get a distorted voice, which is not
    the same thing at all. The reason is structural. A sung note is a SOURCE
    (the glottal pulse train) passed through a FILTER (the vocal tract, whose
    resonances are the formants that make the vowel). A distortion sits after
    the filter, so it mangles the formants and intermodulates them with each
    other. The vowel goes to mush and the result reads as "voice through a
    guitar pedal".

    A real screamer does the opposite. The vocal tract is left alone — which
    is why you can still hear the words — and the SOURCE is what changes: the
    false (ventricular) folds above the true folds are driven into a
    period-doubled or chaotic vibration. The pitch you hear is still the true
    folds, which is why growlers can sing melodies, and the roughness is
    subharmonic energy at f0/2 and f0/3 plus turbulent noise fired in step
    with the glottal pulses.

    So this engine
    --------------
      1.  Splits the incoming voice into source and filter in real time, with
          an order-18 LPC lattice. What comes out is the residual: the
          singer's glottal excitation, with the vowel taken off it.
      2.  Wrecks the RESIDUAL, not the voice. Everything added here comes
          back out wearing the singer's own formants.
      3.  Drives the wreckage with a LOGISTIC MAP, one iteration per detected
          glottal period. Vocal fold dynamics is a textbook bifurcation
          system (Herzel, Titze, Berry), and so is x <- r*x*(1-x): near
          r = 3.2 it settles into a two-cycle, which is period doubling,
          which is an octave-down rasp; at r = 3.83 it drops into the
          period-three window, which is a guttural; past r = 3.57 it is
          chaotic, which is fry. One knob walks the whole cascade.
      4.  Runs a FALSE FOLD oscillator beside it — a bandpass with a
          saturator in its feedback, driven by the residual. Below a certain
          feedback it merely colours; above it, it oscillates on its own and
          entrains to a subharmonic of the voice. That is biphonation, and it
          is what makes a good growl sound like two things at once.
      5.  Fires noise in bursts locked to the glottal closures, injected
          BEFORE the synthesis filter so it is formant-shaped, and gated by
          the same chaotic sequence so it breaks up when the voice does.
          Common fate: it fuses into the voice instead of sitting on top of
          it as hiss.
      6.  Resizes the throat with WARPED LPC — the autocorrelation is taken
          through an allpass chain, so the coefficients describe a spectrum
          on a bent frequency axis, and synthesising with them slides the
          formants without touching the pitch. Down is a bigger chest. Up is
          a pig squeal.

    Two racks run off one analysis, because a metal vocal on a record is
    almost always more than one take stacked, and one throat can only be in
    one place at a time.

    Around all that is the strip the voice needs to survive the treatment:
    gate, de-esser, EQ, an analogue-flavoured compressor, tube warmth and
    sparkle, and a lookahead limiter.

    Latency. The basket delays the wet path (the jitter line's base offset
    plus its oversampler), so the dry side of BLEND is delayed to match or
    the two comb against each other. On top of that the heat stage's
    oversampler and the limiter's lookahead delay everything. The total is
    reported to the host and is deliberately CONSTANT — the heat stage is
    padded so 1x, 2x and 4x all cost the same — because a latency that
    changes when you move a switch makes a host re-align mid-song.
*/
#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace hf
{

static constexpr int SCOPE_BINS  = 64;
static constexpr int LPC_ORDER   = 18;
static constexpr int GAIN_HIST   = 40;   // per-period gains kept for the display
static constexpr int NUM_RECIPES = 9;

//==============================================================================
inline float clamp01 (float v)                  { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }

// 0..1 -> a..b, logarithmically. Every frequency and time control uses this.
inline float xmap (float v, float a, float b) { return a * std::pow (b / a, clamp01 (v)); }

inline float smoothstep (float e0, float e1, float x)
{
    const float t = clamp01 ((x - e0) / (e1 - e0 + 1.0e-12f));
    return t * t * (3.0f - 2.0f * t);
}

inline bool  bad (float v)   { return ! (v > -1.0e9f && v < 1.0e9f); }
inline float clean (float v) { return bad (v) ? 0.0f : v; }

/*  A ceiling that is transparent well below the limit and asymptotic at it.
    Applied unconditionally at the end, so a bad moment in a feedback path
    cannot put a spike into somebody's monitors. */
inline float ceilSoft (float x, float c)
{
    const float a = std::abs (x);
    const float knee = c * 0.7f;
    if (a <= knee) return x;
    const float y = knee + (c - knee) * std::tanh ((a - knee) / (c - knee));
    return x < 0.0f ? -y : y;
}

//==============================================================================
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; }
    uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (u32() >> 8) * (1.0f / 16777216.0f); }
    float bi()  { return uni() * 2.0f - 1.0f; }
};

//==============================================================================
struct OnePole
{
    float z = 0.0f, a = 0.1f;
    void setHz (float hz, double sr)
    {
        a = 1.0f - std::exp (-6.2831853f * clampf (hz, 0.01f, (float) sr * 0.45f) / (float) sr);
    }
    void setTimeMs (float ms, double sr)
    {
        a = 1.0f - std::exp (-1.0f / clampf ((float) (ms * 0.001 * sr), 1.0f, 1.0e7f));
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

//==============================================================================
/*  Direct-form-II transposed biquad, coefficients from the RBJ cookbook —
    the one every mixing engineer's ear is already calibrated against. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    inline float operator() (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset()  { z1 = z2 = 0.0f; }
    void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

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
    void bandpass (float f, float q, double sr)
    {
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (al, 0.0f, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (float f, float q, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / (2.0f * std::max (0.05f, q));
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float f, float slope, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float t = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c),
             A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf (float f, float slope, float gainDb, double sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * clampf (f, 10.0f, (float) sr * 0.49f) / (float) sr;
        const float c = std::cos (w), s = std::sin (w);
        const float al = s * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float t = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c),
             A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
};

//==============================================================================
/*  Topology-preserving state variable filter — used for the false fold
    resonator, where the loop is nonlinear and an RBJ biquad would misbehave. */
struct Svf
{
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0, lastLp = 0;

    void set (float hz, float q, double sr)
    {
        g = std::tan (3.14159265f * clampf (hz, 10.0f, (float) sr * 0.45f) / (float) sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float bp (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lastLp = v2;
        return v1;
    }
    void reset() { ic1 = ic2 = lastLp = 0.0f; }
};

//==============================================================================
/*  Fractional delay, linear interpolation. Short — per-period timing jitter
    and path alignment, nothing long. */
struct Delay
{
    std::vector<float> b;
    int w = 0, n = 0;

    void prepare (int maxSamples)
    {
        n = std::max (8, maxSamples + 4);
        b.assign ((size_t) n, 0.0f);
        w = 0;
    }
    void reset() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; if (++w >= n) w = 0; }
    inline float read (float d) const
    {
        const float dd = clampf (d, 0.0f, (float) (n - 2));
        const int i = (int) dd;
        const float f = dd - (float) i;
        int r0 = w - 1 - i; while (r0 < 0) r0 += n;
        int r1 = r0 - 1;    while (r1 < 0) r1 += n;
        return b[(size_t) r0] + (b[(size_t) r1] - b[(size_t) r0]) * f;
    }
    inline float readInt (int d) const
    {
        int r = w - 1 - (d < 0 ? 0 : (d > n - 2 ? n - 2 : d));
        while (r < 0) r += n;
        return b[(size_t) r];
    }
};

//==============================================================================
/*  Halfband up/down sampler, polyphase FIR.

    The taps are generated in prepare() rather than pasted in from a table,
    because a halfband's structure then cannot be got subtly wrong: with the
    cutoff at exactly a quarter of the doubled rate, every tap an even
    distance from the centre is zero, so one branch is 16 multiplies and the
    other is a bare delay. Round trip is 15 samples at the base rate. */
struct Half
{
    static constexpr int TAPS = 31;
    static constexpr int NEV  = 16;    // the even-indexed taps h[0], h[2] ... h[30]
    static constexpr int DLY  = 7;     // the pure-delay branch

    std::array<float, NEV> he {};
    std::array<float, NEV> ue {}, de {}, dodd {};
    int pu = 0, pd = 0;

    void prepare()
    {
        std::array<double, TAPS> t {};
        const int c = (TAPS - 1) / 2;
        double sum = 0.0;
        for (int i = 0; i < TAPS; ++i)
        {
            const double x = (double) (i - c);
            const double s = (x == 0.0) ? 0.5
                           : std::sin (3.14159265358979 * 0.5 * x) / (3.14159265358979 * x);
            const double w = 0.42 - 0.5 * std::cos (6.283185307179586 * i / (TAPS - 1))
                                  + 0.08 * std::cos (12.56637061435917 * i / (TAPS - 1));
            t[(size_t) i] = s * w;
            sum += t[(size_t) i];
        }
        for (auto& v : t) v /= sum;                        // unity at DC
        for (int k = 0; k < NEV; ++k) he[(size_t) k] = (float) t[(size_t) (2 * k)];
        reset();
    }
    void reset() { ue.fill (0.0f); de.fill (0.0f); dodd.fill (0.0f); pu = pd = 0; }
    static constexpr int roundTrip() { return (TAPS - 1) / 2; }   // 15 at the base rate

    inline void up (float x, float* out)                   // one in, two out
    {
        ue[(size_t) pu] = x;
        float e = 0.0f;
        for (int k = 0; k < NEV; ++k) e += he[(size_t) k] * ue[(size_t) ((pu - k) & (NEV - 1))];
        out[0] = 2.0f * e;
        out[1] = ue[(size_t) ((pu - DLY) & (NEV - 1))];
        pu = (pu + 1) & (NEV - 1);
    }
    inline float down (float a, float b)                   // two in, one out
    {
        de[(size_t) pd]   = a;
        dodd[(size_t) pd] = b;
        float e = 0.0f;
        for (int k = 0; k < NEV; ++k) e += he[(size_t) k] * de[(size_t) ((pd - k) & (NEV - 1))];
        const float o = dodd[(size_t) ((pd - 8) & (NEV - 1))];
        pd = (pd + 1) & (NEV - 1);
        return e + 0.5f * o;
    }
};

//==============================================================================
/*  Pitch tracking.

    A normalised square difference function on a decimated copy — McLeod's
    method, which is cheap and does not octave-halve the way plain
    autocorrelation does. This gives the PERIOD. The glottal pulse INSTANTS
    are found separately, in the engine, by peak-picking the LPC residual:
    the residual spikes at glottal closure by construction, and that is where
    the growl has to be anchored or the whole thing warbles. */
struct PitchTrack
{
    static constexpr int DEC = 4;
    static constexpr int WIN = 512;
    static constexpr int HOP = 64;

    double sr = 48000.0, dsr = 12000.0;
    OnePole aa1, aa2;
    std::vector<float> ring;
    int rw = 0, decCount = 0, hopCount = 0;
    int minLag = 12, maxLag = 200;

    float f0 = 0.0f, clarity = 0.0f;

    void prepare (double sampleRate);
    void reset();
    void pushSample (float x);
    void analyse();
};

//==============================================================================
/*  The LPC analysis frame.

    Three sets of reflection coefficients come out of one window. One is from
    the ordinary autocorrelation and gives a properly whitened residual with
    sharp closure spikes. The other two are from WARPED autocorrelations, one
    per rack, and are what the synthesis filters use — which is what slides
    the formants. At neutral throat a warped set is not computed at all and
    is copied from the plain one, so analysis and synthesis cancel exactly
    and the block is transparent. */
struct LpcFrame
{
    static constexpr int N = 1024;

    std::vector<float> ring, work, warped, win;
    int rw = 0, hop = 0, hopLen = 256;

    std::array<float, LPC_ORDER> kAna {}, kSynA {}, kSynB {};
    float gain = 0.0f;
    bool  valid = false;

    void prepare (double sr);
    void reset();
    inline void push (float x) { ring[(size_t) rw] = x; if (++rw >= N * 2) rw = 0; }
    inline bool due() { return (++hop >= hopLen) ? (hop = 0, true) : false; }
    void analyse (float lamA, float lamB);

private:
    void warpTo (float lambda, std::array<float, LPC_ORDER>& dest, const double* plain);
    static bool levinson (const double* r, int order, float* k, double& err);
};

//==============================================================================
/*  Lattice filters.

    Reflection coefficients are interpolated per sample toward the new
    frame's values: a convex combination of two sets that are each inside the
    unit circle is itself inside the unit circle, so a coefficient update
    cannot make the filter unstable. Interpolating direct-form coefficients
    instead — the obvious thing — has no such guarantee and blows up on
    ordinary speech. */
struct Lattice
{
    std::array<float, LPC_ORDER + 1> bz {};
    std::array<float, LPC_ORDER> k {};

    void reset() { bz.fill (0.0f); }
    void zero()  { k.fill (0.0f); bz.fill (0.0f); }

    inline float analyse (float x)          // voice in, residual out
    {
        std::array<float, LPC_ORDER + 1> b;
        float f = x;
        b[0] = x;
        for (int m = 1; m <= LPC_ORDER; ++m)
        {
            const float km = k[(size_t) (m - 1)];
            const float bp = bz[(size_t) (m - 1)];
            const float fn = f - km * bp;
            b[(size_t) m] = bp - km * f;
            f = fn;
        }
        for (int m = 0; m <= LPC_ORDER; ++m) bz[(size_t) m] = b[(size_t) m];
        return f;
    }

    inline float synth (float e)            // residual in, voice out
    {
        std::array<float, LPC_ORDER + 1> b;
        float f = e;
        for (int m = LPC_ORDER; m >= 1; --m)
        {
            const float km = k[(size_t) (m - 1)];
            const float bp = bz[(size_t) (m - 1)];
            f = f + km * bp;
            b[(size_t) m] = bp - km * f;
        }
        b[0] = f;
        for (int m = 0; m <= LPC_ORDER; ++m) bz[(size_t) m] = b[(size_t) m];
        return f;
    }
};

//==============================================================================
struct RackParams
{
    float level  = 0.0f;
    float chaos  = 0.5f;     // 0..1 -> logistic r, 2.90 .. 4.00
    float grip   = 0.0f;     // how deep the per-period gain swings
    float jitter = 0.0f;
    float rasp   = 0.0f;     // pulsed noise
    float tone   = 0.5f;     // noise colour
    float fold   = 0.0f;     // residual waveshaping
    float split  = 0.0f;     // false fold oscillator
    float ratio  = 0.5f;     // which subharmonic it wants to lock to
    float throat = 0.5f;     // formant slide, 0.5 = neutral
};

/*  The jitter line's base offset — the room the per-period timing wobble
    swings in. One formula, used by the rack that owns the delay and by the
    engine that compensates the dry path; two copies of this number would
    drift apart. */
inline int jitterBase (double sr) { return (int) (0.0011 * sr) + 4; }

/*  One rack: everything that happens to the residual, plus the vocal tract
    it is put back through. */
struct Rack
{
    double sr = 48000.0;
    float  jitBaseF = 56.0f;

    float x = 0.37f;                        // logistic state
    float gainNow = 1.0f, gainTarget = 1.0f, gainStep = 0.0f;
    int   rampLeft = 0;
    float jitNow = 0.0f, jitTarget = 0.0f;
    float nzAmp = 0.0f;                     // this period's noise burst height
    std::array<float, GAIN_HIST> hist {};
    int   histW = 0;

    Delay   jit;
    Rng     rng;
    Biquad  noiseTilt, noisePeak;
    Svf     ff;
    float   ffOut = 0.0f;
    DcBlock ffDc, outDc;
    Lattice syn;
    Half    osUp, osDn;                     // around the folder only
    float   lastTone = -1.0f;

    void  prepare (double sampleRate);
    void  reset();
    void  newPeriod (const RackParams& p, float period, float f0, float voiced);
    // normalised residual in; this rack's synthesised voice out, level applied
    float run (const RackParams& p, float e, float phase, float voiced,
               float crisp, float frameGain);
    int   subOrder() const;                 // 1, 2, 3, 4, or 0 for "no pattern"
};

//==============================================================================
struct Params
{
    // ---- prep
    float gateOn = 0.0f, gateThr = 0.2f, gateAtt = 0.15f, gateHold = 0.3f, gateRel = 0.4f;
    float hpf = 0.35f;
    float deEssOn = 1.0f, deEssFrq = 0.5f, deEssAmt = 0.35f;

    // ---- the basket
    float engOn = 1.0f, blend = 0.85f, crisp = 0.7f, match = 1.0f;
    RackParams a, b;

    // ---- heat
    float drive = 0.3f, tube = 0.35f, sparkle = 0.25f;
    float os = 1.0f;                        // 0 = 1x, 1 = 2x, 2 = 4x

    // ---- seasoning
    float eqOn = 1.0f;
    float loF = 0.3f, loG = 0.5f;
    float p1F = 0.35f, p1G = 0.5f, p1Q = 0.4f;
    float p2F = 0.62f, p2G = 0.5f, p2Q = 0.4f;
    float hiF = 0.7f, hiG = 0.5f;

    // ---- pressure
    float compOn = 1.0f, cThr = 0.45f, cRatio = 0.35f, cAtt = 0.25f, cRel = 0.35f;
    float cKnee = 0.5f, cMake = 0.0f, cAuto = 1.0f;
    float limOn = 1.0f, limCeil = 0.85f, limRel = 0.4f;

    // ---- master
    float inGain = 0.5f, outGain = 0.5f, mix = 1.0f;
};

/*  The nine recipes. Defined here rather than in the plugin wrapper so the
    offline bench measures exactly what the panel loads — a preset that only
    exists in the UI layer is a preset nothing can check. */
const char* recipeName (int i);
const char* recipeBlurb (int i);
void        applyRecipe (int i, Params& p);

//==============================================================================
/*  The parameter table — the single source of truth.

    Every knob is one row: its id, its host-facing name, its default, how to
    print it, and a function that returns the float it lives in inside
    Params. The plugin builds its APVTS from this table, processBlock copies
    values through the same table, the recipes are diffed through it and the
    bench walks it — so the class of mistake where a parameter list and a
    read order drift apart (which needed its own checker on Blade Ruiner)
    cannot be expressed here at all. */
enum PKind
{
    KP_PCT = 0,   // percent
    KP_SW,        // on/off
    KP_HZ,        // xmap(lo, hi) Hz
    KP_MS,        // xmap(lo, hi) ms
    KP_DB,        // (v - 0.5) * span, span in lo
    KP_CEIL,      // lerp(lo, hi) dB
    KP_RATIO,     // xmap(lo, hi) : 1
    KP_OS,        // 0/1/2 -> 1x/2x/4x
    KP_CHAOS,     // prints the logistic r it lands on
    KP_SUB,       // prints the subharmonic it aims at
    KP_THROAT     // prints the formant shift in percent
};

struct PSpec
{
    const char* id;
    const char* name;
    float def;
    int   kind;
    float lo, hi;
    float& (*get) (Params&);
};

int          numParams();
const PSpec& paramSpec (int i);

float chaosR (float knob);          // knob 0..1 -> logistic r, with the p3 plateau
float subRatio (float knob);        // RATIO knob -> multiple of f0 the folds aim at
float throatLambda (float knob);    // THROAT knob -> allpass warp lambda

//==============================================================================
/*  Everything the strip needs one of per channel. Kept in a struct rather
    than as thirty members with L and R suffixes: a suffixed pair is a place
    where the left filter quietly gets used on the right signal, and nothing
    about the sound tells you. */
struct Chan
{
    Biquad  hp1, hp2;                 // input high pass, 4th order
    Biquad  ds1, ds2;                 // de-esser LOW half; the high half is x - low
    Half    up1, dn1, up2, dn2;       // heat oversampling
    float   os4Held = 0.0f;
    OnePole sparkSplit, sparkPost;
    DcBlock heatDc;
    Biquad  eqLo, eqP1, eqP2, eqHi;
    Delay   dry;                      // aligns the dry side of BLEND
    Delay   pad;                      // keeps latency equal at 1x, 2x and 4x
    Delay   lim;                      // limiter lookahead
    Delay   mix;                      // the plugin's own dry, fully delayed

    void prepare (double sr, int basketLatency, int limLen, int total);
    void reset();
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* left, float* right, int numSamples);

    int latencySamples() const { return reportedLatency; }

    Params p;

    // ---- what the panel is shown ---------------------------------------
    float inRms = 0.0f, outRms = 0.0f;
    float gateGr = 0.0f, deEssGr = 0.0f, compGr = 0.0f, limGr = 0.0f;
    float f0Hz = 0.0f, clarity = 0.0f, matchDb = 0.0f, resLevel = 0.0f;
    int   orderA = 1, orderB = 1;
    std::array<float, GAIN_HIST> gainsA {}, gainsB {};

private:
    double sr = 48000.0;
    int reportedLatency = 0;

    Chan ch[2];

    // ---- prep (detectors are mono, shared)
    OnePole gateFast, dsHf, dsAll;
    float   gateGain = 0.0f, dsGain = 1.0f;
    int     gateHoldLeft = 0;

    // ---- analysis (mono — the basket works on the mid signal)
    PitchTrack pitch;
    LpcFrame   frame;
    Lattice    ana;
    std::array<float, LPC_ORDER> kAnaNow {}, kAnaStep {};
    std::array<float, LPC_ORDER> kSynANow {}, kSynAStep {};
    std::array<float, LPC_ORDER> kSynBNow {}, kSynBStep {};
    int   interpLeft = 0;
    float frameGain = 0.0f, frameGainStep = 0.0f;

    // ---- glottal epoch detection
    OnePole epochLp, epochFloor;
    float   ePrev = 0.0f, ePrev2 = 0.0f;
    int     sinceEpoch = 0;
    float   periodNow = 200.0f, phase = 0.0f;

    Rack  rackA, rackB;
    int   basketLatency = 0;

    // ---- heat
    int     heatPad = 0;
    float   heatGain = 1.0f;
    OnePole heatInF, heatOutF;

    // ---- pressure
    OnePole scDet, autoMk;
    float   compG1 = 0.0f, compG2 = 0.0f;
    int     limLen = 0;
    float   limEnv = 0.0f, limG1 = 1.0f, limG2 = 1.0f;

    // ---- level match around the basket
    OnePole matchDryF, matchWetF;
    float   matchGain = 1.0f;

    OnePole inM, outM;

    float heatShape (float x, Chan& c);
    void  updateFilters();

    float lastHpf = -1.0f, lastDs = -1.0f, lastEq[11] {};
    int   lastOs = -1;
};

} // namespace hf
