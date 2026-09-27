#pragma once

// KICKSTART — the parameter table, in ONE place (the Hairfryer SPECS[]
// pattern). The host layout, the engine, the factory presets, the panel and
// the bench all walk this table, so a knob cannot be wired to the wrong
// parameter: nothing names a parameter twice.
//
// The whole instrument is twenty-two controls in five sections. The idea is
// not one simulation per drum machine but a small set of PHYSICAL axes that
// every kick drum lives on:
//
//   BODY   a pitched resonator whose pitch falls (SWEEP / BEND), which dies
//          away (DECAY) with a shape running from a struck membrane's
//          exponential to a modern flat-then-gone sustain (CURVE), whose
//          wave runs sine -> triangle -> rounded square (WAVE), and which
//          grows the inharmonic modes and tension-glide of a real drum head
//          as SKIN comes up. 808, 909 and an acoustic kick are three points
//          in this one space.
//   HIT    the beater: CLICK level and its TONE, from a felt thud through a
//          wooden knock and the 909's noise click to a digital tick.
//   ERA    GRIT (the 8- and 12-bit sample machines: DMX, LinnDrum, SP-12,
//          MPC60) and ROOM (the air around a real kit, and the raw material
//          of a techno rumble once it hits the drive).
//   SHAPE  transient ATTACK / SUSTAIN, DRIVE through four of Battlestar
//          Overdrive's engines, COLOUR (the drive's own low-pass), COMP and
//          its SPEED.
//   OUT    LEVEL, VELOCITY sensitivity, KEY tracking.

#include <cstddef>

namespace ks
{

struct Params
{
    // BODY
    float pitch   = 52.0f;    // Hz, the settled fundamental
    float sweep   = 18.0f;    // semitones the pitch starts above it
    float bend    = 30.0f;    // ms, time constant of the pitch fall
    float decay   = 450.0f;   // ms to -60 dB
    float curve   = 0.15f;    // 0 exponential .. 1 held-then-gone
    float wave    = 0.0f;     // 0 sine, .5 triangle, 1 rounded square
    float skin    = 0.0f;     // 0 electronic .. 1 a real drum head
    // HIT
    float click   = 0.45f;
    float tone    = 0.55f;    // 0 felt thud .. 1 digital tick
    // ERA
    float grit    = 0.0f;     // sample-rate + bit-depth reduction
    float room    = 0.0f;
    // SHAPE
    float attack  = 0.0f;     // -1 .. +1 transient
    float sustain = 0.0f;     // -1 .. +1 transient
    float drive   = 0.0f;
    float engine  = 0.0f;     // 0 IDLE BURN, 1 HYPERDRIVE, 2 RAZOR WING, 3 SUPERNOVA
    float colour  = 1.0f;     // 0 dark .. 1 open (exactly bypassed)
    float comp    = 0.0f;
    float speed   = 0.5f;     // 0 slow (punch through) .. 1 fast (flatten)
    // OUT
    float level   = 0.0f;     // dB
    float velo    = 0.5f;     // velocity sensitivity
    float key     = 0.0f;     // key tracking on/off
};

enum Kind { K_FLOAT = 0, K_CHOICE, K_BOOL };

struct PSpec
{
    const char* id;
    const char* name;       // host name
    const char* label;      // panel label
    const char* unit;       // "Hz" "st" "ms" "%" "dB" "bi" (bipolar %) ""
    float lo, hi, def;
    float centre;           // skew so this value sits at mid-travel (0 = linear)
    int   kind;
    const char* choices;    // "A|B|C" for K_CHOICE
    float Params::* member;
    const char* section;
    const char* hint;
};

inline const PSpec* specs()
{
    static const PSpec s[] =
    {
        { "pitch",  "Pitch",   "PITCH",  "Hz", 25.0f, 160.0f,  52.0f,  55.0f, K_FLOAT, nullptr, &Params::pitch,   "BODY",
          "Where the kick settles. The note name is shown beside it." },
        { "sweep",  "Sweep",   "SWEEP",  "st",  0.0f,  48.0f,  18.0f,  12.0f, K_FLOAT, nullptr, &Params::sweep,   "BODY",
          "How far above PITCH the hit starts. 808 a little, 909 a lot, gabber a lot more." },
        { "bend",   "Bend",    "BEND",   "ms",  2.0f, 300.0f,  30.0f,  30.0f, K_FLOAT, nullptr, &Params::bend,    "BODY",
          "How fast the pitch falls. Short is a snap, long is a zap." },
        { "decay",  "Decay",   "DECAY",  "ms", 50.0f, 4000.0f, 450.0f, 450.0f, K_FLOAT, nullptr, &Params::decay,  "BODY",
          "Time until the body is 60 dB down." },
        { "curve",  "Curve",   "CURVE",  "%",   0.0f,   1.0f,  0.15f,  0.0f,  K_FLOAT, nullptr, &Params::curve,   "BODY",
          "0: a struck drum's exponential. 100: held full, then gone - the modern sub." },
        { "wave",   "Wave",    "WAVE",   "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::wave,    "BODY",
          "Sine, through triangle (the 909), to a rounded square." },
        { "skin",   "Skin",    "SKIN",   "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::skin,    "BODY",
          "From an electronic oscillator to a real drum head: membrane modes, and a pitch that rises the harder you hit." },
        { "click",  "Click",   "CLICK",  "%",   0.0f,   1.0f,  0.45f,  0.0f,  K_FLOAT, nullptr, &Params::click,   "HIT",
          "The beater." },
        { "tone",   "Tone",    "TONE",   "%",   0.0f,   1.0f,  0.55f,  0.0f,  K_FLOAT, nullptr, &Params::tone,    "HIT",
          "Felt thud, wooden knock, the 909's noise click, a digital tick." },
        { "grit",   "Grit",    "GRIT",   "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::grit,    "ERA",
          "Lower sample rate and bit depth: 12-bit SP-12 and Linn 9000 around 30 %, 8-bit DMX around 55 %." },
        { "room",   "Room",    "ROOM",   "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::room,    "ERA",
          "The air around a real kit. Into DRIVE it becomes a techno rumble." },
        { "attack", "Attack",  "ATTACK", "bi", -1.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::attack,  "SHAPE",
          "Transient: more or less of the first few milliseconds." },
        { "sustain","Sustain", "SUSTAIN","bi", -1.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::sustain, "SHAPE",
          "Transient: more or less of the tail." },
        { "drive",  "Drive",   "DRIVE",  "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::drive,   "SHAPE",
          "Four engines from Battlestar Overdrive, level-matched by measurement." },
        { "engine", "Engine",  "ENGINE", "",    0.0f,   3.0f,  0.0f,   0.0f,  K_CHOICE,
          "IDLE BURN|HYPERDRIVE|RAZOR WING|SUPERNOVA", &Params::engine, "SHAPE",
          "IDLE BURN warm tube, HYPERDRIVE harmonics, RAZOR WING hard clip, SUPERNOVA crushed." },
        { "colour", "Colour",  "COLOUR", "%",   0.0f,   1.0f,  1.0f,   0.0f,  K_FLOAT, nullptr, &Params::colour,  "SHAPE",
          "The drive's low-pass. Fully open is no filter at all." },
        { "comp",   "Comp",    "COMP",   "%",   0.0f,   1.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::comp,    "SHAPE",
          "Threshold and ratio on one knob, with its own make-up gain." },
        { "speed",  "Speed",   "SPEED",  "%",   0.0f,   1.0f,  0.5f,   0.0f,  K_FLOAT, nullptr, &Params::speed,   "SHAPE",
          "Slow lets the hit punch through; fast flattens and pumps." },
        { "level",  "Level",   "LEVEL",  "dB", -24.0f,  6.0f,  0.0f,   0.0f,  K_FLOAT, nullptr, &Params::level,   "OUT",
          "Output. A soft ceiling holds everything under 0 dBFS." },
        { "velo",   "Velocity","VELO",   "%",   0.0f,   1.0f,  0.5f,   0.0f,  K_FLOAT, nullptr, &Params::velo,    "OUT",
          "How much a hard hit is louder, brighter and bends further." },
        { "key",    "Key track","KEY",   "",    0.0f,   1.0f,  0.0f,   0.0f,  K_BOOL,  nullptr, &Params::key,     "OUT",
          "On: the MIDI note sets the pitch, C1 = PITCH. Off: every note plays the same kick." },
    };
    return s;
}

constexpr int kNumParams = 21;

enum { ENG_IDLE = 0, ENG_HYPER, ENG_RAZOR, ENG_NOVA, NUM_DRIVE_ENGINES };

} // namespace ks
