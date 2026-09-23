/*  Probe round 4.

    Check 22 measured the DOM: a control belonging to a view that is not on
    screen is DETACHED, so only the active view's controls were countable and
    it read 85 of 395 on a page where all 395 exist. It asks the page's own
    control map instead - and a SECOND check, which is the one that matters,
    walks every view and asserts every parameter is REACHABLE somewhere.
*/
const fs = require("fs");
const P = "C:/Users/peter/b/ThirtyThousandYears/test/uiprobe.js";
let s = fs.readFileSync(P, "utf8");

const edits = [
  [`      var missing = [], dup = [], nohint = [], autohint = [];
      IDS.forEach(function (id) {
        var all = document.querySelectorAll('.ctl[data-id="' + id + '"]');
        if (!all.length) { missing.push(id); return; }
        if (all.length > 1) dup.push(id + "x" + all.length);
        var h = all[0].getAttribute("data-hint");
        if (!h) nohint.push(id);
        else if (all[0].dataset.hintAuto) autohint.push(id);
      });`,
   `      var missing = [], dup = [], nohint = [], autohint = [];
      IDS.forEach(function (id) {
        /*  Ask the page's map, not the DOM: a control in a view that is not
            showing is detached, because the views MOVE the same node. */
        var c = window.__TTY.ctl(id);
        if (!c) { missing.push(id); return; }
        if (document.querySelectorAll('.ctl[data-id="' + id + '"]').length > 1) dup.push(id);
        var h = c.getAttribute("data-hint");
        if (!h) nohint.push(id);
        else if (c.dataset.hintAuto) autohint.push(id);
      });`],
  [`            ok("every view builds with panels in it", empty.length === 0, empty.join(" | "));`,
   `            ok("every view builds with panels in it", empty.length === 0, empty.join(" | "));
            /*  The check that matters: after walking every view, is there a
                parameter that has a control but no home anywhere? */
            var unreachable = IDS.filter(function (id) { return !SEEN[id]; });
            ok("every parameter is reachable in some view (" + (IDS.length - unreachable.length) + " of " + IDS.length + ")",
               unreachable.length === 0, unreachable.slice(0, 16).join(" "));
            /*  ...and none is in two places at once while a view is open: a
                control is ONE node, so a strip must not lose it to a view. */
            ok("no control was taken from the header, foot or macro strips",
               STOLEN.length === 0, STOLEN.slice(0, 12).join(" "));`],
  [`              return frame().then(function () {
                var page = document.querySelector('.view[data-view="' + v[0] + '"] .page:not([hidden])');
                if (!page) { empty.push(v.join("/") + ":nopage"); return; }
                var pans = page.querySelectorAll("[data-panel]");
                if (!pans.length) empty.push(v.join("/") + ":nopanels");`,
   `              return frame().then(function () {
                var page = document.querySelector('.view[data-view="' + v[0] + '"] .page:not([hidden])');
                if (!page) { empty.push(v.join("/") + ":nopage"); return; }
                var pans = page.querySelectorAll("[data-panel]");
                if (!pans.length) empty.push(v.join("/") + ":nopanels");
                page.querySelectorAll(".ctl").forEach(function (c) { SEEN[c.dataset.id] = 1; });
                document.querySelectorAll("#head .ctl, #foot .ctl, #macros .ctl").forEach(function (c) { SEEN[c.dataset.id] = 1; });
                STRIPIDS.forEach(function (id) {
                  if (!document.querySelector('#head .ctl[data-id="' + id + '"], #foot .ctl[data-id="' + id + '"], #macros .ctl[data-id="' + id + '"]')
                      && STOLEN.indexOf(id) < 0) STOLEN.push(id + "@" + v.join("/"));
                });`],
  [`      var over = [], empty = [];`,
   `      var over = [], empty = [], SEEN = {}, STOLEN = [];
      /*  What the always-visible strips own, read from the page before any
          view is switched: these must never move into a view. */
      var STRIPIDS = Array.prototype.slice.call(
        document.querySelectorAll("#head .ctl, #foot .ctl, #macros .ctl")).map(function (c) { return c.dataset.id; });`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(P, s);
console.log("probe patched (" + edits.length + " edits)");
