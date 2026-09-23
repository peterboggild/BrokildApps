// Line 230 of CLAUDE.md carries a literal backspace where backslashes belong:
// `C:<BS>Users<BS>peter<BS>b<BS>ArtefactB2311` collapsed to `C:UserspeterArtefactB2311`.
// The documented bash-heredoc backslash bug, in the file that documents it.
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const BS = String.fromCharCode(92);
const lines = s.split(/\r?\n/);
const i = lines.findIndex(l => l.indexOf("C:UserspeterArtefactB2311") >= 0 || /[\u0000-\u0008\u000e-\u001f]/.test(l));
if (i < 0) { console.error("nothing to repair"); process.exit(1); }
const before = lines[i];
lines[i] = "### Artefact B2311.22 VST3 (`C:" + BS + "Users" + BS + "peter" + BS + "b" + BS + "ArtefactB2311`) \u2014 the alien synth";
fs.writeFileSync(p, lines.join(NL));
console.log("repaired line " + (i + 1));
console.log("  was: " + JSON.stringify(before));
console.log("  now: " + JSON.stringify(lines[i]));
