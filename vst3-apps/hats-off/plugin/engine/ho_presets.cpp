#include "ho_presets.h"

#include <cstring>

namespace ho
{

namespace
{
#define HO_VALUES(name, ...) const PresetValue name[] = { __VA_ARGS__ };
#define HO_N(name) (int) (sizeof (name) / sizeof (name[0]))

    // ---- ACOUSTIC: cast bronze, the kit as it is heard ---------------------
    HO_VALUES (vHat14,    {"level",-3.9f},{"size",14},{"density",0.75f},{"bloom",0.15f},{"open",0.12f},{"sizzle",0.35f},
               {"chick",0.6f},{"strike",0.55f},{"stick",0.4f},{"decay",2200},{"cut",300},{"width",0.4f},{"air",0.85f})
    HO_VALUES (vLooseHat, {"level",-9.5f},{"size",14},{"density",0.8f},{"bloom",0.2f},{"open",0.4f},{"sizzle",0.7f},
               {"chick",0.6f},{"strike",0.6f},{"stick",0.35f},{"decay",2200},{"cut",280},{"width",0.45f},{"air",0.85f})
    HO_VALUES (vJazzRide, {"level",-14.9f},{"size",20},{"density",0.9f},{"bloom",0.35f},{"open",1},{"sizzle",0},
               {"strike",0.45f},{"stick",0.55f},{"decay",5000},{"cut",150},{"width",0.6f},{"room",0.15f},{"choke",0})
    HO_VALUES (vSizzleRide,{"level",-15.3f},{"size",20},{"density",0.9f},{"bloom",0.35f},{"open",1},{"sizzle",0.6f},
               {"strike",0.5f},{"stick",0.5f},{"decay",6000},{"cut",150},{"width",0.6f},{"choke",0})
    HO_VALUES (vRideBell, {"level",-13.4f},{"size",21},{"density",0.7f},{"bloom",0.2f},{"open",1},{"sizzle",0},
               {"strike",0},{"stick",0.5f},{"decay",5000},{"cut",200},{"width",0.4f},{"choke",0})
    HO_VALUES (vDarkRide, {"level",-15.3f},{"size",22},{"density",1},{"bloom",0.4f},{"trash",0.12f},{"open",1},{"sizzle",0.15f},
               {"strike",0.5f},{"stick",0.45f},{"decay",6500},{"cut",120},{"air",0.6f},{"width",0.6f},{"choke",0})
    HO_VALUES (vCrash16,  {"level",-12.9f},{"size",16},{"density",0.9f},{"bloom",0.6f},{"open",1},{"sizzle",0},
               {"strike",0.9f},{"stick",0.2f},{"decay",3500},{"cut",180},{"width",0.6f},{"choke",0})
    HO_VALUES (vCrash18,  {"level",-12.9f},{"size",18},{"density",0.95f},{"bloom",0.7f},{"open",1},{"sizzle",0},
               {"strike",1},{"stick",0.2f},{"decay",4500},{"cut",150},{"width",0.65f},{"choke",0})
    HO_VALUES (vSplash,   {"level",-9.6f},{"size",10},{"density",0.7f},{"bloom",0.4f},{"open",1},{"sizzle",0},
               {"strike",0.9f},{"stick",0.3f},{"decay",1100},{"cut",300},{"width",0.5f},{"choke",0})
    HO_VALUES (vChina,    {"level",-15.8f},{"size",18},{"density",0.9f},{"bloom",0.5f},{"trash",0.75f},{"open",1},{"sizzle",0},
               {"strike",0.95f},{"stick",0.3f},{"decay",3000},{"cut",200},{"width",0.55f},{"choke",0})
    HO_VALUES (vRoomHats, {"level",-3.8f},{"size",14},{"density",0.75f},{"bloom",0.15f},{"open",0.15f},{"sizzle",0.4f},
               {"chick",0.6f},{"strike",0.55f},{"stick",0.35f},{"decay",2200},{"cut",250},{"width",0.5f},{"room",0.45f})

    // ---- VINTAGE: the circuits and the sample machines ----------------------
    HO_VALUES (v808C,     {"level",0.5f},{"bronze",0},{"density",0.5f},{"open",0},{"decay",2000},{"cut",3000},{"stick",0},
               {"sizzle",0},{"chick",0.4f},{"width",0})
    HO_VALUES (v808O,     {"level",0.5f},{"bronze",0},{"density",0.5f},{"open",0.85f},{"decay",650},{"cut",3000},{"stick",0},
               {"sizzle",0},{"chick",0.4f},{"width",0})
    HO_VALUES (v808Cym,   {"level",-2.3f},{"bronze",0},{"density",0.5f},{"size",20},{"open",1},{"decay",1800},{"cut",1500},
               {"stick",0},{"sizzle",0},{"width",0},{"choke",0})
    HO_VALUES (v909C,     {"level",-1.1f},{"bronze",0.5f},{"noise",0.35f},{"size",13},{"density",0.7f},{"open",0},{"decay",2400},
               {"grit",0.55f},{"cut",4000},{"air",0.95f},{"stick",0.1f},{"sizzle",0.1f},{"width",0})
    HO_VALUES (v909O,     {"level",-2.1f},{"bronze",0.5f},{"noise",0.35f},{"size",13},{"density",0.7f},{"open",0.8f},{"decay",900},
               {"grit",0.55f},{"cut",4000},{"air",0.95f},{"stick",0.1f},{"sizzle",0.2f},{"width",0})
    HO_VALUES (v909R,     {"level",-8.2f},{"bronze",0.8f},{"noise",0.15f},{"size",20},{"density",0.8f},{"open",1},{"decay",2500},
               {"grit",0.55f},{"strike",0.5f},{"cut",1500},{"sizzle",0},{"width",0},{"choke",0})
    HO_VALUES (v909Cr,    {"level",-11.1f},{"bronze",0.9f},{"noise",0.2f},{"size",17},{"density",0.9f},{"bloom",0.5f},{"open",1},
               {"decay",2800},{"grit",0.55f},{"strike",1},{"cut",800},{"sizzle",0},{"width",0},{"choke",0})
    HO_VALUES (v606,      {"level",1.2f},{"bronze",0},{"noise",0.5f},{"density",0.8f},{"open",0.1f},{"decay",1600},{"cut",5000},
               {"stick",0},{"sizzle",0},{"width",0})
    HO_VALUES (vCr78,     {"level",2.1f},{"bronze",0},{"noise",0.9f},{"density",0.3f},{"open",0},{"decay",2000},{"cut",6000},
               {"air",0.6f},{"stick",0},{"sizzle",0},{"width",0})
    HO_VALUES (vLinn,     {"level",-3.7f},{"bronze",0.95f},{"size",14},{"density",0.75f},{"open",0.05f},{"decay",1800},{"grit",0.45f},
               {"stick",0.3f},{"cut",2000},{"width",0})
    HO_VALUES (v707,      {"level",-5.1f},{"bronze",0.9f},{"size",14},{"density",0.7f},{"open",0.1f},{"decay",1500},{"grit",0.4f},
               {"air",0.7f},{"cut",2500},{"width",0})
    HO_VALUES (vSp1200,   {"level",1.2f},{"bronze",0.95f},{"size",14},{"density",0.8f},{"open",0.1f},{"decay",2000},{"grit",0.32f},
               {"colour",0.55f},{"comp",0.4f},{"cut",600},{"width",0})
    HO_VALUES (vKraft,    {"level",-10.7f},{"bronze",0},{"trash",0.6f},{"density",0.2f},{"size",18},{"open",1},{"decay",700},
               {"echo",0.3f},{"time",3},{"sizzle",0},{"width",0},{"choke",0})

    // ---- TECHNO -----------------------------------------------------------------
    HO_VALUES (vTechC,    {"level",5.1f},{"bronze",0.35f},{"noise",0.3f},{"open",0},{"decay",2400},{"cut",5000},{"grit",0.2f},
               {"drive",0.35f},{"engine",1},{"comp",0.3f},{"width",0.2f})
    HO_VALUES (vRollO,    {"level",3.3f},{"bronze",0.5f},{"noise",0.25f},{"open",0.75f},{"decay",900},{"cut",3500},
               {"comp",0.45f},{"speed",0.7f},{"width",0.3f})
    HO_VALUES (vTick,     {"level",-1.9f},{"bronze",0.6f},{"density",0.3f},{"open",0},{"decay",1200},{"cut",6000},{"stick",0.6f},
               {"width",0.7f})
    HO_VALUES (vDetRide,  {"level",-6.5f},{"bronze",0.6f},{"noise",0.15f},{"size",20},{"density",0.8f},{"open",1},{"decay",2200},
               {"grit",0.4f},{"cut",1500},{"width",0.3f},{"choke",0})
    HO_VALUES (vHardTech, {"level",0.2f},{"bronze",0.3f},{"noise",0.4f},{"open",0.2f},{"decay",1400},{"cut",3000},
               {"drive",0.75f},{"engine",2},{"comp",0.5f},{"width",0.2f})
    HO_VALUES (vWareRide, {"level",-10.9f},{"bronze",0.8f},{"size",21},{"density",0.85f},{"open",1},{"decay",3000},{"cut",800},
               {"room",0.55f},{"width",0.5f},{"choke",0})

    // ---- DUB ----------------------------------------------------------------------
    HO_VALUES (vDubHat,   {"level",-6.8f},{"bronze",0.9f},{"open",0.55f},{"sizzle",0.4f},{"decay",1500},{"echo",0.55f},{"time",3},
               {"room",0.2f},{"colour",0.7f},{"width",0.4f})
    HO_VALUES (vRootsHat, {"level",-3.5f},{"size",14},{"open",0.2f},{"sizzle",0.5f},{"decay",2000},{"room",0.3f},{"echo",0.25f},
               {"time",0},{"air",0.7f},{"width",0.4f})
    HO_VALUES (vDubTech,  {"level",1.1f},{"bronze",0.4f},{"noise",0.2f},{"open",0.5f},{"decay",900},{"cut",2500},{"echo",0.65f},
               {"time",3},{"room",0.5f},{"colour",0.5f},{"width",0.5f})
    HO_VALUES (vSkank,    {"level",-2.6f},{"bronze",0.9f},{"open",0},{"decay",1800},{"cut",1500},{"echo",0.4f},{"time",2},
               {"width",0.3f})
    HO_VALUES (vSpaceCym, {"level",-13},{"size",18},{"density",0.9f},{"bloom",0.8f},{"open",1},{"strike",0.9f},{"decay",4000},
               {"echo",0.5f},{"time",5},{"room",0.6f},{"width",0.7f},{"choke",0})

    // ---- DUBSTEP & BASS -----------------------------------------------------------------
    HO_VALUES (vDsHat,    {"level",3.7f},{"bronze",0.7f},{"open",0},{"decay",2000},{"cut",5500},{"stick",0.5f},{"comp",0.5f},
               {"speed",0.8f},{"attack",0.4f},{"width",0.3f})
    HO_VALUES (vHalfCrash,{"level",-5.8f},{"size",18},{"density",0.95f},{"bloom",0.7f},{"open",1},{"strike",1},{"decay",4500},
               {"room",0.5f},{"comp",0.35f},{"drive",0.2f},{"width",0.7f},{"choke",0})
    HO_VALUES (vTrapHat,  {"level",0.2f},{"bronze",0.3f},{"noise",0.15f},{"open",0},{"decay",1500},{"cut",6000},{"stick",0.3f},
               {"velo",0.8f},{"width",0.2f})
    HO_VALUES (vGrime,    {"level",3.9f},{"bronze",0},{"grit",0.5f},{"open",0.05f},{"decay",1600},{"drive",0.4f},{"engine",3},
               {"cut",4000},{"width",0})
    HO_VALUES (vUkg,      {"level",-3.1f},{"bronze",0.6f},{"noise",0.2f},{"open",0.3f},{"decay",1400},{"cut",3500},{"width",0.5f})

    // ---- MODERN -------------------------------------------------------------------------
    HO_VALUES (vHouseO,   {"level",-0.5f},{"bronze",0.55f},{"noise",0.25f},{"open",0.7f},{"decay",1000},{"cut",3000},{"comp",0.3f},
               {"width",0.35f})
    HO_VALUES (vLofi,     {"level",-0.3f},{"bronze",0.9f},{"open",0.1f},{"grit",0.5f},{"colour",0.45f},{"decay",1800},{"cut",500},
               {"velo",0.8f},{"width",0.2f})
    HO_VALUES (vPlate,    {"level",-2},{"size",24},{"density",0.5f},{"trash",0.5f},{"bloom",0.3f},{"open",1},{"strike",0.8f},
               {"decay",4000},{"drive",0.5f},{"engine",3},{"cut",120},{"width",0.5f},{"choke",0})
    HO_VALUES (vPsyHat,   {"level",0.7f},{"bronze",0.4f},{"noise",0.3f},{"open",0.35f},{"decay",700},{"cut",6500},{"width",0.6f},
               {"comp",0.3f})

    HO_VALUES (vInit,     {"level",-3.9f})

#undef HO_VALUES

    const Preset kPresets[] =
    {
        { "Init",               "INIT",     "A 14-inch hi-hat, nearly closed: the GM notes play it closed, pedal and open.", vInit, HO_N (vInit) },

        { "Studio Hi-Hat 14",   "ACOUSTIC", "A pair of 14-inch hats, mic'd close: closed, pedal and open on the GM notes.", vHat14, HO_N (vHat14) },
        { "Loose Hat",          "ACOUSTIC", "The pedal eased off: the plates touch and sizzle.", vLooseHat, HO_N (vLooseHat) },
        { "Jazz Ride 20",       "ACOUSTIC", "A 20-inch ride: a defined stick over a wash that builds.", vJazzRide, HO_N (vJazzRide) },
        { "Sizzle Ride",        "ACOUSTIC", "A ride with rivets in it.", vSizzleRide, HO_N (vSizzleRide) },
        { "Ride Bell",          "ACOUSTIC", "Played on the bell: the ping.", vRideBell, HO_N (vRideBell) },
        { "Dark Ride 22",       "ACOUSTIC", "A big, dark, dry 22-inch ride.", vDarkRide, HO_N (vDarkRide) },
        { "Crash 16",           "ACOUSTIC", "A thin 16-inch crash, fast to open.", vCrash16, HO_N (vCrash16) },
        { "Crash 18",           "ACOUSTIC", "An 18-inch crash: the explosion, then the bloom.", vCrash18, HO_N (vCrash18) },
        { "Splash 10",          "ACOUSTIC", "A 10-inch splash, short and bright.", vSplash, HO_N (vSplash) },
        { "China 18",           "ACOUSTIC", "An 18-inch china: trashy, upturned, rude.", vChina, HO_N (vChina) },
        { "Hats In A Room",     "ACOUSTIC", "The hi-hat heard from the overheads.", vRoomHats, HO_N (vRoomHats) },

        { "808 Closed Hat",     "VINTAGE",  "Roland TR-808: six squares, a band-pass, gone.", v808C, HO_N (v808C) },
        { "808 Open Hat",       "VINTAGE",  "The 808's open hat; the closed one chokes it.", v808O, HO_N (v808O) },
        { "808 Cymbal",         "VINTAGE",  "The 808's cymbal: the same squares, lower and longer.", v808Cym, HO_N (v808Cym) },
        { "909 Closed Hat",     "VINTAGE",  "Roland TR-909: a real hat through six bits.", v909C, HO_N (v909C) },
        { "909 Open Hat",       "VINTAGE",  "The 909's open hat, the one every house record uses.", v909O, HO_N (v909O) },
        { "909 Ride",           "VINTAGE",  "The 909's ride: 6-bit, bright, endless.", v909R, HO_N (v909R) },
        { "909 Crash",          "VINTAGE",  "The 909's crash.", v909Cr, HO_N (v909Cr) },
        { "606 Hats",           "VINTAGE",  "Roland TR-606: squares and noise, thin and ticking.", v606, HO_N (v606) },
        { "CR-78 Hat",          "VINTAGE",  "Roland CR-78: a burst of noise, politely filtered.", vCr78, HO_N (vCr78) },
        { "LinnDrum Hat",       "VINTAGE",  "Linn: a real hat, 8-bit.", vLinn, HO_N (vLinn) },
        { "707 / 727 Hat",      "VINTAGE",  "The mid-eighties Roland sample hat.", v707, HO_N (v707) },
        { "SP-1200 Hat",        "VINTAGE",  "E-mu SP-1200: 12-bit, filtered, pushed.", vSp1200, HO_N (vSp1200) },
        { "Kraftwerk Metal",    "VINTAGE",  "A ring-modulated circuit cymbal with an echo.", vKraft, HO_N (vKraft) },

        { "Techno Closed",      "TECHNO",   "Circuit, noise and bronze, pushed through HYPERDRIVE.", vTechC, HO_N (vTechC) },
        { "Rolling Open Hat",   "TECHNO",   "The off-beat open hat, compressed to roll.", vRollO, HO_N (vRollO) },
        { "Minimal Tick",       "TECHNO",   "A tiny, wide, dry tick.", vTick, HO_N (vTick) },
        { "Detroit Ride",       "TECHNO",   "A gritty machine ride.", vDetRide, HO_N (vDetRide) },
        { "Hard Techno Hat",    "TECHNO",   "RAZOR WING and a compressor.", vHardTech, HO_N (vHardTech) },
        { "Warehouse Ride",     "TECHNO",   "A ride in a big concrete room.", vWareRide, HO_N (vWareRide) },

        { "Dub Hat Echo",       "DUB",      "A half-open hat into a dotted-eighth tape echo.", vDubHat, HO_N (vDubHat) },
        { "Roots Hi-Hat",       "DUB",      "A loose real hat with a sixteenth-note echo.", vRootsHat, HO_N (vRootsHat) },
        { "Dub Techno Hat",     "DUB",      "Dark, open, and the echo does the rest.", vDubTech, HO_N (vDubTech) },
        { "Skank Hat",          "DUB",      "A closed hat with an eighth-note echo.", vSkank, HO_N (vSkank) },
        { "Space Cymbal",       "DUB",      "A crash thrown into the echo and the room.", vSpaceCym, HO_N (vSpaceCym) },

        { "Dubstep Hat",        "DUBSTEP",  "Tight, bright, compressed, all attack.", vDsHat, HO_N (vDsHat) },
        { "Halftime Crash",     "DUBSTEP",  "A big crash in a room for the drop.", vHalfCrash, HO_N (vHalfCrash) },
        { "Trap Hat",           "DUBSTEP",  "A short hat for rolls; velocity does the rest.", vTrapHat, HO_N (vTrapHat) },
        { "Grime Hat",          "DUBSTEP",  "A crushed circuit hat.", vGrime, HO_N (vGrime) },
        { "UKG Shuffle Hat",    "DUBSTEP",  "A half-open hat for the shuffle.", vUkg, HO_N (vUkg) },

        { "House Open Hat",     "MODERN",   "The open hat on the off-beat.", vHouseO, HO_N (vHouseO) },
        { "Lo-Fi Hat",          "MODERN",   "A hat left in a drawer since 1994.", vLofi, HO_N (vLofi) },
        { "Metal Plate",        "MODERN",   "A 24-inch sheet of trash through SUPERNOVA.", vPlate, HO_N (vPlate) },
        { "Psytrance Hat",      "MODERN",   "A bright, wide, half-open hat.", vPsyHat, HO_N (vPsyHat) },
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

} // namespace ho
