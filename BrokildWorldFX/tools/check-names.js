/*  One product, one name.

    Every Brokild plugin's patch folder must be named EXACTLY what the DAW
    calls the plugin — its PRODUCT_NAME — because that is the only name a user
    ever sees. When the two drift apart, the user's question is "where did my
    presets go", and the answer is a folder named something they have never
    been shown. Photo-Synth shipped as "Photo-Synth2" and kept its patches in
    "Photo-Synth 2"; nothing else disagreed, which is precisely how a single
    exception survives.

        node tools/check-names.js

    Exits non-zero if any plugin's patchFolder() name differs from its
    PRODUCT_NAME, or if a patch folder exists on disk that no plugin claims.  */
"use strict";
const fs = require("fs");
const path = require("path");

const B = "C:/Users/peter/b";
const DOCS = "C:/Users/peter/Documents/Brokild patches";
const DIRS = ["ArtefactB2311", "ArtefactB2311_67", "BlackRider", "BladeRuiner",
              "CloneWars", "EscapeRoom", "FullMetalRacket", "Hairfryer",
              "MarsWars", "PhotoSynth"];

let bad = 0;
const claimed = new Set();

for (const d of DIRS) {
  const cml = path.join(B, d, "CMakeLists.txt");
  if (!fs.existsSync(cml)) { console.log("  " + d.padEnd(18) + "no CMakeLists"); continue; }
  const pm = fs.readFileSync(cml, "utf8").match(/PRODUCT_NAME\s+"([^"]+)"/);
  const product = pm ? pm[1] : null;

  let folder = null, former = [];
  const srcDir = path.join(B, d, "Source");
  if (fs.existsSync(srcDir)) {
    for (const f of fs.readdirSync(srcDir)) {
      if (!/\.(cpp|h)$/.test(f)) continue;
      const t = fs.readFileSync(path.join(srcDir, f), "utf8");
      const m = t.match(/patchFolder\s*\(\s*"([^"]+)"/);
      if (m && !folder) folder = m[1];
      //  former names live in the 4th argument, as a brace list of strings
      const fm = t.match(/patchFolder\s*\([^;]*?\{\s*("(?:[^"\\]|\\.)*"(?:\s*,\s*"(?:[^"\\]|\\.)*")*)\s*\}\s*\)\s*;/s);
      if (fm && !former.length) {
        const parts = fm[1].match(/"(?:[^"\\]|\\.)*"/g) || [];
        former = parts.map(p => p.slice(1, -1)).filter(p => !p.startsWith("\\\""));
      }
    }
  }
  if (folder) claimed.add(folder);
  for (const f of former) claimed.add(f);

  const ok = product && folder && product === folder;
  if (!ok && folder) bad++;
  console.log("  " + d.padEnd(18)
    + (product || "?").padEnd(22)
    + (folder ? ("patches: " + folder) : "no patchFolder call").padEnd(34)
    + (folder ? (ok ? "ok" : "*** MISMATCH ***") : ""));
}

if (fs.existsSync(DOCS)) {
  const orphans = fs.readdirSync(DOCS, { withFileTypes: true })
    .filter(e => e.isDirectory() && !claimed.has(e.name)).map(e => e.name);
  if (orphans.length) {
    console.log("\n  patch folders on disk that no plugin claims:");
    orphans.forEach(o => console.log("      " + o));
    bad += orphans.length;
  }
}
console.log(bad ? "\n" + bad + " problem(s)" : "\nall names consistent");
process.exit(bad ? 1 : 0);
