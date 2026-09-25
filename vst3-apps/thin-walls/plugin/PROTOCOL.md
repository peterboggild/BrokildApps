# Thin Walls — page ↔ native contract (v3: four sources, a material per surface, breakable walls)

The panel (`Source/ui/ui.html`) is a pure VIEW. The APVTS in the processor is the single
source of truth; the page never keeps a value the native side does not also hold.
Everything below is what the two sides agree on. The engine (`Source/Engine.*`) and the
page are written against this file.

## 1. The apartment (metres, fixed — the page hard-codes it, the engine owns it)

Coordinates: x east, y north, z up. Origin at the south-west corner of the LARGE room.
Yaw in degrees, 0 = facing +x (east), counter-clockwise positive (90 = north).

| room | index | x | y | height | note |
|---|---|---|---|---|---|
| LARGE | 0 | 0.0 – 6.0 | 0.0 – 5.0 | 2.8 | living room |
| SMALL | 1 | 3.0 – 6.0 | 5.0 – 8.5 | 2.5 | box room |
| GIANT | 2 | 6.0 – 18.0 | 0.0 – 9.0 | 5.0 | hall |

The region x 0–3, y 5–8.5 is outside the apartment (solid). Nothing can be placed there.

Doors are 0.90 m wide, 2.05 m high, sill at z = 0. Each has a hinge side; the leaf swings
INTO the room named in `swingsInto`. Aperture `a` (0 closed … 1 open) uncovers a strip of
width `0.9·a` on the side away from the hinge. The leaf angle is 90°·a into that room.

| door | index | between | on wall | span | hinge at | swings into |
|---|---|---|---|---|---|---|
| SMALL–LARGE | 0 | rooms 1 & 0 | y = 5.0 | x 4.05 – 4.95 | x = 4.05 (west) | SMALL (north side) |
| LARGE–GIANT | 1 | rooms 0 & 2 | x = 6.0 | y 2.05 – 2.95 | y = 2.05 (south) | GIANT (east side) |
| SMALL–GIANT | 2 | rooms 1 & 2 | x = 6.0 | y 6.05 – 6.95 | y = 6.95 (north) | GIANT (east side) |

Listener ear height is fixed at 1.65 m (standing). Head radius 8.75 cm.

## 2. Sources

Four sources, each a virtual loudspeaker or a pure point source, each with its own input.
A source whose INPUT is OFF is silent and costs nothing; it is still shown (ghosted) and can
be placed. Inputs come from two stereo buses: MAIN (the track) and AUX (a second input bus
the host can route another track into — "sidechain" in most DAWs).

Two types:
- **PURE** — a point source, no box. DIRECTIVITY 0 = omnidirectional, 1 = cardioid (the same
  at every frequency). Drawn as a sphere on a microphone stand; HEIGHT is the stand.
- **LOUDSPEAKER** — a two-way studio monitor (6.5" woofer, 1" tweeter, crossover 2.2 kHz):
  nearly omni at 125 Hz, beaming with frequency in front, about 22 dB down straight behind
  at 4 kHz and up. DIRECTIVITY does not apply (the page greys it). Drawn as the cabinet.

## 3. Host parameters (all normalised 0..1; the table is `TW_SPECS[]` in PluginProcessor.cpp)

Per source n = 1..4 (ids `s1x`, `s2x`, … ):

| id | name | kind | real value |
|---|---|---|---|
| `s{n}x` | SOURCE n X | float | x = v·18 m |
| `s{n}y` | SOURCE n Y | float | y = v·9 m |
| `s{n}z` | SOURCE n HEIGHT | float | z = 0.2 + v·2.2 m |
| `s{n}yaw` | SOURCE n FACING | float | yaw = v·360° |
| `s{n}type` | SOURCE n TYPE | choice `PURE|LOUDSPEAKER` | |
| `s{n}dir` | SOURCE n DIRECTIVITY | float | 0 omni .. 1 cardioid (PURE only) |
| `s{n}in` | SOURCE n INPUT | choice `OFF|MAIN L|MAIN R|MAIN L+R|AUX L|AUX R|AUX L+R` | |
| `s{n}lvl` | SOURCE n LEVEL | float | dB = −24 + v·30 (0.8 = 0 dB) |

Defaults: source 1 at (3.0, 2.5, 1.2 m) facing 0°, LOUDSPEAKER, MAIN L+R, 0 dB.
Source 2 at (1.5, 1.0, 1.2) facing 45°, LOUDSPEAKER, OFF. Source 3 at (4.5, 7.0, 1.2) facing
270°, PURE with directivity 0.5, OFF. Source 4 at (12.0, 4.5, 1.6) facing 180°, PURE, OFF.
In normalised terms: x = 3.0 → 0.1667, y = 2.5 → 0.2778, z = 1.2 → 0.4545, level 0 dB → 0.8.

Then the rest, in this order:

| id | name | default | kind | real value |
|---|---|---|---|---|
| `lisx` | LISTENER X | 0.25 | float | x = v·18 m (4.5) |
| `lisy` | LISTENER Y | 0.2778 | float | y = v·9 m (2.5) |
| `lisyaw` | LISTENER FACING | 0.5 | float | yaw = v·360° (180 = west) |
| `door1` | DOOR SMALL–LARGE | 1.0 | float | aperture 0..1 |
| `door2` | DOOR LARGE–GIANT | 1.0 | float | aperture |
| `door3` | DOOR SMALL–GIANT | 0.0 | float | aperture |
Then, for each room n = 1..3 (n = 1 LARGE, 2 SMALL, 3 GIANT), seven rows in this order:

| id | name | default | kind | real value |
|---|---|---|---|---|
| `mat{n}` | ROOM n WALLS | 1, 1, 2 | choice `ABSORBING|FURNISHED|PLASTER|TILED|STUDIO` | the four vertical walls |
| `flr{n}` | ROOM n FLOOR | 0 | choice `AS WALLS|ABSORBING|FURNISHED|PLASTER|TILED|STUDIO` | index 0 means follow the walls |
| `cel{n}` | ROOM n CEILING | 0 | choice (same as the floor) | index 0 means follow the walls |
| `fold{n}a` | ROOM n FOLD A | 0.5 | float | metres = (v − 0.5)·1.2, so ±0.6 m; 0.5 is a flat wall |
| `foldp{n}a` | ROOM n FOLD A POS | 0.5 | float | along the wall, 0.15 + v·0.7 |
| `fold{n}b` | ROOM n FOLD B | 0.5 | float | the second breakable wall |
| `foldp{n}b` | ROOM n FOLD B POS | 0.5 | float | |

**The breakable walls** are the two of each room that carry no doorway, so a fold
never collides with a door. Wall numbering: 0 = west (x0), 1 = east (x1),
2 = south (y0), 3 = north (y1).

| room | fold A | fold B |
|---|---|---|
| LARGE | west wall, x = 0 | south wall, y = 0 |
| SMALL | west wall, x = 3 | north wall, y = 8.5 |
| GIANT | east wall, x = 18 | north wall, y = 9 |

A fold splits its wall at `foldp` along its length and pushes that point `fold`
metres along the wall's OUTWARD normal. Positive pushes the point out of the
room, which disperses; **negative makes the wall concave, which focuses sound at
a point and is worse than leaving it flat** — the plan should say so when a fold
goes negative. The room's plan polygon, its floor area and its volume all follow
the fold, so pushing a wall out really does make the room bigger.
| `direct` | DIRECT | 0.8 | float | dB = −24 + v·30 (0.8 = 0 dB) |
| `early` | EARLY REFLECTIONS | 0.8 | float | same law |
| `reverb` | REVERB | 0.8 | float | same law |
| `mix` | MIX | 1.0 | float | dry/wet, 1 = fully placed in the room; the dry side is the MAIN pair |
| `output` | OUTPUT | 0.8 | float | same dB law |
| `earspan` | EAR SPACING | 0.0294 | float | metres = 0.15 + v·0.85 (17.5 cm = a head, up to 1 m = a spaced pair) |

65 parameters. Choice parameters travel normalised: v = index / (steps − 1).

Positions outside every room are clamped by the engine into the nearest room. The page
should not let the user drag there in the first place.

## 4. Bridge

The page speaks to native with `window.__JUCE__.backend.emitEvent("tw", obj)` and listens
with `window.__JUCE__.backend.addEventListener(name, fn)`. When the bridge is absent
(headless test), the page must still boot and render with its own defaults.

### Page → native (`k` selects the kind)

| message | meaning |
|---|---|
| `{k:"hello"}` | sent once at boot; native replies with `initialState` until acked |
| `{k:"stateack"}` | the page has applied `initialState` |
| `{k:"p", id, v}` | set a parameter (normalised) |
| `{k:"touch", id, down:true/false}` | begin/end a host gesture around a drag |
| `{k:"presetSave"}` | native opens a Save dialog in the house patch folder, writes JSON |
| `{k:"presetLoad"}` | native opens an Open dialog, applies the file, echoes every value |
| `{k:"presetDefault"}` | every parameter back to its default |
| `{k:"showrays", v:0/1}` | page-only preference, carried in the project (not a host param) |
| `{k:"selsrc", v:0..3}` | which source the SOURCE panel is editing; page-only, carried in the project |
| `{k:"wavOpen"}` | native opens a file dialog for a WAV/AIFF/FLAC/MP3; the file becomes the TEST SIGNAL, looped, replacing the MAIN input (both channels) while playing |
| `{k:"wavPlay", v:0/1}` | start/stop the looped test signal (the input passes through when stopped) |
| `{k:"wavGain", v:0..1}` | test-signal level, dB = -40 + v*40 (default 0.75 = -10 dB) |

### Native → page

`initialState`:
```
{ build:"260922.1", showrays:1, selsrc:0, auxConnected:0/1,
  wav:{ name:"", playing:0, seconds:0, gain:0.75 },
  params:[{id, name, v, stepped, steps, choices:"A|B|C"|null}, ...] }   // in TW_SPECS order
```
`auxConnected` says whether the host has enabled the AUX bus (so the page can mark AUX
inputs as unavailable).

`hostParam`: `{ params:[{id, v}, ...] }` — values the page did not move itself (automation,
preset load, undo). The page must apply them to its controls and its views.

`wav`: `{ name, playing, seconds, gain }` — the test-signal state, whenever it changes.

`notice`: `{ text }` — one line for the status strip (preset saved/loaded, refusal).

`scene` (30 Hz, the acoustic picture as the engine sees it right now):
```
{ sources:[ { pos:[x,y,z], yaw:deg, type:0|1, room:0..2, active:0/1, dir:0..1 }, x4 ],
  lis:[x,y,z], lisRoom:0..2, lisYaw:deg,
  paths:[ { t:"direct"|"r1"|"r2"|"portal"|"leaf"|"wall", s:0..3, pts:[[x,y,z],...], db:-12.3, ms:8.7 }, ... ],
  rt:[ [t250,t1k,t4k,t8k], [..], [..] ],   // per room, seconds, current materials + doors
  plan:[ [[x,y],[x,y],...], [..], [..] ],  // each room's plan polygon, following the folds
  in:dBFS, out:dBFS, drr:dB }
```
`s` is the source a path belongs to. `t` meanings: `direct` line of sight; `r1`/`r2`
first/second order wall reflections (pts carry the bounce points); `portal` a path bent
through an open doorway; `leaf` through a closed door leaf; `wall` through the party wall.
`db` is the path's broadband level relative to the direct sound at 1 m; `ms` its arrival time.

## 5. What the page owns

- The 3D POV view (the camera IS the listener: position `lisx/lisy`, yaw `lisyaw`; a
  page-only pitch). Walking and mouse-look write `lisx/lisy/lisyaw` through `p` messages,
  wrapped in `touch` gestures. Walking is blocked by walls and by a door whose aperture is
  below 0.5. All four sources are drawn (inactive ones ghosted).
- The plan view: drag any source or the listener, turn them with their heading handles,
  click a source to select it for the SOURCE panel, click a door to toggle it (0 ↔ 1), drag
  along a door to set an aperture. Rays from `scene` when `showrays` is on.
- Controls under the two views, every one with a hint.
- `window.__TW` debug hook: `{ state(), setParam(id,v), scene(), render(), draws, errors }`.

---

# v4 additions (2026-09-25): furniture, cinematic mode, video export

## 6. Furniture

The layout is PROJECT STATE, not host parameters (like `showrays`): saved in the
project and in preset files, never automated. Up to 24 pieces. Native owns the
catalogue; the page draws from it, so sizes can never drift.

`initialState` gains:
```
furniture: [ { id:"sofa", name:"SOFA", w:2.10, d:0.90, h:0.85, zb:0, zt:0.85,
               occludes:1, reflectTop:0, absorb1k:1.70 }, ... ],   // the catalogue, in engine order
furn:      [ { t:"sofa", x:1.5, y:4.3, yaw:0 }, ... ]              // the layout
```
Catalogue ids: `sofa armchair bed rug curtain bookcase table piano wardrobe person`.
`w` is the extent along the piece's own x (its width), `d` along its own y (depth),
`h` its height; yaw in degrees, 0 = width along +x, counter-clockwise positive (as
everywhere). `zb..zt` is the part that blocks sound (a table: only its top slab,
0.71-0.75; a grand piano: its body 0.35-1.00 above its legs). `occludes:0` for the
rug and the curtain. `reflectTop:1` for the table and the piano (hard tops).

Page -> native: `{k:"furn", items:[{t, x, y, yaw}, ...]}` - the WHOLE layout, every
time it changes (throttle to <= 30 Hz while dragging, always send the final state
on release). Unknown ids are dropped, extra pieces beyond 24 are dropped.

Native -> page: `furn` event `{ items:[...] }` whenever the layout changes for a
reason the page did not cause (preset load, project restore, preset default = empty).

A piece belongs to the room its CENTRE is in. The page should keep centres inside a
room and, where it can, keep the footprint inside that room's plan polygon.
`presetDefault` clears the layout.

What the engine does with it (so the panel can say so honestly): absorption per
octave band added to the room's Eyring A (a rug replaces the floor under it);
scattering (every wall bounce weakened, the late field gains it); occlusion (a path
through a piece's box bends over, under or round it with Maekawa's loss, smoothly
on both sides of the shadow edge); a hard top reflects. `scene.rt` and the path
`db` values already include all of it.

## 7. Cinematic mode (page only)

A page preference remembered in localStorage (per profile, not per project). OFF
must leave the view exactly as it is today, including "no draws while idle".

## 8. Recording a take and exporting an MP4

Page -> native:

| message | meaning |
|---|---|
| `{k:"recStart"}` | start capturing a take: the input audio (after the test signal replaces MAIN), every host parameter per audio block, and furniture changes. Cap 4 minutes. |
| `{k:"recStop"}` | stop capturing |
| `{k:"recCam", t, pitch, fov}` | while recording, the page's own camera (pitch and field of view are page-only), about 20 per second; `t` = the latest `rec.sec` the page received |
| `{k:"vidBegin", w, h, fps}` | start an export of the last take at this size (even numbers) and frame rate. Native first renders the take's AUDIO offline with a fresh engine (progress in `rec`), then sends `vidPlan`. |
| `{k:"vidFrame", i, jpg}` | frame `i` (0-based) as a base64 JPEG (no `data:` prefix), exactly `w` x `h`. Send the next one only after `vidAck` for this one. |
| `{k:"vidEnd"}` | all frames sent; native finishes the MP4 |
| `{k:"vidCancel"}` | abandon the export (the partial file is deleted) |
| `{k:"reveal"}` | show the last exported file in Explorer |

Native -> page:

`rec`, ~10 Hz while anything is happening and once on every change of state:
```
{ state:"idle"|"recording"|"ready"|"rendering"|"exporting"|"done"|"error",
  sec: <recorded seconds so far, or the take's length>, max: 240,
  progress: 0..1, file:"C:\...\take.mp4", text:"one line for the status strip" }
```
`ready` = a take is held and can be exported.

`vidPlan` (after the audio render):
```
{ fps, n:<frames>, seconds, w, h,
  ids:[ param ids in TW_SPECS order ],
  frames:[ [v0, v1, ...], ... ],       // n arrays, normalised 0..1 (choices as index/(steps-1)),
                                       // the host parameters at each frame's time
  furn:[ { f:<first frame>, items:[{t,x,y,yaw},...] }, ... ],   // layout changes, frame-stamped
  cam:[ { t, pitch, fov }, ... ] }     // what the page sent while recording
```
The page renders frame i from those values (NOT from the live controls), encodes it
as a JPEG and sends it. `vidAck` `{ i }` answers each frame.

---

# v5 additions (2026-09-25): light sync and wall pictures

## 9. Light sync (page preference, per profile; the data comes from native)

`scene` gains:
```
light: [l0, l1, l2]            // 0..1 per room: how much sound is in that room now
beat:  { bpm, ppq, playing }   // the host's clock (bpm 0 when the host has none)
```
`light` is the engine's own follower: what the room's sources play plus its late
field; half LEVEL (fast envelope on -48..-12 dB) and half PUNCH (fast over slow
envelope, so an onset flashes and a held sound glows). Exactly 0 in silence.
Measured: an onset reads 0.93 where the same noise held reads 0.44; a sealed room
next door reads 0.000.

Two drives, the page's choice: FOLLOW (a room's pendants follow `light[room]`) and
BEAT (pulse on the host's beats: phase = frac(ppq), extrapolated between scenes
with bpm/60 per second while `playing`; fall back to FOLLOW when not playing).
The slider scales the effect; 0 = exactly today's lamps and today's idle rule.

`vidPlan` gains, per frame: `light:[[l0,l1,l2],...]` (the offline render's own
follower at that frame's time) and `beat:[[ppq,bpm,playing],...]` (the recorded
host clock), so an exported video pulses exactly with its sound.

## 10. Wall pictures

Up to 8. A picture hangs on a wall of a room's BOX: `room` 0..2, `wall` 0 = x0
(west), 1 = x1 (east), 2 = y0 (south), 3 = y1 (north); `along` = metres from the
wall's lower-coordinate end (from y0 for walls 0/1, from x0 for walls 2/3) to the
picture's centre; `z` = centre height, m; `w` = width, m; `aspect` = height / width;
`frame` 0 thin black, 1 oak, 2 gilt, 3 unframed canvas; `kind` 0 PRINT (visual
only), 1 ACOUSTIC PANEL (5 cm printed absorber: its face area w*w*aspect adds
PANEL_ALPHA - wall alpha per band into the room's absorption; measured: 4 m^2
halves the tiled living room's RT60 at 1 kHz, 3.42 -> 1.69 s). On a folded
(broken) wall the page draws the picture on the panel under its centre, at that
panel's angle.

Page -> native:
| message | meaning |
|---|---|
| `{k:"picAdd", id, jpg}` | store an image once (base64 JPEG, no data: prefix, ~1024 px max side) |
| `{k:"pics", items:[{id, room, wall, along, z, w, aspect, frame, kind}, ...]}` | the whole layout, on every change (throttle drags to <= 30 Hz, final on release) |

Native -> page: `initialState` gains `pics:[...]` and `picImages:{id: jpg}`; a `pics`
event `{ items, images }` when a preset or project brings a layout. Images no
picture uses are dropped. `presetDefault` clears the pictures. Saved in the
project and in preset files.
