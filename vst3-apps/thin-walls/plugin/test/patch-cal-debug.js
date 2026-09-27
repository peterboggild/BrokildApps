// debug accumulators for the late-field calibration (cheap, kept)
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 60)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}
edit("Source/Engine.h", rep => {
  rep("    const PathSlot&  slot (int i) const { return slots[(size_t) i]; }",
      "    const PathSlot&  slot (int i) const { return slots[(size_t) i]; }\n    // debug: running sums of y^2 and of the injected signal^2 per room\n    double dbgFieldEnergy[NUM_ROOMS] = { 0, 0, 0 }, dbgInjEnergy[NUM_ROOMS] = { 0, 0, 0 };");
});
edit("Source/Engine.cpp", rep => {
  rep("            F.y = y * 0.25f;\n            F.feed.write (F.y);", "            F.y = y * 0.25f;\n            F.feed.write (F.y);\n            dbgFieldEnergy[r] += (double) F.y * F.y;");
  rep("            const float srcIn = inj[r];", "            const float srcIn = inj[r];\n            dbgInjEnergy[r] += (double) srcIn * srcIn;");
});
edit("test/probe.cpp", rep => {
  rep("            std::vector<float> L, R; run (e, p, L, R, (int) (8 * fs), 10);\n            double en = 0;",
      "            std::vector<float> L, R; run (e, p, L, R, (int) (8 * fs), (int) fs);\n            double en = 0;");
  rep("            std::printf (\"   EDC:\");",
      "            std::printf (\"   FDN alone: inj energy %.4f, y energy %.4f -> G %.3f (formula %.3f)\\n\", e.dbgInjEnergy[0], e.dbgFieldEnergy[0], e.dbgFieldEnergy[0] / e.dbgInjEnergy[0], 0.0);\n            std::printf (\"   EDC:\");");
  rep("            for (int t = 0; t <= 400; t += 20) std::printf (\" %d:%.0f\", t, 10 * std::log10 (edc[(size_t) (t * fs / 1000)] / edc[0] + 1e-30));",
      "            const size_t t0 = (size_t) fs; for (int t = 0; t <= 400; t += 20) std::printf (\" %d:%.0f\", t, 10 * std::log10 (edc[t0 + (size_t) (t * fs / 1000)] / edc[t0] + 1e-30));");
});
if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("ok");
