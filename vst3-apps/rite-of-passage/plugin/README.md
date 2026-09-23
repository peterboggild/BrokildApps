# RITE OF PASSAGE — transitions (VST3)

A transition is not an effect. It is a journey with a landing, and the landing
is the part everyone remembers.

One automatable slider carries the music from where it is to where it is
going. Six slots fire along the way, each on its own schedule. ARRIVAL is a
separate command, so scrubbing the slider while you edit cannot set off a drop.

Design authority: [`RITE-OF-PASSAGE-DESIGN.md`](../RITE-OF-PASSAGE-DESIGN.md)
at the repo root. Read it before changing anything structural.

**Status: the spine is built and measured; six of the twelve effects are in.**
133 engine checks and 20 wrapper checks pass. Nobody has heard it in a DAW.

## What is here

**The spine, complete.** Six slots holding any effect, duplicates allowed, the
slot order being the chain order. Per-slot ENTER / EXIT / CURVE / DEPTH with
per-parameter curve overrides. Stepped parameters that adopt only on the beat
grid. Sample-accurate, bar-aligned ARRIVAL with three tail modes (BYPASS /
SPILL / CLEAR) and an IMPACT. Energy-preserving stereo: per-slot
PLACE (stereo / mid / side / L / R), SPREAD, TURN, and a MONO GATE released by
the arrival. BS.1770 loudness throughout. BWFX inserted post-chain.

**Six of the twelve effects:**

| | | level | pitch |
|---|---|---|---|
| **CLIMB** | resonant sweep to self-oscillation | SPECTRAL | exact |
| **TAPE** | analogue echo, the head moves and the pitch bends | NEUTRAL | moves |
| **STUTTER** | capture and roll, tightening to a buzz | NEUTRAL | exact at 0 |
| **CHOP** | rhythmic gate, accelerating | NEUTRAL | exact |
| **RISER** | noise / tone / Shepard generator | INTENTIONAL | moves |
| **GAP** | the silence before the drop | INTENTIONAL | exact |

**Still to come:** GRAIN, BLOOM, FREEZE, REVERSE, BRAKE, DIVE. Adding one is
purely additive — a saved rite naming an effect this build does not have
leaves that slot empty rather than failing to load, so nothing breaks.

## The two contracts, and why they have their own tests

Peter's two: **an effect changes loudness only when that is what it is for,
and nothing moves pitch unless moving pitch is the point.** Both are declared
per effect and per parameter in the descriptors, and the bench reads the
declarations and holds each effect to its own. A NEUTRAL knob may move the
programme loudness by 1.0 dB and no more; a declared non-mover may move the
pitch by 2 cents and no more.

Three things the bench caught that would otherwise have shipped:

- Three CHOPs in series came out **5.5 dB louder than one**, because each gate
  compensated as though it were gating the full-scale material the one before
  it had already thinned. That is the "layers get louder" fault exactly.
- A linear dry/wet dipped **3 dB** in the middle on STUTTER, because a
  repeated slice is decorrelated from the live signal.
- The plugin rendered **differently at 64 samples than at 256**, because the
  clock was accumulated rather than derived.

## Layout

    engine/     the DSP. Plain C++17, JUCE-free, benchable offline.
                rop_dsp        TPT filter, energy-preserving stereo, curves
                rop_loudness   BS.1770 K-weighting — the metric §8 is stated in
                rop_effect     the interface, and the level/pitch declarations
                rop_effects    the effects and the registry
                rop_rack       six slots, the score, ARRIVAL
    src/        the JUCE wrapper: 12 host parameters, state, BWFX, the panel.
    test/       bench.cpp    the engine bench, no JUCE (133 checks)
                hosttest.cpp the wrapper harness, no DAW (20 checks)

## Building

**The bench first, always.** It needs nothing but a compiler:

```sh
cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
cmake --build test/build
./test/build/roptest            # must print ALL CLEAR
```

**The plugin.** JUCE is fetched on first configure; `-DROP_JUCE_DIR=...` uses
a local checkout. BWFX is compiled in from `../BrokildWorldFX`.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/ropehost                # must print ALL CLEAR
```

Linux needs the usual JUCE X11 set for the plugin; the bench needs none of it.

## Cost

Six slots loaded, 48 k: **~93 × real time, about 1 % of one core.** Latency is
zero — every v1 effect is zero-latency, and when FREEZE and DIVE arrive the
plugin will report a fixed worst case rather than a latency that changes when
you assign a slot (§10).

## Changing the engine

1. Run the bench before touching anything, so you know what clear looks like.
2. Make the change.
3. Run the bench. If a number moved, find out why before deciding it is fine.
4. If the change is a choice between two ways of doing something, MEASURE BOTH
   and put both numbers in the comment. The comment above CHOP's makeup lists
   three versions and what each one cost, so nobody re-runs those experiments
   by accident.
