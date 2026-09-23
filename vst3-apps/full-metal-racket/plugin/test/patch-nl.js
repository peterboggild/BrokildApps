// Route every nonlinear-damping coefficient through nlFor(character, Q), and
// split the waveshaper models off onto their own field. Exact-count anchors,
// collected misses, abort before writing (the house rule: a patch that applies
// eight of twelve leaves a file that is neither old nor new).
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];

const SUBS = [
  ["                v.nl = lerpf (1.2f, 5.5f, tone);",
   "                v.nl = nlFor (0.7f + tone * 1.6f, v.q);"],
  ["                v.nl = tone;                                     // shaper amount",
   "                v.shape = tone;                                  // waveshaper: no resonator here"],
  ["                v.nl = 1.6f + tone * 3.0f;",
   "                v.nl = nlFor (0.9f + tone * 1.7f, v.q);"],
  ["                v.nl = 2.2f;",
   "                v.nl = nlFor (1.1f, v.q);"],
  ["            v.nl = 1.4f + tone * 2.6f;",
   "            v.nl = nlFor (0.8f + tone * 1.5f, v.q);"],
  ["                v.nl = 3.0f;",
   "                v.nl = nlFor (1.4f, v.q);"],
  ["                v.nl = tone;" + NL + "                v.famGain = 2.40f;",
   "                v.shape = tone;" + NL + "                v.famGain = 2.40f;"],
  ["                v.nl = 2.0f;",
   "                v.nl = nlFor (1.0f, v.q);"],
  ["        if (v.nl > 0.0f) { const float g = 1.0f + v.nl * 3.0f;",
   "        if (v.shape > 0.0f) { const float g = 1.0f + v.shape * 3.0f;"],
  ["    v.nl      = 0.0f;",
   "    v.nl      = 0.0f;" + NL + "    v.shape   = 0.0f;"],
  ["sy = symp[(size_t) c].tick (sympExc[(size_t) c], 1.5f);",
   "sy = symp[(size_t) c].tick (sympExc[(size_t) c], sympNl);"],
  ["sy = symp[(size_t) c].tick (0.0f, 1.5f);",
   "sy = symp[(size_t) c].tick (0.0f, sympNl);"],
  ["const float b = body[0].tick (in, 0.8f) * 0.7f + body[1].tick (in, 0.8f) * 0.45f;",
   "const float b = body[0].tick (in, bodyNl) * 0.7f + body[1].tick (in, bodyNl) * 0.45f;"],
  // the two shared resonators get their coefficients the same way
  ["    // the shared shell, retuned per block",
   "    // the shared shell and the sympathetic shells, retuned per block." + NL +
   "    // Their damping goes through nlFor too, for the same reason a voice's does." + NL +
   "    const float bodyNl = nlFor (0.9f, 6.0f);" + NL +
   "    const float sympNl = nlFor (1.2f, 9.0f);"]
];

for (const [a, b] of SUBS) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push("x" + n + "  " + a.trim().slice(0, 64)); continue; }
  s = s.split(a).join(b);
}

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("patched OK (" + SUBS.length + " replacements, nl=" + (NL.length === 2 ? "CRLF" : "LF") + ")");
