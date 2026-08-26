# BWFX build — handover

## STATUS 2026-08-27: BWFX 1.5.0 — SPECTRA PHASE C: EVERY SYNTH ON THE BUS

Peter's go ("proceed with Phase B, cautiously" → his four decisions →
"Phase C first"), plus three of his live reports fixed en route:

- **Phase C**: Black Rider, Blade Ruiner, Escape Room and Photo-Synth 2
  now map the world-mod bus (Clone Wars already did) — every SYNTH shows
  the full live SPECTRA rack. Per-synth mapping notes and verification
  status in BWFX-BUGLIST.md item 2c; the pattern is the Clone Wars one
  (golden-angle fans, gate-keyed sag, wmActive guard, memcmp-proven
  neutral bus — bench sections added to BR/BRD/ER; PS2 has no bench, its
  neutral path is exact-identity arithmetic + live CDP).
- **Mars Wars and Hairfryer are NOT synths** (Peter): no bus for them,
  and their racks come OUT entirely in wave 2 — with the PS2 hard-cut
  retirement of internal FX/SPECTRA and the FX-photo remap
  (R→SPACE length, G→ECHO mix, B→TUBE drive). Decisions recorded in
  BWFX-BUGLIST.md item 2-wave-2.
- **Fixed from Peter's live reports**: overlay reorder drag (Chromium
  silently releases pointer capture when the pedal is reparented
  mid-drag — the move/up listeners now live on the document); off-state
  legibility (dim module-colour ember in the OFF rocker lens, off-pedal
  opacity 0.62→0.85); TUBE level neutrality (equal-power blend summed
  correlated dry+wet to +3 dB at mid drive, and the static satComp only
  held at its 0.3 calibration amplitude — now a linear blend plus a slow
  MEASURED RMS auto-gain, worst settled deviation 0.84 dB across the
  whole DRIVE range at two input levels; bench section added).
- Fleet at 260826.5, Clone Wars 260826.7. BWFX bench 364 checks ALL
  CLEAR; BR 6036, BRD and ER ALL CLEAR with new world-mod sections.

## STATUS 2026-08-27: BWFX 1.4.0 — SPECTRA PHASE B + BUILT-IN PRESETS

Peter's go ("proceed with Phase B, cautiously") plus his mid-build rule:
no change to the sound or performance of functioning synths, and presets
built in. Both are structural, not promised: unarmed characters render
memcmp-bit-identically and cost zero audio-thread CPU; presets are data
applied through the same tolerant fromJson path patches ride.

- Four characters: **DARK DRONE** (pure bus: cluster/sag + PS2's exact
  drift law; the sub-oscillator deliberately did not port — no note on
  the bus, an audio octaver would be mush), **PSYCHEDELIC PINK** (swirl
  LFO trio on the bus + private phaser→chorus→reverse-reverb wash),
  **INDUSTRIAL BLACK** (pure FX: crusher→tube→stutter→tape-echo, comb
  left behind on purpose), **GLASS CATHEDRAL** (private hall + the
  0.05 Hz breath; AIR dropped rather than faked).
- New machinery: characters may OWN audio DSP (hasAudio/prepare/
  resetAudio/processAudio/service) reusing the rack's module classes
  privately; the rack crossfades them by presence before the pedal
  chain. **Audio characters work in every host today** — the overlay
  shows them everywhere (per-character `audio` flag in cdesc); pure
  modulators appear only where busLive.
- **10 built-in presets** (VELVET STAGE … POSSESSED CHOIR) in the C++
  (numPresets/presetName/presetBlob), served to the overlay via the
  state payload, applied with the new adapter op "blob". The fragment's
  DEFAULT_CDESC and DEFAULT_PRESETS are GENERATED (`bwfxtest --cdesc` /
  `--presets` + scratchpad/regen-cdesc.js) — never hand-typed.
- Bench grew to 358 checks ALL CLEAR (phase B suite: bus laws measured,
  audio chars additive/deterministic/silence-safe/presence-0-transparent/
  disarm-click-bounded; every preset bounded + silence-safe + the
  KIERANATOR pattern preset round-trips). UI probe 16/16. cwtest ALL
  PASS. One bench lesson repeated itself: pink's slowest LFO (24.6 s
  period) needs a full cycle in the test window or its lower half never
  gets sampled — the vacuous-window trap again.
- Fleet at 260826.4, Clone Wars 260826.6. Phase C (other six host
  mappings) stays on BWFX-BUGLIST.md.

## STATUS 2026-08-26 night: BWFX 1.3.0 — ROTARY, KIERANATOR, SPECTRA PHASE A

The three Fable-tier items from the buglist, in one wave (Fable 5 session):

- **ROTARY** (module 11): a Leslie with real inertia — counter-rotating
  horn/drum, per-rotor AM beam + doppler FM, belt wobble, independent
  spin-up/down ramps (horn ~1 s, drum ~4 s), bench-measured via a
  normalized-autocorrelation envelope-rate estimator. Peter asked for
  extra care on authenticity here; the voicing notes live in
  modules/bwfx_modules.cpp.
- **KIERANATOR** (module 12): the step-sequenced glitcher — 16-step
  pattern painted in the fragment's first custom pedal editor, 8 read-
  effects over a rolling capture buffer, host-bar aligned. Named for the
  late Kieran Foster (dblue, Glitch 2); tribute line on the pedal is one
  line in bwfx-rack.js, easy to retire if Peter decides against it.
  Machinery that came with it: Rack::setTransport, Module extra blobs
  ("x", morph-defect at 0.5), Descriptor::custom.
- **SPECTRA phase A**: the character rack is LIVE — TAPE SEANCE + INSECT
  SWARM publish the world-mod bus {det, pan, tremDepth+Rate, sag,
  filterMul}; presence scales the grip and rides the morph. First host
  mapping: Clone Wars (five bus inputs across the 16 clones, sag keyed
  to smoothed GATE, wmActive-guarded — neutral bus memcmp-identical,
  render_test scenario 8). Hosts that map the bus call
  setWorldModConsumed(true); the overlay shows the live SPECTRA rack
  only there (busLive), placeholder elsewhere. The fragment's
  DEFAULT_CDESC is generated by "bwfxtest --cdesc", same rule as --desc.
- Bench 306 checks ALL CLEAR; SPECTRA UI probe 16/16; CW bench ALL PASS.
  Clone Wars 260826.5, the other six 260826.3. Phases B/C (remaining
  characters, other six host mappings) stay on BWFX-BUGLIST.md.

## STATUS 2026-08-26 late: BWFX 1.2.0 — TEN MODULES, TEMPO SYNC

First cash-in of the BWFX promise: the library grew and every synth got
the new modules BY REBUILD ALONE, no per-synth work beyond one line.

- SYNC (FREE|2/1..1/32) x FEEL (STRAIGHT|TRIPLET|DOTTED) on ECHO, GATE and
  the new HARMONIC. Rack::setBpm + Module::setTempo carry the clock; all
  seven adapters feed it. FREE / no-host-clock are bit-identical to 1.1.0.
- Four new modules: GRIT (PS2 lofi), STRIP (comp + 5-band EQ in one pedal),
  SHIMMER (Blade Ruiner FDN-8, octave-up folded into the loop), HARMONIC
  (brownface opposite-phase split + TREM + true-pitch VIBRATO).
- kMaxParams 8 -> 16 (STRIP). Biquad gained RBJ shelves. The UI fragment's
  DEFAULT_DESC is now GENERATED by "bwfxtest --desc", never hand-typed —
  use that whenever the registry changes.
- Bench 260 checks ALL CLEAR; UI probe 21/21; live CDP round trip.
  All seven built, installed, mirrored, committed; six zips rebuilt;
  Clone Wars 260826.4, the other six 260826.2.
- Built on Opus 5 (Peter's call). Left for Fable deliberately: ROTARY,
  SPECTRA phase A, the GLITCHER machinery — see BWFX-BUGLIST.md.

## STATUS 2026-08-26 evening: ROLLED OUT TO ALL SEVEN SYNTHS

Same session, Peter's go: BWFX 1.1.0 added per-module **PRESENCE** (the
rack-owned dry/wet every pedal carries, teal) and **Rack::applyMorph** —
union of two patches' racks, presence crossfade, staggered stepped
defection, order swap through the dip (bench 211 checks ALL CLEAR; morph
sweep max step == the settled baseline). Clone Wars MORPH now carries the
rack (260826.3, zip cut by CI dispatch, live).

All six remaining synths adopted the rack via the shared adapter header
`adapter/bwfx_juce.h` (the pipe written once): Black Rider, Hairfryer,
The Mars Wars, Blade Ruiner, Escape Room (globe-only button — its panel
is cryptic on purpose), Photo-Synth 2 (rack blob rides INSIDE its preset
files — the native side injects/strips the "bwfx" key so the page's
preset flow is untouched). Each synth: build 260826.1, LoadLibraryW OK,
installed to Common Files\VST3\Brokild + AudioDev mirror, local repo
committed, dist zip rebuilt with the existing manual, landing page +
app.json bumped. Black Rider and Photo-Synth 2 were verified LIVE over
CDP (native round trips incl. presence). Manuals were deliberately NOT
regenerated (Peter's instruction) — they do not mention BWFX yet.

Ideas/fixes/features now collect in **BWFX-BUGLIST.md** (this repo root) and ship in batches when Peter says go, Clone Wars-style. Still open: SPECTRA characters (the world-mod bus is in place, neutral),
per-synth manual sections, and the host synths' worldMod mapping (call
four of the adapter — deferred with SPECTRA).

## PREVIOUS STATUS 2026-08-26 (build session): core + first citizen DONE

- `C:\Users\peter\b\BrokildWorldFX` exists (git repo) and is mirrored at this
  repo's root as `BrokildWorldFX/` (mirror after every BWFX commit:
  `git archive HEAD | tar -x -C <BrokildApps>/BrokildWorldFX`). Six founding
  modules (TUBE, SWEEP, ENSEMBLE, GATE, ECHO, SPACE — SPACE on an own
  partitioned FFT convolver, no JUCE), descriptor-driven Rack, JSON state,
  rack overlay fragment `ui/bwfx-rack.js`, bench 203 checks ALL CLEAR.
- Clone Wars is integrated (build 260826.1, four-call adapter), empty rack
  proven bit-identical in cwtest, verified live over CDP, installed. See the
  260826.1 section of clone-wars/HANDOVER.md — including the new iron rule
  for bwfx-rack.js copies.
- NOT done yet: SPECTRA characters (the world-mod bus exists and is neutral;
  design decision 7 tells you how to port them), the other five synths
  (Black Rider, Blade Ruiner, Escape Room, Hairfryer, Mars Wars —
  ~an afternoon each, same four calls), Photo-Synth 2 whenever convenient,
  and the workflow_dispatch to cut a .1 site zip.
- Deviation from the original plan, deliberate: no separate GitHub repo /
  submodule for BWFX — the mirror lives in THIS repo so CI gets it without
  pinning and BWFX+host changes land in one atomic commit. If a separate
  repo is ever wanted, the folder moves out and BWFX_DIR points at it;
  nothing else changes.

Original handover below, for the record.

---

For the fresh session that builds Brokild World FX. Written 2026-08-26 by the
VS Code session that designed it with Peter. Read this, then the three files
below, before writing anything.

## Read first, in this order

1. `BWFX-DESIGN.md` (this repo's root) — the SEVEN load-bearing decisions.
   The design is buildable from cold and is the authority. Do not re-litigate
   it; Peter settled every point personally.
2. `vst3-apps/clone-wars/HANDOVER.md` — coordination rules (main is truth,
   fetch before editing, build-id-in-four-places, CI cuts the zip, a phone
   session exists), plus the build history .4–.16; local builds have since
   reached .17 (the BUGLIST pass). The SITE ZIP is still .4 — nothing since
   has been cut by workflow_dispatch; do not let that surprise you.
3. `vst3-apps/clone-wars/BUGLIST.md` — UPDATE 2026-08-26: items 1–3 are DONE
   (shipped 260825.17); their diagnoses under Done are worth skimming, the
   entrain finding especially. Item 4 is the only open item. Original note: — items 1–3 are a small batch-fix pass
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
  "Pre-BWFX snapshot, 2026-08-26" commit (CloneWars also has the .17
  BUGLIST-pass commit). COMMIT AT EVERY WORKING STEP.
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
