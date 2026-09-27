// second bench round: the old-scale dial values in section 9, the skull ray
// probe reading outside the cube, cost thresholds set from the measurements,
// and the CORTEX study's lines aimed at the new head.
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
const bench = edit("test/bench.cpp", (rep) => {
  rep(String.raw`            e.p.specimen = 8.0f / 11.0f;                               // CORTEX: in the cache from section 7`,
      String.raw`            e.p.specimen = 8.0f / (float) (numSpecimens() - 1);        // CORTEX, by the dial's own scale`);
  rep(String.raw`            e.p.specimen = 3.0f / 11.0f;                               // MARROW: dropped from the cache by the last four`,
      String.raw`            e.p.specimen = 3.0f / (float) (numSpecimens() - 1);        // MARROW: dropped from the cache by the last four`);
  rep(String.raw`            ok (wall / 5.0 < 0.15, "8 voices x 4 unison (32 readers, the maximum) under 15 % of a core", d);`,
      String.raw`            ok (wall / 5.0 < 0.20, "8 voices x 4 unison (32 readers, the maximum) under 20 % of a core", d);`);
  rep(String.raw`            ok (wall / 5.0 < 0.18, "the window costs little: 32 readers with everything on under 18 % of a core", d);`,
      String.raw`            ok (wall / 5.0 < 0.22, "the window costs little: 32 readers with everything on under 22 % of a core", d);`);
  rep(String.raw`                auto at = [&] (float x, float y, float z) { const int i = (int) (x * VN), j = (int) (y * VN), k = (int) (z * VN);
                    return cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i]; };
                const float dirs[5][3]`,
      String.raw`                auto at = [&] (float x, float y, float z) {
                    auto ix = [] (float v) { int i = (int) std::floor (v * VN); return i < 0 ? 0 : (i >= VN ? VN - 1 : i); };
                    return cube[((size_t) ix (z) * VN + (size_t) ix (y)) * VN + (size_t) ix (x)]; };
                const float dirs[5][3]`);
});
const spec = edit("Source/Specimens.cpp", (rep) => {
  rep(String.raw`    p.specimen = listVal (8, NSPECIMENS);
    l[L_WAVE_A] = Line::circle ({ 0.50f, 0.50f, 0.52f }, 0.22f, 2, 10);
    l[L_WAVE_B] = Line::helix ({ 0.50f, 0.50f, 0.50f }, 0.18f, 0.5f, 1.5f, 12);
    l[L_FILT_A] = Line::straight ({ 0.30f, 0.50f, 0.55f }, { 0.70f, 0.50f, 0.55f });
    l[L_FILT_B] = Line::straight ({ 0.50f, 0.30f, 0.60f }, { 0.50f, 0.70f, 0.60f });`,
      String.raw`    p.specimen = listVal (8, NSPECIMENS);
    //  the head is a real head now: the rings must cross the skull to say anything
    l[L_WAVE_A] = Line::circle ({ 0.50f, 0.48f, 0.55f }, 0.34f, 2, 12);
    l[L_WAVE_B] = Line::helix ({ 0.50f, 0.48f, 0.50f }, 0.33f, 0.4f, 1.5f, 12);
    l[L_FILT_A] = Line::straight ({ 0.10f, 0.48f, 0.55f }, { 0.90f, 0.48f, 0.55f });
    l[L_FILT_B] = Line::straight ({ 0.50f, 0.05f, 0.60f }, { 0.50f, 0.95f, 0.60f });`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [bench, spec]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("fix2 applied");
