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

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace cw
{

static constexpr int kVoices     = 16;
static constexpr int kArmySize   = 8;
static constexpr int kNoteSlots  = 3;
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
    vfEnvF,       // 0..1 one-knob shape (filter env)
    vfEnvA,       // 0..1 one-knob shape (amp env)
    vfLoop,       // 0/1 envelope self-retrigger
    vfDrift,      // 0..1
    vfPan,        // -1..1
    vfLevel,      // 0..1 fader
    vfMute,       // 0/1
    vfSolo,       // 0/1
    vfNote,       // 0..2 which held note slot this clone follows
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
    gDrone,         // 0/1  power-on drone: empty slot 1 falls back to base pitch
    gHq,            // 0/1  1 = HQ (oversampled nonlinearities)
    gSpringDwell,   // 0..1
    gSpringMix,     // 0..1
    gSpringFreeze,  // 0/1
    gTapeTime,      // 0..1  60 ms .. 1.2 s
    gTapeFdbk,      // 0..1
    gTapeMix,       // 0..1
    gBbdRate,       // 0..1  0.05..6 Hz
    gBbdDepth,      // 0..1
    gDriveAmt,      // 0..1
    numGlobals
};

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
        // Pade-ish, accurate to ~1e-4 in ±4; cheaper than std::tanh and
        // bit-identical across platforms for the test harness.
        if (x >  4.97f) return  1.0f;
        if (x < -4.97f) return -1.0f;
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

    struct SVF     // TPT state-variable, the GROWL/SCREAM circuits
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        // returns lowpass; nonlinear damping via tanh on the band state
        float process (float x, float g, float k, float drive);
    };

    struct Ladder  // 4-pole tanh cascade, the LADDER circuit
    {
        float s[4] {};
        void reset() { s[0] = s[1] = s[2] = s[3] = 0; }
        float process (float x, float g, float res, bool hq);
    };

    struct Env
    {
        float value = 0;
        int   stage = 0;         // 0 idle, 1 attack, 2 sustain/decaying
        bool  looping = false;
        void  gateOn()  { stage = 1; }
        void  gateOff() { if (stage != 0) stage = 2; }
        float tick (float atkCoef, float decCoef, bool loop);
    };

    struct Voice
    {
        double phase = 0, lfoPhase = 0;
        float  triState = 0;           // leaky integrator for tri
        float  freqCurrent = 110;      // glide state (Hz)
        float  driftState = 0;         // OU random walk
        uint32_t noiseState = 1;
        float  shValue = 0;            // sample & hold latch
        SVF    svf;  Ladder ladder;
        Env    envAmp, envFlt;
        bool   gated = false;
        float  ampSmooth = 0, panSmooth = 0, cutSmooth = 0.5f;
        // fixed tolerance offsets, refreshed when unit seed changes
        float  tolDetune = 0, tolCut = 0, tolEnv = 1, tolLevel = 1, tolLfo = 1;
        float  lfoPhase0 = 0;
        float  meterAcc = 0;
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

    static float footMult (int f) { const float m[4] = { 0.25f, 0.5f, 1.0f, 2.0f }; return m[f & 3]; }

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

    // note slots: midi note or -1; released flag for latch semantics
    std::atomic<int>  slotNote[kNoteSlots] { -1, -1, -1 };
    std::atomic<int>  slotHeld[kNoteSlots] { 0, 0, 0 };
    std::atomic<int>  slotOrder { 0 };

    // buses & master state
    float warCurrent = 0.5f;
    float tideState = 0, tideState2 = 0;
    uint32_t tideRng = 77;
    Onepole hpfL1, hpfL2, hpfR1, hpfR2, bassLpL, bassLpR;
    float masterSmooth = 0.75f;

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
