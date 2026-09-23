/*  ui.html — the last of the window tuning, and it is worth writing down what
    the measurement said: the head was NOT rendering small.  Its bounds on
    screen matched the cube's to three decimals (fw .272 fh .394, the same
    numbers a solid volume and the wireframe both give).  What was small was
    the BRIGHT part: the scalp and outer cortex sit at the very bottom of the
    window, and a squared ramp takes 0.19 down to 0.037.  So the ramp is
    gentler now and the window's floor sits under the tissue rather than in
    the middle of it.  */
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(FILE, "utf8");
const miss = [];
function rep(name, from, to){
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}
rep("gentler ramp", "    float ramp = a * a;", "    float ramp = a * (0.30 + 0.70 * a);");
rep("window floor",
"    const lo = at(0.10), hi = at(0.60);",
"    const lo = at(0.02), hi = at(0.58);");
rep("closer camera",
"const CAM = { yaw:0.72, pitch:0.42, dist:2.55, spin:true, anchors:true };\nconst HOME = { yaw:0.72, pitch:0.42, dist:2.55 };",
"const CAM = { yaw:0.72, pitch:0.38, dist:2.15, spin:true, anchors:true };\nconst HOME = { yaw:0.72, pitch:0.38, dist:2.15 };");
if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched");
