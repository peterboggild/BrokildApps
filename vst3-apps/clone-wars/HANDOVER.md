# Clone Wars — handover note

For any Claude session (cloud, VS Code, or otherwise) or human picking this up.
Sessions do **not** coordinate with each other automatically — **this repo is
the only shared state**. Read this before changing anything.

Last updated: 2026-08-25, build **260825.3**, by the cloud session that built
the plugin (branch `claude/dex-server-host-9jkd55`).

## Coordination protocol

1. **`main` is the source of truth** and is what the site serves (GitHub
   Pages). At the time of writing, `main` = `cc442a6` and contains everything:
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
  service bay) resets scars *and* the clock. Age/wear/serial live in the DAW
  project state — per instance, deliberately.
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
