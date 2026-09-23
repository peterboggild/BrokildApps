// after the second look at the plates: the orbits open on the face, the
// maxilla is an alveolar arch with a palate and cheekbones instead of a brick,
// and a Hounsfield volume gets a bone threshold in the gantry so soft tissue
// does not fog a bone window.
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
const an = edit("Source/Anatomy.cpp", (rep) => {
  rep(String.raw`        const float F = std::pow (std::abs (X) / 6.4f, 3.0f) + std::pow (std::abs (Y - 4.0f) / 4.6f, 3.0f) + std::pow (std::abs (Z + 5.2f) / 4.4f, 3.0f);`,
      String.raw`        const float F = std::pow (std::abs (X) / 6.4f, 3.0f) + std::pow (std::abs (Y - 4.0f) / 4.6f, 3.0f) + std::pow (std::abs (Z + 4.6f) / 5.0f, 3.0f);`);
  rep(String.raw`        //  the maxilla: a bone block behind the face, between the orbits and the upper teeth
        const float M = std::pow (std::abs (X) / 4.8f, 3.0f) + std::pow (std::abs (Y - 4.2f) / 3.3f, 3.0f) + std::pow (std::abs (Z + 4.0f) / 1.7f, 3.0f);
        if (M < 1.0f) { bone = true; hu = M > 0.62f ? CORT - 100.0f : trab (X, Y, Z) - 40.0f; }`,
      String.raw`        //  the maxilla: the upper alveolar arch, the hard palate across it, and the cheekbones
        {
            const float ux = 4.5f, uy = 6.1f, ucy = 1.7f;
            const float ur = std::sqrt (X * X / (ux * ux) + (Y - ucy) * (Y - ucy) / (uy * uy));
            const float uth = std::atan2 (X / ux, (Y - ucy) / uy);
            if (Z > -5.9f && Z < -2.4f && std::abs (uth) < 1.6f && ur > 0.70f && ur < 1.0f)
            { bone = true; hu = (ur > 0.92f || ur < 0.78f) ? CORT - 150.0f : trab (X, Y, Z) - 40.0f; }
            if (Z > -5.95f && Z < -5.5f && ur < 0.75f && std::abs (uth) < 1.6f) { bone = true; hu = 700.0f; }
            for (int s = -1; s <= 1; s += 2)
                if (ell (X - 5.3f * (float) s, Y - 4.4f, Z + 2.3f, 1.35f, 1.7f, 1.6f) < 1.0f) { bone = true; hu = CORT - 200.0f; }
        }`);
  rep(String.raw`        if (std::abs (X) < 1.15f && Y > 2.6f && Y < 9.4f && Z > -6.6f && Z < -0.9f)`,
      String.raw`        if (std::abs (X) < 1.15f && Y > 2.6f && Y < 9.4f && Z > -5.4f && Z < -0.9f)`);
  rep(String.raw`        const float O = ell (std::abs (X) - 3.15f, Y - 5.6f, Z + 0.9f, 1.95f, 2.5f, 1.9f);
        if (O >= 1.0f && O < 1.28f && rv < 1.02f && Z > -3.0f && Y > 3.6f) { bone = true; hu = CORT - 250.0f; }
        if (O < 1.0f) { hu = FAT + 15.0f; bone = false; }
        if (ell (std::abs (X) - 3.15f, Y - 6.7f, Z + 0.9f, 1.15f, 1.15f, 1.15f) < 1.0f) hu = 32.0f;
        if (O < 1.0f && ell (std::abs (X) - 3.15f, Y - 6.7f, Z + 0.9f, 1.2f, 1.2f, 1.2f) < 1.0f && Y > 7.6f) hu = 60.0f;   // lens`,
      String.raw`        //  the orbits open on the face: cones that reach past the frontal bone
        const float O = ell (std::abs (X) - 3.15f, Y - 7.0f, Z + 0.9f, 1.95f, 2.8f, 1.9f);
        if (O >= 1.0f && O < 1.30f && rv < 1.03f && Z > -3.0f && Y > 4.0f) { bone = true; hu = CORT - 250.0f; }
        if (O < 1.0f) { hu = Y > 9.2f ? AIR : FAT + 15.0f; bone = false; }
        if (ell (std::abs (X) - 3.15f, Y - 7.8f, Z + 0.9f, 1.15f, 1.15f, 1.15f) < 1.0f) hu = 32.0f;
        if (O < 1.0f && ell (std::abs (X) - 3.15f, Y - 7.8f, Z + 0.9f, 1.2f, 1.2f, 1.2f) < 1.0f && Y > 8.7f) hu = 60.0f;   // lens`);
});
const ui = edit("Source/ui/ui.html", (rep) => {
  rep(String.raw`uniform float uAspect, uDensity, uWin, uLev, uGlowGain, uBand;`,
      String.raw`uniform float uAspect, uDensity, uWin, uLev, uGlowGain, uBand, uFloor;`);
  rep(String.raw`    float ramp = a * (0.30 + 0.70 * a);`,
      String.raw`    //  a Hounsfield volume gets a threshold as well: below a third of the
    //  window nothing is drawn, so a bone window shows bone and not a fog of
    //  soft tissue.  A phantom (uFloor 0) keeps the plain ramp.
    float gate = uFloor < 0.01 ? 1.0 : smoothstep(uFloor, uFloor + 0.25, a);
    float ramp = a * (0.30 + 0.70 * a) * gate;`);
  rep(String.raw`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uBand"), GX.band ? 1.0 : 0.0);`,
      String.raw`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uBand"), GX.band ? 1.0 : 0.0);
  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uFloor"), VOL.hu ? 0.30 : 0.0);`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [an, ui]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("fix4 applied");
