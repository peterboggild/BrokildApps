/*  1. THE WHEEL DRAGS THE BODY THROUGH THE PLANE (Peter's idea, and it is
       exactly the design: the wheel cranks the WINCH target, and the section
       pull does the rest — the object glides through your slice with its own
       inertia, organs condensing in and evaporating out as it passes).

    2. The stir detector watches HEADING ROTATION, not angle about the
       centroid — a straight drag past a centroid sweeps angle too (that is
       just parallax), which is why a to-and-fro drag read as stir and
       cancelled itself to nothing. Only a genuine loop turns its heading
       continuously.

    3. KNEAD becomes PULLING: the quantity follows the stretch — the distance
       from where you gripped — in any direction. Monotone, immediate,
       tactile: drag away to wind up, return to wind down, release to keep.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [

//  gesture state gains heading-rotation tracking
[`  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, node: tn };`,
`  gest = { organ: o, id, t0: performance.now(), x0: e.clientX, y0: e.clientY,
           lx: e.clientX, ly: e.clientY, angle: 0, path: 0, mode: "?",
           heading: null, rot: 0,
           v0: VAL[id] !== undefined ? VAL[id] : 0.5, dv: 0, node: tn };`, "state"],

//  heading rotation accumulates alongside the centroid angle
[`  const a1 = Math.atan2(g.ly - g.organ.cy, g.lx - g.organ.cx);
  const a2 = Math.atan2(e.clientY - g.organ.cy, e.clientX - g.organ.cx);
  let da = a2 - a1;
  if (da > Math.PI) da -= 2 * Math.PI;
  if (da < -Math.PI) da += 2 * Math.PI;
  g.angle += da;
  g.lx = e.clientX; g.ly = e.clientY;`,
`  const a1 = Math.atan2(g.ly - g.organ.cy, g.lx - g.organ.cx);
  const a2 = Math.atan2(e.clientY - g.organ.cy, e.clientX - g.organ.cx);
  let da = a2 - a1;
  if (da > Math.PI) da -= 2 * Math.PI;
  if (da < -Math.PI) da += 2 * Math.PI;
  g.angle += da;
  /*  heading rotation: the only honest stir detector. A straight drag past
      a centroid sweeps centroid-angle too (parallax); only a genuine loop
      turns its own heading, continuously and in one sense. */
  if (step > 2.5) {
    const h = Math.atan2(dy, dx);
    if (g.heading !== null) {
      let dh = h - g.heading;
      if (dh > Math.PI) dh -= 2 * Math.PI;
      if (dh < -Math.PI) dh += 2 * Math.PI;
      g.rot += dh;
    }
    g.heading = h;
  }
  g.lx = e.clientX; g.ly = e.clientY;`, "heading"],

//  the classifier: stir by rotation, stroke by straightness, knead otherwise
[`    if (Math.abs(g.angle) > Math.PI * 0.45) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
    /*  and the gesture everyone actually makes: an ordinary deliberate
        drag. It KNEADS — the quantity follows the working of the tissue,
        signed by the drag's rotation sense about the organ. Without this
        the commonest gesture of all mapped to NOTHING, which read —
        correctly — as "the specimen does not respond". */
    else if (g.path > 26 && (now - g.t0) >= 260) g.mode = "knead";
  }`,
`    if (Math.abs(g.rot) > Math.PI * 1.3) g.mode = "stir";
    else if (g.path > 90 && (now - g.t0) < 260 && net / g.path > 0.93) g.mode = "stroke";
    /*  and the gesture everyone actually makes: an ordinary deliberate
        drag. It KNEADS: the quantity follows the STRETCH — the distance
        from where you gripped — in any direction. Drag away to wind up,
        come back to wind down, let go to keep. Without this the commonest
        gesture of all mapped to NOTHING, which read — correctly — as "the
        specimen does not respond". */
    else if (g.path > 22 && (now - g.t0) >= 180) g.mode = "knead";
  }
  if (g.mode === "knead" && Math.abs(g.rot) > Math.PI * 1.3) g.mode = "stir";`, "classify"],

//  knead follows stretch, not rotation sign
[`  if (g.mode === "knead") {
    const dir = (da >= 0 ? 1 : -1);
    sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + dir * step * 0.0016);
  }`,
`  if (g.mode === "knead") {
    const stretch = Math.hypot(e.clientX - g.x0, e.clientY - g.y0);
    sendParam(g.id, clamp(g.v0 + (stretch - 16) * 0.0024, 0, 1));
  }`, "knead act"],

//  THE WHEEL: crank the winch — the body glides through the plane
[`window.addEventListener("keydown", e => {
  if (e.key === "Escape" && fieldOpen) {`,
`/*  Peter's idea, and it was already the mechanism: the wheel cranks the
    WINCH, and the section pull carries the 4D body through your plane with
    its own inertia — organs condense in and evaporate out as it passes.
    Shift for a fine crank. Automation-visible, because the winch is. */
cv.addEventListener("wheel", e => {
  if (fieldOpen) return;
  e.preventDefault();
  const d = (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? 0.008 : 0.035);
  sendParam("winch", (VAL.winch !== undefined ? VAL.winch : 0.5) + d);
}, { passive: false });

window.addEventListener("keydown", e => {
  if (e.key === "Escape" && fieldOpen) {`, "wheel"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wU();
console.log("wheel cranks the body through the plane; stir by heading; knead by stretch");
