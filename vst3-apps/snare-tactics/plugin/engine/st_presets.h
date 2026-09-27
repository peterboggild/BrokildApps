#pragma once

// SNARE TACTICS — the factory presets. Each is a SPARSE list of (id, value)
// laid over the defaults, so a preset names only what makes it itself, and a
// parameter added later lands at its own default in every old preset.
//
// Values are in the engine's own units (Hz, semitones, ms, 0..1, -1..1, dB);
// ENGINE is 0 IDLE BURN, 1 HYPERDRIVE, 2 RAZOR WING, 3 SUPERNOVA; TIME is
// 0 1/16, 1 1/8T, 2 1/8, 3 1/8D, 4 1/4, 5 1/4D, 6 1/2; KEYS 0 FIXED, 1 GM KIT,
// 2 CHROMATIC.

#include "st_params.h"

namespace st
{

struct PresetValue { const char* id; float v; };

struct Preset
{
    const char* name;
    const char* bank;
    const char* blurb;
    const PresetValue* values;
    int numValues;
};

int           numPresets();
const Preset& preset (int i);

Params        presetParams (int i);
bool          setById (Params& p, const char* id, float v);
float         getById (const Params& p, const char* id);
int           presetByName (const char* name);      // -1 if none

} // namespace st
