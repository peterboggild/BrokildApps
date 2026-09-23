// Panel round 2, from looking at the render:
//  1. FX has four models where everything else has three, so its knob rows sat
//     lower than the other eleven and the whole grid lost its line. The model
//     block gets a fixed height — alignment across twelve strips matters more
//     than a tight box on eleven of them.
//  2. The rail had a large dead black area. It gets a master output meter,
//     which is useful, fills it honestly, and belongs on a machine.
//  3. Chunkier fader caps — they read as thin at panel scale.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}

const U = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
const E = "C:/Users/peter/b/FullMetalRacket/Source/PluginEditor.cpp";

const writeU = edit(U, [
  // 1 · every strip's knobs start at the same y, whatever its model count
  [`.mods{flex:none;display:flex;flex-direction:column;gap:2px}`,
   `/*  Fixed height, room for four keys. FX has four models and everything
    else has three; letting the block shrink dropped FX's knobs a row out of
    line with the other eleven and the grid stopped reading as a grid. */
.mods{flex:none;display:flex;flex-direction:column;gap:2px;height:90px;justify-content:flex-start}`,
   "mods height"],

  // 2 · the master meter in the rail
  [`#rail .fill{flex:1}`,
   `#rail .fill{flex:1;display:flex;flex-direction:column;gap:6px;justify-content:flex-end;padding:6px 0}
#vu{flex:1;min-height:60px;display:flex;gap:4px;align-items:stretch;justify-content:center}
#vu .col{width:16px;border-radius:2px;position:relative;overflow:hidden;
  background:linear-gradient(180deg,#0c0803,#221a0e);
  box-shadow:inset 0 0 4px #000,0 1px 0 #ffffff18}
#vu .col b{position:absolute;left:0;right:0;bottom:0;height:0;display:block;
  background:linear-gradient(180deg,#ff5a2a,#ffb35c 38%,#c2600f);box-shadow:0 0 8px #ff8f1f88}
#vulab{font:700 7.5px/1 var(--f-lab);letter-spacing:.2em;text-transform:uppercase;
  color:#8d7d5e;text-align:center;flex:none}`,
   "vu css"],

  [`  el("div", "fill", rail);
  const g = el("div", "", rail);`,
   `  { const f = el("div", "fill", rail);
    const vu = el("div", "", f); vu.id = "vu";
    VUCOL[0] = el("b", "", el("div", "col", vu));
    VUCOL[1] = el("b", "", el("div", "col", vu));
    const l = el("div", "", f); l.id = "vulab"; l.textContent = "Out"; }
  const g = el("div", "", rail);`,
   "vu markup"],

  [`let built = false;

function build() {`,
   `let built = false;
const VUCOL = [null, null];
let vuHold = [0, 0];

function build() {`,
   "vu state"],

  // drive the meter from the per-channel levels we already receive
  [`NB.on("meter", p => {
  const m = p.m || [];`,
   `NB.on("meter", p => {
  const m = p.m || [];
  /*  A master meter from the channel levels we already have: the sum is what
      reaches the bus, and it falls back slowly so a hit stays visible for
      longer than the 15 Hz the values arrive at. */
  let sum = 0;
  for (let i = 0; i < m.length; i++) sum += m[i] || 0;
  const lvl = clamp(sum * 0.85, 0, 1);
  for (let ch = 0; ch < 2; ch++) {
    vuHold[ch] = Math.max(lvl, vuHold[ch] * 0.78);
    if (VUCOL[ch]) VUCOL[ch].style.height = (vuHold[ch] * 100).toFixed(1) + "%";
  }`,
   "vu drive"],

  // 3 · chunkier caps
  [`.fader .cap{position:absolute;left:1px;width:24px;height:22px;border-radius:3px;`,
   `.fader .cap{position:absolute;left:0;width:26px;height:26px;border-radius:3px;`,
   "fader cap size"],
  [`    cap.style.top = ((1 - t) * Math.max(0, node.clientHeight - 22)) + "px";`,
   `    cap.style.top = ((1 - t) * Math.max(0, node.clientHeight - 26)) + "px";`,
   "fader cap travel"],
  [`.fader{position:relative;width:26px;height:96px;`,
   `.fader{position:relative;width:26px;height:100px;`,
   "fader height"]
]);

// 4 · the editor opens at the deck's own aspect, so there are no bars at all
//     until the user resizes. (Scaled-to-fit bars are normal, but they were
//     the first thing anyone noticed.)
const writeE = edit(E, [
  [`    setResizeLimits (980, 560, 3800, 2200);
    setSize (1680, 900);`,
   `    setResizeLimits (980, 558, 3800, 2200);
    setSize (1720, 980);           // exactly the deck's canvas: no letterbox at open`,
   "editor size"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeU(); writeE();
console.log("panel round 2 patched OK");
