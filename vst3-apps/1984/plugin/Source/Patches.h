#pragma once
#include "Engine.h"
#include <cstdint>

namespace n84
{
int         numPatches();
const char* patchName (int i);
const char* patchCategory (int i);
void        applyPatch (int i, Params& p);          // from defaults
void        randomPatch (uint32_t seed, Params& p);  // a playable instrument, never the performance controls
}
