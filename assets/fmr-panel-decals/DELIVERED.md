# FULL METAL RACKET decal delivery

This delivery uses standalone production files for the highest-priority parts
and retains the remaining generated composites as source sheets. Standalone
files are already named as their target assets and therefore do not appear in
the JSON block.

The crop rectangles below use a top-left origin. Circular assets always use a
square crop. The composite sheets are useful source material, but some objects
need final extraction/cleanup before being wired into the panel; see the notes
after the JSON block.

```json
{
  "sheets": [
    {
      "file": "sheet-controls-source.png",
      "size": [1242, 1266],
      "parts": [
        { "name": "fmr-fadercap-v", "rect": [150, 32, 282, 440] },
        { "name": "fmr-fadercap-m", "rect": [624, 32, 512, 378] },
        { "name": "fmr-slot-v", "rect": [198, 624, 208, 520] },
        { "name": "fmr-slot-m", "rect": [770, 624, 200, 642] }
      ]
    },
    {
      "file": "sheet-keys-lamps-source.png",
      "size": [1536, 1024],
      "parts": [
        { "name": "fmr-keydark", "rect": [32, 32, 700, 328] },
        { "name": "fmr-keydark-lit", "rect": [804, 32, 700, 328] },
        { "name": "fmr-pad", "rect": [32, 544, 590, 282] },
        { "name": "fmr-led", "rect": [646, 512, 448, 448], "shape": "circle" },
        { "name": "fmr-led-lit", "rect": [1064, 512, 448, 448], "shape": "circle" }
      ]
    },
    {
      "file": "sheet-materials-fittings-source.png",
      "size": [1536, 1024],
      "parts": [
        { "name": "fmr-anodised", "rect": [32, 16, 710, 710] },
        { "name": "fmr-cheek", "rect": [1084, 16, 192, 710] },
        { "name": "fmr-nameplate", "rect": [32, 772, 924, 174] },
        { "name": "fmr-screw", "rect": [1080, 768, 208, 208], "shape": "circle" }
      ]
    }
  ]
}
```

## Standalone files

- `fmr-knob.png` — centred square knob with vertical red pointer.
- `fmr-key.png` — unlit cream model key, normalized to the requested aspect.
- `fmr-key-lit.png` — matching illuminated model-key state, normalized to the
  same target dimensions and alpha silhouette.
- `fmr-panel.png` — cream orange-peel panel texture, exported at 4096 × 4096.

## Source-sheet notes

- `sheet-controls-source.png`: the four requested components are isolated on
  transparency, but the fader-cap silhouettes are taller than the brief calls
  for. Use as photographic source material or flatten the caps during final
  extraction.
- `sheet-keys-lamps-source.png`: useful photographic faces, but the generator
  added dark mounting surrounds and a faint brown field. Extraction/background
  cleanup is required. The lamp crops are square as required.
- `sheet-materials-fittings-source.png`: useful material sources. The walnut
  cheek, nameplate and screw need background cleanup. The anodised quadrant is
  not guaranteed mathematically seamless and should be offset-tested before
  use.

All images were generated with the built-in image-generation workflow from
the specifications in `BRIEF.md`.
