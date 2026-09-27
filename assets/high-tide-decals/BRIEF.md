# HIGH TIDE — panel decal brief

Paste this whole file to ChatGPT as one message. It is self-contained: the
style rules, all twelve parts with exact pixel sizes and filenames, what each
part becomes on the panel, and where to commit them.

---

## The task

Generate twelve photographic component decals for the front panel of a
software synthesiser called **HIGH TIDE**, then commit them to GitHub. One
isolated object per image, PNG with transparency (the three ground textures
and the cover are the exceptions, and say so).

## The machine

High Tide is a wavetable synth with no stored waveforms. A mass rolls across a
sculpted heightmap terrain; a rising **tide** floods the passes between the
valleys; keyframe **pins** on a timeline tow the ball. None of that is your
concern — the terrain, the water, the ball, the contour lines, the pins'
positions, the sliders and every piece of text are drawn live by code. What
the code cannot draw is a real material, and that is what these parts are:
isolated photographic pieces that it composites, scales and rotates.

The panel is **a chart table at night.** A dark sea-ink ground `#0a141c`, like
worn leather or linen stretched over a navigator's table. Brass instrument
fittings — bright brass `#c9a24a` in the light, shadow brass `#8a6a2a` in the
turn. A cream tide-table paper strip `#e8dcc0` carrying the timeline. Accents
of teal water `#1fb5c4` and coral `#ff7a59` (those two are painted by code;
you will only meet the coral once, on the flag). Think Admiralty chart, brass
tide gauge, a Victorian instrument register — but **clean**. Not steampunk,
not cluttered, no gears, no rivets for their own sake. Well-kept, well-used
instruments on a quiet table. No branding and no lettering anywhere unless a
part explicitly asks for it; exactly one part does.

## Rules that apply to EVERY part, without exception

- Perfectly **flat-on and orthographic**. No perspective, no tilt, no angle.
  (Part 07 is a side elevation and part 12 is a painting; both still have no
  perspective distortion on the object itself, and 12 is the only part
  allowed a camera angle.)
- **Transparent background** (PNG with alpha). Nothing behind the object.
  The three textures in Group A and the cover (12) are opaque and fill their
  frame instead.
- **No drop shadow and no cast shadow of any kind.** Self-shading on the
  object itself is wanted; shadow falling away from it is not. The panel
  lights the parts itself.
- The object **fills the frame edge to edge** at the exact pixel dimensions
  given, with no padding or margin unless the part says otherwise.
- Even, soft, slightly warm light from slightly above — a shaded lamp over a
  table, not daylight. Brass reads as satin, not mirror: no hot specular
  bursts, no reflections of a room.
- Photoreal. Not illustration, not stylised, not obviously 3D-rendered.
  (Part 11 is engraved ink and part 12 is painterly; each says so.)
- Deliver at **exactly** the pixel size given. Aspect ratio matters most — a
  part delivered at the wrong aspect gets squashed on the panel and it shows
  immediately.
- Anything described as **perfectly circular** gets rotated or scaled by code.
  It must be a true circle, exactly centred, touching all four edges of its
  square. A 3 % offset makes a knob wobble as it turns and a pearl roll
  lopsided.

---

## GROUP A · grounds

*These three are textures, not objects: opaque, no transparency needed. Each
must tile seamlessly, with completely uniform brightness across the image and
no vignette, because the panel repeats them. An end cap, a rounded corner or a
shaded edge is what makes a texture stop being tileable — the surface must run
straight off all four edges, cut flat.*

### 01 · Chart-table ground — `ht-ground.png` — **2048 × 2048**, tileable

A seamless tileable photographic texture of the top of a navigator's chart
table: dark sea-ink `#0a141c`, worn leather or fine linen stretched over the
table, seen from directly above.

Very fine surface texture — the grain of the leather or the weave of the linen
just visible, and no coarser than that. Barely-there wear: a faint, even
softening as from years of elbows and charts, but **no scratches, no scuffs,
no stains, no creases, nothing** that would repeat visibly when tiled.
Completely uniform overall brightness across the whole image so the tile
seams disappear. The tone should stay very dark — this sits behind everything
and the brass and the paper have to stand off it.

No objects, no lettering, no edges, no stitching, no vignette. Flat-on,
orthographic, evenly lit, fully seamless and tileable in both directions.

### 02 · Tide-table paper — `ht-paper.png` — **2048 × 1024**, tileable

A seamless tileable photographic texture of cream laid paper `#e8dcc0`, of the
kind a nineteenth-century tide table was printed on, seen from directly above.

Fine fibre texture with the faint parallel laid lines of the mould visible
under raking light — subtle, not a pattern. A faint, sparse age foxing: a few
soft tan spots, small and irregular, spread so thinly that the repeat is not
obvious when tiled. Slightly warmer at the spots, otherwise uniform.

**NO ruled lines, NO printed grid, NO print of any kind, no watermark, no
torn or deckled edge.** The timeline's ticks and numbers are drawn by code on
top of this. Completely uniform overall brightness. Flat-on, orthographic,
evenly lit, fully seamless and tileable in both directions.

### 03 · Chart-table rail — `ht-wood.png` — **4096 × 256** (very wide and short), tileable left to right

A seamless, **HORIZONTALLY TILEABLE** photographic texture of dark oak, of the
kind used for the rail along the edge of a chart table. A very wide, short
strip, landscape.

Dark oak `#3d2b1a`, straight open grain running **horizontally along the long
axis**, satin oil finish, slight natural colour variation across the height.

**CRITICAL — this is a TEXTURE, not an object:**

- It must tile seamlessly **LEFT TO RIGHT**. The grain at the very right edge
  has to continue into the grain at the very left edge.
- **NO rounded ends, NO end caps, NO chamfer or bevel at the left or right.**
  The wood must run straight off both short edges, cut flat.
- No vignette and no shading that changes along the length — uniform
  brightness from end to end, or the repeat shows as banding.
- The **top and bottom long edges may carry a soft horizontal highlight**, as
  light catching a rounded front edge of the rail, but that highlight must be
  constant all the way along.

Opaque; no transparency needed. No hardware, no lettering, no shadow, no
perspective. Please keep the exact 4096 : 256 aspect.

---

## GROUP B · brass fittings

*All transparent background. Brass is bright `#c9a24a` where the light sits
and shadow brass `#8a6a2a` where it turns away. Satin, machined, clean —
polished by use, not mirror-polished.*

### 04 · Instrument bezel — `ht-bezel.png` — **512 × 512**

A single flat-on, straight-down photograph of a circular brass bezel ring
from a ship's instrument — the ring only, with **nothing inside it**.

A machined brass ring occupying the **outer ~18 % of the radius**: the outer
edge of the ring touches the four edges of the frame, and the inner edge sits
at about 82 % of the radius. A gentle rounded profile so the light rolls from
bright brass at the top of the ring to shadow brass at the bottom of its
inner lip; a fine turned line or two around the face is fine, a scale is not.
No numbers, no marks, no screws.

**CRITICAL:** perfectly circular, perfectly centred, the outer circle touching
all four edges. The **centre must be fully transparent** and the inner hole a
true circle, concentric with the outer — the gauge dial is drawn by code
inside it, and an off-centre hole shows at once. Transparent outside the ring
as well. No shadow, no perspective, no panel behind it.

### 05 · Knob cap — `ht-knob.png` — **512 × 512**

*The most-repeated brass part on the panel — every rotary control is one of
these.*

A single flat-on, top-down photograph of a small solid brass knob cap, as
fitted to a Victorian scientific instrument.

Seen from directly above. A flat or very slightly domed top face with a plain
satin finish — **at most a hint of CONCENTRIC turning, very fine rings running
AROUND the centre**, the way a lathe-turned cap is finished. It must **NOT**
have radial or spoke-like lines running outward from the centre and must not
look like a fan, a turbine or a starburst: at panel size those collapse into a
rotating pattern. If in doubt, make the face plainer. A fine knurled or milled
outer skirt catching a ring of small highlights. One crisp **dark inlaid
pointer line** — near-black enamel or blued steel let into the brass — running
from the exact centre straight up to the rim, pointing to **12 o'clock**.

**CRITICAL:** perfectly circular, perfectly centred in the frame, the circle
touching all four edges. The pointer must be exactly vertical and exactly on
the centre — this image gets **rotated**, so any offset makes the knob wobble
as it turns. Transparent background, no shadow, no perspective, no panel
behind it.

### 06 · Chart tack — `ht-tack.png` — **256 × 256**

A single flat-on, straight-down photograph of a brass chart tack — a drawing
pin of the kind that holds a chart to the table — seen from directly above,
so only the **domed brass head** is visible.

A smooth, softly domed brass head, bright at the crown, rolling to shadow
brass at the rim; a small soft highlight just above centre. Plain — no
lettering, no knurling, no slot. It is used many times and quite small, so a
simple clean dome reads far better than a detailed one.

**CRITICAL:** perfectly circular, perfectly centred, the circle touching all
four edges of the frame. Transparent background, no shadow, no perspective,
no paper behind it.

### 07 · Pennant flag — `ht-flag.png` — **160 × 200** (portrait)

A photograph of a small enamel pennant flag on a slim brass pin, of the kind
used to mark a position on a chart — seen **from the side** (a side
elevation, not from above), still orthographic with no perspective.

The pin runs **vertically along the left edge** of the frame, full height, a
slim round brass pin with a small brass ball or cap at its top. The flag is a
small red enamel pennant — a warm coral-red near `#ff7a59`, the panel's
accent — attached at the pin near the top and **pointing to the right**, a
slightly tapering triangle or swallow-tail, filling the rest of the frame.
Glossy enamel with one soft highlight; a thin dark edge where the enamel
meets its brass border.

The pin and flag together fill the frame edge to edge. Flat-on to the side,
no perspective, transparent background, no drop shadow, no paper behind it.

### 08 · Nameplate — `ht-nameplate.png` — **1600 × 360**

*The one part that carries lettering.*

A flat-on photograph of a rectangular engraved brass nameplate, of the kind
screwed to the case of a Victorian scientific instrument.

Satin brass with a narrow bevelled edge all round catching a bright line
along the top and a shadow line along the bottom, and four small slotted
brass screw heads, one near each corner, all four the same. The plate fills
the frame edge to edge.

Engraved into the face, the words **HIGH TIDE** — exactly those two words,
spelled exactly so, in engraved serif capitals (a classical, instrument-maker
letterform, evenly spaced, centred). The letters are **cut INTO the brass and
darkened** in the cut, as real engraving is — not raised, not printed, not
painted on. Below the word, centred and shorter than it, **a single thin
engraved wave line**: one continuous gentle sinuous line with three or four
low crests, cut the same way.

**CRITICAL:** the lettering must read exactly "HIGH TIDE" and nothing else —
no maker's name, no model number, no date, no city. Flat-on, orthographic,
transparent background, no drop shadow, no perspective.

---

## GROUP C · glass and nacre

*Transparent background. These two are the tide gauge and the ball itself.*

### 09 · Tide-gauge tube — `ht-glass.png` — **64 × 512** (very tall and narrow)

A flat-on photograph of a vertical glass capsule tube — a tide-gauge tube —
**empty**, seen from directly in front.

A slim clear glass tube running the full height of the frame, closed at both
ends with a short brass ferrule at the top and at the bottom, each a plain
turned brass collar. The glass carries soft vertical highlights along its
walls where the light catches the curve, and nothing else.

**CRITICAL:** the **interior of the tube must be genuinely transparent** —
real alpha, not a painted grey — because the water level is drawn by code
inside it. The highlights belong on the glass walls, near the left and right
edges, not down the middle. The **middle section must be perfectly uniform
along its length**, because the tube gets stretched to fit each gauge; only
the two ferrules are allowed to be different from the rest. Transparent
background, no shadow, no perspective, no panel behind it.

### 10 · The pearl — `ht-pearl.png` — **256 × 256**

*The ball that rolls on the terrain. The one part whose light direction is
specified, because it sits in a scene lit from that side.*

A single photograph of a nacre pearl — a real pearl, round, seen straight on —
lit softly from the **upper left**.

Warm white nacre with the soft iridescence of real pearl: faint shifts of
rose, green and blue across the surface, a broad soft highlight at the upper
left, the lower right turning gently into a cool shadow with a hint of
reflected light along the bottom edge. Not a chrome ball, not glass — the
sheen sits *in* the surface, not on it.

**CRITICAL:** perfectly circular, perfectly centred, the circle touching all
four edges of the frame. No shadow beneath it, no surface for it to rest on,
no reflection of a room. Transparent background, no perspective.

---

## GROUP D · ornament and cover

### 11 · Compass rose — `ht-rose.png` — **1024 × 1024**

A chart compass rose drawn in fine engraved ink, of the kind printed on a
nineteenth-century Admiralty chart — **the ink only, on transparent**.

Dark blue-black ink, a fine engraved line style: a many-pointed star with
the principal points long and the intermediate points shorter, an outer
graduated ring or two of fine ticks, and a small **north star** or fleur at
the top point. Delicate, even line weight, the way a copperplate chart is
cut. No lettering at all — **no N, E, S, W, no numbers**.

**CRITICAL:** the ink is the only opaque thing in the image; **no paper, no
tint, no vignette behind it** — the rose is laid on the dark ground by code at
low opacity as an ornament, so anything that is not ink must be fully
transparent. Perfectly circular design, perfectly centred, its outer ring
touching all four edges. Flat-on, orthographic, no shadow.

### 12 · Manual cover — `ht-cover.png` — **1600 × 900** (landscape), opaque

*The one painterly part. This is an illustration, not a component, and it is
exempt from the flat-on and transparent-background rules — but not from the
no-text rule.*

A moonlit painting for the cover of the instrument's manual: a sculpted
terrain of smooth valleys and ridges — a heightmap made real, rolling hills
in dark stone or wet clay — seen from a **low angle**, close to the surface,
so the ridges rise against a night sky. Dark water is flooding the passes
between the ridges, teal-black `#1fb5c4` in the moonlight, still, reflecting
the moon. A single pearl rests in one of the valleys, catching the light. At
the edge of the frame, the corner of a chart table with brass instruments —
a bezel, a gauge, the rail — as if the terrain were laid out on the table
itself.

Painterly and atmospheric, mostly dark, with the moon's light on the ridge
crests, the water and the pearl. The palette is the panel's: sea-ink, brass,
cream, teal water, a touch of coral. **Opaque, fills the frame, no text, no
title, no lettering, no signature** — the title is set on top by code.
Exact 16 : 9.

---

## What these become

So the priorities make sense — this is where each part lands on the panel:

| part | becomes |
|---|---|
| 01 `ht-ground.png` | the ground behind everything, tiled |
| 02 `ht-paper.png` | the paper strip under the timeline, tiled |
| 03 `ht-wood.png` | the top and bottom rails of the panel, tiled left to right |
| 04 `ht-bezel.png` | the ring around each gauge; the dial inside is drawn by code |
| 05 `ht-knob.png` | every rotary control, rotated by code |
| 06 `ht-tack.png` | a PIN on the timeline — one per keyframe, placed by code |
| 07 `ht-flag.png` | the release marker at the end of the timeline |
| 08 `ht-nameplate.png` | the header |
| 09 `ht-glass.png` | the tide-gauge sliders, stretched to length and filled with water by code |
| 10 `ht-pearl.png` | the ball that rolls on the terrain |
| 11 `ht-rose.png` | a faint ornament in the corner of the terrain view |
| 12 `ht-cover.png` | the manual cover |

---

## Where to commit them

Repository **peterboggild/BrokildApps**, branch `main`, new folder:

```
assets/high-tide-decals/
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
  "set": "high-tide-decals",
  "machine": "High Tide",
  "files": [
    {"file": "ht-ground.png",    "size": [2048, 2048], "tileable": true,  "what": "Chart-table ground, sea-ink leather/linen"},
    {"file": "ht-paper.png",     "size": [2048, 1024], "tileable": true,  "what": "Cream laid tide-table paper"},
    {"file": "ht-wood.png",      "size": [4096, 256],  "tileable": true,  "what": "Dark oak rail, tiles left to right"},
    {"file": "ht-bezel.png",     "size": [512, 512],   "tileable": false, "what": "Brass instrument bezel ring, open centre"},
    {"file": "ht-knob.png",      "size": [512, 512],   "tileable": false, "what": "Brass knob cap, dark pointer at 12"},
    {"file": "ht-tack.png",      "size": [256, 256],   "tileable": false, "what": "Brass chart tack, from above"},
    {"file": "ht-flag.png",      "size": [160, 200],   "tileable": false, "what": "Red enamel pennant on brass pin, side view"},
    {"file": "ht-nameplate.png", "size": [1600, 360],  "tileable": false, "what": "Engraved brass nameplate, HIGH TIDE"},
    {"file": "ht-glass.png",     "size": [64, 512],    "tileable": false, "what": "Empty tide-gauge glass tube, brass ferrules"},
    {"file": "ht-pearl.png",     "size": [256, 256],   "tileable": false, "what": "Nacre pearl, lit upper left"},
    {"file": "ht-rose.png",      "size": [1024, 1024], "tileable": false, "what": "Compass rose, ink only, no lettering"},
    {"file": "ht-cover.png",     "size": [1600, 900],  "tileable": false, "what": "Manual cover painting, opaque"}
  ]
}
```

**Please append to `DELIVERED.md` as you go** — write each part's entry as
soon as that file is committed, rather than saving it all for the end. If the
run stops halfway, that file is what lets it resume instead of starting over,
and it lets me wire up whatever has arrived without waiting for the rest.

## If you can only do a few

Do **01, 05, 10 and 08** first, in that order. The ground is what stops the
sea-ink looking like a flat fill; the knob is the most-repeated fitting; the
pearl is the thing you watch; and the nameplate is the only part that cannot
be drawn passably by code.

---

## Combining parts into sheets

**Combining is welcome.** Several related parts on one sheet is fine and saves
you time. Two things make it work, and one of them I cannot guess.

### Resolution does not matter. ASPECT RATIO does.

Every part gets scaled to fit its place on the panel, so a knob delivered at
400 px is as good as one at 512. What cannot be fixed by scaling is the
**shape**:

- Anything described as *perfectly circular* — the bezel (04), the knob (05),
  the tack (06), the pearl (10), the rose (11) — must sit in a **square** crop
  region, centred. If its crop is 500 × 460 it becomes an oval on the panel
  and every knob wobbles as it turns.
- The rectangular parts must keep roughly the width-to-height ratio given for
  them. Within about 5 % is fine; the panel absorbs the rest.
- Do not put two parts with different aspect ratios in one crop region.
- The three tileable textures (01, 02, 03) and the cover (12) are better as
  standalone files — a texture on a sheet loses its seamless edges to the
  crop.

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
      "file": "sheet-brass.png",
      "size": [2048, 1024],
      "parts": [
        { "name": "ht-knob",  "rect": [32, 32, 512, 512],  "shape": "circle" },
        { "name": "ht-bezel", "rect": [608, 32, 512, 512], "shape": "circle" },
        { "name": "ht-flag",  "rect": [1184, 32, 160, 200] }
      ]
    }
  ]
}
```

- `name` — the target filename from the list above, **without** `.png`.
- `rect` — `[x, y, width, height]` in that sheet's own pixels, top-left origin.
- `shape` — `"circle"` for the five that must be round; omit otherwise.

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
3. **A painted-in background where alpha was asked for** — a grey inside the
   glass tube (09), paper behind the rose (11), a hole in the bezel (04) that
   is dark rather than transparent. Code has to draw *through* those.
4. **An end cap on a texture** — a rounded end or a shaded edge on the rail
   (03), or a repeating blemish on the ground (01) or the paper (02). It shows
   as a seam every tile.
5. **Wrong or missing content** — any lettering other than "HIGH TIDE" on the
   nameplate, letters on the rose, a scale on the bezel, water already in the
   tube, text on the cover.
