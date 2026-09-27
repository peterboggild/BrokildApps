/*  ui.html — SOLID and SURFACE, and an opacity law with numbers behind it.

    Two things the first render measured wrong.

    (1) A specimen with air around it (the brain, the skull) and a specimen
    whose field fills the cube (every waveform volume) want opposite transfer
    functions.  A ramp shows the first whole and turns the second into a brick;
    a band shows the second as a surface and hides the first's skull, so the
    head came out as a small ball floating inside its own bone.  So the choice
    is a CONTROL, set from the histogram when a specimen loads — SOLID if more
    than a third of the cube is air, SURFACE otherwise — and overridable.

    (2) The opacity was about eight times too high.  A ray crosses the head in
    roughly 56 steps, so for a ghost that still shows the lines inside it the
    per-step alpha wants to be about 0.016, not 0.13.  DENSITY now runs
    0.15 + 6 d^2, which measures as transmittance 0.87 / 0.44 / 0.05 across the
    slider — a faint ghost, a ghost, and solid.

    node test/patch-gantry2.js
*/
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(FILE, "utf8");
const miss = [];
function rep(name, from, to, count){
  count = count === undefined ? 1 : count;
  const n = s.split(from).length - 1;
  if (n !== count){ miss.push(name + " (found " + n + ", wanted " + count + ")"); return; }
  s = s.split(from).join(to);
}

rep("uniforms",
`uniform float uAspect, uDensity, uWin, uLev, uGlowGain;`,
`uniform float uAspect, uDensity, uWin, uLev, uGlowGain, uBand;`);

rep("transfer",
`    //  a BAND, not a ramp: opaque in the middle of the window, clear at both
    //  ends, so the skull is see-through and a waveform volume is a surface
    //  rather than a brick.
    float band = a * (1.0 - a) * 4.0;
    band = band * band;
    float alpha = clamp(band * uDensity * dt * 30.0, 0.0, 1.0);`,
`    //  SOLID is a ramp — everything denser than the window's floor, which is
    //  how a scanned object should read.  SURFACE is a band, opaque only in
    //  the middle of the window, which turns a field that fills the cube into
    //  the surface where it crosses the level.
    float ramp = a * a;
    float band = a * (1.0 - a) * 4.0; band = band * band;
    float shape = mix(ramp, band, uBand);
    float alpha = clamp(shape * uDensity * dt, 0.0, 1.0);`);

rep("density law",
`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uDensity"), GX.density * 1.6);`,
`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uDensity"), 0.15 + GX.density * GX.density * 6.0);
  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uBand"), GX.band ? 1.0 : 0.0);`);

rep("cube dimmer",
`  for (const e of CUBE_E) verts.push(e[0],e[1],e[2], 0.10,0.16,0.20, e[3],e[4],e[5], 0.10,0.16,0.20);`,
`  for (const e of CUBE_E) verts.push(e[0],e[1],e[2], 0.07,0.11,0.15, e[3],e[4],e[5], 0.07,0.11,0.15);`);

rep("gx state",
`const GX = { density:0.50, win:0.42, lev:0.55 };`,
`const GX = { density:0.50, win:0.42, lev:0.55, band:false };`);

rep("auto window",
`  const p10 = at(0.10), p50 = at(0.50), p90 = at(0.90);
  const win = clamp(0.62 * (p90 - p10), 0.16, 0.92);
  GX.lev = clamp(p50, 0.05, 0.95);
  GX.win = clamp((win - 0.06) / 1.1, 0, 1);
  TOMO.dirty = true;
}`,
`  const p05 = at(0.05), p10 = at(0.10), p50 = at(0.50), p90 = at(0.90), p95 = at(0.95);
  /*  a third of the cube empty means this is an OBJECT in air, not a field. */
  const air = 1 - n / (d.length / 3);
  GX.band = air < 0.33;
  let lev, win;
  if (GX.band){ lev = p50; win = clamp(0.62 * (p90 - p10), 0.16, 0.92); }
  else        { lev = (p05 + p95) * 0.5; win = clamp((p95 - p05) * 1.06, 0.16, 1.05); }
  GX.lev = clamp(lev, 0.05, 0.95);
  GX.win = clamp((win - 0.06) / 1.1, 0, 1);
  TOMO.dirty = true;
  if (bandBtn){ bandBtn.textContent = GX.band ? "SURFACE" : "SOLID"; bandBtn.classList.toggle("on", GX.band); }
}`);

rep("band button markup",
`        <button class="tinybtn on" id="btnAnchors">ANCHORS</button>`,
`        <button class="tinybtn" id="btnBand">SOLID</button>
        <button class="tinybtn on" id="btnAnchors">ANCHORS</button>`);

rep("band button wiring",
`  $("#btnAnchors").addEventListener("click", e => { CAM.anchors = !CAM.anchors;`,
`  bandBtn = $("#btnBand");
  bandBtn.addEventListener("click", () => {
    GX.band = !GX.band;
    bandBtn.textContent = GX.band ? "SURFACE" : "SOLID";
    bandBtn.classList.toggle("on", GX.band);
    say(GX.band ? "surface: the window shows one level" : "solid: everything above the window's floor");
  });
  hint(bandBtn, "SOLID / SURFACE",
    "how the window is read.  SOLID shows everything denser than its floor, which is how a scanned object should look; SURFACE is opaque only in the middle of the window, which turns a specimen that fills the cube into the single surface where its field crosses the level.  It is chosen for you when a specimen loads.");
  $("#btnAnchors").addEventListener("click", e => { CAM.anchors = !CAM.anchors;`);

rep("band button handle",
`let tableSld = null, densSld = null, winSld = null, levSld = null;`,
`let tableSld = null, densSld = null, winSld = null, levSld = null, bandBtn = null;`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched " + FILE);
