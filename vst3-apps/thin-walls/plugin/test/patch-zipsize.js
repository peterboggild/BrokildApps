/*  The download size, MEASURED from the file that was just cut - never typed,
    and idempotent, so it can be re-run after the zip is cut again. Both
    download buttons carry it. */
const fs = require("fs");
const P = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/index.html";
const Z = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/thin-walls/Thin-Walls-VST3-win64.zip";
if (!fs.existsSync(Z)) { console.error("NO ZIP - cut it first"); process.exit(1); }
const mb = (fs.statSync(Z).size / 1048576).toFixed(1) + " MB";

let s = fs.readFileSync(P, "utf8");

/*  First run: the buttons carry no size and the hero one still has the note
    saying where the size comes from. Later runs: replace whatever size is
    already there. Both paths are checked, and the count is asserted. */
s = s.replace(
  "      <!-- The size goes in when the zip is cut, measured from the file. -->\n", "");
s = s.replace(
  "Download for Windows <small>VST3 + standalone</small>",
  "Download for Windows <small>VST3 + standalone &middot; " + mb + "</small>");
s = s.replace(
  "Thin-Walls-VST3-win64.zip <small>VST3 + standalone</small>",
  "Thin-Walls-VST3-win64.zip <small>VST3 + standalone &middot; " + mb + "</small>");
s = s.replace(/VST3 \+ standalone &middot; [0-9.]+ MB/g,
              "VST3 + standalone &middot; " + mb);

const n = s.split("VST3 + standalone &middot; " + mb).length - 1;
if (n !== 2) { console.error("expected both buttons to carry the size, found " + n); process.exit(1); }
fs.writeFileSync(P, s);
console.log("download size written to both buttons: " + mb);
