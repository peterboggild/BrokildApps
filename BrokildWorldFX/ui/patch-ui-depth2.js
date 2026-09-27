/*  Wheeling was reading the depth back from the native echo, so seven notches
    in quick succession all read the same stale value and each sent the same
    result — the wheel moved one step no matter how far you turned it.

    The rail updates its own copy first and then tells the native side, which
    is the right way round for a gesture: the echo confirms rather than
    supplies. It is also what every knob in this overlay already does.
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
"  function wheelDepth(dest, dir, fine) {\n"
+ "    var m = macroOf(dest);\n"
+ "    if (m < 0 || m !== armed) return false;\n"
+ "    var a = depthOf(dest) + dir * (fine ? 1 : 10);\n"
+ "    a = Math.max(-100, Math.min(100, a));\n"
+ "    if (a === 0) a = dir > 0 ? 1 : -1;      // 0 would remove it; wheel never should\n"
+ "    if (send) send({ op: 'assign', i: m, d: dest, a: a });\n"
+ "    return true;\n"
+ "  }",

"  function wheelDepth(dest, dir, fine) {\n"
+ "    var m = macroOf(dest);\n"
+ "    if (m < 0 || m !== armed) return false;\n"
+ "    var a = depthOf(dest) + dir * (fine ? 1 : 10);\n"
+ "    a = Math.max(-100, Math.min(100, a));\n"
+ "    if (a === 0) a = dir > 0 ? 1 : -1;      // 0 would remove it; wheel never should\n"
+ "    /*  Move OUR copy first, then tell the native side. Reading the depth\n"
+ "        back from the echo made every notch of a fast wheel read the same\n"
+ "        stale value, so the control moved one step however far you turned\n"
+ "        it. The echo confirms; it does not supply. */\n"
+ "    for (var j = 0; j < macros[m].length; j++)\n"
+ "      if (macros[m][j].d === dest) { macros[m][j].a = a; break; }\n"
+ "    markAssignable();\n"
+ "    drawMacros();\n"
+ "    if (send) send({ op: 'assign', i: m, d: dest, a: a });\n"
+ "    return true;\n"
+ "  }", "optimistic wheel");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the wheel moves as far as you turn it");
