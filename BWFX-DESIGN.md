# Brokild World FX (BWFX) — design

Settled with Peter 2026-08-26. The shared, global FX system for all Brokild
synths. "World" because the modules are global: drop a new FX into BWFX and,
after a rebuild, every synth offers it.

## What it is

One repo of FX modules — DSP in plain C++17 (JUCE-free, Engine-style,
benchable offline) plus pedal face plates (Photo-Synth 2's pedal look) —
COMPILED INTO every Brokild plugin. Not runtime DLLs, deliberately: Smart
App Control blocks fresh DLL hashes per file, and patches must stay a
promise on every machine.

## The six load-bearing decisions

1. ADDITIVE RACK. Each synth keeps its existing native FX untouched; the
   BWFX rack inserts as one extra stage (before the final limiter), DEFAULT
   EMPTY. Empty = bit-transparent, so the day a synth adopts BWFX, every
   existing patch and factory seed is provably unchanged. Retiring a synth's
   native FX in favour of rack modules is a separate per-synth decision.

2. SELF-DESCRIBING MODULES (the Hairfryer SPECS[] lesson). Each module:
     struct Descriptor { id, name, version, params[]{id,name,def,lo,hi,step} };
     prepare(fs, maxBlock) / reset() / process(L,R,n) / setParam(i,v)
   and a registry bwfx::allModules(). The rack UI, the state format and the
   bench are all GENERATED from the descriptors — one source of truth, so
   the wired-to-the-wrong-knob bug class cannot be expressed.

3. STRING-KEYED OPAQUE RACK STATE, NOT HOST PARAMS. The rack (module list,
   order, per-module params, enables, wet/dry) serialises as JSON keyed by
   module-id + param-id, stored inside the host synth's existing state and
   user patch files. Unknown keys are ignored; missing modules are absent.
   => adding modules/params to BWFX requires ZERO changes in any synth: no
   APVTS layout edits, no id tables, no migrations. v1 cost: rack knobs are
   not host-automatable. v2 option if wanted: a FIXED pool of 8 generic
   "BWFX Macro" host params mappable onto rack knobs (pool never resizes,
   so it never breaks).

4. ONE ADAPTER PER SYNTH, WRITTEN ONCE (~an afternoon each):
   (a) one rack.process(L,R,n) call in the master chain;
   (b) one save/restore of the rack blob in state + user patches;
   (c) one message pipe ({k:"bwfx",...}) to the shared rack UI fragment,
       which renders the pedal faces inside that synth's EFFECT RACK hatch
       (drag-to-reorder, ~30 ms crossfade on reorder so it never clicks)
       and skins itself from per-synth CSS variables (--bwfx-*).
   After that, BWFX updates reach the synth BY REBUILD ALONE, forever.

5. THE RACK IS ITS OWN POP-UP, EVERYWHERE (Peter, 2026-08-26). The rack
   opens as an overlay window inside the plugin - it does NOT reshape any
   synth's panel. The ONLY visible change a synth undergoes when adopting
   BWFX is one button: **BWFX**, wearing the same distinct accent colour in
   every product (slightly but clearly apart from the host's own button
   colour - suggestion: a cool teal/cyan lamp against the usual ambers),
   so it reads as the same global system wherever it appears. The overlay
   CHROME ships with the shared fragment and looks identical in every
   synth (the BWFX brand look, pedal faces and all); only the button obeys
   the host panel's styling plus the accent. This applies to Photo-Synth 2
   too when it adopts the rack - consistency beats its integrated layout.
   For Clone Wars this means a separate BWFX overlay, NOT merged into its
   existing EFFECT RACK hatch; the native four stay where they are.

   THE GLOBE (Peter, 2026-08-26): the BWFX button wears a small minimal
   globe - a sphere with an equatorial and a meridian line. Canonical SVG,
   identical in every synth, strokes inherit the accent via currentColor:

     <svg class="bwfx-globe" viewBox="0 0 16 16" width="13" height="13"
          fill="none" stroke="currentColor" stroke-width="1.2">
       <circle cx="8" cy="8" r="6.2"/>
       <ellipse cx="8" cy="8" rx="6.2" ry="2.5"/>
       <ellipse cx="8" cy="8" rx="2.5" ry="6.2"/>
     </svg>

   The globe + accent colour ARE the BWFX mark; nothing else is branded.

6. THE PANEL VERSIONS WITH THE LIBRARY. The rack overlay - its chrome,
   layout, pedal faces, drag behaviour, every future feature (module
   search, A/B racks, whatever comes) - ships as the UI fragment INSIDE
   the BWFX repo and is compiled into each synth with the DSP. So a BWFX
   update that redesigns or extends the rack panel reaches ALL synths on
   their next rebuild with zero per-synth work, exactly like the DSP does.
   Host synths never own any rack UI beyond the button.

## Patch-compatibility policy (Kemper / Quad Cortex style)

- A patch stores its own rack; a NEW module never touches existing patches.
- Every new parameter DEFAULTS to legacy behaviour — old patches don't
  store it, get the default, sound as before. This is the whole contract.
- Genuine sound improvements and bug fixes flow through to all patches.
- A genuinely different algorithm ships as a new mode or a "MkII" module
  ALONGSIDE the old one, never replacing it. Module version int in the
  descriptor, stored in patches, for the day a break is unavoidable.

## Bench

BWFX carries its own offline bench: per module — bounded, no NaN, silence
in silence out, bypass transparent, deterministic renders, and a level
check (unity-ish small-signal gain). Host synth benches then only assert
"rack empty = bit-identical output". Mind the inherited lessons: never
prepare a Convolution under a message-thread impulse load (irLock), cubic
Catmull-Rom on modulated delay taps, never inject noise inside a feedback
loop, exponential drive gains.

## Rollout

1. Create the BrokildWorldFX repo: registry + Rack + rack-UI fragment +
   bench. Founding modules extracted from Photo-Synth 2's Engine.cpp chain
   (delay, chorus, phaser, stutter, saturation, convolution reverb) with
   its pedal face plates.
2. First citizen: Clone Wars (BUGLIST item 4) — rack in the EFFECT RACK
   hatch alongside its native four, which stay.
3. Then per synth, in any order: Black Rider, Blade Ruiner, Escape Room,
   Hairfryer, The Mars Wars (FX plugin — rack fits naturally). Photo-Synth
   2 already ships the founding chain natively; it adopts the rack whenever
   convenient, unaffected either way.
4. Local checkouts at C:\Users\peter\b\BrokildWorldFX referenced by CMake
   path variable (BWFX_DIR); the Clone Wars CI adds it as a git submodule.
