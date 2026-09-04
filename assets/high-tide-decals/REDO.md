# HIGH TIDE decals — four parts to redo

Paste this whole file to ChatGPT as one message. Eight of the twelve parts are
in the plug-in and look excellent. Four need to come back. Nothing else is
needed; do not regenerate the parts that are already good.

---

## What went wrong, in one sentence

Four parts arrived **letterboxed**: the drawing was pasted into a band of a
larger black canvas instead of filling the canvas at the requested size. The
delivered file is the right number of pixels, but the picture inside it is not.

Here is what was actually drawn inside each canvas, measured:

| part | asked for | actually drawn | drawn aspect vs asked |
|---|---|---|---|
| `ht-ground.png` | 2048 × 2048 | 2048 × **156** at the top | 13.1 : 1 vs 1 : 1 |
| `ht-paper.png` | 2048 × 1024 | 2048 × **155** at the top | 13.2 : 1 vs 2 : 1 |
| `ht-nameplate.png` | 1600 × 360 | 1589 × **124** | 12.8 : 1 vs 4.4 : 1 |
| `ht-rose.png` | 1024 × 1024 | 943 × **357** | 2.6 : 1 vs 1 : 1 |

The other eight were perfect and are already composited into the instrument:
`ht-bezel` (a round oscilloscope now sits in it), `ht-pearl` (it is the ball
rolling on the terrain), `ht-tack` (every point on the timeline), `ht-flag`
(the release marker), `ht-glass` (the gauge tubes), `ht-wood` (two rails).
`ht-knob` is beautiful and this panel has no rotary control to put it on, so it
is kept for later — no need to send it again.

---

## The rule that was broken, and it is the important one

**Fill the whole canvas.** The picture must reach all four edges of the file at
the exact pixel size given. Do not pad, do not letterbox, do not paste a
smaller render into a bigger frame. If the generator makes a different shape,
**resize the drawing to the canvas** — or better, draw it at the right shape in
the first place. Aspect ratio matters more than resolution: a part delivered at
the wrong aspect gets stretched on the panel and it shows immediately.

**A texture must have texture in it.** Two of these are surfaces, and both came
back essentially flat: the ground varies by about 1 % of full brightness across
the whole image, the paper by about 1.3 %. At 100 % zoom a person must be able
to see grain, fibre and weave. As a number to aim at: the standard deviation of
luminance across the image should be at least 8 of 255, and the image should
still read as the material at 25 % zoom.

---

## The four parts

### 01 · Chart-table ground — `ht-ground.png` — **2048 × 2048**, seamlessly tileable

A seamless tileable photographic texture of a dark sea-ink blue-black surface,
`#0a141c`: worn bookbinding leather or coarse linen stretched over a
navigator's chart table, lit evenly and photographed straight down.

Visible material: the grain of the leather or the weave of the linen, fine and
even, catching a little light on the raised fibres. Very slight, very
low-contrast unevenness in the sheen, as though a hand has passed over it for
years. No scratches, no stains, no patches, nothing that would repeat visibly
when tiled.

Fills the entire 2048 × 2048 frame. Uniform overall brightness corner to corner
so the tile seams disappear. No lettering, no objects, no hardware, no edges,
no vignette. Flat-on, orthographic, evenly lit, fully seamless.

### 02 · Tide-table paper — `ht-paper.png` — **2048 × 1024**, seamlessly tileable

A seamless tileable photographic texture of cream laid writing paper, `#e8dcc0`,
the kind a tide table is printed on.

Visible material: the laid lines of the paper, its fibre, a faint cloudiness in
the pulp, and a little age in the tone. Slight, sparse foxing is welcome as long
as it is fine enough not to repeat visibly.

**No ruled lines, no printing, no text, no borders.** Fills the entire
2048 × 1024 frame, uniform overall brightness, fully seamless and tileable.
Flat-on, orthographic, evenly lit.

### 03 · Engraved brass nameplate — `ht-nameplate.png` — **1600 × 360**, transparent background

An engraved brass nameplate reading exactly **HIGH TIDE**, in engraved serif
capitals, with a single thin engraved wave line beneath the words, and one small
slotted screw head in each of the four corners.

**What went wrong last time:** the plate came back squashed into a thin band
while the lettering kept its size, so the plate's own bottom edge cuts the
letters in half. The type must sit **inside** the plate with a clear margin:
roughly 15 % of the plate's height as empty brass above the capitals and 15 %
below the baseline, and the wave line inside that lower margin.

Aged, softly polished brass with fine horizontal brushing. The engraving is
recessed and dark, catching a highlight on its lower lip. The plate fills the
frame edge to edge at 1600 × 360 — the plate IS the image, its own bevelled
edge is the edge of the picture. Transparent outside the plate. Flat-on,
orthographic, no cast shadow. This is the only part that carries lettering.

### 04 · Compass rose — `ht-rose.png` — **1024 × 1024**, transparent background

A chart compass rose in dark blue-black ink on transparent, in the fine
engraved style of an Admiralty chart: a **complete circle**, north star at the
top, eight principal points with finer subdivisions between them, a graduated
outer ring of degree ticks.

**What went wrong last time:** what came back was a half rose, squashed to about
two and a half times wider than tall. It must be a full circle and perfectly
round — the drawn shape must be as tall as it is wide, centred in the frame,
filling it edge to edge.

Line art only: no fill, no shading, no lettering of any kind, not even N/E/S/W.
Even line weight, as though drawn with a ruling pen. Transparent background.

---

## Delivering

Commit the four PNGs into `assets/high-tide-decals/` in the GitHub repo
`peterboggild/BrokildApps`, overwriting the existing files, and append to
`DELIVERED.md` a line per file saying what it is.

**Before you commit, check each one:** open it and confirm the drawing reaches
all four edges of the canvas, that the file is exactly the pixel size asked
for, and — for the two textures — that you can see the material at 100 % zoom.
