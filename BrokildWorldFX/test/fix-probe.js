/*  Repair the two string literals my own inline node -e broke: "\\n" inside a
    JS template literal became a REAL newline inside a C++ string. The bash /
    escaping rule, for the third time today. */
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/test/tempoprobe.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BS = String.fromCharCode(92);      // a real backslash

const fixes = [
  ['    std::printf ("' + NL + '=== 2b. THE OTHER SYNCED MODULES, same change ===' + NL + '");',
   '    std::printf ("' + BS + 'n=== 2b. THE OTHER SYNCED MODULES, same change ===' + BS + 'n");'],
  ['            if (t < 0) { std::printf ("   %s: not found' + NL + '", id); continue; }',
   '            if (t < 0) { std::printf ("   %s: not found' + BS + 'n", id); continue; }'],
  ['            std::printf ("   %-14s settled step %.4f, around the change %.4f  -> %s' + NL + '",',
   '            std::printf ("   %-14s settled step %.4f, around the change %.4f  -> %s' + BS + 'n",']
];
let n = 0;
for (const [a, b] of fixes) {
  const c = s.split(a).length - 1;
  if (c === 1) { s = s.split(a).join(b); n++; }
}
fs.writeFileSync(P, s);
console.log("repaired " + n + " of " + fixes.length + " broken literals");
