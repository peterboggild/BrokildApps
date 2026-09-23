#pragma once

// Native audio engine for Photo-Synth2 — a 1:1 port of the original browser
// app's audio layer: the "photo-synth-voice" AudioWorklet (16-voice pool),
// the additive fallback engine, and the WebAudio FX graph built in
// buildChain() (tube saturation, stereo delay with tape/freeze/wow,
// dual-convolver reverb, 4-stage phaser, dual-line chorus, stutter gate,
// lofi stage), reorderable and switchable exactly like the original.
//
// The UI (unchanged original JS) drives this through parameter/voice
// messages that mirror what it used to send to the WebAudio graph.

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

namespace ps
{

//==============================================================================
// Global engine parameters, mirroring the WebAudio AudioParams the UI touches.
enum Param
{
    pMaster = 0,
    pPkFreq, pPkGain,
    pSatDry, pSatWet, pSatPre, pSatToneF,
    pDelayTime0, pDelayTime1, pDelayFb, pDampF, pDhpF, pDfeed, pWowDepth, pDelayDry, pDelayWet,
    pRevDry, pRevWet, pRvG0, pRvG1,
    pPhDry, pPhWet, pPhRate, pPhDepth,
    pChDry, pChWet, pChRate, pChDepth,
    pStRate, pStBase, pStMod,
    pLofiCrush, pLofiNoise, pLofiDirt,
    pLpFreq, pLpQ, pLpGain,
    pEnvGain,          // additive engine envelope gain (scheduled)
    pAddGain,          // additive engine on/off gain
    pPartF0,           // 10 consecutive: additive partial frequencies
    pPartG0 = pPartF0 + 10,   // 10 consecutive: additive partial gains
    pLimit = pPartG0 + 10,    // output limiter amount (0..1, transparent peak catcher)
    numParams
};

// Per-voice message fields (same set the worklet's {type:"params"} accepted).
enum VoiceField
{
    vfBase = 0, vfRatio, vfMorph, vfPulse, vfDetune, vfLevel, vfCut, vfRes, vfMode,
    vfDrive, vfAttack, vfDecay, vfSustain, vfRelease, vfGlide, vfSpread,
    vfEnvAmp, vfEnvFlt, vfSub, vfSubOct, vfEnvPitch, vfStop, vfGate,
    vfPan, vfGain,
    vfWave,             // 0 photo-morph, 1 sine, 2 triangle, 3 sawtooth, 4 square
    numVoiceFields
};

enum FxModule { fxSaturation = 0, fxPhaser, fxChorus, fxStutter, fxLofi, fxDelay, fxReverb, numFxModules };

enum class ReverbType { room, hall, plate, spring, reverse };

static constexpr int kNumVoices = 16;
static constexpr int kNumPartials = 10;

//==============================================================================
// Commands crossing from the message thread to the audio thread.
struct VoiceEvent
{
    int64_t frame = 0;
    uint32_t fieldMask = 0;                    // bits of VoiceField (params only)
    float values[8] {};                        // packed in field order
    int gate = -1;                             // -1 = no gate change
};

struct EventBatch                              // heap payload, freed by message thread
{
    int voice = 0;
    std::vector<VoiceEvent> events;
    std::atomic<bool> consumed { false };      // set by the audio thread after copy
};

struct Command
{
    enum Type : uint8_t
    {
        setParamValue,      // param: value f1 at time d1 (-1 = now)
        setParamTarget,     // param: target f1, tau f2, start d1 (-1 = now)
        cancelParam,        // param schedule cleared
        setVoiceField,      // voice i1, field i2, value f1, tau f2 (pan only)
        voiceEvents,        // ptr = EventBatch*
        voiceClear,         // voice i1 (-1 = all)
        voiceReset,         // voice i1
        setFxOrder,         // i1 = packed order (3 bits each), i2 = enabled bits
        setDelayLimit,      // i1 = 0/1 (tanh in delay loop)
        setLpType,          // i1 = biquad type index
        setRevXfade,        // i1 = active convolver index after crossfade
        setMeta,            // i1 = voiceMode(0 mono/1 poly), i2 = voices | notes<<8 | osc<<16
        setHostTune         // f1 = global tuning in semitones, applied to host MIDI notes
    };
    Type type = setParamValue;
    int16_t param = 0;
    int32_t i1 = 0, i2 = 0;
    float f1 = 0, f2 = 0;
    double d1 = -1;
    EventBatch* ptr = nullptr;
};

//==============================================================================
// A WebAudio-style AudioParam: exponential setTargetAtTime smoothing plus a
// tiny schedule queue for setValueAtTime / setTargetAtTime with start times.
struct NativeParam
{
    struct Sched { double t; float v; float tau; bool isTarget; };

    float current = 0, target = 0;
    float tau = 0;                  // 0 = snap
    std::vector<Sched> queue;       // preallocated; entries arrive time-sorted
    size_t head = 0;

    void init (float v)
    {
        current = target = v; tau = 0;
        queue.clear(); queue.reserve (1024); head = 0;
    }

    void push (const Sched& s)              // audio thread: no alloc past capacity
    {
        if (queue.size() < queue.capacity())
            queue.push_back (s);
    }

    void cancel() { queue.clear(); head = 0; }

    // advance by n samples; activates due schedule entries; returns value at end
    float tick (double timeNow, double fs, int n)
    {
        while (head < queue.size() && queue[head].t <= timeNow)
        {
            const auto& s = queue[head];
            if (s.isTarget) { target = s.v; tau = s.tau; }
            else            { current = target = s.v; tau = 0; }
            ++head;
        }
        if (head > 0 && head == queue.size()) { queue.clear(); head = 0; }
        if (tau <= 0.0f) { current = target; }
        else
        {
            const float a = 1.0f - std::exp ((float) (-(double) n / (tau * fs)));
            current += (target - current) * a;
        }
        return current;
    }
};

//==============================================================================
// WebAudio-spec biquad (LP/HP interpret Q in dB; peaking gain in dB).
struct Biquad
{
    enum Type { lowpass = 0, highpass, bandpass, notch, allpass, peaking };
    Type type = lowpass;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1L = 0, z2L = 0, z1R = 0, z2R = 0;

    void reset() { z1L = z2L = z1R = z2R = 0; }
    void set (Type t, double freq, double Q, double gainDb, double fs);

    inline float processL (float x) { const double y = b0 * x + z1L; z1L = b1 * x - a1 * y + z2L; z2L = b2 * x - a2 * y; return (float) y; }
    inline float processR (float x) { const double y = b0 * x + z1R; z1R = b1 * x - a1 * y + z2R; z2R = b2 * x - a2 * y; return (float) y; }
};

//==============================================================================
// Exact port of the worklet's ladder stage set.
struct Ladder
{
    std::array<double, 4> z {};
    void reset() { z.fill (0.0); }
    float process (float x, float g, float k, int mode, float dk, float dnorm);
};

//==============================================================================
// Exact port of the worklet VoiceProcessor. Runs at 2x and decimates.
class Voice
{
public:
    struct Params
    {
        float base = 220, ratio = 1, morph = 0.5f, pulse = 0, detune = 6, level = 0;
        float cut = 14000, res = 0, drive = 0.25f;
        int mode = 0;
        float attack = 0.012f, decay = 0.25f, sustain = 1, release = 0.18f, glide = 0.03f, spread = 0.5f;
        float envAmp = 1, envFlt = 0;
        float sub = 0, subOct = 1, envPitch = 0, stop = 0;
        int wave = 0;
    };

    void prepare (double sampleRate, uint32_t seed);
    void setField (int field, float v);
    void addEvents (const std::vector<VoiceEvent>& evs);
    void clearEvents() { eventCount = 0; }
    void resetState();

    // renders and ADDS (through pan + gain) into mixL/mixR
    void render (float* mixL, float* mixR, int n, int64_t startFrame);

    Params p;
    int gate = 0;
    float panTarget = 0, panCurrent = 0, panTau = 0;
    // world-mod bus, per voice at block rate (neutral = exact identity)
    double wmMul = 1.0;                       // detune fan (sag rides gl below)
    float  wmFmul = 1.0f, wmGain = 1.0f, wmPanAdd = 0.0f;
    float  wmSagAmt = 0.0f;                   // extra gate-keyed sag, semitones
    float gainTarget = 0, gainCurrent = 0;      // vGain: engine/extras switch

    bool anyEvents() const { return eventCount > 0; }
    int64_t firstEventFrame() const { return eventCount > 0 ? events[0].frame : INT64_MAX; }

private:
    float shape (double& tri, double ph, double dt, double pw);
    void applyEvent (const VoiceEvent& e);

    double fsOut = 48000, fs = 96000;
    double gl = 0, phS = 0, subL = 0, stopMul = 1;
    int stage = 0, pg = 0;
    double f0 = 220, morph = 0.5, pulse = 0, detune = 6, level = 0, cut = 14000, res = 0;
    double env = 0;
    double phA = 0, phB = 0.37;
    double triA = 0, triB = 0;
    double driftA = 0, driftB = 0, lfo = 0;
    Ladder ladL, ladR;
    double dz0 = 0, dz1 = 0;
    // DC blocker on the voice output: the ladder, the comb and an
    // asymmetric pulse can all leave an offset behind.
    double dcxL = 0, dcyL = 0, dcxR = 0, dcyR = 0, dcR = 0.9993;
    std::vector<double> cbL, cbR;              // comb buffers (8192)
    int cw = 0;
    int quiet = 0;
    int blockCounter = 0;                      // 128-sample cadence for drift/pwm
    double pwm = 0.5, dk = 1, dnorm = 1;
    // per-quantum coefficients (recomputed on the 128-sample cadence, as the
    // worklet did once per render quantum)
    double cAGlide = 0, cLA = 0.5, cLB = 0.5, cEnvAmp = 1, cFltK = 0,
           cEnvPitch = 0, cSubDiv = 2, cStopTarget = 1, cAStop = 0;
    uint32_t rng = 1;

    static constexpr int kMaxEvents = 4096;
    std::array<VoiceEvent, (size_t) kMaxEvents> events;
    int eventCount = 0;

    float rand01() { rng = 1664525u * rng + 1013904223u; return (float) (rng / 4294967296.0); }
};

//==============================================================================
// Exact port of the worklet LofiProcessor.
struct Lofi
{
    float pCrush = 0, pNoise = 0, pDirt = 0;
    double crush = 0, noise = 0, dirt = 0;
    double holdL = 0, holdR = 0, holdAcc = 0;
    double crEnv = 0, crVal = 0;
    uint32_t rng = 22222;
    double fs = 48000;
    void prepare (double sampleRate) { fs = sampleRate; }
    void process (float* L, float* R, int n);
    float rand01() { rng = 1664525u * rng + 1013904223u; return (float) (rng / 4294967296.0); }
};

//==============================================================================
// Approximation of the WebAudio DynamicsCompressor used purely as a safety
// catcher (threshold -6 dB, knee 10, ratio 3, attack 6 ms, release 250 ms)
// including its automatic makeup gain.
struct SafetyCompressor
{
    double fs = 48000;
    float envDb = 0;
    float makeup = 1.32f;
    void prepare (double sampleRate) { fs = sampleRate; envDb = 0; }
    void process (float* L, float* R, int n);
};

//==============================================================================
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);
    void process (float* L, float* R, int n);

    // host MIDI (audio thread, sample offset within current block)
    void hostNoteOn (int note, int sampleOffset);
    void hostNoteOff (int note, int sampleOffset);
    void hostAllNotesOff();

    // message-thread API
    bool pushCommand (const Command& c);
    void loadReverbImpulse (ReverbType type, float lengthSeconds);   // message thread
    juce::var snapshotState();                                       // message thread (approximate: uses last-sent targets)
    void applyStateSnapshot (const juce::var& v);                    // message thread -> commands
    static int defaultFxPacked();

    // Offline/single-threaded use only (the render engine): apply directly.
    void applyCommandNow (const Command& c) { applyCommand (c); }
    Voice& voiceAt (int i) { return voices[(size_t) i]; }

    // Brokild World FX world-mod bus (plain stores, any thread). Neutral =
    // exact-identity multipliers, so it is bit-identical when unused.
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

    double getSampleRate() const { return fsHost; }
    double getCurrentTime() const { return (double) currentFrame.load() / fsHost; }
    int64_t getCurrentFrame() const { return currentFrame.load(); }

    // mirrors for editor-closed automation & snapshotting (message thread)
    std::atomic<bool> everConfigured { false };   // shadows hold a full picture
    std::array<std::atomic<float>, numParams> shadowParams {};
    std::atomic<int> shadowLpType { 0 }, shadowDLimit { 0 };
    std::atomic<int> shadowFxPacked { 0 }, shadowFxEnabled { 0x7f };
    std::atomic<int> shadowRevType { 0 };
    std::atomic<float> shadowRevLen { 1.8f };
    std::atomic<float> hostTune { 0.0f };        // Tune + Fine, in semitones
    std::array<std::array<std::atomic<float>, numVoiceFields>, kNumVoices> shadowVoice {};

    static juce::AudioBuffer<float> makeReverbImpulse (ReverbType type, float lengthSeconds, double fs);

private:
    void drainCommands();
    void applyCommand (const Command& c);
    void repitchHeld();                       // slide held host notes to a new global tuning
    void processSub (float* L, float* R, int n);
    void renderAdditive (float* L, float* R, int n);
    void processSaturation (float* L, float* R, int n);
    void processDelay (float* L, float* R, int n);
    void processPhaser (float* L, float* R, int n);
    void processChorus (float* L, float* R, int n);
    void processStutter (float* L, float* R, int n);
    void processReverb (float* L, float* R, int n);

    double fsHost = 48000;
    std::atomic<int64_t> currentFrame { 0 };
    std::array<std::atomic<float>, 6> wmIn { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    double wmT = 0.0;                    // seconds, for the world-mod trem

    std::array<NativeParam, numParams> params;
    std::array<Voice, kNumVoices> voices;

    // note allocation mirror (audio thread)
    struct Slot { int note = -1; int64_t seq = 0; bool on = false; };
    std::array<Slot, 4> slots {};
    int64_t slotSeq = 0;
    std::vector<int> heldMono;
    int metaMode = 0, metaVoices = 1, metaNotes = 4, metaOsc = 2;

    // additive engine
    std::array<double, kNumPartials> partPhase {};
    Biquad lpBiquad;                       // additive filter
    int lpTypeIndex = 0;

    Biquad pkBiquad;                       // hue formant (peaking, Q 1.2)

    // saturation
    Biquad satDC, satToneBq;
    std::unique_ptr<juce::dsp::Oversampling<float>> satOs;
    juce::HeapBlock<float> satBufL, satBufR;

    // delay
    std::vector<float> dBufL, dBufR;
    int dMask = 0, dWrite = 0;
    Biquad dampBq, dhpBq;
    double wowPhase = 0;
    bool dLimit = false;

    // phaser
    std::array<Biquad, 4> phAP;
    std::array<float, 256> phFbBufL {}, phFbBufR {};
    int phFbPos = 0;
    double phLfoPhase = 0;

    // chorus
    std::vector<float> chBufL, chBufR;
    int chMask = 0, chWrite = 0;
    Biquad chLP;
    double chLfoPhase = 0;

    // stutter
    double stLfoPhase = 0;

    // lofi
    Lofi lofi;

    // reverb: two convolvers crossfaded
    std::array<juce::dsp::Convolution, 2> conv;
    std::array<std::atomic<bool>, 2> irLoaded { false, false };
    std::atomic<int> revActive { 0 };        // written by the message thread, read while rendering
    // prepare() and loadImpulseResponse() on the same Convolution must not
    // overlap; the host prepares on its own thread while the editor loads
    // impulses from the message thread, so the two are serialised here.
    // The audio callback never takes this lock.
    juce::CriticalSection irLock;
    juce::HeapBlock<float> rvInL, rvInR;

    // fx routing (audio thread)
    int fxOrderPacked = 0;
    int fxEnabledBits = 0;

    SafetyCompressor comp;
    float limEnv = 0;                      // output limiter envelope
    double dcMxL = 0, dcMyL = 0, dcMxR = 0, dcMyR = 0, dcMR = 0.9993;   // master DC blocker

    juce::AbstractFifo cmdFifo { 65536 };
    std::vector<Command> cmdStorage;

    // scratch
    juce::HeapBlock<float> scratchL, scratchR, wetL, wetR;
    int maxBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE (Engine)
};

} // namespace ps
