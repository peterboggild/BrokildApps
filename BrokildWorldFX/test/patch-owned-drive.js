// BWFX 1.7.1: a hand on a MACRO-OWNED control moves the macro.
//
// Peter, 1984, 2026-09-23: "not sure the BWFX rack mix works - when BWFX are
// on, and rack mix is reduced to zero, FXs still play". Macro 5 ships wired to
// the rack mix at -100 %, and a macro MAPS its destination: the rack computes
// mixOff = mapped - base every block, so a raw setMix(0) from the overlay is
// put straight back to 1 - macro5. The slider moved; the effects stayed.
// The overlay already paints an owned control teal and moves it where the
// macro puts it; what it did not do was route a drag on it THROUGH the macro.
// Now it inverts the mapping (the same law the rack applies, sign and clamp
// included) and sends {op:"macro"}, the path the rail itself uses.
"use strict";
const fs = require("fs");
const files = {};
function load(k, p) { const raw = fs.readFileSync(p, "utf8"); files[k] = { p, crlf: raw.indexOf("\r\n") >= 0, s: raw.replace(/\r\n/g, "\n"), n: 0 }; }
function edit(k, from, to, count = 1) {
  const f = files[k]; const parts = f.s.split(from);
  if (parts.length - 1 !== count) { console.error("ANCHOR MISS in " + k + " (" + (parts.length - 1) + "): " + from.slice(0, 100)); process.exit(1); }
  f.s = parts.join(to); f.n++;
}
const root = "C:/Users/peter/b/BrokildWorldFX/";
load("ui", root + "ui/bwfx-rack.js");
load("rack", root + "src/bwfx_rack.cpp");
load("probe", root + "test/uiprobe.js");

edit("ui",
`  function depthOf(dest) {
    if (!macros) return 0;
    for (var i = 0; i < macros.length; i++)
      for (var j = 0; j < macros[i].length; j++) if (macros[i][j].d === dest) return macros[i][j].a;
    return 0;
  }
`,
`  function depthOf(dest) {
    if (!macros) return 0;
    for (var i = 0; i < macros.length; i++)
      for (var j = 0; j < macros[i].length; j++) if (macros[i][j].d === dest) return macros[i][j].a;
    return 0;
  }

  /*  A macro OWNS this destination - it maps the control across its range -
     so a hand on the control has nowhere to go except THROUGH the macro:
     invert the mapping and move the macro's host parameter instead. Before
     this, dragging RACK MIX to zero sent a raw mix write that macro 5 put
     straight back on the next audio block: the slider moved, the effects
     stayed (Peter, 1984, 2026-09-23). Returns true when it took the gesture;
     the caller then leaves the base value alone. */
  function driveOwned(dest, value, lo, hi) {
    var m = macroOwning(dest);
    if (m < 0 || !macroVals) return false;
    var depth = depthOf(dest) / 100;
    if (!depth || hi === lo) return false;
    var from = depth >= 0 ? lo : hi;
    var v = (value - from) / (depth * (hi - lo));
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    macroVals[m] = v;
    if (send) send({ op: "macro", i: m, v: v });
    drawMacros();
    drawModValues();
    return true;
  }
`);

edit("ui",
`    mixIn.addEventListener("input", function () {
      state.mix = parseInt(mixIn.value, 10) / 100;
      mixOut.textContent = mixIn.value + " %";
      if (send) send({ op: "mix", v: state.mix });
    });`,
`    mixIn.addEventListener("input", function () {
      var mv = parseInt(mixIn.value, 10) / 100;
      mixOut.textContent = mixIn.value + " %";
      if (driveOwned("mix", mv, 0, 1)) return;      // macro 5 holds it: move the macro
      state.mix = mv;
      if (send) send({ op: "mix", v: state.mix });
    });`);

edit("ui",
`          inp.addEventListener("input", function () {
            var nv = parseFloat(inp.value);
            out.textContent = fmt(pd, nv);
            setParamLocal(id, pd, nv);
            drawModValues();
          });`,
`          inp.addEventListener("input", function () {
            var nv = parseFloat(inp.value);
            if (driveOwned(id + "." + pd.id, nv, pd.lo, pd.hi)) return;   // owned: the macro moves
            out.textContent = fmt(pd, nv);
            setParamLocal(id, pd, nv);
            drawModValues();
          });`);

edit("ui",
`        inp.addEventListener("input", function () {
          var nv = parseInt(inp.value, 10);
          out.textContent = nv + " %";
          ms.pr = nv / 100;
          if (send) send({ op: "presence", m: id, v: ms.pr });`,
`        inp.addEventListener("input", function () {
          var nv = parseInt(inp.value, 10);
          out.textContent = nv + " %";
          if (driveOwned(id + ".pr", nv / 100, 0, 1)) return;             // owned: the macro moves
          ms.pr = nv / 100;
          if (send) send({ op: "presence", m: id, v: ms.pr });`);

edit("ui", `  var VERSION = "1.7.0";`, `  var VERSION = "1.7.1";`);
edit("rack", `const char* Rack::version() { return "1.7.0"; }`, `const char* Rack::version() { return "1.7.1"; }`);

edit("probe",
`CHECK(SRC.indexOf("function packed()") > 0, "no 16/32 compaction on save");`,
`CHECK(SRC.indexOf("function packed()") > 0, "no 16/32 compaction on save");
//  1.7.1: a drag on a macro-owned control goes THROUGH the macro
CHECK(SRC.indexOf("function driveOwned(") > 0, "no driveOwned: an owned control still writes its base");
CHECK(/driveOwned\\("mix", mv, 0, 1\\)/.test(SRC), "RACK MIX does not route through macro 5");
CHECK(/driveOwned\\(id \\+ "\\." \\+ pd\\.id/.test(SRC), "module rows do not route through their macro");
CHECK(/driveOwned\\(id \\+ "\\.pr"/.test(SRC), "presence rows do not route through their macro");`);

for (const k in files) { const f = files[k]; fs.writeFileSync(f.p, f.crlf ? f.s.replace(/\n/g, "\r\n") : f.s); console.log(k + ": " + f.n + " edits"); }
