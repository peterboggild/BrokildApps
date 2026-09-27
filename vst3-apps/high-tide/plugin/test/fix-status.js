"use strict";
const fs = require("fs");
const f = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/HIGH-TIDE-DESIGN.md";
let s = fs.readFileSync(f, "utf8");
const a = "123 bench checks ALL CLEAR, panel probe 12/12,";
const b = "283 bench checks ALL CLEAR, panel probe 14/14,";
if (s.split(a).length !== 2) { console.log("MISS"); process.exit(1); }
fs.writeFileSync(f, s.split(a).join(b), "utf8");
console.log("status line corrected");
