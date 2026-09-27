/*  Level-match the starters against the bench's measured rms. A starter that
    is twice as loud as the next one is not safe ground either. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/Factory.cpp";
let s = fs.readFileSync(f, "utf8");
const L = {
  sSoftKeys: 1.00, sSub: 0.48, sPluckBass: 0.93, sWideBass: 0.82, sSquareBass: 0.61,
  sSawLead: 1.00, sSquareLead: 0.79, sSweepLead: 0.60, sGlideLead: 0.59,
  sOrgan: 1.00, sElectricPiano: 0.78, sBell: 0.96, sSlowPad: 0.76, sMorphPad: 0.70,
  sGlass: 0.80, sDrone: 0.74
};
const miss = [];
for (const k in L) {
  const re = new RegExp("(void " + k + " \\(Terrain& t, Lanes& l, Params& p\\)[\\s\\S]*?set \\(p, \"level\", )([0-9.]+)(f\\);)");
  if (!re.test(s)) { miss.push(k); continue; }
  s = s.replace(re, "$1" + L[k].toFixed(2) + "$3");
}
if (miss.length) { console.log("MISS level for: " + miss.join(", ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("starters level-matched");
