/*  One-off patch: the panel's fallback SPECS table and the Voice section learn
    the OVERSAMPLE parameter that landed in the engine after the panel was
    written; the ledger stops colliding with the view tip. Exact-count anchors;
    nothing is written on a miss. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const misses = [];
function rep(a, b, n = 1) {
  const c = s.split(a).length - 1;
  if (c !== n) { misses.push(`expected ${n} of ${JSON.stringify(a.slice(0, 60))}, found ${c}`); return; }
  s = s.split(a).join(b);
}
rep(`  ["ceiling","CEILING",KP.PCT,0.5,"the hard-knee ceiling's threshold"]\n`,
    `  ["ceiling","CEILING",KP.PCT,0.5,"the hard-knee ceiling's threshold"],\n  ["quality","OVERSAMPLE",KP.LIST,1,"how finely the ball's clock is stepped",0,1,["2X","4X"]]\n`);
rep(`["Voice",   ["voiceMode","unison","spread","width","glide","tune","ceiling","level"]],`,
    `["Voice",   ["voiceMode","unison","spread","width","glide","tune","ceiling","level","quality"]],`);
rep(`#viewtip{position:absolute;right:10px;bottom:8px;font:italic 10px var(--serif);color:rgba(205,214,220,.45);pointer-events:none}`,
    `#viewtip{position:absolute;right:10px;top:8px;max-width:38%;text-align:right;font:italic 10px var(--serif);color:rgba(205,214,220,.45);pointer-events:none}`);
if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s, "utf8");
console.log("patched");
