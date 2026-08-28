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
#include <vector>
#include <string>

namespace bwfx
{

constexpr int kMaxParams  = 16;   // per module (raised from 8 for STRIP's
                                  // comp+EQ; state is keyed by id, so the
                                  // arrays simply got roomier — old blobs
                                  // round-trip unchanged, proven in the bench)
constexpr int kMaxModules = 16;   // registry headroom (4-bit order packing)
constexpr int kSubBlock   = 32;   // control-rate granularity in samples

/*  MACROS. The only part of the rack a host can automate, and the count is
    FROZEN: these become host parameters, and a parameter list that grows is
    the one thing this design exists to avoid. Five, decided 2026-08-28 —
    four free plus one that ships assigned to the rack's dry/wet.

    A macro is neutral at zero. Zero means "the patch exactly as saved", the
    same contract as an empty rack, a presence, or the world-mod bus. */
constexpr int kMacros       = 5;
constexpr int kMaxMacroDest = 64;   // total assignments across all five

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
    const char* custom = nullptr;   // UI hook: "steps" = the fragment renders
                                    // its step-grid editor above the knobs
};

// ---------------------------------------------------------------------------
// The fixed world-modulation bus. SPECTRA characters write it per sub-block;
// each host synth maps it onto its voice engine ONCE in its adapter. Fixed
// size = new characters reach every synth by rebuild alone. The tremolo is
// carried as depth+rate (not a single gain) because the swarm characters
// need PER-VOICE phases — the character describes the modulation, the host
// realises it across its own voices with golden-angle offsets.
struct WorldMod
{
    float detuneCents = 0;   // extra detune, fanned +-1 across host voices
    float panSpread   = 0;   // 0..1 extra stereo scatter, fanned likewise
    float tremDepth   = 0;   // 0..1 per-voice tremolo depth
    float tremRate    = 0;   // Hz, per-voice spread around this
    float pitchSag    = 0;   // semitones down; hosts key it to their GATE
                             // (in tune while held, sags as the note dies —
                             // the Photo-Synth lesson, never the amp env)
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

    // Host tempo, per sub-block (audio thread). Modules with a SYNC option
    // read it; the rest ignore it. 0 or nonsense means "no host clock" —
    // a module must then behave exactly as if it were set to FREE.
    virtual void setTempo (double) {}

    // Bar position, per sub-block (audio thread): ppq extrapolated to the
    // sub-block start, and whether the transport rolls. ppq < 0 = unknown
    // (host never sent it) — a pattern module then free-runs internally.
    virtual void setClock (double /*ppq*/, bool /*playing*/) {}

    // Opaque per-module extra state — for state a knob cannot carry (the
    // DISRUPTOR's drawn pattern). Rides in the blob as "x"; message thread;
    // the module must hand it to its audio thread lock-free itself.
    virtual std::string getExtra() const { return {}; }
    virtual void setExtra (const std::string&) {}

    // Plain stores; readable any time (UI/processor thread).
    void  setParam (int i, float v) { if (i >= 0 && i < kMaxParams) pv[(size_t) i].store (v, std::memory_order_relaxed); }

    /*  What the DSP reads: the stored value PLUS whatever the macros are
        adding this sub-block. The rack has already clamped the offset so the
        sum lands inside the parameter's own range, which is why this can be a
        plain add rather than a clamp on every read.

        With nothing assigned every offset is 0.0f, and `x + 0.0f` is exact —
        so a rack without macros renders the identical samples it always did. */
    float getParam (int i) const
    {
        return (i >= 0 && i < kMaxParams)
             ? pv[(size_t) i].load (std::memory_order_relaxed)
             + pvo[(size_t) i].load (std::memory_order_relaxed)
             : 0.0f;
    }

    /*  The stored value alone — what the PATCH says. The panel, the state and
        a morph all want this one: a knob that wandered because a macro was
        moving would be showing something no patch ever described. */
    float getParamRaw (int i) const
    {
        return (i >= 0 && i < kMaxParams) ? pv[(size_t) i].load (std::memory_order_relaxed) : 0.0f;
    }

    //  written by the rack once per sub-block, audio thread
    void setParamOffset (int i, float v)
    {
        if (i >= 0 && i < kMaxParams) pvo[(size_t) i].store (v, std::memory_order_relaxed);
    }

protected:
    std::array<std::atomic<float>, kMaxParams> pv {};

private:
    std::array<std::atomic<float>, kMaxParams> pvo {};
};

// The registry. Module type indices are the DEFAULT chain order.
int numModuleTypes();
const Descriptor& moduleDescriptor (int type);
Module* createModule (int type);          // message thread; caller owns

// All descriptors as one JSON string, for the rack UI fragment.
std::string descriptorJson();

// ---------------------------------------------------------------------------
// SPECTRA characters: not FX in the chain but personalities on the bus.
// A character ADDS its contribution to a WorldMod each sub-block; the rack
// combines all armed characters by the Photo-Synth rules (detune/pan add,
// multipliers multiply, tremolo unions, sag takes the max), scaled by each
// character's presence (its arm strength — which is also what a morph rides).
class Character
{
public:
    virtual ~Character() = default;
    virtual const Descriptor& desc() const = 0;
    virtual void reset() = 0;                            // audio thread safe
    virtual void tick (double dt, WorldMod& add) = 0;    // audio thread

    // Characters that OWN audio DSP (Pink's wash, Black's grind chain,
    // Glass's hall — reusing the rack's module classes privately) implement
    // these; pure modulators keep the defaults. processAudio runs IN PLACE at
    // full strength — the rack crossfades the result by the character's
    // presence envelope, so presence 0 is bit-transparent by construction.
    // Audio characters work in EVERY host (the rack's audio path needs no
    // engine mapping); only the tick() half needs a bus-consuming host.
    virtual bool hasAudio() const { return false; }
    virtual void prepare (double /*fs*/, int /*maxBlock*/) {} // message thread; may allocate
    virtual void resetAudio() {}                              // audio thread, no alloc
    virtual void processAudio (float* /*L*/, float* /*R*/, int /*n*/,
                               float /*duck*/) {}             // audio thread
    virtual void service() {}                                 // message thread ~15 Hz

    void  setParam (int i, float v) { if (i >= 0 && i < kMaxParams) pv[(size_t) i].store (v, std::memory_order_relaxed); }
    float getParam (int i) const    { return (i >= 0 && i < kMaxParams) ? pv[(size_t) i].load (std::memory_order_relaxed) : 0.0f; }

protected:
    std::array<std::atomic<float>, kMaxParams> pv {};
};

constexpr int kMaxChars = 8;
int numCharacters();
const Descriptor& characterDescriptor (int c);
Character* createCharacter (int c);       // message thread; caller owns
std::string characterJson();              // for the rack UI fragment

// Built-in rack presets: curated named blobs shipped with the library,
// [{name, blob:{...}}]. A preset is applied through Rack::fromJson — the
// same tolerant path patches ride — so applying one can never express a
// state a patch could not. Purely additive: nothing applies one on its own.
std::string presetsJson();
int numPresets();
const char* presetName (int i);
const char* presetBlob (int i);            // the sparse blob JSON, verbatim

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
    // Host tempo for the synced modules. Hosts call this from processBlock
    // with the playhead's BPM; harmless when a host offers none.
    void setBpm (double bpm) { bpmIn.store (bpm > 1.0 && bpm < 999.0 ? bpm : 0.0, std::memory_order_relaxed); }

    // The full transport, for pattern modules (the DISRUPTOR): call from
    // processBlock, audio thread, once per block, ppq at block start.
    // Hosts that only know BPM keep calling setBpm; ppq stays "unknown".
    void setTransport (double bpm, double ppq, bool playing)
    {
        setBpm (bpm);
        ppqIn.store (ppq, std::memory_order_relaxed);
        playingIn.store (playing, std::memory_order_relaxed);
        samplesSinceTransport = 0;
    }

    // Per-module extra state (message thread) — see Module::setExtra.
    void setExtra (int type, const std::string& x);
    std::string getExtra (int type) const;

    // --- MACROS ------------------------------------------------------------
    /*  The five host parameters, 0..1, called from processBlock like setBpm.
        A macro ADDS to its destinations rather than setting them, the way the
        Mars Wars patch bay adds to a knob: the panel keeps showing the
        patch's own value, an automation lane never fights the knob, and a
        macro at rest is exactly the patch. */
    void setMacro (int i, float v);
    float getMacro (int i) const;

    /*  Assignments are STRUCTURE, not values: they live in the blob, travel
        with a patch, cross synths, and a morph leaves them alone.

        A destination is addressed by NAME, never by index — "echo.time",
        "space.pr", "mix" — so an assignment survives the registry gaining or
        losing modules. One this build does not recognise is kept verbatim and
        written back out, which is what lets a rack from a newer BWFX round
        trip through an older one. Depth is -1..+1 of the destination's own
        full range. */
    void setMacroAssign (int macro, const std::string& dest, float depth);   // depth 0 removes
    void clearMacroAssigns (int macro);
    std::string macroAssignJson() const;      // for the overlay: [[{d,a}...] x5]
    bool macroIsDefault() const;              // never edited: macro 5 holds the dry/wet
    void macroSetDefault();

    // --- SPECTRA characters (message thread edits, audio thread ticks) -----
    void setCharArmed (int c, bool on);
    void setCharParam (int c, int p, float v);
    void setCharPresence (int c, float v);
    bool  getCharArmed (int c) const;
    float getCharParam (int c, int p) const;
    float getCharPresence (int c) const;

    // The host adapter declares ONCE that its engine consumes worldMod();
    // the overlay then shows the SPECTRA rack live instead of the
    // "arriving" plate. Hosts without a mapping stay on the plate.
    void setWorldModConsumed (bool on) { busConsumed.store (on, std::memory_order_relaxed); }
    bool isWorldModConsumed() const    { return busConsumed.load (std::memory_order_relaxed); }
    // PRESENCE: the per-module dry/wet the rack itself owns — the same scalar
    // that makes power toggles click-free, promoted to a stored, morphable
    // knob. 0 = the module is inert (bit-transparent), 1 = fully in.
    void setPresence (int type, float v);

    // --- queries -----------------------------------------------------------
    bool  anyEnabled() const;
    bool  getEnabled (int type) const;
    float getParam (int type, int p) const;
    float getPresence (int type) const;
    void  getOrder (int* types) const;             // numModuleTypes() entries
    float getMix() const;

    // --- state: the opaque string blob hosts store verbatim ----------------
    std::string toJson() const;                    // message thread
    void fromJson (const std::string& s);          // message thread
    void clearState();                             // back to default empty

    // Patch morphing (message thread): glide the rack between two stored
    // blobs. The UNION of both patches' modules runs; a module only in A
    // fades out via presence as t rises, one only in B fades in, one in both
    // interpolates. Continuous params lerp; stepped ones (choices) defect at
    // their own deterministic threshold staggered across the travel; the
    // chain order swaps through the click-free dip at mid-morph. An empty
    // blob is the default empty rack, so morphing to a rack-less patch
    // breathes the whole rack out. Hosts with a morph feature call this per
    // morph tick; every host gets it by rebuild alone.
    void applyMorph (const std::string& a, const std::string& b, float t);

    // Rack state as one JSON payload for the UI (order/enables/params/mix).
    std::string uiStateJson() const;

    // The combined SPECTRA bus as of the last processed block (audio thread
    // publishes; the host reads it in processBlock BEFORE its engine runs —
    // one block of modulation latency, inaudible at LFO rates).
    WorldMod worldMod() const;

    static const char* version();                  // "1.0.0"

private:
    std::array<std::unique_ptr<Module>, kMaxModules> mods;
    int nTypes = 0;

    std::atomic<uint64_t> orderPacked { 0 };       // 4 bits per slot
    std::atomic<uint32_t> enabledBits { 0 };
    std::atomic<float>    mixIn { 1.0f };
    std::atomic<double>   bpmIn { 0.0 };           // 0 = no host clock
    std::atomic<double>   ppqIn { -1.0 };          // <0 = unknown
    std::atomic<bool>     playingIn { false };
    int samplesSinceTransport = 0;                 // audio thread only
    std::array<std::atomic<float>, kMaxModules> presenceIn {};

    // SPECTRA
    std::array<std::unique_ptr<Character>, kMaxChars> chars;
    int nChars = 0;
    std::atomic<uint32_t> charArmedBits { 0 };
    std::array<std::atomic<float>, kMaxChars> charPresenceIn {};
    std::array<bool, kMaxChars> charWasOff {};     // audio thread: reset on arm
    std::array<std::atomic<float>, 6> busOut {};   // det,pan,tremD,tremR,sag,fmul
    std::atomic<bool> busConsumed { false };
    void tickCharacters (int nSamples);            // audio thread
    // audio characters: per-character presence ramp, reset on rising edge
    std::array<float, kMaxChars> charEnv {};
    std::array<bool,  kMaxChars> charAudioOff {};
    void processCharacterAudio (float* L, float* R, int n);  // audio thread

    // MACROS
    struct Assign { std::string dest; float depth = 0.0f; };
    std::array<std::vector<Assign>, kMacros> macroAssign;   // message thread owns
    std::array<std::atomic<float>, kMacros> macroIn {};
    bool macroDefaulted = true;      // no "m" key seen: macro 5 holds the dry/wet

    /*  The resolved table the audio thread reads. Published double-buffered
        with an atomic index, the same way the KIERANATOR publishes its
        pattern: the writer fills the spare slot and swaps, so the audio
        thread never sees a half-written table. */
    struct RDest
    {
        uint8_t kind = 0;            // 1 mix, 2 module param, 3 module presence
        uint8_t macro = 0;
        uint8_t type = 0, param = 0;
        float depth = 0.0f, lo = 0.0f, hi = 1.0f;
    };
    std::array<std::array<RDest, kMaxMacroDest>, 2> destBuf {};
    std::array<int, 2> destCount { 0, 0 };
    std::atomic<int> destIdx { 0 };
    void materialiseDefault();      // write the implicit wiring down before editing
    void republishMacros();          // message thread: resolve names -> indices
    void applyMacros();              // audio thread: one pass per sub-block
    void macroFromBlob (const std::string& s);
    std::string macroToBlob() const; // "" when never edited, so old blobs match
    std::array<std::atomic<float>, kMaxModules> presenceOff {};
    std::atomic<float> mixOff { 0.0f };

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

    struct BlobState;                              // parsed blob (bwfx_rack.cpp)
    void parseBlob (const std::string& s, BlobState& out) const;
    void applyBlobState (const BlobState& bs);
};

} // namespace bwfx
