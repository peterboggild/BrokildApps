#pragma once

// HATS OFF — the parameter table, in ONE place (the Hairfryer SPECS[] pattern,
// as in Kickstart and Snare Tactics). The host layout, the engine, the
// presets, the panel and the bench all walk this table.
//
// A cymbal is a bronze plate with hundreds of inharmonic modes, energy that
// flows from its low modes into its high ones after the hit, and a sound that
// depends on where it is struck. A hi-hat is two of them and a pedal. The
// controls are those axes:
//
//   METAL  PITCH, SIZE, BRONZE (the 808's six squares <-> a cast plate),
//          DENSITY, BLOOM (the energy cascade), TRASH (the china), DECAY, NOISE
//   HAT    OPEN (closed hat <-> free cymbal), SIZZLE, CHICK, CHOKE
//   HIT    STRIKE (bell <-> bow <-> edge), STICK
//   EQ     CUT, AIR
//   ERA    GRIT, ROOM, WIDTH
//   ECHO   ECHO, TIME
//   SHAPE  ATTACK, SUSTAIN, DRIVE, ENGINE, COLOUR, COMP, SPEED
//   OUT    LEVEL, VELO, KEYS

#include <cstddef>

namespace ho
{

struct Params
{
    // METAL
    float pitch   = 0.0f;     // semitones
    float size    = 14.0f;    // inches
    float bronze  = 1.0f;     // 0 circuit (808) .. 1 cast bronze
    float density = 0.6f;     // how many modes: bell-like .. wash
    float bloom   = 0.2f;     // the energy cascade: 0 none .. 1 a crash's swell
    float trash   = 0.0f;     // china
    float decay   = 1500.0f;  // ms, the free cymbal to -60 dB (near 1 kHz)
    float noise   = 0.0f;     // a noise layer (CR-78, 606, the 909's air)
    // HAT
    float open    = 0.15f;    // 0 closed hat .. 1 a free cymbal
    float sizzle  = 0.3f;     // plates touching (half-open) or rivets (open)
    float chick   = 0.5f;     // the foot alone
    float choke   = 1.0f;     // OFF / ON: a closed hit chokes a ringing one
    // HIT
    float strike  = 0.5f;     // 0 bell, .5 bow, 1 edge
    float stick   = 0.35f;
    // EQ
    float cut     = 250.0f;   // Hz, high-pass
    float air     = 0.8f;     // 0 dark .. 1 open (exactly bypassed)
    // ERA
    float grit    = 0.0f;
    float room    = 0.0f;
    float width   = 0.3f;
    // ECHO
    float echo    = 0.0f;
    float time    = 3.0f;
    // SHAPE
    float attack  = 0.0f;
    float sustain = 0.0f;
    float drive   = 0.0f;
    float engine  = 0.0f;
    float colour  = 1.0f;
    float comp    = 0.0f;
    float speed   = 0.5f;
    // OUT
    float level   = 0.0f;
    float velo    = 0.5f;
    float keys    = 1.0f;     // 0 FIXED, 1 GM KIT, 2 CHROMATIC
};

enum Kind { K_FLOAT = 0, K_CHOICE };

struct PSpec
{
    const char* id;
    const char* name;
    const char* label;
    const char* unit;       // "st" "in" "ms" "Hz" "%" "dB" "bi" ""
    float lo, hi, def;
    float centre;
    int   kind;
    const char* choices;
    float Params::* member;
    const char* section;
    const char* hint;
};

inline const PSpec* specs()
{
    static const PSpec s[] =
    {
        { "pitch",  "Pitch",   "PITCH",  "st", -24.0f, 24.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::pitch,   "METAL",
          "Tunes the whole cymbal up or down, in semitones." },
        { "size",   "Size",    "SIZE",   "in",  8.0f,  24.0f, 14.0f,  14.0f, K_FLOAT, nullptr, &Params::size,    "METAL",
          "The cymbal's diameter: an 8-inch splash to a 24-inch ride. Bigger is lower, longer and denser." },
        { "bronze", "Bronze",  "BRONZE", "%",   0.0f,   1.0f,  1.0f,   0.0f, K_FLOAT, nullptr, &Params::bronze,  "METAL",
          "0: the 808's six square waves through a band-pass. 100: a cast bronze plate, hundreds of modes." },
        { "density","Density", "DENSITY","%",   0.0f,   1.0f,  0.6f,   0.0f, K_FLOAT, nullptr, &Params::density, "METAL",
          "How many modes ring: few is bell-like and tonal, many is a wash. On the circuit, how far apart the squares sit." },
        { "bloom",  "Bloom",   "BLOOM",  "%",   0.0f,   1.0f,  0.2f,   0.0f, K_FLOAT, nullptr, &Params::bloom,   "METAL",
          "The energy cascade: after the hit, energy flows up into the high modes and the cymbal gets brighter. A crash's swell." },
        { "trash",  "Trash",   "TRASH",  "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::trash,   "METAL",
          "The china: a rough, trashy, upturned edge." },
        { "decay",  "Decay",   "DECAY",  "ms", 30.0f, 8000.0f, 1500.0f, 800.0f, K_FLOAT, nullptr, &Params::decay, "METAL",
          "How long the free cymbal rings to -60 dB. A closed hat is much shorter: that is OPEN's job." },
        { "noise",  "Noise",   "NOISE",  "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::noise,   "METAL",
          "A noise layer: the CR-78's and 606's hats, and the air of a 909." },
        { "open",   "Open",    "OPEN",   "%",   0.0f,   1.0f,  0.15f,  0.0f, K_FLOAT, nullptr, &Params::open,    "HAT",
          "The pedal: 0 a closed hi-hat, a little open is loose, 100 a free cymbal (ride, crash, china)." },
        { "sizzle", "Sizzle",  "SIZZLE", "%",   0.0f,   1.0f,  0.3f,   0.0f, K_FLOAT, nullptr, &Params::sizzle,  "HAT",
          "Rattle: hat plates touching when half-open, or rivets in a free cymbal." },
        { "chick",  "Chick",   "CHICK",  "%",   0.0f,   1.0f,  0.5f,   0.0f, K_FLOAT, nullptr, &Params::chick,   "HAT",
          "The foot alone: the plates clapped shut (the pedal note, 44)." },
        { "choke",  "Choke",   "CHOKE",  "",    0.0f,   1.0f,  1.0f,   0.0f, K_CHOICE, "OFF|ON", &Params::choke, "HAT",
          "On: a closed or pedal hit chokes whatever is still ringing, as closing a real hi-hat does." },
        { "strike", "Strike",  "STRIKE", "%",   0.0f,   1.0f,  0.5f,   0.0f, K_FLOAT, nullptr, &Params::strike,  "HIT",
          "Where the stick lands: the bell (a ping), the bow (a ride), the edge (a crash)." },
        { "stick",  "Stick",   "STICK",  "%",   0.0f,   1.0f,  0.35f,  0.0f, K_FLOAT, nullptr, &Params::stick,   "HIT",
          "The stick's own tick: the definition of a ride." },
        { "cut",    "Cut",     "CUT",    "Hz", 20.0f, 8000.0f, 250.0f, 600.0f, K_FLOAT, nullptr, &Params::cut,    "EQ",
          "High-pass. Up to thin a cymbal to a tick." },
        { "air",    "Air",     "AIR",    "%",   0.0f,   1.0f,  0.8f,   0.0f, K_FLOAT, nullptr, &Params::air,     "EQ",
          "The top end. Fully open is no filter at all." },
        { "grit",   "Grit",    "GRIT",   "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::grit,    "ERA",
          "Lower sample rate and bit depth: the 909's 6-bit cymbals near 60 %, the SP-1200 near 30 %." },
        { "room",   "Room",    "ROOM",   "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::room,    "ERA",
          "A room: more is bigger as well as louder." },
        { "width",  "Width",   "WIDTH",  "%",   0.0f,   1.0f,  0.3f,   0.0f, K_FLOAT, nullptr, &Params::width,   "ERA",
          "Spreads the cymbal's modes across the stereo field, as overheads hear it. 0 is mono." },
        { "echo",   "Echo",    "ECHO",   "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::echo,    "ECHO",
          "A dub tape echo, ping-pong: more send and more feedback together." },
        { "time",   "Echo time","TIME",  "",    0.0f,   6.0f,  3.0f,   0.0f, K_CHOICE,
          "1/16|1/8T|1/8|1/8D|1/4|1/4D|1/2", &Params::time, "ECHO",
          "Echo time, locked to the host tempo (120 BPM on its own)." },
        { "attack", "Attack",  "ATTACK", "bi", -1.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::attack,  "SHAPE",
          "Transient: more or less of the first few milliseconds." },
        { "sustain","Sustain", "SUSTAIN","bi", -1.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::sustain, "SHAPE",
          "Transient: more or less of the wash." },
        { "drive",  "Drive",   "DRIVE",  "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::drive,   "SHAPE",
          "Four engines from Battlestar Overdrive, level-matched by measurement." },
        { "engine", "Engine",  "ENGINE", "",    0.0f,   3.0f,  0.0f,   0.0f, K_CHOICE,
          "IDLE BURN|HYPERDRIVE|RAZOR WING|SUPERNOVA", &Params::engine, "SHAPE",
          "IDLE BURN warm tube, HYPERDRIVE harmonics, RAZOR WING hard clip, SUPERNOVA crushed." },
        { "colour", "Colour",  "COLOUR", "%",   0.0f,   1.0f,  1.0f,   0.0f, K_FLOAT, nullptr, &Params::colour,  "SHAPE",
          "The drive's low-pass. Fully open is no filter at all." },
        { "comp",   "Comp",    "COMP",   "%",   0.0f,   1.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::comp,    "SHAPE",
          "Threshold and ratio on one knob, with its own make-up gain." },
        { "speed",  "Speed",   "SPEED",  "%",   0.0f,   1.0f,  0.5f,   0.0f, K_FLOAT, nullptr, &Params::speed,   "SHAPE",
          "Slow lets the stick through; fast brings the wash up to meet it." },
        { "level",  "Level",   "LEVEL",  "dB", -24.0f,  6.0f,  0.0f,   0.0f, K_FLOAT, nullptr, &Params::level,   "OUT",
          "Output. A soft ceiling holds everything under 0 dBFS." },
        { "velo",   "Velocity","VELO",   "%",   0.0f,   1.0f,  0.5f,   0.0f, K_FLOAT, nullptr, &Params::velo,    "OUT",
          "How much harder hits are louder and brighter." },
        { "keys",   "Keys",    "KEYS",   "",    0.0f,   2.0f,  1.0f,   0.0f, K_CHOICE,
          "FIXED|GM KIT|CHROMATIC", &Params::keys, "OUT",
          "FIXED: every note as dialled. GM KIT: 42 closed, 44 pedal, 46 open, 53 bell, 49 52 55 57 crash. CHROMATIC: F#1 = PITCH." },
    };
    return s;
}

constexpr int kNumParams = 31;

enum { ENG_IDLE = 0, ENG_HYPER, ENG_RAZOR, ENG_NOVA, NUM_DRIVE_ENGINES };
enum { KEYS_FIXED = 0, KEYS_GM, KEYS_CHROMATIC };
enum Artic { ART_DIALLED = 0, ART_CLOSED, ART_PEDAL, ART_OPEN, ART_BELL, ART_EDGE, NUM_ARTICS };

constexpr int kNumTimes = 7;
constexpr float kTimeBeats[kNumTimes] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

} // namespace ho
