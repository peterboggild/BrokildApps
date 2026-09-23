/*  Repair the refill/skin handlers, which a line-range edit computed against a
 *  STALE line numbering merged into one another. Verifies the exact bounds
 *  before writing rather than trusting the numbers again. */
const fs = require("fs");
const p = "C:/Users/peter/b/BattlestarOverdrive/Source/ui/ui.html";
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const lines = s.split(NL);

const first = lines.findIndex(l => l.indexOf('getElementById("refill").addEventListener') >= 0);
if (first < 0) { console.error("refill handler not found"); process.exit(1); }
let last = -1;
for (let i = first; i < Math.min(lines.length, first + 20); ++i) {
  if (lines[i].indexOf('NB.send({ k:"skin"') >= 0) { last = i + 1; break; }
}
if (last < 0 || lines[last].trim() !== "});") {
  console.error("did not find the merged block's end; refusing to write");
  process.exit(1);
}
console.log("repairing lines " + (first + 1) + ".." + (last + 1));

const fixed = [
'document.getElementById("refill").addEventListener("click", () => {',
'  // No explanation. The panel says nothing it does not have to - the lamp IS',
'  // the indication, and the gauge says the rest.',
'  setParam("autorefill", (V.autorefill || 0) > 0.5 ? 0 : 1, true);',
'});',
'',
'document.getElementById("skinBtn").addEventListener("click", () => {',
'  skin = (skin === "chrome") ? "orange" : "chrome";',
'  document.body.classList.toggle("skin-orange", skin === "orange");',
'  NB.send({ k:"skin", v:skin });',
'});'
];

lines.splice(first, last - first + 1, ...fixed);
fs.writeFileSync(p, lines.join(NL));
console.log("handlers repaired");
