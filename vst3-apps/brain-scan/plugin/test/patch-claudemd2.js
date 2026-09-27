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
/* the Brain Scan line that flagged the stale collection */
rep("brain scan buglist line",
"Ideas parked in `BUGLIST.md` — decals not delivered; **the Brokild Collection zip still says \"all seven\" and predates both High Tide and Brain Scan.**",
"Ideas parked in `BUGLIST.md` (decals were never delivered and the panel does not need them).");
/* the collection's own entry, next to the FMR section that first built it */
rep("collection entry",
"- **THE BROKILD COLLECTION** (`vst3-apps/collection/`): all seven published plugins in one 61 MB zip (35 MB without the standalones — kept them in). Hairfryer excluded, it is unlisted. **Clone Wars' binary is taken from the CI-built zip**, never a local build — the house rule holds inside a bundle too; dispatch `clone-wars.yml` and `git pull` before assembling.",
"- **THE BROKILD COLLECTION** (`vst3-apps/collection/`): rebuilt 2026-09-04 as **all NINE** — 75 MB, eight instruments and one effect. Hairfryer excluded, it is unlisted.\n  - **Every binary now comes from that plugin's OWN PUBLISHED ZIP, never a local build tree.** That makes the Clone Wars CI-only rule hold by construction for all nine, and guarantees the collection contains the same file the individual download gives you — which `build-collection-zip.ps1` then PROVES by hash after re-extracting the finished archive.\n  - The per-plugin zips disagree about their internal shape (some carry an inner folder, some are flat), so the builder parses no paths: it searches each extraction for the `.vst3` bundle, the `.exe` and the `.pdf` and insists on exactly one of each.\n  - **It went stale twice over before anyone noticed**: the August zip predated the Photo-Synth → Photo Synth rename, so it still shipped `Photo-Synth2.vst3` in a folder called `Photo-Synth 2`, and it predated High Tide and Brain Scan. **A collection is a derived artefact — re-cut it whenever the fleet gains or renames a plugin.**\n  - Both builders live in `BrokildWorldFX/tools/` (`build-collection-zip.ps1`, then `build-collection-page.js`, which takes the download button's size from the zip on disk). The page builder MOVED there from `FullMetalRacket/tools/build-collection.js` on 2026-09-04 and the old copy was deleted — a builder for the whole fleet belongs with the fleet's tooling, and two copies drift.");
if (miss.length){ console.error("MISSED:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched CLAUDE.md");
