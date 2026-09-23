"use strict";
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
const raw = fs.readFileSync(p, "utf8");
const NL = raw.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
let s = raw.replace(/\r\n/g, "\n");
const anchor = "- Not built, deliberately: MPE; a vocoder (the choir is a fixed bank by design and a real one needs a sidechain bus); decals (ordered in `1984-DECALS.md`; every hook has its procedural fallback and the panel is finished without them).\n";
if (s.split(anchor).length !== 2) { console.error("anchor miss"); process.exit(1); }
const add = [
"- **2026-09-23, the decals arrived (`Downloads\\1984-decals.zip`) and went in the same morning.** All seventeen at spec dimensions with honest alpha (bodies 252/255), measured by me before believing the delivery's own table. Fifteen accepted; the toggle pair refused (the two bezels are not the same pixels, so UP and DOWN would jump — the delivery said so itself), the procedural toggles stay. Two ingest fixes: **the panel tile was not certified seamless, so it is mirror-tiled 2×2 and resampled back to 1024²** (seamless by construction); **the smoked-glass overlay is opaque and the page composites it OVER the readout**, so it ships at 28 % alpha or the display vanishes behind its own glass. Plumbing: the page asks for `assets/decals/<file>`; BinaryData knows basenames, so the resource provider now matches on the last path component; the PNGs ride in via a `file(GLOB)` in CMake. Verified LIVE: `__N84.decals()` reports 15, the html carries every `d-*` class, zero errors, and the panel LOOKED right. The plates, preview and zip were re-cut from the decal build.",
"- **Peter, same morning: \"the bend wheel increases the notes in small steps — not very analog\".** Real: bend, wheel and aftertouch were one-poled ONCE PER BLOCK (35 % of the distance every 5–10 ms — stairs). Now a per-control-tick buffer (every 8 oversampled samples) filled once per block with an 8 ms time constant, read by every voice at its own tick. Bench gained the check that would have caught it: an octave bend step must glide (90 % within 8–80 ms) and never stair more than 250 cents per 2 ms window — the old code stepped 420 cents in its first block. **Anything the player moves by hand must be smoothed at control rate, never block rate.**",
"- Not built, deliberately: MPE; a vocoder (the choir is a fixed bank by design and a real one needs a sidechain bus); the toggle decals (unregistered pair).",
""].join("\n");
s = s.replace(anchor, add);
fs.writeFileSync(p, s.replace(/\n/g, NL));
console.log("CLAUDE.md: decals + bend recorded");
