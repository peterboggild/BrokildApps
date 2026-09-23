/*  "Photo Synth" on the website too.

    Two traps handled deliberately:

    1. THE SPLIT HEADING. The landing page writes the name as
       `<h1>Photo-Synth <span>2</span></h1>`, so a plain text replacement walks
       straight past it — this bit twice during the Mars Wars rename. It is
       replaced as markup, and verified afterwards by RENDERING the page and
       reading the heading back, never by grepping for the old string (grep
       says clean when the string genuinely is not there in one piece).

    2. FILE NAMES vs PROSE. `Photo-Synth2-Manual.pdf` must become
       `Photo-Synth-Manual.pdf` (the house convention is the hyphenated
       product name), while prose becomes "Photo Synth". Doing both with one
       ordered list corrupts the first with the second, so the file names are
       parked behind placeholders while the prose is swept.

    Engineering records (BWFX design/handover/buglists) are deliberately left
    alone: they document what things were called when the decisions were made,
    and rewriting them falsifies the record.
*/
"use strict";
const fs = require("fs");
const path = require("path");
const NLo = String.fromCharCode(10);
const ROOT = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps";
const miss = [];

/* ── 1. the split heading, as markup ─────────────────────────────────── */
{
  const P = ROOT + "/vst3-apps/photo-synth-2/index.html";
  let s = fs.readFileSync(P, "utf8");
  const A = "<h1>Photo-Synth <span>2</span></h1>";
  const B = "<h1>Photo <span>Synth</span></h1>";
  const n = s.split(A).length - 1;
  if (n !== 1) miss.push("split heading x" + n);
  else { fs.writeFileSync(P, s.split(A).join(B)); }
}
if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }

/* ── 2. the sweep ────────────────────────────────────────────────────── */
const FILES = [                       // user-facing only
  "manifest.json", "README.md",
  "music-apps/photo-synth/app.json", "music-apps/photo-synth/index.html",
  "vst3-apps/photo-synth-2/app.json", "vst3-apps/photo-synth-2/index.html",
  "vst3-apps/collection/app.json", "vst3-apps/collection/index.html",
  "vst3-apps/black-rider/index.html", "vst3-apps/blade-ruiner/index.html",
  "vst3-apps/clone-wars/index.html", "vst3-apps/escape-room/index.html",
  "vst3-apps/full-metal-racket/index.html", "vst3-apps/hairfryer/index.html",
  "vst3-apps/martian-gain/index.html"
];

const PARK = [                        // file names, parked while prose is swept
  ["Photo-Synth2-Manual.pdf",      "\u0001MANUAL\u0001"],
  ["Photo-Synth2-VST3-win64.zip",  "\u0001ZIP\u0001"],
  ["HTML-to-VST3-Devkit-Manual.pdf", "\u0001DEVMAN\u0001"],
  ["HTML-to-VST3-Devkit.zip",        "\u0001DEVZIP\u0001"]
];
const UNPARK = [
  ["\u0001MANUAL\u0001", "Photo-Synth-Manual.pdf"],
  ["\u0001ZIP\u0001",    "Photo-Synth-VST3-win64.zip"],
  ["\u0001DEVMAN\u0001", "HTML-to-VST3-Devkit-Manual.pdf"],
  ["\u0001DEVZIP\u0001", "HTML-to-VST3-Devkit.zip"]
];
const PROSE = [
  ["Photo-Synth 2 (VST3)", "Photo Synth (VST3)"],
  ["Photo Synth 2 (HTML)", "Photo Synth (HTML)"],
  ["Photo-Synth 2", "Photo Synth"],
  ["Photo Synth 2",  "Photo Synth"],
  ["Photo-Synth2",   "Photo Synth"],
  ["Photo-Synth",    "Photo Synth"]
];

let touched = 0, absent = [];
for (const rel of FILES) {
  const p = path.join(ROOT, rel);
  if (!fs.existsSync(p)) { absent.push(rel); continue; }
  let s = fs.readFileSync(p, "utf8");
  const before = s;
  for (const [a, b] of PARK)   s = s.split(a).join(b);
  for (const [a, b] of PROSE)  s = s.split(a).join(b);
  for (const [a, b] of UNPARK) s = s.split(a).join(b);
  if (s !== before) { fs.writeFileSync(p, s); touched++; }
}
console.log("swept " + touched + " files"
  + (absent.length ? "  (missing: " + absent.join(", ") + ")" : ""));
