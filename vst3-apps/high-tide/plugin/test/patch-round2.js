/*  The second pass on the starters, all four items measured:

    1. The balls' mutual push is what makes a unison patch drift with the
       strike (DRONE spread 0.45 drifted 6.8 cents, GLASS spread 0.20 only
       0.76). It was there to stop the balls sitting on top of each other and
       DETUNE now does that job properly, so it is halved.
    2. CLAV was inaudible: the force tap alone is a quiet way to hear a bowl.
    3. ORGAN at full level clipped the ceiling on a two-note chord.
    4. The pads' spread comes down with the push.
*/
"use strict";
const fs = require("fs");
const misses = [];
function patch(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  for (const [a, b] of pairs) {
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split("/").pop() + ": expected 1 of " + JSON.stringify(a.slice(0, 64)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const E = "C:/Users/peter/b/HighTide/Source/Engine.cpp";
const F = "C:/Users/peter/b/HighTide/Source/Factory.cpp";

const outE = patch(E, [[
`    const double repK = nb > 1 ? (double) p.spread * 0.02 : 0.0;`,
`    /*  Halved once DETUNE existed: a push between the balls shifts each
        one's period a little, and the shift depends on their energy — which
        made a wide-unison patch drift with the strike (6.8 cents at SPREAD
        0.45, measured). Separating the balls is DETUNE's job now; this only
        stops them sitting exactly on top of one another. */
    const double repK = nb > 1 ? (double) p.spread * 0.01 : 0.0;`]]);

const outF = patch(F, [
  [`        set (p, "tapPos", 0.3f); set (p, "tapVel", 0.35f); set (p, "tapFrc", 0.55f);
        set (p, "tone", 0.8f); set (p, "strike", 0.8f); set (p, "velSens", 0.85f);
        set (p, "friction", 0.34f); set (p, "release", 0.8f);`,
   `        set (p, "tapPos", 0.6f); set (p, "tapVel", 0.5f); set (p, "tapFrc", 0.85f);
        set (p, "tone", 0.8f); set (p, "strike", 0.9f); set (p, "velSens", 0.85f);
        set (p, "friction", 0.26f); set (p, "release", 0.8f);`],
  [`        set (p, "ampA", 0.0f); set (p, "ampD", 0.6f); set (p, "ampS", 0.35f); set (p, "ampR", 0.22f);
        set (p, "level", 0.85f);`,
   `        set (p, "ampA", 0.0f); set (p, "ampD", 0.6f); set (p, "ampS", 0.35f); set (p, "ampR", 0.22f);
        set (p, "level", 1.00f);`],
  //  ORGAN: frictionless and sustaining, so it stacks fast on a chord
  [`        set (p, "friction", 0.0f);              // frictionless: it holds while the key is down
        set (p, "release", 0.75f);
        set (p, "ampA", 0.03f); set (p, "ampD", 0.3f); set (p, "ampS", 1.0f); set (p, "ampR", 0.10f);
        set (p, "level", 1.00f);`,
   `        set (p, "friction", 0.0f);              // frictionless: it holds while the key is down
        set (p, "release", 0.75f);
        set (p, "ampA", 0.03f); set (p, "ampD", 0.3f); set (p, "ampS", 1.0f); set (p, "ampR", 0.10f);
        set (p, "level", 0.80f);`],
  //  the pads: less push, so less drift
  [`        set (p, "unison", 1.0f); set (p, "detune", 0.20f); set (p, "spread", 0.4f); set (p, "width", 0.85f);`,
   `        set (p, "unison", 1.0f); set (p, "detune", 0.22f); set (p, "spread", 0.28f); set (p, "width", 0.85f);`],
  [`        set (p, "unison", 1.0f); set (p, "detune", 0.28f); set (p, "spread", 0.45f); set (p, "width", 1.0f);`,
   `        set (p, "unison", 1.0f); set (p, "detune", 0.30f); set (p, "spread", 0.28f); set (p, "width", 1.0f);`],
]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(outE[0], outE[1], "utf8");
fs.writeFileSync(outF[0], outF[1], "utf8");
console.log("round 2 applied: repulsion halved, CLAV audible, ORGAN levelled, pads calmed");
