// The cheek is wood GRAIN. Forcing it to the panel's 120:3320 aspect stretched
// a 710-tall crop into a 5312-tall strip — a 649% smear that makes the grain
// seven times its own size and stops reading as wood at all. It is tiled
// vertically instead, which keeps the grain the size it was photographed.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const w1 = edit(R + "tools/ingest-decals.ps1", [
  ['  "fmr-cheek"        = 120.0 / 3320.0\n',
   '  # fmr-cheek is deliberately absent: it is wood GRAIN, and correcting it to\n' +
   '  # the panel\'s very tall aspect smears the grain to seven times its scale.\n' +
   '  # It keeps its own proportions and the panel tiles it vertically instead.\n',
   "cheek aspect"]
]);

const w2 = edit(R + "Source/ui/ui.html", [
  [`add('.cheek{background:url("' + u + '") center/100% 100% no-repeat!important}');`,
   `add('.cheek{background:url("' + u + '") top center/100% auto repeat-y!important}');`,
   "cheek css"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
w1(); w2();
console.log("cheek keeps its grain and tiles vertically");
