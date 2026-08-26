# Brokild World FX (BWFX)

The shared, global FX system for all Brokild synths. "World" because the
modules are global: drop a new FX into BWFX and, after a rebuild, every
synth offers it. Design authority: `BWFX-DESIGN.md` at the BrokildApps repo
root (settled with Peter 2026-08-26) — read it before changing anything
structural here.

## What this repo holds

- `src/` — the core. `bwfx.h` is the whole public API: self-describing
  module descriptors, the registry, the `Rack` (order, enables, per-module
  params, wet/dry, ~30 ms reorder dip so structural changes never click),
  string-keyed JSON state, and the fixed world-modulation bus
  `{detuneCents, panSpread, tremolo, pitchSag, filterMul}` for SPECTRA
  characters. Plain C++17, JUCE-free, no allocation/locks/logging in
  `process()`.
- `modules/` — the FX modules and their registry. Founding set extracted
  from Photo-Synth 2's engine: tube saturation, stereo delay, convolution
  reverb (own partitioned FFT convolver — no JUCE), 4-stage phaser,
  dual-line chorus, stutter gate.
- `ui/bwfx-rack.js` — the rack overlay fragment. Self-contained: injects
  its own CSS and DOM (the BWFX brand look — pedal faceplates and rocker
  switches from Photo-Synth 2), builds every control from the descriptors,
  drag-to-reorder. Hosts ship it verbatim and show ONE button (the globe,
  teal accent). The overlay chrome is identical in every synth.
- `test/` — the offline bench. Per module: bounded, no NaN, silence in →
  silence out, bypass bit-transparent, deterministic, unity-ish gain;
  plus convolver correctness vs direct convolution, JSON round-trip,
  unknown-key tolerance, and the empty-rack == bit-identical proof.

## The contract (why hosts never change)

- Rack state is an OPAQUE STRING BLOB keyed by module id + param id,
  stored inside each host's existing state and patch files. Unknown keys
  are ignored; missing modules get defaults. Adding modules or params to
  BWFX therefore requires ZERO changes in any host synth — no APVTS
  edits, no migrations. Rack knobs are not host-automatable (v1).
- The rack is ADDITIVE and DEFAULT EMPTY. Empty = the host's `process`
  call returns without touching the buffers — provably bit-identical.
- Kemper-style compatibility: every new parameter defaults to legacy
  behaviour; a genuinely different algorithm ships as a new module or a
  MkII beside the old one, never replacing it. Module `version` int is
  stored in patches for the day a break is unavoidable.

## Integrating a host (one afternoon, four calls)

1. `rack.process(L, R, n)` once in the master chain (after the synth's
   native output stage). Also call `rack.prepare(fs, maxBlock)` from
   prepareToPlay and `rack.service()` from the editor's ~15 Hz timer
   (IR builds and other message-thread work happen there).
2. Save/restore `rack.toJson()` / `rack.fromJson(...)` in host state AND
   user patch files. A patch stores its own rack; factory patches without
   a blob get `rack.clearState()` (empty).
3. Pipe `{k:"bwfx", ...}` UI messages to the rack and emit rack state to
   the page; include `ui/bwfx-rack.js` in the panel and add the ONE
   BWFX button (globe SVG + teal accent — canonical markup in
   BWFX-DESIGN.md).
4. Map `rack.worldMod()` onto the voice engine once (neutral until the
   SPECTRA characters land).

## Build

No build of its own — hosts compile the sources in
(`add_subdirectory` → target `bwfx`, or just add `src/*.cpp` +
`modules/*.cpp` and the two include dirs). Deliberately not a DLL:
Smart App Control blocks fresh DLL hashes per file, and patches must
stay a promise on every machine.

Bench: `cmake -S test -B test/build` then build Release and run
`bwfxtest.exe`. MSVC needs nothing special (no big stack objects — the
Rack heap-allocates its buffers).

## Inherited hard-won rules (do not relearn)

- Never prepare/rebuild convolution state under the audio thread: IR
  builds happen in `service()` on the message thread, into the inactive
  spectra set, atomically swapped, crossfaded.
- Cubic Catmull-Rom on every modulated delay tap (linear smears the top
  octave and zippers).
- Never inject noise inside a feedback loop.
- Drive gains are exponential (`10^(v/32)`) — the loudest knob rule.
- A module's disabled path must be a SKIP, not a zero-mix — that is what
  makes bypass bit-transparent.
