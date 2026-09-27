#pragma once

// SNARE TACTICS — the parameter table, in ONE place (the Hairfryer SPECS[]
// pattern, as in Kickstart). The host layout, the engine, the presets, the
// panel and the bench all walk this table, so nothing names a parameter twice.
//
// A snare is three coupled sources, and the controls are their physical axes:
//
//   HEAD   the batter head: TUNE, a pitch DROP and how fast it BENDs back,
//          DECAY, the electronic pair's WAVE (sine -> triangle -> square),
//          SKIN from the 808/909 two-oscillator body to a real membrane's
//          Bessel modes, and RING (how undamped the head is).
//   WIRES  the snares under the resonant head: WIRES level, SIZZLE (their
//          decay), TENSION (loose: dark, long, buzzing at the head's pitch,
//          lagging the stick, ringing along with the head; tight: bright,
//          short, crisp) and AIR (their top end).
//   HIT    STRIKE position (centre -> edge -> rim shot), the STICK's click,
//          the CLAP layer and the SPREAD of its bursts.
//   ERA    GRIT (the sample machines), ROOM, and the 80s GATE that cuts it.
//   ECHO   a dub tape echo, ping-pong, TIME synced to the host tempo.
//   SHAPE  transient ATTACK / SUSTAIN, DRIVE through four Battlestar
//          Overdrive engines, COLOUR, COMP and its SPEED.
//   OUT    LEVEL, VELOCITY sensitivity, and KEYS (FIXED / GM KIT / CHROMATIC).

#include <cstddef>

namespace st
{

struct Params
{
    // HEAD
    float tune    = 185.0f;   // Hz, the head's settled fundamental
    float drop    = 5.0f;     // semitones the pitch starts above it
    float bend    = 18.0f;    // ms, time constant of the pitch fall
    float decay   = 260.0f;   // ms, the body to -60 dB
    float wave    = 0.0f;     // electronic pair: 0 sine, .5 triangle, 1 square
    float skin    = 0.5f;     // 0 two oscillators .. 1 membrane modes
    float ring    = 0.35f;    // 0 damped (gel, tape) .. 1 wide open
    float body    = 0.8f;     // the head's level (0.8 = unity)
    // WIRES
    float wires   = 0.6f;
    float sizzle  = 300.0f;   // ms, the wires to -60 dB
    float tension = 0.55f;    // 0 loose .. 1 tight
    float air     = 0.7f;     // 0 dark .. 1 open
    // HIT
    float strike  = 0.25f;    // 0 centre, .7 edge, 1 rim shot
    float stick   = 0.35f;
    float clap    = 0.0f;
    float spread  = 0.45f;    // clap burst spacing
    // ERA
    float grit    = 0.0f;
    float room    = 0.0f;
    float gate    = 0.0f;     // 0 off; else the room is cut after 40..600 ms
    // ECHO
    float echo    = 0.0f;
    float time    = 3.0f;     // 1/16 1/8T 1/8 1/8D 1/4 1/4D 1/2
    // SHAPE
    float attack  = 0.0f;
    float sustain = 0.0f;
    float drive   = 0.0f;
    float engine  = 0.0f;     // 0 IDLE BURN, 1 HYPERDRIVE, 2 RAZOR WING, 3 SUPERNOVA
    float colour  = 1.0f;     // 0 dark .. 1 open (exactly bypassed)
    float comp    = 0.0f;
    float speed   = 0.5f;
    // OUT
    float level   = 0.0f;     // dB
    float velo    = 0.5f;
    float keys    = 1.0f;     // 0 FIXED, 1 GM KIT, 2 CHROMATIC
};

enum Kind { K_FLOAT = 0, K_CHOICE };

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
        { "tune",   "Tune",    "TUNE",   "Hz", 100.0f, 420.0f, 185.0f, 190.0f, K_FLOAT, nullptr, &Params::tune,   "HEAD",
          "The head's pitch once it settles. The note name is shown in the display." },
        { "drop",   "Drop",    "DROP",   "st",  0.0f,  24.0f,   5.0f,   6.0f, K_FLOAT, nullptr, &Params::drop,   "HEAD",
          "How far above TUNE the hit starts. A little is a real head; a lot is a zap." },
        { "bend",   "Bend",    "BEND",   "ms",  2.0f, 200.0f,  18.0f,  20.0f, K_FLOAT, nullptr, &Params::bend,   "HEAD",
          "How fast the pitch falls back to TUNE." },
        { "decay",  "Decay",   "DECAY",  "ms", 40.0f, 2000.0f, 260.0f, 300.0f, K_FLOAT, nullptr, &Params::decay, "HEAD",
          "Time until the head is 60 dB down." },
        { "wave",   "Wave",    "WAVE",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::wave,   "HEAD",
          "The electronic pair's wave: sine (808), triangle (909), square." },
        { "skin",   "Skin",    "SKIN",   "%",   0.0f,   1.0f,   0.5f,   0.0f, K_FLOAT, nullptr, &Params::skin,   "HEAD",
          "0: two oscillators, the 808/909 body. 100: a real membrane, its modes set by where it is struck." },
        { "ring",   "Ring",    "RING",   "%",   0.0f,   1.0f,  0.35f,   0.0f, K_FLOAT, nullptr, &Params::ring,   "HEAD",
          "How undamped the head is. Low: gel and tape. High: the overtone ring left wide open." },
        { "body",   "Body",    "BODY",   "%",   0.0f,   1.0f,   0.8f,   0.0f, K_FLOAT, nullptr, &Params::body,   "HEAD",
          "How much of the head you hear. Down to nothing for a clap, or wires alone." },
        { "wires",  "Wires",   "WIRES",  "%",   0.0f,   1.0f,   0.6f,   0.0f, K_FLOAT, nullptr, &Params::wires,  "WIRES",
          "The snare wires. The 808 called this SNAPPY." },
        { "sizzle", "Sizzle",  "SIZZLE", "ms", 30.0f, 2000.0f, 300.0f, 300.0f, K_FLOAT, nullptr, &Params::sizzle, "WIRES",
          "Time until the wires are 60 dB down." },
        { "tension","Tension", "TENSION","%",   0.0f,   1.0f,  0.55f,   0.0f, K_FLOAT, nullptr, &Params::tension, "WIRES",
          "Loose: dark, long, buzzing at the head's pitch. Tight: bright, short and crisp." },
        { "air",    "Air",     "AIR",    "%",   0.0f,   1.0f,   0.7f,   0.0f, K_FLOAT, nullptr, &Params::air,    "WIRES",
          "The top end of the wires and the clap." },
        { "strike", "Strike",  "STRIKE", "%",   0.0f,   1.0f,  0.25f,   0.0f, K_FLOAT, nullptr, &Params::strike, "HIT",
          "Where the stick lands: the centre, out toward the edge (more ring), then a rim shot." },
        { "stick",  "Stick",   "STICK",  "%",   0.0f,   1.0f,  0.35f,   0.0f, K_FLOAT, nullptr, &Params::stick,  "HIT",
          "The stick's own click." },
        { "clap",   "Clap",    "CLAP",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::clap,   "HIT",
          "A hand clap layered on the snare, 808 style." },
        { "spread", "Spread",  "SPREAD", "%",   0.0f,   1.0f,  0.45f,   0.0f, K_FLOAT, nullptr, &Params::spread, "HIT",
          "The clap's burst spacing. 0 is one tight burst; up from there, four hands." },
        { "grit",   "Grit",    "GRIT",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::grit,   "ERA",
          "Lower sample rate and bit depth: SP-1200 and Akai around 30 %, 8-bit around 55 %." },
        { "room",   "Room",    "ROOM",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::room,   "ERA",
          "A room: more is bigger as well as louder." },
        { "gate",   "Gate",    "GATE",   "ms",  0.0f, 600.0f,   0.0f, 200.0f, K_FLOAT, nullptr, &Params::gate,   "ERA",
          "The 80s gate: the room is cut this long after each hit. 0 is off." },
        { "echo",   "Echo",    "ECHO",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::echo,   "ECHO",
          "A dub tape echo, ping-pong: more send and more feedback together." },
        { "time",   "Echo time","TIME",  "",    0.0f,   6.0f,   3.0f,   0.0f, K_CHOICE,
          "1/16|1/8T|1/8|1/8D|1/4|1/4D|1/2", &Params::time, "ECHO",
          "Echo time, locked to the host tempo (120 BPM on its own)." },
        { "attack", "Attack",  "ATTACK", "bi", -1.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::attack,  "SHAPE",
          "Transient: more or less of the first few milliseconds." },
        { "sustain","Sustain", "SUSTAIN","bi", -1.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::sustain, "SHAPE",
          "Transient: more or less of the tail." },
        { "drive",  "Drive",   "DRIVE",  "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::drive,   "SHAPE",
          "Four engines from Battlestar Overdrive, level-matched by measurement." },
        { "engine", "Engine",  "ENGINE", "",    0.0f,   3.0f,   0.0f,   0.0f, K_CHOICE,
          "IDLE BURN|HYPERDRIVE|RAZOR WING|SUPERNOVA", &Params::engine, "SHAPE",
          "IDLE BURN warm tube, HYPERDRIVE harmonics, RAZOR WING hard clip, SUPERNOVA crushed." },
        { "colour", "Colour",  "COLOUR", "%",   0.0f,   1.0f,   1.0f,   0.0f, K_FLOAT, nullptr, &Params::colour,  "SHAPE",
          "The drive's low-pass. Fully open is no filter at all." },
        { "comp",   "Comp",    "COMP",   "%",   0.0f,   1.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::comp,    "SHAPE",
          "Threshold and ratio on one knob, with its own make-up gain." },
        { "speed",  "Speed",   "SPEED",  "%",   0.0f,   1.0f,   0.5f,   0.0f, K_FLOAT, nullptr, &Params::speed,   "SHAPE",
          "Slow lets the crack through; fast flattens it and brings the wires up." },
        { "level",  "Level",   "LEVEL",  "dB", -24.0f,  6.0f,   0.0f,   0.0f, K_FLOAT, nullptr, &Params::level,   "OUT",
          "Output. A soft ceiling holds everything under 0 dBFS." },
        { "velo",   "Velocity","VELO",   "%",   0.0f,   1.0f,   0.5f,   0.0f, K_FLOAT, nullptr, &Params::velo,    "OUT",
          "How much harder hits are louder, brighter and further from a ghost note." },
        { "keys",   "Keys",    "KEYS",   "",    0.0f,   2.0f,   1.0f,   0.0f, K_CHOICE,
          "FIXED|GM KIT|CHROMATIC", &Params::keys, "OUT",
          "FIXED: every note is the snare. GM KIT: 38 snare, 40 rim shot, 37 cross-stick, 39 clap. CHROMATIC: D1 = TUNE." },
    };
    return s;
}

constexpr int kNumParams = 31;

enum { ENG_IDLE = 0, ENG_HYPER, ENG_RAZOR, ENG_NOVA, NUM_DRIVE_ENGINES };
enum { KEYS_FIXED = 0, KEYS_GM, KEYS_CHROMATIC };
enum Artic { ART_SNARE = 0, ART_RIM, ART_XSTICK, ART_CLAP, NUM_ARTICS };

constexpr int kNumTimes = 7;
constexpr float kTimeBeats[kNumTimes] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

} // namespace st
