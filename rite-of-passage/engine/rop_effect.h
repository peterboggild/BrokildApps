#pragma once

// RITE OF PASSAGE — the effect interface, and the declarations §8 holds
// every effect to.
//
// Self-describing, the house pattern (BWFX's descriptors, Hairfryer's
// SPECS[]): the score, the state format, the panel and the bench are all
// generated from these tables, so the wired-to-the-wrong-knob bug class
// cannot be expressed. What is new here is that a descriptor also declares
// what the effect is ALLOWED TO DO to level and pitch, and the bench reads
// those declarations and tests against them.

#include "rop_dsp.h"

namespace rop
{

// §8.1 — what this effect may do to loudness
enum class Level : uint8_t
{
    Neutral,      // must not move loudness at all as its parameters move
    Spectral,     // loudness follows what was removed, and nothing else
    Intentional   // the level change IS the effect
};

// §4 — what ARRIVAL does to a slot
enum class Tail : uint8_t { Bypass = 0, Spill, Clear };

// §6 — where in the field a slot works
enum class Place : uint8_t { Stereo = 0, Mid, Side, Left, Right };

// how a parameter travels between A and B
enum class Warp : uint8_t { Linear = 0, Log };

struct ParamDesc
{
    const char* id;          // state key, stable forever
    const char* name;        // label
    float def, lo, hi;
    float step;              // 0 = continuous, else the grid (1 = stepped)
    const char* unit;
    Warp  warp = Warp::Linear;
    const char* choices = nullptr;   // "LP|BP|HP" for stepped choices

    /*  §8.1 at parameter granularity. true means "this knob is ALLOWED to
        move the loudness, because moving it is what the knob is for" — a
        delay's MIX, a riser's LEVEL, a filter's CUTOFF. The bench sweeps
        every OTHER parameter of a NEUTRAL or SPECTRAL effect and holds the
        loudness to +-1 dB. Declaring it per effect was not fine-grained
        enough: CLIMB's cutoff is supposed to change the level and its
        resonance is not, and that distinction is the whole claim. */
    bool levelKnob = false;
};

struct EffectDesc
{
    const char* id;
    const char* name;
    const char* sub;
    const ParamDesc* params;
    int   numParams;

    Level level;
    bool  movesPitch;          // §8.2: false means the bench holds it to ±2 cents
    bool  generator;           // sums in rather than processing what arrives
    bool  perChannelParams;    // honours SPREAD's two parameter sets (§6)

    /*  §8.2. true means this effect RE-SEQUENCES TIME — a sine in is not a
        sine out, because slices are repeated or reversed, so the bench's
        pitch meter does not apply to it. Its exactness is tested
        structurally instead (the loop must read at exactly rate 1), which is
        a stronger test than a frequency measurement anyway. */
    bool  reordersTime = false;
    int   latency = 0;
};

// Everything an effect needs to know about the world, per sub-block.
struct Ctx
{
    double fs = 48000.0;
    double bpm = 0.0;            // 0 = no host clock; behave as if free
    double ppq = -1.0;           // at the start of this sub-block; <0 unknown
    double ppqPerSample = 0.0;
    bool   playing = false;
    float  t = 0.0f;             // the master slider
    float  u = 0.0f;             // this slot's own position after curve and depth
    bool   inLane = false;
};

class Effect
{
public:
    virtual ~Effect() = default;

    virtual const EffectDesc& desc() const = 0;

    virtual void prepare (double fs, int maxBlock) = 0;   // message thread
    virtual void reset() = 0;                             // audio thread, no alloc

    /*  Audio thread. pL and pR are the resolved parameter values for this
        sub-block — two sets, because SPREAD runs the two channels a few
        percent apart on the score (§6). An effect that declares
        perChannelParams = false may ignore pR. */
    virtual void process (float* L, float* R, int n,
                          const float* pL, const float* pR, const Ctx& c) = 0;

    // §9 — capture and release
    virtual void arm() {}                     // the slot has entered its lane
    virtual void release (Tail) {}            // ARRIVAL, or leaving the lane
    virtual bool ringing() const { return false; }   // still has a tail to give
};

// The registry. Only IMPLEMENTED effects appear; the twelve of the design doc
// arrive one at a time and a saved rite naming one that is not here yet
// leaves its slot empty rather than failing to load.
int numEffects();
const EffectDesc& effectDescriptor (int type);
int effectTypeByName (const char* id);        // -1 when unknown
Effect* createEffect (int type);              // message thread; caller owns

} // namespace rop
