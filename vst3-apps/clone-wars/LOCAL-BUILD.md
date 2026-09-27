# Building Clone Wars locally

**There is one copy of this source and you are looking at it.** It lives here,
in the BrokildApps repo, because GitHub Actions builds the shipped zip from
this path. Edit it here. Do not keep a second copy anywhere.

There used to be one at `C:\Users\peter\b\CloneWars`, and on 2026-08-30 it was
found 41 lines stale — a panel fix made in this tree had never been copied
across, and a CI build would have shipped the old file. That copy is retired to
`C:\Users\peter\b\_retired\`. Its C++ was byte-identical to this tree, so
nothing was lost.

## Build

Configure with the **build directory outside Dropbox** — build output is
hundreds of megabytes and would otherwise churn through sync:

```
cmake -S "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/clone-wars/plugin" ^
      -B "C:/Users/peter/b/_build/CloneWars" ^
      -DJUCE_DIR="C:/Users/peter/AudioDev/Projects/BrokildVSTTemplate/external/JUCE" ^
      -DBWFX_DIR="C:/Users/peter/b/BrokildWorldFX"
cmake --build "C:/Users/peter/b/_build/CloneWars" --config Release
```

`JUCE_DIR` is required — the README's `C:/AudioDev/JUCE` does not exist on this
machine. `BWFX_DIR` points at the canonical BWFX checkout; the copy mirrored
into this repo is for CI, which has no access to `b/`.

Two MSVC-only fixes live in `test/CMakeLists.txt` so the shipped sources stay
untouched: `_USE_MATH_DEFINES` (no `M_PI` in MSVC — the benches were written
for gcc on CI) and `/STACK:33554432`, because `cw::Engine` is a stack local and
its delay lines alone are ~512 KB against Windows' 1 MB default. Without the
latter you get `0xC00000FD` before a single line prints, which git-bash
surfaces only as a bare exit 127.

## Shipping

**Never hand-build the published zip.** A manual `workflow_dispatch` of
`.github/workflows/clone-wars.yml` builds it and commits it with `[skip ci]`,
so the shipped binary always matches a green CI run — and CI produces a real
`moduleinfo.json`, which a local build here often cannot (Smart App Control
blocks the freshly built `vst3_helper.exe` and leaves a zero-byte file).

Building locally for testing and installing is fine and expected. Just do not
let a local build become the published artefact.

## The iron rules

* Every UI change goes into **both** `plugin/Source/ui/ui.html` and
  `mockup/index.html` — identical except the bridge script (`CW_BRIDGED`).
* The build id lives in **four** places: `ui.html`, `mockup/index.html`, the
  landing `index.html`, and `app.json` + `README.txt`.
* A fresh instance must be silent (DRONE and latches off). There is a test.
* Offline render is always XHQ.
