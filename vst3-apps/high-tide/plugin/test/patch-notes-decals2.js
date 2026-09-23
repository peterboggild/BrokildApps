/*  Record the second decal round. */
"use strict";
const fs = require("fs");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [a0, b0] of pairs) {
    const a = a0.split("\n").join(NL), b = b0.split("\n").join(NL);
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split(/[\\/]/).pop() + ": expected 1 of " + JSON.stringify(a0.slice(0, 56)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const B = "C:/Users/peter/b/HighTide/BUGLIST.md";
const D = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/HIGH-TIDE-DESIGN.md";
const C = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";

const b = edit(B, [
  [`### 3. Four decals to redo (sent 2026-09-04)
Order written for ChatGPT at \`BrokildApps/assets/high-tide-decals/REDO.md\`.
\`ht-ground\`, \`ht-paper\`, \`ht-nameplate\` and \`ht-rose\` came back letterboxed —
the drawing pasted into a band of a larger black canvas — and the two textures
have no grain in them (luminance sd 2.6 and 3.3 of 255). The nameplate's
lettering is cut in half by the plate's own bottom edge in the delivered file.
When they arrive: \`tools/ingest-decals.ps1\` takes them (add the part back to
its \`$WANT\` table), then point the matching \`--decal-*\` hook at the file. The
hooks are already in the panel: ground on \`#deck\`, paper on \`#tl\`, nameplate
on \`#plate\`, and the rose wants a new home (a faint ornament in a corner of
the terrain view was the intention).

\`ht-knob\` is perfect and has NO home: this panel has no rotary control, and
the one round thing it has — the TUNE LOCK switch dot — uses its colour to say
whether it is on, which a brass cap would hide. Keep it for a future revision.`,
`### 3. One decal still to fix: \`ht-nameplate\` (order sent 2026-09-04)
\`BrokildApps/assets/high-tide-decals/REDO.md\` now asks for that one part only.
Both deliveries drew the plate as a thin bar while the lettering kept its full
size, so the plate's own bottom edge cuts through the capitals — a composition
fault no crop can undo. The hook is ready: \`--decal-nameplate\` on \`#plate\`,
plus the two rules that hide the drawn name (kept in the patch script
\`test/patch-decals-fix.js\` if they need putting back).

\`ht-knob\` is perfect and has NO home: this panel has no rotary control, and
the one round thing it has — the TUNE LOCK switch dot — uses its colour to say
whether it is on, which a brass cap would hide. Keep it for a future revision.
\`ht-cover\` is a moonlit panorama band rather than a cover; it would suit a
header strip on the manual's first page, which nothing needs today.`],
  [`- **260904.3 — six decals into the panel.**`,
`- **260904.4 — three more decals, rescued rather than re-ordered.** The redo
  came back letterboxed exactly as before, but the DRAWINGS were good this
  time (the ground's luminance variation went 1 % -> 8 % of full scale), so the
  ingest takes them from this side: **ground** and **paper** are strips
  mirrored top-to-bottom into tiles that repeat without a seam — the
  chart-table surface and the paper under the timeline — and the **rose** is
  the top half of a circular rose, used rising out of the corner of the terrain
  view where only its upper quadrant is in frame. Dark ink on a dark panel is
  invisible, so the rose is filled THROUGH ITS OWN ALPHA with pale brass
  (\`tintStencil\`). Eleven of twelve parts are now in.

- **260904.3 — six decals into the panel.**`]
]);

const d = edit(D, [
  [`*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.3,`,
   `*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.4,`],
  [`Four came back letterboxed and one has no home; the redo order is
\`assets/high-tide-decals/REDO.md\`. Three lessons worth keeping:`,
`A second delivery fixed the drawings but not the framing — the same four
arrived letterboxed again — so three of them were taken from this side instead
of asking a third time: the ground and paper strips are mirrored top-to-bottom
into tiles that repeat without a seam, and the rose (the top half of a circle,
cleanly cropped rather than squashed) rises out of a corner where only its
upper quadrant is in frame. Dark ink on a dark panel cannot be seen, so the
rose is filled through its own alpha with pale brass. Eleven of twelve parts
are in; only the nameplate still needs redrawing, and \`ht-knob\` has no home.
Four lessons worth keeping:`],
  [`- **Find every consumer of a variable before redefining it.** The panel already
  had \`--decal-*\` hooks, and \`--decal-bezel\` was the HEADER's background layer
  as well as the gauge ferrules. Pointing it at a 320 px brass ring painted a
  stretched ring across the whole header. It is the same rule as for an audio
  parameter, met on a CSS variable.`,
`- **Find every consumer of a variable before redefining it.** The panel already
  had \`--decal-*\` hooks, and \`--decal-bezel\` was the HEADER's background layer
  as well as the gauge ferrules. Pointing it at a 320 px brass ring painted a
  stretched ring across the whole header. It is the same rule as for an audio
  parameter, met on a CSS variable.
- **When an order fails twice, stop ordering and start salvaging.** A strip of
  fine grain mirrors into a seamless tile; a cropped half-circle can be placed
  so the crop is off the frame. Both keep the artist's drawing and invent
  nothing. What cannot be salvaged is a composition fault — the nameplate's
  letters are cut by the plate's own edge, and no crop puts that back.`]
]);

const c = edit(C, [
  [`- **Redo order at \`assets/high-tide-decals/REDO.md\`** for ground, paper, nameplate and rose.`,
`- 2026-09-04 **260904.4 — the second decal delivery: rescued, not re-ordered.** The same four came back LETTERBOXED again, but the drawings were good this time (ground luminance sd 2.6 -> 21.4, paper 3.3 -> 8.8), so three were taken from this side: **ground and paper strips are mirrored top-to-bottom into seamless tiles** (a fine random grain mirrors invisibly — the standard trick), and the **rose is the top half of a circle, cleanly cropped rather than squashed**, so it rises out of a corner of the terrain view where only its upper quadrant is in frame. **Dark ink on a dark panel is invisible: the rose is filled THROUGH ITS OWN ALPHA with pale brass** (\`tintStencil\` — draw, then source-in a fill). Eleven of twelve parts in; 769 KB. **When an order fails twice, stop ordering and start salvaging** — but a composition fault cannot be salvaged, which is why the nameplate is still out.
- **Redo order at \`assets/high-tide-decals/REDO.md\`** now asks for the nameplate alone.`]
]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
for (const [f, s] of [b, d, c]) fs.writeFileSync(f, s, "utf8");
console.log("notes updated for the second decal round");
