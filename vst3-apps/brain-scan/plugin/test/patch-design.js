/*  BRAIN-SCAN-DESIGN.md — correct the two cost claims to what the bench
    actually measures, and add the section the build earned.
    node test/patch-design.js
*/
const fs = require("fs"), path = require("path");
const FILE = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/BRAIN-SCAN-DESIGN.md";
let s = fs.readFileSync(FILE, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(name, from, to){
  from = from.split("\n").join(NL); to = to.split("\n").join(NL);
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}

rep("cost claim 1",
"  B-spline read. 8 voices × 4 unison well under 5 % of a core.",
"  B-spline read. 8 voices × 4 unison measures **11.0 % of one core** — the\n  bench asserts under 15 %. (The design said \"well under 5\"; it was wrong,\n  and this is the measurement.)");
rep("cost claim 2",
"   every specimen bounded and audible; 8 voices + 4 unison < 5 % of a core.**",
"   every specimen bounded and audible; 8 voices × 4 unison — 32 readers, the\n   maximum — under 15 % of a core (measured 11.0 %).**");

rep("append the build section",
"- The brain is the show-piece; SPINE is the instrument's bread.",
`- The brain is the show-piece; SPINE is the instrument's bread.

## 10. What the build taught — 2026-09-04

Every line here is a number the bench, the panel probe or a live CDP session
produced. The engine and the specimens went in first and cleanly (38 checks);
everything below is from the panel and the plug-in shell.

**\`bs::Line\` and \`juce::Line<T>\` are ambiguous, and only the plug-in can
see it.** The engine and its bench never include JUCE, so the clash appeared
for the first time in \`PluginProcessor.cpp\`, which has \`using namespace bs\`
and every JUCE header — twenty errors from one name. Qualify the type at every
use in a file that has both. The same file also called
\`DynamicObject::getProperty\` with a default argument; it takes one.

**A raymarched volume needs a gradient or it is fog, whatever else is tuned.**
Three rounds went into the opacity before the real fault was admitted: there
was no light on the tissue. A central-difference normal plus a headlight and a
rim term turned the specimen from haze into an object, and did more than any
transfer-function change before it.

**A scanned object and a field that fills the cube want OPPOSITE transfer
functions.** A ramp shows a brain whole and turns a waveform volume into an
opaque brick; a band shows the waveform's surface and hides the brain's skull.
So it is a control — SOLID or SURFACE — chosen from the histogram when a
specimen loads (SOLID if more than a third of the cube is air) and overridable
from the panel. The window is auto-set with it, the way a scanner picks a
window per protocol, and both sliders move so the panel shows the truth.

**The opacity was eight times too high, and arithmetic said so.** A ray crosses
the head in about 56 steps, so a ghost that still shows the lines inside it
wants a per-step alpha near 0.016, not 0.13. DENSITY now runs \`0.15 + 6 d²\`,
which measures as transmittance 0.87 / 0.44 / 0.05 across the slider.

**One window serves two views, so it has to suit both.** Pushing the window's
floor down to the 2nd percentile made the gantry read better and blew the SLICE
out — everything above the 58th percentile clipped to white and the cortex
detail went with it. p05 to p85 satisfies both.

**"The head renders too small" was false, and only a measurement settled it.**
Three separate attempts to find a scale bug failed because the first probe ran
with a stale specimen's window (a band correctly makes a *constant* field
invisible — the probe lied before the code did). Rendering a solid volume and
the bare wireframe from the same camera gave identical bounds, fw 0.272 fh
0.394, matching the analytic \`(0.5/2.05)/0.62\`. The geometry was exact
throughout; what was small was the *bright* part, because a squared ramp took
the outer tissue's a = 0.19 down to 0.037.

**A menu's closer must not eat the click it was armed for.** A document-level
\`pointerdown\` closer removes the menu before the item's \`click\` ever fires;
a pointerdown inside the menu has to re-arm instead of closing.

**The panel is proven three ways, and they are not interchangeable.**
\`node --check\` on the extracted script (which cannot see an unclosed tag —
B2311.104's lesson); \`test/uiprobe.js\`, which loads the real page in headless
Chrome and feeds it the processor's own vocabulary, 24 checks including that a
straight line of four collinear points reads straight to five decimals, i.e.
the page evaluates a line exactly as \`Engine::Line::at\` does; and a live CDP
session against the standalone, which is the only one that proves WebView2,
the bridge, the 262 144-texel volume stream and the audio actually work
together.

**Hints are not optional here.** Every parameter carries the gloss the
processor already sends, and the probe fails if any control lacks one — 69
hints, placed beside the control and never over it (High Tide's round, applied
from the start).`);

if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched " + FILE);
