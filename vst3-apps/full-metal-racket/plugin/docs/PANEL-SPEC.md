# FULL METAL RACKET — panel geometry, and what to order from ChatGPT

Measured from the live panel (build 260826.1) over CDP, not estimated.
All coordinates are in **deck pixels on a 1680 × 900 canvas**. The deck
scales to fit whatever window it is in, so these are the design units.

---

## 1. One honest thing first

**An image generator cannot hit exact pixel coordinates.** Asking for a
complete faceplate with 96 knob holes registered to the table below will
come back looking right and being 20–40 px out everywhere, and no amount of
prompt detail fixes it — that is not what the models are good at.

So the division of labour is:

* **I build every interactive part in CSS/SVG** — knobs, pointers, lamps,
  selectors, pads. They have to be live, themeable and pixel-exact.
* **ChatGPT makes the raster decals**: flat, isolated, single objects on
  transparent backgrounds. Those it does superbly, and I composite them at
  the coordinates below.

The geometry table is still what you want to hand over, because it fixes
the **aspect ratio and pixel size** of each asset. Ask for each at 4× the
size given, PNG with alpha, flat-on, no perspective, no drop shadow.

---

## 2. The deck

| Region | x | y | w | h |
|---|---|---|---|---|
| Deck (whole machine) | 0 | 0 | 1680 | 900 |
| Walnut cheek, left | 0 | 0 | 26 | 900 |
| Walnut cheek, right | 1654 | 0 | 26 | 900 |
| Faceplate (cream steel) | 26 | 0 | 1628 | 900 |
| Header row | 40 | 12 | 1600 | 56 |
| Global strip (THE WEB) | 40 | 78 | 1600 | 76 |
| Channel strips | 40 | 165 | 1600 | 706 |
| Footer legend | 40 | 881 | 1600 | 9 |

## 3. The twelve channel strips

Strip *i* (i = 0…11) has **left = 40 + i × 133.75**, **width 128.8**,
**top 165**, **height 706**. Order left to right:

`BD 1 · BD 2 · SD 1 · SD 2 · TOM 1 · TOM 2 · TOM 3 · FX · CH · OH · CY 1 · CY 2`

Positions **relative to a strip's own top-left**:

| Element | x | y | w | h | note |
|---|---|---|---|---|---|
| Channel name | centred | 6 | — | 15 | 15 px bold condensed |
| Family label | centred | 24 | — | 7 | 7 px mono, letterspaced |
| MODEL selector | 5 | 43 | 118.8 | 21 | |
| Knob TUNE | cx 33.2 | cy 168 | ⌀54 | | row 1, left |
| Knob DECAY | cx 95.6 | cy 168 | ⌀54 | | row 1, right |
| Knob TONE | cx 33.2 | cy 316 | ⌀54 | | row 2, left |
| Knob SNAP | cx 95.6 | cy 316 | ⌀54 | | row 2, right |
| Knob BEND | cx 33.2 | cy 464 | ⌀54 | | row 3, left |
| Knob DRIVE | cx 95.6 | cy 464 | ⌀54 | | row 3, right |
| Knob LEVEL | cx 33.7 | cy 605 | ⌀54 | | |
| Knob PAN | cx 95.1 | cy 605 | ⌀54 | | |
| MUTE button | 5 | 662 | 30 | 22 | |
| Trigger pad | 39 | 648 | 84.8 | 50 | dark, amber lamp centred |

Every knob label sits 8 px tall, centred, immediately under its knob.

## 4. The global strip

Knob centres all at **y = 109**, ⌀46, absolute x:

| Control | cx |
|---|---|
| RAIL SAG | 85 |
| KIT BODY | 163 |
| BLEED | 241 |
| AGE | 319 |
| KIT TUNE | 397 |
| HAT LINK (button, 78 × 34) | 484 |

The rest of the strip is legend text, right-aligned.

## 5. The header

Left to right: nameplate badge, KIT selector, RANDOM, PANIC, OS selector,
MASTER knob (⌀46), the BWFX globe (⌀36), then a status line. Laid out by
flow rather than by coordinate, because the kit names change width.

---

## 6. What to order — five prompts, ready to paste

Ask for PNG with transparency, flat-on, orthographic, no drop shadow, no
perspective, no background. Sizes are 4× the deck pixels.

**1 · Panel texture** (tileable, becomes the faceplate ground)
> A seamless, perfectly flat-on photographic texture of a 1980s Japanese
> drum machine's painted steel front panel in warm cream-beige (#e7dcc3).
> Very slight orange-peel paint grain, faint even wear, no lettering, no
> hardware, no screws, no shadows, no perspective, evenly lit, fully
> tileable. 4096 × 4096.

**2 · Knob cap** (replaces my CSS knob; must be a perfect circle, centred)
> A single flat-on, top-down photograph of a small silver aluminium
> synthesiser knob cap: fine concentric brushing on the top face, a knurled
> milled skirt, and one flat matte-red pointer line running from the centre
> to the edge. Perfectly circular, perfectly centred, transparent
> background, no shadow, no perspective, no panel behind it. 1024 × 1024.

**3 · Walnut cheek** (the end panels — final size 26 × 900, so 104 × 3600)
> A flat-on photograph of a solid walnut end cheek for a 1980s synthesiser:
> warm mid-brown, straight open grain running vertically along the long
> axis, one softly rounded front edge with a slight highlight, satin oil
> finish. Tall and narrow. No hardware, no shadow, no perspective.
> 104 × 3600.

**4 · Nameplate badge** (final 232 × 56, so 928 × 224)
> A rectangular brushed-aluminium badge, flat-on: horizontal brushing, a
> thin bevelled edge catching a soft highlight, and two tiny countersunk
> screw holes at the left and right ends. Blank — no lettering at all.
> Transparent background, no shadow, no perspective. 928 × 224.

**5 · Trigger pad** (final 84.8 × 50, so 340 × 200)
> A flat-on photograph of a small rectangular rubber drum-machine trigger
> pad with slightly rounded corners: matte dark charcoal rubber with a fine
> stippled surface, a subtle chamfered edge catching one soft highlight
> along the top. Blank centre, no lettering, no lamp. Transparent
> background, no shadow, no perspective. 340 × 200.

**Do not order**: the lettering (silkscreen legends are live text, so they
stay crisp at any zoom and can change with the model), the LEDs, the
selector boxes, or any composite "whole panel" image.

---

## 7. Colours, so the assets match what is already on screen

| Token | Hex | Used for |
|---|---|---|
| panel | `#e7dcc3` | faceplate ground |
| panel-hi | `#f4eddc` | lit top edge |
| panel-lo | `#d3c6a6` | strip fill, shadowed cream |
| ink | `#1b1509` | silkscreen black |
| ink2 | `#5f5138` | secondary legends |
| wood | `#6d4526` | walnut cheek mid-tone |
| amber | `#ff8f1f` | lamps |
| red | `#d8412a` | knob pointers, accents |
| teal | `#33bfa9` | the BWFX globe, and nothing else |
