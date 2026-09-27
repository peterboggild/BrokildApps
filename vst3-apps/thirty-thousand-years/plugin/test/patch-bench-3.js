// Repair the one string literal split by a shell-eaten backslash-n in dumpParams.
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/test/bench.cpp";
let s = fs.readFileSync(path, "utf8");
const NL = String.fromCharCode(92) + "n";
const a = 'HUMANITY\\"]}\n");';
if (s.split(a).length !== 2) { console.log("MISS"); process.exit(1); }
s = s.replace(a, 'HUMANITY\\"]}' + NL + '");');
fs.writeFileSync(path, s);
console.log("repaired");
