const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(FILE, "utf8");
const from = "  lineAt, blendAt, shownScan, get gl(){ return GL.gl; }, GL,";
const to = "  lineAt, blendAt, shownScan, get gl(){ return GL.gl; }, GL,\n"
  + "  setParam, selectLine, setPlane: i => { TOMO.plane = i; TOMO.dirty = true;\n"
  + "    $$(\"#planeTabs .tab\").forEach(b => b.classList.toggle(\"on\", +b.dataset.plane === i)); buildTableSlider(); },\n"
  + "  fitInfo: () => ({ scale: DECK.scale, w: innerWidth, h: innerHeight }),";
if (s.split(from).length - 1 !== 1){ console.error("anchor not unique"); process.exit(1); }
fs.writeFileSync(FILE, s.split(from).join(to));
console.log("patched");
