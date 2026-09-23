/*  CLAUDE.md — add Brain Scan to the fleet table and record the session.
    node test/patch-claudemd.js
*/
const fs = require("fs");
const FILE = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(FILE, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(name, from, to){
  from = from.split("\n").join(NL); to = to.split("\n").join(NL);
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}

rep("fleet table row",
"| High Tide | `b\\HighTide` | `brokild-high-tide` |",
"| High Tide | `b\\HighTide` | `brokild-high-tide` |\n| **Brain Scan** | `b\\BrainScan` | `brokild-brain-scan` |");

rep("new section",
"### Wave Scaffold — DESIGN ONLY (2026-09-04)",
`### Brain Scan (\`C:\\Users\\peter\\b\\BrainScan\`) — the volume synth, BUILT AND SHIPPED 2026-09-04
- **Store a volume, not a waveform, and read it along a line.** A specimen is a scalar field on a 64³ cube; a scan line is a Catmull-Rom curve through it; one cycle of the waveform is \`w(s) = V(g(s))\`. The filter's cutoff and the modulator are the same thing read slower — two more lines. Design \`BrokildApps/BRAIN-SCAN-DESIGN.md\` (§10 = what the build taught, every line a number). Plugin code **\`BrSc\`**, PRODUCT_NAME "Brain Scan", build id \`BS_BUILD_ID\` (260904.1), Brokild collection folder, patches \`Documents\\Brokild patches\\Brain Scan\`, private repo \`brokild-brain-scan\`, published at \`vst3-apps/brain-scan/\`.
- **THE CLAIM, and it is measured:** SCAN blends the GEOMETRY of two anchor lines and only then reads the field. Two pulses of duty 0.23/0.77 both have a null at the 5th harmonic, so every crossfade of them keeps it (−65.8 dB); the midway PATH reads duty 0.50, whose 5th is −14.0 dB, exactly the formula. Fifty decibels from the same two endpoints. Bench 38 checks; panel probe 24; cost 11.0 % of a core at 8 voices × 4 unison (the design first claimed "well under 5 %" — corrected in the doc to the measurement).
- **\`bs::Line\` vs \`juce::Line<T>\` is invisible to a headless bench.** The engine and its bench never include JUCE; \`PluginProcessor.cpp\` has \`using namespace bs\` AND every JUCE header, so twenty errors came from one name the moment the plug-in was first compiled. Qualify the type in any file that has both. Same file: \`DynamicObject::getProperty\` takes ONE argument.
- **A raymarched volume needs a gradient or it is fog, whatever else you tune.** Three rounds went into opacity before admitting there was no light on the tissue. A central-difference normal + headlight + rim did more than every transfer-function change before it.
- **A scanned object and a field that fills the cube want OPPOSITE transfer functions** — a ramp shows a brain whole and turns a waveform volume into a brick; a band shows the waveform's surface and hides the brain's skull. So it is a control (SOLID / SURFACE), auto-chosen from the histogram on load (SOLID if >⅓ of the cube is air) and overridable, with the window auto-set the way a scanner picks one per protocol. **One window serves the slice AND the gantry, so it has to suit both:** a floor at p02 read well in 3D and blew the slice out; p05..p85 satisfies both.
- **"The head renders too small" was FALSE and only a measurement settled it.** Rendering a solid volume and the bare wireframe from the same camera gave identical bounds (fw 0.272 fh 0.394, matching \`(0.5/2.05)/0.62\`). Geometry was exact all along; the *bright* part was small because a squared ramp took the outer tissue's a = 0.19 to 0.037. **And the first probe lied before the code did** — it ran with a stale specimen's window, and a band correctly makes a CONSTANT field invisible.
- **The panel is proven three ways and they are not interchangeable**: \`node --check\` on the extracted script (cannot see an unclosed tag — .104's lesson), \`test/uiprobe.js\` in headless Chrome fed the processor's own vocabulary (initialState, a synthetic 64³ volume, a lines payload, a view frame — and it asserts a straight line of four collinear points reads straight to 5 dp, i.e. the page evaluates a curve exactly as \`Engine::Line::at\` does), and a live CDP session, the only one that proves WebView2 + the bridge + the 262 144-texel volume stream + audio together.
- **A menu's closer must not eat the click it was armed for**: a document \`pointerdown\` closer removes the menu before the item's \`click\` fires; a pointerdown INSIDE the menu has to re-arm instead of closing.
- Hints on every control (69), placed beside and never over it; the probe fails if a parameter has none. Line points are edited on the tomography slice — drag keeps the depth, click adds at the table's depth, alt-click removes.
- Registered in \`install-fleet.ps1\` and \`check-names.js\`. Reusable in the tree: \`test/uiprobe.js\`, \`tools/cdp.js\`, \`test/plates-jobs.json\`, \`test/make-zip.ps1\` (loads the DLL out of the archive and compares hashes), \`test/make-landing.js\` (the landing page from High Tide's shell with the palette swapped), \`tools/tojpg.ps1\`. Ideas parked in \`BUGLIST.md\` — decals not delivered; **the Brokild Collection zip still says "all seven" and predates both High Tide and Brain Scan.**

### Wave Scaffold — DESIGN ONLY (2026-09-04)`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched CLAUDE.md");
