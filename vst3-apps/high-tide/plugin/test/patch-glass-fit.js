/*  The glass brings its own brass ferrules, so the drawn ones underneath make
    a doubled cap at each end. Hide them when the glass is in, and let the
    teal read slightly through the tube rather than painting over it. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const a = `.dec-glass .tube{background-image:var(--decal-glass);background-size:100% 100%;
  background-repeat:no-repeat}`;
const b = `.dec-glass .tube{background-image:var(--decal-glass);background-size:100% 100%;
  background-repeat:no-repeat}
.dec-glass .tube::before,.dec-glass .tube::after{display:none}
.dec-glass .tube .fill{opacity:.88}`;
if (s.split(a).length - 1 !== 1) { console.log("MISS: the .dec-glass rule"); process.exit(1); }
fs.writeFileSync(f, s.split(a).join(b), "utf8");
console.log("the glass wears its own ferrules");
