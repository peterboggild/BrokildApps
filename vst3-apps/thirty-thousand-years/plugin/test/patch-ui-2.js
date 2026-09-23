/*  Panel round 2 — one real fault and one overflow, both found by the probe.

    1. PLACED was computed inside buildStage(), which runs BEFORE the header,
       foot and macro strips are built. So the 22 ids those strips own - VOLUME,
       DRONE, the eight macros, the patch dial - counted as unplaced and were
       ALSO listed by the MORE strip. Since a control is one node that the views
       MOVE, opening the MIX view would have picked the DRONE switch, the volume
       fader and every macro knob out of the foot and dropped them in a spare
       box. Nothing would have looked broken until someone changed view.
       PLACED is now computed after every panel exists, in applyInitial.

    2. The HISTORY view's TRAVEL panel was 234 px of content in a 222 px box.
       The timeline canvas is the part that can give: 54 -> 38 px, and its
       margin with it.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  // 1. PLACED after everything exists
  [`  PANELS.forEach(function (p) { p._want.forEach(function (w) { PLACED[w[0]] = 1; }); });
}`,
   `}
/*  Which ids have a home. It has to be counted after EVERY panel exists -
    the views, the header, the foot and the macros - or the strips' own
    controls look unplaced and the spare box takes them away from them. */
function countPlaced() {
  PLACED = {};
  PANELS.forEach(function (p) { (p._want || []).forEach(function (w) { PLACED[w[0]] = 1; }); });
}`],
  [`    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id]) spare.push([id, PNAME[id] || id]); });`,
   `    countPlaced();
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id]) spare.push([id, PNAME[id] || id]); });`],
  // 2. the travel canvas
  [`  HTL = el("canvas", "scr", p._row.parentNode); HTL.width = 660; HTL.height = 54;
  HTL.style.cssText = "width:100%;height:54px;margin-top:6px";`,
   `  HTL = el("canvas", "scr", p._row.parentNode); HTL.width = 660; HTL.height = 38;
  HTL.style.cssText = "width:100%;height:38px;margin-top:4px";`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
