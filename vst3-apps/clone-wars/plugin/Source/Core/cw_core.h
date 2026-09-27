#pragma once

// Clone Wars — the engine core.
//
// Deliberately JUCE-free: plain C++17, no allocation after prepare(), no
// locks, no I/O. The JUCE processor wraps it; test/render_test.cpp compiles
// it headless with g++ and renders WAVs so the sound can be validated on
// any machine before a Windows plugin build.
//
// Real-time rules (devkit): process() never allocates, locks or logs.
// All parameter entry points are plain float stores read at sub-block rate.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace cw
{

static constexpr int kVoices     = 16;
static constexpr int kArmySize   = 8;
static constexpr int kNoteSlots  = 3;    // legacy: vfNote's range, kept for state compat
static constexpr int kMaxHeld    = 8;    // held notes tracked before the oldest is stolen
static constexpr int kSubBlock   = 32;   // control-rate granularity in samples

//==============================================================================
// Per-voice fields. Mirror ids in the UI as v{01..16}_{name}.
enum VoiceField
{
    vfWave = 0,   // 0 saw, 1 pulse, 2 tri, 3 sine
    vfFoot,       // 0..3 = 32' 16' 8' 4'
    vfTune,       // -1..1  (±50 cents)
    vfCut,        // 0..1
    vfRes,        // 0..1
    vfLfoWave,    // 0 sin, 1 tri, 2 sqr, 3 s&h
    vfLfoRate,    // 0..1 (exp 0.02..8 Hz)
    vfLfoAmp,     // 0..1
    vfLfoFlt,     // 0..1
    vfFAtk,       // filter envelope: attack, decay, sustain level, release
    vfFDec,
    vfFSus,
    vfFRel,
    vfAAtk,       // amp envelope: the same four
    vfADec,
    vfASus,
    vfARel,
    vfLoop,       // 0/1 envelope self-retrigger
    vfDrift,      // 0..1
    vfPan,        // -1..1
    vfLevel,      // 0..1 fader
    vfMute,       // 0/1
    vfSolo,       // 0/1
    vfNote,       // legacy, ignored: note assignment is gNoteMode's job now
    vfPw,         // 0..1 pulse duty (0.05..0.95); the pulse wave only
    numVoiceFields
};

// Globals. Mirror ids in the UI as g_{name}.
enum GlobalParam
{
    gTolerance = 0, // 0..1  component scatter between clones
    gTide,          // 0..1  minutes-scale conductor depth
    gEntrainA,      // 0..1  phase pull within army A
    gEntrainB,
    gDriftMaster,   // 0..1  scales all per-voice drift
    gTemperA,       // 0..2  0 growl, 1 scream, 2 ladder
    gTemperB,
    gLatchA,        // 0/1
    gLatchB,
    gKbdA,          // 0..1  filter key tracking per army
    gKbdB,
    gBaseA,         // 0..1  base pitch, midi 24..60
    gBaseB,
    gGlide,         // 0..1  0..2 s portamento
    gSpread,        // 0..1  fans all microtunes symmetrically
    gWar,           // 0..1  A/B crossfade target
    gWarSlew,       // 0..1  crossfade slew 50 ms .. 300 s
    gMaster,        // 0..1
    gWidth,         // 0..1  stereo width (0.5 = unity)
    gBusSat,        // 0..1  bus soft-clip amount (auto-gained)
    gBassMono,      // 0/1
    gHpf,           // 0/1
    gDrone,         // 0/1  power-on drone: an army with no note sounds its base
    gHq,            // 0..2 quality: 0 LOW (1x), 1 HQ (2x), 2 XHQ (4x oversampled
                    //      nonlinearities; the host forces 2 for offline render)
    gSpringDwell,   // 0..1
    gSpringMix,     // 0..1
    gSpringFreeze,  // 0/1
    gTapeTime,      // 0..1  60 ms .. 1.2 s
    gTapeFdbk,      // 0..1
    gTapeMix,       // 0..1
    gBbdRate,       // 0..1  0.05..6 Hz
    gBbdDepth,      // 0..1
    gDriveAmt,      // 0..1
    gRanks,         // 0..1  THE RANKS: 0.5 = as set; ->0 pulls every continuous
                    //       per-voice value to its army's mean (unison); ->1
                    //       exaggerates the differences (up to 2.5x). Discrete
                    //       rows (wave, footage, notes) and the faders are
                    //       untouched, and the panel's stored values never move.
    gNoteMode,      // 0..2 unison / treaty poly / war poly (see NoteMode)
    gEnvFilt,       // 0..1 how much of the cutoff headroom ABOVE the knob the
                    //      filter envelope opens (0 = CUT alone)
    gLfoSync,       // 0/1  LFO rates locked to the host clock
    gFxMix,         // 0..1 global effect mix: dry console <-> the full rack
                    //      (sat, drive, bbd, tape, spring; the corrective bus
                    //      stays in circuit). 1.0 = exactly the old behaviour.
    gLfoDiv,        // 0..9 synced LFO cycle: 4/1 2/1 1/1 1/2 1/3 1/4 1/6 1/8 1/16 1/32
    gCutoff,        // 0..1 MASTER CUT: 0.5 neutral, offsets all 16 cutoffs
                    //      together. The per-clone CUTOFF knobs stay put and
                    //      keep their spread; this is the knob you sweep.
    numGlobals
};

// How the 16 clones are shared out over the held notes. See gNoteMode.
enum NoteMode
{
    nmUnison = 0,   // every clone on the bottom note
    nmTreaty,       // one body: the 16 split left to right, low note to high
    nmWar           // two armies contesting the chord across the WAR fader
};

// Divide `count` clones over `n` notes: palindromic and centre-weighted, so
// 16 over 3 is 5-6-5, over 4 is 4-4-4-4 and over 5 is 3-3-4-3-3.
void divideClones (int count, int n, int* out);

const char* voiceFieldId (int f);   // "wave", "foot", ...
const char* globalId (int g);       // "tolerance", ...

//==============================================================================
struct Patch          // one full parameter snapshot (the seed generator fills it)
{
    float global[numGlobals] {};
    float voice[kVoices][numVoiceFields] {};
};

// Deterministic: same seed → same patch on every machine. Category first,
// Black Rider style. Returns the category name.
const char* generatePatch (uint32_t seed, Patch& out);
void defaultPatch (Patch& out);

//==============================================================================
namespace detail
{
    inline uint32_t rngStep (uint32_t& s)
    {
        s += 0x6D2B79F5u;
        uint32_t t = s;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return t ^ (t >> 14);
    }
    inline float rng01 (uint32_t& s) { return (float) (rngStep (s) >> 8) / 16777216.0f; }

    inline float fastTanh (float x)
    {
        // Pade x(27+x²)/(27+9x²): monotone on ±3 and exactly 1 at |x| = 3;
        // beyond that it OVERSHOOTS 1 (up to ~1.03), so clamp at 3, not
        // further out — saturators must never exceed unity.
        if (x >  3.0f) return  1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

//==============================================================================
class Engine
{
public:
    Engine();

    // Message thread; allocates. Safe to call again on sample-rate change.
    void prepare (double sampleRate, int maxBlockSize);

    // Plain stores; readable any time. UI/processor thread.
    void setGlobal (int g, float v)            { globalsIn[g].store (v, std::memory_order_relaxed); }
    void setVoice  (int v, int f, float value) { voiceIn[v][f].store (value, std::memory_order_relaxed); }
    void applyPatch (const Patch& p);

    // Fixed per-instance identity: which "production batch" this unit is.
    // Feeds tolerance offsets and LFO phase scatter. Part of saved state.
    void setUnitSeed (uint32_t s) { unitSeed.store (s, std::memory_order_relaxed); }

    // Host tempo for LFO SYNC, and the SCATTER button. Both are message
    // thread; the audio thread picks them up at the next sub-block.
    void setBpm (double b) { hostBpm.store (b, std::memory_order_relaxed); }
    // Brokild World FX world-mod bus (plain stores, any thread). The five
    // values are fanned across the sixteen clones inside renderVoices; a
    // NEUTRAL bus (0,0,0,0,1) is skipped entirely, so it is bit-identical.
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

    // Performance wheels: bend in semitones (+-2), mod 0..1 (vibrato depth -
    // each clone vibrates on its OWN LFO phase, so the ensemble shimmers).
    void setBend (float semis) { bendIn.store (semis, std::memory_order_relaxed); }
    void setMod  (float m)     { modIn.store (m, std::memory_order_relaxed); }
    void scatterLfoPhases() { scatterReq.store (1, std::memory_order_relaxed); }

    // Note slots (host MIDI). May be called from the audio thread.
    void noteOn  (int midiNote);
    void noteOff (int midiNote);
    void allNotesOff();

    // Audio thread.
    void process (float* L, float* R, int n);

    // Meters (audio thread writes, UI reads).
    float voiceMeter (int v) const { return meters[v].load (std::memory_order_relaxed); }
    float masterMeterL() const     { return meterL.load (std::memory_order_relaxed); }
    float masterMeterR() const     { return meterR.load (std::memory_order_relaxed); }

private:
    //==========================================================================
    struct Onepole { float z = 0; float process (float x, float a) { z += a * (x - z); return z; } };

    // Sallen-Key two-pole, the GROWL and SCREAM circuits, ported from Black
    // Rider: two one-pole TPT stages with feedback of K through a one-pole of
    // the opposite kind and an asymmetric diode clipper in the feedback path.
    // Solved as a zero-delay loop for the linear part, then refined twice for
    // the clipper. Q = 1/(2-K); K = 2 is the edge of self-oscillation, and the
    // clipper is what holds it there instead of letting it run away.
    struct K35
    {
        float G = 0.09f, s1 = 0, s2 = 0, s3 = 0, sat = 1.0f;
        void reset() { s1 = s2 = s3 = 0.0f; }
        void setG (float g) { G = g / (1.0f + g); }
        float clip (float x) const
        {
            // the diode pair: firm, a touch asymmetric so the scream has an edge
            const float t = x > 0.0f ? x : x * 1.12f;
            const float y = sat * detail::fastTanh (t / sat);
            return x > 0.0f ? y : y / 1.12f;
        }
        float step (float x, float K)
        {
            const float v1 = (x - s1) * G; const float a = v1 + s1; s1 = a + v1;
            const float oneG = 1.0f - G;
            const float den = 1.0f / (1.0f - G * K * oneG);
            float y = (G * a + oneG * s2 - G * K * oneG * s3) * den;
            float u = a;
            for (int it = 0; it < 2; ++it)            // refine for the clipper
            {
                const float hp = oneG * (y - s3);
                u = a + clip (K * hp);
                y = G * u + oneG * s2;
            }
            const float v2 = (u - s2) * G; s2 = y + v2;
            const float vh = (y - s3) * G; const float lp = vh + s3; s3 = lp + vh;
            return y;
        }
        // os steps, linear-interp upsampling from xPrev, averaging decimator
        float process (float x, float xPrev, float K, int os);
    };

    // Transistor ladder, ported from Black Rider: four trapezoidal one-poles
    // with the loop solved EXACTLY for the linear case, and only then the
    // differential pair applied as a tanh. Solving first and saturating after
    // is what keeps the tuning exact - a saturator inside the loop has no
    // phase - while the saturator still sets the self-oscillation amplitude.
    // The bass thinning as resonance rises is correct ladder behaviour.
    struct Ladder
    {
        float G = 0.09f, s1 = 0, s2 = 0, s3 = 0, s4 = 0;
        void reset() { s1 = s2 = s3 = s4 = 0.0f; }
        void setG (float g) { G = g / (1.0f + g); }
        float step (float x, float k)
        {
            const float G2 = G * G, G4 = G2 * G2, oneG = 1.0f - G;
            const float S = G2 * G * oneG * s1 + G2 * oneG * s2 + G * oneG * s3 + oneG * s4;
            const float y4lin = (G4 * x + S) / (1.0f + k * G4);
            float u = x - k * y4lin;
            u = 1.3f * detail::fastTanh (u * (1.0f / 1.3f));
            auto stage = [this] (float in, float& s)
            { const float v = G * (in - s); const float y = v + s; s = y + v; return y; };
            auto soft = [] (float y) { return y * (1.0f - std::min (0.3f, y * y * 0.025f)); };
            const float y1 = soft (stage (u,  s1));
            const float y2 = soft (stage (y1, s2));
            const float y3 = soft (stage (y2, s3));
            return stage (y3, s4);
        }
        float process (float x, float xPrev, float k, int os);
    };

    // stateless saturator, oversampled the same way (for the master chain)
    static float satOS (float x, float& xPrev, float drive, float norm, int os);

    struct Env
    {
        float value = 0;
        int   stage = 0;         // 0 idle, 1 attack, 2 sustain/decaying
        bool  looping = false;
        void  gateOn()  { stage = 1; }
        void  gateOff() { if (stage != 0) stage = 2; }
        // Full ADSR: attack to 1, decay to sus while held, release with its
        // own coefficient. Loop mode cycles attack <-> decay.
        float tick (float atkCoef, float decCoef, float relCoef, float sus, bool loop);
    };

    struct Voice
    {
        double phase = 0, lfoPhase = 0;
        float  triState = 0;           // leaky integrator for tri
        float  freqCurrent = 110;      // glide state (Hz)
        int    lastFoot = -1;          // footage jumps are instant, never glide
        float  prevOsc = 0;            // previous osc sample (filter upsampling)
        float  driftState = 0;         // OU random walk
        uint32_t noiseState = 1;
        float  shValue = 0;            // sample & hold latch
        K35    k35;  Ladder ladder;
        Env    envAmp, envFlt;
        bool   gated = false;
        float  ampSmooth = 0, panSmooth = 0, cutSmooth = 0.5f;
        // fixed tolerance offsets, refreshed when unit seed changes
        float  tolDetune = 0, tolCut = 0, tolEnv = 1, tolLevel = 1, tolLfo = 1;
        float  lfoPhase0 = 0;
        float  meterAcc = 0;
        float  wmGate = 0;             // smoothed gate for the sag (world mod)
    };

    struct DelayLine
    {
        std::array<float, 1 << 17> buf {};   // ~2.7 s at 48 k
        int  w = 0;
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) & (int) (buf.size() - 1); }
        float read (float delaySamples) const;
    };

    //==========================================================================
    void refreshTolerances();
    void controlTick();                       // once per sub-block
    void renderVoices (float* mixLA, float* mixRA, float* mixLB, float* mixRB, int n);
    void masterChain (float* L, float* R, int n);

    static float footMult (int f)   // 64' 32' 16' 8' 4' 2'
    {
        static constexpr float m[6] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        return m[std::clamp (f, 0, 5)];
    }

    //==========================================================================
    double fs = 48000.0;
    int    maxBlock = 0;

    std::array<std::atomic<float>, numGlobals> globalsIn {};
    std::array<std::array<std::atomic<float>, numVoiceFields>, kVoices> voiceIn {};
    float G[numGlobals] {};                        // control-rate copies
    float V[kVoices][numVoiceFields] {};

    std::atomic<uint32_t> unitSeed { 0xC70BE5u };
    uint32_t tolSeedApplied = 0;

    std::array<Voice, kVoices> voices;

    // Held notes: midi note or -1, a held flag so latch can keep a released
    // note sounding, and an arrival stamp so the oldest is stolen first.
    // Every entry is written out - a short brace list zero-fills the rest.
    std::atomic<int>      heldNote [kMaxHeld] { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::atomic<int>      heldOn   [kMaxHeld] {  0,  0,  0,  0,  0,  0,  0,  0 };
    std::atomic<uint32_t> heldStamp[kMaxHeld] {  0,  0,  0,  0,  0,  0,  0,  0 };
    std::atomic<uint32_t> heldClock { 1 };
    std::atomic<double>   hostBpm { 120.0 };
    std::atomic<int>      scatterReq { 0 };
    std::atomic<float>    bendIn { 0.0f };
    std::atomic<float>    modIn { 0.0f };
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float wmDet = 0, wmPan = 0, wmTremD = 0, wmTremR = 0, wmSag = 0, wmFmul = 1;
    bool  wmActive = false;
    double wmT = 0;                    // seconds, for the trem phases
    uint32_t              scatterState = 0x9E3779B9u;

    // Rebuilt once per sub-block by assignNotes(): the midi note each clone
    // plays (-1 = none), whether that note is physically held, and whether the
    // army has any note at all (DRONE fills in for an army that has not).
    int  vAssign[kVoices] {};
    int  vAssignHeld[kVoices] {};
    bool armyGated[2] {};
    int  activeCount = 0;
    // sticky seats: the sounding set of the previous sub-block, so a release
    // can be told apart from an arrival (only arrivals redistribute)
    int  lastList[kMaxHeld] {};
    int  lastListN = 0;
    int  lastMode = -1;
    void assignNotes (const float* Gp);

    // buses & master state
    float warCurrent = 0.5f;
    float tideState = 0, tideState2 = 0;
    uint32_t tideRng = 77;
    Onepole hpfL1, hpfL2, hpfR1, hpfR2, bassLpL, bassLpR;
    float masterSmooth = 0.75f;
    float satPrevL = 0, satPrevR = 0, drvPrevL = 0, drvPrevR = 0,
          mstPrevL = 0, mstPrevR = 0;   // upsampling memories for saturators

    // FX
    DelayLine tapeL, tapeR, bbdL, bbdR;
    Onepole tapeToneL, tapeToneR, bbdToneL, bbdToneR;
    double bbdPhase = 0, wowPhase = 0;
    struct AP { std::array<float, 4096> buf {}; int w = 0; int len = 1; float g = 0.5f; float process (float x); };
    AP springAp[4];
    struct Comb { std::array<float, 8192> buf {}; int w = 0; int len = 1; float damp = 0.3f, fb = 0.8f; Onepole lp; float process (float x); };
    Comb springComb[3];
    float springChirpPhase = 0;

    // scratch buffers (sized in prepare)
    std::array<float, 4096> busLA {}, busRA {}, busLB {}, busRB {};

    std::array<std::atomic<float>, kVoices> meters {};
    std::atomic<float> meterL { 0 }, meterR { 0 };
    int meterCountdown = 0;
};

} // namespace cw
