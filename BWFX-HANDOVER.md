# BWFX build — handover

For the fresh session that builds Brokild World FX. Written 2026-08-26 by the
VS Code session that designed it with Peter. Read this, then the three files
below, before writing anything.

## Read first, in this order

1. `BWFX-DESIGN.md` (this repo's root) — the SEVEN load-bearing decisions.
   The design is buildable from cold and is the authority. Do not re-litigate
   it; Peter settled every point personally.
2. `vst3-apps/clone-wars/HANDOVER.md` — coordination rules (main is truth,
   fetch before editing, build-id-in-four-places, CI cuts the zip, a phone
   session exists), plus the build history .4–.16.
3. `vst3-apps/clone-wars/BUGLIST.md` — items 1–3 are a small batch-fix pass
   (footage instant, phantom release note [diagnosed, fix specified],
   shift-drag marquee). If still Open when you start, do them FIRST as one
   Clone Wars build — they are an hour and independent of BWFX. Item 4 IS
   the BWFX first-citizen work.
4. Workspace `CLAUDE.md` — machine facts. Non-negotiables that bite:
   Smart App Control blocks fresh binaries BY HASH (relink on "Permission
   denied"/4551 — even a bench exe); a STALE test exe prints ALL PASS
   without compiling, delete it if the build errored; patch big files with
   node scripts that normalise CRLF and abort on any missed anchor
   (never heredocs with backslashes).

## Safety state (already done — do not redo)

- All 7 synth trees in `C:\Users\peter\b\` have local git repos with a
  "Pre-BWFX snapshot, 2026-08-26" commit. COMMIT AT EVERY WORKING STEP.
- Off-machine tarball: `00 VSCODE/BrokildBackups/pre-bwfx-snapshot-2026-08-26.tar.gz`.
- Clone Wars canonical source is `vst3-apps/clone-wars/plugin/` in THIS repo
  (pushed); `C:\Users\peter\b\CloneWars` is the build mirror — every change
  must land in both (copy back after editing, see its HANDOVER).

## Environment facts you will need

- Build: `cmake -S . -B build -G "Visual Studio 18 2026" -A x64
  -DJUCE_DIR=C:/Users/peter/AudioDev/Projects/BrokildVSTTemplate/external/JUCE`
  then `cmake --build build --config Release`. (README's C:/AudioDev/JUCE
  does not exist.)
- Install: copy the whole `<Name>.vst3` FOLDER to
  `C:\Program Files\Common Files\VST3\Brokild\`, then verify with a
  LoadLibraryW probe (pattern in CLAUDE.md / this week's commits).
- Benches: engine-only exes need `_USE_MATH_DEFINES` and `/STACK:33554432`
  (cw::Engine is a stack local, ~2.2 MB+); see clone-wars `plugin/test/CMakeLists.txt`.
- Panel testing: headless Chrome + injected error trap + stubbed
  `window.__JUCE__` + real PointerEvents; assert SENT MESSAGES, never just
  querySelector — a parse error leaves the DOM intact and every control dead
  (the .4/.5 disaster). Probe pattern: this session's scratchpad
  `audit-probe.js` (rebuild it from the description in clone-wars HANDOVER
  260825.6 notes if gone).
- Clone Wars panel rule: EVERY UI change goes to BOTH
  `plugin/Source/ui/ui.html` AND `mockup/index.html` (bridge code ui-only).
- Build id: bump in ui.html + mockup + landing index.html + app.json +
  README.txt, all identically. Site zip is cut ONLY by a manual
  workflow_dispatch of `.github/workflows/clone-wars.yml` (zip currently .4;
  everything since is installed locally only).

## The BWFX build, concretely

1. Create `C:\Users\peter\b\BrokildWorldFX` (git init immediately):
   - `src/` — bwfx core: Descriptor/Module interface, registry, Rack
     (order, enables, per-module params, wet/dry, ~30 ms reorder crossfade),
     JSON state (string-keyed by module id + param id, unknown keys ignored),
     the fixed world-modulation bus {detuneCents, panSpread, tremolo,
     pitchSag, filterMul}.
   - `modules/` — founding set EXTRACTED from
     `C:\Users\peter\b\PhotoSynth\Source\Engine.cpp` (delay, chorus, phaser,
     stutter, saturation, convolution reverb). Mind: irLock (never prepare
     Convolution under a message-thread impulse load), Catmull-Rom taps,
     exponential drive gains, never inject noise inside a feedback loop.
   - `ui/` — the rack overlay fragment: FX rack LEFT, SPECTRA rack RIGHT,
     Photo-Synth pedal faceplates + `.spec-rocker` look (markup/CSS in
     `PhotoSynth\Source\ui\ui.html`), drag-to-reorder, skinned via
     `--bwfx-*` CSS variables. The canonical globe SVG is in BWFX-DESIGN.md.
   - `test/` — per-module bench: bounded, no NaN, silence in/out, bypass
     transparent, deterministic, unity-ish gain. Runs with plain MSVC like
     the other benches.
   - SPECTRA characters (Dark Drone, tape/insect/pink/glass...) come via the
     modulation bus, NOT via host-control snapshots (design decision 7);
     port their `mods(dt,P)` behaviour from PhotoSynth's ui.html SPEC_MODULES
     + Engine hooks. They may come in a SECOND pass after the FX rack works.
2. First citizen: Clone Wars. Adapter = four calls (process, state blob,
   message pipe, mod-bus mapping) + ONE header button: the globe + teal
   accent, its own overlay, NOT inside the existing EFFECT RACK hatch.
   Rack default EMPTY: prove `rack empty == bit-identical` in cwtest before
   anything else. Native four FX stay.
3. Then per synth in any order (each ~an afternoon, same four calls).
   Photo-Synth 2 last/whenever — its native chain already ships.
4. Every synth's session ends: benches ALL PASS, LoadLibraryW OK, installed,
   committed (local repo AND, for Clone Wars, this repo pushed).

## Working with Peter

- He tests in Ableton and reports by ear; his reports have been RIGHT every
  time this week (dead bridge, ladder level, phantom note, seed-keyed grime).
  Measure before dismissing, measure after fixing; put numbers in replies.
- Voice notifications: `Add-Type -AssemblyName System.Speech; ...Speak(...)` —
  say "I have new information", wait 5 s, then the message. He is often away
  from the screen.
- New bugs go on BUGLIST.md, batched; he says go for a build.
- He prefers the finished artefact over questions on open-ended briefs, but
  asked to be asked when designs genuinely fork (AskUserQuestion worked well).
