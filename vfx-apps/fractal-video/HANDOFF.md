# Fractal Video — handoff (2026-10-28, updated 2026-11-01)

Main copy since 2026-11-01: BrokildApps/vfx-apps/fractal-video (the old Dropbox folder 00 VSCODE/Fractal Video is retired).

## State

Rewrite of the first prototype (kept in `_old-prototype/`). Covers most of PLAN.md stages 1–2 and the
authoring half of stages 3–4:

- 15 formulas with real distance estimators: Mandelbulb (+ Julia mix), Mandelbox, Amazing Box,
  Menger KIFS, Sierpinski, Octahedral KIFS, Kaleidoscopic IFS, Folded bulb hybrid, Quaternion Julia,
  Apollonian, and (2026-10-29) Pseudo-Kleinian, KaliBox, Amazing Surf, Icosahedral KIFS (Knighty type-5 fold,
  offset = weighted cell corners), Hyperbolic honeycomb ({4,3,n} cubes in the Poincare ball, camera inside the
  central cube; Travel 0.486 = one cube at n=5, so it loops). `unbounded` is a per-formula flag now; a formula
  may set camera fov/tx/ty/tz. Defaults chosen by rendering each one (contact sheets), not by guess. Space ops: repetition, twist, bound sphere. Orbit-trap colouring, sun + fill light,
  soft shadows, AO, fog, glow, ACES grade.
- Per-scene colour schemes (Look tab), look presets, palette blending across transitions.
- Timeline: contiguous sections, scene per section, transitions landing on the downbeat
  (different formulas cut at the midpoint inside a fog swell; camera is cut with them).
- Audio: master + stems, offline analysis (RMS, low/mid/high bands for the master, onsets → pulse track,
  waveform peaks), tempo suggestion, stem-vs-master alignment check, files + analysis kept in IndexedDB.
- Modulation matrix: grid / master / per-stem sources → shape params, space, camera, colour, light;
  depth, attack/release, look-ahead; signal plot over the song.
  Routes are switched on/off PER SCENE (2026-10-28): `row.off` = scene ids where the route is off
  (old global `on:false` migrates to off everywhere). The row switch acts on the selected scene, the
  `all` / `2/3` chip opens a strip for the rest; across a transition a route's share follows the blend
  (`Schema.routeOn`, weight in `evaluate`). Duplicating a scene copies its on/off; deleting one cleans it up.
- Keyframes (2026-10-29, `js/keys.js`): right-click a Shape / Space / Ray march / numeric Look slider to
  keyframe it in the selected scene; then moving it writes a key at the playhead (quarter-beat snap) and the
  slider follows the playhead. `project.keys` = lanes `{dest, interp smooth|linear|step, off[], pts[{sec,b,v}]}`,
  points stored RELATIVE TO THEIR SECTION (split/merge keep absolute positions). Evaluator order: scene →
  blend → keys (absolute, weighted by the lane's scenes like routes) → routes add on top. A key over a section
  of a scene the lane is off in is refused with a toast. Timeline shows one lane per parameter (drag a
  diamond, right-click deletes); Modulation tab lists lanes.
- Camera keyframes (2026-10-29): K or "Keyframe camera" makes lanes cam.dist/yaw/pitch/roll/fov/tx/ty/tz (on in the selected
  scene) and keys the current pose; then mouse orbit/pan/wheel, Reset (F) and the Camera sliders write keys at the playhead
  (`Keys.setCamera`, the drag starts from the KEYED pose via `Keys.cameraValue`). Yaw/roll interpolate the short way,
  distance in log; auto-orbit is not keyable and adds on top of a keyed yaw (`camera.orbitYaw`); across a formula change
  camera keys CUT at the midpoint with the shape. Timeline and Modulation tab show the camera as ONE row (a diamond = all
  camera values at that beat). Frames now carry the evaluated `camera` pose. 17 checks.
- Auto-flight (2026-11-01, `js/flight.js`, Shape tab -> Auto-flight): a camera that skims the surface by itself. Computed
  ONCE (Compute flight) and stored in `project.flight.track` (x,y,z,yaw,pitch,roll every 1/4 beat per run of consecutive
  flown scenes), so preview and movie match; the evaluator plays it back (Catmull-Rom) after keys, before routes, in Timeline
  view only. The simulator (30 Hz) asks a GPU probe (`renderer.probe`: the formula's own mapD, 32x1 float target) for the
  distance, its gradient and 25 marched rays (24 look-ahead + 1 straight down). Height is held on the DOWN RAY, not on the
  distance estimate (the DE is a lower bound, 2-5x too small on the Mandelbulb, so holding it flew far too high); the DE is the
  safety net (push out divided by the gradient length; never move more than half the free distance). A formula cut restarts
  the flight from the new scene's camera (the old position may be inside the new shape); playback cuts/blends like the camera
  does in transitions. Up = surface normal (85 %), so the surface stays below. `flightSig` marks a path out of date.
  Tests: 2 async checks in scripts/check.js against exact distance functions (plane, sphere).
- `Start Fractal Video.cmd` = `npm start` + opens the browser.
- Movie quality presets (2026-10-29, `Schema.MOVIE_PRESETS`): Quick test / Preview / Standard / High / Final =
  size scale x samples x ray-march quality (draft = no shadows) x crf; the renderer takes `opts.march` for movie
  frames (stills stay High). Speeds in the menu are ESTIMATES (pixels x samples x a guessed march factor), not measured.
- Evaluator is pure and deterministic (`js/evaluator.js`), shared by preview, still render and the Node driver.
- Still render: current frame at output size, tiled, N samples, PNG download.

## Stability fixes vs. the old prototype

The old app: drew the full-song waveform every frame on the main thread (millions of samples per frame),
rendered continuously at up to 1280² even when idle, never handled WebGL context loss, rebuilt the mod
matrix while sliders were dragged, and ignored the mod matrix in the evaluator. A GPU-heavy page that
also restarts itself when its host restores it (VS Code Simple Browser) can loop crash → reload → crash.

Now: tiled refinement bounds each draw call; nothing is drawn when idle; adaptive live scale; async
shader compiles per formula; context loss → Draft, second loss → stop until clicked; safe start after an
unclean exit or when embedded (`fv-session` marker in localStorage; `?autostart=1` bypasses it for tests,
`?safe=1` forces it).

## Verified (headless Chrome, SwiftShader)

All 15 formulas compile and render; Node checks (`npm run check`, 19 tests); render-driver dry run over
2400 frames; synthetic 120 BPM master + kick/bass stems: tempo suggested 120 / 0.0 s, onsets on the beat,
alignment 0 ms, kick→power route moves power on the beat; still PNG; context loss + restore; real renderer
crash → safe start; audio restored from IndexedDB after reload; layout at 1600×1000 and 1280×760.

Not verified: real-GPU frame rates on the T1000, real Ableton stems, long (10 min) files.

## Added 2026-10-28 (later)

- Backdrops: `scene.backdrop` (schema `BACKDROP`), `js/backdrop.js` (media store: ImageBitmap / muted
  `<video>`, version counter), evaluator emits `gpu.backdrop.layers` (texture transform, weight, movie
  time), renderer composites up to two layers (A/B in a transition) on texture units 2/3 and uploads on
  version change. Movie time is decided by the evaluator, so a frame-accurate render only has to call
  `backdrops.prepare(frame)` (seek + wait) before each frame — the still render already does.
  `backdrop.place` = behind | front; front layers are blended after shading and glow (`overlayBlend`:
  normal/screen/add/multiply/soft light, texture alpha honoured, images decoded with straight alpha)
  and do not take part in the fog fade. One layer per scene; behind + front at once would need a
  second slot per scene (the shader already takes two layers).
- 22 dark palettes (Wasteland, Dark & grim), 8 gritty look presets, film grain (display pass, seeded per
  1/24 s of song time).

## Movie render (built 2026-10-28)

Render tab → Movie. In the page: `Ev.evaluate(project, analysis, n / fps)` (timeline, modulated) →
`backdrops.prepare` → `renderer.renderFrame` (same tiled High-quality job as the still, raw RGBA rows
bottom-up, render targets kept while `renderer.setHold(true)`) → POST to the server. `server.js`
`/render/{check,start,audio,frame,finish,cancel}` (header `X-FV-Render: 1`, same-origin only) pipes frames
into ffmpeg (`-vf vflip`, libx264 CRF, yuv420p, faststart; master uploaded once, `-ss start`, AAC 320k,
`-t` = range length). Soundtrack: AAC mode pre-encodes the used range (`prepareAac`: static gain
from the master's true peak, then lowered until the decoded AAC sample peak <= -0.3 dBFS, 10 ms fade-in when
the range starts mid-song) and muxes it with `-c:a copy`; PCM mode writes pcm_s24le in .mov, bit-exact
(verified with md5 on 'For Morket - Mix_Master #2.wav': -7.8 LUFS, LRA 3.2 → AAC needed -1.7 dB, 0 overs). Upload of frame n overlaps rendering of n+1. Cancel kills ffmpeg and deletes the
partial file; abandoned jobs are killed after 30 min without frames. Output: `<outDir>/<name>-<stamp>.mp4`
(relative outDir = inside this folder). ffmpeg is found on PATH, `FFMPEG`, or the winget package folder.
Verified headless: 60-frame 320×180 range with master → valid H.264 + AAC, 2.000 s, correct frames;
cancel cleans up. Not verified: long full-HD renders on the T1000 (the tab must stay visible).

## Next (PLAN.md order)

1. Optional headless/CLI movie render (`render-driver.js` driving Chrome over CDP with the same page code).
2. Sensitivity sweep per parameter (replace the static "calm" bands with measured ones), visual speed limit.
3. Shape variants (morph targets A/B/C) as a modulation destination; camera spline + auto-flight with DE safety.
4. Events (flash limiter), MIDI lanes, section mask per route, parameter plot beside the stem lanes.

## VR movies (built 2026-11-01)

Render tab -> Movie -> VR format: 360 stereo top/bottom (left eye on top, square frame), 180 stereo side by side
(VR180, 2:1), or 360 mono (2:1). The fractal shader gets an equirectangular camera (uVR, uVRLayout, uVRSpan,
uIpd): omni-directional stereo, each eye offset at right angles to the horizontal view direction, faded by
cos(latitude) so the poles do not swap the eyes. Eye spacing = Stereo depth (default 3 %) x the evaluated camera
distance, so the felt scale stays put through zooms and auto-flight. "Keep the horizon level" drops the roll in
the render only. Backdrops are placed in front, as in the flat view; vignette is off in VR. VR movies are H.265
(libx265, crf+2, hvc1 tag); the server injects Spherical Video V2 metadata (st3d + sv3d/equi, vr-meta.js, also
shifts stco/co64 when moov is first) and ends the file name with _360_TB / _180_LR / _360. "VR still (PNG)"
renders the current frame in the VR format. Verified headless: all three formats render, stereo direction
correct (exaggerated spacing: left-eye image shifted right), a 360 TB movie through the server reads back in
ffprobe as hevc, top and bottom, equirectangular. Not verified: in a headset or on YouTube.
Watching: YouTube (reads the metadata), or a Quest's Files/Gallery player, Skybox, DeoVR (name tag).
