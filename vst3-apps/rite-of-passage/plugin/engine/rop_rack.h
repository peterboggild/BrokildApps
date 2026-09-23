#pragma once

// RITE OF PASSAGE — the spine: six slots, the score, and ARRIVAL.
//
// Design authority: RITE-OF-PASSAGE-DESIGN.md. This is the part the plugin
// IS; the effects are interchangeable and the score is not.
//
// Real-time rules: process() never allocates, locks or logs. Assignment
// (setSlotEffect) is message thread, allocates, and publishes with one atomic
// store; the retired instance waits for service() to free it.

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "rop_effect.h"
#include "rop_loudness.h"

namespace rop
{

// One slot's place on the score, and what it holds.
struct SlotState
{
    bool  on = true;
    float A[kMaxParams] {};
    float B[kMaxParams] {};

    float enter = 0.0f, exit = 1.0f;   // where on the slider it travels
    float depth = 1.0f;                // how far along A->B it actually gets
    int   curve = CurveLinear;
    int8_t paramCurve[kMaxParams];     // -1 = inherit the slot's curve

    Place place = Place::Stereo;
    Tail  tail  = Tail::Bypass;
    int   quantise = 0;                // stepped defection grid: 0 bar, 1 1/2, 2 1/4

    SlotState() { for (auto& c : paramCurve) c = -1; }
};

struct ArrivalConfig
{
    bool  restoreDry = true;
    bool  fireImpact = true;
    float impactTune = 48.0f;      // Hz
    float impactDecay = 700.0f;    // ms
    float impactLevel = -6.0f;     // dB
    int   grid = 0;                // 0 bar, 1 1/2, 2 1/4 — where it is allowed to land
};

class Rack
{
public:
    Rack();
    ~Rack();

    void prepare (double sampleRate, int maxBlock);   // message thread
    void reset();                                     // audio thread safe

    // --- editing (message thread) -----------------------------------------
    void setSlotEffect (int slot, int type);     // -1 empties the slot; allocates
    int  slotEffect (int slot) const;
    SlotState& state (int slot) { return st[(size_t) slot]; }
    const SlotState& state (int slot) const { return st[(size_t) slot]; }
    void service();                              // frees retired instances

    ArrivalConfig arrival;

    // --- globals (plain values; the host writes them between blocks) ------
    float spread = 0.0f;        // 0..1, the two channels run the score apart
    float turn = 0.0f;          // -1..1, field rotation, +-45 degrees
    float monoGate = 0.0f;      // 0..1, how far toward mono the last of the build goes
    float monoGateSpan = 0.15f; // the last fraction of the travel it happens over
    float bassMonoHz = 120.0f;
    float mix = 1.0f;           // global dry/wet across the whole chain
    float outputGain = 1.0f;

    // --- driving it (audio thread) ----------------------------------------
    void setTransport (double bpm, double ppq, bool playing);
    void setPosition (float t) { pos = clampf (t, 0.0f, 1.0f); }
    float position() const { return pos; }

    void armArrival()   { arrivalArmed = true; }
    void cancelArrival(){ arrivalArmed = false; }
    bool arrivalPending() const { return arrivalArmed; }
    bool arrived() const { return didArrive; }
    /*  arrived() is a STATE and clears itself when the slider falls back —
        with AUTO on RESET that is the same block it fired in, so nothing
        outside can observe it. The count is what a check can hold on to. */
    int  arrivalCount() const { return nArrivals; }

    void process (float* L, float* R, int n);

    // --- metering ----------------------------------------------------------
    double lufs() const { return meter.lufs(); }

    // resolve one slot's parameters at a given slider position — the bench
    // reads this directly to prove the score is obeyed
    void resolve (int slot, float t, float* out) const;

private:
    bool fieldActive() const;
    void runSubBlock (float* L, float* R, int n, int blockOffset);
    void runSubBlockRange (float* L, float* R, int from, int to, double subPpq);
    void fireArrival();
    void releaseSlot (int i);
    void rearmSlot (int i);

    double sr = 48000.0;
    int maxBlock = 512;

    std::array<SlotState, kSlots> st;
    std::array<std::atomic<Effect*>, kSlots> live {};
    std::array<int, kSlots> liveType {};
    std::vector<std::unique_ptr<Effect>> owned, retired;

    float pos = 0.0f;
    int subPhase = 0;   // absolute, so sub-block edges do not move with the host's buffer
    std::atomic<bool> arrivalArmed { false };
    bool didArrive = false;
    int  nArrivals = 0;
    std::array<bool, kSlots> released {}, wasIn {};

    // transport
    /*  The clock is derived from an absolute sample count, never accumulated
        (see setTransport). ppqOrigin is re-seated only when the host actually
        disagrees — a jump, or a tempo change. */
    double bpm = 120.0, ppq = 0.0, ppqPerSample = 0.0;
    double ppqOrigin = 0.0;
    long long samplesSinceOrigin = 0;
    bool playing = false, haveClock = false, needResync = true;

    // stepped-parameter adoption, §3
    std::array<std::array<float, kMaxParams>, kSlots> stepped {};
    std::array<std::array<bool, kMaxParams>, kSlots> steppedValid {};

    // stereo and output
    WidthStage width;
    BassMono bass;
    Smooth widthSm, mixSm, outSm;

    // the impact ARRIVAL fires, §4 — not a slot, because it happens once
    double impPhase = 0.0, impEnv = 0.0, impDecay = 0.0, impFreq = 48.0, impLevel = 0.0;
    Rng impRng;

    LoudnessMeter meter;
    std::vector<float> scratchL, scratchR, msKeep, dryL, dryR;
};

} // namespace rop
