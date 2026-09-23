/*  ui.html — light on the tissue, and a window a radiologist would recognise.

    Three measured corrections.

    (1) The SOLID auto-window ran from the 5th to the 95th percentile of the
    tissue, which is not what a brain window is: a wide window with a squared
    ramp left everything but the densest core almost transparent, so the head
    rendered as a small bright ball inside its own skull.  A real brain window
    is NARROW and centred low — p10 to p60 here — and the head comes out whole.

    (2) The SURFACE band was squared, which made it so peaky that a waveform
    volume was a barely-there haze.  Unsquared, with the opacity raised to
    account for a shell being thin, it reads as a surface.

    (3) Neither mode had any light on it, and an unshaded volume is fog
    whatever else you do to it.  A central-difference gradient gives a normal;
    a headlight and a rim term turn the specimen into an object with form.
    The gradient is only taken where the sample is actually contributing.

    node test/patch-gantry3.js
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

rep("light uniform",
`uniform float uAspect, uDensity, uWin, uLev, uGlowGain, uBand;`,
`uniform float uAspect, uDensity, uWin, uLev, uGlowGain, uBand;
uniform vec3 uLight;`);

rep("shaded step",
`    float ramp = a * a;
    float band = a * (1.0 - a) * 4.0; band = band * band;
    float shape = mix(ramp, band, uBand);
    float alpha = clamp(shape * uDensity * dt, 0.0, 1.0);
    //  bone grey, never white — the lines are added light, and they can only
    //  be seen against tissue that has left them some room.
    vec3 tissue = mix(vec3(0.20, 0.25, 0.30), vec3(0.66, 0.70, 0.72), a);
    vec3 g = texture(uGlow, p).rgb;
    col += trans * (tissue * alpha + g * g * uGlowGain * dt * 40.0);`,
`    float ramp = a * a;
    float band = a * (1.0 - a) * 4.0;
    float shape = mix(ramp, band * 2.2, uBand);
    float alpha = clamp(shape * uDensity * dt, 0.0, 1.0);
    //  bone grey, never white — the lines are added light, and they can only
    //  be seen against tissue that has left them some room.
    vec3 tissue = mix(vec3(0.20, 0.25, 0.30), vec3(0.66, 0.70, 0.72), a);
    if (alpha > 0.0025){
      //  the gradient of the field is the surface normal; without it a volume
      //  render is fog however the opacity is tuned.
      float e = 1.0 / 64.0;
      vec3 gr = vec3(texture(uVol, p + vec3(e,0,0)).r - texture(uVol, p - vec3(e,0,0)).r,
                     texture(uVol, p + vec3(0,e,0)).r - texture(uVol, p - vec3(0,e,0)).r,
                     texture(uVol, p + vec3(0,0,e)).r - texture(uVol, p - vec3(0,0,e)).r);
      float gl = length(gr);
      if (gl > 1e-4){
        vec3 nrm = -gr / gl;
        float lam = max(0.0, dot(nrm, uLight));
        float rim = pow(1.0 - abs(dot(nrm, rd)), 3.0);
        tissue = tissue * (0.34 + 0.86 * lam) + vec3(0.10, 0.17, 0.24) * rim;
      }
    }
    vec3 g = texture(uGlow, p).rgb;
    col += trans * (tissue * alpha + g * g * uGlowGain * dt * 40.0);`);

rep("light uniform upload",
`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uBand"), GX.band ? 1.0 : 0.0);`,
`  gl.uniform1f(gl.getUniformLocation(GL.pVol, "uBand"), GX.band ? 1.0 : 0.0);
  //  a headlight, a little up and to the left of the viewer
  const lx = -b.f[0] + 0.55*b.u[0] - 0.38*b.r[0];
  const ly = -b.f[1] + 0.55*b.u[1] - 0.38*b.r[1];
  const lz = -b.f[2] + 0.55*b.u[2] - 0.38*b.r[2];
  const ll = Math.hypot(lx, ly, lz) || 1;
  gl.uniform3f(gl.getUniformLocation(GL.pVol, "uLight"), lx/ll, ly/ll, lz/ll);`);

rep("solid window",
`  else        { lev = (p05 + p95) * 0.5; win = clamp((p95 - p05) * 1.06, 0.16, 1.05); }`,
`  else {
    /*  a brain window: narrow and low, so the whole head reads and only the
        bone clips to solid — not the p05..p95 sprawl, which showed nothing
        but the densest core. */
    const lo = at(0.10), hi = at(0.60);
    lev = (lo + hi) * 0.5; win = clamp((hi - lo) * 1.15, 0.16, 1.05);
  }`);
rep("unused percentiles",
`  const p05 = at(0.05), p10 = at(0.10), p50 = at(0.50), p90 = at(0.90), p95 = at(0.95);`,
`  const p10 = at(0.10), p50 = at(0.50), p90 = at(0.90);`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched " + FILE);
