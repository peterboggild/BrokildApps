#pragma once

/*  FULL METAL RACKET — the engine.

    Twelve analogue drum voices built from six shared mechanisms, not twelve
    unrelated circuits (FULL-METAL-RACKET-DESIGN.md §2):

      A  nonlinear resonator     amplitude-dependent damping
      B  tension                 amplitude bends the frequency (hard hit
                                 starts sharp and falls to pitch)
      C  coupled modes           inharmonic ratios exchanging energy
      D  metal core              six squares at awkward ratios
      E  multi-band decay        a cymbal's bands do not decay together
      F  excitation              the trigger IS the attack

    JUCE-free on purpose so the offline bench renders the identical audio.
    Everything is float except resonator state, which is double — these run
    close to self-oscillation and a float integrator audibly drifts. */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>
#include <atomic>
#include <vector>

#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif

namespace fmr
{

//==============================================================================
inline float clamp01 (float v)                  { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float clampf  (float v, float a, float b){ return v < a ? a : (v > b ? b : v); }
inline float lerpf   (float a, float b, float t){ return a + (b - a) * t; }
/*  Exponential map: the only honest way to put a frequency or a time on a
    linear knob. Every KP_HZ / KP_MS parameter goes through this. */
inline float xmap (float v, float a, float b)   { return a * std::pow (b / a, clamp01 (v)); }

// A soft ceiling: transparent below 0.7, asymptotic above. Applied
// unconditionally on the master (the Blade Ruiner lesson — a limiter switch
// turns off gain reduction, never the ceiling).
inline float ceilSoft (float x)
{
    const float a = std::fabs (x);
    if (a <= 0.7f) return x;
    const float over = a - 0.7f;
    const float y = 0.7f + 0.3f * (over / (over + 0.3f));
    return x < 0.0f ? -y : y;
}

//==============================================================================
/*  Deterministic noise. One stream per voice so a render is reproducible
    sample for sample, which the bench depends on. */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 0x9E3779B9u; }
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()  { return (float) (next() >> 8) * (1.0f / 16777216.0f); }        // 0..1
    inline float bi()   { return uni() * 2.0f - 1.0f; }                                  // -1..1
};

//==============================================================================
/*  BLOCK A — the nonlinear resonator.

    A TPT state-variable filter run as a resonator. Two things make it a drum
    rather than a sine with an envelope:

      * the damping term is NONLINEAR — a force opposing motion that grows
        with the square of velocity, injected at the input rather than baked
        into the coefficients (so the coefficients stay fixed inside the
        inner loop, and the term is unconditionally dissipative hence
        unconditionally stable). This is what a transistor does to a twin-T
        pushed toward self-oscillation: the decay starts fast at high level
        and then hangs on, and it does so differently at different levels.

      * setF() is called at control rate with a frequency that already
        carries the tension term, so pitch and amplitude are coupled.

    Q is the DECAY control on the ping models: no VCA at all, exactly as the
    808's bridged-T does it. That is why they breathe. */
struct Reso
{
    double ic1 = 0.0, ic2 = 0.0;
    float  a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, kd = 1.0f;
    float  lastBp = 0.0f, lastLp = 0.0f;

    void reset() { ic1 = ic2 = 0.0; lastBp = lastLp = 0.0f; }

    // f in Hz, q as a Q factor, fs the (oversampled) rate
    void setF (float f, float q, float fs)
    {
        f = clampf (f, 8.0f, fs * 0.47f);
        const float g = (float) std::tan (M_PI * (double) f / (double) fs);
        kd = 1.0f / clampf (q, 0.4f, 4000.0f);
        a1 = 1.0f / (1.0f + g * (g + kd));
        a2 = g * a1;
        a3 = g * a2;
    }

    /*  nl = nonlinear damping coefficient. The term is a drag force with the
        sign of the velocity, so it can only ever remove energy — that is what
        makes it unconditionally stable.

        It SATURATES rather than growing without bound: a transistor holding a
        twin-T back is a saturating element, not a square law. Left unbounded,
        the drag squashed a full-velocity hit almost down to a quarter-velocity
        one, and accents stopped reading. */
    inline float tick (float in, float nl)
    {
        const float av = std::fabs (lastBp);
        const float drag = nl > 0.0f ? nl * lastBp * (av / (1.0f + av * 1.8f)) : 0.0f;
        const float v3 = (in - drag) - (float) ic2;
        const float v1 = a1 * (float) ic1 + a2 * v3;
        const float v2 = (float) ic2 + a2 * (float) ic1 + a3 * v3;
        ic1 = 2.0 * (double) v1 - ic1;
        ic2 = 2.0 * (double) v2 - ic2;
        lastBp = v1;      // bandpass — velocity
        lastLp = v2;      // lowpass  — displacement
        return v1;
    }
};

//==============================================================================
/*  A plain RBJ biquad, for the tone shaping and the band splits. Direct form
    I in double state — these sit in decaying signal paths where denormals
    would otherwise cost more than the state does. */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0; }

    inline float tick (float in)
    {
        const double y = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = in; y2 = y1; y1 = y;
        return (float) y;
    }

    void setLP (float f, float q, float fs) { set (0, f, q, 0.0f, fs); }
    void setHP (float f, float q, float fs) { set (1, f, q, 0.0f, fs); }
    void setBP (float f, float q, float fs) { set (2, f, q, 0.0f, fs); }
    void setPk (float f, float q, float gDb, float fs) { set (3, f, q, gDb, fs); }

    void set (int type, float f, float q, float gDb, float fs)
    {
        f = clampf (f, 10.0f, fs * 0.47f);
        q = clampf (q, 0.05f, 40.0f);
        const double w = 2.0 * M_PI * (double) f / (double) fs;
        const double cs = std::cos (w), sn = std::sin (w);
        const double al = sn / (2.0 * (double) q);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        if (type == 0)       { B0 = (1 - cs) / 2; B1 = 1 - cs;  B2 = B0;      A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; }
        else if (type == 1)  { B0 = (1 + cs) / 2; B1 = -(1 + cs); B2 = B0;    A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; }
        else if (type == 2)  { B0 = al;           B1 = 0;       B2 = -al;     A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; }
        else                 { const double A = std::pow (10.0, (double) gDb / 40.0);
                               B0 = 1 + al * A;   B1 = -2 * cs; B2 = 1 - al * A;
                               A0 = 1 + al / A;   A1 = -2 * cs; A2 = 1 - al / A; }

        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};

//==============================================================================
/*  BLOCK D — one oscillator of the metal core. polyBLEP square: the cluster
    runs at a few hundred Hz but everything above it is what you hear, so an
    aliased square would fold audible grit straight into the band the hats
    live in. */
struct Blep
{
    double ph = 0.0, inc = 0.0;

    void reset (double p = 0.0) { ph = p; }
    void setF (float f, float fs) { inc = (double) f / (double) fs; }

    static inline double poly (double t, double dt)
    {
        if (t < dt)          { t /= dt;      return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)    { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    inline float square()
    {
        ph += inc;
        if (ph >= 1.0) ph -= 1.0;
        double v = ph < 0.5 ? 1.0 : -1.0;
        v += poly (ph, inc);
        double t2 = ph + 0.5; if (t2 >= 1.0) t2 -= 1.0;
        v -= poly (t2, inc);
        return (float) v;
    }
};

//==============================================================================
/*  An analogue-style envelope: a near-instant charge and a capacitor
    discharge. Attack is a linear ramp because that is what a charge through
    a small series resistance looks like at this timescale; decay is a true
    exponential. Crashes want an attack above zero (the swell). */
struct Env
{
    float v = 0.0f, target = 0.0f, dec = 0.999f;
    int   atk = 0, atkLeft = 0;

    void reset() { v = 0.0f; atkLeft = 0; }

    void trigger (float level, float attackSamples, float decaySamples)
    {
        target  = level;
        atk     = (int) (attackSamples < 1.0f ? 1.0f : attackSamples);
        atkLeft = atk;
        dec     = std::exp (-1.0f / (decaySamples < 4.0f ? 4.0f : decaySamples));
        if (atk <= 1) { v = level; atkLeft = 0; }
    }

    inline float tick()
    {
        if (atkLeft > 0) { v += (target - v) / (float) atkLeft; --atkLeft; }
        else             { v *= dec; }
        return v;
    }
};

//==============================================================================
/*  Halfband decimator, designed at init. Used in cascade: 4x -> 2x -> 1x.
    Only computes an output when one is wanted, so the cost is per OUTPUT
    sample, not per oversampled sample. */
struct Halfband
{
    static constexpr int N = 43;
    float h[N] = { 0.0f };
    float z[N] = { 0.0f };
    int   pos = 0;

    void design()
    {
        const int M = (N - 1) / 2;
        double sum = 0.0;
        for (int k = 0; k < N; ++k)
        {
            const int d = k - M;
            const double s = (d == 0) ? 0.5 : std::sin (M_PI * 0.5 * d) / (M_PI * d);
            const double w = 0.42 - 0.5 * std::cos (2.0 * M_PI * k / (N - 1))
                                  + 0.08 * std::cos (4.0 * M_PI * k / (N - 1));
            h[k] = (float) (s * w);
            sum += h[k];
        }
        for (int k = 0; k < N; ++k) h[k] = (float) (h[k] / sum);
    }

    void reset() { std::memset (z, 0, sizeof (z)); pos = 0; }

    inline void push (float x) { z[pos] = x; if (++pos >= N) pos = 0; }

    inline float read() const
    {
        float acc = 0.0f;
        int p = pos - 1; if (p < 0) p += N;
        for (int k = 0; k < N; ++k)
        {
            acc += h[k] * z[p];
            if (--p < 0) p += N;
        }
        return acc;
    }
};

//==============================================================================
constexpr int NCH    = 12;      // channels
constexpr int VOICES = 4;       // per channel — a hat roll overlaps itself
constexpr int NRES   = 3;       // coupled modes per voice
constexpr int NMETAL = 6;       // the metal cluster
constexpr int NBAND  = 3;       // block E
constexpr int NWIRE  = 5;       // the snare bed
constexpr int CTRL   = 8;       // control-rate block, in oversampled samples

enum Family { FAM_KICK = 0, FAM_SNARE, FAM_TOM, FAM_PERC, FAM_METAL };

// ---- the per-channel parameter slots ---------------------------------------
enum CP
{
    CP_MODEL = 0,   // list, family-dependent
    CP_TUNE,        // Hz, family range
    CP_DECAY,       // ms, family range
    CP_TONE,        // brightness / filter
    CP_SNAP,        // kick: click · snare: noise · tom: skin · metal: HPF · perc: click
    CP_BEND,        // kick: pitch drop · snare: wires · tom: tension · metal: spread · perc: sweep
    CP_DRIVE,
    CP_LEVEL,
    CP_PAN,
    CP_MUTE,
    CP_REBOUND,     // stick bounce: flam -> drag -> buzz roll
    CP_KEY,         // chromatic play, decay scaling with pitch
    NCP
};

enum GP
{
    GP_VOLUME = 0,
    GP_OS,          // 0 = 1x, 1 = 2x, 2 = 4x
    GP_SAG,         // the shared power rail
    GP_AGE,         // component tolerances
    GP_KITTUNE,     // tunes the three toms together
    GP_HATLINK,     // OH follows CH
    GP_BLEED,       // THE WEB
    GP_BODY,        // the shared shell
    GP_MORPH,       // kit A <-> kit B
    GP_SEQ,         // the sequencer runs
    GP_SWING,       // global swing, lanes offset from it
    GP_FEEL,        // THE HAND: push and pull per subdivision
    GP_GRIP,        // THE HAND: velocity carried between hits
    GP_TEMPO,       // internal clock, used when the host gives none
    GP_PUNCH,       // transient shaper, keyed to each voice's own trigger
    NGP
};

struct Params
{
    float ch[NCH][NCP];
    float g[NGP];
    double bpm = 120.0;       // not a table param
    Params();
};

//==============================================================================
/*  The sequencer's data. Not host parameters — twelve lanes of thirty-two
    steps across sixteen patterns is far too much to put in an APVTS, so it
    lives in the same kind of opaque blob the BWFX rack uses, and only the
    steps that are ON are ever written out. */
constexpr int NSTEP = 32;
constexpr int NPAT  = 16;

struct Step
{
    uint8_t on = 0;
    uint8_t vel = 100;        // 0..127
    uint8_t prob = 100;       // 0..100 %
    uint8_t ratchet = 1;      // 1..8 hits on this step
    uint8_t cond = 0;         // 0 none, 1 = 1:2, 2 = 1:3, 3 = 1:4, 4 = FILL, 5 = NOT FILL, 6 = PREV
    int8_t  micro = 0;        // -50..+50 % of a step
};

struct Lane
{
    uint8_t len = 16;         // 1..32 — per lane, so polymeter is free
    uint8_t div = 1;          // 0 = 32nd, 1 = 16th, 2 = 8th, 3 = quarter
    uint8_t dir = 0;          // 0 fwd, 1 rev, 2 pendulum, 3 random
    int8_t  swing = 0;        // added to the global swing
    uint8_t mute = 0;
    Step step[NSTEP];
};

struct Pattern { Lane lane[NCH]; };

//==============================================================================
/*  The parameter table — one source of truth, built once in a loop rather
    than typed out 128 times. The APVTS layout, processBlock, the kits, the
    page and the bench all walk it, so the read-order class of bug (which
    needed a whole checker in Blade Ruiner) cannot be expressed here. */
enum PKind { KP_PCT = 0, KP_SW, KP_HZ, KP_MS, KP_LIST, KP_BIPOL, KP_VOL, KP_INT, KP_SEMI };

struct PSpec
{
    const char* id;
    const char* name;
    float def;
    int   kind;
    float lo, hi;      // formatting range; for KP_LIST hi is the max index
    int   chan;        // -1 for a global
    int   slot;
};

int          numParams();
const PSpec& paramSpec (int i);
int          paramIndex (const char* id);
float        paramMax (const PSpec& s);
const char* const* listNames (const char* id, int& count);

/*  Whether a parameter is part of a KIT — so it morphs, as opposed to
    belonging to the performance. */
bool morphable (const PSpec& s);

inline float& pvalue (Params& p, const PSpec& s)
{
    return s.chan >= 0 ? p.ch[s.chan][s.slot] : p.g[s.slot];
}

const char* channelId   (int c);
const char* channelName (int c);
int         channelFamily (int c);
const char* familyName  (int f);
int         defaultNote (int c);        // the GM-ish map

//  The kit library — two hundred seeds, in Kits.cpp. Category is decided
//  first from a bijective permutation, THEN parameters are generated to fit,
//  so a kit's name always describes its sound by construction.
int         numKits();
const char* kitName (int i);
void        applyKit (int i, Params& p);
int         numSeeds();
int         numSeedCategories();
const char* seedCategoryName (int c);
int         seedCategory (int seed);
const char* seedName (int seed);
void        applySeed (int seed, Params& p);

//==============================================================================
/*  One drum voice. Every family renders from the same blocks; `fam` picks
    which of them are wired together. Four per channel — a fast hat roll
    overlaps itself and cutting it off is the sound of a cheap machine. */
struct Voice
{
    int   fam = FAM_KICK;
    int   model = 0;
    bool  active = false;
    float vel = 1.0f;
    int   age = 0;                 // samples since trigger, for the excitation

    Reso  res[NRES];
    Blep  met[NMETAL];
    /*  noiseF is separate from tone ON PURPOSE. Ticking one biquad twice in a
        sample with two different inputs is not "using the filter twice" — it
        is a different, undesigned recursion, and it left the toms with a
        tail that outlived their own decay by seconds. One filter, one
        signal, one tick. */
    Biquad band[NBAND], tone, noiseF, hp, clickF;
    Biquad wire[NWIRE];
    Env    amp, clickE, noiseE, bandE[NBAND], wireE;
    Rng    rng;

    float  f0 = 100.0f;            // base frequency this hit
    float  keySemis = 0.0f;        // KEY MODE: how far from the reference note
    float  ratio[NRES] = { 1.0f, 1.6f, 2.1f };
    float  resGain[NRES] = { 1.0f, 0.0f, 0.0f };
    float  q = 20.0f;
    float  nl = 0.0f;              // nonlinear damping, derived from Q (see nlFor)
    float  shape = 0.0f;           // waveshaper drive, for the models with no resonator
    float  tension = 0.0f;
    float  couple = 0.0f;
    float  pitchEnv = 0.0f, pitchDec = 0.999f, pitchV = 0.0f;
    float  excLeft = 0.0f, excDec = 0.5f, excAmp = 0.0f;
    float  noiseAmt = 0.0f, clickAmt = 0.0f, wireAmt = 0.0f, wireThresh = 0.0f;
    float  drive = 0.0f;
    float  famGain = 1.0f;         // level match across the twelve, measured
    float  bandMix[NBAND] = { 1.0f, 1.0f, 1.0f };
    float  clapLeft = 0.0f; int clapStage = 0; float clapGap = 0.0f;
    float  peak = 0.0f;            // for tension and culling
    int    quiet = 0;

    void reset();
};

//==============================================================================
struct Engine
{
    Engine() { setWorldMod (0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); }

    Params p;

    void  prepare (double sampleRate, int maxBlock);
    void  reset();
    /*  aux, when given, is NCH mono buffers — each channel's own contribution
        before panning, decimated the same way the main mix is. Beatmakers ask
        for separate outs first and it decides the bus architecture, so it is
        here from the start rather than bolted on. Costs nothing when null. */
    void  process (float* L, float* R, int n, float* const* aux = nullptr);

    /*  allowRebound is false for the bounces REBOUND itself schedules: a
        bounce that could bounce is a geometric series, and the first version
        of this turned twelve hits into a hundred and fifty. */
    void  trigger (int chan, float velocity, float keySemis = 0.0f, bool allowRebound = true);
    void  noteOn  (int note, float velocity);
    void  noteOff (int note) { (void) note; }      // drums do not hold
    void  allNotesOff();

    // the BWFX world-mod bus
    void setWorldMod (float detCents, float panSpread, float tremDepth,
                      float tremRate, float pitchSag, float filterMul);

    // for the panel's meters and trigger lamps
    float channelLevel (int c) const { return meter[(size_t) c]; }

    //  ---- transport and the sequencer ------------------------------------
    /*  The present flag answers "does this host have a transport at all", which is a
        different question from "is it rolling" — the standalone and a host
        that reports no position must not look alike. */
    void setTransport (double bpm, double ppq, bool playing, bool present = true);

    //  THE GATE (buglist 1). seq is the intent; this is the session-local
    //  latch that lets the machine run when no host transport is rolling.
    void setFreeRun (bool on) { freeRun = on; }
    bool isFreeRunning() const { return freeRun; }
    bool hostHasTransport() const { return hostPresent; }
    bool isRunning() const { return p.g[GP_SEQ] >= 0.5f && (playing || freeRun); }
    bool isPlayingHost() const { return playing; }
    int  seqStep() const { return uiStep; }          // for the panel's running light
    /*  Each lane's OWN position. A single global counter cannot describe a
        machine whose lanes have different lengths and divisions — with LAST
        at 11 the light ran on to 32 while the lane had already looped. */
    int  seqStepFor (int c) const { return uiLaneStep[(size_t) (c < 0 ? 0 : (c >= NCH ? NCH - 1 : c))]; }

    /*  Put the free-running clock back to the top. With a host rolling the
        bar governs and this changes nothing; without one it is the difference
        between a pattern that starts at step one and a pattern that starts
        wherever the last one happened to leave off. */
    void restartClock() { ppqFree = 0.0; uiLaneStep.fill (-1); nHits = 0; for (auto& l : lastVel) l = 0.0f; }

    //  RECORD: what you play is written into the current pattern
    std::atomic<bool> recArm { false };
    Pattern pat[NPAT];
    int curPat = 0;

    //  ---- MORPH: two whole kits, blended -------------------------------
    Params kitA, kitB;
    bool   haveA = false, haveB = false;
    /*  Applied to a COPY of the parameters each block, never written back:
        morph is a performance control, and a fader that silently rewrote a
        hundred knobs would make its own automation unusable. */
    void applyMorph (Params& dst) const;

private:
    double fs = 48000.0, osFs = 48000.0;
    int    osFactor = 1, osSel = -1;
    int    maxBlockSize = 512;

    std::array<std::array<Voice, VOICES>, NCH> voices;
    std::array<int, NCH>   rr { };                 // round robin
    std::array<float, NCH> meter { };
    std::array<float, NCH> tol { };                // AGE tolerances
    std::array<float, NCH> tolDrift { };
    std::array<Reso,  NCH> symp;                   // THE WEB: one sympathetic resonator per channel
    std::array<float, NCH> sympExc { };            // its excitation, a short pulse not one sample
    float sympDec = 0.99f;

    Reso   body[2];                                // the shared shell
    Biquad bodyTone;

    Halfband dec1L, dec1R, dec2L, dec2R;
    std::vector<Halfband> auxA, auxB;              // per-channel, built on demand

    float  rail = 1.0f, railEnv = 0.0f;
    float  masterDcL = 0.0f, masterDcR = 0.0f, masterDcXL = 0.0f, masterDcXR = 0.0f;
    Rng    grng;
    int    driftTick = 0;

    /*  The world-mod bus, copied once per block. NOTE the constructor: this
        array is value-initialised to zeros, but slot 5 is filterMul, whose
        neutral value is 1.0 — a zeroed bus is not a neutral bus, it is a
        request to retune every resonator to 0 Hz. That cost a debugging
        round; the whole machine rang at the 8 Hz clamp instead of at its
        tuning, and looked for all the world like a broken resonator. */
    std::array<std::atomic<float>, 6> wmIn { };
    float wmDet = 0.0f, wmPan = 0.0f, wmTremD = 0.0f, wmTremR = 4.0f, wmSag = 0.0f, wmFilt = 1.0f;
    bool  wmActive = false;
    float wmPhase = 0.0f;

    void  setupVoice (Voice& v, int c, float velocity, float keySemis);
    float renderVoice (Voice& v, int c);
    void  updateOs();

    /*  Scheduled hits, in oversampled samples from the start of the block.
        REBOUND fills this with a stick's decaying bounce, and the sequencer
        fills it with the step's own sample-accurate position. */
    struct Hit { int chan = 0; float vel = 0.0f; float key = 0.0f; int at = 0; bool rb = true; };
    static constexpr int MAXHITS = 512;
    std::array<Hit, MAXHITS> hits { };
    int nHits = 0;
    void scheduleHit (int chan, float vel, float key, int atSamples, bool allowRebound = true);
    void fireDue (int sampleInBlock);

    // transport, for the sequencer
    double bpmNow = 120.0, ppqNow = 0.0, ppqFree = 0.0;
    bool   playing = false;
    /*  Separate from the playing flag on purpose. A host has a tempo whether or not
        its transport is rolling, and the sequencer should use it either way —
        conflating the two made the machine run at its internal tempo until
        you pressed play. */
    bool   hostBpm = false;
    bool   hostPresent = false;      // the host has a transport at all
    bool   freeRun = false;          // session-local latch, never a parameter
    int    uiStep = -1;
    std::array<int, NCH> uiLaneStep { };
    std::array<double, NCH> uiLaneFrac { };     // for quantising a recorded hit
    float  lastVel[NCH] { };          // THE HAND: what this limb hit last
    void   runSequencer (int nSamples);
};

} // namespace fmr
