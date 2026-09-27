#pragma once

/*  THE MODULATION MATRIX — every specimen wired differently.

    Peter's brief: a rosette control, turned up, should set three or four OTHER
    controls moving — oscillating, or oscillating faster — and the wiring should
    be unique to each entry in the catalogue. A TEMPERATURE governs how much of
    it happens at all.

    ------------------------------------------------------------------------
    WHY THERE CANNOT BE A FEEDBACK LOOP

    Not because the wiring is checked for cycles, but because a cycle cannot be
    expressed. A wire's SOURCE reads the stored parameter — the knob position,
    what the host and the panel hold — and its DESTINATION is written into a
    separate effective array that nothing reads back. One hop, always. Even a
    matrix wiring A to B and B to A is then simply two independent wires, and
    the `apply` signature is what enforces it: `const float* stored` in, `float*
    eff` out, and no path from eff to stored.

    That also happens to be exactly what was asked for. "Increasing one of these
    rosette controls can mean that 3-4 other parameters oscillate" describes a
    single hop from a knob position, not a network.

    Bounded excursion is kept as well, belt and braces: the total offset on any
    destination is clamped to a fraction of its range before it is applied.

    ------------------------------------------------------------------------
    TEMPERATURE, and the two tiers

    77 K, liquid nitrogen: nothing moves. Not "almost nothing" — the offsets are
    skipped entirely, so a cold instrument is bit-identical to one built before
    any of this existed. The bench checks that by memcmp.

    293 K, room temperature and the default: the material and voice controls
    breathe, about nine per cent of their range.

    800 K: thirty per cent, and the SCENE controls join in.

    The scene controls are TRAVERSE — which is the dimensional cross-section
    itself — and CUT BEARING and CUT OFFSET, which are the sounding line. Peter:
    "major scene controls should not be affected. I think. Maybe at very high
    temperatures." So they are held at zero until 500 K and reach a smaller
    maximum, twelve per cent, at 800 K. What the player has framed stays framed
    until the thing is genuinely too hot to hold.  */

#include "Engine.h"

namespace ax
{

//==============================================================================
constexpr float TEMP_MIN  = 77.0f;      // liquid nitrogen — frozen
constexpr float TEMP_ROOM = 293.0f;     // the default
constexpr float TEMP_MAX  = 800.0f;

constexpr float TEMP_SCENE_ONSET = 500.0f;   // below this the scene is untouched
constexpr float EXC_ORDINARY     = 0.30f;    // of range, at TEMP_MAX
constexpr float EXC_SCENE        = 0.12f;    // of range, at TEMP_MAX

/*  0 = never modulated, 1 = ordinary, 2 = a scene control.

    The never-modulated list is not squeamishness. LEVEL and CEILING would make
    the instrument breathe in loudness, which is a fault and not a feature;
    CONFORMANCE, REFERENCE, REGISTER and BEND RANGE would detune a held note;
    HABIT would change the specimen underneath its own matrix; EXTENT and ORDERS
    are stepped and rebuilding the body to a different SIZE thirty times a
    second is expensive and audible; APERTURES and GLIDE are voice allocation;
    and TEMPERATURE cannot modulate itself. */
int modTier (const char* id);

//==============================================================================
struct ModWire
{
    int   src = 0, dst = 0;
    int   kind = 0;            // 0: the source sets the DEPTH; 1: it sets the RATE
    float depth = 0.5f;        // fraction of the destination's allowance
    float rate = 0.1f;         // Hz at a source of one half
    float phase = 0.0f;        // where in its cycle this wire starts
};

struct ModMatrix
{
    int n = 0;
    ModWire w[16];
};

/*  Deterministic, and different for every entry in the catalogue. Four sources,
    each fanning out to two or three destinations, which is what "increasing one
    control makes three or four others oscillate" means in practice. */
ModMatrix modMatrixFor (int habit);

//==============================================================================
class Modulator
{
public:
    void  setHabit (int h);
    void  advance (double dt, const float* stored);

    /*  stored -> eff. Never the other way; see the note at the top of this
        file. `eff` may be the same length as the parameter table and is fully
        written. At TEMP_MIN this is a straight copy, and is meant to be. */
    void  apply (const float* stored, float* eff, float kelvin) const;

    const ModMatrix& matrix() const { return m; }
    int   habitOf() const { return habit; }

    /*  What the panel draws: how far each destination is currently pushed, as
        a signed fraction of its allowance. */
    float displacement (int paramIndex, const float* stored, float kelvin) const;

private:
    int       habit = -1;
    ModMatrix m;
    float     ph[16] {};
};

} // namespace ax
