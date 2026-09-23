# THIRTY THOUSAND YEARS — decal order

*For the image generator. Six isolated parts; the panel places them by a
geometry table and looks finished without any of them. Written after the
1984 and High Tide deliveries, which is why every rule below is a rule.*

## The instrument, in one paragraph (so the parts belong to it)
A drone synthesizer for humanity's darkest hour: cities whose machines still
run after the people are gone, intelligence growing inside abandoned
infrastructure, broadcasts continuing without a listener, steel under
impossible strain. The panel is an operational console built to outlive its
civilisation: severe, precise, tactile, beautifully proportioned. Dark
graphite, bone-white lettering, muted amber, oxidised metal accents, a
sparse warning vermilion, a cold cyan for spectral data. Textures are used
sparingly and the controls are clean. **No theatrical horror decoration, no
illegible distressing, no glitch effects, no decorative machinery.**

## The rules (each one cost a round on an earlier order)
1. **One part per file, isolated, on a transparent background.** Never a
   registered faceplate: an image generator cannot hit exact coordinates.
2. **The drawing fills the canvas.** Measure the non-transparent bounding
   box before writing "deviations: none" — four of twelve High Tide parts
   came back as a strip in the top fifth of a black canvas, and three of
   the Rite parts did the same under the same wording.
3. **Honest alpha.** A transparent background means alpha 0, not black. An
   opaque texture means alpha 255 everywhere. A glass overlay means the
   glass's own transparency (30–40 %), not opaque grey.
4. **Anything circular is delivered in a SQUARE canvas**, centred, with the
   circle touching the edges.
5. **A texture tiles**: no end caps, no vignette, no lighting gradient. The
   plate texture will be mirror-tiled if it is not certified seamless.
6. **Aspect is not free** (resolution is): a part is resampled to size but
   never stretched; deliver at the stated aspect.
7. Deliver a `DELIVERED.md` naming each file, its pixel size, and the
   measured content bounding box.

## The parts

| # | file | canvas | what it is |
|---|---|---|---|
| 1 | `panel.png` | 1024×1024, opaque, SEAMLESS tile | Graphite console skin: brushed, very dark (mean luminance about 12–16 %), a faint machined grain running horizontally, no logos, no gradients, no vignette. It repeats across a 1440×900 deck; nothing on it may draw the eye. |
| 2 | `plate.png` | 1024×1024, opaque, SEAMLESS tile | A second, slightly lighter oxidised-metal tile (mean 18–22 %) with a fine, irregular patina, for the raised sub-panels. Same rules as #1. |
| 3 | `knob.png` | 512×512, transparent outside the circle | The macro knob CAP, seen from straight above, no pointer (the page draws it): a dark graphite disc with a thin bone-white bezel, a subtle machined ring near the rim, a slight concave centre. Neutral lighting from straight above (it will be rotated; a directional highlight would rotate with it). |
| 4 | `glass.png` | 1024×512, transparent (the glass itself 30–40 % alpha) | The smoked glass over the SPECTRAL RECORD: a very dark, slightly warm-grey translucent sheet with one soft diagonal reflection and a hairline highlight along the top edge. It is composited OVER a drawing, so it must let the drawing through. |
| 5 | `wordmark.png` | 2048×256, transparent | THIRTY THOUSAND YEARS in a distinctive, legible title treatment: a wide, severe geometric sans or a slab, letter-spaced, bone-white (#E7E1D3) with the faintest engraved edge. The letters fill the canvas height; nothing else. **Check every letter is inside the canvas**: the 1984 wordmark's plank was shorter than its own lettering. |
| 6 | `screw.png` | 128×128, transparent outside the circle | A machined countersunk screw head, oxidised steel, slot at 15°, top-lit. Placed at the sub-panel corners. |

## Palette (for reference, not to be painted onto parts)
graphite `#0C0D10` / `#15171B`, bone `#E7E1D3`, ash `#A9A294`, amber
`#C8963C`, oxide `#6E5A44`, vermilion `#C2432E`, cold cyan `#5CC8D8`.

## How they are wired
The page (`Source/ui/ui.html`) probes `assets/decals/<name>.png` at boot and
switches an `html.d-<name>` class only when the file loads; `tools/ingest-decals.ps1`
(to be written when the parts arrive, from the High Tide one) crops each
part to its content box, mirror-tiles the two textures, and writes the
runtime files into `assets/decals/`. The plug-in compiles them in through
BinaryData and serves them by basename. Ordering nothing is fine: the panel
ships procedural.
