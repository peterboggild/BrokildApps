#pragma once

// RITE OF PASSAGE — the BWFX slot wrapper.
//
// Peter, 2026-09-22: "would it make sense to have an effect that somehow
// hooks the whole BWFX ecosystem in as a wrapper? That would enable me to
// have a submenu where i could insert any of the BWFX FX in one of the six
// slots."
//
// It does, and the reason is PLACEMENT rather than reach. §12 already gives
// the rack's five macros their own lanes on the score, so every World module
// was already sequenceable; what it could not do is put one INSIDE the
// chain. A GATE on the sides entering at 70 %. An ECHO that SPILLs through
// the drop while everything around it CLEARs. §2 says the ordering of six
// things is the second instrument in this plugin, and this hands that
// instrument the whole World rack.
//
// The two interfaces are nearly the same by inheritance of house style —
// process() in place, prepare() on the message thread, reset() with no
// allocation, a 32-sample control rate that rop_dsp.h already calls "the
// BWFX granularity" — so the wrapper is a forwarder, not a bridge.
//
// These appear in the SAME registry as the native eighteen, APPENDED, with
// ids prefixed "bwfx." — both for the reason the registry file already
// gives (an index that moves changes what a saved rite means) and because
// `stutter` is a native effect AND BWFX's GATE.

#include "rop_effect.h"

namespace rop::bw
{

int  numWrapped();                         // how many BWFX modules are offered
const EffectDesc& wrappedDescriptor (int); // index into that set, not a rack type
Effect* createWrapped (int);               // message thread; caller owns

} // namespace rop::bw
