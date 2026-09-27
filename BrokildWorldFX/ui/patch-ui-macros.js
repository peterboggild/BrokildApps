/*  The MACROS rail: five faders across the bottom of the rack window, and the
    arm-then-click gesture that wires them.

    Design notes that the code should not have to re-derive:
      * the rail is ALWAYS visible, not behind a mode — you reach for it while
        playing;
      * a macro ADDS, so an assigned knob keeps showing the patch's own value.
        The ring says "something is moving this", not "this is not yours";
      * MACRO 5 ships assigned to the rack's dry/wet. Nothing about it is
        special-cased — it is an ordinary assignment that happens to be there,
        and clicking it away frees a fifth macro like any other.
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

// ── CSS ────────────────────────────────────────────────────────────────────
rep('    ".bwfx-foot{padding:8px 18px 2px;font:10px ui-monospace,Menlo,monospace;letter-spacing:.14em;color:#4d625e;",',
  J([
'    /* --- the macro rail: five faders, always visible --- */',
'    ".bwfx-macros{display:flex;align-items:stretch;gap:10px;padding:10px 16px 12px;',
'      border-top:1px solid #1d2830;background:linear-gradient(180deg,#0d151a,#0a1116)}",',
'    ".bwfx-macros>.lab{font:600 9.5px ui-monospace,Menlo,monospace;letter-spacing:.2em;',
'      color:#5d7a74;align-self:center;flex:none;writing-mode:horizontal-tb}",',
'    ".bwfx-mac{flex:1 1 0;min-width:0;display:grid;gap:3px}",',
'    ".bwfx-mac .mn{display:flex;align-items:center;gap:5px;cursor:pointer;',
'      font:600 10px ui-monospace,Menlo,monospace;letter-spacing:.12em;color:#8ea6a1;',
'      border:1px solid transparent;border-radius:4px;padding:1px 4px}",',
'    ".bwfx-mac .mn:hover{color:#cfe4df}",',
'    ".bwfx-mac.armed .mn{color:#0a0d10;background:var(--bwfx-acc);border-color:transparent}",',
'    ".bwfx-mac .cnt{margin-left:auto;font-size:9px;opacity:.8}",',
'    ".bwfx-mac input[type=range]{width:100%;margin:0}",',
'    ".bwfx-mac output{font:600 9.5px ui-monospace,Menlo,monospace;color:#6d8a85}",',
'    /* an assignable control while a macro is armed */',
'    ".bwfx-assignable{outline:1px dashed rgba(63,224,216,.55);outline-offset:2px;border-radius:4px;',
'      cursor:crosshair}",',
'    ".bwfx-assigned{outline:1px solid var(--bwfx-acc);outline-offset:2px;border-radius:4px}",',
'    ".bwfx-foot{padding:8px 18px 2px;font:10px ui-monospace,Menlo,monospace;letter-spacing:.14em;color:#4d625e;",'
  ]), "css");

// ── markup: the rail sits between the columns and the footer ───────────────
rep("      '  <div class=\"bwfx-foot\"><span>BWFX ' + VERSION + '</span>' +",
  J([
"      '  <div class=\"bwfx-macros\" id=\"bwfxMacros\"><div class=\"lab\">MACROS</div></div>' +",
"      '  <div class=\"bwfx-foot\"><span>BWFX ' + VERSION + '</span>' +"
  ]), "markup");

// ── state + the rail itself ────────────────────────────────────────────────
rep("  var veil = null, listEl = null, mixIn = null, mixOut = null;",
  J([
"  var veil = null, listEl = null, mixIn = null, mixOut = null;",
"  /*  macros[i] = [{d,a}, ...] — the WIRING, which is rack structure and",
"     comes from the native side. macroVals[i] is the live host-parameter",
"     value: the rail shows it and can nudge it, but the host owns it. */",
"  var macros = null, macroVals = null, macEls = [], armed = -1;"
  ]), "state");

rep("  function move(id, dir) {",
  J([
"  /*  A destination key, the same string the native side resolves: 'mix',",
"     '<module>.<param>', '<module>.pr'. Addressed by NAME so an assignment",
"     survives the registry changing under it. */",
"  function destLabel(d) {",
"    if (d === 'mix') return 'RACK MIX';",
"    var dot = d.indexOf('.');",
"    if (dot < 0) return d.toUpperCase();",
"    var m = descOf(d.slice(0, dot)), p = d.slice(dot + 1);",
"    var mn = m ? m.name : d.slice(0, dot).toUpperCase();",
"    if (p === 'pr') return mn + ' PRESENCE';",
"    if (m) for (var i = 0; i < m.params.length; i++) if (m.params[i].id === p) return mn + ' ' + m.params[i].name;",
"    return mn + ' ' + p.toUpperCase();",
"  }",
"",
"  function macroOf(dest) {",
"    if (!macros) return -1;",
"    for (var i = 0; i < macros.length; i++)",
"      for (var j = 0; j < macros[i].length; j++) if (macros[i][j].d === dest) return i;",
"    return -1;",
"  }",
"  function depthOf(dest) {",
"    if (!macros) return 0;",
"    for (var i = 0; i < macros.length; i++)",
"      for (var j = 0; j < macros[i].length; j++) if (macros[i][j].d === dest) return macros[i][j].a;",
"    return 0;",
"  }",
"",
"  /*  Arming is a mode, and the only one in the overlay — it has to be easy",
"     to leave. Escape, clicking the macro again, or picking another. */",
"  function armMacro(i) {",
"    armed = (armed === i) ? -1 : i;",
"    drawMacros();",
"    markAssignable();",
"  }",
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
"  }",
"",
"  /*  Clicking an assignable control while a macro is armed: assign at full",
"     positive depth, or take it away if this macro already has it. */",
"  function assignClick(dest) {",
"    if (armed < 0) return false;",
"    var cur = macroOf(dest);",
"    var a = (cur === armed) ? 0 : 100;",
"    if (send) send({ op: 'assign', i: armed, d: dest, a: a });",
"    return true;",
"  }",
"",
"  function drawMacros() {",
"    var wrap = veil && veil.querySelector('#bwfxMacros');",
"    if (!wrap) return;",
"    if (!macEls.length) {",
"      for (var i = 0; i < 5; i++) (function (i) {",
"        var c = document.createElement('div');",
"        c.className = 'bwfx-mac';",
"        var nm = document.createElement('div');",
"        nm.className = 'mn';",
"        nm.innerHTML = '<span>M' + (i + 1) + '</span><span class=\"cnt\"></span>';",
"        nm.addEventListener('click', function () { armMacro(i); });",
"        var r = document.createElement('input');",
"        r.type = 'range'; r.min = 0; r.max = 100; r.value = 0;",
"        r.addEventListener('input', function () {",
"          if (macroVals) macroVals[i] = parseInt(r.value, 10) / 100;",
"          if (send) send({ op: 'macro', i: i, v: parseInt(r.value, 10) / 100 });",
"          var o = c.querySelector('output'); if (o) o.textContent = r.value + ' %';",
"        });",
"        var o = document.createElement('output');",
"        o.textContent = '0 %';",
"        c.appendChild(nm); c.appendChild(r); c.appendChild(o);",
"        wrap.appendChild(c);",
"        macEls.push({ box: c, name: nm, range: r, out: o });",
"      })(i);",
"    }",
"    for (var k = 0; k < macEls.length; k++) {",
"      var e = macEls[k];",
"      var list = (macros && macros[k]) ? macros[k] : [];",
"      e.box.classList.toggle('armed', armed === k);",
"      e.name.querySelector('.cnt').textContent = list.length ? String(list.length) : '';",
"      e.name.title = list.length",
"        ? list.map(function (x) { return destLabel(x.d) + ' ' + (x.a > 0 ? '+' : '') + x.a + ' %'; }).join('\\n')",
"        : 'Click to arm, then click a control to assign it';",
"      var v = macroVals ? Math.round((macroVals[k] || 0) * 100) : 0;",
"      e.range.value = v;",
"      e.out.textContent = v + ' %';",
"    }",
"  }",
"",
"  function move(id, dir) {"
  ]), "rail");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("overlay: the macro rail");
