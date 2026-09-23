// repair string literals that a shell heredoc split with real newlines
const fs = require("fs");
const p = require("path").join(__dirname, "probe.cpp");
let s = fs.readFileSync(p, "utf8");
const NL = String.fromCharCode(10);
const BS = String.fromCharCode(92);
let n = 0;
s = s.replace('inGain %.3f' + NL + '", MATERIAL_NAMES[m]', () => { n++; return 'inGain %.3f' + BS + 'n", MATERIAL_NAMES[m]'; });
s = s.replace('std::printf ("' + NL + '");', () => { n++; return 'std::printf ("' + BS + 'n");'; });
if (n !== 2) { console.error("expected 2 repairs, made " + n); process.exit(1); }
fs.writeFileSync(p, s);
console.log("repaired 2 literals");
