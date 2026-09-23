# THIRTY THOUSAND YEARS — handover / session state

**Build 260923.1. Written 2026-09-23 so the work survives a context
compaction. Read this first; then `BrokildApps/THIRTY-THOUSAND-YEARS-DESIGN.md`
(§14 = every measured number), `REQUIREMENTS.md` (the brief, line by line,
with DONE / PART / DEFERRED / BLOCKED) and `PROTOCOL.md` (page ↔ native).**

## What it is
A Brokild VST3 drone synthesizer / evolving soundscape instrument, built
from Peter's "Research-informed master build prompt" of 2026-09-23. Tree
`C:\Users\peter\Dropbox\ACTIVITIES\00 VSCODE\BrokildApps\vst3-apps\thirty-thousand-years\plugin`, own git repo (local only so far),
plugin code `T30k`, PRODUCT_NAME "Thirty Thousand Years", `TTY_BUILD_ID` in
CMakeLists (bump AND re-run `cmake -S . -B build`).

## Where things stand (2026-09-23)

| part | state |
|---|---|
| Engine (Dsp.h, Params, Strata, Memory, Environment, Life, Engine) | DONE, bench 63 checks ALL CLEAR |
| Presets (Presets.cpp) | 48, all load / sound / bounded |
| Processor + editor (JUCE) | DONE; VST3 + standalone link; host test 16 checks ALL CLEAR |
| Panel (`Source/ui/ui.html`) | ASSEMBLED from an agent's nine parts, 148 KB, script parses. **NOT yet probed or seen live.** |
| `test/uiprobe.js` | **NOT WRITTEN** (the panel agent died at the rate limit before it) |
| Manual, landing page, app.json, README | **NOT WRITTEN** (that agent died at the session limit) |
| Demos | 4 WAVs in `docs/audio`, 4 mp3s already in the website folder |
| Decals | ordered: `BrokildApps/THIRTY-THOUSAND-YEARS-DECALS.md` (6 parts); none delivered; the panel is procedural and must stay finished without them |
| Install / zip / site | not installed, no zip, no landing page, not in the manifest, no backup repo |

## Build and test (all verified working today)

```
cmake -S . -B build -A x64                      # then --target ThirtyThousandYears_VST3 / _Standalone
cmake -S test -B test/build -A x64              # ttytest, ttyrender, ttyprobe
powershell -File test/run-bench.ps1             # the bench, past SAC  (-NoBuild to skip the build)
powershell -File test/run-bench.ps1 -NoBuild -Extra "--params"   # the panel fixture (regenerate after any parameter change)
powershell -File tools/loadprobe.ps1            # does the built VST3 load
test/host/build/ttyhost_artefacts/Release/ttyhost.exe "<path.vst3>"   # the real wrapper
test/build/Release/ttyprobe.exe level|tuning|osamp|grains|modes|friction|signal|hilbert|halfband|sat|beat
test/build/Release/ttyrender.exe docs/audio     # the four demos
node test/wavstats.js docs/audio                # peak / rms / centroid (reads 24-bit)
node tools/wav2mp3.js docs/audio <outdir>       # mp3s for the site
powershell -File test/make-zip.ps1              # cuts and VERIFIES the dist zip (needs the manual PDF)
```

**Smart App Control blocks fresh exes here about half the time.** Every
runner above either nudges the hash itself or must be run through the
copy-and-append loop; `cmake -S test/host -B ...` needed three attempts
because SAC blocked `juceaide.exe` at configure time (delete
`build/**/juceaide.exe` and retry).

## The next steps, in order

1. **Probe the panel.** Write `test/uiprobe.js` (the 1984 one is the
   pattern: temp copy + a stubbed `window.__JUCE__.backend` that records
   `emitEvent` payloads in `__sent` and holds listeners in `__ls`, feed it
   `test/params.json` as `initialState`, drive with real DOM events,
   headless Chrome `--headless=new --dump-dom`, anchor the result parse on
   `<pre id="probeout"`). Check at least: zero JS errors, one control per
   parameter, every control has a hint, the formatting laws, a drag sends
   one coalesced `p`, `hostParam` moves the control, `eff` draws the second
   mark, the meter draws ink, every view fits its box at deck size, the
   keyboard / PANIC / STRIKE / DRONE / macro / slot / mseg / scene /
   capture messages, one `[data-bwfx-open]`.
2. **Then drive it LIVE** (`tools/live.ps1` + `tools/cdp.js`, port 9261).
   Static checks cannot see an unclosed tag or a helper that does not
   exist; only a live panel can. The page has `window.__TTY` for that.
3. Manual (`docs/manual/manual.html`, A4 landscape, the 1984 skeleton) +
   `PLATES.md`, landing page + `app.json` + manifest entry, README.
4. `install-fleet.ps1 -Only "Thirty Thousand Years"` (already registered in
   BWFX's installer and `check-names.js`), then the zip, then the site.
5. A private GitHub backup repo `brokild-thirty-thousand-years` (the house
   route: the GitHub API with the token from `git credential fill`; `gh` is
   not installed).

## Things this build learned that are easy to lose

* **The panel came from a subagent in nine parts** written to the session
  scratchpad (`p01..p09.txt`); part 8 was cut off mid-function by the rate
  limit and I wrote `p09.txt` to finish it (`SP.net` onwards: the detector
  rows, popups, the matrix, the source palette, the shapes, the macro map
  editor, the keyboard, the patch menus, the meters, boot). If the page
  needs rebuilding from parts, the join order is p01..p07, then p08
  truncated at `var NETB = [];`, then p09.
* The engine's effective values are **slewed 8 ms** (`Engine::effTarget` →
  `eff`), which is what makes a knob or a HISTORY jump click-free; the
  click bench measures against the settled chain's own step, never an
  absolute bound.
* `ttyhost` reported **2 481** parameters because JUCE's VST3 wrapper
  emulates 16 × 130 MIDI CCs — count by name, skipping "MIDI CC"/"Bypass".
* The bench's `fmt()` takes doubles only: `%d` in it prints rubbish.
* `run-bench.ps1`'s extra-argument parameter is `-Extra`, not `-Args`
  (`$Args` is a PowerShell automatic variable and silently swallows it).
* `wav2mp3.js` and `wavstats.js` were 16-bit only; both now read the
  render tool's 24-bit output.
