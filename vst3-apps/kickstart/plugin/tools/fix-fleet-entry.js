// one-off: repair the Kickstart entry in install-fleet.ps1, whose backslashes
// a shell ate (and turned "\b" into a real backspace)
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BrokildWorldFX/tools/install-fleet.ps1";
const BS = String.fromCharCode(92);
let s = fs.readFileSync(p, "utf8");
const lines = s.split("\n");
let fixed = 0;
for (let i = 0; i < lines.length; ++i) {
    if (lines[i].includes("# Kickstart: source in the website repo")) {
        lines[i] = "  # Kickstart: source in the website repo (vst3-apps" + BS + "kickstart" + BS + "plugin), built outside Dropbox" + (lines[i].endsWith("\r") ? "\r" : "");
        ++fixed;
    }
    if (lines[i].includes("build = \"C:") && lines[i].includes("Kickstart")) {
        lines[i] = "     build = \"C:" + BS + "Users" + BS + "peter" + BS + "b" + BS + "_build" + BS + "Kickstart" + BS + "plugin\" }" + (lines[i].endsWith("\r") ? "\r" : "");
        ++fixed;
    }
}
if (fixed !== 2) { console.log("ABORT, fixed", fixed); process.exit(1); }
fs.writeFileSync(p, lines.join("\n"));
console.log("ok");
