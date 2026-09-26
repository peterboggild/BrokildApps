#pragma once

// HATS OFF — the factory presets. Each is a SPARSE list of (id, value) laid
// over the defaults. Values in the engine's own units; ENGINE 0 IDLE BURN,
// 1 HYPERDRIVE, 2 RAZOR WING, 3 SUPERNOVA; TIME 0 1/16 .. 3 1/8D .. 6 1/2;
// CHOKE 0 OFF, 1 ON; KEYS 0 FIXED, 1 GM KIT, 2 CHROMATIC.

#include "ho_params.h"

namespace ho
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
int           presetByName (const char* name);

} // namespace ho
