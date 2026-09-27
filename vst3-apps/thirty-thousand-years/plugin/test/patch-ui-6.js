/*  Panel round 6. Sizing the canvas from its wrapper created a feedback loop:
    a canvas at width/height 100 % inside a wrapper whose height comes from its
    content grows the wrapper, which grows the canvas, and the record reached
    508 x 508 inside a 166 px panel.

    The canvas is taken OUT of layout (absolute, inset 0) so the wrapper's size
    is decided by the panel alone, and the record panel is made a flex column
    so the wrapper's flex:1 means something.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");

const edits = [
  [`#rec{display:block;width:100%;height:100%;image-rendering:pixelated}
.cvwrap{flex:1;min-width:0;position:relative}`,
   `/*  The canvas is absolutely placed so it can never size its own wrapper:
    sizing the bitmap from the wrapper while the wrapper sized itself from the
    bitmap ran the record up to 508 x 508 in a 166 px panel. */
.cvwrap{flex:1;min-width:0;min-height:0;position:relative;overflow:hidden}
.cvwrap canvas{position:absolute;left:0;top:0;width:100%;height:100%;display:block}`],
  [`SP.rec = function (p) { p._recwrap = el("div", "cvwrap", p); p._recwrap.style.flex = "1"; RECW.push(p._recwrap); };`,
   `SP.rec = function (p) {
  p.style.display = "flex"; p.style.flexDirection = "column";
  p._row.style.flex = "none";
  p._recwrap = el("div", "cvwrap", p);
  RECW.push(p._recwrap);
};`],
  [`    var cv = RECC[i], ww = w.clientWidth, hh = w.clientHeight;`,
   `    var cv = RECC[i], ww = w.clientWidth, hh = w.clientHeight;
    if (ww < 8 || hh < 8) return;`],
];

let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n !== 1) miss.push(n + "x: " + a.slice(0, 60).replace(/\n/g, " ")); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("ui.html patched (" + edits.length + " edits)");
