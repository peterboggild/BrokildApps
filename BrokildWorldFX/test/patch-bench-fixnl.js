/*  Repair one printf whose \n was eaten by bash quoting, and drop the stray
    %% that only belongs inside a CHECK's format string.

    Written as a FILE, which is the standing rule for anything containing
    backslashes or quotes — bash evaluates them before node ever sees them,
    and it has now cost time in this session twice.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/test/bench.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BS = String.fromCharCode(92);
const miss = [];
function rep(a, b, tag) {
  const A = a.split(String.fromCharCode(10)).join(NL);
  const B = b.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

rep('        std::printf ("   clamp: macro-driven vs set-to-top, settled %.2e\n", dTop);',
    '        std::printf ("   clamp: macro-driven vs set-to-top, settled %.2e' + BS + 'n", dTop);',
    "printf newline");

//  these two are ordinary comments, not format strings
rep("        echoRack (50.0f,  1.0f, 1.0f, up);          // +100 %% -> clamps at the top",
    "        echoRack (50.0f,  1.0f, 1.0f, up);          // +100 % -> clamps at the top", "comment a");
rep("        echoRack (50.0f, -1.0f, 1.0f, down);        // -100 %% -> the bottom",
    "        echoRack (50.0f, -1.0f, 1.0f, down);        // -100 % -> the bottom", "comment b");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("repaired");
