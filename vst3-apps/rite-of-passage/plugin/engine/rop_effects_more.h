#pragma once

// RITE OF PASSAGE — the second six effects (rop_effects_more.cpp), exposed to
// the registry in rop_effects.cpp. Each is a descriptor plus a factory; the
// registry appends them after the first six so every existing index and
// every saved rite is untouched.

#include "rop_effect.h"

namespace rop { namespace more {

//  the descriptors, as accessors: the objects live in the .cpp's own
//  anonymous namespace beside the classes that read them
const EffectDesc& grainDesc();
const EffectDesc& bloomDesc();
const EffectDesc& freezeDesc();
const EffectDesc& reverseDesc();
const EffectDesc& brakeDesc();
const EffectDesc& diveDesc();

Effect* makeGrain();
Effect* makeBloom();
Effect* makeFreeze();
Effect* makeReverse();
Effect* makeBrake();
Effect* makeDive();

} } // namespace rop::more
