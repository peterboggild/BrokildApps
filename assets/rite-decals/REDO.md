# RITE OF PASSAGE decals — one part to redo

Eleven of the twelve are in the plugin and look right. Thank you. This is the
one that cannot be used, and one measurement worth knowing for next time.

## 12 · `12-wordmark.png` — REDO

**The lettering is taller than the plank it is burnt into**, so the bottom of
every letter is cut off by the plank's own lower edge. "RITE OF PASSAGE" reads
as a row of letter-tops. Nothing can rescue this from here — a crop keeps the
clipping and a stretch makes it worse — so the panel is drawing the title in
type until a replacement arrives.

Please redo it with the **whole plank inside the canvas and the whole of every
letter inside the plank**, with clear wood above and below the lettering:

- 1536 × 256, transparent outside the plank.
- The plank spans the full width and roughly 180–220 px of the height, centred,
  so there is transparent margin above and below it.
- The lettering sits inside the plank with at least 20 px of unburnt wood above
  the cap height and below the baseline.
- Everything else as before: charred oak, the letters branded into it rather
  than printed on it, no outline, no drop shadow, no border.

## For the whole delivery — the fault that ran through seven of twelve

`DELIVERED.md` said "Deviations: none". Seven parts were in fact **letterboxed**:
the drawing occupied a strip at the TOP of the stated canvas and the rest was
empty. Measured content, as a fraction of the canvas each file declares:

| part | canvas | actually drawn | share |
|---|---|---|---|
| 01 charred-wood-ground | 1024 × 1024 | 1024 × 202 | 20 % |
| 02 fired-clay-slab | 1024 × 1024 | 1024 × 176 | 17 % |
| 03 ash-wear-mask | 1024 × 1024 | 1024 × 242 | 24 % |
| 04 lintel-beam | 2048 × 256 | 2041 × 75 | 29 % |
| 05 upright-post | 256 × 768 | 90 × 768 | 35 % |
| 06 marker | 256 × 320 | 189 × 307 | 71 % |
| 12 wordmark | 1536 × 256 | 1527 × 108 | 42 % |

All but the wordmark were rescued here: each is cropped to what is drawn, and
the three tiling grounds are mirrored top-to-bottom into a seamless tile
(`tools/ingest-decals.ps1` in the plugin does it, and re-doing a part just means
re-running it). So this is not a complaint about those seven — it is the reason
the wordmark could not be saved the same way, because clipping is not padding.

**The check that catches it**: before writing DELIVERED.md, measure the bounding
box of the non-transparent (or non-black) pixels in each file and compare it
with the canvas. If they differ, say so in the Deviations section rather than
"none" — a stated size is not a drawn size.

## Not a fault, just worth recording

`10-glyph-sheet.png` is exactly right: the 4 × 3 grid is on its nominal
384 × 288 cells and every mark is centred in its own cell with clear space.
It slices perfectly. Note that it carries **eleven** effect marks plus the
empty brackets, and the plugin now has **twelve** effects — GAP has no mark of
its own and falls back to type on the panel. A thirteenth cell for GAP (the
silence before the drop) would finish the set; it is not urgent.

## Also wanted now: seven more marks (added 2026-09-18)

The plugin grew from twelve effects to **eighteen**, so the sheet's eleven
marks no longer cover it. Seven effects currently fall back to their name in
type on the panel, which works and looks like what it is.

A second sheet in exactly the same hand would finish the set: **1024 x 1536,
a 3 x 2 grid of 341 x 768 cells** (or simply another 4 x 3 sheet with five
cells left empty, whichever is easier to draw), same ash white `#e6ddcd` on
transparent, same stiff brush, same clear space, no grid lines.

| mark | the effect | what it should read as |
|---|---|---|
| 1 | GAP | **A bold vertical stroke broken cleanly in two**, with a clear gap between the halves. The gap is the subject; the stroke exists to be interrupted. |
| 2 | SWIRL | **A spiral that does not close** — three loose turns, each drawn with a visible wobble, as though the hand could not hold the circle. Distinct from cell 2 of the first sheet, which is a tight spiral with a straight tail. |
| 3 | MANGLE | **A heavy horizontal bar torn across the middle**, the two halves offset vertically, with a ragged edge where it broke. |
| 4 | SWARM | **Seven short parallel strokes leaning at slightly different angles**, crowded together but never touching — a flock, not a comb. |
| 5 | DUST | **A solid square dissolving from its lower right into a coarse scatter of square specks**, the specks square rather than round. |
| 6 | ORBIT | **An ellipse seen nearly edge on**, with a heavy dot on the near side of it and a light, broken arc on the far side where it passes behind. |
| 7 | CHANT | **A vertical column of five horizontal bars of differing lengths**, widest in the middle, like a spectrum standing on end. |

Everything else about the brief is unchanged. If only one gets drawn, make it
GAP: it has been waiting since the first sheet.
