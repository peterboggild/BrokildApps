// after looking at the plates: the gantry's opacity was linear per step, so a
// 0.7 cm bone shell (two steps of 96) came out a ghost — an exponential
// extinction with a wide DENSITY range fixes thin bone without changing the
// phantoms at the default. Plus three anatomy corrections seen on the slices.
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
const ui = edit("Source/ui/ui.html", (rep) => {
  rep(String.raw`    float alpha = clamp(shape * uDensity * dt, 0.0, 1.0);`,
      String.raw`    //  extinction, not a linear step: a thin bone shell (two steps of 96)
    //  must be able to go opaque, which a per-step clamp never let it
    float alpha = 1.0 - exp(-shape * uDensity * dt);`);
  rep(String.raw`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uDensity"), 0.15 + GX.density * GX.density * 6.0);`,
      String.raw`  /*  DENSITY is exponential: 1.65 per unit length at the middle (the look the
      phantoms shipped with), 0.045 at the bottom, 60 at the top — where a
      0.7 cm skull table reads solid */
  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uDensity"), 1.65 * Math.exp(7.25 * (GX.density - 0.5)));`);
  rep(String.raw`  GX.band = false;
  if (bandBtn){ bandBtn.textContent = "SOLID"; bandBtn.classList.remove("on"); }
  GX.preset = name;`,
      String.raw`  GX.band = false;
  if (bandBtn){ bandBtn.textContent = "SOLID"; bandBtn.classList.remove("on"); }
  //  a body is thin shells and cavities: the gantry needs the density up to show them
  GX.density = (name === "BONE" || name === "LUNG") ? 0.86 : 0.74;
  densSld && densSld.redraw();
  GX.preset = name;`);
  rep(String.raw`    hint:"the transfer function's opacity: low and the lines float in a ghost, high and the tissue closes over them." });`,
      String.raw`    hint:"the transfer function's opacity: low and the lines float in a ghost, high and the tissue closes over them.  A body's window presets raise it, because bone is a thin shell and needs it." });`);
});
const an = edit("Source/Anatomy.cpp", (rep) => {
  rep(String.raw`        //  the nose
        if (ell (X, Y - 9.0f, Z + 3.6f, 1.25f, 1.6f, 2.1f) < 1.0f) hu = SOFT;
        //  the maxilla: a bone block behind the face, below the orbits
        const float M = std::pow (std::abs (X) / 4.8f, 3.0f) + std::pow (std::abs (Y - 4.2f) / 3.3f, 3.0f) + std::pow (std::abs (Z + 4.6f) / 2.2f, 3.0f);`,
      String.raw`        //  the nose, joined to the face
        if (ell (X, Y - 8.3f, Z + 3.5f, 1.2f, 1.5f, 1.9f) < 1.0f) hu = SOFT;
        //  the maxilla: a bone block behind the face, between the orbits and the upper teeth
        const float M = std::pow (std::abs (X) / 4.8f, 3.0f) + std::pow (std::abs (Y - 4.2f) / 3.3f, 3.0f) + std::pow (std::abs (Z + 4.0f) / 1.7f, 3.0f);`);
  rep(String.raw`        //  the orbits: fat, and a globe in each
        const float O = ell (std::abs (X) - 3.15f, Y - 5.6f, Z + 0.9f, 1.95f, 2.5f, 1.9f);
        if (O < 1.0f) { hu = FAT + 15.0f; bone = false; }`,
      String.raw`        //  the orbits: a bony rim, fat inside, and a globe in each
        const float O = ell (std::abs (X) - 3.15f, Y - 5.6f, Z + 0.9f, 1.95f, 2.5f, 1.9f);
        if (O >= 1.0f && O < 1.28f && rv < 1.02f && Z > -3.0f && Y > 3.6f) { bone = true; hu = CORT - 250.0f; }
        if (O < 1.0f) { hu = FAT + 15.0f; bone = false; }`);
});
if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
for (const f of [ui, an]) fs.writeFileSync(f.p, f.get(), "utf8");
console.log("fix3 applied");
