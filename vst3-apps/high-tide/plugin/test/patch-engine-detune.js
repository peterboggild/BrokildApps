/*  DETUNE into the engine: the SPECS row and the per-ball clock.

    (Two Edit calls for this were refused earlier because the file had not been
    read in this session, and the build still succeeded — Params carried the
    field and nothing read it — so the bench measured a detune that was never
    applied. A node patch script is the honest way to touch a file the editor
    will not.) */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/Engine.cpp";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b, n = 1) {
  const c = s.split(a).length - 1;
  if (c !== n) { misses.push("expected " + n + " of " + JSON.stringify(a.slice(0, 70)) + ", found " + c); return; }
  s = s.split(a).join(b);
}

rep(`    { "spread",    "SPREAD",        "how differently the balls are struck, and how they repel", 0.30f, KP_PCT,   0, 0,        P(spread),    nullptr, 0 },`,
    `    { "spread",    "SPREAD",        "how differently the balls are struck, and how they repel", 0.30f, KP_PCT,   0, 0,        P(spread),    nullptr, 0 },
    { "detune",    "DETUNE",        "how far apart the balls' clocks run - fifty cents at full", 0.00f, KP_PCT,   0, 0,        P(detune),    nullptr, 0 },`);

rep(`        bl.detMul = (wmActive && wmDet != 0.0f) ? std::pow (2.0, (double) wmDet * fan / 1200.0) : 1.0;`,
`        /*  DETUNE and the SPECTRA bus are one mechanism: each ball's own clock
            runs a little fast or slow, fanned by the golden angle so ball 0
            (whose fan is exactly 0) always plays the note that was asked for.
            At zero the exponent is exactly 0 and pow(2,0) is exactly 1.0, so an
            un-detuned unison stays bit-identical to what it was. */
        const double detCents = (double) p.detune * 50.0 + (wmActive ? (double) wmDet : 0.0);
        bl.detMul = detCents == 0.0 ? 1.0 : std::pow (2.0, detCents * fan / 1200.0);`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("Engine.cpp patched: DETUNE reaches the balls");
