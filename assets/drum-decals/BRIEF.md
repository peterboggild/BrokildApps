# Decal brief: BEET, KICKSTART, SNARE TACTICS, HATS OFF

One family of four panels, dressed as **raw vintage machine controls**: the controls of a steam
hammer, a lathe, a factory switch cabinet, military field gear. Not audio equipment. No hi-fi knobs, no
studio console, no synth look.

## The idea: one parts kit, four machines

Every panel is built from the SAME kit of parts (knobs, levers, buttons, lamps, gauges), so the four
read as one family. What differs is the MACHINE each one is, carried by its paint and its nameplate:

| panel | the machine | paint | nameplate |
|---|---|---|---|
| **KICKSTART** | a steam pile-driver: a kick is a hammer | red-lead oxide primer on cast iron, worn to black iron on the edges | raised cast-iron letters |
| **SNARE TACTICS** | military field equipment | olive drab, stencilled, scuffed | white stencil on olive |
| **HATS OFF** | a metal lathe in a cymbal foundry (cymbals are spun bronze) | machine-tool grey-green enamel, bronze swarf in the corners | engraved brass plate |
| **BEET** | the factory control cabinet that runs all three | hammer-finish grey-blue enamel, eight machine bays | enamel sign, white on black |

## How the parts are used (read this before generating anything)

The plug-ins draw every control from a geometry table in code. An image generator cannot hit exact pixel
coordinates, so **it makes ISOLATED PARTS, never a finished faceplate**. The code places, rotates and
labels them. **All text on scales and labels is drawn by the code**, never painted into a part, except the
four nameplates.

## Rules for every sheet (each one was learned from a delivery that broke it)

1. **Transparent PNG. Each part FILLS its cell edge to edge.** Two earlier deliveries pasted the drawing
   into a strip of a much larger empty canvas and reported "deviations: none". Measure the non-transparent
   bounding box of every part against its cell before writing the delivery note.
2. **Knobs and levers: seen from straight above, pointer or handle pointing straight UP (12 o'clock).**
   The code rotates them. A knob drawn at an angle cannot be rotated convincingly.
3. **Anything circular sits in a SQUARE cell, centred.**
4. **Two-state parts (a toggle up/down, a button up/pressed, a lamp off/on) are pixel-registered pairs**:
   the same canvas, and the body, bezel and screws in EXACTLY the same place in both, so only the moving
   part changes. An earlier toggle pair was refused because the two bezels were different drawings and the
   switch jumped when flipped.
5. **One light**: from the upper left, soft, the same on every part of every sheet.
6. **Grounds (paint textures) tile seamlessly** in both directions: no vignette, no edges, no end caps,
   no screws, no single big feature that would repeat visibly.
7. **No text anywhere except the four nameplates.** No numbers on scales, no brand names, no signatures.
8. Worn, used, real: scratches, chipped edges showing the metal beneath, grime where hands go. Photographic,
   not illustration, not vector.
9. **Deliver a `DELIVERED.md`** listing, for every sheet, the crop rectangle of every part in pixels and any
   deviation from this brief.

---

## SHEET 1 - the knobs (shared by all four panels)

Canvas **2048 x 1536**, a 4 x 3 grid of **512 x 512** cells, transparent. Row by row:

1. **Bakelite fluted knob, large**: black phenolic, deep flutes, a polished aluminium cap with a white
   engraved pointer line to 12 o'clock. The workhorse control.
2. **Bakelite fluted knob, small**: the same, smaller in its cell (fills about 70 %).
3. **Clamping star knob**: five-lobed black bakelite, like a machine-vice clamp, a steel insert with a
   pointer notch at 12.
4. **Chicken-head selector**: a long tapered bakelite pointer knob, the tip pointing to 12. For stepped
   choices (ENGINE, WAVE, ERA, slot type).
5. **Cast-iron handwheel**: a spoked machine handwheel (four spokes, one spoke straight up), with a small
   turned wooden spinner handle near the rim. The hero control of a panel.
6. **Knurled steel set knob**: small, bright knurled steel, a scribed line at 12.
7. **Crank lever**: a heavy lever arm pivoting from the centre of the cell, handle ball at the top.
8. **Fluted knob, red bakelite**: as 1, in oxblood red phenolic. For the one dangerous control per panel (DRIVE).
9. **Fluted knob, bronze**: as 1, turned bronze. For HATS OFF's METAL section.
10. **Pointer skirt**: an empty stamped-steel dial skirt (a ring with no markings) that sits UNDER a knob.
    The code draws the numbers around it.
11. **Detent plate**: a round steel plate with 12 evenly spaced punched notches around the edge, no text.
12. **Empty cell** (leave transparent).

Prompt to paste:

> A sprite sheet of vintage industrial machine control knobs, photographed from directly above on a
> transparent background. Canvas 2048 x 1536, a 4 x 3 grid of 512 x 512 cells, one part per cell, each
> part centred and filling its cell. Every knob's pointer or handle points straight up. Row 1: a large
> black fluted bakelite knob with a polished aluminium cap and white engraved pointer line; the same knob
> smaller; a five-lobed black bakelite clamping star knob; a long tapered bakelite chicken-head pointer
> knob. Row 2: a four-spoke cast-iron machine handwheel with a turned wooden spinner handle, one spoke
> pointing up; a small knurled steel set knob with a scribed line; a heavy crank lever arm pivoting from
> the centre with a ball handle at the top; an oxblood-red fluted bakelite knob. Row 3: a turned bronze
> fluted knob; a plain stamped-steel dial skirt ring with no markings; a round steel plate with 12 punched
> notches around its rim and no text; an empty cell. Worn and used: scratches, polish where fingers go,
> grime in the flutes. Soft light from the upper left, identical for every part. Photographic. No text,
> no numbers, no logos anywhere.

## SHEET 2 - switches, buttons and the two hero buttons (shared)

Canvas **2048 x 2048**, a 4 x 4 grid of **512 x 512** cells, transparent. **Columns 1-2 are one
registered pair, columns 3-4 another** (rule 4): the housing identical, only the moving part changes.

- Row 1: **bat-handle toggle switch**, handle UP | the same, handle DOWN || **guarded toggle**: a toggle
  under a hinged red safety cover, cover CLOSED | cover flipped OPEN, toggle visible.
- Row 2: **THE HIT BUTTON**, a big industrial palm push-button, a fat black mushroom head in a heavy
  chromed collar on a cast base, UP | PRESSED (head lower, collar shadow tighter) || **THE E-STOP**, a red
  emergency-stop mushroom on a yellow collar ring (no text on the ring), UP | PRESSED. On BEET it is PANIC.
- Row 3: **square push-button** in a steel bezel, black cap, UP | PRESSED || **rotary cam switch**: a
  black T-bar handle on a square steel plate, handle pointing to 12 | handle turned 90 degrees to 3.
- Row 4: **slide lever** in a slot (a short heavy lever in a machined steel slot), lever at the TOP | lever
  at the BOTTOM || **key switch**: a steel lock barrel with a key, key upright | key turned 90 degrees.

Prompt to paste:

> A sprite sheet of vintage industrial machine switches and push-buttons, photographed from directly
> above, transparent background. Canvas 2048 x 2048, a 4 x 4 grid of 512 x 512 cells, each part centred
> and filling its cell. The sheet is made of two-state PAIRS: in each pair the housing, bezel and screws
> are pixel-identical and only the moving part changes. Row 1: a bat-handle toggle switch up, then down;
> a toggle under a hinged red safety cover, cover closed, then cover open. Row 2: a big industrial palm
> push-button with a fat black mushroom head in a heavy chrome collar on a cast base, released, then
> pressed down; a red emergency-stop mushroom button on a plain yellow collar ring, released, then pressed.
> Row 3: a square black push-button in a steel bezel, released, then pressed; a black T-bar rotary cam
> switch on a square steel plate, pointing up, then turned to the right. Row 4: a short heavy lever in a
> machined steel slot, at the top, then at the bottom; a steel key switch, key upright, then turned right.
> Worn factory equipment, scratches and grime, photographic, soft light from the upper left identical for
> every part. No text, no numbers, no logos, nothing written on the yellow ring.

## SHEET 3 - lamps and gauges (shared)

Canvas **2048 x 1536**, 4 x 3 grid of **512 x 512**, transparent.

- Row 1: **jewel pilot lamp**, faceted glass in a chrome bezel, AMBER OFF | AMBER ON (glowing, light
  spilling onto the bezel) | RED OFF | RED ON. Pairs registered.
- Row 2: GREEN OFF | GREEN ON | **bulls-eye signal lamp** (a larger domed lamp, factory style) OFF | ON.
- Row 3: **pressure gauge**: a round gauge in a heavy brass bezel with glass, a blank white dial face with
  NO numbers and NO needle | **the needle alone**, a black spade-tipped needle with a brass hub, pointing
  straight up, on transparent | **rectangular panel meter** housing with glass and a blank cream dial, no
  needle | **its needle alone**, pointing up.

Prompt to paste:

> A sprite sheet of vintage factory indicator lamps and gauges, photographed from directly above,
> transparent background. Canvas 2048 x 1536, a 4 x 3 grid of 512 x 512 cells, one part per cell,
> centred and filling it. Row 1: a faceted amber jewel pilot lamp in a chrome bezel, unlit, then lit and
> glowing; the same lamp in red, unlit, then lit. Row 2: the same lamp in green, unlit, then lit; a larger
> domed bulls-eye factory signal lamp, unlit, then lit. In each unlit/lit pair the bezel is pixel-identical.
> Row 3: a round pressure gauge with a heavy brass bezel and glass, the dial face blank white with no
> numbers and no needle; a lone black spade-tipped gauge needle with a brass hub pointing straight up;
> a rectangular panel meter with glass and a blank cream dial, no needle; a lone thin meter needle pointing
> straight up. Worn, photographic, soft light from the upper left. No text, no numbers.

## SHEET 4 - labels and plates (shared)

Canvas **2048 x 1024**, transparent.

- **Embossed label tape** (the old hand-punched kind): three blank strips, black, red and olive, each
  **1024 x 128**, tiling HORIZONTALLY (no end cuts; the code cuts it to length). Blank, the code embosses the
  text.
- **Blank riveted enamel plate**, 512 x 256, black enamel, four rivets, chipped corners, no text.
- **Blank engraved brass plate**, 512 x 256, four screws, no text.
- **Blank stencil plate**, 512 x 256, olive steel, no text.

Prompt to paste:

> A sprite sheet of blank vintage machine labels on a transparent background, canvas 2048 x 1024.
> Top: three long strips of embossed plastic label tape, black, red and olive, each 1024 x 128, blank,
> seamless so they can repeat horizontally, no cut ends. Bottom row, each 512 x 256: a blank black enamel
> plate with four rivets and chipped corners; a blank engraved brass plate with four screws; a blank
> olive-drab steel plate. Worn, photographic, soft light from the upper left. Absolutely no text or
> letters on anything.

## SHEETS 5-8 - one per machine: its paint and its nameplate

Each: a **1024 x 1024 seamless tiling ground** (rule 6) and **the nameplate** on transparent, on separate
canvases (ask for two images).

**5 KICKSTART** - ground: red-lead oxide primer on cast iron, sand-cast texture, worn through to dark
iron in patches. Nameplate 1600 x 400: the word **KICKSTART** in raised cast-iron letters on a cast plate,
the letters' tops polished bright by wear, painted red-lead background.

**6 SNARE TACTICS** - ground: olive drab military paint on steel, scuffs, a faint stencil ghost is NOT
allowed (rule 7). Nameplate 1600 x 400: **SNARE TACTICS** in white military stencil letters on an olive
steel plate, four bolts.

**7 HATS OFF** - ground: machine-tool grey-green enamel (the colour of an old lathe), oil stains, a few
bronze turnings. Nameplate 1600 x 400: **HATS OFF** deeply engraved and filled black in a polished brass
plate, four screws.

**8 BEET** - ground: hammer-finish grey-blue enamel of an electrical control cabinet. Nameplate 1600 x 400:
**BEET** in white on a black enamel sign, chipped, four rivets. Plus a third part: **a machine-bay frame**,
1024 x 1400, transparent: a recessed steel bay with a heavy riveted frame and a hinged-door edge, empty in
the middle. The code places eight of them, one per slot.

Prompt to paste (edit the machine each time):

> Two images. First: a seamless, tileable 1024 x 1024 texture of [red-lead oxide primer on sand-cast
> iron, worn through to dark iron in patches]. No edges, no vignette, no screws, no features that would
> repeat visibly, no text. Photographic. Second, on a transparent background, 1600 x 400, filling the
> canvas: a nameplate reading exactly [KICKSTART], [in raised cast-iron letters with wear-polished tops on
> a red-lead painted cast plate]. Spell it exactly. Soft light from the upper left.

---

## Where they go

Beet uses all of it. The three drums use Sheets 1-4 plus their own machine sheet, so re-dressing them
later is a reskin of an existing panel (the geometry does not move), not a redesign. Beet shows each
drum's OWN panel when a slot is selected, so a reskinned drum looks the same inside Beet at no extra cost;
Beet's own art is only its kit strip (Sheet 8's cabinet ground, nameplate and bays, plus the shared parts). Each panel has ONE
hero control (a handwheel or the HIT button) and one red knob (DRIVE); everything else is bakelite. A
panel of 21 to 31 controls built entirely from handwheels would be unusable, and the scale is what makes
the heroes read as heroes.
