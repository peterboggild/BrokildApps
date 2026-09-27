#pragma once

// RITE OF PASSAGE — the third six (rop_effects_third.cpp).
//
// SWIRL, MANGLE, SWARM, DUST, ORBIT, CHANT. Exposed as accessor functions for
// the same reason the second six are: the descriptor objects live beside the
// classes that read them, in the .cpp's own anonymous namespace.

#include "rop_effect.h"

namespace rop { namespace third {

const EffectDesc& swirlDesc();
const EffectDesc& mangleDesc();
const EffectDesc& swarmDesc();
const EffectDesc& dustDesc();
const EffectDesc& orbitDesc();
const EffectDesc& chantDesc();

Effect* makeSwirl();
Effect* makeMangle();
Effect* makeSwarm();
Effect* makeDust();
Effect* makeOrbit();
Effect* makeChant();

} } // namespace rop::third
