# FULL METAL RACKET — round 2 delivery

This package contains the three new orders from the end of `BRIEF.md`:

- `fmr-cheek.png` — opaque 240 × 1200 vertically tileable walnut texture,
  without rounded short ends or end caps.
- `fmr-knob.png` — replacement 1024 × 1024 knob. The top face has been made
  rotationally symmetric with restrained concentric lathe grain; a 52 px QA
  check was used to remove the rejected fan/starburst behaviour.
- `sheet-step-buttons.png` — registered three-state step-button sheet. The
  three objects were normalized to the same 460 × 420 visual box and pasted
  at the same offset inside equal 576 × 528 crop regions.

The step buttons are delivered as a sheet because identical crop geometry and
registration matter more than their native resolution. There is no exterior
bloom. The slight highlight and darker lower edge are self-shading on the cap,
not a cast shadow onto a panel.

```json
{
  "sheets": [
    {
      "file": "sheet-step-buttons.png",
      "size": [1920, 640],
      "parts": [
        { "name": "fmr-step",     "rect": [32, 56, 576, 528] },
        { "name": "fmr-step-on",  "rect": [672, 56, 576, 528] },
        { "name": "fmr-step-acc", "rect": [1312, 56, 576, 528] }
      ]
    }
  ]
}
```

All crop regions use a top-left origin and have 64 px transparent gutters
between them plus at least 32 px around the outside.
