/*  check-names.js scans trees under b\ for a CMakeLists, so the two plugins whose
    source lives in the WEBSITE repo - Clone Wars and Rite of Passage - are
    invisible to it, and their patch folders were reported as unclaimed. That is
    not a naming fault, it is the checker not knowing where to look.

    DIRS becomes a list of entries that may carry their own path. Every existing
    entry stays a bare string and behaves exactly as before.
*/
const fs = require("fs");
const p = "C:/Users/peter/b/BrokildWorldFX/tools/check-names.js";
let s = fs.readFileSync(p, "utf8");
const misses = [];
const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };

rep(`const DIRS = ["ArtefactB2311_1", "ArtefactB2311", "ArtefactB2311_67", "ArtefactB2311_104",
              "BlackRider", "BladeRuiner",
              "CloneWars", "EscapeRoom", "FullMetalRacket", "Hairfryer",
              "MarsWars", "PhotoSynth", "HighTide", "BrainScan", "ThinWalls"];`,
`/*  A bare string is a tree under b\\. An object carries its own root, for the two
    plugins whose source lives in the WEBSITE repo instead - without them the
    checker cannot read their PRODUCT_NAME and calls their patch folders
    unclaimed, which is the checker not knowing where to look rather than a
    naming fault. */
const SITE = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps";
const DIRS = ["ArtefactB2311_1", "ArtefactB2311", "ArtefactB2311_67", "ArtefactB2311_104",
              "BlackRider", "BladeRuiner",
              "CloneWars", "EscapeRoom", "FullMetalRacket", "Hairfryer",
              "MarsWars", "PhotoSynth", "HighTide", "BrainScan", "ThinWalls",
              { name: "CloneWars(site)",    root: SITE + "/vst3-apps/clone-wars/plugin" },
              { name: "Legion",             root: SITE + "/vocal-harmonizer" },
              { name: "RiteOfPassage",      root: SITE + "/rite-of-passage" }];`);

rep(`for (const d of DIRS) {
  const cml = path.join(B, d, "CMakeLists.txt");
  if (!fs.existsSync(cml)) { console.log("  " + d.padEnd(18) + "no CMakeLists"); continue; }`,
`for (const entry of DIRS) {
  const d = typeof entry === "string" ? entry : entry.name;
  const root = typeof entry === "string" ? path.join(B, d) : entry.root;
  const cml = path.join(root, "CMakeLists.txt");
  if (!fs.existsSync(cml)) { console.log("  " + d.padEnd(18) + "no CMakeLists"); continue; }`);

rep(`  const srcDir = path.join(B, d, "Source");`, `  const srcDir = path.join(root, "Source");`);

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
fs.writeFileSync(p, s);
console.log("check-names can now see the plugins whose source lives in the website repo");
