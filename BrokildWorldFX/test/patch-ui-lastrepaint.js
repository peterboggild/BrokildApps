/*  Moving LAST did nothing on screen: the note still said STEP 16 and the
    dimming did not move, because the step grid is built once and only
    repaints when a cell is painted. The value was reaching the engine all
    along — this is the Photo-Synth Q-factor bug again, a control that works
    and looks broken, and it is the same fix: find every consumer of the value
    you added, the display as well as the audio.

    A custom editor registers what it wants to be told about, so the generic
    parameter setter does not have to know the word "last".
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const miss = [];
const J = (a) => a.join(NL);
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

rep(J([
  "  function setParamLocal(id, pd, v) {",
  "    var ms = state.modules[id];",
  "    if (!ms) return;",
  "    ms.p[pd.id] = v;",
  '    if (send) send({ op: "set", m: id, p: pd.id, v: v });',
  "  }"
]), J([
  "  /*  A custom pedal editor can ask to be redrawn when one of the module's",
  "     own knobs moves — LAST changes where the loop ends, and the grid shows",
  "     that. Keyed by module and parameter so the generic setter never has to",
  "     know what a step grid is. */",
  "  var CUSTOM_REDRAW = {};",
  "",
  "  function setParamLocal(id, pd, v) {",
  "    var ms = state.modules[id];",
  "    if (!ms) return;",
  "    ms.p[pd.id] = v;",
  '    if (send) send({ op: "set", m: id, p: pd.id, v: v });',
  "    var cr = CUSTOM_REDRAW[id];",
  "    if (cr && cr.on[pd.id]) cr.fn();",
  "  }"
]), "setParamLocal");

rep(J([
  '        hint.textContent = "PICK A BRUSH ' + String.fromCharCode(0xB7) + ' PAINT ' + String.fromCharCode(0xB7)
    + ' SAME BRUSH CLEARS ' + String.fromCharCode(0xB7) + ' ?? DRAWS A DIFFERENT ONE EACH TIME";',
  "        paintAll();"
]), J([
  '        hint.textContent = "PICK A BRUSH ' + String.fromCharCode(0xB7) + ' PAINT ' + String.fromCharCode(0xB7)
    + ' SAME BRUSH CLEARS ' + String.fromCharCode(0xB7) + ' ?? DRAWS A DIFFERENT ONE EACH TIME";',
  "        CUSTOM_REDRAW[id] = { on: { last: 1 }, fn: paintAll };",
  "        paintAll();"
]), "register redraw");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("LAST now repaints the grid");
