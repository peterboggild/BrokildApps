# 1984 — decal order for ChatGPT

*The panel is complete without these. Each decal slots into a CSS hook the
page already has, and the page falls back to its own drawing if a file is
missing or fails to load. So the order is optional per part, and any part can
be redone alone.*

## The look, in one paragraph (paste this before every prompt)

> A late night in 1984 in a small film-score studio. The instrument is a
> black anodised aluminium deck, matte, with a very fine brushed grain, cream
> silkscreen lines and lettering, and long-throw slider caps in solid
> colours — oxblood red, amber, ivory, teal, slate blue. Chrome toggle
> switches, an amber vacuum-fluorescent display behind smoked glass, two
> small tape reels in a window, walnut end cheeks with an oiled finish. No
> logos of any real manufacturer. No text unless the prompt says so.
> Photographic realism, studio product lighting from the top left, no
> dramatic reflections, no lens flare. Every part is delivered ISOLATED on a
> transparent background (PNG with alpha), cropped tight to the object with
> no letterboxing, at the exact aspect ratio stated.

## Delivery rules (they matter — two earlier orders came back unusable)

1. **One object per file, cropped to its own bounding box.** Do not paste a
   drawing into a larger canvas and leave black or transparent margin: the
   page scales each part to its slot by the file's edges. (High Tide's first
   delivery was letterboxed and four parts had to be rescued; Rite of
   Passage's wordmark could not be.)
2. **Aspect ratio is not free.** Resolution is (everything is rescaled) but a
   part drawn at the wrong aspect is stretched. Anything round needs a square
   crop.
3. **Textures must tile**: no end caps, no vignette, no visible seam when
   repeated. A texture with rounded ends reads as a stack of blocks.
4. **Transparent means transparent.** An "empty" glass or a lamp OFF is still
   an object with an opaque body — do not return a near-fully transparent
   frame (Battlestar's empty fuel tube had mean alpha 18/255 and let the
   panel show through).
5. **Measure before saying "deviations: none"**: the non-transparent bounding
   box of each file must equal the canvas.
6. Deliver `DELIVERED.md` beside the files listing each file, its pixel size,
   and any deviation.

## The parts

Folder: `assets/decals/`. Hooks are CSS variables on `:root` in
`Source/ui/ui.html`; the page reads `--decal-<name>` and switches the part on
only when the image loads.

| # | file | size (px) | aspect | hook | what it is |
|---|---|---|---|---|---|
| 1 | `panel.png` | 1024×1024 | 1:1, **tiles** | `--decal-panel` | the black anodised deck: matte, brushed grain barely visible, very slight variation, no edges, no screws, no text |
| 2 | `cheek-left.png` | 300×1400 | 3:14 | `--decal-cheek` | a walnut end cheek, vertical grain, oiled satin finish, front face only (the page mirrors it for the right side) |
| 3 | `cap-red.png` | 128×256 | 1:2 | `--decal-cap-red` | a slider cap seen straight on: oxblood red plastic, a fine white index line across the middle, softly bevelled, a faint highlight top-left |
| 4 | `cap-amber.png` | 128×256 | 1:2 | `--decal-cap-amber` | the same cap in amber |
| 5 | `cap-ivory.png` | 128×256 | 1:2 | `--decal-cap-ivory` | the same cap in ivory |
| 6 | `cap-teal.png` | 128×256 | 1:2 | `--decal-cap-teal` | the same cap in teal |
| 7 | `cap-slate.png` | 128×256 | 1:2 | `--decal-cap-slate` | the same cap in slate blue |
| 8 | `track.png` | 32×1024 | 1:32 | `--decal-track` | a slider slot: a dark recessed groove with a hairline chrome lip each side, no end caps (the page clips it to length) |
| 9 | `knob.png` | 512×512 | 1:1 | `--decal-knob` | a knob seen exactly from above: black skirt, chrome ring, a cream pointer line from the centre to the top edge (pointing straight UP — the page rotates it) |
| 10 | `toggle-up.png` | 128×192 | 2:3 | `--decal-toggle-up` | a chrome toggle switch lever pointing UP, with its round bezel, seen straight on |
| 11 | `toggle-down.png` | 128×192 | 2:3 | `--decal-toggle-down` | the same lever DOWN, same bezel position (the two must register: same bezel pixels) |
| 12 | `vfd-glass.png` | 1600×320 | 5:1 | `--decal-vfd` | the smoked glass window of a vacuum-fluorescent display: dark tinted glass with a subtle top reflection and a thin bezel; **no lettering inside** (the page draws the readout) |
| 13 | `reel.png` | 512×512 | 1:1 | `--decal-reel` | one tape reel from above, transparent between the spokes, a hub with three spokes, a fine tape edge on the flange, no tape wound on |
| 14 | `lamp-on.png` | 96×96 | 1:1 | `--decal-lamp-on` | a small round amber indicator lamp, lit, with a soft halo inside the crop |
| 15 | `lamp-off.png` | 96×96 | 1:1 | `--decal-lamp-off` | the same lamp unlit: dark amber glass, still opaque |
| 16 | `wordmark.png` | 1200×360 | 10:3 | `--decal-wordmark` | the numerals **1984** in a heavy geometric sans, cream, engraved-and-filled look, on transparent; nothing else |
| 17 | `screw.png` | 64×64 | 1:1 | `--decal-screw` | one Phillips screw head, chrome, seen from above |

## What NOT to draw

- No slider tracks with caps drawn into them, no knobs with value scales
  around them, no labels: the page draws every scale and every word.
- No shadows that leave the crop (they become a rectangle on a dark panel
  when composited).
- Nothing photographed at an angle: every part is straight-on.

## Verification the page does

Each hook is tested with an `Image` load; `naturalWidth/naturalHeight` must
match the aspect in the table within 2 %, else the part is refused and the
procedural drawing stays. `tools/decal-audit.ps1` (High Tide's) reports the
content bounding box of every delivered file against its canvas.
