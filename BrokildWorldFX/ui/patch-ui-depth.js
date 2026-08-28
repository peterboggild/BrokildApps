/*  DEPTH: how much of a destination a macro dials in, and which way.

    Assignment was landing at +100 % with no way to change it, which makes a
    macro all-or-nothing — the half of "what does this macro do" that actually
    shapes a patch was missing.

    The gesture is the Mars Wars patch bay's, because it is already the house
    convention and Peter knows it: WHEEL OVER A CABLE SETS THE AMOUNT. Here,
    while a macro is armed, the wheel over one of its wired controls moves the
    depth from -100 % to +100 %, and the row shows the number. Negative is
    drawn hollow there; here it simply reads with its sign, which is clearer
    on a row that already carries a label and a value.
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
  const A = a.split(String.fromCharCode(10)).join(NL);
  const B = b.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

// ── CSS: the depth badge on a wired row ────────────────────────────────────
rep('    ".bwfx-assigned{outline:1px solid var(--bwfx-acc);outline-offset:2px;border-radius:4px}",',
J([
'    ".bwfx-assigned{outline:1px solid var(--bwfx-acc);outline-offset:2px;border-radius:4px}",',
'    ".bwfx-dep{margin-left:6px;padding:0 4px;border-radius:3px;background:var(--bwfx-acc);",',
'    "color:#0a0d10;font:600 9px ui-monospace,Menlo,monospace;letter-spacing:.06em}",',
'    ".bwfx-dep.neg{background:transparent;color:var(--bwfx-acc);",',
'    "box-shadow:inset 0 0 0 1px var(--bwfx-acc)}",'
]), "css");

// ── the badge, kept in step by markAssignable ──────────────────────────────
rep(J([
"  function markAssignable() {",
"    if (!veil) return;",
"    veil.querySelectorAll('[data-bwfx-dest]').forEach(function (el) {",
"      var d = el.getAttribute('data-bwfx-dest');",
"      el.classList.toggle('bwfx-assignable', armed >= 0);",
"      el.classList.toggle('bwfx-assigned', macroOf(d) >= 0);",
"      el.title = macroOf(d) >= 0",
"        ? destLabel(d) + '  \\u00b7  MACRO ' + (macroOf(d) + 1) + ' at ' + depthOf(d) + ' %'",
"        : (armed >= 0 ? 'Click to assign to MACRO ' + (armed + 1) : '');",
"    });",
"  }"
]), J([
"  function markAssignable() {",
"    if (!veil) return;",
"    veil.querySelectorAll('[data-bwfx-dest]').forEach(function (el) {",
"      var d = el.getAttribute('data-bwfx-dest');",
"      var m = macroOf(d);",
"      el.classList.toggle('bwfx-assignable', armed >= 0);",
"      el.classList.toggle('bwfx-assigned', m >= 0);",
"      el.title = m >= 0",
"        ? destLabel(d) + '  \\u00b7  MACRO ' + (m + 1) + ' at ' + depthOf(d) + ' %'",
"          + (armed === m ? '  \\u00b7  wheel to change it' : '')",
"        : (armed >= 0 ? 'Click to assign to MACRO ' + (armed + 1) : '');",
"",
"      /*  The depth, on the row, in words — a macro whose amount you can only",
"          discover by hovering is a macro you cannot dial in. */",
"      var lab = el.querySelector('label') || el.querySelector('span');",
"      var bad = el.querySelector('.bwfx-dep');",
"      if (m < 0) { if (bad) bad.remove(); return; }",
"      if (!bad && lab) { bad = document.createElement('span'); bad.className = 'bwfx-dep'; lab.appendChild(bad); }",
"      if (bad) {",
"        var a = depthOf(d);",
"        bad.textContent = 'M' + (m + 1) + ' ' + (a > 0 ? '+' : '') + a;",
"        bad.classList.toggle('neg', a < 0);",
"      }",
"    });",
"  }",
"",
"  /*  Wheel over a wired control while its macro is armed: the amount, -100",
"     to +100. The Mars Wars patch bay's gesture, because it is already the",
"     house convention for exactly this question. Shift for single steps. */",
"  function wheelDepth(dest, dir, fine) {",
"    var m = macroOf(dest);",
"    if (m < 0 || m !== armed) return false;",
"    var a = depthOf(dest) + dir * (fine ? 1 : 10);",
"    a = Math.max(-100, Math.min(100, a));",
"    if (a === 0) a = dir > 0 ? 1 : -1;      // 0 would remove it; wheel never should",
"    if (send) send({ op: 'assign', i: m, d: dest, a: a });",
"    return true;",
"  }"
]), "badge + wheel");

// ── the wheel handler, beside the click one ────────────────────────────────
rep(J([
"    veil.addEventListener(\"keydown\", function (ev) {",
"      if (ev.key === \"Escape\" && armed >= 0) { armMacro(armed); ev.stopPropagation(); }",
"    });"
]), J([
"    veil.addEventListener(\"wheel\", function (ev) {",
"      if (armed < 0) return;",
"      var el = ev.target && ev.target.closest ? ev.target.closest(\"[data-bwfx-dest]\") : null;",
"      if (!el) return;",
"      if (wheelDepth(el.getAttribute(\"data-bwfx-dest\"), ev.deltaY < 0 ? 1 : -1, ev.shiftKey))",
"      { ev.preventDefault(); ev.stopPropagation(); }",
"    }, { capture: true, passive: false });",
"    veil.addEventListener(\"keydown\", function (ev) {",
"      if (ev.key === \"Escape\" && armed >= 0) { armMacro(armed); ev.stopPropagation(); }",
"    });"
]), "wheel handler");

// ── the hint line, so the gesture is discoverable at all ───────────────────
rep("      '  <div class=\"bwfx-macros\" id=\"bwfxMacros\"><div class=\"lab\">MACROS</div></div>' +",
    "      '  <div class=\"bwfx-macros\" id=\"bwfxMacros\"><div class=\"lab\">MACROS</div></div>' +" + NL
  + "      '  <div class=\"bwfx-machint\" id=\"bwfxMacHint\"></div>' +", "hint markup");

rep('    ".bwfx-dep.neg{background:transparent;color:var(--bwfx-acc);",',
J([
'    ".bwfx-dep.neg{background:transparent;color:var(--bwfx-acc);",',
'    ".bwfx-machint{padding:0 16px 8px;font:9.5px ui-monospace,Menlo,monospace;",',
'    "letter-spacing:.13em;color:#4d625e;min-height:13px}",'
]), "hint css");

rep(J([
"    for (var k = 0; k < macEls.length; k++) {"
]), J([
"    var hint = veil.querySelector('#bwfxMacHint');",
"    if (hint) hint.textContent = armed < 0",
"      ? 'CLICK A MACRO TO ARM IT \\u00b7 THEN CLICK ANY CONTROL TO PUT IT ON THAT MACRO'",
"      : ('MACRO ' + (armed + 1) + ' ARMED \\u00b7 CLICK A CONTROL TO ADD OR REMOVE IT'",
"         + ' \\u00b7 WHEEL OVER ONE TO SET HOW FAR IT GOES \\u00b7 ESC WHEN DONE');",
"    for (var k = 0; k < macEls.length; k++) {"
]), "hint text");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("depth: wheel over a wired control, and the row says how far");
