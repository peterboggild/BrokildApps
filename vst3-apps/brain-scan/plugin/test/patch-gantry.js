/*  ui.html — the gantry's transfer function, done as a radiologist would.

    What the first live shot showed: with a plain "everything above the floor"
    ramp the head renders as a solid white ball and the scan lines — the whole
    point of the picture — are buried inside it, and a WAVEFORM specimen (whose
    field fills the cube from 0.1 to 0.9) is an opaque brick at every setting.

    The fix is the real thing: WINDOW is a width and LEVEL is a centre, and the
    opacity is a BAND across that window, not a ramp.  A band shows the
    iso-surface where the field is near the level — for CORTEX that is the
    tissue seen through a transparent skull, for SPINE the surface the waveform
    makes — and it leaves the rest of the cube clear for the lines to glow in.
    The slice keeps the ramp, because a slice is a greyscale map and that is
    what a window/level pair means there.

    node test/patch-gantry.js
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

/* ---- 1. the shader: a band, and tissue that never goes white ----------- */
rep("shader transfer",
`    float d = texture(uVol, p).r;
    float a = clamp((d - uLev + uWin * 0.5) / max(uWin, 0.01), 0.0, 1.0);
    a = a * a;
    float alpha = clamp(a * uDensity * dt * 22.0, 0.0, 1.0);
    vec3 tissue = mix(vec3(0.36, 0.44, 0.52), vec3(1.0, 0.985, 0.94), a);
    vec3 g = texture(uGlow, p).rgb;
    col += trans * (tissue * alpha + g * g * uGlowGain * dt * 34.0);`,
`    float d = texture(uVol, p).r;
    float a = clamp((d - uLev + uWin * 0.5) / max(uWin, 0.01), 0.0, 1.0);
    //  a BAND, not a ramp: opaque in the middle of the window, clear at both
    //  ends, so the skull is see-through and a waveform volume is a surface
    //  rather than a brick.
    float band = a * (1.0 - a) * 4.0;
    band = band * band;
    float alpha = clamp(band * uDensity * dt * 30.0, 0.0, 1.0);
    //  bone grey, never white — the lines are added light, and they can only
    //  be seen against tissue that has left them some room.
    vec3 tissue = mix(vec3(0.20, 0.25, 0.30), vec3(0.66, 0.70, 0.72), a);
    vec3 g = texture(uGlow, p).rgb;
    col += trans * (tissue * alpha + g * g * uGlowGain * dt * 40.0);`);

/* ---- 2. round the read cursors ---------------------------------------- */
rep("point shader",
`const FS_LINE = \`#version 300 es
precision mediump float; in vec3 vCol; out vec4 frag;
void main(){ frag = vec4(vCol, 1.0); }\`;`,
`const FS_LINE = \`#version 300 es
precision mediump float; in vec3 vCol; out vec4 frag;
uniform float uPoint;
void main(){
  if (uPoint > 0.5){ vec2 d = gl_PointCoord - 0.5; float r = dot(d, d);
    if (r > 0.25) discard; frag = vec4(vCol * (1.0 - r * 2.6), 1.0); return; }
  frag = vec4(vCol, 1.0);
}\`;`);
rep("point flag off",
`  gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
  gl.drawArrays(gl.LINES, 0, verts.length/6);`,
`  gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
  gl.uniform1f(gl.getUniformLocation(GL.pLine, "uPoint"), 0.0);
  gl.drawArrays(gl.LINES, 0, verts.length/6);`);
rep("point flag on",
`  gl.vertexAttribPointer(lc, 3, gl.FLOAT, false, 24, 12);
  gl.drawArrays(gl.POINTS, 0, pts.length/6);`,
`  gl.vertexAttribPointer(lc, 3, gl.FLOAT, false, 24, 12);
  gl.uniform1f(gl.getUniformLocation(GL.pLine, "uPoint"), 1.0);
  gl.drawArrays(gl.POINTS, 0, pts.length/6);`);

/* ---- 3. defaults, and the auto window --------------------------------- */
rep("gx defaults",
`const GX = { density:0.55, win:0.62, lev:0.42 };`,
`const GX = { density:0.50, win:0.42, lev:0.55 };
/*  A scanner picks its window from the study, and so does this: on every new
    specimen the window is centred on the median of the tissue and made about
    as wide as its spread.  Move the sliders and they stay where you put them
    until the next specimen — which is what the hint says. */
function autoWindow(){
  const d = VOL.data, h = new Uint32Array(64);
  let n = 0;
  for (let i = 0; i < d.length; i += 3){ const v = d[i]; if (v > 8){ h[v >> 2]++; n++; } }
  if (n < 200) return;
  const at = f => { let c = 0; const want = n * f;
    for (let b = 0; b < 64; b++){ c += h[b]; if (c >= want) return (b * 4 + 2) / 255; } return 1; };
  const p10 = at(0.10), p50 = at(0.50), p90 = at(0.90);
  const win = clamp(0.62 * (p90 - p10), 0.16, 0.92);
  GX.lev = clamp(p50, 0.05, 0.95);
  GX.win = clamp((win - 0.06) / 1.1, 0, 1);
  TOMO.dirty = true;
}`);
rep("auto window on arrival",
`  VOL.spec = p.spec; VOL.name = p.name || "";
  VOL.dirty = true; TOMO.dirty = true;`,
`  VOL.spec = p.spec; VOL.name = p.name || "";
  VOL.dirty = true; TOMO.dirty = true;
  autoWindow();
  winSld && winSld.redraw(); levSld && levSld.redraw(); densSld && densSld.redraw();`);

/* ---- 4. the third slider ---------------------------------------------- */
rep("gantry slider row",
`      <div style="display:flex;gap:10px"><div class="sld" id="densitySld"></div><div class="sld" id="windowSld"></div></div>`,
`      <div style="display:flex;gap:9px"><div class="sld" id="densitySld"></div><div class="sld" id="windowSld"></div><div class="sld" id="levelSld"></div></div>`);
rep("build the sliders",
`  mkSlider($("#densitySld"), { name:"DENSITY", def:0.55,
    get:() => GX.density, set:v => { GX.density = v; },
    fmt: v => Math.round(v*100) + " %",
    hint:"the transfer function's opacity: low and the lines float in a ghost, high and the tissue closes over them." });
  mkSlider($("#windowSld"), { name:"WINDOW", def:0.62,
    get:() => GX.win, set:v => { GX.win = v; TOMO.dirty = true; },
    fmt: v => Math.round(v*100) + " %",
    hint:"the CT window: how much of the density range is spread across the greys, in the gantry and on the slice alike." });`,
`  densSld = mkSlider($("#densitySld"), { name:"DENSITY", def:0.50,
    get:() => GX.density, set:v => { GX.density = v; },
    fmt: v => Math.round(v*100) + " %",
    hint:"the transfer function's opacity: low and the lines float in a ghost, high and the tissue closes over them." });
  winSld = mkSlider($("#windowSld"), { name:"WINDOW", def:0.42,
    get:() => GX.win, set:v => { GX.win = v; TOMO.dirty = true; },
    fmt: v => Math.round(0.06 + v*1.1, 2) === 0 ? "0" : (0.06 + v*1.1).toFixed(2),
    hint:"window WIDTH — how much of the density range is shown at all.  Narrow it and the gantry shows a single surface of the specimen; the slice hardens at the same time.  It is set for each specimen as it loads and then stays where you put it." });
  levSld = mkSlider($("#levelSld"), { name:"LEVEL", def:0.55,
    get:() => GX.lev, set:v => { GX.lev = v; TOMO.dirty = true; },
    fmt: v => v.toFixed(2),
    hint:"window LEVEL — which density the window is centred on.  In the gantry it chooses WHICH surface of the specimen you are looking at; sweep it and you travel out through the tissue." });`);
rep("slider handles",
`let tableSld = null;`,
`let tableSld = null, densSld = null, winSld = null, levSld = null;`);

/* ---- 5. the keyboard's white keys were invisible ----------------------- */
rep("white key border",
`.wk{position:absolute;top:0;bottom:0;background:linear-gradient(180deg,#fbfaf6,#e6e2d8 80%,#cfcabd);
  border-right:1px solid #b9b4a7;cursor:pointer}`,
`.wk{position:absolute;top:0;bottom:0;background:linear-gradient(180deg,#fbfaf6,#e9e5db 78%,#d6d1c4);
  border-right:1px solid #9a948a;box-shadow:inset -2px 0 3px -2px rgba(0,0,0,.35);cursor:pointer}`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched " + FILE);
