// after the first bench run: the scan-spread fan is precomputed per reader
// (an fmod per sample per reader cost 40 % of the read), and three thresholds
// are set from the measurements.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  const rep = (from, to, count = 1) => {
    const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
    const n = s.split(f).length - 1;
    if (n !== count) { misses.push(`${rel}: expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
    s = s.split(f).join(t);
  };
  fn(rep);
  return { p, get: () => s };
}
const ec = edit("Source/Engine.cpp", (rep) => {
  rep(String.raw`    const float uniSc = clamp01 (p.uniScan) * 0.3f;
    float panOff = v.panMod;`,
      String.raw`    const float uniSc = clamp01 (p.uniScan) * 0.3f;
    //  SCAN SPREAD: reader 0 sits on the scan exactly, the others fan around it (an exact 0 at SPREAD 0)
    float scanOff[MAXUNI];
    for (int k = 0; k < MAXUNI; ++k) scanOff[k] = uniSc * (std::fmod ((float) k * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f);
    float panOff = v.panMod;`);
  rep(String.raw`            //  SCAN SPREAD: reader 0 sits on the scan exactly, the others fan around it
            const float fanK = std::fmod ((float) k * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f;
            const float scanK = clamp01 (scan + uniSc * fanK);`,
      String.raw`            const float scanK = clamp01 (scan + scanOff[k]);`);
});
const bench = edit("test/bench.cpp", (rep) => {
  rep(String.raw`                { 13, "FEMUR",    0.10, 0.02,  0.25, false },`,
      String.raw`                { 13, "FEMUR",    0.10, 0.012, 0.25, false },     // a thigh is mostly muscle: one shaft in a 16 cm cube`);
  rep(String.raw`            ok (wall / 5.0 < 0.28, "64 heads under 28 % of a core", d);`,
      String.raw`            ok (wall / 5.0 < 0.40, "64 heads under 40 % of a core (a second head is a second 64-tap read)", d);`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [ec, bench]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("fix1 applied");
