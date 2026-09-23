/*  Probe round 5. `patch` has no .ctl in any view because the header presents
    it as the preset window. Exempting it silently would hide the day it stops
    being presented at all, so the probe PROVES it instead: the named element
    must exist and must show the preset the page was told about.
*/
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(P, "utf8");

const a = `            var unreachable = IDS.filter(function (id) { return !SEEN[id]; });
            ok("every parameter is reachable in some view (" + (IDS.length - unreachable.length) + " of " + IDS.length + ")",
               unreachable.length === 0, unreachable.slice(0, 16).join(" "));`;
const b = `            /*  patch is presented by the header's preset window rather than a
                knob; anything else without a home is a fault. */
            var PRESENTED = { patch: "#pname" };
            var unreachable = IDS.filter(function (id) { return !SEEN[id] && !PRESENTED[id]; });
            ok("every parameter is reachable in some view (" + (IDS.length - unreachable.length) + " of " + IDS.length + ")",
               unreachable.length === 0, unreachable.slice(0, 16).join(" "));
            var notShown = Object.keys(PRESENTED).filter(function (id) {
              var e = document.querySelector(PRESENTED[id]);
              return !e || !e.textContent.trim() || e.textContent.trim() === "\\u2014";
            });
            ok("...and the ones presented another way are on screen (patch: the preset window)",
               notShown.length === 0, notShown.join(" "));`;

if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);

/*  The preset window is only filled by a patchinfo event, so feed one before
    the view walk - a check that depends on state must establish it itself. */
const a2 = `      var over = [], empty = [], SEEN = {}, STOLEN = [];`;
const b2 = `      feed("patchinfo", { i: 15, name: "THIRTY THOUSAND YEARS", cat: "EVOLVING WORLD", note: "The flagship journey." });
      var over = [], empty = [], SEEN = {}, STOLEN = [];`;
if (s.split(a2).length !== 2) { console.log("ANCHOR MISS 2"); process.exit(1); }
s = s.replace(a2, b2);

fs.writeFileSync(P, s);
console.log("probe patched");
