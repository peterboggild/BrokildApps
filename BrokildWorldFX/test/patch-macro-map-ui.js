/*  Reapply the fragment half of the mapping change, which the previous script
    silently threw away.

    That script built THREE edit closures — one for the C++ and two for
    bwfx-rack.js — each of which read the file at creation time and held its
    own copy. Running them in order meant the last writer overwrote the first
    one's work, and it reported success either way. Exactly the patcher bug
    already recorded in CLAUDE.md, wearing a different hat: one file, one
    read, one write.
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

// ── the mapping, matching Rack::applyMacros exactly ────────────────────────
rep(
'  function modOffset(dest, lo, hi) {\n'
+ '    if (!macros || !macroVals) return 0;\n'
+ '    var off = 0;\n'
+ '    for (var m = 0; m < macros.length; m++)\n'
+ '      for (var j = 0; j < macros[m].length; j++)\n'
+ '        if (macros[m][j].d === dest)\n'
+ '          off += (macroVals[m] || 0) * (macros[m][j].a / 100) * (hi - lo);\n'
+ '    return off;\n'
+ '  }\n'
+ '  function effective(dest, base, lo, hi) {\n'
+ '    var v = base + modOffset(dest, lo, hi);\n'
+ '    return v < lo ? lo : (v > hi ? hi : v);\n'
+ '  }',

'  /*  The SAME mapping Rack::applyMacros computes, sign and clamp included.\n'
+ '     Duplicated here deliberately: the alternative is a panel showing one\n'
+ '     thing while the ears hear another, which is the bug this replaced.\n'
+ '\n'
+ '       value = (depth >= 0 ? lo : hi) + macro * depth * (hi - lo)\n'
+ '\n'
+ '     so +100 % runs bottom to top and -100 % runs top to bottom. */\n'
+ '  function macroOwning(dest) {\n'
+ '    if (!macros) return -1;\n'
+ '    for (var m = 0; m < macros.length; m++)\n'
+ '      for (var j = 0; j < macros[m].length; j++)\n'
+ '        if (macros[m][j].d === dest) return m;\n'
+ '    return -1;\n'
+ '  }\n'
+ '  function effective(dest, base, lo, hi) {\n'
+ '    var m = macroOwning(dest);\n'
+ '    if (m < 0 || !macroVals) return base;\n'
+ '    var depth = depthOf(dest) / 100;\n'
+ '    var from = depth >= 0 ? lo : hi;\n'
+ '    var v = from + (macroVals[m] || 0) * depth * (hi - lo);\n'
+ '    return v < lo ? lo : (v > hi ? hi : v);\n'
+ '  }', "formula");

// ── the slider follows, because the macro owns it ──────────────────────────
rep(
'      var e = effective(r.dest, r.base(), r.pd.lo, r.pd.hi);\n'
+ '      var moved = Math.abs(e - r.base()) > (r.pd.hi - r.pd.lo) * 1e-4;\n'
+ '      r.out.textContent = fmt(r.pd, e);\n'
+ '      r.out.classList.toggle(\'bwfx-modded\', moved);',

'      var owned = macroOwning(r.dest) >= 0;\n'
+ '      var e = effective(r.dest, r.base(), r.pd.lo, r.pd.hi);\n'
+ '      r.out.textContent = fmt(r.pd, e);\n'
+ '      r.out.classList.toggle(\'bwfx-modded\', owned);\n'
+ '      /*  The macro OWNS this control, so the slider goes where the macro\n'
+ '          puts it. A number that moved while the slider sat still was the\n'
+ '          most confusing part of the additive version. */\n'
+ '      if (r.input) {\n'
+ '        r.input.classList.toggle(\'bwfx-owned\', owned);\n'
+ '        if (owned) r.input.value = e;\n'
+ '      }', "display");

rep(
'          MODROWS.push({ row: row, out: out, pd: pd, dest: id + "." + pd.id,\n'
+ '                         base: function () { return parseFloat(inp.value); } });',
'          MODROWS.push({ row: row, out: out, input: inp, pd: pd, dest: id + "." + pd.id,\n'
+ '                         base: function () { return parseFloat(inp.value); } });', "register");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the fragment now computes the mapping too");
