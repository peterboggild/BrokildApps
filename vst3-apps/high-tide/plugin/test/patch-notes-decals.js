/*  Record the decal round: the buglist, the design doc, CLAUDE.md, and the
    landing page's download size. */
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
const L = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/high-tide/index.html";

const b = edit(B, [
  [`## Awaiting go

*(nothing at present)*

## Shipped`,
`## Awaiting go

### 3. Four decals to redo (sent 2026-09-04)
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
whether it is on, which a brass cap would hide. Keep it for a future revision.

## Shipped`],
  [`- 260904.1 — the instrument (see \`BrokildApps/HIGH-TIDE-DESIGN.md\` §12).`,
`- **260904.3 — six decals into the panel.** The pearl IS the ball; the scope
  became a round instrument in a brass bezel; every point on the timeline is a
  brass chart tack and the release marker a red pennant; the gauges are glass
  tubes (the part is drawn standing and the gauges lie down, so the ingest
  turns it a quarter turn); two oak rails under the header and over the
  timeline. Each is applied only once its image has LOADED, so the panel
  without them is exactly the panel that shipped. 524 KB in the binary,
  served from BinaryData at \`/decals/\`.

- 260904.1 — the instrument (see \`BrokildApps/HIGH-TIDE-DESIGN.md\` §12).`]
]);

const d = edit(D, [
  [`*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.2,`,
   `*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.3,`],
  [`## 13. THE CENTRE LINE — the discovery of the second round (260904.2)`,
`## 14. The decals (260904.3)

Twelve photographic parts were ordered through
\`assets/high-tide-decals/BRIEF.md\` and delivered. Six went in: the pearl is
the ball, the bezel made the scope a round instrument, the tack is every point
on the timeline, the flag is the release marker, the glass is the gauge tubes
(drawn standing, used lying — the ingest turns it), and the oak rail runs under
the header and over the timeline. Each is applied only when its image has
loaded, so a missing file leaves the drawn panel exactly as it was.

Four came back letterboxed and one has no home; the redo order is
\`assets/high-tide-decals/REDO.md\`. Three lessons worth keeping:

- **A part's delivered size is not its drawn size.** Four arrived with the
  drawing pasted into a band of a larger black canvas, so the file was the
  right number of pixels and the picture inside it was the wrong shape. The
  ingest crops every part to its own content before scaling; a straight rescale
  to the asked-for size would have smeared them. Measure the content box, do
  not trust the header.
- **Content is where a transparent image is OPAQUE and where an opaque one is
  NOT BLACK.** Asking for both at once cropped the dark engraving off the
  bottom of the brass nameplate, because dark lettering failed the brightness
  half of the test. Choose the test from whether the part actually carries
  alpha.
- **Find every consumer of a variable before redefining it.** The panel already
  had \`--decal-*\` hooks, and \`--decal-bezel\` was the HEADER's background layer
  as well as the gauge ferrules. Pointing it at a 320 px brass ring painted a
  stretched ring across the whole header. It is the same rule as for an audio
  parameter, met on a CSS variable.

## 13. THE CENTRE LINE — the discovery of the second round (260904.2)`]
]);

const c = edit(C, [
  [`- **Not built (deliberately):** the 2-D bowl (design §3.2), the bifurcation view (§3.4), MPE. The mod wheel and channel pressure lift the TIDE.`,
`- 2026-09-04 **260904.3 — the decals** (ChatGPT delivered all twelve from \`assets/high-tide-decals/BRIEF.md\`). Six wired in behind a load check so the panel without them is byte-for-byte the panel that shipped: **pearl = the ball** (drawn on the 2-D overlay at the projected world point, over the WebGL glow), **bezel = a round oscilloscope**, **tack = every timeline point**, **flag = the release marker**, **glass = the gauge tubes** (drawn standing, used lying — `+"`tools/ingest-decals.ps1`"+` turns it a quarter turn), **wood = two rails**. 524 KB, served from BinaryData at \`/decals/\` by filename lookup through \`originalFilenames\`.
- **Three decal lessons, all measured:** (1) **a part's delivered size is not its drawn size** — four came back LETTERBOXED (the drawing pasted into a band of a larger black canvas: ground 2048x156 of 2048x2048, paper 2048x155 of 2048x1024, nameplate 1589x124 of 1600x360, rose 943x357 of 1024x1024), so the ingest crops to the content box before scaling; (2) **content is where a transparent image is OPAQUE and where an opaque one is NOT BLACK** — asking for both cropped the dark engraving off the brass nameplate; (3) **find every consumer of a variable before redefining it** — the panel's \`--decal-bezel\` hook was the HEADER's background layer as well as the tube ferrules, so pointing it at a brass RING painted a stretched ring across the whole header. Same rule as for an audio parameter, met on a CSS variable.
- **Redo order at \`assets/high-tide-decals/REDO.md\`** for ground, paper, nameplate and rose. \`ht-knob\` is perfect and has no home (no rotary control on this panel; the TUNE LOCK dot uses its colour for state). Tools kept: \`tools/ingest-decals.ps1\`, \`decal-audit.ps1\` (content coverage + row profile), \`contact-sheet.ps1\` (checkerboard sheet of a folder), \`crop-zoom.ps1\`. **PowerShell variables are case-INSENSITIVE — a local \`$out\` ate the \`-Out\` parameter.**
- **Not built (deliberately):** the 2-D bowl (design §3.2), the bifurcation view (§3.4), MPE. The mod wheel and channel pressure lift the TIDE.`]
]);

const l = edit(L, [[`(zip, 7.6 MB)`, `(zip, 8.2 MB)`]]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
for (const [f, s] of [b, d, c, l]) fs.writeFileSync(f, s, "utf8");
console.log("buglist, design doc, CLAUDE.md and landing page updated");
