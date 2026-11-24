# Journey to the Moon

A realistic Apollo-style flight to the Taurus-Littrow valley for the Meta Quest 3 browser. Design, decisions and
stages: `MOON-DESIGN.md` in the repository root. This file describes what is built.

**State: stage 0, the Moon test.** Three views of one baked Moon, to judge realism and frame rate in the headset
before anything else is built:

| View | What it tests |
|---|---|
| Lunar orbit, 110 km | The whole-Moon and regional data, shading without limb darkening, shadows near the terminator |
| Low pass, 15 km | The 59 m and 11 m data and the joins between levels at speed |
| On the surface | The valley from eye height: the 2 m terrain model, generated small craters, rocks, soil, long shadows |

The lander and the Earth are stand-ins (stage 5 and stage 2 build the real ones). There are no stars yet.

On the surface (added after the first headset test):

- **Bootprints** (`src/prints.js`): the Apollo overshoe's ribbed sole, a print every stride whether you walk in the
  room or with the stick, two side by side where you land after a teleport, and a trail from the ladder to where
  you start. One instanced mesh (one draw call, up to 1200 prints, the oldest reused). Each print is shaded by the
  terrain's own shader (`moon.printMaterial`) with the print's relief on top and an eight-step march over its height
  for the shadows of its walls and ribs at the low Sun. The relief is made in code; no image files.
- **Sound in the suit** (`src/sound.js`, synthesised): a dull thud through the body and a dry, gritty scrape at
  every step, and the suit fan's soft hiss and hum (can be switched off on the start page). Nothing from outside:
  there is no air.
- **Teleport**: hold the right trigger to aim an arc, let go to stand there (a short blink). Only on the detailed
  ground, ±145 m round the lander. Can be switched off on the start page.
- **Standing on the ground as drawn**: walking, rocks and prints use the finest mesh's own triangles
  (`gridHeight` in `src/moon.js`), not the finer data behind its shading. Rocks take the lowest point under their
  footprint and are bedded in, so none hangs in the air on a slope or a crater rim.

## How the Moon is drawn

Seven nested levels centred on the landing site, each a grid mesh with one RGBA map, built offline by
`tools/build.py`:

| Level | Covers | Map resolution | Source |
|---|---|---|---|
| G | the whole Moon | 1.4 km | NASA SVS CGI Moon Kit (LOLA, 64 px/degree) |
| L1 | ±12° | 355 m | the same |
| L2 | ±4° (121 km) | 59 m | SLDEM2015 (LOLA + Kaguya), 512 px/degree |
| L3 | ±22 km | 11 m | LROC NAC terrain model of the Apollo 17 site, 5 m |
| L4 | ±5 km | 2.4 m | LROC NAC terrain model, 2 m |
| L5 | ±1 km | 0.5 m | the 2 m model plus generated craters |
| L6 | ±150 m | 7 cm | generated craters (0.3 m and up), soil relief, rocks |

Map channels: R, G = surface normal (east, north); B = the elevation angle of the terrain horizon in the direction of
the Sun; A = albedo detail. The shader compares B with the Sun's height: cast shadows with a penumbra for any Sun
height, from one texture read. A coarser level is not drawn where the next finer one lies; the finer level's border
is laid exactly on the coarser level's triangles and melts into its own data over a few cells.

Light: Lunar-Lambert reflectance (no limb darkening), a phase function with the opposition surge (the ground
brightens around the point opposite the Sun), shadows filled only by light bounced from the sunlit ground. The Sun
is where it was at the Apollo 17 landing: 13.3° high, azimuth 95.5°.

Nothing procedural runs per pixel in the headset: everything heavy is baked.

Depth: standing on the surface, the distant levels are drawn first with far clipping planes, then the depth buffer
is cleared and everything within a few kilometres is drawn with near ones. In XR the per-eye projection matrices
are patched for each pass (`setPlanes` in `src/main.js`).

## Generated detail, and what is real

Real: every landform down to about 5 m (the massifs, the valley floor, craters larger than about 10 m).
Generated, statistically right but not the actual ones: craters below about 7 m, the rocks, the soil texture.
The 2 m terrain model is smoothed below its true resolution before the generated craters are added, because its
stereo noise turns into lumps when magnified.

## Files

| File | What it holds |
|---|---|
| `index.html` | Start page (English), import map for the vendored three.js |
| `src/main.js` | Renderer, the two-pass depth scheme, views, controls, frame-rate panel, test hooks |
| `src/moon.js` | Loads the levels, builds the meshes, the lunar shader |
| `src/extras.js` | Sun, stand-in Earth, rocks, stand-in LM, the handrail of the orbit views |
| `assets/moon/` | Built data: `moon.json`, one map and one mesh per level, colour maps, rocks, walking heights |
| `tools/download.py` | Fetches the NASA source data (3.9 GB) to `%LOCALAPPDATA%\moon-data` (never into the repository) |
| `tools/build.py` | Builds `assets/moon/` from the source data. Python 3 with numpy, scipy, tifffile, imagecodecs, pillow |
| `tools/encode.py`, `tools/ktx2.mjs` | Encode the built maps to KTX2 (Basis Universal UASTC) with the WebAssembly encoder |
| `test/xr.test.cjs` | Emulated Quest 3 test (IWER, playwright-core with the installed Chrome) |
| `tools/shot.ps1` | Headless Chrome screenshot of the flat preview |
| `vendor/` | three.js r170, KTX2Loader and the Basis transcoder (MIT / Apache 2.0) |

## Rebuilding the data

```
python tools/download.py          # once, 3.9 GB
python tools/build.py             # PNG maps, about ten minutes: for development
python tools/encode.py            # PNG/JPG -> KTX2 as published, about five minutes, switches moon.json to ktx2
```

The KTX2 encoder: put `basis_encoder.js` and `basis_encoder.wasm` from
github.com/BinomialLLC/basis_universal (`webgl/encoder/build`) in `%LOCALAPPDATA%\moon-data\ktx`.

Built assets are large. Commit them only when they have changed for good: every commit of them stays in the
repository's history.

## Testing

Serve the repository root (`python -m http.server 8765`) and open `/vr-apps/moon/`.

URL parameters: `debug=shadow` (the baked shadows only, each detail level in its own colour: shows whether a dark
patch is a shadow and whether shadows meet across level borders), `view=orbit|low|surface`, `theta=` (degrees along the orbit before the site), `yaw=`, `pitch=`,
`x=`, `z=`, `heading=` (surface position and facing), `sun=` (degrees of lunar rotation after landing, 0.51 per
hour), `exposure=`, `scale=` (VR resolution), `still` (no motion), `shot` (hide the panel).

`tools/shot.ps1` (headless Chrome, virtual time) only works with the PNG maps: the KTX2 transcoder runs in
workers that virtual time does not wait for. Set `"ext": "png"` in `assets/moon/moon.json` for screenshots, and
back to `"ktx2"` before committing. `test/xr.test.cjs` runs in real time and works with either; it enters VR in
an emulated Quest 3 and checks both eyes in all three views.

Hooks on `window.__moon`: `S` (state), `setView(name, theta)`, `setSun(deg)`, `look(yaw, pitch)`,
`stand(x, z, heading)`, `teleport(x, z)`, `prints` (`count`, `add`, `trail`), `suit` (`step()`).

In XR, read the head from `camera.matrixWorld` after `renderer.xr.updateCamera(camera)`; the XR array camera
(`renderer.xr.getCamera()`) reports the world origin from `getWorldPosition`.

## Sources

All NASA / USGS, public domain. No NASA insignia are used.

- NASA Scientific Visualization Studio, CGI Moon Kit: https://svs.gsfc.nasa.gov/4720 (colour: LROC WAC; elevation: LOLA)
- SLDEM2015, Barker et al. 2016: http://imbrium.mit.edu/DATA/SLDEM2015/
- LROC NAC digital terrain models `NAC_DTM_APOLLO17` and `NAC_DTM_APOLLO17_2M` (NASA/GSFC/Arizona State University):
  https://data.lroc.im-ldi.com/lroc/view_rdr_product/NAC_DTM_APOLLO17
- Sun position at landing: Apollo 17 traverse planning data (Apollo Lunar Surface Journal)
