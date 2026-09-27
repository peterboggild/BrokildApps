/*  BLACK RIDER — engine

    A two-oscillator analogue monosynth that sits between two classics
    without copying either: the filter can be the Sallen-Key screamer with a
    diode clipper in its feedback path (the Korg MS-20 kind — the resonance
    goes nasty, keeps its bass and squeals with harmonics when it
    self-oscillates) or a four-pole transistor ladder with a saturator in
    every stage (the Moog kind — the resonance is round, the bass goes away
    as it comes up, and it sings a clean sine when it takes off).

    What makes it sound like a circuit rather than a formula
    ------------------------------------------------------
    Every part of the signal path does the thing its analogue equivalent
    does when pushed, because those are the things an ear recognises:

      * The oscillators are phase accumulators with polynomial band-limited
        steps and ramps (polyBLEP / polyBLAMP), run 2x or 4x oversampled,
        and each one has its own slow temperature walk, its own fast jitter,
        an exponential converter that goes flat at the top of the range the
        way a real one does (reset time), and a weak injection-locking pull
        towards the other oscillator when they sit close — so two oscillators
        detuned by a few cents slowly fall into lock, exactly as they do on
        a single circuit board. All of this is scaled by one VINTAGE knob.
      * Hard sync is done properly: the slave's discontinuity is band-limited
        with a step whose height is the actual jump, not a fixed one.
      * The mixer saturates. So does the filter input. So do both filter
        models, in their own different ways, which is most of the difference
        between them.
      * The envelopes are the RC shapes — attack charging towards a target
        above the ceiling and clamped, decay and release exponential — and a
        retrigger starts from wherever the envelope is, never from zero.
      * Three voices, each with its own component tolerances: a poly patch
        is three slightly different synthesizers.

    The patch bay is eight cables between any source and any destination,
    summed, at audio rate. Nothing distinguishes "audio" from "control":
    an oscillator into the cutoff is filter FM, an envelope into the delay
    time is a pitch-shifting echo, the VCA output into the filter input is
    the famous MS-20 feedback patch.

    The effects are the ones a monosynth actually gets played through: an
    asymmetric drive, a BBD chorus, a tape delay whose time control really
    does bend the pitch while it moves, and a spring reverb built from three
    dispersive allpass springs — the "boing" is the dispersion, not an EQ.
*/
#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string>

namespace bk
{

static constexpr int MAX_VOICES   = 5;   // poly voices
static constexpr int UNI_VOICES   = 3;   // unison keeps its three-voice fan
static constexpr int NUM_CABLES   = 8;
static constexpr int MAX_OS       = 4;
static constexpr int SCOPE_N      = 2048;   // power of two: the ring is masked
static constexpr int NUM_SOURCES  = 16;   // including "-"
static constexpr int NUM_DESTS    = 18;   // including "-"
static constexpr int NUM_RECIPES  = 12;

//==============================================================================
inline float clamp01 (float v)                  { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp (float a, float b, float t)   { return a + (b - a) * t; }
inline float xmap (float v, float a, float b)   { return a * std::pow (b / a, clamp01 (v)); }
inline bool  bad (float v)                      { return ! (v > -1.0e9f && v < 1.0e9f); }
inline float clean (float v)                    { return bad (v) ? 0.0f : v; }

/*  A ceiling that is transparent well below the limit and asymptotic at it.
    On the master, unconditionally, so nothing here can put a spike into
    somebody's monitors. */
inline float ceilSoft (float x, float c)
{
    const float a = std::abs (x);
    const float knee = c * 0.7f;
    if (a <= knee) return x;
    const float y = knee + (c - knee) * std::tanh ((a - knee) / (c - knee));
    return x < 0.0f ? -y : y;
}

/*  tanh, rational approximation good to ~1e-6 over the range where it
    matters. The ladder takes five of these per sample per voice. */
inline float ftanh (float x)
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float p = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float q = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return p / q;
}

/*  2^x for the pitch path, called once per oscillator per sample. */
inline float fexp2 (float x)
{
    x = clampf (x, -60.0f, 60.0f);
    const int   i = (int) std::floor (x);
    const float f = x - (float) i;
    // 2^f on [0,1): minimax-ish 5th order
    const float p = 1.0f + f * (0.69314718f + f * (0.24022652f + f * (0.05550411f
                        + f * (0.00961813f + f * 0.00133336f))));
    union { uint32_t u; float fl; } bits;
    bits.u = (uint32_t) ((i + 127) << 23);
    return p * bits.fl;
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
    // 4-point Hermite, for anything whose delay moves
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
/*  Halfband decimator, polyphase FIR, 47 taps with a Kaiser window: the
    stopband is below -90 dB, so what the oscillators and filters put above
    the base-rate Nyquist stays there. Two in, one out. For 4x it is run
    twice in cascade. */
struct HalfDown
{
    static constexpr int TAPS = 47;
    static constexpr int NEV  = 24;
    static constexpr int C    = (TAPS - 1) / 2;          // 23, the centre tap (odd index)
    std::array<float, NEV> he {};
    std::array<float, 32> ev {}, od {};
    int p = 0;

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
        // even-indexed taps h[0], h[2], ..., h[46] -> 24 of them; the odd taps are
        // zero except h[23] = 0.5 (after normalisation ~0.5)
        for (int k = 0; k < NEV; ++k) he[(size_t) k] = (float) t[(size_t) (2 * k)];
        centre = (float) t[(size_t) C];
        reset();
    }
    float centre = 0.5f;
    void reset() { ev.fill (0.0f); od.fill (0.0f); p = 0; }

    /*  y[n] = sum_k h[k] x[2n - k]. With x split into a (even, x[2n]) and
        b (odd, x[2n+1]): the even taps see the even samples, and the only
        odd tap, the centre, sees one odd sample delayed by (C-1)/2 = 11. */
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
/*  The parameter table — the single source of truth. The plugin builds its
    APVTS from it, processBlock copies values through it, the recipes and
    the randomiser are diffed through it, the page is told about it, and the
    bench walks it. */
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
    KP_INT        // plain integer 0..hi (bend range, cable ends)
};

struct Params;

struct PSpec
{
    const char* id;
    const char* name;
    float def;
    int   kind;
    float lo, hi;            // formatting range; for KP_LIST/KP_INT hi is the max index
    float& (*get) (Params&);
};

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);
const char* const* listNames (const char* id, int& count);   // for KP_LIST params
float        paramMax (const PSpec& s);                      // 1, or the list/int max

const char* sourceName (int i);
const char* destName (int i);

//==============================================================================
struct Cable { float src = 0, dst = 0, amt = 0.5f; };

struct Params
{
    // voice
    float mode = 0, spread = 0.7f, udet = 0.25f, glide = 0, legato = 1, tune = 0.5f,
          bend = 2, vintage = 0.35f, os = 1, volume = 0.5f;
    // vco 1
    float o1wave = 0, o1oct = 2, o1semi = 0.5f, o1fine = 0.5f, o1pw = 0.5f, o1pwm = 0, o1lvl = 0.8f;
    // vco 2
    float o2wave = 0, o2oct = 2, o2semi = 0.5f, o2fine = 0.52f, o2pw = 0.5f, o2pwm = 0, o2lvl = 0.7f,
          o2sync = 0, o2fm = 0, o2kbd = 1;
    // mixer
    float suboct = 0, sublvl = 0, nzcol = 0, nzlvl = 0, ringlvl = 0, fdrive = 0.2f;
    // filter
    float fmodel = 0, hpf = 0, hpeak = 0, lpf = 0.6f, lpeak = 0.2f, fenv = 0.65f, fkey = 0.5f, flfo = 0;
    // envelopes
    float e1a = 0.1f, e1d = 0.45f, e1s = 0.3f, e1r = 0.4f, e1vel = 0.3f;
    float e2a = 0.05f, e2d = 0.4f, e2s = 0.8f, e2r = 0.35f, e2vel = 0.5f;
    // lfo
    float lwave = 0, lrate = 0.45f, lpitch = 0, lwheel = 0.5f, lkey = 0;
    float lsync = 0, lfeel = 0;          // host-clock sync: FREE/1-1..1-16, straight/triplet/dotted
    // vca
    float vcamode = 0;
    // effects
    float drv = 0, drvtone = 0.6f;
    float chrate = 0.4f, chdepth = 0.5f, chmix = 0;
    float dltime = 0.55f, dlfb = 0.4f, dltone = 0.5f, dlwow = 0.3f, dlmix = 0;
    float dlsync = 0, dlfeel = 0;
    float spdwell = 0.5f, sptone = 0.5f, spmix = 0;
    // patch bay
    Cable cable[NUM_CABLES];
    // which seed patch is on the dial (0..199). Rides in state; the engine
    // ignores it — it is a selector, not a sound parameter.
    float seed = 0;
    // host tempo, set by the processor from the playhead (not a table param)
    double bpm = 120.0;
};

const char* recipeName (int i);
const char* recipeBlurb (int i);
void        applyRecipe (int i, Params& p);

//==============================================================================
/*  The seed library. Two hundred deterministic patches (Seed.cpp): the same
    number is always the same instrument, everywhere. Each seed is assigned a
    CATEGORY first (bass, lead, pad, ...), then its parameters are generated to
    fit — so the name and the category always match the sound. */
static constexpr int NUM_SEEDS = 200;
int         seedNumCategories();
const char* seedCategoryName (int cat);
void        seedInfo (int seed, int& cat, int& rank);     // category index + rank within it
std::string seedName (int seed);
void        seedPatch (int seed, Params& p);              // writes the whole patch

// knob -> real value, shared by engine, host formatting and bench
float glideMs (float v);
float egAttackMs (float v);
float egDecayMs (float v);
float hzOf (const PSpec& s, float v);

//==============================================================================
/*  One band-limited discontinuity inserter. Every oscillator output is
    delayed by one sample so that the half of a polyBLEP that belongs to the
    sample BEFORE an edge can still be applied to it. */
struct BlepQueue
{
    float prev = 0.0f, cur = 0.0f;     // corrections pending for the previous and current sample
    // a jump of 'delta' that happened 'f' samples ago (0 <= f < 1)
    inline void step (float delta, float f)
    {
        const float g = 1.0f - f;
        prev += 0.5f * delta * f * f;
        cur  -= 0.5f * delta * g * g;
    }
    // a slope change of 'ds' per sample, 'f' samples ago
    inline void ramp (float ds, float f)
    {
        const float g = 1.0f - f;
        prev += ds * f * f * f * (1.0f / 6.0f);
        cur  += ds * g * g * g * (1.0f / 6.0f);
    }
};

enum Wave { W_SAW = 0, W_TRI, W_PULSE, W_SINE };

struct Osc
{
    float ph = 0.0f, inc = 0.0f;
    float pending = 0.0f;            // last naive sample, awaiting output
    BlepQueue bq;
    float lastOut = 0.0f;            // what this oscillator output last sample (for sync & ring)
    float wrapF = -1.0f;             // >= 0 when the phase wrapped this sample: samples since
    bool  wrapped = false;
    OnePole tone;                    // the buffer's rounding of the reset
    DcBlock dc;

    void reset (float phase)
    {
        ph = phase; pending = 0.0f; bq = {}; lastOut = 0.0f; wrapF = -1.0f; wrapped = false;
        tone.reset(); dc.reset();
    }

    static inline float naive (int wave, float p, float pw)
    {
        switch (wave)
        {
            case W_SAW:   return 2.0f * p - 1.0f;
            case W_TRI:   return p < 0.5f ? -1.0f + 4.0f * p : 3.0f - 4.0f * p;
            case W_PULSE: return p < pw ? 1.0f : -1.0f;
            default:      { const float a = 6.2831853f * p; return std::sin (a) + 0.02f * std::sin (2.0f * a); }
        }
    }

    /*  Advance one sample. 'syncF' >= 0 means the master wrapped that many
        samples ago and this oscillator must restart there. Returns the
        band-limited output for the PREVIOUS sample. */
    inline float tick (int wave, float frequencyNorm, float pw, float syncF)
    {
        inc = clampf (frequencyNorm, 0.0f, 0.45f);
        const float phPrev = ph;
        float u = ph + inc;                    // unwrapped
        wrapped = false; wrapF = -1.0f;

        // the oscillator's own edges within (prev, now]
        float ownWrapF = -1.0f;
        if (u >= 1.0f) { u -= 1.0f; ownWrapF = inc > 0.0f ? u / inc : 0.0f; }

        /*  Both 'F's count samples AGO. A reset pre-empts any own edge that
            would have come after it, i.e. one whose "ago" is smaller. */
        const bool doSync = syncF >= 0.0f;
        if (doSync && ownWrapF >= 0.0f && ownWrapF < syncF) ownWrapF = -1.0f;

        // ---- own wrap
        if (ownWrapF >= 0.0f)
        {
            wrapped = true; wrapF = ownWrapF;
            switch (wave)
            {
                case W_SAW:   bq.step (-2.0f, ownWrapF); break;
                case W_TRI:   bq.ramp (8.0f * inc, ownWrapF); break;
                case W_PULSE: if (pw > 0.0f) bq.step (2.0f, ownWrapF); break;
                default: break;
            }
        }
        // ---- pulse falling edge / triangle apex, searched on the unwrapped phase
        if (wave == W_PULSE || wave == W_TRI)
        {
            const float edge = wave == W_PULSE ? pw : 0.5f;
            const float uu = phPrev + inc;
            float ef = -1.0f;
            if (phPrev < edge && uu >= edge) ef = (uu - edge) / std::max (inc, 1.0e-9f);
            else if (uu - 1.0f >= edge && phPrev >= edge) ef = (uu - 1.0f - edge) / std::max (inc, 1.0e-9f);   // after the wrap, in the new cycle
            if (ef >= 0.0f && ! (doSync && ef < syncF))
            {
                if (wave == W_PULSE) bq.step (-2.0f, ef);
                else                 bq.ramp (-8.0f * inc, ef);
            }
        }
        ph = u;

        // ---- sync: restart the phase 'syncF' samples ago
        if (doSync)
        {
            const float phEdge = [&]{ float q = phPrev + (1.0f - syncF) * inc; return q >= 1.0f ? q - 1.0f : q; }();
            const float before = naive (wave, phEdge, pw);
            ph = syncF * inc;
            const float after = naive (wave, 0.0f, pw);
            const float delta = after - before;
            if (std::abs (delta) > 1.0e-6f) bq.step (delta, syncF);
            if (wave == W_TRI)
            {
                const float slopeBefore = phEdge < 0.5f ? 4.0f * inc : -4.0f * inc;
                bq.ramp (4.0f * inc - slopeBefore, syncF);
            }
            wrapped = true; wrapF = syncF;
        }

        const float nowNaive = naive (wave, ph, pw);
        const float out = pending + bq.prev;           // the previous sample, now complete
        pending = nowNaive + bq.cur;                   // this one waits for next sample's "prev" half
        bq.prev = 0.0f; bq.cur = 0.0f;
        return out;
    }
};

//==============================================================================
/*  ADSR with the analogue shapes. Attack charges towards 1.3 and is clamped
    at 1 — the RC curve, fast out of the gate and slowing — decay and release
    are exponential. Retrigger continues from the current level. */
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
    void gate (bool on)
    {
        if (on) stage = ATT;
        else if (stage != IDLE) stage = REL;
    }
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

//==============================================================================
/*  MS-20 style two-pole Sallen-Key ("SCREAM"): two one-pole TPT stages with
    feedback of K through a one-pole of the opposite kind and a clipper in
    the feedback path. Solved as a zero-delay loop for the linear part, then
    refined twice for the clipper. Q = 1/(2-K); K = 2 is the edge of
    self-oscillation and the clipper is what keeps it there. */
struct Korg35
{
    float g = 0.1f, G = 0.1f;
    float s1 = 0, s2 = 0, s3 = 0;
    float sat = 1.0f;

    void reset() { s1 = s2 = s3 = 0.0f; }
    void setG (float gg) { g = gg; G = g / (1.0f + g); }

    inline float clip (float x) const
    {
        // the diode pair: firm, a touch asymmetric so the scream has an edge
        const float t = x > 0.0f ? x : x * 1.12f;
        const float y = sat * ftanh (t / sat);
        return x > 0.0f ? y : y / 1.12f;
    }

    inline float lowpass (float x, float K)
    {
        // stage 1
        const float v1 = (x - s1) * G; const float a = v1 + s1; s1 = a + v1;
        const float oneG = 1.0f - G;
        const float den = 1.0f / (1.0f - G * K * oneG);
        float y = (G * a + oneG * s2 - G * K * oneG * s3) * den;
        // clipper in the feedback, two refinements
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float hp = oneG * (y - s3);
            const float fb = clip (K * hp);
            u = a + fb;
            y = G * u + oneG * s2;
        }
        const float v2 = (u - s2) * G; s2 = y + v2;      // s2 = y + v2 where y = v2 + s2
        const float vh = (y - s3) * G; const float lp = vh + s3; s3 = lp + vh;
        return y;
    }

    inline float highpass (float x, float K)
    {
        const float oneG = 1.0f - G;
        // stage 1, high pass
        const float v1 = (x - s1) * G; const float l1 = v1 + s1; s1 = l1 + v1; const float a = x - l1;
        const float den = 1.0f / (1.0f - K * G * oneG);
        float y = oneG * (a - s2 + K * oneG * s3) * den;
        float u = a;
        for (int it = 0; it < 2; ++it)
        {
            const float lp = G * y + oneG * s3;
            const float fb = clip (K * lp);
            u = a + fb;
            y = oneG * (u - s2);
        }
        const float v2 = (u - s2) * G; s2 = s2 + 2.0f * v2;
        const float vl = (y - s3) * G; const float l3 = vl + s3; s3 = l3 + vl;
        return y;
    }
};

/*  Moog style transistor ladder ("LADDER"): four one-pole stages, each with
    a tanh on its input and output, resonance fed back from the fourth with
    a half-sample average to compensate the unit delay. The bass thins out as
    the resonance rises, which is the correct behaviour and the reason the
    two models feel so different under the same knob. */
struct Ladder
{
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float G = 0.1f;
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }
    void setG (float g) { G = g / (1.0f + g); }

    /*  Four trapezoidal one-poles, the loop solved exactly for the linear
        case (y4 = (G^4 x + S) / (1 + k G^4)), then the differential pair at
        the summing node applied as a tanh. Solving first and saturating
        after keeps the tuning of the resonance exact — a saturator on the
        loop has no phase — while the amplitude of a self-oscillation is
        still decided by the saturator. A gentle cubic on each stage gives
        the stages' own compression without upsetting the loop. */
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
        const float y4 = stage (y3, s4);
        return y4;
    }
};

//==============================================================================
/*  Per-voice sources and sinks of the patch bay, indices matching the name
    tables in Engine.cpp. */
enum Src { S_NONE = 0, S_VCO1, S_VCO2, S_NOISE, S_FILT, S_VCA, S_EG1, S_EG2, S_LFO, S_SH,
           S_KEY, S_VEL, S_WHEEL, S_AT, S_BEND, S_GATE };
enum Dst { D_NONE = 0, D_P1, D_P2, D_PBOTH, D_PW1, D_PW2, D_FM2, D_HPF, D_LPF, D_LPEAK, D_HPEAK,
           D_FIN, D_VCACV, D_VCAIN, D_LRATE, D_PAN, D_DRIVE, D_DLYT };

struct VoiceTol   // component tolerances, fixed per voice
{
    float tune1 = 0, tune2 = 0;      // cents
    float cut = 0;                   // octaves
    float envA = 1, envD = 1, envR = 1;
    float drift1 = 0, drift2 = 0;    // phase of the slow walks
};

struct Voice
{
    Osc o1, o2;
    Korg35 k35lp, k35hp;
    Ladder lad;
    Env eg1, eg2;
    Rng rng;
    VoiceTol tol;

    int   note = -1, lastNote = 60;
    float vel = 0.8f;
    bool  gate = false;
    int   order = 0;                 // note-on serial, for stealing
    int   seat = -1;                 // poly stereo station 0..4, -1 unseated
    uint64_t onAt = 0;               // note-on time in samples, for the strum window
    float pitch = 60.0f;             // glided note number
    float pan = 0.0f, panTarget = 0.0f;
    float subPh = 0.0f, subState = -1.0f, subPending = 0.0f;   // the divider off VCO1
    BlepQueue subBq;
    float pinkB0 = 0, pinkB1 = 0, pinkB2 = 0;
    float driftS1a = 0, driftS1b = 0, driftS2a = 0, driftS2b = 0, jit1 = 0, jit2 = 0;
    float lastFilt = 0.0f, lastVca = 0.0f, lastNoise = 0.0f;
    float vcaSmooth = 0.0f;
    float wmGate = 0.0f;             // smoothed gate for the world-mod sag
    DcBlock outDc;
    int   ctrlPhase = 0;
    float gK35 = 0.1f, gHp = 0.1f, gLad = 0.1f;      // held filter coefficients
    float driveG = 1.0f;

    void reset();
    bool sounding() const { return eg2.active() || gate; }
};

//==============================================================================
/*  One spring: a delay loop with a long cascade of first-order allpasses in
    it. A helical spring is stiffness-dominated for transversal waves, so
    high frequencies travel FASTER and every reflection arrives as a
    downward chirp — the "boing". A first-order allpass with a positive
    coefficient has exactly that shape of group delay, large at DC and
    small at Nyquist, and sixty of them in series make the chirp long enough
    to hear. The loop is band-limited the way the transducers are. */
struct Spring
{
    static constexpr int MAX_AP = 192;
    Delay line;
    std::array<float, MAX_AP> apz {};
    int nAp = 60;
    float apA = 0.75f;
    int delaySamples = 1000;
    OnePole lp, hp;
    float fbLast = 0.0f;
    void prepare (double sr, float ms, int stagesAt48k, float coef);
    void reset();
    inline float tick (float x, float fb)
    {
        float v = ceilSoft (x + fb * fbLast, 2.0f);
        line.push (v);
        float y = line.readInt (delaySamples);
        for (int i = 0; i < nAp; ++i)
        {
            const float z = apz[(size_t) i];
            const float out = -apA * y + z;
            apz[(size_t) i] = y + apA * out;
            y = out;
        }
        y = hp.hp (lp.lp (y));
        fbLast = y;
        return y;
    }
};

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
    // Brokild World FX world-mod bus (plain stores, any thread). Fanned
    // across the voices in renderVoice; a NEUTRAL bus (0,0,0,0,0,1) is
    // skipped entirely, so it is bit-identical to not calling this at all.
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

    void setBend (float v)   { bendIn = clampf (v, -1.0f, 1.0f); }   // -1..1 of the bend range
    void setWheel (float v)  { wheelIn = clamp01 (v); }
    void setAftertouch (float v) { atIn = clamp01 (v); }
    void setSustain (bool on);

    // ---- for the panel
    float outRms = 0.0f, outPeak = 0.0f;
    float uiEnv1 = 0.0f, uiEnv2 = 0.0f, uiLfo = 0.0f, uiCut = 0.0f;
    std::array<float, SCOPE_N> scope {};
    int   scopeWrite = 0;
    std::array<int, MAX_VOICES> uiNotes { -1, -1, -1, -1, -1 };
    std::array<float, MAX_VOICES> uiPan {};
    int   heldCount = 0;
    bool  locked = false;           // VCO2 pulled into lock with VCO1

    double sr = 48000.0;
    int    osf = 2;
    double fsOs = 96000.0;

private:
    void configureRate();
    void renderVoice (Voice& v, int vi, float* outL, float* outR, int nOs);
    void assignPans();
    Voice* allocVoice (int note);
    void retuneUnison();

    std::array<Voice, MAX_VOICES> voices;
    int   noteSerial = 0;
    uint64_t samplesDone = 0;        // running clock for the strum window
    std::array<int, 16> heldStack {}; int heldN = 0;     // mono note memory
    bool  sustain = false;
    std::array<bool, 128> heldKeys {};
    std::array<bool, 128> susKeys {};

    float bendIn = 0.0f, wheelIn = 0.0f, atIn = 0.0f;
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFmul = 1;
    bool  wmActive = false;
    double wmT = 0;                  // seconds, for the trem phases
    float bendSm = 0.0f, wheelSm = 0.0f, atSm = 0.0f;

    // lfo, global
    float lfoPh = 0.0f, lfoVal = 0.0f, shVal = 0.0f, driftVal = 0.0f, driftTarget = 0.0f;
    Rng   lfoRng;
    float lfoRateMod = 0.0f;

    // oversampling, and the per-OS-sample globals the voices read
    std::vector<float> osL, osR, lfoBuf, shBuf, driveModBuf;
    HalfDown dnL1, dnR1, dnL2, dnR2;

    // drive
    DcBlock drvDcL, drvDcR;
    OnePole drvToneL, drvToneR;

    // chorus
    Delay chL, chR;
    float chPh = 0.0f;
    Biquad chBwL, chBwR;
    Rng   chRng;

    // tape delay
    Delay dlL, dlR;
    float dlTimeSm = 0.0f;
    float wowPh = 0.0f, flutPh = 0.0f, wowRnd = 0.0f, wowRndTgt = 0.0f, wowRndT = 0.0f;
    OnePole dlLpL, dlLpR, dlHpL, dlHpR;
    Biquad dlBumpL, dlBumpR;
    Rng   dlRng;
    float dlFbL = 0.0f, dlFbR = 0.0f;

    // spring
    std::array<Spring, 3> springs;
    std::array<float, 2> difz {};       // a short input diffuser
    float difz1 = 0.0f, difz2 = 0.0f;
    Delay dif1, dif2;
    float spInSm = 0.0f;

    // master
    Rng   hissRng;
    float humPh = 0.0f;
    float rmsAcc = 0.0f, peakAcc = 0.0f; int rmsN = 0;
    int   scopeHold = 0;
    float scopePrev = 0.0f;

    int maxBlock = 512;
    int lastOsParam = -1;
    float globalDst[NUM_DESTS] {};
    int   leadVoice = 0;
};

} // namespace bk
