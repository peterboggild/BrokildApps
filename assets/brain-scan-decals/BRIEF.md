# BRAIN SCAN — panel decal brief

Paste this whole file to ChatGPT as one message. It is self-contained: the
style rules, all twelve parts with exact pixel sizes and filenames, what each
part becomes on the panel, and where to commit them.

---

## The task

Generate twelve photographic component decals for the front panel of a
software synthesiser called **BRAIN SCAN**, then commit them to GitHub. One
isolated object per image, PNG with transparency (the two textures and the
cover are the exceptions, and say so).

## The machine

Brain Scan is a synthesiser that reads its waveforms out of a three-
dimensional volume — a CT scan — along lines the player draws through it. The
scan itself, the slices, the glowing lines, the waveform traces and every
piece of text are drawn live by code. What the code cannot draw is a real
material, and that is what these parts are: isolated photographic pieces that
it composites, scales and rotates.

The panel is **a CT scanner console from a good hospital, about 1995**, and
still, unmistakably, a musical instrument. Warm off-white powder-coated
enclosure `#e6e3da`, the kind that has been wiped down every day for years.
Cool grey fittings `#9aa0a6`. Dark charcoal knobs and buttons `#2b2e33` with
white index marks. One accent, a clinical signal blue `#2b6fb3`, on a stripe
and on the lit buttons. The screens are near-black glass `#0d1114`; what
glows on them — phosphor green traces `#39ff88`, bone-white CT slices, amber
warnings `#ffb347` — is all painted by code. A single red mushroom STOP
button. Think Siemens, GE and Philips consoles of that era, an anaesthesia
monitor, a laboratory centrifuge: **clean, calm, well-made, a little dated,
utterly trustworthy.** Not sci-fi, not a spaceship, no chrome, no neon, no
hexagons. No branding and no lettering anywhere unless a part explicitly asks
for it; exactly one part does.

## Rules that apply to EVERY part, without exception

- Perfectly **flat-on and orthographic**. No perspective, no tilt, no angle.
  (Part 12 is a painting and is the only part allowed a camera angle.)
- **Transparent background** (PNG with alpha). Nothing behind the object.
  The two textures in Group A and the cover (12) are opaque and fill their
  frame instead.
- **No drop shadow and no cast shadow of any kind.** Self-shading on the
  object itself is wanted; shadow falling away from it is not. The panel
  lights the parts itself.
- The object **fills the frame edge to edge** at the exact pixel dimensions
  given, with no padding or margin unless the part says otherwise.
- Even, soft, slightly cool light from above — fluorescent ceiling light in a
  scanner room, diffused. Plastics read as satin, metal as brushed: no hot
  specular bursts, no reflections of a room, no fingerprints.
- Photoreal. Not illustration, not stylised, not obviously 3D-rendered.
  (Part 12 is painterly and says so.)
- Deliver at **exactly** the pixel size given. Aspect ratio matters most — a
  part delivered at the wrong aspect gets squashed on the panel and it shows
  immediately.
- Anything described as **perfectly circular** gets rotated or scaled by code.
  It must be a true circle, exactly centred, touching all four edges of its
  square. A 3 % offset makes a knob wobble as it turns.

---

## GROUP A · grounds

*These two are textures, not objects: opaque, no transparency needed. Each
must tile seamlessly, with completely uniform brightness across the image and
no vignette, because the panel repeats them. An end cap, a rounded corner or a
shaded edge is what makes a texture stop being tileable — the surface must run
straight off all four edges, cut flat.*

### 01 · Enclosure ground — `bs-ground.png` — **2048 × 2048**, tileable

A seamless tileable photographic texture of powder-coated steel, warm
off-white `#e6e3da`, of the kind medical equipment enclosures are finished in,
seen from directly above.

The fine, even, slightly pebbled texture of a powder coat — just visible, no
coarser than that. A faint warmth, as years of cleaning give it; **no
scratches, no scuffs, no stains, no screws, no seams, nothing** that would
repeat visibly when tiled. Completely uniform overall brightness across the
whole image so the tile seams disappear.

No objects, no lettering, no edges, no vignette. Flat-on, orthographic,
evenly lit, fully seamless and tileable in both directions.

### 02 · Ventilation grille — `bs-grille.png` — **2048 × 256** (very wide and short), tileable left to right

A seamless, **HORIZONTALLY TILEABLE** photograph of a ventilation grille strip
from the side of an equipment enclosure: a row of short **vertical** slots
punched into the same off-white `#e6e3da` powder-coated steel, dark inside
each slot, evenly spaced along the whole length.

**CRITICAL — this is a TEXTURE, not an object:**

- It must tile seamlessly **LEFT TO RIGHT**: the slot spacing must divide the
  width exactly, so the pattern continues across the join.
- **NO rounded ends, NO end caps, NO border at the left or right.** The steel
  and the slots must run straight off both short edges, cut flat.
- No vignette and no shading that changes along the length.
- The top and bottom long edges are plain steel, no border.

Opaque; no transparency needed. No lettering, no shadow, no perspective.
Please keep the exact 2048 : 256 aspect.

---

## GROUP B · fittings

*All transparent background. Charcoal plastics `#2b2e33`, satin; brushed
aluminium where metal is called for.*

### 03 · Monitor bezel — `bs-bezel.png` — **1600 × 1000** (landscape)

A single flat-on photograph of the bezel of a 1990s medical CRT monitor —
**the frame only, with nothing inside it**.

A moulded frame in cool grey plastic `#9aa0a6`, rounded outer corners, a
gently bevelled inner lip that steps down toward the screen opening, evenly
lit. The frame band is **80 px wide on all four sides**, so the transparent
opening is 1440 × 840, centred, with slightly rounded inner corners. No
buttons, no logo, no lettering, no LED, no screws on the front face.

**CRITICAL:** the **opening must be fully transparent** — real alpha, not a
painted black rectangle — because the scan is drawn by code inside it. The
frame is sliced by code (top, bottom, sides and corners separately) to fit
screens of different sizes, so **the band must be exactly the same width and
finish all the way round** and the corners must be symmetric. Transparent
outside the frame as well. No shadow, no perspective, no wall behind it.

### 04 · Control knob — `bs-knob.png` — **512 × 512**

*The most-repeated part on the panel — every rotary control is one of these.*

A single flat-on, top-down photograph of a rotary control knob from a piece
of medical or laboratory equipment: charcoal `#2b2e33` satin plastic, seen
from directly above.

A flat top face with a shallow finger dimple or a slightly raised centre, a
**ribbed or finely knurled outer grip** catching a ring of small soft
highlights, and one crisp **white index line** running from near the centre
straight up to the rim, pointing to **12 o'clock**. Plain otherwise: no
numbers, no scale (the scale is printed on the panel by code), no logo.

It must **NOT** have radial spoke-like lines across the top face and must
not look like a fan or a starburst: at panel size those collapse into a
rotating pattern. If in doubt, make the face plainer.

**CRITICAL:** perfectly circular, perfectly centred in the frame, the circle
touching all four edges. The index line must be exactly vertical and exactly
on the centre — this image gets **rotated**, so any offset makes the knob
wobble as it turns. Transparent background, no shadow, no perspective, no
panel behind it.

### 05 · Fader cap — `bs-fader.png` — **160 × 320** (portrait)

A single flat-on, top-down photograph of the cap of a linear slider (a fader
cap) from a piece of equipment: charcoal `#2b2e33` satin plastic, a
rectangular cap with softly rounded corners, slightly domed, and one **white
index line running horizontally across its middle**.

The cap fills the frame edge to edge. It slides **vertically** on the panel,
so the frame is portrait: the cap is taller than it is wide. No lettering, no
screw. Transparent background, no shadow, no perspective, no slot behind it.

### 06 · Membrane button, unlit — `bs-button.png` — **256 × 256**

A single flat-on photograph of a **square** membrane button with softly
rounded corners, as found on a scanner console or a laboratory instrument:
off-white `#e6e3da` with a faint satin sheen, a very shallow dome, and a
**small rectangular indicator window** near its top edge, dark and unlit.
The square fills the frame edge to edge. No lettering (the caption is printed
below it by code), no icon.

Transparent background, no shadow, no perspective, no panel behind it.

### 07 · Membrane button, lit — `bs-button-lit.png` — **256 × 256**

**The same button as 06, identical in every way** — same shape, same position
in the frame, same lighting — except that the indicator window is now **lit
signal blue `#2b6fb3`**, glowing softly with a little bloom onto the button's
face. Code swaps 06 for 07 when the button is on, so the two must register
pixel for pixel: please make it from the same image.

### 08 · STOP button — `bs-stop.png` — **512 × 512**

A single flat-on, top-down photograph of a red mushroom-head emergency stop
button `#d1352b`, of the kind fitted to industrial and medical equipment,
seen from directly above.

The domed red head fills the circle, satin, with one soft highlight above
centre; around it a narrow **yellow collar** ring at the very edge. No
lettering on the head (no "STOP" text — it is printed by code beside it), no
arrows.

**CRITICAL:** perfectly circular, perfectly centred, the circle touching all
four edges of the frame. Transparent background, no shadow, no perspective.

### 09 · Nameplate — `bs-nameplate.png` — **1600 × 360**

*The one part that carries lettering.*

A flat-on photograph of a rectangular **model plate** of the kind riveted to
the front of a scanner console: brushed aluminium, a narrow radiused edge
catching a bright line along the top and a shadow line along the bottom,
four small round rivet heads, one near each corner, all four the same. The
plate fills the frame edge to edge.

Printed on the face in **dark charcoal `#2b2e33`, a clean geometric sans-
serif capital letterform** (the register of Helvetica or Univers as used on
1990s medical equipment), the words **BRAIN SCAN** — exactly those two words,
spelled exactly so, evenly spaced, centred, filling most of the plate's
width. Below the words, centred and much smaller, **a single thin printed
line with one small ECG-style pulse in the middle of it** — flat, one sharp
spike up, one small dip, flat again. Screen-printed, crisp, flat in the
surface — not raised, not engraved.

**CRITICAL:** the lettering must read exactly "BRAIN SCAN" and nothing else —
no maker's name, no model number, no serial, no CE mark, no date. Flat-on,
orthographic, transparent background, no drop shadow, no perspective.

### 10 · Status lamp, unlit — `bs-lamp.png` — **128 × 128**

A single flat-on photograph of a small round panel indicator lamp — a lens in
a thin chrome bezel — **unlit**, the lens a dark translucent amber-grey.

**CRITICAL:** perfectly circular, perfectly centred, the circle touching all
four edges of the frame. Transparent background, no shadow, no perspective.

### 11 · Status lamp, lit — `bs-lamp-lit.png` — **128 × 128**

**The same lamp as 10, identical in every way** — same bezel, same position
in the frame, same lighting — except that the lens is now **lit warm amber
`#ffb347`**, glowing with a small soft bloom that stays inside the frame.
Code swaps 10 for 11, so the two must register pixel for pixel: please make
it from the same image.

---

## GROUP C · cover

### 12 · Manual cover — `bs-cover.png` — **1600 × 900** (landscape), opaque

*The one painterly part. This is an illustration, not a component, and it is
exempt from the flat-on and transparent-background rules — but not from the
no-text rule.*

A painting for the cover of the instrument's manual: a dim scanner room at
night, the great white ring of a CT gantry seen from a **low three-quarter
angle**, the patient table empty, and inside the ring — floating where the
head would be — a translucent human brain rendered the way a CT volume is
rendered: bone-white and pale blue, luminous, with a few **thin glowing lines
threaded through it**, green and amber, like traces on a monitor. On the
console at the edge of the frame, the glow of a screen carrying a waveform.

Painterly and atmospheric, mostly dark, the light coming from the brain and
the screens. The palette is the panel's: off-white, cool grey, signal blue,
phosphor green, amber, one point of red. **Opaque, fills the frame, no text,
no title, no lettering, no signature, no logos** — the title is set on top by
code. Exact 16 : 9.

---

## What these become

So the priorities make sense — this is where each part lands on the panel:

| part | becomes |
|---|---|
| 01 `bs-ground.png` | the enclosure behind everything, tiled |
| 02 `bs-grille.png` | the ventilation strip along the foot of the panel, tiled left to right |
| 03 `bs-bezel.png` | the frame around the scan viewport and the slice monitor; the screens inside are drawn by code |
| 04 `bs-knob.png` | every rotary control, rotated by code |
| 05 `bs-fader.png` | the cap of every slider (DENSITY, TABLE, the mixer) |
| 06 / 07 `bs-button.png` / `bs-button-lit.png` | every membrane button — AXIAL / CORONAL / SAGITTAL, the filter modes, the line selectors |
| 08 `bs-stop.png` | the STOP (all notes off) |
| 09 `bs-nameplate.png` | the header |
| 10 / 11 `bs-lamp.png` / `bs-lamp-lit.png` | SCAN, VOICE and CLIP lamps |
| 12 `bs-cover.png` | the manual cover |

---

## Where to commit them

Repository **peterboggild/BrokildApps**, branch `main`, new folder:

```
assets/brain-scan-decals/
```

Filenames exactly as given above — the panel looks them up by name, and a
misspelling means that part silently falls back to the drawn version.

Alongside the PNGs, write a **`DELIVERED.md`** in the same folder listing
filename → what it is, with a fenced ```json block in this shape. Prose
around it is welcome — candid notes about which parts came out weaker, or
where a crop is approximate, are the most useful thing you can add — but the
block is what gets read:

```json
{
  "version": 1,
  "set": "brain-scan-decals",
  "machine": "Brain Scan",
  "files": [
    {"file": "bs-ground.png",     "size": [2048, 2048], "tileable": true,  "what": "Off-white powder-coated enclosure ground"},
    {"file": "bs-grille.png",     "size": [2048, 256],  "tileable": true,  "what": "Ventilation grille strip, tiles left to right"},
    {"file": "bs-bezel.png",      "size": [1600, 1000], "tileable": false, "what": "Grey CRT monitor bezel, open centre, 80 px band"},
    {"file": "bs-knob.png",       "size": [512, 512],   "tileable": false, "what": "Charcoal control knob, white index at 12"},
    {"file": "bs-fader.png",      "size": [160, 320],   "tileable": false, "what": "Charcoal fader cap, white index across"},
    {"file": "bs-button.png",     "size": [256, 256],   "tileable": false, "what": "Square membrane button, unlit window"},
    {"file": "bs-button-lit.png", "size": [256, 256],   "tileable": false, "what": "Same button, window lit signal blue"},
    {"file": "bs-stop.png",       "size": [512, 512],   "tileable": false, "what": "Red mushroom STOP button, yellow collar"},
    {"file": "bs-nameplate.png",  "size": [1600, 360],  "tileable": false, "what": "Brushed aluminium model plate, BRAIN SCAN"},
    {"file": "bs-lamp.png",       "size": [128, 128],   "tileable": false, "what": "Round panel lamp, unlit"},
    {"file": "bs-lamp-lit.png",   "size": [128, 128],   "tileable": false, "what": "Same lamp, lit amber"},
    {"file": "bs-cover.png",      "size": [1600, 900],  "tileable": false, "what": "Manual cover painting, opaque"}
  ]
}
```

**Please append to `DELIVERED.md` as you go** — write each part's entry as
soon as that file is committed, rather than saving it all for the end. If the
run stops halfway, that file is what lets it resume instead of starting over,
and it lets me wire up whatever has arrived without waiting for the rest.

## If you can only do a few

Do **04, 01, 03 and 09** first, in that order. The knob is the most-repeated
fitting; the ground is what stops the off-white looking like a flat fill; the
bezel is what makes the screens read as screens; and the nameplate is the only
part that cannot be drawn passably by code.

---

## Combining parts into sheets

**Combining is welcome.** Several related parts on one sheet is fine and saves
you time. Two things make it work, and one of them I cannot guess.

### Resolution does not matter. ASPECT RATIO does.

Every part gets scaled to fit its place on the panel, so a knob delivered at
400 px is as good as one at 512. What cannot be fixed by scaling is the
**shape**:

- Anything described as *perfectly circular* — the knob (04), the STOP (08),
  the lamps (10, 11) — must sit in a **square** crop region, centred. If its
  crop is 500 × 460 it becomes an oval on the panel and every knob wobbles as
  it turns.
- The rectangular parts must keep roughly the width-to-height ratio given for
  them. Within about 5 % is fine; the panel absorbs the rest. The bezel (03)
  is the exception: its band width matters, so please keep it standalone.
- Do not put two parts with different aspect ratios in one crop region.
- The two tileable textures (01, 02) and the cover (12) are better as
  standalone files — a texture on a sheet loses its seamless edges to the
  crop.
- The lit/unlit pairs (06/07, 10/11) must be **the same image with the light
  changed**, so if they go on a sheet, put each pair side by side at exactly
  the same size.

### Leave gutters

At least 32 px of **fully transparent** pixels between parts on a sheet, and
around the outside. That way a crop region that is a few pixels out still
clips nothing.

### Record sheets in the same `DELIVERED.md`

Add a `"sheets"` array beside `"files"`:

```json
{
  "sheets": [
    {
      "file": "sheet-fittings.png",
      "size": [2048, 1024],
      "parts": [
        { "name": "bs-knob",   "rect": [32, 32, 512, 512],  "shape": "circle" },
        { "name": "bs-stop",   "rect": [608, 32, 512, 512], "shape": "circle" },
        { "name": "bs-fader",  "rect": [1184, 32, 160, 320] }
      ]
    }
  ]
}
```

- `name` — the target filename from the list above, **without** `.png`.
- `rect` — `[x, y, width, height]` in that sheet's own pixels, top-left origin.
- `shape` — `"circle"` for the four that must be round; omit otherwise.

A part delivered as its own standalone file needs no sheet entry: just name
the file correctly and it is picked up as it is.

---

## What is worth a redo, and what is not

The ingest side crops each part, trims its transparent margin and then
**stretches it to the aspect the panel needs**, reporting the correction. So
a part whose *proportions* are off is not a defect — it is absorbed, and
anything over about 25 % gets flagged and I will ask for that one part
specifically. Please do not re-roll a whole sheet for it.

**DO redo for these, which cannot be fixed downstream:**

1. **A baked-in drop shadow.** It doubles against the panel's own lighting and
   there is no way to remove it from a flattened image.
2. **Perspective or any camera angle** on parts 01–11. The panel is dead
   flat-on; a part shot at an angle cannot be un-projected.
3. **A painted-in background where alpha was asked for** — a black rectangle
   inside the bezel (03), a panel behind a button. Code has to draw *through*
   those.
4. **An end cap on a texture** — a border on the grille (02) or a repeating
   blemish on the ground (01). It shows as a seam every tile.
5. **A lit/unlit pair that does not register** — a button or lamp whose lit
   version is a different photograph. The swap flickers.
6. **Wrong or missing content** — any lettering other than "BRAIN SCAN" on
   the nameplate, "STOP" printed on the button head, a logo on the bezel,
   text on the cover.
