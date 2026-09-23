"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const a = `#viewtip{position:absolute;right:10px;top:8px;max-width:38%;text-align:right;font:italic 10px var(--serif);color:rgba(205,214,220,.45);pointer-events:none}`;
const b = `#viewtip{position:absolute;left:74px;top:12px;max-width:46%;text-align:left;font:italic 10px var(--serif);color:rgba(205,214,220,.45);pointer-events:none}`;
if (s.split(a).length !== 2) { console.log("MISS viewtip"); process.exit(1); }
s = s.split(a).join(b);
fs.writeFileSync(f, s, "utf8");
console.log("patched");
