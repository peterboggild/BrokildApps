# Thin Walls — handover

## 2026-09-22: RELEASED, build 260922.1
Published at https://peterboggild.github.io/BrokildApps/vst3-apps/thin-walls/ — landing
page, 17-page handbook, 8.1 MB zip (the DLL is loaded OUT of the archive and hashed
against the build), preview, manifest entry. Installed in both VST3 houses and loading;
the INSTALLED bytes carry 260922.1. Gates: 90 engine checks, 73 panel checks, manual
overflow/folio/plates all clean over 17 sheets.

Two gate faults were found and fixed while releasing, both worth knowing about:
- `tools/shoot-manual.ps1` had a typed page count of 14 against a 17-page manual, so
  three sheets had never been photographed. The count now comes from the document.
- The folio check tested only LEAF elements, which exempts a paragraph containing a
  link. Pages 13 and 16 were both running under the folio. It now tests anything that
  carries a text node of its own.

Left for the consolidation thread (Peter named it): no sibling landing page links to
Thin Walls, and none links to Battlestar Overdrive either; the Brokild Collection is
still the ten of 2026-09-19 and does not include Thin Walls.

---

# Handover from the build session (2026-09-21)

State: engine finished and bench-proven (67 checks ALL CLEAR), plugin builds (VST3 +
standalone, build 260922.1), installed in both VST3 houses, live-checked with a WAV
playing, committed and pushed to the private repo `brokild-thin-walls` (branch main).
Design + every measured number: `BrokildApps/THIN-WALLS-DESIGN.md`. Contract: `PROTOCOL.md`.
CLAUDE.md has a "Thin Walls" section.

## What is left (all straightforward; fine on Opus)

1. **Panel v2 final.** An Opus subagent was rewriting `Source/ui/ui.html` + `test/uiprobe.js`
   for four sources, the two-way monitor / sphere-on-a-stand models, input selectors and
   room polish. The tree already holds a v2 page (47 params, 4 sources) that runs live;
   check `git status` / the file's date to see whether the agent's final version landed.
   Then: `node test/uiprobe.js` (must be ALL CLEAR), rebuild
   (`cmake --build build --config Release`), `powershell -File tools/live.ps1` and
   `tools/live.ps1 -Jobs test/live-audio-jobs.json` (both print JSON; the audio one
   must show inDb ~ -30 while the test WAV plays, paths switching to portal/leaf/wall
   when source 1 is moved into the hall and door 2 shut), then
   `powershell -File C:\Users\peter\b\BrokildWorldFX\tools\install-fleet.ps1 -Only ThinWalls`.
   Known nit from the release agent: `HINT.save` in ui.html says "19 values"; there are 47.
2. **Release phase 2** — everything is written and waits for the final panel:
   plates `tools/live.ps1 -Jobs test/manual-plates.json` → `tools/crop-plates.ps1`
   (reconcile its crop table with the "rects" job output) → `tools/shoot-manual.ps1`
   (overflow gate) → `powershell -File C:\Users\peter\b\PhotoSynth\devkit\tools\make-pdf.ps1
   -Html docs\manual\manual.html -Pdf docs\manual\Thin-Walls-Manual.pdf` →
   `tools/make-preview.ps1` → `powershell -File test\make-zip.ps1` (relink cleanly first:
   live.ps1 may have appended SAC hash-nudge bytes to the exe) → put the zip size into the
   landing page's two `<small>` tags → serve BrokildApps over http and verify the card,
   the zip and the PDF answer → commit BY EXPLICIT PATH in BrokildApps
   (`vst3-apps/thin-walls/`, `assets/app-previews/thin-walls.jpg`, `manifest.json`,
   `THIN-WALLS-DESIGN.md`; two untracked .code-workspace files there belong to another
   session) and push. Then post the link.
3. **The work-PC failure.** Peter's downloaded exe showed IE's "navigation cancelled" page
   on his WORK PC. The loader is now linked statically and the editor checks for the
   WebView2 runtime (`GetAvailableCoreWebView2BrowserVersionString`) and paints a message
   with the download link when it is absent; the standalone uses a per-launch profile in
   %TEMP%. Not yet confirmed on that PC. If it still fails there: ask for the Windows
   version and whether Edge is installed; the message page will now say which it is.
4. Peter's open wish list, none started: nothing else pending. The bench and the design
   doc list the known limits (fixed floor plan, second-order specular reflections).

## Fleet-wide finding worth a CLAUDE.md line
Every Brokild WebView plugin loads `WebView2Loader.dll` dynamically (JUCE default). On
Peter's home PC it is found only because the Windows Performance Toolkit is on the PATH
(`C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\WebView2Loader.dll`).
JUCE's CMake already links `WebView2LoaderStatic.lib` when NEEDS_WEBVIEW2 is TRUE; adding
`JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` to the target's PUBLIC compile definitions
makes the plugin independent of that. Thin Walls has it; the rest of the fleet does not.
