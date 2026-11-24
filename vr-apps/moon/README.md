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
- **Teleport**: hold the right trigger to aim an arc (thrown from the hand, falling in lunar gravity), let go to
  stand there (a short blink). The right stick forward throws it further: from a hop of ten metres to kilometres,
  out to the massifs. Anywhere in the valley, ±4.8 km round the lander. Can be switched off on the start page.
- **The whole valley is walkable** (`src/ground.js`, `src/ground-worker.js`): see the next section.
- **Standing on the ground as drawn**: walking, rocks and prints use the finest mesh's own triangles
  (`gridHeight` in `src/moon.js`), not the finer data behind its shading. Rocks take the lowest point under their
  footprint and are bedded in, so none hangs in the air on a slope or a crater rim.

## The briefing (`briefing.html`, optional, not authentic)

A dark room with one large curved screen and a 1:60 model of the Apollo-Saturn V on a pedestal (the same model as
the outside view, `src/vehicle.js`). Twelve boards drawn in code (`src/briefing-slides.js`) walk through the whole
mission, each narrated (`tools/script/briefing.json`, Windows voice for now): the crew, the vehicle, launch to
orbit, Earth orbit and trans-lunar injection, transposition and docking, the coast, lunar orbit, into the Lunar
Module, the powered descent, Taurus-Littrow, the way home, and the non-authentic controls. On the vehicle and launch
boards the model turns and runs through its staging. Trigger or A: next board; B or X: back; the last board leads
to the launch. Test: `test/briefing.test.cjs`; screenshots `tools/shot-brief.cjs`.

## Chapter 1: the launch (`launch.html`)

**Curiosity, not authentic** (the owner's wish: authentic, but allowing curiosity):
- **Y**: a large window opens in the wall to your left (`setCheat` in `src/cabin.js`; its own wall cells, a red rim).
- **Hold B**: you leave the spacecraft and look at it from outside (`placeOutside` in `src/launch.js`): left stick
  round it and nearer or further (7-600 m), right stick up and down; the horizon stays level. Release to return.
- The **vehicle from outside** (`src/vehicle.js`): the three stages with their paint, the F-1 and J-2 engines with
  their flames (the F-1 plume blooms with height), the stages and the interstage skirt falling away at staging with
  the retro-rocket flash, the escape tower pulling the boost cover off and flying away, the Service Module, and the
  Command Module in its aluminised tape (windows, hatch, docking probe). Steam and smoke roll out of the flame trench
  at ignition (`src/pad.js`).

**Hands, faces, body:**
- Your **gloved hands** follow the controllers (Portalkniven's hands, `src/hands.js`, in the cabin glove colour).
  Touching a panel turns the hand red, a small "Not now" sign appears and the controller ticks.
- The crew's **faces** are a photoscanned head (Lee Perry-Smith, see Sources) seen through the bubble helmet, under
  the communications cap, the scan's closed eyes opened (cut out, with eyeballs behind), the two crewmates tinted and
  proportioned differently; their heads and eyes move a little.
- Looking down you see **your own suit** in the couch (torso, hoses, legs; no arms: your hands are the
  controllers). An option on the start page.

**Boarding** (the default start, `src/pad.js`): you stand at the tower end of the crew access arm, 106 m up, the
count holding at T−3:30. Walk (left stick, or in the room) along the enclosed arm into the White Room, where the
Command Module's hatch stands open under the boost protective cover, your crewmates strapped in inside. Walk up to
the hatch (or pull the trigger there): the view goes dark, the hatch closes with its latch ratchet, you are seated
in the couch, the arm swings back to its park position, and the count resumes. From the arm: the Saturn V's upper
stages beside you venting vapour, the escape tower above, the launch umbilical tower round you, the pad and its
flame trench below, the crawlerway towards the Vehicle Assembly Building, the propellant spheres, the beach and the
Atlantic to the east. The pad leader and Launch Control speak while you board (cued radio lines).

From the Commander's couch (the left one) on top of the Saturn V: T−3:30 to orbit insertion at T+11:53 and a minute
beyond. You sit upright; the cabin is turned round you so the main display console is in front, the hatch above.

- **The flight** (`src/ascent.js`): altitude, speed and pitch against time from published Apollo flight data,
  downrange integrated from them; Apollo 17's event times. The roll program turns you from facing the tower to
  facing downrange; you fly heads down, so the Earth's horizon comes into the windows above your head.
- **The cabin** (`src/cabin.js`): the 33° cone with its five windows where they were (the hatch window over the
  centre couch, two rendezvous windows looking forward, a side window each side), set deep in the wall; the main
  display console as three panels with a generated panel face and a few hundred 3D toggle switches; the DSKY showing
  the real launch programs (P02 on the pad, then P11, verb 06 noun 62: velocity, altitude rate, altitude); the
  mission event timer; an attitude ball that turns with the spacecraft. The walls are painted, riveted panels with
  Velcro patches, stencilled markings and wear (a generated texture), with the hatch and its latch gear, handholds,
  cable runs, floodlights and couch struts on them, and stowage lockers on the aft bulkhead. Your crewmates wear
  the launch suit: white, with the red and blue hose connectors and hoses, wrist rings, gloves, the flag on the
  shoulder, and the clear bubble helmet over the communications cap. Their faces are kept plain on purpose.
- **Seating in VR**: the headset's own origin is wherever the Quest was last recentred, so the couch is brought to
  you when the session starts (`seat()` in `src/launch.js`); a thumbstick press re-seats you.
- **The cover**: until the escape tower is jettisoned (T+3:18) the boost protective cover blinds every window except
  a 20 cm hole over the hatch window; then the windows open onto a sky already going black.
- **Outside** (`src/earth.js`, `src/sky-worker.js`): the sky and the air in front of the ground by single
  scattering (Rayleigh and Mie), computed in a Worker as a table for the current altitude, packed round the limb
  (the thin blue line on the Earth's edge from orbit comes out of it); the Earth from NASA's Blue Marble (December)
  with a sharper map of Florida and the Atlantic down the launch track, sun glint on the oceans, generated clouds;
  on the pad, the ground and the red launch umbilical tower with its swing arms; the escape tower and cover flying
  away ahead at jettison.
- **Drawn through the windows only**: the cabin is drawn first and writes the stencil; the outside is drawn only
  where it did not, in two more passes with their own near and far planes.
- **Sound** (`src/launch-sound.js`, `src/radio.js`): the rocket through the structure (rumble, roar, crackle;
  loudest in the first half minute and through max Q, silence at staging), the separation charges, the escape
  motor; cabin fans and pumps. The radio: 63 lines (`tools/script/launch.json`) in five voices on four channels,
  Launch Control and the pad loop band-limited, Houston with the Quindar tones, your crewmates on the intercom.
  Original lines in the style of Apollo procedure, not quotes. Voices are Windows voices rendered by
  `tools/tts.ps1` (run it in PowerShell 7: Windows PowerShell 5 does not see all the voices); your own recordings
  replace them file for file in `assets/script/voice/`.
- **Shaking**: the cabin shakes round you (a few millimetres, the S-IC cutoff jolt); your view never does. The
  controllers vibrate with the rocket. A setting on the start page: as it was, half, none.
- **Time**: A or X runs it ×4 or ×16 once the windows are clear, never across the staging or the shutdown.

Earth textures: `tools/earth.py` from NASA's Blue Marble Next Generation, December
(https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/december/). The clouds are
real: the cloud cover of 6 December 2019, taken from NASA's VIIRS true-colour mosaic of that day (Suomi NPP, from
NASA GIBS) against the cloud-free Blue Marble, for the whole Earth (10 km) and for the launch track (1 km). The
day was picked for a clear sky over Florida and a frontal band and fields of cumulus over the Atlantic.

Test aids: `launch.html?t=` (start time), `?still`, `?nocabin` (the outside without the spacecraft), `?shot`;
`tools/shot-launch.cjs`; `test/launch.test.cjs` (emulated Quest 3). Hooks on `window.__launch`.

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

## The ground round the walker

Near the lander the finest level (L6, ±150 m) was baked offline. Anywhere else in the valley a Worker makes the
same kind of ground for a square of 192 m round the walker, in about half a second on a PC:

- **Mesh**: the valley's real heights (`assets/moon/valley.dat`: the LROC NAC model at 4.9 m, bicubic) plus
  generated craters, laid exactly onto the coarser level's triangles along the square's border and round L6, so
  there is no step anywhere.
- **Map** (1024², 19 cm): the normal of the generated detail only, the horizon angle towards the Sun of that detail
  and of the rocks, and albedo detail. The shader adds the normal to the parent level's (L4, or L5 where it lies),
  multiplies the two shadow terms, and takes the albedo from the parent: so the mountains' shadows, the real
  landforms and the colour carry straight through.
- **Rocks**: instanced, lit against the square's own map and L4's (so they go dark in a massif's shadow).
- **Everything generated is a pure function of position** (seeded per 32 m tile). When the square moves with the
  walker (re-centred after 22 m), the overlap is identical: nothing pops, and bootprints stay where they were made.
- **Craters of 1.5-7 m inside L5** are the ones the build stamped into L5 (`assets/moon/craters.dat`): L5's map
  shades them already, so the worker only gives them shape and sharper shadows. Outside L5 they are generated.

The levels leave the square out (`uPatch`); the square leaves L6 out. A teleport blinks to black, waits for the
square at the target if it is not there, and blinks back in. Walking on it, rocks, bootprints and the frame-rate
panel work as by the lander. Known limits: the generated detail is coarser than by the lander (19 cm against 7 cm),
and in a band of about 25 m round L6's border only the 0.5 m data shows.

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
| `src/ground.js`, `src/ground-worker.js` | The generated ground round the walker anywhere in the valley |
| `src/geo.js` | Site frame against lunar latitude and longitude, mesh heights, seeded random numbers (page and worker) |
| `src/prints.js`, `src/sound.js` | Bootprints; the sound inside the suit |
| `assets/moon/` | Built data: `moon.json`, one map and one mesh per level, colour maps, rocks, walking heights |
| `tools/download.py` | Fetches the NASA source data (3.9 GB) to `%LOCALAPPDATA%\moon-data` (never into the repository) |
| `tools/build.py` | Builds `assets/moon/` from the source data. Python 3 with numpy, scipy, tifffile, imagecodecs, pillow |
| `tools/encode.py`, `tools/ktx2.mjs` | Encode the built maps to KTX2 (Basis Universal UASTC) with the WebAssembly encoder |
| `test/xr.test.cjs` | Emulated Quest 3 test (IWER, playwright-core with the installed Chrome) |
| `tools/shot.ps1` | Headless Chrome screenshot of the flat preview (PNG maps, near the lander only) |
| `tools/shot.cjs` | The same in real time with Playwright: works with KTX2 and anywhere in the valley |
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
`x=`, `z=`, `heading=` (surface position and facing, anywhere in the valley), `trail` (bootprints ahead), `sun=` (degrees of lunar rotation after landing, 0.51 per
hour), `exposure=`, `scale=` (VR resolution), `still` (no motion), `shot` (hide the panel).

`tools/shot.ps1` (headless Chrome, virtual time) only works with the PNG maps: the KTX2 transcoder runs in
workers that virtual time does not wait for. Set `"ext": "png"` in `assets/moon/moon.json` for screenshots, and
back to `"ktx2"` before committing. `test/xr.test.cjs` runs in real time and works with either; it enters VR in
an emulated Quest 3 and checks both eyes in all three views.

Hooks on `window.__moon`: `S` (state), `setView(name, theta)`, `setSun(deg)`, `look(yaw, pitch)`,
`stand(x, z, heading)`, `teleport(x, z)` and `teleporting`, `prints` (`count`, `add`, `trail`), `suit` (`step()`),
`ground` (`count`, `ms`, `rocks`, `busy`, `detailed(x, z)`, `request(x, z)`).

In XR, read the head from `camera.matrixWorld` after `renderer.xr.updateCamera(camera)`; the XR array camera
(`renderer.xr.getCamera()`) reports the world origin from `getWorldPosition`.

## Sources

- Crew faces: "Infinite, 3D Head Scan" by Lee Perry-Smith, licensed under Creative Commons Attribution 3.0
  Unported (https://creativecommons.org/licenses/by/3.0/), based on a work at www.triplegangers.com; taken from the
  three.js examples (`vendor/head/`, licence file beside it). Modified: cut below the neck, eyes opened, tinted.
- Hands: the WebXR generic hand model (@webxr-input-profiles/assets, MIT), `models/`, licence file beside it.

All NASA / USGS, public domain. No NASA insignia are used.

- NASA Scientific Visualization Studio, CGI Moon Kit: https://svs.gsfc.nasa.gov/4720 (colour: LROC WAC; elevation: LOLA)
- SLDEM2015, Barker et al. 2016: http://imbrium.mit.edu/DATA/SLDEM2015/
- LROC NAC digital terrain models `NAC_DTM_APOLLO17` and `NAC_DTM_APOLLO17_2M` (NASA/GSFC/Arizona State University):
  https://data.lroc.im-ldi.com/lroc/view_rdr_product/NAC_DTM_APOLLO17
- Sun position at landing: Apollo 17 traverse planning data (Apollo Lunar Surface Journal)
