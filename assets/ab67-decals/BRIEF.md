# ARTEFACT B2311.67 — graphics I would like from ChatGPT

Everything the artefact itself is made of is **computed**, and has to stay that
way: the tiling is a live cut through a four-dimensional lattice and it
rearranges as the player travels, so no bitmap could keep being correct. What a
generated image *can* do is the two things procedural code is worst at —
**supply an environment for the facets to reflect**, and **give the crate the
weight of a real object**.

Priority order below. (1) is by far the biggest gain; (2) is the next; (3) is
optional polish.

---

## 1 · MATCAPS — four of them  (the big one)

A matcap is simply a photograph-like render of a **single sphere filling a
square frame, lit by a studio**. I look the image up by the direction each
crystal facet is pointing, which gives the whole surface a real environment to
reflect. This is the single largest realism upgrade available to the panel:
faceted things look real because of what they reflect, and right now they
reflect nothing.

**Format for all four — this matters more than the content:**
- **1024 × 1024, square, PNG.**
- One sphere, **perfectly centred, touching all four edges** (the sphere's
  silhouette is the inscribed circle). No margin, no drop shadow, no ground
  plane, no horizon line, no props.
- **Outside the circle: pure black.**
- Straight-on view, orthographic-looking. No perspective, no vignette.
- No text, no watermark, no border.

Then, the four:

**1a — "ANODISED SPECTRUM".** A polished metal sphere with a thin-film
interference coating: the colour must sweep smoothly through magenta → blue →
cyan → gold as the surface curves away, the way anodised titanium or a soap
film does. Dark near the centre where it faces us, brilliant and saturated
toward the rim. One crisp bright key highlight upper-left, one dimmer cool fill
lower-right. Mirror-smooth, no texture, no scratches.

**1b — "DARK MINERAL".** A sphere of near-black polished stone, like obsidian or
hematite — deep charcoal with a faint blue-green cast, one long soft specular
streak, very low overall brightness, a whisper of internal translucency at the
rim. Almost nothing happening. This is the one that will cover most of the
object.

**1c — "WET GLASS".** A clear glass sphere over a black ground: bright
compressed rim, a couple of small sharp caustic-like highlights, strong
brightening at the very edge, otherwise dark. Cold neutral, not blue-tinted.

**1d — "COLD FIRE".** A sphere of dark metal lit only by two coloured sources —
a teal one from the upper left and a deep amber one from the lower right —
meeting in a dark band across the middle. Saturated, moody, no white light at
all.

---

## 2 · THE CRATE — the human packing case

The rectangle around the artefact is deliberately **ours**: an instrument case
we built to carry something we do not understand. It should look photographed
and worn, so the computed object inside reads as genuinely other. Isolated
parts, not a laid-out panel — I place them.

**2a — BRUSHED ALUMINIUM, seamless tile. 1024 × 1024, PNG, tileable in both
directions.** Fine horizontal brush marks, faint milling swirls, a few shallow
scuffs. Cool grey, evenly lit, flat-on, no highlights baked in, no vignette, no
edges or borders in the image at all — it must tile invisibly.

**2b — WORN PAINT / STENCIL WEAR, seamless alpha tile. 1024 × 1024, PNG,
tileable.** White marks on pure black: chipping, flaking, scratch wear, dust —
the *pattern* of wear only, no letters, no numbers, no symbols. I multiply this
over my own lettering so the stencils look sprayed and rubbed.

**2c — CASE CORNER FITTING. 1024 × 1024, PNG with transparent background.** A
single anodised aluminium corner bracket of the kind on a flight case: two
flanges meeting at a right angle, four countersunk rivets, slightly scratched,
lit from the upper left. Isolated on transparency, filling the frame, nothing
else in the image.

---

## 3 · OPTIONAL — surface micro-detail

**3a — CRYSTAL GROWTH STRIATIONS, seamless normal-map-ready tile. 1024 × 1024,
PNG, tileable.** Greyscale height, mid-grey base. Very fine parallel growth
lines with occasional forks and terraces, as on a mineral crystal face — like
the surface of a bismuth or galena crystal at high magnification. Subtle: no
feature should be more than a hint. No colour, no lighting, no shadows.

---

## What NOT to send

- Nothing biological, ribbed, skeletal or Giger-like. The artefact is a
  mineral, an optical object, a lattice — not a creature.
- No lettering or numerals baked into anything.
- No composed "panel" or faceplate: an image generator cannot hit exact pixel
  coordinates, and everything here is placed by code. Isolated parts only.
- Aspect ratio is the one thing that cannot be fixed afterwards. Resolution is
  free — everything gets rescaled — but a sphere drawn as an oval stays an oval,
  and a tile with an edge in it never tiles.
