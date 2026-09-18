# RITE OF PASSAGE — panel decal brief

Paste this whole file as one message. It is self-contained: the style rules,
all twelve parts with exact pixel sizes and filenames, what each part becomes
on the panel, and where to commit them.

---

## The task

Generate twelve photographic component decals for the front panel of an audio
plugin called **RITE OF PASSAGE**, then commit them to
`assets/rite-decals/` in the BrokildApps repository. One isolated object per
image, PNG with transparency — the three grounds in Group A are the
exceptions and say so.

## The machine

Rite of Passage builds transitions. One slider carries the music from where it
is to where it is going, six effects fire along the way on their own
schedules, and at the end there is an arrival. The panel is **a threshold**:
a doorway of two uprights and a crossbeam, with the slider's marker travelling
through the gate, and six notched lanes below it — one per effect — that
catch fire in turn as the transition proceeds.

The lanes, the marker's position, the curves, the glow, every number and every
letter of the interface are drawn live by code. What code cannot draw is a
real material. That is what these parts are.

## THE ONE RULE THAT OVERRIDES EVERYTHING

**Nothing on this panel may be traceable to any real culture, living or
historical.** No borrowed script, no borrowed symbol system, no motif that
belongs to a real people. Do not reference, evoke, pastiche or "take
inspiration from" any specific tradition, region, or indigenous art form. If
a viewer could name a culture looking at it, it is wrong and it gets rejected.

What is wanted instead is the set of marks that **every** human group has
invented independently, owned by nobody: **tally strokes, dots, circles,
chevrons, notches, and a doorway.** Abstract geometry, honest materials, and
the look of something made by hand with a stiff brush and a blade. Think
"marks counted on a post by somebody who needed to count something" — not
ornament, not decoration, not a style.

The rite this panel depicts is fictional and must stay unidentifiable.

## The look

Warm, burnt, used. **Charred wood, fired clay, ochre pigment, ash, bone,
waxed cord and hammered dark iron.** Nothing polished, nothing new, nothing
precious. Every object has been handled a great deal and cared for a little.

Palette:

| | hex | |
|---|---|---|
| ground | `#141010` | charred wood, a warm black |
| ochre red | `#b3452a` | dried pigment |
| ochre yellow | `#d39b3a` | |
| ash / bone | `#e6ddcd` | |
| smoke | `#6b625a` | |
| ember | `#ff6a2a` | painted by code only — do not put it in any part |

**Do not add glow, heat, fire or light to any part.** The panel lights itself:
the ember colour is applied live by code to whatever is currently active. You
are delivering everything in its **cold, unlit, at-rest state.**

## Rules that apply to EVERY part, without exception

- Perfectly **flat-on and orthographic**. No perspective, no tilt, no angle.
- **Transparent background** (PNG with alpha). The three grounds in Group A
  are opaque and fill their frame instead.
- **No drop shadow and no cast shadow of any kind.** Self-shading on the
  object itself is wanted; shadow falling away from it is not.
- The object **fills the frame edge to edge** at the exact pixel dimensions
  given, with no padding or margin unless the part says otherwise.
- Even, soft, warm light from slightly above — a low fire in a room, not
  daylight and not a studio. Iron reads as dark satin, never chrome. Clay
  reads matte.
- **No lettering, no numerals, no digits anywhere** unless the part explicitly
  asks for it. Exactly one part does (12).
- Photoreal, not illustration and not obviously 3D-rendered. Parts 10 and 11
  are hand-painted marks and say so.
- Deliver at **exactly** the pixel size given. Aspect ratio matters most — a
  part delivered at the wrong aspect gets squashed on the panel and it shows
  immediately.
- **Anything described as tiling must have no edges, no corners, no vignette
  and no end caps.** It will be repeated; a piece with ends reads as a stack
  of blocks.
- Anything described as **perfectly circular** gets rotated by code. It must
  be a true circle, exactly centred, touching all four edges of its frame.

---

# GROUP A — the grounds (opaque, seamless tiling)

## 01 · `01-charred-wood-ground.png` — 1024 × 1024, OPAQUE, TILING

The panel's background. A plank of hardwood that has been **burned and then
brushed back** — the surface charred deep warm black `#141010`, with the grain
showing through where the char was scrubbed away, in browns and greys no
lighter than `#3a302a`. Deep grain, a few shallow splits, no knots at the
centre. Matte; char does not shine.

Seamless in both directions. No plank edges, no board joins, no vignette.

## 02 · `02-fired-clay-slab.png` — 1024 × 1024, OPAQUE, TILING

The strip the six lanes are cut into. Unglazed fired clay in a dusty pale
ochre, around `#a08468`, with the very fine pitting and occasional dark
inclusion of earthenware. Slightly uneven in tone across the tile, as fired
clay is. Matte, no glaze, no shine, no crackle glaze pattern.

Seamless in both directions. No slab edges.

## 03 · `03-ash-wear-mask.png` — 1024 × 1024, TILING, GREYSCALE ON TRANSPARENT

A wear and soot overlay the panel multiplies over everything. Soft irregular
deposits of pale ash and grey soot on a **fully transparent** background —
no background colour at all, only the ash. Varying density: some areas nearly
clear, some a light film. Organic and blotchy, never a regular pattern, never
a noise texture.

Seamless in both directions.

---

# GROUP B — the threshold

## 04 · `04-lintel-beam.png` — 2048 × 256

The crossbeam of the doorway, running the full width of the panel above the
main slider. A single squared timber, charred like part 01, **bound at each
end with waxed cord** in tight even turns, and with a band of hammered dark
iron around the middle third — flat-headed, forged, slightly irregular, with
the planishing marks visible.

Straight-on elevation. The beam fills the frame's full width and height. Its
left and right ends are cut square and are visible (this part does not tile).

## 05 · `05-upright-post.png` — 256 × 768

One upright of the doorway; code mirrors it for the other side. The same
charred timber as part 04, a little narrower, standing vertically and filling
the frame top to bottom. **Three shallow notches cut into it** at irregular
heights — counted marks, blade-cut, the pale wood showing inside the cut. One
cord binding near the top.

The bottom end runs off the frame; the top end is cut square.

## 06 · `06-marker.png` — 256 × 320

The handle of the main slider — the thing that travels through the gate, and
the object the eye follows. A **smooth river stone**, pale bone grey, roughly
egg shaped with the point downward, **bound in a cradle of waxed cord** that
wraps it four or five times and crosses over the front. The cord is dark and
slightly frayed. The stone is worn completely smooth; the cord is not.

Vertical, centred, filling the frame with a few pixels of clearance at the
sides. This part is the hero of the panel at rest — give it the most care.

---

# GROUP C — the controls

## 07 · `07-knob-large.png` — 512 × 512, PERFECTLY CIRCULAR

The main knobs. A disc of **fired clay**, matte, the colour of part 02, with
a **single blade-cut notch** through the rim at the twelve o'clock position
showing the paler unfired body inside. Around the rim, a **band of waxed cord**
wrapped tightly for grip, dark and slightly uneven, covering the outer sixth
of the radius. The clay face is very slightly concave and catches soft light
from the upper left.

**No pointer, no line, no marks on the face** beyond the one rim notch — code
draws the indicator. No numerals, no scale, no ticks.

True circle, exactly centred, touching all four edges.

## 08 · `08-knob-small.png` — 256 × 256, PERFECTLY CIRCULAR

The per-slot knobs. The same object as part 07, smaller and plainer: fired
clay, one rim notch at twelve o'clock, **no cord binding** — the rim is bare
clay with a chamfered edge. Same material, same light, same rules. No pointer,
no marks on the face.

True circle, exactly centred, touching all four edges.

## 09 · `09-slot-socket.png` — 384 × 384

The empty recess a slot's mark sits in, six across the panel. A **square plate
of hammered dark iron** with a shallow square recess sunk into its centre,
the recess about two thirds of the plate's width. The iron is forged, not
cast: uneven planishing marks, softened corners, a little surface rust in the
low spots. Four small rivets, one near each corner, flat-headed and slightly
proud.

The recess is **empty** — nothing in it. Straight-on. The plate fills the
frame.

---

# GROUP D — the marks

## 10 · `10-glyph-sheet.png` — 1536 × 1152, 4 × 3 GRID OF 384 × 288 CELLS

**The most important part in this brief.** Twelve invented marks, one per
effect, delivered as one sheet so that they are unmistakably the work of one
hand. Code slices the sheet on an exact 4 × 3 grid, so **every cell must be
exactly 384 × 288 and every mark must be centred in its own cell** with clear
space around it. No grid lines, no borders, no cell backgrounds, no labels.

**Medium**: ochre pigment painted by hand onto a flat surface with a stiff
brush. Slightly uneven stroke edges, a little thinning where a stroke ends,
the occasional bristle streak. **Not** vector-clean, **not** calligraphic,
**not** a font. A person made these quickly and meant them.

**Colour**: deliver every mark in **ash white `#e6ddcd`** on a fully
transparent background. Code tints them — cold at rest, ember when active — so
the pigment must be near-neutral and even in value, with no colour of its own
and no glow.

Re-read THE ONE RULE before drawing these. They are abstract geometry. They
must not resemble any real alphabet, syllabary, numeral system, rune,
pictograph or religious symbol. If a mark looks like a letter, redraw it.

Reading order, left to right, top to bottom:

| cell | mark |
|---|---|
| 1 | **Five vertical strokes** in a row, each taller than the one before it, left to right. The tallest is twice the shortest. |
| 2 | **A spiral** of about two and a half turns, unwinding clockwise, its outer end pulled out straight into a tail running to the right. |
| 3 | **Seven vertical strokes**, all the same height, with the spacing halving left to right so they crowd together at the right edge. |
| 4 | **One thick horizontal bar** interrupted by four even vertical gaps — a comb with teeth missing. |
| 5 | **A solid triangle** whose right third has broken apart into a scatter of separate dots, the dots getting smaller as they move right. |
| 6 | **Four concentric arcs** — not closed circles — opening rightward from a single heavy dot, each arc longer and thinner than the last. |
| 7 | **A small solid square held between two facing brackets**, with three short parallel strokes crossing the square diagonally. |
| 8 | **A chevron pointing left**, its tip touching a solid vertical line; two smaller chevrons trailing away to the right, each lighter than the last. |
| 9 | **An open circle** with a **solid wedge driven into its lower right**, breaking the line of the circle where it enters. |
| 10 | **A long horizontal line** with a single stroke crossing it and curving down and away to the right, below the line. |
| 11 | **A vertical column of seven dots**, growing from a fine point at the bottom to a heavy filled disc at the top. |
| 12 | **Two facing brackets with nothing between them.** This mark is absence, and it must read as deliberate — confident, well-made, and empty. It is not an error. |

## 11 · `11-arrival-mark.png` — 768 × 768

The mark that is struck at the moment of arrival, drawn over the whole panel
for a single frame. **A burst**: one heavy central point with about sixteen
strokes radiating outward, of clearly uneven length and weight, some short and
thick, some long and fine — struck fast, not measured out. Same hand, same
stiff brush, same ash white `#e6ddcd` on transparent, same pigment quality as
part 10.

Not a star, not a sun, not a symmetrical starburst. It should look like
something hit the surface hard and the pigment went outward from the point of
impact.

Centred, filling the frame. No glow.

## 12 · `12-wordmark.png` — 1536 × 256

The only lettering in this brief. The words

**RITE OF PASSAGE**

**branded into charred wood** with a hot iron — the letters pressed in, the
wood scorched darker at the edge of each stroke and slightly raised around it,
one or two strokes pressed unevenly so the brand did not take cleanly. Plain
sans-serif capitals, wide letter spacing, no serifs and no decoration. The
letters are the only content; the wood behind them is part of the object.

On a transparent background — the plate of wood is the object and its edges
are visible, a long shallow rectangle with the grain running horizontally.

---

## Delivery

- PNG, transparency where stated, exactly the stated pixel dimensions.
- Commit to `assets/rite-decals/` with exactly the filenames above.
- Add a `DELIVERED.md` in the same folder listing which parts were delivered,
  at what size, and any part where you had to deviate and why.

If any part cannot be made at the stated aspect, deliver the rest and say
which one failed rather than delivering it squashed. Aspect is the one thing
that cannot be fixed on the panel.
