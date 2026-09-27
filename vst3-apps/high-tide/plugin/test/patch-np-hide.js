/*  The plate's own lettering is hidden by VISIBILITY, not by a transparent
    colour: a transparent colour leaves the text-SHADOW behind, which drew a
    second, larger HIGH TIDE spilling out under the plate. Visibility keeps the
    box, so the plate still sizes to its own name. */
"use strict";
const fs = require("fs");
const f = "C:/Users/peter/b/HighTide/Source/ui/ui.html";
let s = fs.readFileSync(f, "utf8");
const a = `.dec-nameplate #plate .np{color:transparent}`;
const b = `.dec-nameplate #plate .np{visibility:hidden}`;
if (s.split(a).length - 1 !== 1) { console.log("MISS: the .np rule"); process.exit(1); }
fs.writeFileSync(f, s.split(a).join(b), "utf8");
console.log("the plate's engraved name is the only one now");
