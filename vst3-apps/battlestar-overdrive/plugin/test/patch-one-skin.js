// 260926.2: the Space Panther panel is the ONLY panel, and the on-screen skin
// prompt goes. The panther art takes over art/panel.jpg + art/screen-glass.jpg
// (done by tools/ingest-skin2.ps1 -AsOnly); this removes the switching code.
// Every cut is validated first; nothing is written on a miss.
const fs = require("fs");
const f = process.argv[2];
let s = fs.readFileSync(f, "utf8");
const nl = (s.match(/\r\n/g) || []).length > (s.match(/\n/g) || []).length / 2 ? "\r\n" : "\n";
s = s.replace(/\r\n/g, "\n");
const miss = [];

function cutBetween(startMark, endMark, keepEnd, label) {
  const a = s.indexOf(startMark);
  const b = a < 0 ? -1 : s.indexOf(endMark, a + startMark.length);
  if (a < 0 || b < 0 || s.indexOf(startMark, a + 1) >= 0) { miss.push(label); return; }
  s = s.slice(0, a) + (keepEnd ? "" : "") + s.slice(keepEnd ? b : b + endMark.length);
}
function replaceOnce(from, to, label) {
  const n = s.split(from).length - 1;
  if (n !== 1) { miss.push(label + " (" + n + ")"); return; }
  s = s.replace(from, to);
}

// CSS: the skin-2 block
cutBetween("\n  /* ---- panel skins", 'screen-glass-2.jpg"); }\n', false, "css block");
// screen no longer a button
replaceOnce("#screenWrap{ position:absolute; overflow:hidden; cursor:pointer; touch-action:none; }",
            "#screenWrap{ position:absolute; overflow:hidden; }", "screenWrap css");
// debug hook: panel/prompt fields and prompt helpers (init stays)
replaceOnce("                    panel:panel, prompt:{ open:prompt.open, boxes:prompt.boxes.length },\n", "", "state fields");
replaceOnce("  openPrompt(){ openPrompt(); }, pickPanel(n){ pickPanel(n); },\n", "", "prompt hooks");
cutBetween("  /* where each number of the prompt sits", "  fuelFrame(){", true, "promptTargets");
// the whole switching + prompt section, up to the ghost section
cutBetween("/* ======================================================================\n   panel skins - picked on the screen itself",
           "/* ======================================================================\n   the ghost transmission", true, "js section");
// the draw call and the init lines
replaceOnce("\n  /* --- the skin prompt, over everything --------------------------- */\n  drawPrompt(now, w, h);\n", "\n", "draw call");
replaceOnce("  NUM_PANELS = d.panels || 2;\n  applyPanel(d.panel || 1, false);\n", "", "init lines");

for (const w of ["prompt", "applyPanel", "panel-2", "NUM_PANELS"])
  if (s.indexOf(w) >= 0 && miss.length === 0) miss.push("leftover '" + w + "' at " + s.indexOf(w) + ": " + s.substr(s.indexOf(w) - 40, 90));

if (miss.length) { console.log("ABORTED, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(f, s.replace(/\n/g, nl), "utf8");
console.log("written");
