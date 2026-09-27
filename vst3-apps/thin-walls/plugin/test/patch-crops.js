// Replace the $CROPS table in tools/crop-plates.ps1 with the rectangles the
// "rects" job measured in the window the plates were actually shot in
// (1400 x 900, the panel's own size). Exact anchors; nothing is written on a
// miss, because a crop guessed from an older layout still produces a JPEG.
const fs = require("fs");
const p = "C:/Users/peter/b/ThinWalls/tools/crop-plates.ps1";
let s = fs.readFileSync(p, "utf8");

const start = "$CROPS = @{";
const end = "}\n";
const i = s.indexOf(start);
if (i < 0) { console.error("ANCHOR MISS: $CROPS = @{"); process.exit(1); }
const j = s.indexOf("\n}", i);
if (j < 0) { console.error("ANCHOR MISS: end of the hashtable"); process.exit(1); }

const table = [
  "$CROPS = @{",
  "  'panel'          = @(0,      0, 1400, 900, 1)",
  "  'pov'            = @(0,     42,  812, 424, 2)",
  "  'pov-speaker'    = @(0,     42,  812, 424, 2)",
  "  'pov-pure'       = @(0,     42,  812, 424, 2)",
  "  'pov-handle'     = @(0,     42,  812, 424, 2)",
  "  'plan'           = @(812,   42,  588, 424, 2)",
  "  'plan-labels'    = @(812,   42,  588, 424, 2)",
  "  'plan-open'      = @(812,   42,  588, 424, 2)",
  "  'plan-shut'      = @(812,   42,  588, 424, 2)",
  "  'plan-folded'    = @(812,   42,  588, 424, 2)",
  "  'ctrl-materials' = @(245,  476,  449, 211, 3)",
  "  'ctrl-folds'     = @(706,  476,  449, 211, 3)",
  "  'ctrl-source'    = @(1167, 476,  219, 211, 3)",
  "  'ctrl-levels'    = @(14,   696,  449, 173, 3)",
  "  'ctrl-wav'       = @(937,  696,  449, 173, 3)",
  "  'hud'            = @(12,    54,  159, 118, 4)",
  "  'status'         = @(0,    877, 1400,  23, 2)"
].join("\n");

s = s.slice(0, i) + table + s.slice(j);

// and the comment above it, which still described the old two-row layout
const oldNote = "# Defaults are the layout at the editor's own 1400 x 900: a 42 px header, a\n# 229 px control row and a 23 px status strip, with the views taking the rest\n# and splitting 58 / 42. Overwrite them from the \"rects\" job if they disagree.";
if (s.indexOf(oldNote) >= 0) {
  s = s.replace(oldNote,
    "# These are the layout at the editor's own 1400 x 900, measured 2026-09-22:\n" +
    "# a 42 px header, the two views over 424 px splitting 812 / 588, then TWO\n" +
    "# rows of control groups at y 476 and y 696, and a 23 px status strip.\n" +
    "# Overwrite them from the \"rects\" job whenever the panel's layout moves.");
} else {
  console.error("NOTE: the old layout comment was not found - left as it was");
}

fs.writeFileSync(p, s);
console.log("crop-plates.ps1 updated with 17 rectangles");
