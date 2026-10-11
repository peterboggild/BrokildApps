# Magneto Transport: decal order

This is a list of image parts for a software tape-echo instrument called
**Magneto Transport**. Each part is made separately and the program puts the
parts together, so **never paint a whole panel**. A program places every part
at exact coordinates, and an image generator cannot hit exact coordinates.
What matters is that each part looks right on its own canvas, at the stated
aspect ratio, with real transparency where asked.

You can paste this document as it is. It assumes you have seen nothing else.

---

## 1. The style, for every part (paste this paragraph with every request)

> A 1970s professional studio tape machine, photographed straight on, for a
> realistic software panel. Dark anodised steel and black crackle paint, with
> brushed aluminium reels, black rubber, and aged brass lettering and trim. It
> is a working machine that is well kept: fine scratches and a little dust, no
> rust, no dents, no grime. One soft light from the upper left at about 45
> degrees, so highlights sit upper left and shadows fall lower right. Flat,
> orthographic, top-down view with no perspective, no tilt and no camera blur.
> No text, logos, numbers or labels unless the request lists exact lettering.
> No background scenery and no drop shadow outside the object unless asked.

**Palette (hex), use these and nothing brighter:**

| role | hex |
|---|---|
| near-black ink | #0b0d0c |
| panel steel | #1a1d1b, #232724 |
| edge, bevel | #33382f, highlight #4a5145 |
| brass lettering | #c9b27a, dim #8a7a52, low #5a5138 |
| paper (meter face) | #d8cfb4 |
| amber lamp | #ffb340 |
| red lamp, red line | #d2412c |
| green lamp | #7fbf6a |
| tape oxide brown | #4a3220 |
| smoked glass | #0a0b0a |

---

## 2. Rules for the delivery (please read; past deliveries failed on these)

1. **One PNG per part**, sRGB, named exactly as listed (`mt-...png`).
2. **Use the canvas size given.** The aspect ratio is what matters. Resolution
   can be higher, but never change the aspect.
3. **Fill the canvas as described.** Before you write that there were no
   deviations, measure the drawing's bounding box (the non-transparent pixels,
   or the non-background pixels on an opaque part) against the canvas. Report
   it in the delivery notes as `x, y, w, h` in pixels. Two earlier deliveries
   came back with the drawing in a strip at the top of a larger canvas while
   the notes said "no deviations". A drawing smaller than its canvas is a
   deviation.
4. **Transparent means real alpha.** Outside the object, alpha is exactly 0.
   Inside an opaque object, alpha is 255. No checkerboard pattern painted into
   the pixels and no white or black halo. An "empty" glass part once came back
   7 % opaque everywhere, which made it unusable.
5. **Opaque parts** fill the whole canvas edge to edge, with no border and no
   vignette.
6. **Anything that rotates** sits on a SQUARE canvas with its pivot exactly at
   the centre of the canvas.
7. **Pairs must be registered.** For each pair (off/on, up/down,
   released/pressed) the two images have the same canvas and the object at the
   same place, so that switching between them moves nothing but the light or
   the press.
8. **Tiling textures tile.** They have no end caps, no border, no vignette and
   no single feature that would repeat visibly. The left edge must continue
   into the right edge and the top into the bottom.
9. **Lettering.** Most lettering is done by the program. Only the wordmark
   (part 24) and the two plaques (part 23) are asked as lettering, and the
   exact text is given. If the generator cannot spell it exactly, deliver the
   part blank and the program letters it.
10. **Delivery notes.** Put a `DELIVERED.md` beside the files: one line per file
    with its canvas size, the bounding box from rule 3, and anything that is not
    as asked.

---

## 3. The parts

Sizes are **on screen at 1x** in pixels (the panel is 1600 x 900 at 1x), so
you can judge how much detail will show. Order at the canvas size given, which
is about 4x. Fine detail that is smaller than a pixel at 1x will not be seen.

### A. Surfaces (opaque, tiling)

**1. `mt-deck.png`** — the deck plate the tape machine is built on (the upper
half of the panel, 1600 x 436 at 1x). Canvas **1024 x 1024**, opaque, tiling
both ways. Black crackle-finish paint over steel, very fine and even, slightly
lighter than the panel steel (#1d201e average). No screws and no features.

**2. `mt-steel.png`** — the control panel's base metal behind the plates.
Canvas **1024 x 1024**, opaque, tiling both ways. Dark anodised steel
(#1a1d1b average) with a very faint horizontal brush grain.

**3. `mt-plate.png`** — the metal plates that each hold a group of knobs (eight
plates of 228 to 528 x 170 at 1x). Canvas **1024 x 1024**, opaque, tiling both
ways. A slightly raised steel plate face, #232724 average, with a fine vertical
brush grain (lines run top to bottom). Plain face only: the program draws the
plate edges, bevel and screws.

**4. `mt-header.png`** — the strip along the top (1600 x 64 at 1x). Canvas
**2048 x 128**, opaque, tiling LEFT TO RIGHT only. Satin black anodised
aluminium with a soft vertical gradient (lighter at the top, #262a27 to
#151816) and a fine horizontal brush grain.

### B. The tape machine (transparent unless stated)

**5. `mt-reel-flange.png`** — the front flange of a 10.5 inch professional
aluminium tape reel (NAB style, three large windows). On screen 327 px across.
Canvas **1600 x 1600**, transparent, **pivot at the centre**, the flange
filling the canvas to within 8 px of each edge. Brushed aluminium with a
circular brush pattern. The **three windows are cut right through: alpha 0
inside them**, because the tape on the reel shows through. The centre hole is
also cut through (alpha 0) to a diameter of **43 % of the flange**, because the
hub (part 6) sits there. One window points straight up. No tape, no label.

**6. `mt-reel-hub.png`** — the NAB hub at the reel's centre. On screen 141 px
across. Canvas **1024 x 1024**, transparent, **pivot at the centre**, filling
the canvas to within 8 px. Machined aluminium hub with three short drive keys
at 120 degrees (one pointing straight up) and a dark centre spindle. The keys
must be visible, because they show the reel turning.

**7. `mt-capstan.png`** — the capstan, a polished steel shaft seen end on.
On screen 12 px across. Canvas **256 x 256**, transparent, pivot at the centre,
filling to within 4 px. Polished steel with one faint machining mark so that
turning is visible.

**8. `mt-pinch.png`** — the pinch roller, black rubber tyre on a steel hub,
seen end on. On screen 30 px across. Canvas **512 x 512**, transparent, pivot
at the centre, filling to within 6 px. A matte black rubber tyre, a small
steel centre with one bolt head off centre (shows rotation).

**9. `mt-roller.png`** — a tape guide roller, steel, seen end on. On screen
15 px across. Canvas **256 x 256**, transparent, pivot at the centre, filling
to within 4 px. Chrome steel with a darker centre and one small mark.

**10. `mt-arm.png`** — a tape tension arm: a flat steel bar with a pivot boss
at one end and a roller boss at the other. Canvas **512 x 512**, transparent.
**The pivot boss is centred at the canvas centre (256, 256); the bar runs
straight to the RIGHT and its roller end is centred at (480, 256).** Bar width
about 40 px on that canvas, pivot boss about 70 px across, roller boss about
60 px across. The program rotates the arm about the pivot.

**11. `mt-head-erase.png`, `mt-head-record.png`, `mt-head-play.png`** — three
tape heads seen from above, the tape running across their top edge. On screen
18 x 28 px each. Canvas **352 x 544** each (portrait), transparent, the head
filling the canvas to within 6 px. A mu-metal shield can (dark steel, satin)
with the polished head face at the TOP edge and a thin vertical gap line in
the middle of the face. Tell them apart by a coloured band just below the
face: erase **dull red #a8442f**, record **brass #c9b27a**, playback **steel
grey #4a4f45**. No letters (the program writes E, R, 1, 2, 3).

**12. `mt-carriage.png`** — the sliding carriage that carries a playback head
along a rail (seen from above, below the head). On screen 26 x 30 px. Canvas
**416 x 480**, transparent, filling to within 6 px. A small black anodised sled
with two visible slider shoes at the bottom.

**13. `mt-rail.png`** — the rail the carriages slide on. On screen 345 x 10 px.
Canvas **2048 x 64**, transparent above and below the bar, **tiling LEFT TO
RIGHT**. A ground steel bar about 32 px tall, centred vertically, with a
polished top edge. No scale marks (the program draws the scale).

**14. `mt-headplate.png`** — the mounting plate under the head block. On
screen 380 x 83 px. Canvas **1520 x 332**, opaque, filling the canvas. A thick
machined aluminium plate, darker than the reels, with a soft bevel on all four
edges and four countersunk screws, one near each corner (about 40 px in from
each corner).

**15. `mt-looptank.png`** — the tape loop tank, a smoked-glass box the loop of
tape folds into, seen from the front. On screen 273 x 248 px. Canvas
**1320 x 1200**, filling the canvas. A dark steel frame about 50 px wide all
round, with **smoked glass inside the frame at 35 % opacity (alpha about 90)**,
so the folded tape drawn behind it shows through, and a faint diagonal
reflection in the upper left of the glass. The frame is opaque. The program
writes the label under it.

### C. Controls

**16. `mt-knob.png`** — a control knob seen from straight above: a black
knurled skirt with a dark aluminium cap. Used at 38, 46 and 58 px. Canvas
**512 x 512**, transparent, **pivot at the centre**. The knob is a circle
**338 px across** (66 % of the canvas), centred, so that the program can draw
its scale and amber position arc around it. The skirt is knurled (fine
radial ridges); the cap is about 70 % of the knob's width, slightly domed,
satin. A **cream pointer line #ffcf7a** runs from the cap's centre straight UP
to the skirt's edge. Nothing outside the knob's circle.

**17. `mt-key-up.png` + `mt-key-down.png`** (pair, registered) — a segmented
selector key, with no lettering. The program stretches it to fit its label,
so it is used as a nine-slice: **the outer 24 px on every side is the edge and
is never stretched**. Canvas **192 x 96** each, opaque, filling the canvas.
Up: a raised black key with a soft top highlight. Down: the same key pressed
in and lit, a pale brass face (#e2cd95 to #b39a5c). Same edges, same place.

**18. `mt-button-up.png` + `mt-button-down.png`** (pair, registered) — a push
button, no lettering, nine-slice with a **24 px edge**. Canvas **192 x 96**,
opaque, filling the canvas. Up: a raised dark steel button with a slight
dome. Down: the same pushed in by about 4 px, darker on the top edge.

**19. Lamps** (three pairs, registered): `mt-lamp-amber-off.png`,
`mt-lamp-amber-on.png`, `mt-lamp-red-off.png`, `mt-lamp-red-on.png`,
`mt-lamp-green-off.png`, `mt-lamp-green-on.png`. On screen 8 to 10 px. Canvas
**128 x 128** each, transparent, a round panel lamp centred, its lens **80 px
across**, in a thin black bezel. Off: a dark, coloured glass dome. On: lit
from inside, with a soft glow that may extend to the canvas edge but must fade
to alpha 0 before it.

**20. `mt-slider-track.png` + `mt-slider-thumb.png`** — a horizontal fader.
Track: on screen 176 x 10 px, canvas **1408 x 80**, opaque, nine-slice with a
**24 px edge**, a sunken black slot. Thumb: on screen 12 x 20 px, canvas
**192 x 320**, transparent, filling to within 8 px, a brushed steel fader cap
with one dark centre line running top to bottom.

**21. `mt-screw.png`** — a panel screw head, slotted. On screen 9 px across.
Canvas **128 x 128**, transparent, pivot at the centre, filling to within 4 px.
The slot runs left to right through the centre (the program turns each screw
at random).

**22. VU meter**: `mt-vu-face.png` + `mt-vu-needle.png`. On screen the meter
is 112 x 56 px. Face: canvas **896 x 448**, opaque, filling the canvas. A
black bezel about 28 px wide all round and, inside it, a warm cream paper
face (#d8cfb4), slightly aged, lit evenly, with a faint inner shadow under
the bezel's top edge. **Leave the face blank**: no scale, no numbers, no
letters (the program draws the scale exactly). Needle: canvas **64 x 512**,
transparent, a thin black needle pointing straight UP, its pivot at the
**bottom centre (32, 500)**, tapering to a point at the top.

**23. Plaques**: `mt-plaque-left.png` and `mt-plaque-right.png` — two small
engraved brass plates on the machine. On screen 112 x 104 px. Canvas
**896 x 832** each, opaque, filling the canvas. Dark aged brass with a fine
bevel and two small screws (top centre and bottom centre). Exact engraving,
centred, uppercase, in a plain engineering typeface, filled with dark paint:

- left: **BROKILD** (larger), then on separate lines **MAGNETO** /
  **TRANSPORT** / **MODEL MT-1** / **FLUX MEMORY** / **TAPE ECHO**
- right: **CAUTION** (larger), then **KEEP HEADS** / **CLEAN** /
  **DEMAGNETISE** / **WEEKLY** / **6.3 mm TAPE**

If the spelling is not exact, deliver both plates blank.

**24. `mt-wordmark.png`** — the name lettered in brass, as on the machine's
nameplate. On screen about 250 x 30 px. Canvas **2000 x 240**, transparent,
the lettering filling the canvas to within 16 px. Exact text, in mixed case
exactly as written: **Magneto Transport**. A condensed, slightly wide-spaced
engineering typeface, polished brass letters (#c9b27a) slightly raised, with a
thin dark edge. If the spelling is not exact, do not deliver this part.

**25. `mt-glass.png`** — smoked glass for the readout windows (seven windows,
44 to 150 px wide). Canvas **384 x 192**, transparent, nine-slice with a
**24 px edge**. Dark glass at about **25 % opacity (alpha about 64)** with a
faint highlight along the top edge. It is laid OVER amber digits, so it must
stay see-through.

---

## 4. What happens on return (for us, not for the deliverer)

`tools/ingest-decals.ps1` crops each file to its measured content box, checks
the alpha of every transparent part (outside 0, inside 255), checks that each
pair is registered (same canvas, same content box within 2 px), checks that
tiling textures tile (edge difference), and prints a report per part before
anything is wired in. A part that fails is reported and the program keeps its
own drawing for it. Everything above can arrive in batches.

Where each part goes, for the integration: the deck plate and the machine
parts are drawn on the 1600 x 528 machine canvas (1 machine unit = 0.826 px
at 1x); the reels sit at (230, 210) and (1370, 210) with a flange radius of
198 units and a hub radius of 85.5; the capstan is at (1010, 392), the pinch
roller at (1010, 420); the guides at (505, 391) and (1060, 391); the arms
pivot at (425, 300) and (1145, 300) with their rollers at (455, 352) and
(1120, 352); the heads sit on the tape line y = 400 at x = 562.5 (erase), 600
(record), and 600 + 15 x the head position in cm (playback); the rail runs
from x = 622.5 to 960 at y = 462; the loop tank is (40, 60, 330, 300). The
controls take their sizes from `__MT.geom()` in the panel.
