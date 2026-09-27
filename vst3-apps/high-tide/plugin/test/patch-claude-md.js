"use strict";
const fs = require("fs");
const f = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(f, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
function rep(a, b) { const c = s.split(a).length - 1; if (c !== 1) { misses.push("expected 1 of " + JSON.stringify(a.slice(0, 70)) + ", found " + c); return; } s = s.split(a).join(b); }

rep("| **Clone Wars** | **`BrokildApps/vst3-apps/clone-wars/`** — in the website repo, so CI can build it | none; it is in BrokildApps |",
    "| **Clone Wars** | **`BrokildApps/vst3-apps/clone-wars/`** — in the website repo, so CI can build it | none; it is in BrokildApps |" + NL +
    "| High Tide | `b\\HighTide` | `brokild-high-tide` |");

const oldHead = "### High Tide — DESIGN ONLY (2026-09-03)";
const i = s.indexOf(oldHead);
if (i < 0) misses.push("no High Tide section");
else {
  s = s.slice(0, i) + [
"### High Tide (`C:\\Users\\peter\\b\\HighTide`) — the terrain wavetable synth, BUILT 2026-09-04",
"- **Shipped 2026-09-04 as build 260904.1** in one session from Peter's \"Can you do it?\" (priorities he set: sound first, workflow second, graphics original third). Design `BrokildApps/HIGH-TIDE-DESIGN.md` (§12 = what the build taught, every line a bench number), contract `PROTOCOL.md` in the tree (page ↔ native; the panel and the engine were built in parallel against it). Plugin code `HiTd`, PRODUCT_NAME \"High Tide\", Brokild collection folder, patches `Documents\\Brokild patches\\High Tide`, published at `vst3-apps/high-tide/` (landing, 15-page manual, 7.1 MB zip verified byte-identical to the build), preview + manifest entry. Private repo `brokild-high-tide`.",
"- **What it is:** a wavetable synth that stores no waveforms. Terrain = heightmap 512×128 (`Terrain` in Engine.h), a frame is a bowl, the voice is a ball under velocity Verlet at 4x (2x is the economy); position is a slow damped second coordinate towed by a tether (HOLD) toward a PIN lane's target across a relief that TIDE floods (`U_eff = U − R + max(R, T)`); ROCK/BREATH is the Duffing/Mathieu drive. Isochrone by Landau's width function → TUNE LOCK in the page keeps pitch exact while sculpting. Bench `test/bench.cpp` 123 checks ALL CLEAR (`test/run-bench.ps1` past SAC); panel probe `test/uiprobe.js` 12/12 in headless Chrome; live CDP `tools/live.ps1` + `tools/cdp.js` + `test/live-jobs.json`; plates `test/plates-jobs.json`; zip `test/make-zip.ps1`.",
"- **The five lessons that cost the most (details in the design doc §12):** (1) sub-step the integrator where the terrain is stiff AND decide the budget from the look-ahead position, or a wall bounce jitters the period (+18 dB non-harmonic); (2) LEVELS OF DETAIL — blur the terrain to the ball's step per note (σ=2^L cells), the terrain synth's mip-map; without it cell-scale detail is noise at high notes; (3) a hard stiffness knee cannot be integrated at any sub-step count — round it over the brush floor (factory BOX = softplus knee); (4) a probe that measures harmonics of the NOTE lies about a bowl that plays its OWN pitch — measure the actual fundamental first (half the first bench run's failures); (5) `juce::String(const char*)` is Latin-1: wrap UTF-8 table strings in `CharPointer_UTF8` or an em dash ships as mojibake in a plate.",
"- **Panel:** `Source/ui/ui.html` (~1470 dense lines, written by a subagent in three parts and joined by `cat`; the agent was cut off by a rate limit before the join). WebGL2 terrain with effective-terrain rendering (flooded columns rise to the water line — the picture IS the mechanics), sculpt rail with tune lock, tide-gauge controls generated from `initialState`, cream timeline with two anchors + loop + wake. `window.__HT` debug hooks (the BWFX.debug lesson). Decals ordered via `assets/high-tide-decals/BRIEF.md` (12 parts) — they slot in through the `--decal-*` CSS variables; none delivered yet, the panel is fully procedural.",
"- **Not built (deliberately):** the 2-D bowl (design §3.2), the bifurcation view (§3.4), MPE. The mod wheel and channel pressure lift the TIDE.",
"- **Lesson on delegation:** the UI subagent hit the account's session rate limit mid-write and died; its three part files in the scratchpad were complete and joined fine. Have agents write parts to the TREE (not the scratchpad) and join as they go.",
""].join(NL);
}
if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("CLAUDE.md patched");
