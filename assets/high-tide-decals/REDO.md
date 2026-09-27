# HIGH TIDE decals — one part still to fix

**Eleven of the twelve parts are now in the instrument.** The second delivery
fixed what mattered most: the ground and the paper have real material in them
at last, and the compass rose is a lovely piece of engraving. Thank you.

Only the **nameplate** is still unusable, and it is a composition problem
rather than a rendering one.

---

## What was taken from the second delivery, and how

The four redone files came back **letterboxed again** — the drawing occupying a
band at the top of the canvas rather than filling it:

| part | canvas | drawing inside it |
|---|---|---|
| `ht-ground.png` | 2048 × 2048 | 2048 × **82** |
| `ht-paper.png` | 2048 × 1024 | 2048 × **101** |
| `ht-rose.png` | 1024 × 1024 | 985 × **365** |
| `ht-nameplate.png` | 1600 × 360 | 1600 × **110** |

Three of them were rescued on this side rather than asking a third time:

- **ground** and **paper** — the strips are good texture now (the ground's
  luminance variation went from 1 % to 8 % of full scale), so each strip is
  mirrored top-to-bottom into a tile that repeats without a seam. They are the
  chart-table surface and the tide-table paper under the timeline.
- **rose** — it is the top half of a circular rose, cleanly cropped rather than
  squashed, so it is used rising out of a corner of the terrain view where only
  its upper quadrant is in frame. That is a normal chart ornament and it
  invents nothing.

**No need to send those three again.**

---

## The one to redo · `ht-nameplate.png` — **1600 × 360**, transparent background

An engraved brass nameplate reading exactly **HIGH TIDE** in engraved serif
capitals, a single thin engraved wave line beneath the words, and one small
slotted screw head in each of the four corners.

**The problem, in both deliveries:** the plate is drawn as a thin bar while the
lettering keeps its full size, so the plate's own bottom edge cuts through the
letters. In the last file the capitals are sliced roughly in half. No crop can
put that back.

**What it needs:** the type must sit *inside* the plate with clear brass around
it. Take the plate's height as 100 %:

- empty brass above the capitals: about **20 %**
- the capitals themselves: about **40 %**
- the engraved wave line and empty brass below: the remaining **40 %**

The words must not touch the plate's edge or its bevel anywhere.

Aged, softly polished brass with fine horizontal brushing. The engraving is
recessed and dark, catching a highlight on its lower lip. **The plate fills the
frame edge to edge at 1600 × 360** — the plate is the picture, and its own
bevelled edge is the edge of the file. Transparent outside the plate. Flat-on,
orthographic, no cast shadow. This is the only part that carries lettering.

If it is easier to get right: draw the plate at a comfortable size and shape
first, with the lettering correctly placed inside it, and then resize the whole
drawing to 1600 × 360 — rather than placing a finished drawing into a canvas.

---

## Delivering

Commit `ht-nameplate.png` into `assets/high-tide-decals/` in
`peterboggild/BrokildApps`, overwriting the existing file, and add a line to
`DELIVERED.md`.

**Before committing, open it and check:** the file is exactly 1600 × 360, the
brass reaches all four edges, and there is clear brass above and below the
letters.
