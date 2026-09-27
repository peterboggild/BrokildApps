#include "ks_presets.h"

#include <cstring>

namespace ks
{

namespace
{
#define KS_VALUES(name, ...) const PresetValue name[] = { __VA_ARGS__ };
#define KS_N(name) (int) (sizeof (name) / sizeof (name[0]))

    // ---- ACOUSTIC: SKIN up, small electronic sweep, a real beater, air ----
    KS_VALUES (vStudio,  {"level",1.9f},{"pitch",58},{"sweep",5},{"bend",18},{"decay",380},{"curve",0.08f},{"skin",0.55f},
               {"click",0.55f},{"tone",0.55f},{"room",0.18f},{"attack",0.3f},{"sustain",-0.2f},
               {"drive",0.12f},{"engine",0},{"colour",0.85f},{"comp",0.35f},{"speed",0.4f})
    KS_VALUES (vJazz,    {"level",3.4f},{"pitch",74},{"sweep",3},{"bend",25},{"decay",700},{"curve",0},{"skin",0.85f},
               {"click",0.25f},{"tone",0.22f},{"room",0.32f},{"sustain",0.2f},{"colour",0.72f},
               {"comp",0.4f},{"speed",0.75f})
    KS_VALUES (vRock,    {"level",4.8f},{"pitch",52},{"sweep",6},{"bend",15},{"decay",300},{"curve",0.05f},{"skin",0.6f},
               {"click",0.7f},{"tone",0.66f},{"room",0.25f},{"attack",0.5f},{"sustain",-0.3f},
               {"drive",0.2f},{"engine",0},{"colour",0.8f},{"comp",0.5f},{"speed",0.75f})
    KS_VALUES (vFelt,    {"level",4.2f},{"pitch",60},{"sweep",4},{"bend",22},{"decay",420},{"curve",0},{"skin",0.5f},
               {"click",0.4f},{"tone",0.12f},{"room",0.12f},{"colour",0.6f},{"comp",0.45f},{"speed",0.75f})
    KS_VALUES (vTowel,   {"level",6},{"pitch",64},{"sweep",2},{"bend",12},{"decay",170},{"curve",0},{"skin",0.4f},
               {"click",0.5f},{"tone",0.35f},{"room",0.05f},{"sustain",-0.5f},
               {"drive",0.15f},{"engine",0},{"colour",0.55f},{"comp",0.5f},{"speed",0.75f})
    KS_VALUES (vKitRoom, {"level",4.1f},{"pitch",56},{"sweep",4},{"bend",20},{"decay",520},{"curve",0},{"skin",0.65f},
               {"click",0.5f},{"tone",0.45f},{"room",0.6f},{"attack",0.2f},{"colour",0.75f},
               {"comp",0.5f},{"speed",0.75f})

    // ---- CLASSIC: the analogue boxes and the sample machines --------------
    KS_VALUES (v808,     {"level",-2.8f},{"pitch",49},{"sweep",3},{"bend",60},{"decay",1600},{"curve",0.05f},
               {"click",0.15f},{"tone",0.3f},{"drive",0.08f},{"engine",0},{"velo",0.3f})
    KS_VALUES (v808s,    {"level",0.7f},{"pitch",55},{"sweep",4},{"bend",40},{"decay",350},{"curve",0.05f},
               {"click",0.3f},{"tone",0.35f})
    KS_VALUES (v909,     {"level",-0.1f},{"pitch",52},{"sweep",24},{"bend",22},{"decay",420},{"curve",0.15f},{"wave",0.35f},
               {"click",0.6f},{"tone",0.7f},{"drive",0.1f},{"engine",0},{"comp",0.2f},{"speed",0.5f})
    KS_VALUES (v909h,    {"level",0.6f},{"pitch",50},{"sweep",30},{"bend",18},{"decay",500},{"curve",0.3f},{"wave",0.45f},
               {"click",0.8f},{"tone",0.75f},{"drive",0.35f},{"engine",0},{"comp",0.45f},{"speed",0.55f})
    KS_VALUES (v727,     {"level",-0.6f},{"pitch",62},{"sweep",10},{"bend",20},{"decay",280},{"wave",0.2f},{"skin",0.3f},
               {"click",0.5f},{"tone",0.55f},{"grit",0.35f},{"colour",0.7f})
    KS_VALUES (vLinn,    {"level",1.1f},{"pitch",60},{"sweep",6},{"bend",16},{"decay",320},{"skin",0.5f},
               {"click",0.6f},{"tone",0.55f},{"grit",0.45f},{"room",0.15f},{"attack",0.2f},
               {"drive",0.1f},{"engine",0},{"colour",0.65f},{"comp",0.3f})
    KS_VALUES (vDmx,     {"level",0.1f},{"pitch",55},{"sweep",8},{"bend",20},{"decay",450},{"skin",0.45f},
               {"click",0.6f},{"tone",0.6f},{"grit",0.55f},{"room",0.25f},{"colour",0.7f},{"comp",0.25f})
    KS_VALUES (vLinn9k,  {"level",-4.9f},{"pitch",58},{"sweep",7},{"bend",18},{"decay",380},{"skin",0.5f},
               {"click",0.55f},{"tone",0.6f},{"grit",0.3f},{"room",0.2f},{"attack",0.2f})
    KS_VALUES (vSp12,    {"level",0.2f},{"pitch",54},{"sweep",5},{"bend",25},{"decay",550},{"skin",0.35f},
               {"click",0.45f},{"tone",0.45f},{"grit",0.32f},{"room",0.1f},
               {"drive",0.25f},{"engine",0},{"colour",0.5f},{"comp",0.5f},{"speed",0.35f})
    KS_VALUES (vMpc,     {"level",0.1f},{"pitch",52},{"sweep",6},{"bend",22},{"decay",480},{"skin",0.3f},
               {"click",0.5f},{"tone",0.4f},{"grit",0.28f},{"attack",0.25f},
               {"drive",0.2f},{"engine",0},{"colour",0.6f},{"comp",0.45f},{"speed",0.4f})
    KS_VALUES (vKr55,    {"level",0.3f},{"pitch",68},{"sweep",8},{"bend",12},{"decay",220},{"wave",0.1f},
               {"click",0.35f},{"tone",0.5f},{"colour",0.8f})
    KS_VALUES (vDrumComp,{"level",-0.5f},{"pitch",62},{"sweep",14},{"bend",14},{"decay",300},{"wave",0.25f},
               {"click",0.5f},{"tone",0.6f},{"grit",0.25f})
    KS_VALUES (vCr78,    {"level",0.3f},{"pitch",70},{"sweep",4},{"bend",10},{"decay",200},
               {"click",0.2f},{"tone",0.3f},{"colour",0.6f})

    // ---- MODERN: sweep, sustain, drive and the compressor do the talking --
    KS_VALUES (vRumble,  {"level",0.4f},{"pitch",48},{"sweep",20},{"bend",20},{"decay",520},{"curve",0.3f},{"wave",0.3f},
               {"click",0.55f},{"tone",0.7f},{"room",0.6f},{"drive",0.6f},{"engine",0},
               {"colour",0.35f},{"comp",0.55f},{"speed",0.7f})
    KS_VALUES (vGabber,  {"level",-0.7f},{"pitch",55},{"sweep",36},{"bend",35},{"decay",700},{"curve",0.75f},{"wave",0.6f},
               {"click",0.7f},{"tone",0.8f},{"attack",0.2f},{"drive",0.9f},{"engine",2},
               {"colour",0.8f},{"comp",0.3f},{"speed",0.6f})
    KS_VALUES (vHardstyle,{"level",-0.3f},{"pitch",50},{"sweep",30},{"bend",25},{"decay",650},{"curve",0.6f},{"wave",0.5f},
               {"click",0.9f},{"tone",0.85f},{"attack",0.5f},{"drive",0.75f},{"engine",2},
               {"colour",0.7f},{"comp",0.5f},{"speed",0.6f})
    KS_VALUES (vDubstep, {"level",0.2f},{"pitch",50},{"sweep",22},{"bend",12},{"decay",320},{"curve",0.35f},{"wave",0.1f},
               {"click",0.85f},{"tone",0.9f},{"attack",0.6f},{"sustain",-0.2f},
               {"drive",0.3f},{"engine",1},{"comp",0.6f},{"speed",0.3f})
    KS_VALUES (vTrap,    {"level",-1.3f},{"pitch",43},{"sweep",12},{"bend",45},{"decay",2600},{"curve",0.55f},{"wave",0.1f},
               {"click",0.25f},{"tone",0.6f},{"drive",0.3f},{"engine",1},{"colour",0.9f},
               {"comp",0.3f},{"speed",0.5f},{"key",1})
    KS_VALUES (vPsy,     {"level",4.6f},{"pitch",58},{"sweep",30},{"bend",6},{"decay",180},{"curve",0.2f},
               {"click",0.9f},{"tone",1.0f},{"attack",0.4f},{"sustain",-0.4f},
               {"drive",0.1f},{"engine",0},{"comp",0.4f},{"speed",0.8f})
    KS_VALUES (vHardTech,{"level",-0.5f},{"pitch",46},{"sweep",28},{"bend",18},{"decay",450},{"curve",0.45f},{"wave",0.5f},
               {"click",0.7f},{"tone",0.75f},{"room",0.3f},{"drive",0.6f},{"engine",2},
               {"colour",0.45f},{"comp",0.5f},{"speed",0.65f})
    KS_VALUES (vIndustrial,{"level",1.7f},{"pitch",52},{"sweep",26},{"bend",16},{"decay",420},{"curve",0.35f},{"wave",0.4f},
               {"click",0.7f},{"tone",0.7f},{"grit",0.4f},{"room",0.2f},{"drive",0.6f},{"engine",3},
               {"colour",0.55f},{"comp",0.4f},{"speed",0.5f})
    KS_VALUES (vHouse,   {"level",0.5f},{"pitch",52},{"sweep",18},{"bend",16},{"decay",380},{"curve",0.15f},{"wave",0.25f},
               {"click",0.55f},{"tone",0.6f},{"drive",0.2f},{"engine",0},{"comp",0.35f},{"speed",0.5f})
    KS_VALUES (vLofi,    {"level",1},{"pitch",54},{"sweep",8},{"bend",24},{"decay",500},{"skin",0.3f},
               {"click",0.4f},{"tone",0.4f},{"grit",0.6f},{"room",0.2f},{"drive",0.3f},{"engine",3},
               {"colour",0.35f},{"comp",0.35f},{"speed",0.4f})
    KS_VALUES (vSub,     {"level",-2.1f},{"pitch",45},{"sweep",8},{"bend",30},{"decay",900},{"curve",0.5f},
               {"click",0.12f},{"tone",0.5f},{"drive",0.15f},{"engine",1})

KS_VALUES (vInit, {"level",-2.8f})

#undef KS_VALUES

    const Preset kPresets[] =
    {
        { "Init",            "INIT",     "A plain electronic kick to start from.", vInit, KS_N (vInit) },

        { "Studio Kick",     "ACOUSTIC", "A mic'd 22-inch kick: beater, head and a little room.", vStudio, KS_N (vStudio) },
        { "Jazz Kick",       "ACOUSTIC", "An 18-inch drum, resonant head on, felt beater, lots of tone.", vJazz, KS_N (vJazz) },
        { "Rock Kick",       "ACOUSTIC", "26 inches, hard beater, pushed.", vRock, KS_N (vRock) },
        { "Felt Beater",     "ACOUSTIC", "Soft and round: the felt side of the beater.", vFelt, KS_N (vFelt) },
        { "Towel In The Drum","ACOUSTIC","Seventies dead: blanket in the shell, all thud.", vTowel, KS_N (vTowel) },
        { "Kit In A Room",   "ACOUSTIC", "The whole kick, heard from the other side of the room.", vKitRoom, KS_N (vKitRoom) },

        { "808 Boom",        "CLASSIC",  "Roland TR-808: the long, low, almost pure boom.", v808, KS_N (v808) },
        { "808 Short",       "CLASSIC",  "TR-808 with the decay turned down.", v808s, KS_N (v808s) },
        { "909 Classic",     "CLASSIC",  "Roland TR-909: the sweep, the triangle, the click.", v909, KS_N (v909) },
        { "909 Hard",        "CLASSIC",  "TR-909 pushed through the desk.", v909h, KS_N (v909h) },
        { "707 / 727",       "CLASSIC",  "The brittle mid-eighties Roland sample machines.", v727, KS_N (v727) },
        { "LinnDrum",        "CLASSIC",  "Linn: a well-recorded real kick, 8-bit.", vLinn, KS_N (vLinn) },
        { "DMX",             "CLASSIC",  "Oberheim DMX: bigger, airier, 8-bit.", vDmx, KS_N (vDmx) },
        { "Linn 9000",       "CLASSIC",  "12-bit and cleaner.", vLinn9k, KS_N (vLinn9k) },
        { "SP-12 Boom Bap",  "CLASSIC",  "E-mu SP-12: 12-bit, pushed, compressed.", vSp12, KS_N (vSp12) },
        { "MPC60 Thump",     "CLASSIC",  "Akai MPC60: the thump behind a thousand records.", vMpc, KS_N (vMpc) },
        { "KR-55",           "CLASSIC",  "Korg KR-55: short, woody analogue.", vKr55, KS_N (vKr55) },
        { "Drum Computer",   "CLASSIC",  "Movement Systems: analogue meets digital.", vDrumComp, KS_N (vDrumComp) },
        { "CR-78",           "CLASSIC",  "Roland CR-78: the soft preset-rhythm knock.", vCr78, KS_N (vCr78) },

        { "Techno Rumble",   "MODERN",   "Room into drive into a low-pass: the rumble is the reverb, distorted.", vRumble, KS_N (vRumble) },
        { "Gabber",          "MODERN",   "Rotterdam: a 909 sweep held flat and clipped hard.", vGabber, KS_N (vGabber) },
        { "Hardstyle",       "MODERN",   "The punch, then the distorted tail.", vHardstyle, KS_N (vHardstyle) },
        { "Dubstep Punch",   "MODERN",   "A bright click on a short clean sub, compressed.", vDubstep, KS_N (vDubstep) },
        { "Trap 808",        "MODERN",   "The long tuned 808: key tracking on, play it as a bass.", vTrap, KS_N (vTrap) },
        { "Psytrance Tick",  "MODERN",   "Short, tight, all attack.", vPsy, KS_N (vPsy) },
        { "Hard Techno",     "MODERN",   "Clipped, with a room behind it.", vHardTech, KS_N (vHardTech) },
        { "Industrial Crush","MODERN",   "SUPERNOVA and grit: broken on purpose.", vIndustrial, KS_N (vIndustrial) },
        { "House Thump",     "MODERN",   "Four to the floor, warm.", vHouse, KS_N (vHouse) },
        { "Lo-Fi Dust",      "MODERN",   "A kick left in a drawer since 1991.", vLofi, KS_N (vLofi) },
        { "Clean Sub",       "MODERN",   "Almost no click, a long held sub, harmonics so it reads on a phone.", vSub, KS_N (vSub) },
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

Params presetParams (int i)
{
    Params p;
    for (int k = 0; k < kNumParams; ++k) p.*(specs()[k].member) = specs()[k].def;
    const Preset& pr = preset (i);
    for (int k = 0; k < pr.numValues; ++k) setById (p, pr.values[k].id, pr.values[k].v);
    return p;
}

} // namespace ks
