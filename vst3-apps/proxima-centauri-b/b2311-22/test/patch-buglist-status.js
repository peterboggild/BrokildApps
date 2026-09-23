/*  Five BWFX buglist items shipped and their headings still say "awaiting
    go" — items 9-12 in BWFX 1.6.0 (the KIERANATOR round) and 14 in 1.7.0
    (the macros). A stale list is worse than no list: the next session reads
    it and rebuilds something that already exists. */
"use strict";
const fs = require("fs");
const P = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BWFX-BUGLIST.md";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const DASH = String.fromCharCode(8212);
const miss = [];
const subs = [
  ["### 9. KIERANATOR: A/B pages and a LAST step " + DASH + " SPEC, awaiting go",
   "### 9. KIERANATOR: A/B pages and a LAST step " + DASH + " SHIPPED in BWFX 1.6.0"],
  ["### 10. KIERANATOR: a RANDOM brush " + DASH + " SPEC, awaiting go",
   "### 10. KIERANATOR: a RANDOM brush " + DASH + " SHIPPED in BWFX 1.6.0"],
  ["### 11. KIERANATOR: a CHAOS slider " + DASH + " SPEC, awaiting go",
   "### 11. KIERANATOR: a CHAOS slider " + DASH + " SHIPPED in BWFX 1.6.0"],
  ["### 12. KIERANATOR: colour the brush keys " + DASH + " SPEC, awaiting go",
   "### 12. KIERANATOR: colour the brush keys " + DASH + " SHIPPED in BWFX 1.6.0"],
  ["### 14. BWFX MACROS " + DASH + " automating the rack. SPEC, settled, awaiting go",
   "### 14. BWFX MACROS " + DASH + " automating the rack. SHIPPED in BWFX 1.7.0"]
];
for (const [a, b] of subs) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(a.slice(0, 34) + " x" + n); continue; }
  s = s.split(a).join(b);
}
if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("5 headings corrected: 9-12 shipped in 1.6.0, 14 in 1.7.0");
