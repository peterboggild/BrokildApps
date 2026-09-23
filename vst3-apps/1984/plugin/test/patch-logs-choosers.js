// Records the chooser round and the RACK MIX fix in CLAUDE.md, and puts the
// pending fleet rebuild for BWFX 1.7.1 on the BWFX bug list.
"use strict";
const fs = require("fs");
function patch(p, from, to) {
  const raw = fs.readFileSync(p, "utf8");
  const crlf = raw.indexOf("\r\n") >= 0;
  const s = raw.replace(/\r\n/g, "\n");
  const parts = s.split(from);
  if (parts.length !== 2) { console.error("ANCHOR MISS (" + (parts.length - 1) + ") in " + p); process.exit(1); }
  const out = parts.join(to);
  fs.writeFileSync(p, crlf ? out.replace(/\n/g, "\r\n") : out);
  console.log("patched " + p);
}

const claude = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
patch(claude,
"- Not built, deliberately: MPE; a vocoder (the choir is a fixed bank by design and a real one needs a sidechain bus); the toggle decals (unregistered pair).\n",
[
"- **2026-09-23 build 260923.1 — the choosers** (Peter: \"the HTML-like menu controls break the illusion… the 'choosers' often just look like a standard gui\"). The list boxes in the slider rows (MODE, QUALITY, FEET, CIRCUIT) are chrome SLIDE SWITCHES in a groove with a printed legend and an LED per position, in the SAME cell the list box had, so nothing in the layout moved; the header's patch `<select>` is a VFD-style window that opens a hand-built BANK CHART (nine category columns, numbered entries) on the BODY — the deck carries a transform, the Photo-Synth fly-out lesson. The hidden `<select id=patchSel>` stays as the value store and the legends keep the `.seg b` class, so nothing that addressed either changed. A long legend on a two-position switch breaks at its first space on purpose (\"LADDER\" over \"24 dB\") instead of rag-wrapping. **A screenshot over CDP fires a window RESIZE** and the chart first closed mid-shot; a resize now RE-PLACES it. Panel probe 113 checks; plates, preview, 18-page manual and zip re-cut; both houses load.",
"- **The RACK MIX report (Peter: \"when BWFX are on and rack mix is reduced to zero, FXs still play\") was a BWFX FRAGMENT fault, fleet-wide since 1.7.0, fixed in BWFX 1.7.1.** Macro 5 ships wired to `mix` at −100 % and a macro MAPS its destination, so the rack recomputes `mixOff = mapped − base` every block: a raw `setMix(0)` from the overlay was put straight back to `1 − macro5`. The overlay already painted the slider teal as owned and moved it to where the macro put it; what it never did was route a hand on it THROUGH the macro. `driveOwned()` now inverts the mapping (the same law as `Rack::applyMacros`, sign and clamp) and sends `{op:\"macro\"}` — the rail's own path — for mix, module parameters and presence. **Proven at the rack, not the overlay: `{op:\"macro\"}` returns no state echo, so `BWFX.debug().rack` read stale until an `op:\"init\"` re-asked — then macro 5 = 1, off = −1, effective mix 0, and back.** The fragment is compiled into every plugin: 1984 carries it; **the other synths get it on their next rebuild** (BWFX-BUGLIST item 18, awaiting go).",
"- Not built, deliberately: MPE; a vocoder (the choir is a fixed bank by design and a real one needs a sidechain bus); the toggle decals (unregistered pair).",
""].join("\n"));

const bug = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BWFX-BUGLIST.md";
patch(bug,
"## Open\n",
[
"## Open",
"",
"### 18. Fleet rebuild for BWFX 1.7.1 (RACK MIX drives macro 5) — awaiting go *(2026-09-23)*",
"",
"Found on 1984: with the rack on and RACK MIX dragged to zero, the effects kept playing. Macro 5 ships wired to `mix` at −100 % and a macro MAPS its destination, so the rack recomputed `mixOff = mapped − base` every block and put a raw mix write straight back. BWFX 1.7.1 fixes it in the FRAGMENT (`driveOwned()`: a hand on any macro-owned control — mix, a module parameter, a presence — inverts the mapping and moves the macro's host parameter instead). The fragment is compiled into every plugin, so **every synth built against 1.7.0 still has the dead RACK MIX slider until it is rebuilt**: Black Rider, Blade Ruiner, Escape Room, Full Metal Racket, Photo Synth, Clone Wars (CI dispatch), High Tide, Brain Scan, the four Artefacts, Legion and Rite of Passage (native panels — check whether their RACK MIX writes the base too). 1984 carries 1.7.1 already. Rebuild = `cmake --build` per tree + `install-fleet.ps1` + re-cut each zip; nothing else changes (core untouched, bench 409 ALL CLEAR).",
"",
""].join("\n"));
