/*  The rose is drawn in dark chart ink, and the terrain view is dark, so it
    was invisible. Its ALPHA is the drawing, so use it as a stencil: draw it
    once into an offscreen canvas, then fill through it with pale brass.
    Cached at load, not rebuilt per frame. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b) {
  const c = s.split(a).length - 1;
  if (c !== 1) { misses.push("expected 1 of " + JSON.stringify(a.slice(0, 72)) + ", found " + c); return; }
  s = s.split(a).join(b);
}

rep(`["pearl","tack","flag","bezel","glass","wood","ground","paper","rose"].forEach(n=>{
  const o = {ok:false, img:new Image()};
  o.img.onload = ()=>{ o.ok = true; document.body.classList.add("dec-"+n); };`,
`["pearl","tack","flag","bezel","glass","wood","ground","paper","rose"].forEach(n=>{
  const o = {ok:false, img:new Image()};
  o.img.onload = ()=>{ o.ok = true; document.body.classList.add("dec-"+n);
    if(n==="rose") o.tint = tintStencil(o.img, "#c9a24a"); };`);

rep(`/* where the balls are this frame, in world space, for the overlay to place a
   pearl on each: filled by fillScene, which already has the projection maths */
const PEARLS = [];`,
`/* Line art in dark ink cannot be seen on a dark panel. The alpha IS the
   drawing, so fill through it with a colour that can. */
function tintStencil(img, colour){
  const c = document.createElement("canvas");
  c.width = img.width; c.height = img.height;
  const x = c.getContext("2d");
  x.drawImage(img, 0, 0);
  x.globalCompositeOperation = "source-in";
  x.fillStyle = colour; x.fillRect(0, 0, c.width, c.height);
  return c;
}

/* where the balls are this frame, in world space, for the overlay to place a
   pearl on each: filled by fillScene, which already has the projection maths */
const PEARLS = [];`);

rep(`  if(IMG.rose.ok){
    const RS=Math.min(430, Math.max(210, W*0.30));
    g.globalAlpha=0.10;
    g.drawImage(IMG.rose.img, W-RS*0.62, Hh-RS*0.30, RS, RS*IMG.rose.img.height/IMG.rose.img.width);
    g.globalAlpha=1;
  }`,
`  if(IMG.rose.ok && IMG.rose.tint){
    const r=IMG.rose.tint, RS=Math.min(480, Math.max(240, W*0.34));
    g.globalAlpha=0.20;
    g.drawImage(r, W-RS*0.60, Hh-RS*0.30, RS, RS*r.height/r.width);
    g.globalAlpha=1;
  }`);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("the rose is drawn through its own alpha in brass");
