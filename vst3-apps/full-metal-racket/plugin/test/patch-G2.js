// The metal channels' pitch lives in their BAND filters, not in the oscillator
// cluster alone — so transposing the cluster while leaving the bands where they
// were moved the hat by a seventh instead of two octaves, and barely changed
// what you hear. The bands follow the kit transpose now.
//
// Deliberately NOT transposed: the tone low-passes on kicks and toms. Tuning a
// drum changes its pitch, not the damping of its head.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const wc = edit(R + "Source/Engine.cpp", [
// BELL — the bands are its pitch
[`                for (int b = 0; b < NBAND; ++b)
                {
                    static const float BF[NBAND] = { 2.0f, 4.4f, 8.4f };
                    v.band[b].setBP (clampf (f0 * BF[b] * (0.6f + tone), 200.0f, fsv * 0.45f), 2.2f, fsv);`,
`                for (int b = 0; b < NBAND; ++b)
                {
                    // derived from f0, so the kit transpose is already in them
                    static const float BF[NBAND] = { 2.0f, 4.4f, 8.4f };
                    v.band[b].setBP (clampf (f0 * BF[b] * (0.6f + tone), 200.0f, fsv * 0.45f), 2.2f, fsv);`, "bell bands"],

// METAL — the band centres are fixed frequencies, so they need it explicitly
[`            const float centre = lerpf (2600.0f, 6200.0f, tone);`,
`            /*  These are absolute frequencies rather than multiples of f0, so
                the kit transpose has to be applied by hand — without it a hat
                moved a seventh where everything else moved two octaves, and
                the ear barely noticed the difference. */
            const float kitMul = std::pow (2.0f, kitSemis / 12.0f);
            const float centre = lerpf (2600.0f, 6200.0f, tone) * kitMul;`, "metal centre"],

[`            v.hp.setHP (clampf (lerpf (900.0f, 9000.0f, snap), 60.0f, fsv * 0.45f), 0.72f, fsv);
            v.tone.setHP (clampf (250.0f, 40.0f, fsv * 0.45f), 0.7f, fsv);`,
`            v.hp.setHP (clampf (lerpf (900.0f, 9000.0f, snap) * kitMul, 60.0f, fsv * 0.45f), 0.72f, fsv);
            v.tone.setHP (clampf (250.0f * kitMul, 40.0f, fsv * 0.45f), 0.7f, fsv);`, "metal hp"]
]);

const wt = edit(R + "test/test.cpp", [
[`        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);
        return dominant (b, 48000.0, 9000, 60000, base * 0.45, base * 2.2);`,
`        const int i = paramIndex ((std::string (channelId (c)) + "_tune").c_str());
        const double base = (double) xmap (paramSpec (i).def, paramSpec (i).lo, paramSpec (i).hi);
        //  wide enough to hold a two-octave transpose either side, and for the
        //  metal channels the dominant partial is well above the fundamental
        return dominant (b, 48000.0, 4000, 40000, base * 0.35, base * 5.0);`, "scan range"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wc(); wt();
console.log("metal bands follow the kit transpose");
