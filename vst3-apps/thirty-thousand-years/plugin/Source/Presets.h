/*  Thirty Thousand Years — factory presets.

    A preset is a small text in a line DSL, parsed on the message thread:

        id=value id=value ...            base values (normalised; LIST/INT as integers)
        #MACRO NAME id:depth id:depth    a macro map (NAME = MASS DREAD VIOLENCE INSTABILITY
                                         CONTAMINATION DISTANCE LIFE HUMANITY)
        #SCENE n id=value ...            scene n (1..4) = the base with these overrides
        #SLOT src dst depth [via curve slew lo hi]   a matrix slot (source by name index)

    Anything not named keeps its default, so a preset written today keeps
    meaning when a parameter is added tomorrow.
*/
#pragma once

#include "Engine.h"

namespace tty
{

struct PresetDef
{
    const char* name;
    const char* cat;
    const char* note;
    const char* body;
};

int              numPresets();
const PresetDef& preset (int i);
/*  Applies preset i onto the engine: base Params, macro maps, scenes, slots. */
void applyPreset (int i, Params& base, MacroDest macro[NUM_MACROS][MACRO_DESTS],
                  Params scene[NUM_SCENES], bool sceneSet[NUM_SCENES], Slot slots[NUM_SLOTS], Mseg mseg[4]);
/*  Parses a preset body (the DSL above) onto the same targets. Returns the number of assignments. */
int  applyPresetBody (const char* body, Params& base, MacroDest macro[NUM_MACROS][MACRO_DESTS],
                      Params scene[NUM_SCENES], bool sceneSet[NUM_SCENES], Slot slots[NUM_SLOTS], Mseg mseg[4]);
/*  A bounded mutation of the unlocked parameters. lockMask: bits per stratum group (see Presets.cpp). */
void mutatePatch (uint32_t seed, float amount, int lockMask, Params& p);
/*  The default macro maps every patch starts from (a preset can override any of them). */
void defaultMacros (MacroDest macro[NUM_MACROS][MACRO_DESTS]);

} // namespace tty
