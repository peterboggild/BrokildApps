#pragma once

// DRUMEQ - the drums. One SOURCE per microphone on an acoustic kit, each with
// its own names for the four bands (what an engineer calls that region ON
// THAT DRUM), the knob ranges that make sense there, a standard starting point
// and its presets. Presets and kits are text ("id=value ..."), so they read
// like a mixing note and cannot address a parameter that does not exist (the
// bench parses every one).
//
// The frequencies are the common advice for close-miked rock and metal kits:
// a kick's box at 300-500 Hz and its beater at 3-6 kHz, a snare's body at
// ~200 Hz and its crack at 4-5 kHz, a tom's fundamental under its box,
// cymbals high-passed out of the kick's way. They are STARTING POINTS: every
// kit is tuned differently, which is what FIND DRUM is for.

#include "de_params.h"
#include <string>

namespace de
{
    enum Source { S_KICK_IN, S_KICK_OUT, S_SNARE_TOP, S_SNARE_BOT, S_TOM1, S_TOM2, S_TOM3,
                  S_HAT, S_RIDE, S_ROOM, S_OH, kNumSources };

    struct BandDef { const char* label; float lo, hi; };

    struct SourceDef
    {
        const char* name;          // "KICK IN"
        const char* chip;          // short, for the source row: "KIK IN"
        BandDef band[4];
        float findLo, findHi;      // where FIND DRUM looks for the fundamental (0 = no FIND)
        int findBand;              // which band it moves (0..3)
        const char* standard;      // the STANDARD preset, over the neutral defaults
    };

    const SourceDef& source (int s);

    struct Preset { const char* name; const char* values; };
    int numPresets (int s);
    const Preset& preset (int s, int i);

    //  neutral -> the source's STANDARD -> the preset's own changes; source set
    Params presetParams (int s, int i);

    //  "id=value id=value"; returns false (and names the culprit) on an
    //  unknown id or a value that does not parse
    bool applyText (Params&, const char* text, std::string* err = nullptr);

    //  a factory kit is one preset per drum
    struct Kit { const char* name; int pick[kNumSources]; };
    int numKits();
    const Kit& kit (int i);
}
