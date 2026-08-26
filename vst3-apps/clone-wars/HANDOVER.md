# Clone Wars — handover note

For any Claude session (cloud, VS Code, or otherwise) or human picking this up.
Sessions do **not** coordinate with each other automatically — **this repo is
the only shared state**. Read this before changing anything.

## OPERATOR

As of 2026-08-25 evening, **the VS Code session on Peter's PC is the
operator**: it owns releases, merges to `main`, and the build number.
The cloud session that built 260825.1–.3 stands down; its branch
`claude/dex-server-host-9jkd55` is final history — do not push to it.
Peter can hand the role back at any time by saying so to either session
and updating this line.

Operating the release from the PC needs nothing special:
- Push a green change to `main` (or a branch, then merge).
- Rebuild the download zip with a manual run of the **Clone Wars VST3
  (win64)** workflow on `main` — GitHub → Actions → Run workflow, or
  `gh workflow run clone-wars.yml --ref main`. On green, CI commits
  `Clone-Wars-VST3-win64.zip` to `main` itself (`[skip ci]`); pull after.
- GitHub Pages redeploys `main` automatically; the zip URL caches, so
  verify with `...zip?v=<build>`.

**Top open item for the operator:** the manual
(`tools/cw-manual.html` → `Clone-Wars-Manual.pdf`) still describes the
retired NOTE 1·2·3 slots and doesn't know NOTE MODE, PULSE WIDTH, the
new filters, ENV>FILT or sustain — it ships inside the zip, so it is
now the stalest thing in the box. Regeneration command is in the file
map below.

Last updated: 2026-08-26, build **260826.1**, by the VS Code session on Peter's PC
(see the 260826.1 section below). Before that **260825.17** (the BUGLIST small
pass) and **260825.4**; **260825.3** was the cloud session that built
the plugin (branch `claude/dex-server-host-9jkd55`).

## Coordination protocol

1. **`main` is the source of truth** and is what the site serves (GitHub
   Pages). It contains everything:
   engine, UI, tests, CI, manual, landing page, release zip.
2. **Always `git fetch` + rebase/merge onto `origin/main` before editing.**
   If you find `main` ahead of what you expected, someone else shipped —
   build on top of it, never force-push over it.
3. The cloud session works on `claude/dex-server-host-9jkd55` and merges to
   `main` when the user says ship. If you work in parallel, use your own
   branch and merge through `main`; don't push to the cloud session's branch.
4. **Bump the build number** (`YYMMDD.N`, e.g. 260825.3) on any user-visible
   change. It lives in FOUR places, kept identical:
   - `plugin/Source/ui/ui.html` — panel footer line and the ABOUT hatch
   - `mockup/index.html` — same two spots
   - `index.html` (landing page) — hero build tag and the requirements table
   - `app.json` (note) and `README.txt` (header)
5. Update this note when the state changes hands.


## 260826.1 — BROKILD WORLD FX: first citizen (VS Code session, Peter's PC)

Clone Wars now carries the BWFX rack — the shared, global FX system designed
in `BWFX-DESIGN.md` (repo root; build notes in `BWFX-HANDOVER.md`). The BWFX
source itself lives at the repo root, `BrokildWorldFX/` (mirrored from the
canonical local checkout `C:\Users\peter\b\BrokildWorldFX`, its own git repo);
CI compiles it in via the `BWFX_DIR` CMake path (defaults to the repo layout,
no submodule needed — same-repo atomic commits beat pinning).

- **The adapter is exactly the four contracted calls**: `bwfxRack.process()`
  after `engine.process()` in processBlock; the rack blob in project state
  (`"bwfx"` key) and user patch files; the `{k:"bwfx",...}` message pipe +
  `bwfx-rack.js` served from BinaryData; the world-mod bus mapping is
  DEFERRED until SPECTRA characters exist (the bus is neutral today).
- **The additive contract is proven in cwtest**: empty rack == bit-identical
  output (memcmp over 3 s of drone), an enabled module != identical, and the
  chain stays bounded. Every pre-BWFX project and factory seed sounds
  exactly as before — `fromJson("")` is the default empty rack.
- **A patch stores its own rack** (Kemper rule): user slots save/restore the
  blob; factory seeds and pre-BWFX slots load rack-empty — the seed's
  promised sound. Loading into seed B and MORPH do NOT touch the rack (a
  blob cannot blend); only a seed-A load applies it.
- **Panel**: ONE new control — the BWFX button (globe + teal, `data-bwfx-open`,
  in the expansion-slot row where future modules were always meant to land).
  The overlay ships INSIDE bwfx-rack.js (BWFX brand chrome, Photo-Synth
  pedal faceplates, FX rack left / SPECTRA plate right) — the host page owns
  nothing of it. NEW IRON RULE: `bwfx-rack.js` is edited ONLY in the BWFX
  repo, then copied to `mockup/bwfx-rack.js` and mirrored to
  `BrokildWorldFX/ui/` — the plugin compiles it from `BWFX_DIR` directly.
- **The processor runs its own 15 Hz timer** for `bwfxRack.service()` (reverb
  IR builds): a DAW project restore can enable the reverb with the editor
  closed, so the editor's timer is not enough.
- Verified LIVE over CDP against the standalone: overlay opens, powering
  ECHO + setting TIME 430 round-tripped through the C++ rack and came back
  as native truth on reopen. BWFX's own bench: 203 checks ALL CLEAR
  (convolver vs direct convolution 2.6e-6, click bounds, state round trip).
- CI gained a BWFX bench step and compiles the BWFX sources into the render
  test; push paths now include `BrokildWorldFX/**`.
- Site zip is still whatever workflow_dispatch last cut — dispatch on main
  after merging to ship 260826.1 to the download button.

## 260825.4 — filters, note modes, damage (VS Code session, Peter's PC)

Peter reported four things by ear; all four were real, and all were measured
before and after. Probes live in `plugin/test/` and are marked scratch.

- **The filters are now Black Rider's.** GROWL and SCREAM were one 2-pole TPT
  SVF sharing a K mapping (they rendered *identically* to four decimals), and
  LADDER was a crude cascade running ~7 dB quiet — which is why "any army on
  LADDER made everything go quiet". Ported the Korg35 Sallen-Key ZDF with its
  asymmetric diode clipper, the k35K / k35Kscream mappings, the measured
  piecewise-in-K prewarp, and the ZDF ladder that solves the loop *before*
  saturating. Measured after: self-oscillation lands on the cutoff at **0
  cents** (110/220/440/880 Hz, all three models), and LADDER sits within
  **0.7–2.3 dB** of GROWL. See `test/tuneprobe.cpp`, `test/ladderprobe.cpp`.
- **No cross-army coupling.** A muted army's temper changes the output
  bit-for-bit not at all (`test/mixprobe.cpp`). The shared bus tanh *can* duck
  the mix, so this is worth re-checking after any bus change.
- **CUT had a dead top half.** The filter envelope was added flat (`+ ef*0.45`)
  and `Env` attacked to 1.0 and then HELD, so every sounding note carried a
  permanent +4 octaves and anything above CUT 0.55 clamped to 16 kHz. Now the
  envelope opens the headroom that is LEFT above the knob, with a new global
  **ENV>FILT** depth, and `Env` gained a sustain leg (the amp envelope passes
  1 — a drone must go on droning; the filter envelope passes the shape knob).
- **Seed CUT values were re-mapped** by `(min(1, old + 0.45) - 0.45) / 0.55`, so
  each seed lands where it sounded before. This knowingly bends the "seeds are
  a promise" rule — Peter's explicit call: "much better that the filters sound
  good and right than the existing presets are the same".
- **NOTE MODE** (`g_notemode`) replaces the three note slots: UNISON, TREATY
  POLY (the 16 split left to right, low note to high: 16 / 8-8 / 5-6-5 /
  4-4-4-4 / 3-3-4-3-3, generalised palindromic and centre-weighted in
  `divideClones`) and WAR POLY (the armies contest the chord — A takes the
  lower half, B the upper, sharing the middle note on an odd count).
  `vfNote` is kept for state compatibility but ignored; the **NOTE 1·2·3 row is
  retired and PULSE WIDTH took its place**, so the console neither grew nor
  shrank and no net parameters were added.
- **The oscillator is generated AT the oversampled rate.** It used to run at
  base rate and be interpolated up into the filter, so the quality tiers only
  ever cleaned the filter and could not touch the oscillator's own aliasing.
  **Careful with the audit's aliasing table**: it runs all 16 detuned clones, so
  every other clone's fundamental lands off the harmonic grid and counts as
  "non-harmonic" — it floors near −47 dB whatever the oscillator does, and it
  is not an aliasing measurement. `test/aliasprobe.cpp` isolates one clone with
  nothing detuned: saw at 2 foot measures **−41.3 / −49.7 / −51.2 dB** for
  LOW / HQ / XHQ. The remaining floor is the averaging decimator; a polyphase
  halfband (as in Mars Wars) is the next step if more is wanted.
- **Damage is the unit's, never the patch's.** `wearPoints` defaulted to
  **300.0** and `wearSeed` to a fixed **1337**, so every instance was born
  scarred, and identically scarred. Now 0.0 and a per-instance draw. The panel
  also *ignored* the `wearSeed` the processor sends, and read wear from
  localStorage; both fixed. Verified headless: wear 0 paints 0 pixels on the
  patina canvas, wear 40 paints 4,266.
- **SCATTER and LFO SYNC are wired.** SCATTER sends `{k:"scatter"}` and the
  engine re-draws all 16 LFO phases at the next sub-block; LFO SYNC is a real
  parameter (`g_lfosync`) and the processor feeds host BPM to `Engine::setBpm`.

**Open:** the release zip is still the .3 build — dispatch the workflow to cut
a .4 zip. `plugin/test/CMakeLists.txt` is new, with two MSVC-only fixes kept
there so the shipped sources stay untouched: MSVC has no `M_PI`, and
`cw::Engine` as a stack local overflows Windows' 1 MB default stack
(`/STACK:33554432`) where Linux CI gets 8 MB — without it both benches die with
0xC00000FD before printing a line.
## 260825.9 - the EFFECT RACK

The fx bank (SPRING/BBD/TAPE/DRIVE) left the centre console for its own
pop-up hatch, engine-room pattern: EFFECTS button (where the ? was; the
field manual ? moved to the first expansion-slot button). This shortened
the console considerably. A global FX MIX knob sits on the front console
(g_fxmix, default 1.0 = the old sound): it blends dry console vs the whole
rack around sat/drive/bbd/tape/spring; the corrective bus (bass mono, hpf,
width, master) stays in circuit.

**Future FX modules go in the EFFECT RACK hatch** - Peter has said so
explicitly. The MODULE BAY idea folds into this: the rack shows an
EXPANSION SLOTS row where new modules land.

## 260825.13 - the WAR ARCHIVE

generatePatch is now a five-band ESCALATION generator: 0-19 TENSION (musical,
mild), 20-39 CRISIS, 40-59 SKIRMISH, 60-79 BATTLE, 80-99 MAYHEM (dissonant,
screaming). Seeds 100-199 are USER SLOTS - JSON files by param id in
"Clone Wars User Patches" beside the .vst3 (APPDATA fallback; write-probe,
never hasWriteAccess). resolvePatch() feeds seed loads, MORPH and boot from
one place; loading an empty slot is refused. The old abyss/swarm/engines/
cathedral/rust families are gone - every seed sounds different from .12.
The 100 factory NAMES live in the panel JS (ARCHIVE_NAMES), not in C++.
PATCHES opens the WAR ARCHIVE hatch: five band tabs + USER, ->A / ->B load
targets, name field + SAVE. MORPH disarms on any hand edit (grey knob, ON
button re-arms, loads re-arm) and the whole console now repaints ~15 Hz
during a morph so what you see is always the state. Envelope attacks and
releases reach 20 s. NOTE: audit.cpp / render_test.cpp were updated for the
new fields - a stale test exe prints ALL PASS from before the change, so
delete the exe if the build errors and the run still passes.

## Release pipeline (how a build reaches the download button)

- Any push touching `plugin/**` runs CI (`.github/workflows/clone-wars.yml`):
  Linux engine tests + FFT audit, then a win64 VST3+Standalone build,
  uploaded as an Actions artifact only.
- A **manual `workflow_dispatch`** of the same workflow additionally packages
  the house-style zip (VST3 + `Clone Wars.exe` + manual PDF + README.txt) and
  **commits `Clone-Wars-VST3-win64.zip` to the branch it ran on** with
  `[skip ci]`. Never hand-edit or locally rebuild that zip — dispatch the
  workflow so the shipped binary always matches a green CI run.
- Merge to `main` + push = the site is live. Sequence for a release:
  push feature → CI green → dispatch → pull the zip commit → merge to `main`.

## Map of the machine

- `plugin/Source/Core/cw_core.{h,cpp}` — the whole DSP engine, pure C++17,
  JUCE-free, testable with plain g++ on Linux. 16 voices, two armies,
  three note slots, seed patch generator (five families), quality tiers.
- `plugin/Source/PluginProcessor.*` — JUCE shell: APVTS built from the
  `cw::` id tables (`g_<name>`, `v{01..16}_<field>`), note slots, per-instance
  age/wear/serial persisted in project state, UI message handling.
- `plugin/Source/PluginEditor.*` — WebView2 editor serving `ui.html` from
  BinaryData. `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` in CMakeLists is
  **load-bearing**: without it JUCE silently falls back to the IE backend.
- `plugin/Source/ui/ui.html` — the plugin panel = the mockup **plus** a
  spliced bridge script at the end (look for `CW_BRIDGED`).
- `mockup/index.html` — the same panel, standalone, no bridge.
- `plugin/test/render_test.cpp`, `plugin/test/audit.cpp` — headless tests,
  both run in CI. `cw::Engine` is ~2.2 MB — heap-allocate in tests if you
  need more than one alive (three on the stack overflowed it once).
- `tools/embed-decals.py` — regenerates the damage-decal data URIs in both
  panel files from `assets/damage-decals/` (repo root). Re-runnable.
- `tools/cw-manual.html` — the manual's source. Regenerate the PDF with:
  `chromium --headless --no-pdf-header-footer --print-to-pdf=../Clone-Wars-Manual.pdf cw-manual.html`
  (any current Chrome; images referenced relative to this folder).

## Iron rules (learned the hard way)

- **Two panel files, one panel.** Every UI change goes to BOTH
  `plugin/Source/ui/ui.html` and `mockup/index.html`, identically, except
  bridge code which exists only in the plugin copy. The files are ~1.5 MB
  (embedded decal data URIs) — patch them with small python scripts that
  assert exact-match counts; don't let an editor reformat them.
- **Seeds are a promise.** A seed number must produce the same sound forever,
  on every machine. If you change any table a seed reads (e.g. the footage
  list gained 64' at the front in 260825.3), remap indices so existing seeds
  keep their sound, and verify: the render test prints per-seed stats that
  must not move.
- **Silent open.** A fresh instance makes no sound until played: DRONE and
  the latches default off. There's a test asserting it; keep it passing.
- **Offline render is always XHQ** regardless of the panel tier switch.
- **Patina has no off switch** by design. REPAIR (shift-click the odometer →
  service bay) resets scars *and* the clock. SINCE 260825.15 damage is NEVER
  read back from state: hosts use the same getState/setState for projects AND
  preset files, so restoring it stamped a preset's scars onto the instance.
  Wear now depends on nothing but the time the instance has spent playing
  (still written to state, harmlessly, never read).
- Real-time code: no allocation, locks, or logging in `process()`.

## Current state / open threads

- Build 260825.3 live: silent open, field keyboard in its own bay (2 s
  spaceship landing), ABOUT hatch, LOW/HQ/XHQ tiers (measured, audited),
  photographic patina, row locking (shift-click; ctrl = half-row unison;
  shift-ctrl = half-row keeping intervals; per-side rail lock tools),
  THE RANKS sync/desync contrast slider, footage 64'–2'.
- **State-compat note:** projects saved with 260825.1/.2 restore footage one
  step low (the list grew at the front). Known, accepted, documented.
- Engine-room `SCATTER` and `LFO SYNC` buttons are panel-fitted but **not
  wired** — no engine backing yet. The manual says "wiring scheduled".
- MODULE BAY is empty on purpose (future Photo-Synth-style modules).
- Unmerged side branch `claude/brokildapps-app-order-oyvj0e` carries one
  commit ("Sleeper Agent: add a rain noise track", Aug 24) that is not on
  `main` — unrelated to Clone Wars, left for its author to merge.
- Future wishes mentioned but not requested yet: FX musical tuning pass by
  ear, wiring SCATTER/LFO SYNC, first module for the bay.
