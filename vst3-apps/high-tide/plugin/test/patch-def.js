/*  The hint reported the value the patch happened to load with as the
    parameter's "default" — because the page recorded the first value it ever
    saw. The native side sends the real default in initialState now, so use it:
    it is what the hint quotes and what a double-click resets to. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const a = `              def: had && had.def!==undefined ? had.def : +s.v||0 };     // the first value seen is the default`;
const b = `              //  the native side sends the parameter's real default; the first
              //  value seen is only the fallback for the page's own stub table
              def: s.def!==undefined ? +s.def : (had && had.def!==undefined ? had.def : +s.v||0) };`;
if (s.split(a).length !== 2) { console.log("MISS def anchor"); process.exit(1); }
fs.writeFileSync(f, s.split(a).join(b), "utf8");
console.log("defaults come from the engine now");
