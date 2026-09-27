/*  Map the fuel gauge by MEASURED fill instead of by frame index.
 *  Exact-count anchors; nothing is written if any of them misses. */
const fs = require("fs");
const path = "C:/Users/peter/b/BattlestarOverdrive/Source/ui/ui.html";
let s = fs.readFileSync(path, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];

function rep(find, sub) {
  const f = find.join(NL);
  const n = s.split(f).length - 1;
  if (n !== 1) { misses.push(n + " matches for: " + find[0].trim().slice(0, 60)); return; }
  s = s.replace(f, sub.join(NL));
}

rep([
  "function paintFuel(){",
  "  /* 15 frames, index 0 = full .. 14 = empty */",
  "  const src = meter.fuel;",
  "  const f = Math.max(0, Math.min(1, src));",
  "  const i = Math.max(0, Math.min(14, Math.round((1 - f) * 14)));",
  "  document.getElementById(\"fuel\").style.backgroundPosition = (i / 14 * 100) + \"% 0\";",
  "}"
], [
  "/*  How full each of the 15 frames actually is, MEASURED off the delivered art",
  "    by tools/measure-fuel-levels.ps1 - regenerate it there, never retype it.",
  "",
  "    They are not evenly spaced: they were drawn by eye, and the step between",
  "    neighbours ranges from 0.010 to 0.114 - an elevenfold spread. Advancing",
  "    them at a fixed rate therefore makes the liquid lurch: it barely moves for",
  "    the first three frames and then drops a tenth of the tube in one go.",
  "    Choosing the frame whose measured fill is NEAREST the tank level makes the",
  "    liquid fall evenly instead, which is what the eye is actually judging. */",
  "const FUEL_FILL = [0.998, 0.988, 0.947, 0.845, 0.771, 0.667, 0.582, 0.531,",
  "                   0.418, 0.341, 0.307, 0.237, 0.143, 0.053, 0.000];",
  "",
  "function paintFuel(){",
  "  const f = Math.max(0, Math.min(1, meter.fuel));",
  "  let best = 0, bestD = 1e9;",
  "  for (let i = 0; i < FUEL_FILL.length; ++i){",
  "    const d = Math.abs(FUEL_FILL[i] - f);",
  "    if (d < bestD){ bestD = d; best = i; }",
  "  }",
  "  document.getElementById(\"fuel\").style.backgroundPosition = (best / 14 * 100) + \"% 0\";",
  "}"
]);

if (misses.length) {
  console.error("ABORTED, nothing written:");
  for (const m of misses) console.error("  " + m);
  process.exit(1);
}
fs.writeFileSync(path, s);
console.log("patched paintFuel to map by measured fill");
