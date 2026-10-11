# Decals: what happens when they come back (for us, not the deliverer)

The order is generated from `docs/decal_parts.json` by `tools/build-decal-brief.js`
(`decal_brief.md` + `decal_brief.html`, the page published for the helper). ChatGPT's
image tool makes only 1024x1024, 1536x1024 and 1024x1536, so every part is ordered in one
of those; strips, plates and the head block became tiling textures or nine-slices, and
the program crops and scales.

`powershell -File tools\ingest-decals.ps1 -From <folder> [-Copy]` reads the same JSON and
reports per part: present, canvas size, the drawing's bounding box against the canvas,
alpha (outside 0, inside opaque) for transparent parts, edge continuity for tiling
textures, registration for pairs. Only parts that pass are copied to `assets/decals/`, and
the panel keeps its procedural drawing for anything that fails.

## Where each part goes

Machine canvas 1600 x 528 machine units (1 unit = 0.826 px at 1x):
- reels at (230, 210) and (1370, 210); flange radius 198 units, hub radius 85.5
- capstan (1010, 392), pinch roller (1010, 420), guides (505, 391) and (1060, 391)
- arms pivot at (425, 300) and (1145, 300), rollers at (455, 352) and (1120, 352)
- heads on the tape line y = 400: erase x 562.5, record 600, playback 600 + 15 x position (cm)
- rail x 622.5 to 960 at y 462; head mounting plate (X_ERASE - 34, 398, 459.5, 100)
- loop tank (40, 60, 330, 300)

Controls take their sizes from `__MT.geom()` (snapshot of 2026-10-11 in the session
scratchpad): plates 10, screws 16 (9 px), knobs 46 x17 / 58 x2 / 38 x9, keys 24 px tall,
buttons 26-28 px, slider 176 x 10, VU 112 x 56, lamps 8-10 px.

Nine-slice parts (keys, buttons, slider track, VU face, glass, loop tank) carry their
border width in the JSON (`nine`, source px); use it as `border-image-slice`.
