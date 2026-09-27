/*  Panel round 4.

    The only id left in the spare box was `patch`, and it is not homeless: the
    header presents it as the preset window with its own prev/next buttons and
    the note beneath. It is named as presented-elsewhere now, the way the five
    BWFX macros are.

    The spare box itself is made to scroll. It exists precisely for ids this
    layout has never seen, so it must be able to hold one without pushing its
    column out of shape - the point of a catch-all is that it cannot break.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  [`    countPlaced();
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id]) spare.push([id, PNAME[id] || id]); });`,
   `    countPlaced();
    /*  Presented elsewhere by something that is not a .ctl: the rack fragment
        owns its five macros, and the header's preset window IS the patch dial. */
    var OWNED = { patch: 1 };
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id] && !OWNED[id]) spare.push([id, PNAME[id] || id]); });`],
  [`#more .row{max-height:100%}`,
   `/*  The spare box holds ids this layout has never seen. It scrolls, so one
    cannot push its column out of shape. */
#more .row{max-height:100%;overflow-y:auto;align-content:flex-start}
#more{overflow-y:auto}`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
