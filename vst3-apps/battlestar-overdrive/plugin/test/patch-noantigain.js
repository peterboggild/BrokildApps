/*  Remove antiGain entirely. In mid/side the mid is preserved by construction;
 *  a broadband scalar applied to both channels scales the MONO SUM and
 *  over-boosts the lows, because it compensates a frequency-dependent loss with
 *  a flat gain. Instead the lowpass is made gentle enough not to need it. */
const fs = require("fs");
const miss = [];
function patch(path, edits) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [find, sub] of edits) {
    const f = find.join(NL);
    const n = s.split(f).length - 1;
    if (n !== 1) { miss.push(path + ": " + n + " matches: " + find[0].trim().slice(0, 45)); continue; }
    s = s.replace(f, sub.join(NL));
  }
  return { path, s };
}

const H = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.h";
const C = "C:/Users/peter/b/BattlestarOverdrive/Source/Engine.cpp";

const h = patch(H, [
  [["    void  buildAntiTrim();"], []],
  [["    float antiTrimFor (float anti) const;"], []],
  [[
    "    /*  The same idea for ANTITHRUST, which was the one block in the chain with",
    "        no level compensation at all: measured at prepare by running a reference",
    "        through the comb and the closing lowpass at each setting. The choke is",
    "        handled separately, by makeup rather than by trim, because its gain",
    "        reduction is program-dependent and a compressor is supposed to reduce",
    "        dynamic RANGE, not average level. */",
    "    static constexpr int ANTI_PTS = 9;",
    "    std::array<float, ANTI_PTS> antiTrim {};"
  ], []]
]);

const c = patch(C, [
  // drop the trim table entirely
  [["    buildAntiTrim();"], []],
  // and its application
  [[
    "    float widthAmt = 0.0f, widthDelayTarget = 0.0f, antiGain = 1.0f;"
  ], [
    "    float widthAmt = 0.0f, widthDelayTarget = 0.0f;"
  ]],
  [[
    "            antiOn  = sAnti  > 1.0e-4f;",
    "            antiGain = antiOn ? antiTrimFor (sAnti) : 1.0f;"
  ], [
    "            antiOn  = sAnti  > 1.0e-4f;"
  ]],
  [[
    "            const float sd = S + widthAmt * h;",
    "            y[0] = (M + sd) * antiGain;",
    "            y[1] = (M - sd) * antiGain;"
  ], [
    "            /*  The mid goes through UNSCALED. That is the whole guarantee:",
    "                L+R comes out as L+R went in, so nothing cancels when the",
    "                track is summed, and the centre - which is the sound - does",
    "                not move as the knob opens. An earlier version applied a",
    "                broadband trim here to hold the total energy constant, and",
    "                it dragged the mid down with it: -28.9 dB at full, far worse",
    "                than the level drop it was meant to cure.  */",
    "            const float sd = S + widthAmt * h;",
    "            y[0] = M + sd;",
    "            y[1] = M - sd;"
  ]],
  // a gentler lowpass, so there is nothing left needing compensation
  [[
    "                // The lowpass now only takes the very top off at the extreme,",
    "                // instead of closing to 2.4 kHz and fighting the comb's own",
    "                // first null over the same region.",
    "                chokeHz = lerpf (20000.0f, 7000.0f, sAnti * sAnti);"
  ], [
    "                // A gentle top-end tilt, not a real loss. Closing hard would",
    "                // need level compensation, and compensating a",
    "                // frequency-dependent loss with a flat gain boosts the lows",
    "                // and moves the mono sum - so it is easier not to lose it.",
    "                chokeHz = lerpf (20000.0f, 11000.0f, sAnti * sAnti);"
  ]]
]);

if (miss.length) { console.error("ABORTED:"); miss.forEach(m => console.error("  " + m)); process.exit(1); }
fs.writeFileSync(h.path, h.s);
fs.writeFileSync(c.path, c.s);
console.log("antiGain removed; lowpass softened");
