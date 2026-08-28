/*  SHOW WHERE A MACRO HAS PUT THINGS.

    An assigned control kept showing the patch's own value, which was a
    deliberate decision and the wrong one: the macro was working and there was
    nothing on screen that said so, and "the slider does nothing" is the only
    reasonable conclusion from that. A control a listener cannot see moving is
    indistinguishable from a control that is broken.

    So the row now shows the EFFECTIVE value — where the macros have actually
    put it — in the macro colour, while the slider itself stays at the patch
    value and stays editable. Same split as the badge: the slider is what the
    patch says, the number is what the rack is doing.

    The arithmetic is duplicated here on purpose, exactly as the Full Metal
    Racket morph display duplicates its blend: the alternative is a panel that
    shows one thing while the ears hear another, which is the bug this is
    fixing. Same formula, same clamp, so the two cannot disagree.
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

// ── the shared formula, and a redraw that walks every modulated row ─────────
rep("  function macroOf(dest) {",
[
"  /*  What the macros are adding to this destination right now. The SAME",
"     expression Rack::applyMacros computes, including the clamp — a display",
"     that used a different one would be the very bug this is fixing. */",
"  function modOffset(dest, lo, hi) {",
"    if (!macros || !macroVals) return 0;",
"    var off = 0;",
"    for (var m = 0; m < macros.length; m++)",
"      for (var j = 0; j < macros[m].length; j++)",
"        if (macros[m][j].d === dest)",
"          off += (macroVals[m] || 0) * (macros[m][j].a / 100) * (hi - lo);",
"    return off;",
"  }",
"  function effective(dest, base, lo, hi) {",
"    var v = base + modOffset(dest, lo, hi);",
"    return v < lo ? lo : (v > hi ? hi : v);",
"  }",
"",
"  /*  Every modulated row repaints its number. Called when a macro moves and",
"     after every state adoption, so automation shows up too. */",
"  var MODROWS = [];",
"  function drawModValues() {",
"    for (var i = 0; i < MODROWS.length; i++) {",
"      var r = MODROWS[i];",
"      if (!r.out || !r.row.isConnected) continue;",
"      var e = effective(r.dest, r.base(), r.pd.lo, r.pd.hi);",
"      var moved = Math.abs(e - r.base()) > (r.pd.hi - r.pd.lo) * 1e-4;",
"      r.out.textContent = fmt(r.pd, e);",
"      r.out.classList.toggle('bwfx-modded', moved);",
"    }",
"  }",
"",
"  function macroOf(dest) {"
].join("\n"), "formula");

// ── register each continuous row so it can be repainted ────────────────────
rep(
'          var inp = row.querySelector("input");\n'
+ '          var out = row.querySelector("output");\n'
+ '          inp.addEventListener("input", function () {\n'
+ '            var nv = parseFloat(inp.value);\n'
+ '            out.textContent = fmt(pd, nv);\n'
+ '            setParamLocal(id, pd, nv);\n'
+ '          });',
'          var inp = row.querySelector("input");\n'
+ '          var out = row.querySelector("output");\n'
+ '          //  the slider stays at the PATCH value and stays editable; the\n'
+ '          //  number shows where the macros have put it\n'
+ '          MODROWS.push({ row: row, out: out, pd: pd, dest: id + "." + pd.id,\n'
+ '                         base: function () { return parseFloat(inp.value); } });\n'
+ '          inp.addEventListener("input", function () {\n'
+ '            var nv = parseFloat(inp.value);\n'
+ '            out.textContent = fmt(pd, nv);\n'
+ '            setParamLocal(id, pd, nv);\n'
+ '            drawModValues();\n'
+ '          });',
  "register rows");

// ── the list is rebuilt from scratch on every render, so the registry is too ─
rep("    var list = document.createElement(\"div\");" + NL + "    list.className = \"bwfx-list\";",
    "    MODROWS = [];      // renderList rebuilds every row" + NL
  + "    var list = document.createElement(\"div\");" + NL + "    list.className = \"bwfx-list\";",
  "reset registry");

// ── repaint when a macro moves, and after a state adoption ─────────────────
rep(
"          if (macroVals) macroVals[i] = parseInt(r.value, 10) / 100;\n"
+ "          if (send) send({ op: 'macro', i: i, v: parseInt(r.value, 10) / 100 });",
"          if (macroVals) macroVals[i] = parseInt(r.value, 10) / 100;\n"
+ "          drawModValues();\n"
+ "          if (send) send({ op: 'macro', i: i, v: parseInt(r.value, 10) / 100 });",
  "fader repaint");

rep("      if (built) { renderList(); renderSpectra(); drawMacros(); markAssignable(); }",
    "      if (built) { renderList(); renderSpectra(); drawMacros(); markAssignable(); drawModValues(); }",
  "state repaint");

rep("    markAssignable();\n    drawMacros();\n    if (send) send({ op: 'assign', i: m, d: dest, a: a });",
    "    markAssignable();\n    drawMacros();\n    drawModValues();\n    if (send) send({ op: 'assign', i: m, d: dest, a: a });",
  "wheel repaint");

// ── the modulated number wears the macro colour ────────────────────────────
rep('    ".bwfx-dep{margin-left:6px;padding:0 4px;border-radius:3px;background:var(--bwfx-acc);",',
[
'    ".bwfx-modded{color:var(--bwfx-acc)!important;text-shadow:0 0 6px rgba(63,224,216,.35)}",',
'    ".bwfx-dep{margin-left:6px;padding:0 4px;border-radius:3px;background:var(--bwfx-acc);",'
].join(NL), "modded css");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the row now shows where the macros put it");
