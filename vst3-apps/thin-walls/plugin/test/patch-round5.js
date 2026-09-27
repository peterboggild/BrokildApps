// Round 5: impulses after the first sub-block; DRR across all materials; motion probe split.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

edit("test/bench.cpp", rep => {
  // a fresh path ramps its gain over its first sub-block (2.7 ms); fire after that
  rep("impulseAt (10)", "impulseAt (2400)", 10);
  rep(`            const double drop40 = db (edc[(size_t) (0.040 * fs)]) - db (edc[(size_t) (0.003 * fs)]);`,
      `            const int s0 = firstAbove (b1, 1e-4f);
            const double drop40 = db (edc[(size_t) (s0 + 0.040 * fs)]) - db (edc[(size_t) (s0 + 0.003 * fs)]);`);
  // DRR at the critical distance for every material
  rep(`        // DRR: at the critical distance the reverberant energy equals the direct
        {
            Params p = base(); p.srcType = 0;
            p.door[0] = p.door[1] = p.door[2] = 0;
            const int r = 0; const Room& R = ROOMS[r];
            float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
            // Sabine area from the material (doors shut, walls transmit almost nothing)
            const float A = R.surface() * MATERIAL_ALPHA[p.material[r]][3];`,
      `        // DRR: at the critical distance the reverberant energy equals the direct
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            Params p = base(); p.srcType = 0;
            p.material[0] = p.material[1] = p.material[2] = m;
            p.door[0] = p.door[1] = p.door[2] = 0;
            const int r = 0; const Room& R = ROOMS[r];
            float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
            // Sabine area from the material (doors shut, walls transmit almost nothing)
            const float A = R.surface() * MATERIAL_ALPHA[p.material[r]][3];`);
  rep(`            char buf[160]; std::snprintf (buf, sizeof buf, "large furnished: DRR at the critical distance %.2f m is %.1f dB (0 expected), %.1f at half, %.1f at double", rc, drr[1], drr[0], drr[2]);
            check (std::abs (drr[1]) < 2.5 && drr[0] > drr[1] + 3.5 && drr[2] < drr[1] - 3.5, buf);`,
      `            char buf[200]; std::snprintf (buf, sizeof buf, "large %s: DRR at the critical distance %.2f m is %.1f dB (0 expected), %.1f at half, %.1f at double", MATERIAL_NAMES[m], rc, drr[1], drr[0], drr[2]);
            check (std::abs (drr[1]) < 3.0 && drr[0] > drr[1] + 3.5 && drr[2] < drr[1] - 3.5, buf);`);
});

edit("test/probe.cpp", rep => {
  rep(`        Engine e; e.prepare (fs, 256);
        Params p; p.srcType = 0;
        const int n = (int) (3.0 * fs);`,
      `        Engine e; e.prepare (fs, 256);
        Params p; p.srcType = 0;
        const std::string only = argc > 2 ? argv[2] : "";
        if (only == "direct") { p.earlyDb = -120; p.reverbDb = -120; }
        if (only == "early")  { p.directDb = -120; p.reverbDb = -120; }
        if (only == "reverb") { p.directDb = -120; p.earlyDb = -120; }
        const int n = (int) (3.0 * fs);`);
});
if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("round 5 applied");
