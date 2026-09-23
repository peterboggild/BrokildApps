#pragma once

// Engine template: the reusable machinery for an HTML→VST3 port.
//
//  * NativeParam  — Web Audio AudioParam semantics (setValueAtTime,
//                   setTargetAtTime with exponential approach, a small
//                   time-ordered schedule queue).
//  * Biquad       — Web Audio coefficient formulas, including Q-in-dB for
//                   low/highpass and the 0 Hz clamp.
//  * Command      — one POD struct through a lock-free FIFO.
//  * Engine       — drains commands, splits blocks, ticks parameters.
//
// Add your own parameter ids, voice fields and DSP blocks.

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

namespace ps
{

//==============================================================================
// One entry per engine parameter. Mirror this list, in order, in bridge.js.
enum Param
{
    pMaster = 0,
    // … add yours …
    numParams
};

// One entry per per-voice field. Mirror in bridge.js as VF.
enum VoiceField
{
    vfGate = 0, vfPan, vfGain,
    // … add yours …
    numVoiceFields
};

static constexpr int kNumVoices = 16;

//==============================================================================
struct VoiceEvent
{
    int64_t frame = 0;
    uint32_t fieldMask = 0;      // bit per VoiceField
    float values[8] {};          // packed in field order
    int gate = -1;               // -1 = no change
};

struct EventBatch                // heap payload; the message thread owns it
{
    int voice = 0;
    std::vector<VoiceEvent> events;
    std::atomic<bool> consumed { false };
};

struct Command
{
    enum Type : uint8_t
    {
        setParamValue,   // param, f1 = value,  d1 = time (-1 = now)
        setParamTarget,  // param, f1 = target, f2 = timeConstant, d1 = time
        cancelParam,
        setVoiceField,   // i1 = voice, i2 = field, f1 = value
        voiceEvents,     // ptr = EventBatch*
        voiceClear,      // i1 = voice (-1 = all)
        voiceReset,
        setMeta          // whatever global mode data you need
    };
    Type type = setParamValue;
    int16_t param = 0;
    int32_t i1 = 0, i2 = 0;
    float f1 = 0, f2 = 0;
    double d1 = -1;
    EventBatch* ptr = nullptr;
};

//==============================================================================
/** A Web Audio AudioParam: exponential setTargetAtTime smoothing plus a small
    queue for values scheduled in the future. */
struct NativeParam
{
    struct Sched { double t; float v; float tau; bool isTarget; };

    float current = 0, target = 0;
    float tau = 0;                       // 0 = snap
    std::vector<Sched> queue;            // preallocated; entries arrive sorted
    size_t head = 0;

    void init (float v)
    {
        current = target = v; tau = 0;
        queue.clear(); queue.reserve (1024); head = 0;
    }

    void push (const Sched& s)           // audio thread: never grows past capacity
    {
        if (queue.size() < queue.capacity())
            queue.push_back (s);
    }

    void cancel() { queue.clear(); head = 0; }

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

        if (tau <= 0.0f) current = target;
        else current += (target - current) * (1.0f - std::exp ((float) (-(double) n / (tau * fs))));
        return current;
    }
};

//==============================================================================
/** Web Audio biquad. Q is in dB for lowpass/highpass, linear otherwise;
    peaking gain is in dB. Always clamp swept frequencies away from 0 Hz. */
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
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock);   // message thread: may allocate
    void process (float* L, float* R, int n);         // audio thread: must not

    void hostNoteOn  (int note, int sampleOffset);
    void hostNoteOff (int note, int sampleOffset);
    void hostAllNotesOff();

    bool pushCommand (const Command& c);              // message thread only
    void applyCommandNow (const Command& c) { applyCommand (c); }   // offline use

    double getSampleRate()  const { return fsHost; }
    double getCurrentTime() const { return (double) currentFrame.load() / fsHost; }

    // Mirrors so state survives a re-prepare (sample-rate change) and can be
    // snapshotted for the host without touching the audio thread.
    std::atomic<bool> everConfigured { false };
    std::array<std::atomic<float>, numParams> shadowParams {};
    std::array<std::array<std::atomic<float>, numVoiceFields>, kNumVoices> shadowVoice {};

private:
    void drainCommands();
    void applyCommand (const Command& c);
    void processSub (float* L, float* R, int n);      // one short sub-block

    double fsHost = 48000;
    int maxBlockSize = 0;
    std::atomic<int64_t> currentFrame { 0 };

    std::array<NativeParam, numParams> params;

    juce::AbstractFifo cmdFifo { 65536 };
    std::vector<Command> cmdStorage;

    JUCE_DECLARE_NON_COPYABLE (Engine)
};

} // namespace ps
