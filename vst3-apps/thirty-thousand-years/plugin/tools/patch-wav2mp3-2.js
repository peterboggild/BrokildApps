// The demo files are NN-name.wav; the regex has to be written where the shell cannot eat it.
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/tools/wav2mp3.js";
let s = fs.readFileSync(path, "utf8");
const BS = String.fromCharCode(92);
const a = "const files = fs.readdirSync(inDir).filter(f => /^dd-.*.wav$/.test(f)).sort();";
if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, "const files = fs.readdirSync(inDir).filter(f => /^" + BS + "d" + BS + "d-.*" + BS + ".wav$/.test(f)).sort();");
fs.writeFileSync(path, s);
console.log("filter fixed: " + s.split("\n")[63]);
