#pragma once

// KICKSTART — the factory presets. Each is a SPARSE list of (id, value) laid
// over the defaults, so a preset names only what makes it itself, and a
// parameter added later lands at its own default in every old preset.
//
// Values are in the engine's own units (Hz, semitones, ms, 0..1, -1..1, dB);
// ENGINE is 0 IDLE BURN, 1 HYPERDRIVE, 2 RAZOR WING, 3 SUPERNOVA.

#include "ks_params.h"

namespace ks
{

struct PresetValue { const char* id; float v; };

struct Preset
{
    const char* name;
    const char* bank;        // ACOUSTIC / CLASSIC / MODERN
    const char* blurb;
    const PresetValue* values;
    int numValues;
};

int           numPresets();
const Preset& preset (int i);

//  defaults, then the preset's values; unknown ids are ignored
Params        presetParams (int i);
bool          setById (Params& p, const char* id, float v);
float         getById (const Params& p, const char* id);

} // namespace ks
