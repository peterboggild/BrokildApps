#pragma once
// Magneto Transport - the factory presets (stage 8). docs/PROTOCOL.md.
//
// A preset is the defaults with these values on top, in PHYSICAL units, by parameter
// id. Its OUTPUT comes from PresetTrims.h, written by the leveller (test/presetlevel.cpp:
// every preset as loud as the programme that goes in), so a level
// typed here would be overwritten. Grouped clean -> strange. BYPASS is never touched.
// No JUCE in here: the bench and the leveller read it too.

namespace mt
{

struct FactoryPreset
{
    const char* name;
    const char* group;
    const char* values;   // "id=value id=value ..."
};

inline constexpr const char* PRESET_GROUPS[] = { "CLEAN", "CLASSIC ECHO", "MULTI-HEAD", "CHARACTER", "LOOPS", "STRANGE" };

inline constexpr FactoryPreset FACTORY[] = {
    // ---- CLEAN: a well-kept studio machine doing one job
    { "Studio Slap",          "CLEAN",        "speed=38.1 h1pos=4 mix=0.25 feedback=0.12 condition=5 particles=10" },
    { "Clean Quarter",        "CLEAN",        "sync=7 synchead=0 h1pos=8 mix=0.3 feedback=0.35 condition=5 particles=10" },
    { "Mastering Tape",       "CLEAN",        "repro=1 mix=1 speed=76 h1pos=4 condition=5 particles=30 feedback=0" },
    { "Silky Doubler",        "CLEAN",        "speed=76 h1pos=1.5 h2on=1 h2pos=2.7 h1pan=-0.6 h2pan=0.6 mix=0.35 feedback=0 condition=10" },
    // ---- CLASSIC ECHO: what tape echoes were bought for
    { "Space Echo 1:2:3",     "CLASSIC ECHO", "speed=19 h1pos=4 h2on=1 h2pos=8 h3on=1 h3pos=12 pattern=0 feedback=0.45 mix=0.35 condition=30 treble=-3" },
    { "Dub Delay",            "CLASSIC ECHO", "speed=9.525 h1on=0 h3on=1 h1pos=1.5 h2pos=2.7 h3pos=20 feedback=0.62 bass=3 treble=-4 mix=0.4 condition=40" },
    { "Rockabilly Slap",      "CLASSIC ECHO", "speed=19 h1pos=2.5 feedback=0.15 mix=0.4 drive=3 condition=25" },
    { "Dotted Eighth",        "CLASSIC ECHO", "sync=5 synchead=0 h1pos=6 feedback=0.4 mix=0.3 condition=20" },
    // ---- MULTI-HEAD: three playback heads on one rail
    { "Three-Head Canyon",    "MULTI-HEAD",   "speed=4.75 h1pos=4 h2on=1 h2pos=12 h3on=1 h3pos=20 h1pan=-0.8 h3pan=0.8 feedback=0.5 mix=0.4 treble=-2 condition=25" },
    { "Golden Taps",          "MULTI-HEAD",   "speed=19 pattern=6 h1pos=3 h2on=1 h2pos=4.854 h3on=1 h3pos=7.854 h1pan=-0.6 h3pan=0.6 feedback=0.35 mix=0.35" },
    { "Triplet Ladder",       "MULTI-HEAD",   "sync=7 synchead=0 pattern=2 h1pos=6 h2on=1 h2pos=8 h3on=1 h3pos=12 h2pan=-0.5 h3pan=0.5 feedback=0.35 mix=0.35" },
    { "Ping Pong Heads",      "MULTI-HEAD",   "speed=19 h1pos=4 h2on=1 h2pos=8 h1pan=-1 h2pan=1 feedback=0.45 mix=0.4" },
    // ---- CHARACTER: the tape and the machine as the sound
    { "Hot Saturated Tape",   "CHARACTER",    "repro=1 mix=1 reclevel=7 drive=3 bias=-2 speed=19 h1pos=4 feedback=0" },
    { "Worn Echo Tape",       "CHARACTER",    "stock=3 wear=60 condition=70 speed=4.75 h1pos=4 treble=-6 feedback=0.4 mix=0.5" },
    { "Vintage Low Output",   "CHARACTER",    "stock=1 particles=1 condition=35 speed=19 h1pos=6 feedback=0.4 mix=0.4" },
    { "Shedding Ghosts",      "CHARACTER",    "stock=2 wear=40 condition=45 speed=9.525 h1pos=6 feedback=0.45 mix=0.45" },
    // ---- LOOPS: a closed loop of tape, FLUX MEMORY
    { "Tape Loop Echo",       "LOOPS",        "mode=1 looplen=1 looper=0 speed=19 h1pos=4 feedback=0.4 condition=25 mix=0.4" },
    { "Flux Memory Ghosts",   "LOOPS",        "mode=1 looplen=0.75 looper=1 erase=30 speed=19 h1pos=4 feedback=0.3 mix=0.45" },
    { "Sound on Sound",       "LOOPS",        "mode=1 looplen=2 looper=1 erase=0 speed=19 h1pos=4 feedback=0 mix=0.5" },
    { "Four Bar Loop",        "LOOPS",        "mode=1 sync=1 synchead=0 bars=4 h1pos=4 looper=1 erase=50 feedback=0.2 mix=0.45" },
    // ---- STRANGE: the machine pushed past what it was built for
    { "Broken Machine",       "STRANGE",      "broken=85 speed=19 h1pos=6 feedback=0.4 mix=0.5" },
    { "Varispeed Drunk",      "STRANGE",      "speed=6 condition=100 reels=200 capstan=200 h1pos=4 feedback=0.5 mix=0.5" },
    { "Erase Head Smear",     "STRANGE",      "mode=1 looplen=0.5 looper=1 erase=55 bias=-4 condition=50 speed=19 h1pos=6 feedback=0.6 mix=0.5" },
    { "Hall of Heads",        "STRANGE",      "speed=4.75 h1pos=1.5 h2on=1 h2pos=2.7 h3on=1 h3pos=3.9 feedback=0.62 treble=-8 mix=0.45 condition=30" },
};
inline constexpr int NUM_FACTORY = (int) (sizeof (FACTORY) / sizeof (FACTORY[0]));

} // namespace mt

#include "Params.h"
#include "PresetTrims.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace mt
{
/*  The defaults, then preset i's values, then its loudness trim on OUTPUT. BYPASS and MECH
    SOUNDS keep what `p` had (a preset must not unbypass a bypassed plug-in, nor switch
    the machine's noises on or off). Returns false and
    names the first unknown id or bad value in `err`, without touching anything after it. */
inline bool applyFactoryPreset (int i, Params& p, std::string* err = nullptr)
{
    if (i < 0 || i >= NUM_FACTORY) { if (err) *err = "no such preset"; return false; }
    const float keepBypass = p.bypass, keepMech = p.mech;
    Params q;
    for (int s = 0; s < NUM_SPECS; ++s) q.*(SPECS[s].member) = SPECS[s].def;
    const char* c = FACTORY[i].values;
    while (*c)
    {
        while (*c == ' ') ++c;
        if (! *c) break;
        const char* eq = std::strchr (c, '=');
        if (! eq) { if (err) *err = std::string ("no '=' in ") + c; return false; }
        const std::string id (c, eq);
        char* end = nullptr;
        const float v = std::strtof (eq + 1, &end);
        if (end == eq + 1) { if (err) *err = "bad value for " + id; return false; }
        int s = 0;
        while (s < NUM_SPECS && id != SPECS[s].id) ++s;
        if (s == NUM_SPECS) { if (err) *err = "unknown id " + id; return false; }
        if (v < SPECS[s].lo - 1e-6f || v > SPECS[s].hi + 1e-6f) { if (err) *err = id + " out of range"; return false; }
        q.*(SPECS[s].member) = v;
        c = end;
    }
    q.output = PRESET_TRIM_DB[i];
    q.bypass = keepBypass;
    q.mech = keepMech;
    p = q;
    return true;
}
} // namespace mt
