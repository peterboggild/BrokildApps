# Fractal Video

Authoring tool for fractal music videos (the full plan is in `PLAN.md`). WebGL2 preview, scenes with
per-scene colour schemes, a song timeline, stems as modulation sources, and a modulation matrix that
keys any stem, band or beat-grid signal to formula parameters, camera, colour and light.

## Run

```
npm start
```

Open http://localhost:4173/ in Chrome or Edge, **in a normal browser tab**. Inside an embedded
preview (VS Code Simple Browser, iframes) the GPU preview waits for a click on purpose.

`npm run check` runs the headless tests of the schema, palettes and evaluator.

## Rendering a movie

Render tab → Movie: pick the range (whole song, or from–to seconds / from the playhead), encoding
quality, optionally *Quick test* (half size, 2 samples), then **Render movie**. Each frame is rendered at
High quality with the Samples / frame setting and streamed to ffmpeg by the local server; the MP4
(H.264 + the master as AAC) lands in the output folder (default `renders/` in this folder). The dialog
shows the last frame, time per frame and time left. Keep the tab visible while it runs (browsers pause
hidden tabs); Cancel removes the partial file. Soundtrack: *AAC (MP4)* encodes the master and lowers its level by a static
gain (no compression or limiting) until the encoded peaks stay below 0 dBFS — loud masters typically lose
1–2 dB; *Lossless PCM (MOV)* copies the master bit-for-bit (best for upload to YouTube/Vimeo or editing). Needs ffmpeg (`winget install Gyan.FFmpeg`) and the app
opened through `npm start` after this update (restart the server if it was already running).

## Layout

- **Top bar**: project name, transport (Space), Scene / Timeline preview, preview quality, undo/redo, Project menu.
  *Scene* holds the selected scene (it still reacts to the music while playing); *Timeline* plays the
  arrangement. Clicking a scene card goes to Scene; clicking or scrubbing the timeline goes to Timeline (key S toggles).
- **Left**: scenes (click to edit, drag onto a timeline section to assign, 1–9 to select), audio
  (master, tempo grid, stems with roles, solo, alignment check) and structure (see below).
- **Centre**: viewport. Drag orbit, right-drag pan, wheel zoom, double-click reset. The darkened border
  is the output frame (Render tab → Frame guide).
- **Right**: Shape (formula, parameters with green "calm" bands, *Key to music*, space, camera),
  Look (colour scheme), Modulation (routes), Render (format, still PNG, export).
- **Bottom**: timeline with ruler, contiguous sections (drag the right edge to resize, hatched area = blend
  into the next scene), master waveform, one lane per stem (level + detected hits). Ctrl+wheel zooms.

## Structure analysis (left panel, from the master)

Finds where the song changes and which passages are the same passage, then offers that as a set of
sections. No model and no network: it is the classical self-similarity pipeline — log-mel and chroma
frames, averaged per beat, a cosine self-similarity matrix, and Foote's checkerboard kernel down its
diagonal. Peaks in that curve are boundaries.

What makes it accurate enough to use is the bar grid. Every boundary is quantised to a whole bar and
then nudged onto whichever 4-bar phase already explains most of them, so an estimate a few hundred
milliseconds out lands on exactly the right bar. A boundary two bars off the grid is left alone: more
likely a real odd-length section than an error.

- **Analyse the song** draws only. Boundaries appear on the timeline with the novelty curve in the
  waveform lane and the detection threshold dotted across it, so a missed boundary can be seen to have
  been a near miss and the **sensitivity** moved accordingly. Blue lines were pulled onto the 4-bar grid.
- Passages that are the same get the same letter. Letters, not names: which one is the chorus takes
  knowledge of song form the analysis does not have. Where the cut between "same" and "different"
  falls is decided per song, from the distances themselves; on music with no clear repetition every
  segment stays its own type rather than being grouped on a guess.
- **lvl / brt / dns / bass** are stretched across the song, not measured on an absolute scale: they
  exist to choose between scenes within one track, and on a dense mix an absolute scale puts every
  segment at the same value and says nothing.
- **Use as sections** replaces the sections with the segments, one scene per letter from the scenes
  already in the project, and sets each blend from how abrupt its boundary was. Keyframes belong to
  section ids and do not survive it.
- The tempo grid has to be right first; the analysis is in beats. Changing the BPM and analysing again
  costs milliseconds, as only the spectral pass is cached with the file.

## Colour schemes (Look tab, per scene)

Look presets (palette + sky + light + grade); gradient editor with up to 8 stops (click the bar to add,
drag to move, Del to remove); palette library with hover preview; harmony generator in OKLCH;
hue / colourfulness / lightness nudges; mapping (colour source, perceptual / smooth / RGB / hard-band
blending, wrap, scale, offset, cycle per 8 bars); sky, fog, glow, lights and grade. Copy / paste a palette
or a whole look, or apply it to all scenes. Palettes blend between scenes in transitions. The library includes
"Wasteland" and "Dark & grim" groups, and the look presets include gritty ones (Wasteland, Doomsday,
Nuclear Winter, Irradiated, Eclipse, Rust & Teal, Bunker, Hellfire). Grade → Film grain adds animated grain.

## Backdrops and overlays (Look tab → Backdrop / overlay, per scene)

A picture or a movie behind the fractal, or in front of it as an overlay (Placement). Overlays blend with
Screen (footage on black: smoke, sparks, light leaks), Multiply (grime or scratches on white), Add, Soft light,
or Normal, which uses the file's own transparency (PNG, WebM with alpha). Pinned to the output frame (fill/crop, fit inside, stretch;
opacity, brightness, saturation, blur, zoom, pan). "Fog → backdrop" fades the fractal into it with distance
and fog, which also hides the cut between two formulas. Movies loop, muted, either with the song or
restarting at each section (starting where the section fades in), at an adjustable speed. Backdrop
brightness/opacity/zoom/blur/saturation/pan are modulation destinations. Drop a picture or movie
anywhere on the window to set it for the current scene. Files are stored in the browser like the audio;
H.264 MP4 or WebM, ideally 1080p, play best.

## Keying stems to the fractal

Load the master and stems (drag files anywhere; a name with "master" or "mix" becomes the master).
Each stem is analysed once (level envelope + hits) and cached in the browser, together with the file.
Then either Shape → *Key to music* (pick a parameter and a stem) or the Modulation tab: every route has
source, destination, depth, attack/release and look-ahead (beats), with a plot of the signal over the song.
Shape routes act in every scene whose formula has that parameter.

## Keyframes

Nothing is keyframed until you say so, and you say so on the slider itself.

**A parameter.** Right-click any keyable slider - formula parameters, Space, Ray march, numeric
Look values - and choose *Keyframe "<name>" in <scene>*. From then on, in the scenes that lane is
switched on in, moving the playhead and then the slider writes a key at the playhead instead of
changing the scene, and the slider follows the keyed value as the playhead moves. Right-click again
for the rest: add or set a key at the playhead, delete one, switch the lane on or off per scene,
or remove it entirely.

**The camera.** Put the playhead where you want it, frame the shot, and press **K**. That keys
every camera value at once - position, target, roll, field of view - so a move is two presses at
two points on the timeline.

Keys snap to quarter beats; hold **Alt** while dragging one in the timeline to place it freely.
Lanes live in `project.keys`, stored relative to their section, so splitting or re-cutting a
section carries them along. The evaluator applies them after the scene blend and before the
modulation routes, which means a keyed value sets the value and any route on the same parameter
still adds movement on top of it.

## VR (Quest and other WebXR headsets)

Open the page in the headset browser and press the VR button. It also works over a cable through
Meta Horizon Link, where the PC renders and the headset is a display: the headset does not switch
to the PC on its own, you start the session on whichever machine is to do the work.

**The panel (Y, or point and pull the trigger).** Tabs across the top: one per fractal family, then
Looks and Palettes. One tab at a time, because the headset drops its resolution the moment the frame
rate slips - exactly when you are reaching for the panel - and a 34-name grid is unreadable at that
point. Names are 32px, and the play/hold/camera/sound bar stays put at the bottom whatever tab is
open. The panel opens on the family of the fractal you are looking at.

**Shaping the fractal by hand.** Hold one trigger for a moment and move that hand: up/down and
left/right drive two of that fractal's significant parameters, four across both hands. The delay is
deliberate, so reaching for the panel or pressing both triggers to scale does not jog a parameter.
A short trigger click still just presses a tile.

**Microphone.** Off by default. Switch it on under Sound; the browser asks for permission on the page
first, outside VR.

## Nature family

Six branching formulas - tree, dendrite, nerve cell, root system, mycelium, coral - built on one
recursive distance estimator: a capsule per generation plus a polar wedge fold for the branches, with
gravity applied by carrying world-up through the local frame so a branch bends towards the ground
whichever way it points.

The estimator is approximate, and the defaults come from measuring where it stops being trustworthy.
`scripts/check-branch-de.js` builds the same tree forwards as explicit capsules and compares: within a
tenth of a percent at narrow angles with no twist, degrading with angle, with twist, and with every
generation. That worst case is taken over the whole volume, most of it deep inside the canopy where
rays have already terminated, so it predicts the picture poorly - the shipped settings were chosen by
rendering instead. The tree at eleven generations tore into speckle that a step factor of 0.05
removed, which is tunnelling and not noise; at seven generations all six render clean at 0.35 and
still read as branches. Hence seven generations by default and ten at most. If you raise the
generation count, lower Step factor to match - roughly halving it per extra generation.

## Stability design

See the header of `js/renderer.js`. In short: bounded GPU work per draw call (tiled refinement), no
drawing at all when idle, adaptive live resolution, async shader compiles, WebGL context-loss recovery,
and a safe start (preview waits for a click) after an unclean exit or when embedded.

## Files

- `index.html`, `style.css` — UI shell
- `js/formulas.js` — formula library (params + GLSL distance estimators)
- `js/palettes.js` — colour science, palette presets and generators
- `js/schema.js` — project schema, defaults, look presets, migration from the first prototype
- `js/evaluator.js` — pure frame evaluator (project + analysis + t → frame)
- `js/renderer.js` — WebGL2 renderer
- `js/audio.js` — IndexedDB store, analysis, transport
- `js/structure.js` — song structure: FFT, log-mel/chroma, self-similarity, segmentation (pure, no DOM)
- `js/keys.js` — keyframe lanes, the slider right-click menu, and the per-scene on/off control
- `js/vr-mode.js` — WebXR session: flying, scaling, the hand gesture for parameters, and the panel
- `js/ui-*.js`, `js/app.js` — panels and app glue
- `server.js` — local server; also runs movie renders (frames → ffmpeg)
- `render-driver.js` — Node driver (validates a project and evaluates every frame)
- `scripts/check.js` — headless tests (`npm run check` runs this and the structure tests)
- `scripts/check-structure.js`, `scripts/structure-fixture.js` — structure tests against synthetic
  songs whose form is known exactly
- `scripts/check-branch-de.js` — checks the Nature family's distance estimator against the same tree
  built forwards as explicit capsules
- `_old-prototype/` — the previous version, kept for reference (safe to delete)
