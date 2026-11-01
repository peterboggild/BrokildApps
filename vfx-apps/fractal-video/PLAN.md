# Fractal Video: plan

Working title. A 3D fractal renderer (Mandelbulb3D family) driven by a piece of music, for **music videos**:
rendered offline at full quality, frame by frame, exactly in sync with the audio. Not a live visualiser.

Status 2026-10-27: plan only, nothing built. **Stems are a core feature** (Peter, 2026-10-27): they are what
lets a change be keyed precisely to one instrument.

Contents: 1 what it does · 2 design rules · 3 stems · 4 architecture · 5 fractal engine · 6 analysis ·
7 authoring · 8 render · 9 tests · 10 stages · 11 effort and credits · 12 open questions

## 1. What it does

You export the track from Ableton as a mixdown plus stems (and optionally MIDI). You load them, set the
tempo grid and the song's sections, author a few **scenes** (fractal + lighting + palette you like) and
lay them along the sections. A **modulation matrix** turns each stem into movement. Most of it goes to
camera, light and colour; slow, smoothed amounts go to the structure itself. You preview at low
resolution with the music playing and can scrub. Then you press Render: every frame is drawn at full
quality from its frame number, and ffmpeg muxes the frames with the mixdown into an MP4.

## 2. Design rules

1. **The music can reshape the fractal, but measured, not blind.** Fractal parameters are uneven: in
   one range a parameter bends the form gently, a little further it tears it apart, and a transient
   wired into a wild range is flicker. So shape changes are a first-class destination with their own
   safeguards (section 5.1). Three tiers:
   - *Fast* (any transient): camera push or lurch, light pulses, glow, fog, colour, depth of field,
     and **small shape pulses** that flex the form and return (minor changes, kept to calm ranges).
   - *Shape* (beats, bars, phrases): **medium changes** to the form itself — formula parameters and
     morphs between shape variants, rate-limited by how much the *image* changes, not the number.
   - *Structural* (sections): travel between authored scenes. The in-between states are new
     fractals, not crossfades, because the parameters are blended and then rendered.
2. **Offline means look-ahead.** Everything is analysed before frame 1, so any source can be read
   *ahead of time* (−0.5 to −4 bars). The camera can start climbing before a drop.
3. **Time comes from the frame number.** `t = frame / fps`, no clock. The same project gives the same
   frame every time.
4. **The tempo grid is given, not guessed.** BPM + first downbeat (+ tempo changes), entered by hand;
   automatic detection only suggests.
5. **Stems are the main modulation sources; the mixdown is the soundtrack.** Spectral splitting of a
   mix is kept only as a fallback.
6. **Flash safety is built in.** Large brightness changes limited to 3 per second, checked on the
   finished frames.
7. **Quality is per frame, not per second.** Each frame accumulates many jittered samples
   (antialiasing, soft shadows, depth of field, motion blur). Real-time speed is only needed for preview.

## 3. Stems (the core of the music keying)

### 3.1 Why stems
A kick, a bass line and a pad all live in the low end of a mix; no filter separates them cleanly. With
stems, "the kick pushes the camera" means *the kick*, not "something below 100 Hz", and "the bass
breathes the structure" is not fooled by the kick. Each instrument gets its own clean envelope,
onsets and (for melodic parts) pitch.

### 3.2 How to export from Ableton Live
- File → Export Audio/Video → **Rendered Track: All Individual Tracks** (or select groups first and
  export them, e.g. a DRUMS group split into KICK / SNARE / HATS).
- Same **start and length** for every file (start at bar 1, or at a fixed bar, the same for all).
- WAV, 48 kHz, 24-bit. **Also export the Master** (the mixdown) in the same pass.
- Decide once: stems *with* their effects (what you hear) or dry. Default: with effects, returns off
  (reverb tails would otherwise smear the onsets). The app can take both.
- Optional, and even more precise: **MIDI**. Export the MIDI clips of drum and melodic tracks
  (consolidate first so each track is one clip from bar 1). MIDI gives exact note-on times, velocity
  and pitch, which audio analysis can only estimate.

### 3.3 Stem roles
Each loaded file is given a role, which picks sensible default analysis and suggested routings:

| role | typical analysis | suggested use |
|---|---|---|
| KICK | onsets, punch envelope | camera push, light pulse on the hit |
| SNARE / CLAP | onsets | flash (limited), cut to the next camera key, glow burst |
| HATS / PERC | onset rate, density | sparkle: glints, small orbit-trap highlights |
| BASS | envelope, pitch | structure breathing (slow), ground height, low light colour |
| LEAD / VOCAL | envelope, pitch, brightness | hero light, lens, palette position follows pitch |
| PADS / ATMOS | slow envelope, brightness | fog density, sky colour, scene-blend drift |
| FX / RISERS | envelope, with look-ahead | camera climb into a drop, glow build |
| MASTER | loudness, sections | overall exposure, section detection |

Roles are only defaults: any stem can drive anything.

### 3.4 Alignment check (automatic)
On load, the stems are summed and compared with the master by cross-correlation. If they are offset
(a stem exported from a different start), the app shows the offset in ms and fixes it; if the sum does
not resemble the master at all (wrong files), it warns. A stem that is silent for the whole song is flagged.

### 3.5 MIDI lanes (optional)
A .mid file per instrument becomes a lane of exact events: note on (time, pitch, velocity), note off.
Sources from it: "any note", "note in range", velocity, pitch, note count (chords), sustain. Note
events are read on the same tempo grid as the audio.

## 4. Architecture

One HTML app (WebGL2, self-contained, offline) plus a small Node render driver. Preview and final
render use the same shader, so what you preview is what you get.

```
stems + master + MIDI ──► ANALYSIS (JS, once, cached) ──► feature tracks at video fps + beat grid
                                                                │
project.json ◄── AUTHORING UI (scenes, timeline, camera, mod matrix)
                                                                │
                                 EVALUATOR: frame n → full parameter set (pure function)
                                                                │
                      ┌─────────────────────────────────────────┴──────────────────────┐
             PREVIEW (low res, master playing, scrub)                RENDER (full res, many samples)
                                                                                        │
                                                    Node + CDP driver: frames → ffmpeg pipe → MP4 + master
```

- **The evaluator is a pure function** `params(frame)` of project + analysis. Preview, render, tests
  and the parameter plot all call it.
- **Render driver**: Node launches a *visible* Chrome on the real GPU (headless Chrome falls back to a
  software renderer), drives the page over CDP, reads each frame back and pipes raw RGB into ffmpeg
  (`C:\ffmpeg\bin\ffmpeg.exe`). Resumable from any frame.
- Tools on this PC: Node 24, Chrome, ffmpeg, NVIDIA T1000 (4 GB). The GPU sets *render time*, not
  what can be made. Frame cost is measured in stage 1 before any render time is promised.

## 5. Fractal engine (GLSL)

- **Distance-estimated raymarcher**, float32 (music videos do not need deep zoom).
- **Formula chain** (hybrids, the Mandelbulb3D look): up to ~6 slots, each a formula with its own
  iteration count. First set: Mandelbulb, Mandelbox, Amazing Box variants, KIFS Menger / Sierpinski /
  octahedral folds, kaleidoscopic fold, plus transforms (rotate, scale, offset, sphere fold, box fold).
  Each formula carries its own distance-estimate fudge factor.
- **Space**: domain repetition (endless cities, corridors), ground / heightfield mode for landscapes, cut plane.
- **Colouring**: orbit traps (point, plane, ring) into an 8-stop palette, plus iteration count and AO tint.
- **Lighting**: sun + sky + 2 point lights, soft shadows, ambient occlusion, fog (distance + height),
  glow from near misses, emissive bands ("lit windows"), depth of field, bloom, grade.
- The formula chain is compiled into the shader when it changes, so unused formulas cost nothing.

### 5.1 Shaping the fractal with the music

Every continuous formula parameter is a modulation destination: Mandelbulb power and angle offsets,
box scale and fold limits, sphere-fold radii, KIFS rotation angles and offsets, per-slot weights in the
hybrid chain, domain-repetition spacing, heightfield amplitude. What makes this usable rather than a
flicker machine:

- **Three sizes of change**, chosen per matrix row:
  - *minor*: surface detail — small angle/offset changes, the last iteration's weight; on transients is fine;
  - *medium*: the form bends or grows — power, scale, fold angles over a calm range; beats to phrases;
  - *major*: a different structure — done as a morph or a scene change, landed on a bar or section.
- **Sensitivity map per parameter (measured, not guessed).** Capturing a scene runs a quick sweep: each
  parameter is stepped across its range, small preview images are rendered, and the image change per
  step is measured. The result is drawn under the parameter's slider as calm (green) and wild (red)
  zones, and a matrix row's range is drawn on it, so you can see whether a bass line is about to push
  the box scale into a region where it shatters. A row can be told to stay in calm zones.
- **Visual speed limit.** For shape destinations, smoothing is set as "how much may the image change
  per frame" and converted to parameter units through the measured sensitivity. The same bass
  envelope then moves a parameter quickly where the form is calm and slowly where it is wild, so the
  motion looks even instead of lurching through the sensitive bits.
- **Shape variants (morph targets).** From a scene, capture variants A, B, C… by tweaking the form
  (e.g. "folded open", "compressed", "spiked"). A source then moves between them: one control, several
  parameters moving together, along a path you have looked at. Often the nicest way to key a drop.
- **Shape pulses.** A kick can flex the structure: a short attack/decay that always returns to the
  base form, depth limited to the calm zone around the current setting.
- **Continuous iteration count.** Iterations blend fractionally, so changing detail does not pop.
- **Camera safety.** A form that grows can swallow the camera; the auto-flight and spline camera
  check the distance estimate at the *modulated* shape every frame and back off.
- **Look-ahead applies here too**: a morph can start before the drop and arrive exactly on it.

## 6. Audio analysis

Done once in JS on the decoded audio, cached in the project folder.

Per stem (and the master):
- envelope (RMS and peak);
- onset strength + discrete onsets, with a per-stem sensitivity;
- spectral centroid (brightness) and flux;
- **pitch** for melodic roles (BASS, LEAD / VOCAL): monophonic pitch tracking (YIN), with confidence;
- for the master only: band envelopes (sub / low / mid / high / air), loudness, section novelty curve.

All delivered at the video frame rate; attack/release are set per use in the matrix, not baked in.

Grid sources: beat phase, bar phase, bar number, section, "beats until next section".
Sections are set by hand on the waveform; the novelty curve can suggest them.

## 7. Authoring

- **Scenes**: named parameter sets (formula chain, lighting, palette, fog). Explore in the viewer, press *Capture*.
- **Timeline**: the master's waveform with section markers, and one lane per stem (its waveform +
  detected onsets, or its MIDI notes) so you can *see* what will drive what. Each section is assigned a
  scene or a blend; transitions have a length and a curve, and can land on a downbeat.
- **Camera**: keyframed spline path (position, look-at, roll, focal length) with a speed curve, plus
  *auto-flight* (forward travel per bar, keeping a safe distance from the surface using the distance
  estimate itself).
- **Mod matrix** (24 rows): source (stem + feature, MIDI lane, grid) → destination → depth →
  smoothing (attack/release) → curve → look-ahead shift → quantise (none / beat / bar) → active
  sections. Destinations: any animatable uniform, camera speed/offset/FOV/shake, scene-blend position,
  palette offset.
- **Routing templates**: one click sets up the suggestions in 3.3 for the stems loaded, as a starting point.
- **Events**: on a stem's onset, a MIDI note or a grid position: flash (limited), cut to the next
  camera key, palette step, glow burst.
- **Parameter plot**: any destination drawn over the whole song, beside the stem lanes, to read the
  motion before rendering.

## 8. Render and output

- Presets: 1920×1080, 3840×2160, vertical 1080×1920; 24/25/30/50/60 fps.
- Samples per frame (e.g. 16–256), motion blur over a shutter angle (default 180°).
- Encode: H.264 (CRF ~16) for delivery; H.265 or ProRes for editing. Soundtrack = the master, muxed as
  lossless or AAC 320.
- **Draft render**: quarter resolution, few samples, whole song, to check timing first.
- Output folder per render: video, the project as rendered, a frame-time log, a contact sheet (one frame per bar).

## 9. Tests (gates before a stage is called done)

- **Determinism**: frame k rendered twice in separate sessions, pixels compared.
- **Sync**: a test project flashes on every kick-stem onset; decode the finished MP4, find the flash
  frames, require them to match the kick onsets within one frame over the whole song.
- **Stem alignment**: a deliberately offset stem must be detected and corrected to within 1 ms.
- **MIDI vs audio**: on a test track, MIDI note-ons and the same stem's detected onsets agree within one frame.
- **Flash limiter**: large brightness swings per second on rendered frames ≤ 3.
- **Evaluator vs renderer**: the parameter plot and the values the renderer logged agree.
- **Distance estimate**: per formula, check the DE never oversteps (no holes from new formulas), also
  at the extremes of every modulated range.
- **Shape flicker**: on a draft render, the frame-to-frame image change caused by shape destinations
  must stay under each row's visual speed limit; frames that break it are listed with their time.
- **Sensitivity map honesty**: a parameter swept twice gives the same map, and a row told to stay calm
  never enters a red zone over the whole song.
- **Look at the frames**: the contact sheet is part of every gate.

## 10. Stages

Each stage ends with something usable, so stopping after any stage still leaves a working tool.

| stage | content | ends with |
|---|---|---|
| 1. Engine + viewer | raymarcher, 4 formulas + hybrid chain, orbit-trap colour, lighting, navigation, capture scene, save/load, frame-cost measurement, fractional iterations, sensitivity sweep + calm/wild zones under the sliders | a fractal explorer that shows where each parameter is calm or wild |
| 2. Stems + analysis | load master + stems, roles, alignment check, per-stem envelopes/onsets/pitch, grid, stem lanes on a waveform view, preview with audio + scrubbing | you can see and hear what each stem would drive |
| 3. Matrix + first render | mod matrix incl. shape destinations (minor/medium sizes, visual speed limit, shape pulses, shape variants A/B/C), routing templates, auto-flight camera with shape-aware safety, Node/CDP render driver, ffmpeg mux, draft mode, sync + determinism + shape-flicker gates | **first real music video** (one scene, auto-flight, stem-driven, the form itself moving with the music) |
| 4. Timeline + camera | sections, scene blends and transitions, camera spline, events, parameter plot | edited, multi-scene videos |
| 5. MIDI + polish | MIDI lanes, motion blur, more formulas (with DE tests), landscape presets, palettes, grading | full tool |

The first real video is deliberately at the end of stage 3, before the timeline work.

## 11. Effort and credits

I cannot see your remaining credits or plan, so these are **relative sizes**, not prices. They are
the share of the total work each stage is expected to take (code, tests, fixing, looking at frames):

| stage | share of total | rough working days |
|---|---|---|
| 1. Engine + viewer | ~22 % | 3–4 |
| 2. Stems + analysis | ~18 % | 3–4 |
| 3. Matrix + first render | ~28 % | 4–5 |
| 4. Timeline + camera | ~18 % | 3–4 |
| 5. MIDI + polish | ~14 % | open-ended |

Stages 1–3 (about two thirds of the work) give a first stem-driven video. Shaping the fractal
(section 5.1) adds roughly a tenth to the whole project, almost all of it in stages 1 and 3.

How to stay safe on credits:
1. **Give go one stage at a time.** Each stage is a separate session with a clear end.
2. **Measure after stage 1.** Check usage in Claude Code (`/usage`, or the usage page on claude.ai) before
   and after stage 1; it is a bit over a fifth of the whole, so multiply by about 4.5 for the total, by
   about 3 for stages 1–3.
3. If credits are tight, stop after stage 3: you have a working video tool. Stage 5 can be done in
   small pieces whenever wanted.
4. Long renders cost GPU time, not credits: once stage 3 works, rendering videos needs no Claude at all.

## 12. Open questions

- Name.
- Formats needed most (16:9 only, or vertical too).
- Stems with or without effects as the default export (plan assumes with effects, returns off).
- Where it should live (here in `00 VSCODE`, or with BrokildApps since it serves the music).
- A test track to build against, ideally with stems + master + MIDI exported as in 3.2.
