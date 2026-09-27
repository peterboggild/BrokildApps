/*  Wire the rail up: tag the assignable controls, route a click while a macro
    is armed into an assignment instead of an edit, and take the wiring and
    the live values off the state event.
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

// ── a module's PRESENCE is assignable ──────────────────────────────────────
rep(
'        var row = document.createElement("div");\n'
+ '        row.className = "bwfx-row bwfx-presrow";\n'
+ '        var v = Math.round((typeof ms.pr === "number" ? ms.pr : 1) * 100);',
'        var row = document.createElement("div");\n'
+ '        row.className = "bwfx-row bwfx-presrow";\n'
+ '        row.setAttribute("data-bwfx-dest", id + ".pr");\n'
+ '        var v = Math.round((typeof ms.pr === "number" ? ms.pr : 1) * 100);',
  "presence dest");

// ── every module parameter is assignable ───────────────────────────────────
rep(
'      d.params.forEach(function (pd) {\n'
+ '        var row = document.createElement("div");\n'
+ '        row.className = "bwfx-row" + (pd.choices ? " bwfx-wide" : "");',
'      d.params.forEach(function (pd) {\n'
+ '        var row = document.createElement("div");\n'
+ '        row.className = "bwfx-row" + (pd.choices ? " bwfx-wide" : "");\n'
+ '        row.setAttribute("data-bwfx-dest", id + "." + pd.id);',
  "param dest");

// ── the rack mix is assignable ─────────────────────────────────────────────
rep(
"      '    <div class=\"bwfx-mixrow\"><span>RACK MIX</span>' +",
"      '    <div class=\"bwfx-mixrow\" data-bwfx-dest=\"mix\"><span>RACK MIX</span>' +",
  "mix dest");

// ── one capture-phase click handler turns an edit into an assignment ───────
rep(
"    listEl = veil.querySelector(\"#bwfxList\");",
J([
"    /*  While a macro is armed, a click on an assignable control WIRES it",
"       rather than editing it. Capture phase, so the row's own range/select",
"       listeners never see the event — anything else would move the knob on",
"       the way to assigning it. */",
"    veil.addEventListener(\"pointerdown\", function (ev) {",
"      if (armed < 0) return;",
"      var el = ev.target && ev.target.closest ? ev.target.closest(\"[data-bwfx-dest]\") : null;",
"      if (!el) return;",
"      ev.preventDefault();",
"      ev.stopPropagation();",
"      assignClick(el.getAttribute(\"data-bwfx-dest\"));",
"    }, true);",
"    veil.addEventListener(\"keydown\", function (ev) {",
"      if (ev.key === \"Escape\" && armed >= 0) { armMacro(armed); ev.stopPropagation(); }",
"    });",
"",
"    listEl = veil.querySelector(\"#bwfxList\");"
]), "capture click");

// ── the state event carries the wiring and the values ──────────────────────
rep(
"      if (typeof p.busLive === \"boolean\") busLive = p.busLive;",
J([
"      if (typeof p.busLive === \"boolean\") busLive = p.busLive;",
"      //  the wiring is rack STRUCTURE and comes from the blob; the values",
"      //  are host parameters and the rail only mirrors them",
"      if (p.macros) { macros = p.macros; }",
"      if (p.macroVals) { macroVals = p.macroVals; }"
]), "state macros");

// ── redraw after every state adoption, and when the overlay opens ──────────
rep(
"  function openRack() {\n"
+ "    build();\n"
+ "    open = true;",
"  function openRack() {\n"
+ "    build();\n"
+ "    armed = -1;                       // never open already in a mode\n"
+ "    drawMacros();\n"
+ "    markAssignable();\n"
+ "    open = true;",
  "open redraw");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("rail wired: destinations tagged, clicks captured, state adopted");
