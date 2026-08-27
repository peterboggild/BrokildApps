/*  The pedal editor for the KIERANATOR round.

    A/B is an EDITING page, not a second pattern — the pedal is nowhere near
    wide enough for 32 cells at once, and the sequencer plays straight through
    A into B regardless of which one you are looking at. The playing loop is
    LAST, and cells past it are dimmed so the loop end is visible rather than
    something you have to remember.

    The brush keys carry their own colour: dim when idle, full strength with a
    halo when picked. That is buglist 12, and the real win is that a painted
    pattern becomes readable at a glance instead of sixteen identical lit
    cells — the steps already carry the same hue.
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

// ── the ninth brush ────────────────────────────────────────────────────────
rep(J([
  '    { c: "7", n: "GT",     t: "gate",      col: "#3fe0d8" }',
  "  ];"
]), J([
  '    { c: "7", n: "GT",     t: "gate",      col: "#3fe0d8" },',
  '    { c: "8", n: "??",     t: "random",    col: "#ff6ec7" }',
  "  ];"
]), "random brush");

rep("     The pattern is the module's opaque extra state: 16 chars '0'..'7'. */",
    J([
  "     The pattern is the module's opaque extra state: up to 32 chars",
  "     '0'..'8', where 8 is the RANDOM brush. A/B pages the 32 into two",
  "     screens of 16; LAST says where the loop actually ends. */"]), "header comment");

// ── CSS: coloured keys, page tabs, dimmed steps past LAST ──────────────────
rep(J([
  '    ".bwfx-brush.on{color:#0a0d10;border-color:transparent}",'
]), J([
  '    ".bwfx-brush.on{color:#0a0d10;border-color:transparent}",',
  "    /*  buglist 12: every brush wears its own hue — quietly when idle, at",
  "        full strength with a halo when it is the one in your hand. */",
  '    ".bwfx-brush{transition:background .12s,box-shadow .12s,color .12s}",',
  '    ".bwfx-brush.on{box-shadow:0 0 0 1px rgba(255,255,255,.25),0 0 11px -1px currentColor}",',
  '    ".bwfx-pages{display:flex;gap:4px;align-items:center;margin:0 0 5px}",',
  '    ".bwfx-page{border:1px solid rgba(255,255,255,.14);border-radius:5px;padding:2px 9px;",',
  '      "cursor:pointer;font:600 10px ui-monospace,Menlo,monospace;letter-spacing:.12em;color:#93a1a8}",',
  '    ".bwfx-page.on{background:var(--fxc,#3fe0d8);border-color:transparent;color:#0a0d10}",',
  '    ".bwfx-page.empty{opacity:.55}",',
  '    ".bwfx-pagenote{font:600 9.5px ui-monospace,Menlo,monospace;letter-spacing:.1em;",',
  '      "color:#6d7d86;margin-left:auto}",',
  "    /*  past LAST: still editable, but plainly not in the loop */",
  '    ".bwfx-step.bwfx-past{opacity:.28}",'
]), "brush css");

// ── the editor ─────────────────────────────────────────────────────────────
rep(J([
  "        var pat = typeof ms.x === " + '"string"' + " ? ms.x : " + '""' + ";",
  "        while (pat.length < 16) pat += " + '"0"' + ";",
  "        pat = pat.slice(0, 16);",
  '        var brush = "1";'
]), J([
  "        var NSTEP = 32, PAGE = 16;",
  "        var pat = typeof ms.x === " + '"string"' + " ? ms.x : " + '""' + ";",
  "        while (pat.length < NSTEP) pat += " + '"0"' + ";",
  "        pat = pat.slice(0, NSTEP);",
  '        var brush = "1", page = 0;',
  "",
  "        /*  Store SIXTEEN characters while page B is empty. The native side",
  "            does the same, and between them they keep every rack blob in",
  "            every saved project byte-identical to what it was before pages",
  "            existed. */",
  "        function packed() {",
  "          var n = NSTEP;",
  "          while (n > PAGE && pat.charAt(n - 1) === " + '"0"' + ") n--;",
  "          if (n <= PAGE) n = PAGE;",
  "          var out = pat.slice(0, n);",
  "          return /^0*$/.test(out) ? " + '""' + " : out;",
  "        }",
  "        function lastStep() {",
  "          var pd = null, i;",
  "          for (i = 0; i < d.params.length; i++) if (d.params[i].id === " + '"last"' + ") pd = d.params[i];",
  "          if (!pd) return NSTEP;",
  "          var v = ms.p && ms.p[" + '"last"' + "] !== undefined ? ms.p[" + '"last"' + "] : pd.def;",
  "          return Math.max(2, Math.min(NSTEP, Math.round(v)));",
  "        }"
]), "editor head");

rep(J([
  "        function paintCell(k) {",
  "          var c = pat[k];",
  "          var fx = STEPFX[parseInt(c, 10)] || STEPFX[0];",
  '          cells[k].style.background = c === "0" ? "#141b20" : fx.col;',
  '          cells[k].textContent = c === "0" ? "" : fx.n;',
  '          cells[k].setAttribute("data-q", (k % 4 === 0) ? "1" : "0");',
  "        }",
  "        function setCell(k, c) {",
  "          if (pat[k] === c) return;",
  "          pat = pat.slice(0, k) + c + pat.slice(k + 1);",
  '          ms.x = pat === "0000000000000000" ? "" : pat;',
  "          paintCell(k);",
  '          if (send) send({ op: "extra", m: id, x: pat });',
  "        }"
]), J([
  "        function paintCell(k) {",
  "          var abs = page * PAGE + k;",
  "          var c = pat[abs];",
  "          var fx = STEPFX[parseInt(c, 10)] || STEPFX[0];",
  '          cells[k].style.background = c === "0" ? "#141b20" : fx.col;',
  '          cells[k].textContent = c === "0" ? "" : fx.n;',
  '          cells[k].setAttribute("data-q", (abs % 4 === 0) ? "1" : "0");',
  '          cells[k].classList.toggle("bwfx-past", abs >= lastStep());',
  '          cells[k].title = "STEP " + (abs + 1) + (abs >= lastStep() ? "  (past LAST)" : "");',
  "        }",
  "        function paintAll() { for (var i = 0; i < PAGE; ++i) paintCell(i); paintTabs(); }",
  "        function setCell(k, c) {",
  "          var abs = page * PAGE + k;",
  "          if (pat[abs] === c) return;",
  "          pat = pat.slice(0, abs) + c + pat.slice(abs + 1);",
  "          ms.x = packed();",
  "          paintCell(k);",
  "          paintTabs();",
  '          if (send) send({ op: "extra", m: id, x: ms.x });',
  "        }"
]), "cell funcs");

rep(J([
  "        for (var k2 = 0; k2 < 16; ++k2) paintCell(k2);",
  "        wrap.appendChild(grid);"
]), J([
  "        wrap.appendChild(grid);"
]), "drop old paint");

rep(J([
  "        for (var k = 0; k < 16; ++k) (function (k) {"
]), J([
  "        for (var k = 0; k < PAGE; ++k) (function (k) {"
]), "cell loop");

// page tabs go ABOVE the grid, so build them before appending it
rep(J([
  "        var grid = document.createElement(" + '"div"' + ");",
  '        grid.className = "bwfx-steprow";',
  "        var cells = [];"
]), J([
  "        /*  A/B is an editing page. The sequencer plays straight through A",
  "            into B whatever is on screen — what stops the loop is LAST. */",
  "        var tabs = document.createElement(" + '"div"' + ");",
  '        tabs.className = "bwfx-pages";',
  "        var tabEls = [], pnote = null;",
  "        function paintTabs() {",
  "          var bEmpty = /^0*$/.test(pat.slice(PAGE));",
  "          tabEls.forEach(function (t, i) {",
  '            t.classList.toggle("on", i === page);',
  '            t.classList.toggle("empty", i === 1 && bEmpty && page !== 1);',
  "          });",
  "          if (pnote) pnote.textContent = " + '"LOOP ENDS AT STEP " + lastStep()' + ";",
  "        }",
  "        [" + '"A  1-16", "B  17-32"' + "].forEach(function (label, i) {",
  "          var t = document.createElement(" + '"div"' + ");",
  '          t.className = "bwfx-page";',
  "          t.textContent = label;",
  '          t.addEventListener("click", function () { page = i; paintAll(); });',
  "          tabEls.push(t); tabs.appendChild(t);",
  "        });",
  "        pnote = document.createElement(" + '"div"' + ");",
  '        pnote.className = "bwfx-pagenote";',
  "        tabs.appendChild(pnote);",
  "        wrap.appendChild(tabs);",
  "",
  "        var grid = document.createElement(" + '"div"' + ");",
  '        grid.className = "bwfx-steprow";',
  "        var cells = [];"
]), "page tabs");

// brush keys: colour when idle too, halo when picked
rep(J([
  '          b.className = "bwfx-brush" + (fx.c === brush ? " on" : "");',
  "          b.textContent = fx.n + " + '" "' + " + fx.t.toUpperCase();",
  '          if (fx.c === brush) b.style.background = fx.col;',
  '          b.addEventListener("click", function () {',
  "            brush = fx.c;",
  '            brushes.querySelectorAll(".bwfx-brush").forEach(function (x, i) {',
  "              var f = STEPFX[i + 1];",
  '              x.classList.toggle("on", f.c === brush);',
  '              x.style.background = f.c === brush ? f.col : "";',
  "            });",
  "          });"
]), J([
  '          b.className = "bwfx-brush" + (fx.c === brush ? " on" : "");',
  "          b.textContent = fx.n + " + '" "' + " + fx.t.toUpperCase();",
  "          //  the hue is always on, quietly; picking it turns it up",
  "          function dress(el, f, picked) {",
  "            el.style.color = picked ? " + '"#0a0d10"' + " : f.col;",
  "            el.style.background = picked ? f.col : tint(f.col, 0.13);",
  "            el.style.borderColor = picked ? " + '"transparent"' + " : tint(f.col, 0.4);",
  "          }",
  "          dress(b, fx, fx.c === brush);",
  '          b.addEventListener("click", function () {',
  "            brush = fx.c;",
  '            brushes.querySelectorAll(".bwfx-brush").forEach(function (x, i) {',
  "              var f = STEPFX[i + 1];",
  '              x.classList.toggle("on", f.c === brush);',
  "              dress(x, f, f.c === brush);",
  "            });",
  "          });"
]), "brush keys");

rep(J([
  '        hint.textContent = "PICK A BRUSH ' + String.fromCharCode(0xB7) + ' PAINT THE BAR ' + String.fromCharCode(0xB7) + ' SAME BRUSH AGAIN CLEARS A STEP";'
]), J([
  '        hint.textContent = "PICK A BRUSH ' + String.fromCharCode(0xB7) + ' PAINT ' + String.fromCharCode(0xB7)
    + ' SAME BRUSH CLEARS ' + String.fromCharCode(0xB7) + ' ?? DRAWS A DIFFERENT ONE EACH TIME";',
  "        paintAll();"
]), "hint");

// a tiny colour helper next to the brush table
rep(J([
  "  function descOf(id) {"
]), J([
  "  /*  A hue at low strength, for a control that should read as ITS colour",
  "     without shouting. rgba over the dark panel rather than a mix, so it",
  "     works whatever sits behind it. */",
  "  function tint(hex, a) {",
  "    var n = parseInt(hex.slice(1), 16);",
  '    return "rgba(" + ((n >> 16) & 255) + "," + ((n >> 8) & 255) + "," + (n & 255) + "," + a + ")";',
  "  }",
  "",
  "  function descOf(id) {"
]), "tint helper");

if (miss.length) { console.error("ABORT:" + NL + "  " + miss.join(NL + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("pedal editor: A/B pages, LAST marker, RANDOM brush, coloured keys");
