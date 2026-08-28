/*  The state echo rebuilds the pedal list, which throws away every ring the
    rail had drawn — so the rail and the rings are redrawn straight after it.
    (The overlay-arming trap from the plate pipeline, in a different guise:
    the native echo re-renders the list a beat after you touched it.)
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];
function rep(a, b, tag) {
  const A = a.split(String.fromCharCode(10)).join(NL);
  const B = b.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

rep("      if (built) { renderList(); renderSpectra(); }",
    "      if (built) { renderList(); renderSpectra(); drawMacros(); markAssignable(); }",
    "state redraw");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the rail survives the state echo");
