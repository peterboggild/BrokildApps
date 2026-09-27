/*  ui.html — one window serves two views, so it has to suit both.  With the
    floor pushed down to the 2nd percentile the SLICE blew out: everything
    above the 58th percentile clipped to white and the cortex detail went with
    it.  p05..p85 gives the slice its contrast back and still lets the whole
    head render in the gantry under the gentler ramp.  And the white keys had
    no visible edge at panel scale.  */
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(FILE, "utf8");
const miss = [];
function rep(name, from, to){
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}
rep("window range",
"    const lo = at(0.02), hi = at(0.58);",
"    const lo = at(0.05), hi = at(0.85);");
rep("white key edge",
"  border-right:1px solid #9a948a;box-shadow:inset -2px 0 3px -2px rgba(0,0,0,.35);cursor:pointer}",
"  border-right:1px solid #8c8578;box-shadow:inset -4px 0 5px -3px rgba(0,0,0,.5),inset 0 -3px 4px -2px rgba(0,0,0,.28);cursor:pointer}");
if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched");
