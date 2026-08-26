#pragma once

// Brokild World FX — the shared, global FX rack compiled into every Brokild
// synth. Design authority: BWFX-DESIGN.md (BrokildApps repo root).
//
// Plain C++17, JUCE-free. Real-time rules (house convention): process()
// never allocates, locks or logs; every edit entry point is a plain atomic
// store the audio thread picks up at the next sub-block. prepare() and
// service() run on the message thread and may allocate.

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace bwfx
{

constexpr int kMaxParams  = 8;    // per module
constexpr int kMaxModules = 16;   // registry headroom (4-bit order packing)
constexpr int kSubBlock   = 32;   // control-rate granularity in samples

// ---------------------------------------------------------------------------
// Self-describing modules (the Hairfryer SPECS[] lesson): the rack UI, the
// state format and the bench are all generated from these tables — one
// source of truth, so the wired-to-the-wrong-knob bug class cannot be
// expressed.
struct ParamDesc
{
    const char* id;        // state key, stable forever ("time", "mix", ...)
    const char* name;      // label on the pedal
    float def, lo, hi;     // plain value range (the UI shows these numbers)
    float step;            // 0 = continuous, else the grid (1 = stepped)
    const char* unit;      // display hint: "%", "dB", "ms", "cHz" (v/100 Hz),
                           // "dHz" (v/10 Hz), "cs" (v/100 s), "" = plain
    const char* choices;   // "CLEAN|TAPE" for choice params, else nullptr
};

struct Descriptor
{
    const char* id;        // state key, stable forever ("delay", "reverb"...)
    const char* name;      // faceplate name ("ECHO", "SPACE"...)
    const char* sub;       // one-line tagline under the name
    int version;           // stored in patches; bump only on a true break
    const ParamDesc* params;
    int numParams;
};

// ---------------------------------------------------------------------------
// The fixed world-modulation bus. SPECTRA characters (second pass) write it
// per block; each host synth maps it onto its voice engine ONCE in its
// adapter. Fixed size = new characters reach every synth by rebuild alone.
struct WorldMod
{
    float detuneCents = 0;   // extra detune to fan across the host's voices
    float panSpread   = 0;   // 0..1 extra stereo scatter
    float tremolo     = 1;   // amplitude multiplier (1 = none)
    float pitchSag    = 0;   // downward pitch pull in semitones (tape sag)
    float filterMul   = 1;   // cutoff multiplier
};

// ---------------------------------------------------------------------------
class Module
{
public:
    virtual ~Module() = default;

    virtual const Descriptor& desc() const = 0;

    virtual void prepare (double fs, int maxBlock) = 0;  // message thread
    virtual void reset() = 0;                            // audio thread, no alloc
    virtual void process (float* L, float* R, int n) = 0;// audio thread
    virtual void service() {}                            // message thread ~15 Hz

    // Transition duck, 0..1 (audio thread, per sub-block). Time-based modules
    // scale what they INJECT into their buffers by this (smoothed), so an
    // enable or reorder splice replays later as a fade, not a click — the
    // first echo of a freshly enabled delay otherwise arrives as a hard edge
    // one delay-time after the transition (measured: a 0.14 step, 4x the
    // material's own). Modules without long memory ignore it.
    virtual void inputDuck (float) {}

    // Plain stores; readable any time (UI/processor thread).
    void  setParam (int i, float v) { if (i >= 0 && i < kMaxParams) pv[(size_t) i].store (v, std::memory_order_relaxed); }
    float getParam (int i) const    { return (i >= 0 && i < kMaxParams) ? pv[(size_t) i].load (std::memory_order_relaxed) : 0.0f; }

protected:
    std::array<std::atomic<float>, kMaxParams> pv {};
};

// The registry. Module type indices are the DEFAULT chain order.
int numModuleTypes();
const Descriptor& moduleDescriptor (int type);
Module* createModule (int type);          // message thread; caller owns

// All descriptors as one JSON string, for the rack UI fragment.
std::string descriptorJson();

// ---------------------------------------------------------------------------
// The rack: one instance of every registered module, reorderable, each with
// a power switch, all off by default. Empty (nothing on) = process() returns
// without touching the buffers — bit-transparent by construction.
class Rack
{
public:
    Rack();
    ~Rack();

    void prepare (double fs, int maxBlock);        // message thread; allocates
    void process (float* L, float* R, int n);      // audio thread
    void service();                                // message thread, ~15 Hz

    // --- edits (message thread; plain atomic stores) -----------------------
    void setEnabled (int type, bool on);
    void setParam   (int type, int p, float v);
    void setOrder   (const int* types, int count); // permutation of type ids
    void setMix     (float v);                     // 0 dry .. 1 full rack

    // --- queries -----------------------------------------------------------
    bool  anyEnabled() const;
    bool  getEnabled (int type) const;
    float getParam (int type, int p) const;
    void  getOrder (int* types) const;             // numModuleTypes() entries
    float getMix() const;

    // --- state: the opaque string blob hosts store verbatim ----------------
    std::string toJson() const;                    // message thread
    void fromJson (const std::string& s);          // message thread
    void clearState();                             // back to default empty

    // Rack state as one JSON payload for the UI (order/enables/params/mix).
    std::string uiStateJson() const;

    WorldMod worldMod() const { return {}; }       // neutral until SPECTRA land

    static const char* version();                  // "1.0.0"

private:
    std::array<std::unique_ptr<Module>, kMaxModules> mods;
    int nTypes = 0;

    std::atomic<uint64_t> orderPacked { 0 };       // 4 bits per slot
    std::atomic<uint32_t> enabledBits { 0 };
    std::atomic<float>    mixIn { 1.0f };

    // audio-thread state
    uint64_t orderApplied = 0;
    std::array<float, kMaxModules> env {};         // per-module enable ramp
    std::array<bool,  kMaxModules> wasOff {};      // for reset-on-rising-edge
    float dip = 1.0f;                              // structural-change dip
    float mixSm = 1.0f;
    bool  prepared = false;
    double fs = 48000.0;
    int maxBlock = 0;
    std::unique_ptr<float[]> dryL, dryR;

    static uint64_t packOrder (const int* types, int count);
    void unpackOrder (uint64_t packed, int* types) const;
};

} // namespace bwfx
