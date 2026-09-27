#include "st_presets.h"

#include <cstring>

namespace st
{

namespace
{
#define ST_VALUES(name, ...) const PresetValue name[] = { __VA_ARGS__ };
#define ST_N(name) (int) (sizeof (name) / sizeof (name[0]))

    // ---- ACOUSTIC: SKIN up, the wires doing what wires do, a stick, air -----
    ST_VALUES (vStudio,  {"level",1.6f},{"tune",190},{"drop",3},{"bend",12},{"decay",280},{"skin",0.9f},{"ring",0.35f},
               {"wires",0.6f},{"sizzle",320},{"tension",0.55f},{"air",0.75f},{"strike",0.3f},{"stick",0.4f},
               {"room",0.2f},{"attack",0.2f},{"comp",0.35f},{"speed",0.6f})
    ST_VALUES (vPiccolo, {"level",0.9f},{"tune",290},{"drop",2},{"bend",8},{"decay",200},{"skin",0.9f},{"ring",0.55f},
               {"wires",0.55f},{"sizzle",210},{"tension",0.85f},{"air",0.9f},{"strike",0.35f},{"stick",0.55f},
               {"attack",0.3f},{"comp",0.3f},{"speed",0.7f})
    ST_VALUES (vSeventies,{"level",4.1f},{"tune",150},{"drop",2},{"bend",10},{"decay",170},{"skin",0.85f},{"ring",0.0f},
               {"wires",0.45f},{"sizzle",230},{"tension",0.35f},{"air",0.45f},{"strike",0.15f},{"stick",0.3f},
               {"colour",0.7f},{"comp",0.4f},{"speed",0.6f})
    ST_VALUES (vRockRim, {"level",2.2f},{"tune",200},{"drop",4},{"bend",10},{"decay",340},{"skin",0.85f},{"ring",0.6f},
               {"wires",0.6f},{"sizzle",380},{"tension",0.5f},{"air",0.8f},{"strike",0.95f},{"stick",0.6f},
               {"room",0.25f},{"drive",0.15f},{"comp",0.45f},{"speed",0.7f})
    ST_VALUES (vJazz,    {"level",-5.4f},{"tune",230},{"drop",2},{"decay",380},{"skin",0.95f},{"ring",0.75f},
               {"wires",0.5f},{"sizzle",520},{"tension",0.3f},{"air",0.6f},{"strike",0.5f},{"stick",0.2f},
               {"room",0.3f},{"velo",0.8f})
    ST_VALUES (vGarage,  {"level",-1.4f},{"tune",165},{"drop",3},{"decay",360},{"skin",0.85f},{"ring",0.7f},
               {"wires",0.75f},{"sizzle",700},{"tension",0.12f},{"air",0.55f},{"strike",0.4f},{"stick",0.35f},
               {"room",0.3f},{"drive",0.2f})
    ST_VALUES (vEighties,{"level",5.3f},{"tune",175},{"drop",5},{"bend",14},{"decay",420},{"skin",0.85f},{"ring",0.5f},
               {"wires",0.65f},{"sizzle",420},{"tension",0.5f},{"air",0.75f},{"strike",0.6f},{"stick",0.5f},
               {"room",0.75f},{"gate",240},{"comp",0.55f},{"speed",0.75f})

    // ---- MACHINES: the analogue boxes and the sample machines -----------------
    ST_VALUES (v808,     {"level",-2.8f},{"tune",180},{"drop",2},{"bend",8},{"decay",180},{"skin",0},{"wave",0},
               {"wires",0.55f},{"sizzle",220},{"tension",0.75f},{"air",0.65f},{"strike",0},{"stick",0})
    ST_VALUES (v808s,    {"level",-4.7f},{"tune",185},{"drop",2},{"bend",8},{"decay",150},{"skin",0},{"wave",0},
               {"wires",0.85f},{"sizzle",320},{"tension",0.8f},{"air",0.8f},{"strike",0},{"stick",0})
    ST_VALUES (v909,     {"level",2.6f},{"tune",185},{"drop",7},{"bend",10},{"decay",190},{"skin",0},{"wave",0.5f},
               {"wires",0.65f},{"sizzle",260},{"tension",0.85f},{"air",0.9f},{"strike",0},{"stick",0.15f},
               {"drive",0.1f})
    ST_VALUES (v909t,    {"level",3.4f},{"tune",210},{"drop",8},{"bend",8},{"decay",130},{"skin",0},{"wave",0.55f},
               {"wires",0.7f},{"sizzle",160},{"tension",0.95f},{"air",1.0f},{"strike",0},{"stick",0.2f},
               {"comp",0.3f},{"speed",0.7f})
    ST_VALUES (v707,     {"level",0.4f},{"tune",220},{"drop",4},{"decay",200},{"skin",0.5f},{"wave",0.3f},
               {"wires",0.6f},{"sizzle",230},{"tension",0.75f},{"air",0.6f},{"stick",0.3f},{"grit",0.3f})
    ST_VALUES (vLinn,    {"level",-2.5f},{"tune",200},{"drop",3},{"decay",260},{"skin",0.8f},{"ring",0.4f},
               {"wires",0.6f},{"sizzle",300},{"tension",0.6f},{"air",0.55f},{"strike",0.35f},{"stick",0.4f},
               {"grit",0.45f},{"room",0.2f})
    ST_VALUES (vDmx,     {"level",1.7f},{"tune",175},{"drop",3},{"decay",300},{"skin",0.75f},{"ring",0.45f},
               {"wires",0.65f},{"sizzle",350},{"tension",0.5f},{"air",0.6f},{"strike",0.3f},{"stick",0.4f},
               {"grit",0.55f},{"room",0.3f},{"comp",0.25f})
    ST_VALUES (vSp1200,  {"level",3.9f},{"tune",205},{"drop",3},{"decay",260},{"skin",0.8f},{"ring",0.4f},
               {"wires",0.6f},{"sizzle",280},{"tension",0.6f},{"air",0.6f},{"strike",0.5f},{"stick",0.5f},
               {"grit",0.32f},{"drive",0.25f},{"colour",0.5f},{"comp",0.5f},{"speed",0.5f})
    ST_VALUES (vMpc,     {"level",3.6f},{"tune",220},{"drop",3},{"decay",280},{"skin",0.9f},{"ring",0.45f},
               {"wires",0.6f},{"sizzle",300},{"tension",0.6f},{"air",0.65f},{"strike",0.6f},{"stick",0.5f},
               {"grit",0.28f},{"room",0.15f},{"drive",0.2f},{"comp",0.45f},{"speed",0.55f})
    ST_VALUES (vCr78,    {"level",1.4f},{"tune",300},{"drop",1},{"decay",90},{"skin",0},{"wave",0},
               {"wires",0.45f},{"sizzle",120},{"tension",0.9f},{"air",0.5f},{"strike",0},{"stick",0},{"colour",0.7f})
    ST_VALUES (vSimmons, {"level",-1.2f},{"tune",160},{"drop",14},{"bend",40},{"decay",380},{"skin",0},{"wave",0.7f},
               {"wires",0.5f},{"sizzle",380},{"tension",0.9f},{"air",0.7f},{"strike",0},{"stick",0.2f},
               {"room",0.35f},{"gate",300})
    ST_VALUES (v808Clap, {"level",-4.1f},{"body",0},{"wires",0},{"stick",0},{"clap",1},{"spread",0.5f},{"sizzle",220},
               {"air",0.5f})

    // ---- TECHNO & HOUSE -------------------------------------------------------------
    ST_VALUES (vTechno,  {"level",-0.3f},{"tune",195},{"drop",8},{"bend",10},{"decay",210},{"skin",0.1f},{"wave",0.55f},
               {"wires",0.7f},{"sizzle",280},{"tension",0.85f},{"air",0.85f},{"stick",0.2f},{"room",0.25f},
               {"drive",0.6f},{"engine",2},{"colour",0.7f},{"comp",0.5f},{"speed",0.6f})
    ST_VALUES (vWarehouse,{"level",1.8f},{"body",0.1f},{"tune",240},{"decay",60},{"wires",0.2f},{"sizzle",420},
               {"tension",0.8f},{"stick",0},{"clap",1},{"spread",0.55f},{"room",0.45f},{"comp",0.3f})
    ST_VALUES (vMinimal, {"level",-4.6f},{"tune",400},{"drop",2},{"decay",60},{"skin",0.2f},{"wires",0},
               {"strike",1},{"stick",0.7f},{"room",0.25f},{"echo",0.2f},{"time",3})
    ST_VALUES (vDetroit, {"level",0.6f},{"tune",185},{"drop",6},{"bend",10},{"decay",190},{"skin",0},{"wave",0.5f},
               {"wires",0.6f},{"sizzle",260},{"tension",0.85f},{"air",0.85f},{"stick",0.15f},
               {"clap",0.7f},{"spread",0.5f},{"room",0.2f},{"comp",0.3f})
    ST_VALUES (vDarkRoom,{"level",0},{"tune",170},{"drop",6},{"bend",12},{"decay",230},{"skin",0.2f},{"wave",0.5f},
               {"wires",0.65f},{"sizzle",320},{"tension",0.7f},{"air",0.5f},{"room",0.8f},
               {"drive",0.3f},{"engine",0},{"colour",0.4f},{"comp",0.35f})
    ST_VALUES (vHouse,   {"level",1.2f},{"tune",200},{"drop",5},{"decay",200},{"skin",0.3f},{"wave",0.4f},
               {"wires",0.55f},{"sizzle",260},{"tension",0.8f},{"air",0.8f},{"clap",0.55f},{"spread",0.45f},
               {"room",0.2f},{"comp",0.3f})

    // ---- DUB: rim shots, springs and tape --------------------------------------------
    ST_VALUES (vRoots,   {"level",-8.1f},{"tune",260},{"drop",3},{"decay",240},{"skin",0.9f},{"ring",0.6f},
               {"wires",0.35f},{"sizzle",260},{"tension",0.6f},{"strike",0.95f},{"stick",0.6f},
               {"room",0.35f},{"echo",0.55f},{"time",3},{"colour",0.75f})
    ST_VALUES (vSteppers,{"level",-4.6f},{"tune",220},{"drop",3},{"decay",280},{"skin",0.9f},{"ring",0.5f},
               {"wires",0.5f},{"sizzle",300},{"tension",0.6f},{"strike",0.7f},{"stick",0.45f},
               {"room",0.3f},{"echo",0.45f},{"time",4})
    ST_VALUES (vDubTechno,{"level",0.4f},{"tune",190},{"drop",5},{"decay",200},{"skin",0.2f},{"wave",0.45f},
               {"wires",0.55f},{"sizzle",300},{"tension",0.75f},{"air",0.55f},{"room",0.5f},
               {"echo",0.7f},{"time",3},{"colour",0.4f})
    ST_VALUES (vTubby,   {"level",-7.8f},{"tune",280},{"drop",2},{"decay",200},{"skin",0.85f},{"ring",0.55f},
               {"wires",0.3f},{"sizzle",220},{"strike",1},{"stick",0.7f},{"room",0.25f},
               {"echo",0.85f},{"time",3},{"colour",0.6f})
    ST_VALUES (vSpaceClap,{"level",-5.2f},{"body",0.2f},{"tune",230},{"decay",80},{"wires",0.3f},{"clap",0.85f},
               {"spread",0.5f},{"sizzle",300},{"room",0.3f},{"echo",0.6f},{"time",5},{"colour",0.65f})

    // ---- INDUSTRIAL -----------------------------------------------------------------
    ST_VALUES (vMetal,   {"level",-1.7f},{"tune",320},{"drop",10},{"bend",25},{"decay",420},{"skin",1},{"ring",0.9f},
               {"wires",0.5f},{"sizzle",400},{"tension",0.2f},{"strike",0.9f},{"stick",0.6f},
               {"grit",0.2f},{"room",0.4f},{"drive",0.7f},{"engine",3})
    ST_VALUES (vGated,   {"level",-2.4f},{"tune",165},{"drop",14},{"bend",35},{"decay",380},{"skin",0.1f},{"wave",0.7f},
               {"wires",0.6f},{"sizzle",380},{"tension",0.85f},{"room",0.85f},{"gate",180},
               {"drive",0.5f},{"engine",2},{"comp",0.6f},{"speed",0.8f})
    ST_VALUES (vEbm,     {"level",5.2f},{"tune",240},{"drop",9},{"bend",6},{"decay",110},{"skin",0},{"wave",1},
               {"wires",0.7f},{"sizzle",150},{"tension",1},{"air",0.7f},{"drive",0.4f},{"engine",1},
               {"comp",0.5f},{"speed",0.8f})
    ST_VALUES (vPower,   {"level",-1.8f},{"tune",150},{"drop",6},{"decay",300},{"skin",0.6f},{"ring",0.7f},
               {"wires",1},{"sizzle",900},{"tension",0},{"air",0.3f},{"room",0.6f},{"gate",350},
               {"drive",1},{"engine",2},{"colour",0.5f})
    ST_VALUES (vFactory, {"level",-1.3f},{"tune",170},{"drop",5},{"decay",320},{"skin",0.7f},{"ring",0.6f},
               {"wires",0.6f},{"sizzle",360},{"tension",0.45f},{"strike",0.6f},{"stick",0.5f},
               {"room",0.9f},{"gate",420},{"drive",0.45f},{"engine",0},{"comp",0.5f})

    // ---- DIGITAL HARDCORE and the break -----------------------------------------------
    ST_VALUES (vAtari,   {"level",0.6f},{"tune",230},{"drop",4},{"decay",240},{"skin",0.8f},{"ring",0.4f},
               {"wires",0.8f},{"sizzle",300},{"tension",0.6f},{"strike",0.5f},{"stick",0.5f},
               {"grit",0.6f},{"drive",0.8f},{"engine",3},{"comp",0.6f},{"speed",0.8f})
    ST_VALUES (vAmen,    {"level",4.5f},{"tune",240},{"drop",3},{"decay",260},{"skin",0.92f},{"ring",0.55f},
               {"wires",0.65f},{"sizzle",280},{"tension",0.6f},{"air",0.7f},{"strike",0.55f},{"stick",0.55f},
               {"grit",0.3f},{"room",0.12f},{"drive",0.2f},{"comp",0.55f},{"speed",0.7f})
    ST_VALUES (vBreakcore,{"level",0.8f},{"tune",210},{"drop",6},{"decay",240},{"skin",0.7f},{"ring",0.5f},
               {"wires",0.9f},{"sizzle",250},{"tension",0.4f},{"strike",0.6f},{"stick",0.6f},
               {"grit",0.4f},{"drive",0.9f},{"engine",2},{"comp",0.7f},{"speed",0.9f})
    ST_VALUES (vSpeedcore,{"level",4.5f},{"tune",260},{"drop",12},{"bend",5},{"decay",90},{"skin",0.3f},{"wave",0.6f},
               {"wires",0.8f},{"sizzle",110},{"tension",0.95f},{"stick",0.4f},
               {"drive",0.75f},{"engine",2},{"comp",0.6f},{"speed",0.9f})
    ST_VALUES (vGabber,  {"level",-1.2f},{"tune",200},{"drop",9},{"bend",10},{"decay",220},{"skin",0},{"wave",0.6f},
               {"wires",0.7f},{"sizzle",260},{"tension",0.85f},{"room",0.25f},
               {"drive",0.95f},{"engine",2},{"comp",0.5f},{"speed",0.7f})

    // ---- MODERN -----------------------------------------------------------------------
    ST_VALUES (vTrap,    {"level",-0.5f},{"tune",220},{"drop",4},{"decay",200},{"skin",0.5f},{"ring",0.4f},
               {"wires",0.55f},{"sizzle",240},{"tension",0.8f},{"clap",0.9f},{"spread",0.4f},
               {"room",0.2f},{"comp",0.35f})
    ST_VALUES (vDnb,     {"level",5.6f},{"tune",250},{"drop",4},{"bend",8},{"decay",240},{"skin",0.85f},{"ring",0.45f},
               {"wires",0.65f},{"sizzle",220},{"tension",0.75f},{"air",0.95f},{"strike",0.55f},{"stick",0.6f},
               {"attack",0.4f},{"drive",0.15f},{"comp",0.55f},{"speed",0.75f})
    ST_VALUES (vDubstep, {"level",0.7f},{"tune",190},{"drop",5},{"decay",300},{"skin",0.7f},{"ring",0.45f},
               {"wires",0.7f},{"sizzle",450},{"tension",0.6f},{"clap",0.5f},{"spread",0.45f},
               {"room",0.7f},{"gate",280},{"attack",0.3f},{"drive",0.3f},{"engine",1},{"comp",0.6f})
    ST_VALUES (vLofi,    {"level",0.8f},{"tune",210},{"drop",3},{"decay",240},{"skin",0.85f},{"ring",0.4f},
               {"wires",0.55f},{"sizzle",260},{"tension",0.55f},{"air",0.4f},{"strike",0.4f},{"stick",0.35f},
               {"grit",0.5f},{"colour",0.45f},{"comp",0.35f},{"velo",0.8f})

    ST_VALUES (vInit,    {"level",-4.5f})

#undef ST_VALUES

    const Preset kPresets[] =
    {
        { "Init",              "INIT",       "A plain snare to start from: half head, half machine.", vInit, ST_N (vInit) },

        { "Studio Snare",      "ACOUSTIC",   "A 14-inch wooden snare, mic'd close, a little room.", vStudio, ST_N (vStudio) },
        { "Piccolo Funk",      "ACOUSTIC",   "A piccolo cranked tight: high, dry and snapping.", vPiccolo, ST_N (vPiccolo) },
        { "Seventies Dead",    "ACOUSTIC",   "Tuned low, damped with a wallet, wires loose.", vSeventies, ST_N (vSeventies) },
        { "Rock Rim Shot",     "ACOUSTIC",   "Every hit a rim shot, the shell ringing, pushed.", vRockRim, ST_N (vRockRim) },
        { "Jazz Snare",        "ACOUSTIC",   "Open, ringing, loose wires; play it soft for ghost notes.", vJazz, ST_N (vJazz) },
        { "Loose Garage",      "ACOUSTIC",   "Wires hanging off it, buzzing at the head's pitch.", vGarage, ST_N (vGarage) },
        { "Big Eighties",      "ACOUSTIC",   "A huge room, compressed, then gated shut.", vEighties, ST_N (vEighties) },

        { "808 Snare",         "MACHINES",   "Roland TR-808: two tones and a little noise.", v808, ST_N (v808) },
        { "808 Snappy",        "MACHINES",   "TR-808 with SNAPPY turned up.", v808s, ST_N (v808s) },
        { "909 Snare",         "MACHINES",   "Roland TR-909: two triangles, the drop, bright noise.", v909, ST_N (v909) },
        { "909 Tight",         "MACHINES",   "TR-909 tuned up, short, compressed.", v909t, ST_N (v909t) },
        { "707 / 727",         "MACHINES",   "The brittle mid-eighties Roland sample snare.", v707, ST_N (v707) },
        { "LinnDrum",          "MACHINES",   "Linn: a real snare, 8-bit.", vLinn, ST_N (vLinn) },
        { "DMX",               "MACHINES",   "Oberheim DMX: bigger, roomier, 8-bit.", vDmx, ST_N (vDmx) },
        { "SP-1200 Crack",     "MACHINES",   "E-mu SP-1200: 12-bit, pushed, filtered.", vSp1200, ST_N (vSp1200) },
        { "MPC60 Break",       "MACHINES",   "Akai MPC60: a break snare, chopped and compressed.", vMpc, ST_N (vMpc) },
        { "CR-78",             "MACHINES",   "Roland CR-78: short, high, polite.", vCr78, ST_N (vCr78) },
        { "Simmons SDS-V",     "MACHINES",   "The hexagonal pad: a long pitch sweep into a gated room.", vSimmons, ST_N (vSimmons) },
        { "808 Clap",          "MACHINES",   "The 808 hand clap on its own.", v808Clap, ST_N (v808Clap) },

        { "Techno Crush",      "TECHNO",     "A 909 through RAZOR WING into a small room.", vTechno, ST_N (vTechno) },
        { "Warehouse Clap",    "TECHNO",     "Claps in a big concrete room.", vWarehouse, ST_N (vWarehouse) },
        { "Minimal Rim",       "TECHNO",     "A rim shot and a dotted-eighth echo.", vMinimal, ST_N (vMinimal) },
        { "Detroit Stack",     "TECHNO",     "909 snare and clap together.", vDetroit, ST_N (vDetroit) },
        { "Dark Room",         "TECHNO",     "A snare heard from the far end of the room, the lights off.", vDarkRoom, ST_N (vDarkRoom) },
        { "House Clap Snare",  "TECHNO",     "Warm snare, clap on top.", vHouse, ST_N (vHouse) },

        { "Roots Rim Shot",    "DUB",        "A cracked high rim shot into a dotted-eighth tape echo.", vRoots, ST_N (vRoots) },
        { "Steppers",          "DUB",        "A steady snare with a quarter-note echo.", vSteppers, ST_N (vSteppers) },
        { "Dub Techno Space",  "DUB",        "Soft, dark, and the echo does the rest.", vDubTechno, ST_N (vDubTechno) },
        { "Tubby Throw",       "DUB",        "The echo turned right up: a rim shot thrown into the tape.", vTubby, ST_N (vTubby) },
        { "Space Echo Clap",   "DUB",        "A clap ping-ponging on a dotted quarter.", vSpaceClap, ST_N (vSpaceClap) },

        { "Metal Shop",        "INDUSTRIAL", "A ringing rim struck hard, crushed by SUPERNOVA.", vMetal, ST_N (vMetal) },
        { "Gated Machine",     "INDUSTRIAL", "An electronic snare in a gated room, clipped.", vGated, ST_N (vGated) },
        { "EBM Snap",          "INDUSTRIAL", "Square wave, tight noise, HYPERDRIVE, compressed flat.", vEbm, ST_N (vEbm) },
        { "Power Electronics", "INDUSTRIAL", "Loose wires, full drive, a gated wall of noise.", vPower, ST_N (vPower) },
        { "Factory Floor",     "INDUSTRIAL", "A snare in a vast gated hall.", vFactory, ST_N (vFactory) },

        { "Atari Riot",        "HARDCORE",   "8-bit grit and SUPERNOVA: digital hardcore.", vAtari, ST_N (vAtari) },
        { "Amen Chop",         "HARDCORE",   "The break snare: bright, 12-bit, squashed.", vAmen, ST_N (vAmen) },
        { "Breakcore Blast",   "HARDCORE",   "RAZOR WING at the top and the compressor pumping.", vBreakcore, ST_N (vBreakcore) },
        { "Speedcore Snap",    "HARDCORE",   "A short zap for rolls at any tempo.", vSpeedcore, ST_N (vSpeedcore) },
        { "Gabber Snare",      "HARDCORE",   "A 909 snare clipped as hard as the kick.", vGabber, ST_N (vGabber) },

        { "Trap Snare Clap",   "MODERN",     "Snare and clap, stacked and tight.", vTrap, ST_N (vTrap) },
        { "DnB Snare",         "MODERN",     "Bright, high, all crack.", vDnb, ST_N (vDnb) },
        { "Dubstep Snare",     "MODERN",     "Snare, clap and a gated room, pushed.", vDubstep, ST_N (vDubstep) },
        { "Lo-Fi Ghost",       "MODERN",     "Dusty and dark; play it soft.", vLofi, ST_N (vLofi) },
    };
}

int numPresets() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }

const Preset& preset (int i)
{
    if (i < 0 || i >= numPresets()) i = 0;
    return kPresets[i];
}

bool setById (Params& p, const char* id, float v)
{
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp (specs()[i].id, id) == 0) { p.*(specs()[i].member) = v; return true; }
    return false;
}

float getById (const Params& p, const char* id)
{
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp (specs()[i].id, id) == 0) return p.*(specs()[i].member);
    return 0.0f;
}

int presetByName (const char* n)
{
    for (int i = 0; i < numPresets(); ++i) if (std::strcmp (preset (i).name, n) == 0) return i;
    return -1;
}

Params presetParams (int i)
{
    Params p;
    for (int k = 0; k < kNumParams; ++k) p.*(specs()[k].member) = specs()[k].def;
    const Preset& pr = preset (i);
    for (int k = 0; k < pr.numValues; ++k) setById (p, pr.values[k].id, pr.values[k].v);
    return p;
}

} // namespace st
