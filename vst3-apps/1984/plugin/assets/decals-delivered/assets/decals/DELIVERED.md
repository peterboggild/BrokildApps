# 1984 decal delivery — first pass

17 separate PNG assets generated from 1984-DESIGN.md and 1984-DECALS.md on 2026-09-23. Artwork made with the built-in image generator; cropped and resampled for delivery. No GitHub upload or synth integration was performed.

## Readiness

All files match the requested pixel dimensions and aspect ratios. Fifteen of seventeen alpha bounding boxes span their canvas. The toggle pair and seamless tiling requirements are not fully satisfied; use the procedural fallbacks for those parts until refined. Do not treat this package as a deviations-free production release.

The toggle pair uses one shared source-coordinate crop to retain approximate bezel placement. Independent tight crops would move the bezel between states. The generated bezels are not identical, and the common crop retains unequal transparent vertical margins.

The non-panel artwork preserves the generated alpha, typically 252–253/255 inside solid bodies rather than exactly 255. The glass and lamp-off center alphas are 252 and 253 respectively; neither is an empty/near-transparent frame. The panel was composited onto dark matte grey and exported opaque.

## Measured exports

Bounding boxes use exclusive right and bottom coordinates.

| File | Pixels | Alpha bounding box | Mean alpha /255 | Deviation |
|---|---|---|---|---|
| `panel.png` | 1024×1024 | (0, 0, 1024, 1024) | 255.0 | Repeat-edge differences remain: mean RGB difference 2.67/255 left-right and 6.06/255 top-bottom. Not certified seamless. |
| `cheek-left.png` | 300×1400 | (0, 0, 300, 1400) | 251.89 | No additional exception beyond alpha note above. |
| `cap-red.png` | 128×256 | (0, 0, 128, 256) | 251.61 | No additional exception beyond alpha note above. |
| `cap-amber.png` | 128×256 | (0, 0, 128, 256) | 251.7 | No additional exception beyond alpha note above. |
| `cap-ivory.png` | 128×256 | (0, 0, 128, 256) | 252.07 | No additional exception beyond alpha note above. |
| `cap-teal.png` | 128×256 | (0, 0, 128, 256) | 251.95 | No additional exception beyond alpha note above. |
| `cap-slate.png` | 128×256 | (0, 0, 128, 256) | 251.93 | No additional exception beyond alpha note above. |
| `track.png` | 32×1024 | (0, 0, 32, 1024) | 251.71 | No end caps; top-bottom repeat-edge mean RGB difference 5.32/255. Not certified seamless. |
| `knob.png` | 512×512 | (0, 0, 512, 512) | 196.66 | No additional exception beyond alpha note above. |
| `toggle-up.png` | 128×192 | (0, 0, 128, 183) | 162.64 | Shared crop with down state; bottom transparent margin. Bezel pixels differ between states; not production-registered. |
| `toggle-down.png` | 128×192 | (0, 34, 128, 192) | 153.99 | Shared crop with up state; top transparent margin. Bezel pixels differ between states; not production-registered. |
| `vfd-glass.png` | 1600×320 | (0, 0, 1600, 320) | 251.81 | No additional exception beyond alpha note above. |
| `reel.png` | 512×512 | (0, 0, 512, 512) | 101.45 | No additional exception beyond alpha note above. |
| `lamp-on.png` | 96×96 | (0, 0, 96, 96) | 197.11 | No additional exception beyond alpha note above. |
| `lamp-off.png` | 96×96 | (0, 0, 96, 96) | 197.97 | No additional exception beyond alpha note above. |
| `wordmark.png` | 1200×360 | (0, 0, 1200, 360) | 186.14 | Raised/engraved edge is more dimensional than flat silkscreen. |
| `screw.png` | 64×64 | (0, 0, 64, 64) | 197.01 | No additional exception beyond alpha note above. |

## Installation

Copy selected PNGs into `assets/decals/` in the synth source tree. Filenames match the order. The actual `Source/ui/ui.html` source was not provided or modified; verify its existing CSS hook URLs resolve to these assets in the build. Keep the current procedural rendering for `panel`, `track`, `toggle-up`, and `toggle-down` if strict compliance is required.

## Checks

- Exact dimensions and aspect ratios measured for all 17 PNGs.
- Alpha bounds and body opacity measured; complete values in `audit.json`.
- All exports visually reviewed on a dark background in `1984-decals-preview.png`.
- Panel inspected in a 2×2 repeat; small edge discontinuities measured and disclosed.
- Not tested in the live VST3/WebView.
