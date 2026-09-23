// Harden the driver: a missing control must FAIL its own check, not throw and
// take the whole report with it (a probe that dies reports nothing, which is
// indistinguishable from a page that never loaded).
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(p, "utf8");
const a = `      sample.forEach(function (id) {
        if (!SPEC[id]) { bad.push(id + ":absent"); return; }
        [0, 0.27, 0.5, 0.81, 1].forEach(function (t) {
          var v = SPEC[id].hi > 1 ? Math.round(t * SPEC[id].hi) : t;
          window.__TTY.set(id, v);
          var shown = ctl(id).querySelector(".val").textContent.trim();
          var want = law(id, window.__TTY.get(id));
          if (shown !== want) bad.push(id + "@" + v + ": page '" + shown + "' vs law '" + want + "'");
        });
        window.__TTY.set(id, SPEC[id].def);
      });`;
const b = `      sample.forEach(function (id) {
        if (!SPEC[id]) { bad.push(id + ":absent"); return; }
        var c = ctl(id);
        if (!c || !c.querySelector(".val")) { bad.push(id + ":no control"); return; }
        [0, 0.27, 0.5, 0.81, 1].forEach(function (t) {
          var v = SPEC[id].hi > 1 ? Math.round(t * SPEC[id].hi) : t;
          window.__TTY.set(id, v);
          var shown = c.querySelector(".val").textContent.trim();
          var want = law(id, window.__TTY.get(id));
          if (shown !== want) bad.push(id + "@" + v + ": page '" + shown + "' vs law '" + want + "'");
        });
        window.__TTY.set(id, SPEC[id].def);
      });`;
if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("driver hardened");
