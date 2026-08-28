/*  RACK MIX is a macro destination like any other — and it is the one that
    ships assigned, to macro 5 — but the header row was still showing its base
    value, so the default macro appeared to do nothing. Same omission as the
    pedal rows had, in the one place it matters most.
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

rep(
"  function drawModValues() {\n"
+ "    for (var i = 0; i < MODROWS.length; i++) {",

"  function drawModValues() {\n"
+ "    /*  RACK MIX first: it is a destination like any other, and it is the\n"
+ "        one macro 5 ships assigned to, so leaving it on its base value made\n"
+ "        the default macro look inert. */\n"
+ "    if (mixIn && mixOut) {\n"
+ "      var mOwned = macroOwning('mix') >= 0;\n"
+ "      var me = effective('mix', state.mix, 0, 1);\n"
+ "      mixOut.textContent = Math.round(me * 100) + ' %';\n"
+ "      mixOut.classList.toggle('bwfx-modded', mOwned);\n"
+ "      mixIn.classList.toggle('bwfx-owned', mOwned);\n"
+ "      if (mOwned) mixIn.value = Math.round(me * 100);\n"
+ "    }\n"
+ "    for (var i = 0; i < MODROWS.length; i++) {", "mix row");

//  and the mix row is itself assignable, so it must repaint on a macro move
rep(
"      mixIn.value = Math.round(state.mix * 100);\n"
+ "      mixOut.textContent = Math.round(state.mix * 100) + \" %\";",
"      mixIn.value = Math.round(state.mix * 100);\n"
+ "      mixOut.textContent = Math.round(state.mix * 100) + \" %\";\n"
+ "      drawModValues();          // ...unless a macro owns it", "sync");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the rack mix shows where its macro puts it");
