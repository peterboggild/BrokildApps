// Repair three string literals split by a shell-eaten backslash-n (the heredoc rule).
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/test/probe.cpp";
let s = fs.readFileSync(path, "utf8");
const NL = String.fromCharCode(92) + "n";   // a real backslash followed by n, spliced without typing one
const fixes = [
  ['below the fundamental\n", factor,', 'below the fundamental' + NL + '", factor,'],
  ['rms %.4f\n", i / 48000.0,', 'rms %.4f' + NL + '", i / 48000.0,'],
  ['Hz apart\n", e.voices[0].mass.o1[0].inc,', 'Hz apart' + NL + '", e.voices[0].mass.o1[0].inc,'],
];
let miss = [];
for (const [a] of fixes) if (s.split(a).length !== 2) miss.push(a.slice(0, 40));
if (miss.length) { console.log("MISS: " + miss.join(" | ")); process.exit(1); }
for (const [a, b] of fixes) s = s.replace(a, b);
fs.writeFileSync(path, s);
console.log("probe repaired");
