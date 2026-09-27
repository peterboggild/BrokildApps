// one more lesson under the 260905.2 entry: the probe checked one module and
// the live panel overflowed in three.
const fs = require("fs");
const p = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const anchor = "Panel: one seg row in FILTER; the probe asserts the module still fits its column. Bench 126 / probe 48.";
if (s.split(anchor).length !== 2) { console.error("anchor miss"); process.exit(1); }
const add = anchor + String.raw` **And the panel overflowed anyway, in three OTHER modules** — the .1 knobs (2ND HEAD row, MOD>WINDOW/GRAIN, SCAN SPREAD) had spilled under the keyboard at the real deck size while the probe checked FILTER alone; Peter's screenshot showed it. A .ctl is 56 px, three knobs are 180, so every module needs 198 px: widths rebalanced to 1.13 each (FILTER 1.25 for its circuit switch), MOD RATE and MOD SYNC on one row, the fourth VOICE knob moved down a row, "SCAN FAN" / "MOD>WIN" as panel names for labels that did not fit 56 px, and the line-bench resets on the RANDOM row instead of a new one. The probe now asserts NO module and not the line bench overflows (scrollHeight vs clientHeight, scrollWidth vs clientWidth) and that every knob label fits — the checks that would have caught it. **A layout check on one box proves nothing about the others; measure every box, at the deck's own size.**`;
s = s.replace(anchor, add);
fs.writeFileSync(p, s, "utf8");
console.log("CLAUDE.md layout lesson added");
