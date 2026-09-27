#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/*  Blade Ruiner — three synthesisers that happen to share an output stage.

    LOS ANGELES is a drone: a stack of detuned oscillators on a chord, a rain
    of filtered noise, grains of kipple, and a reverb big enough to lose a
    city in. It does not need to be played; by default it simply is.

    DECKARD is an eight-voice polysynth built the way a CS-80 is built — two
    oscillators and a sub per voice, a ladder filter with its own envelope,
    ring modulation, a delayed vibrato and the ensemble chorus that does most
    of the emotional work.

    REPLICANT is a machine: a sixteen-step sequence derived deterministically
    from a six-bit seed, driving a two-operator FM voice with an inharmonic
    ratio, a bit-crusher and a heartbeat.                                    */
namespace br
{

constexpr int   MAX_POLY   = 8;     // Deckard voices
constexpr int   LA_STACK   = 9;     // Los Angeles drone oscillators
constexpr int   LA_GRAINS  = 14;    // kipple grains in flight
constexpr int   RP_STEPS   = 16;
constexpr int   NUM_LAYERS = 3;
constexpr int   NUM_CHORDS = 6;
constexpr int   NUM_WAVES  = 4;
constexpr int   NUM_RATES  = 7;
constexpr int   SCOPE_BINS = 96;

constexpr float PI_F     = 3.14159265358979f;
constexpr float TWO_PI_F = 6.28318530717959f;

inline float clamp01 (float v)                    { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float lerp    (float a, float b, float t)  { return a + (b - a) * t; }
inline float xmap    (float v, float lo, float hi){ return lo * std::pow (hi / lo, clamp01 (v)); }
inline float midiHz  (float n)                    { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }

/*  Asymmetric soft clip. The negative half folds a little harder, which is
    where the even harmonics — the "warmth" — come from. */
inline float grit (float x, float bias)
{
    const float b = x + bias * 0.22f;
    const float y = b < 0.0f ? std::tanh (b * 1.35f) * 0.78f : std::tanh (b);
    return y - bias * 0.16f;
}

//==============================================================================
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v)  { s = v ? v : 1u; }
    uint32_t next()         { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni()             { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bi()              { return uni() * 2.0f - 1.0f; }
};

//==============================================================================
struct OnePole
{
    float z = 0.0f, a = 0.02f;
    void setHz (float hz, double sr)
    {
        a = 1.0f - std::exp (-TWO_PI_F * std::max (0.01f, hz) / (float) sr);
        a = std::min (a, 1.0f);
    }
    float lp (float x) { z += (x - z) * a; return z; }
    float hp (float x) { return x - lp (x); }
    void  clear()      { z = 0.0f; }
};

/*  Topology-preserving state variable filter (Zavalishin). Stable when the
    cutoff is swept hard, which every one of these three layers does. */
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f, g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void set (float hz, float q, double sr)
    {
        hz = std::min (std::max (hz, 5.0f), (float) sr * 0.47f);
        g  = std::tan (PI_F * hz / (float) sr);
        k  = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    struct Out { float lp, bp, hp; };
    Out tick (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    void clear() { ic1 = ic2 = 0.0f; }
};

/*  Four one-pole sections inside a feedback loop with a saturator, which is
    what makes a ladder sound like a ladder rather than like four filters. */
struct Ladder
{
    float s[4] = { 0, 0, 0, 0 }, G = 0.1f, k = 0.0f;

    void set (float hz, float res, double sr)
    {
        hz = std::min (std::max (hz, 15.0f), (float) sr * 0.45f);
        const float T  = 1.0f / (float) sr;
        const float wa = (2.0f / T) * std::tan (TWO_PI_F * hz * T * 0.5f);
        const float g  = wa * T * 0.5f;
        G = g / (1.0f + g);
        k = 4.0f * std::min (std::max (res, 0.0f), 0.97f);
    }
    float tick (float x)
    {
        float in = std::tanh ((x - k * s[3]) * 0.75f) * 1.33f;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
        }
        return in;
    }
    void clear() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
};

//==============================================================================
inline float polyBlep (float t, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt)          { t /= dt;             return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt)   { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Osc
{
    float phase = 0.0f, inc = 0.0f;
    void setHz (float hz, double sr) { inc = std::max (0.0f, hz) / (float) sr; }
    void advance()                   { phase += inc; if (phase >= 1.0f) phase -= 1.0f; }

    float saw()
    {
        advance();
        return (2.0f * phase - 1.0f) - polyBlep (phase, inc);
    }
    float pulse (float pw)
    {
        advance();
        pw = std::min (std::max (pw, 0.05f), 0.95f);
        float v = phase < pw ? 1.0f : -1.0f;
        v += polyBlep (phase, inc);
        float pf = phase - pw;
        if (pf < 0.0f) pf += 1.0f;
        v -= polyBlep (pf, inc);
        return v;
    }
    float sine()  { advance(); return std::sin (TWO_PI_F * phase); }
    float tri()   { advance(); return 4.0f * std::abs (phase - 0.5f) - 1.0f; }
    void  clear() { phase = 0.0f; }
};

//==============================================================================
/*  Attack is linear so it can be genuinely instant; decay and release are
    exponential, because a pad whose tail falls off a cliff is a pad you can
    hear stopping. */
struct ADSR
{
    enum St { Idle, Att, Dec, Sus, Rel };
    St st = Idle;
    float v = 0.0f, ka = 0.01f, kd = 0.01f, sl = 0.7f, kr = 0.01f;

    void set (float A, float D, float S, float R, double sr)
    {
        ka = 1.0f / std::max (1.0f, A * (float) sr);
        kd = 1.0f - std::exp (-1.0f / std::max (1.0f, D * 0.35f * (float) sr));
        sl = clamp01 (S);
        kr = 1.0f - std::exp (-1.0f / std::max (1.0f, R * 0.35f * (float) sr));
    }
    void gate (bool on) { st = on ? Att : (v > 0.0f ? Rel : Idle); }
    float tick()
    {
        switch (st)
        {
            case Att: v += ka; if (v >= 1.0f) { v = 1.0f; st = Dec; } break;
            case Dec: v += (sl - v) * kd; if (std::abs (v - sl) < 1.0e-4f) { v = sl; st = Sus; } break;
            case Sus: v = sl; break;
            case Rel: v += (0.0f - v) * kr; if (v < 1.0e-5f) { v = 0.0f; st = Idle; } break;
            default:  v = 0.0f; break;
        }
        return v;
    }
    bool active() const { return st != Idle; }
    void clear() { st = Idle; v = 0.0f; }
};

//==============================================================================
struct Delay
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void init (int n)
    {
        int p = 8;
        while (p < n) p <<= 1;
        buf.assign ((size_t) p, 0.0f);
        mask = p - 1;
        w = 0;
    }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float d) const
    {
        const float len = (float) (mask + 1);
        float rp = (float) w - std::min (std::max (d, 1.0f), len - 2.0f);
        while (rp < 0.0f) rp += len;
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        return buf[(size_t) (i0 & mask)] * (1.0f - f) + buf[(size_t) ((i0 + 1) & mask)] * f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
};

/*  Reads its own delay line at twice the rate through two crossfaded windows,
    which is an octave up with a soft seam rather than a chirp. */
struct OctaveUp
{
    Delay d;
    float ph = 0.0f, win = 2400.0f;

    void prepare (double sr)
    {
        win = (float) sr * 0.05f;
        d.init ((int) win * 2 + 8);
        ph = 0.0f;
    }
    float tick (float x)
    {
        d.push (x);
        ph += 1.0f;
        if (ph >= win) ph -= win;
        float p2 = ph + win * 0.5f;
        if (p2 >= win) p2 -= win;
        const float w1 = 0.5f - 0.5f * std::cos (TWO_PI_F * ph / win);
        const float w2 = 0.5f - 0.5f * std::cos (TWO_PI_F * p2 / win);
        return d.read (win - ph) * w1 + d.read (win - p2) * w2;
    }
    void clear() { d.clear(); ph = 0.0f; }
};

//==============================================================================
/*  Eight-line feedback delay network behind four diffusing allpasses. The
    Hadamard mix is cheap and keeps the tail from ever settling into a comb. */
struct Reverb
{
    static constexpr int N = 8;
    Delay line[N], ap[4];
    OnePole damp[N];
    float apLen[4] { 142.0f, 379.0f, 107.0f, 277.0f };
    float baseLen[N] { 1116.0f, 1188.0f, 1277.0f, 1356.0f, 1422.0f, 1491.0f, 1557.0f, 1617.0f };
    float len[N] {};
    float fb = 0.72f, srScale = 1.0f;
    OctaveUp shimmer;
    OnePole shHp;
    bool useShimmer = false;

    void prepare (double sr, bool withShimmer)
    {
        srScale = (float) sr / 44100.0f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseLen[i] * srScale;
            line[i].init ((int) (len[i] * 3.0f) + 64);
            damp[i].setHz (6000.0f, sr);
        }
        for (int i = 0; i < 4; ++i) ap[i].init ((int) (apLen[i] * srScale) + 64);
        useShimmer = withShimmer;
        if (withShimmer) { shimmer.prepare (sr); shHp.setHz (300.0f, sr); }
    }

    void setSize (float size01, float decay01, float dampHz, double sr)
    {
        const float sc = lerp (0.55f, 1.85f, clamp01 (size01)) * srScale;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseLen[i] * sc;
            damp[i].setHz (dampHz, sr);
        }
        fb = lerp (0.62f, 0.945f, clamp01 (decay01));
    }

    void clear()
    {
        for (auto& l : line) l.clear();
        for (auto& a : ap)   a.clear();
        for (auto& d : damp) d.clear();
        shimmer.clear();
        shHp.clear();
    }

    /*  in is mono; out is stereo. shimmerAmt folds an octave-up copy of the
        tail back into the input, which is where the sheen comes from. */
    void tick (float in, float shimmerAmt, float& outL, float& outR)
    {
        float x = in;
        for (int i = 0; i < 4; ++i)
        {
            const float dl = ap[i].read (apLen[i] * srScale);
            const float v  = x - 0.62f * dl;
            ap[i].push (v);
            x = dl + 0.62f * v;
        }

        float y[N];
        for (int i = 0; i < N; ++i) y[i] = damp[i].lp (line[i].read (len[i]));

        // Hadamard-8, in place, three butterfly stages
        float t[N];
        for (int i = 0; i < 4; ++i) { t[i] = y[i] + y[i + 4]; t[i + 4] = y[i] - y[i + 4]; }
        for (int b = 0; b < 8; b += 4)
            for (int i = 0; i < 2; ++i)
            { const float a = t[b + i], c = t[b + i + 2]; t[b + i] = a + c; t[b + i + 2] = a - c; }
        for (int b = 0; b < 8; b += 2)
        { const float a = t[b], c = t[b + 1]; t[b] = a + c; t[b + 1] = a - c; }
        for (int i = 0; i < N; ++i) t[i] *= 0.35355339f;      // 1/sqrt(8)

        float sh = 0.0f;
        if (useShimmer && shimmerAmt > 0.001f)
            sh = shHp.hp (shimmer.tick ((y[0] + y[3]) * 0.5f)) * shimmerAmt;

        for (int i = 0; i < N; ++i)
            line[i].push (x + sh + fb * t[i]);

        outL = (y[0] + y[2] + y[4] + y[6]) * 0.34f;
        outR = (y[1] + y[3] + y[5] + y[7]) * 0.34f;
    }
};

//==============================================================================
/*  Three modulated taps, spread 120 degrees apart. This is most of what makes
    a CS-80 sound like a CS-80. */
struct Ensemble
{
    Delay dl, dr;
    float ph = 0.0f, inc = 0.0f, base = 0.0f, sr = 44100.0f;

    void prepare (double s)
    {
        sr = (float) s;
        dl.init ((int) (sr * 0.05f));
        dr.init ((int) (sr * 0.05f));
        base = sr * 0.0085f;
        inc  = 0.42f / sr;
        ph   = 0.0f;
    }
    void clear() { dl.clear(); dr.clear(); ph = 0.0f; }

    void tick (float inL, float inR, float depth, float& outL, float& outR)
    {
        ph += inc;
        if (ph >= 1.0f) ph -= 1.0f;
        const float d = base * 0.75f * depth;
        const float m1 = std::sin (TWO_PI_F * ph);
        const float m2 = std::sin (TWO_PI_F * (ph + 0.3333f));
        const float m3 = std::sin (TWO_PI_F * (ph + 0.6667f));
        dl.push (inL);
        dr.push (inR);
        const float l = dl.read (base + d * m1) * 0.6f + dr.read (base + d * m2) * 0.4f;
        const float r = dr.read (base + d * m3) * 0.6f + dl.read (base + d * m1) * 0.4f;
        outL = lerp (inL, l, clamp01 (depth) * 0.9f);
        outR = lerp (inR, r, clamp01 (depth) * 0.9f);
    }
};

//==============================================================================
/*  A ceiling that cannot be crossed, whatever happens upstream. The limiter
    below does the musical work but it is a feedback design with no
    lookahead, so the first millisecond of a percussive transient gets past
    it — measured at 1.75 on random patches before this was here. Transparent
    below 70 % of the ceiling, and asymptotic to it above. */
inline float ceilSoft (float x, float c)
{
    const float a = std::abs (x);
    const float knee = c * 0.7f;
    if (a <= knee) return x;
    const float y = knee + (c - knee) * std::tanh ((a - knee) / (c - knee));
    return x < 0.0f ? -y : y;
}

struct Limiter
{
    float env = 0.0f, gr = 1.0f, atk = 0.4f, rel = 0.0002f;
    void prepare (double sr)
    {
        atk = 1.0f - std::exp (-1.0f / (0.0004f * (float) sr));
        rel = 1.0f - std::exp (-1.0f / (0.25f   * (float) sr));
    }
    void tick (float& l, float& r, float ceiling)
    {
        const float peak = std::max (std::abs (l), std::abs (r));
        env += (peak - env) * (peak > env ? atk : rel);
        const float want = env > ceiling ? ceiling / std::max (env, 1.0e-6f) : 1.0f;
        gr += (want - gr) * (want < gr ? atk : rel);
        l *= gr;
        r *= gr;
    }
    void clear() { env = 0.0f; gr = 1.0f; }
};

//==============================================================================
//==============================================================================
/*  The mood organ.

    One number between 0 and 999 is the entire patch. The seed drives a
    xorshift with no other input — no clock, no address, no floating-point
    accumulation — so seed 481 is the same instrument today, tomorrow, on
    another machine and in a project opened next year. That is the whole
    point of it: a mood you can write down.

    The line of text is generated from the same seed by a small grammar, so
    every one of the thousand has its own description. Seventeen of them are
    written by hand instead — five of those are Philip K. Dick's own, and the
    manual says which. */
struct MoodValue { const char* id; float v; };

constexpr int NUM_MOODS = 1000;

std::vector<MoodValue> moodPatch (int seed);
std::string            moodLine  (int seed);
bool                   moodIsWritten (int seed);   // hand-written, not generated

//==============================================================================
struct Params
{
    // ---- global
    float master = 0.64f, tune = 0.5f, limiter = 1.0f;

    /*  Anything the panel shows as a list or a count is carried here as its
        raw value, not as 0..1 — chord index, semitones, step count, the
        six-bit seed. Everything else is a normalised knob. */

    // ---- LOS ANGELES
    float laOn = 1.0f, laLvl = 0.50f, laRoot = 0.0f, laChord = 0.0f, laGate = 0.0f;
    float laSprawl = 0.45f, laSmog = 0.4f, laKipple = 0.3f, laRain = 0.35f;
    float laNeon = 0.35f, laDecay = 0.7f, laDrift = 0.4f, laSub = 0.5f;

    // ---- DECKARD
    float dkOn = 1.0f, dkLvl = 0.55f, dkWave = 0.0f;
    float dkBright = 0.5f, dkRes = 0.32f, dkFenv = 0.55f;
    float dkAtk = 0.28f, dkDec = 0.4f, dkSus = 0.75f, dkRel = 0.55f;
    float dkRing = 0.0f, dkEns = 0.7f, dkGlide = 0.0f, dkVib = 0.25f;
    float dkSpace = 0.55f, dkOct = 0.0f, dkDetune = 0.3f;

    // ---- REPLICANT
    float rpOn = 0.0f, rpLvl = 0.5f, rpRate = 3.0f, rpNexus = 6.0f, rpSteps = 16.0f;
    float rpMetal = 0.45f, rpDecay = 0.35f, rpGlitch = 0.2f, rpMenace = 0.3f;
    float rpSpread = 0.6f, rpSpace = 0.4f, rpGate = 0.0f, rpOct = 0.0f;

    /*  Carried so the panel and the host see it like any other value.
        The DSP does not read it: the mood is a way of arriving at a patch,
        not a thing the patch contains. */
    float mood = -1.0f;

    // ---- from the host
    double bpm = 120.0;
    bool   playing = false;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* left, float* right, int n);

    // Brokild World FX world-mod bus (plain stores, any thread). A NEUTRAL
    // bus (0,0,0,0,0,1) is skipped entirely — bit-identical by memcmp.
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

    void noteOn  (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();

    Params p;

    // ---- meters, read by the message thread
    float layerRms[NUM_LAYERS] { 0, 0, 0 };
    float outRms = 0.0f;
    float limitGr = 1.0f;
    int   rpStepNow = 0;
    float rpHit = 0.0f;                 // decays after each step, for the panel
    float laDriftNow = 0.5f;
    float dkVoicesNow = 0.0f;

    static const char* chordName (int i);
    static const char* waveName  (int i);
    static const char* rateName  (int i);
    static float       rateBeats (int i);

    /*  What the sequence actually is, so the panel can show it rather than
        guess. 0 rest, 1 note, 2 accented note. Read from the message thread
        while the audio thread may be rebuilding it — a torn read shows one
        wrong lamp for one frame, which is the right price for not locking. */
    int patternStep  (int i) const;
    int patternPitch (int i) const;

private:
    // ---------------------------------------------------------------- LA
    struct LA
    {
        Osc  osc[LA_STACK];
        float driftPh[LA_STACK] {};
        float driftInc[LA_STACK] {};
        Osc  sub;
        SVF  smog[2];
        OnePole rumble;
        SVF  rainBp[3];
        float rainPh[3] {};
        OnePole rainLp;
        struct Grain { bool on = false; float amp = 0, dec = 0, pan = 0; Osc o; SVF bp; };
        std::array<Grain, LA_GRAINS> grain;
        float grainClock = 0.0f;
        ADSR env;
        bool open = false;              // gate state, so KEYED does not retrigger every block
        Reverb verb;
        Rng rng;
        float combPh = 0.0f;
        Delay comb;
        OnePole verbHp;
    } la;

    // ------------------------------------------------------------ DECKARD
    struct Voice
    {
        bool  used = false;
        int   note = -1;
        float vel = 0.0f;
        float hz = 0.0f, hzTarget = 0.0f;
        Osc   a, b, sub;
        Ladder filt;
        ADSR  amp, fenv;
        float pan = 0.0f;
        uint32_t age = 0;
        float wmGate = 0.0f;         // smoothed gate for the world-mod sag
    };
    struct DK
    {
        std::array<Voice, MAX_POLY> v;
        Ensemble ens;
        Reverb verb;
        float vibPh = 0.0f, vibRamp = 0.0f;
        uint32_t counter = 0;
        Rng rng;
    } dk;

    // ---------------------------------------------------------- REPLICANT
    struct RP
    {
        float phase = 0.0f;             // 0..1 within the current step
        int   step = 0;
        Osc   car, mod;
        ADSR  env;
        SVF   bp;
        OnePole hold;
        float holdVal = 0.0f, crushPh = 0.0f;
        Osc   heart;
        ADSR  heartEnv;
        Reverb verb;
        Delay stutter;
        float stutterPh = 0.0f;
        bool  stutterOn = false;
        Rng   rng;
        float pan = 0.0f, curHz = 220.0f, curAmp = 0.0f;
        uint8_t gateOf[RP_STEPS] {};
        int8_t  pitchOf[RP_STEPS] {};
        uint8_t accentOf[RP_STEPS] {};
        int     builtFor = -1;
    } rp;

    void buildPattern (int seed);
    void renderLA (float* l, float* r, int n);
    void renderDK (float* l, float* r, int n);
    void renderRP (float* l, float* r, int n);

    double sr = 44100.0;
    Limiter lim;
    OnePole dcL, dcR;
    float rmsAcc[NUM_LAYERS] { 0, 0, 0 }, rmsOutAcc = 0.0f;

    // held notes, shared by every layer that wants them
    std::array<int, 16> held {};
    int heldCount = 0;
    int lowestHeld = -1;
    uint32_t voiceAge = 0;
    float tuneMul = 1.0f;
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFmul = 1;
    bool  wmActive = false;
    double wmT = 0;                  // seconds, for the trem phases
    std::vector<float> tmpL, tmpR;
};

} // namespace br
