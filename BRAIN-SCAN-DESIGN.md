# BRAIN SCAN — design

*A Brokild synth. Written 2026-09-04 from Peter's brief, before any code, in the
house manner: every claim made here is something `test/bench.cpp` measures
against the real engine. Idea note: `WAVE-SCAFFOLD-DESIGN.md` (the concept,
its lineage, and why it is its own instrument and not a High Tide mode).*

Peter's brief, condensed: an entirely different thing from High Tide —
different design, style and controls; reuse of the sound engine allowed. The
waveform, the filter and everything else move between arbitrary lines in a 3D
space projected onto the screen; slice-based ways of seeing the waveform; a
semi-transparent (variable opacity) 3D view where the waveforms emerge as
colours until they block the view; a waveform inspector like tomography slices
on any of XY, XZ, YZ. Called **Brain Scan**, themed as a CT scanner, still
recognisable as an instrument, with exquisite knobs and sliders reminiscent of
medical equipment. One stock volume is a simulated brain; the others should be
the source of musically normal, useful waveforms, and need not be pretty.

## 0. The one-line thesis

**Store a volume, not a waveform, and read it along a line.** A specimen is a
scalar field `V(x, y, z)` in the unit cube. A scan line is a curve through it.
One cycle of the waveform is the field along the curve: `w(s) = V(g(s))`, `s`
the phase. The filter's cutoff and a free modulator are the *same thing read
slower* — a second and a third line, traversed once per note or looped. And
the SCAN position blends the *geometry* of two anchor lines, so the midway
waveform is read from tissue neither anchor visited: a wavetable crossfades
two answers, Brain Scan moves the question.

## 1. Three scanners, one law

| scanner | reads | rate | anchors |
|---|---|---|---|
| WAVE | the audio waveform | once per cycle of the note | WAVE A → WAVE B |
| FILTER | the cutoff of a state-variable filter | once per note over SWEEP (env) or looping at RATE (lfo) | FILTER A → FILTER B |
| MOD | a bipolar modulator → scan, pitch, pan | looping at its own RATE, free or synced | MOD A → MOD B |

One **SCAN** value (0..1) blends every A into its B at once — the base knob,
plus a per-note SCAN ENVELOPE (attack, decay, amount), plus MOD's own
contribution. Blending is of the *positions*: `g(s) = (1 − scan)·A(s) +
scan·B(s)`, then one read of the field. That is the whole difference from a
wavetable, and the bench proves it (§7, item 5).

Lines: 2–16 control points, Catmull-Rom through them, **open or closed**,
a START mark (where the loop's cycle begins) and a WARP (phase distortion:
the read runs faster over one half of the line). What geometry gives for free:

| line shape | waveform |
|---|---|
| closed loop | periodic, no edge: soft |
| open segment | one edge per cycle (the jump between its ends), band-limited by a polyBLEP: bright, saw-like |
| corners | kinks at fixed phases |
| a self-crossing | a value revisited at two phases |
| warp | the same shape read unevenly — phase distortion |
| the whole line moved through the volume | the timbre evolves, the instrument stays |

## 2. Specimens — the stock volumes

`x` is the phase axis: a straight line along x reads the "natural" waveform of
the specimen at that (y, z). `y` and `z` are the two timbre axes, so a plain
wavetable is just a straight line moved in y or z. Volumes are 64³, built by
code at load, in [0, 1]; the panel gets the bytes for its own rendering.

| specimen | what a straight line along x reads | y | z |
|---|---|---|---|
| SINUS | a sine (fundamental, pure) | amplitude | a touch of second harmonic |
| SPINE | a harmonic stack (the wavetable classic) | brightness (1/n⁴ → 1/n) | parity (all → odd only: saw → square) |
| PULSE | a pulse | width 5 % → 95 % | edge softness |
| MARROW | a ramp | curvature (bend) | fold toward a triangle |
| NERVE | a phase-modulated sine | modulation index | modulator ratio |
| RETINA | a decaying two-formant cycle (vowel-like) | first formant | second formant |
| LUNG | smooth 3D noise — a rough, breathy, metallic tone on a loop | — | — |
| SKULL | a hollow sphere: two bumps per cycle through the centre | — | — |
| CORTEX | the simulated brain: skull, CSF, folded cortex, fissure, ventricles, cerebellum, stem — the show-piece, not a wavetable | — | — |

Musically normal by construction: SPINE at (y, z) is exactly a harmonic series
with a known rolloff and parity; PULSE at y is a pulse of known width; the
bench checks both against their formulas.

## 3. Aliasing, DC, edges — the three traps, each measured

- **Mip pyramid.** A path of length `L` texels at level 0 carries up to `L/2`
  harmonics; at pitch `p` the top one is `p·L/2`. Each note picks the level
  `l` where `p·L/2^(l+1) < 0.45·fs`, blending between two levels, and reads
  with a **tricubic B-spline** (C², smoothing) so texel boundaries are not
  corners. Bench: the roughest specimen at C6 keeps non-harmonic energy under
  −50 dB.
- **The edge.** An open line jumps at the wrap. The jump `Δ = w(0) − w(1⁻)` is
  known per tick, and a **polyBLEP** of that height is laid over the wrap.
  Bench: MARROW at C5 under −50 dB with it, ~20 dB worse without.
- **DC.** The mean of the field along the line moves with every scan. A 5 Hz
  blocker after the sum; bench checks a straight line through a lopsided
  field settles under −60 dB of offset.

## 4. The voice

`phase → blended position → field read (mip level, B-spline) → polyBLEP →
SVF (LP / BP / HP, cutoff from the FILTER scanner, key-tracked) → ADSR ×
velocity → pan`. Unison 1–4 with detune and stereo spread, glide, mono/poly.
Master: soft ceiling, DC block, level. BWFX rack after the sum, five macros;
SPECTRA bus consumed (detune fanned per unison voice, sag on the gate,
filterMul on the cutoff, neutral bit-identical — the fleet rule).

## 5. Panel — a CT console, 1995

- **GANTRY (the 3D view).** WebGL raymarched volume of the specimen, bone-
  white on near-black, with the six scan lines as glowing tubes (WAVE red,
  FILTER cyan, MOD amber; the B anchors dimmer; the *blended* line bright,
  with the read cursor running on it). A DENSITY slider is the transfer-
  function opacity: low, the lines float in a ghost; high, the tissue closes
  over them. Drag to orbit, wheel to zoom.
- **TOMOGRAPHY (the inspector).** AXIAL / CORONAL / SAGITTAL membrane buttons
  choose XY / XZ / YZ; a TABLE slider moves the plane through the volume. The
  slice is drawn CT-style (window/level), with the lines' crossings marked and
  the line under edit shown as its projection. **Lines are edited here**: a
  point dragged on the plane keeps the plane's depth; a click on the plane
  adds a point at that depth. That is how a 3D line is drawn with a mouse.
- **MONITOR.** An ECG-style strip: the current cycle of the WAVE read (LEAD I,
  phosphor green), the FILTER read (LEAD II, cyan), the MOD read (amber), and
  vitals: note, cutoff, scan.
- **Controls.** Rotary knobs and faders with the register of medical
  equipment (decals ordered: `assets/brain-scan-decals/BRIEF.md`); square
  membrane buttons; amber status lamps; a red mushroom STOP for all-notes-off.
  Rows: SPECIMEN · SCAN (position, env A/D/amount) · WAVE (line select A/B,
  closed, start, warp, shape presets, randomise) · FILTER (type, cutoff, reso,
  depth, mode, sweep/rate, sync, track) · MOD (rate, sync, → scan, → pitch,
  → pan) · AMP (ADSR, velocity) · VOICE (poly/mono, unison, detune, spread,
  glide, tune, level).
- House rules: teal BWFX globe, five macros, patches in
  `Documents\Brokild patches\Brain Scan`, the loading splash, the About modal.

## 6. Architecture

- JUCE 8, VST3 + Standalone, native-first: `SPECS[]` walked by the APVTS,
  processBlock, factory, initialState and the bench. PRODUCT_NAME `Brain
  Scan`, PLUGIN_CODE **`BrSc`** (checked unique against every `b\*\CMakeLists.txt`
  and Clone Wars on 2026-09-04). Tree `C:\Users\peter\b\BrainScan`; private
  repo `brokild-brain-scan` when it exists (committing IS the backup).
- The lines and the specimen index are state, not host parameters — a JSON
  blob beside the APVTS XML, the BWFX way. SPECIMEN is a parameter but not
  automatable (the mood-organ precedent).
- The chassis (editor splash, WebView2 profile, patch-file vocabulary, param
  echo) is High Tide's, adapted. The sound engine is new; the ADSR and SVF are
  the fleet's usual.
- Cost: per sample per unison voice, two spline evaluations and one 64-tap
  B-spline read. 8 voices × 4 unison measures **11.0 % of one core** — the
  bench asserts under 15 %. (The design said "well under 5"; it was wrong,
  and this is the measurement.)

## 7. The bench — `test/bench.cpp`, plain C++, no JUCE

1. **SINUS is a sine.** A straight line along x at (0.5, 0.5): THD < −60 dB.
2. **SPINE obeys its formula.** Centroid rises monotonically with y; at z = 1
   even harmonics sit > 40 dB under the odd.
3. **PULSE obeys its width.** A straight line at y reads a pulse whose duty
   is the formula's within 2 %.
4. **The three traps.** LUNG at C6: non-harmonic energy < −50 dB (mips). MARROW
   at C5: < −50 dB with the polyBLEP, and the polyBLEP is worth > 20 dB. DC
   after 1 s < −60 dB.
5. **THE CLAIM: path blending is not a crossfade.** Anchors: PULSE at y = 0.2
   (width 0.23) and y = 0.8 (width 0.77). Both have a spectral null at the
   5th harmonic; the midway *path* (y = 0.5, width 0.5) does not. At SCAN 0.5
   the 5th harmonic must read within 3 dB of its formula while the crossfade
   of the two anchor waveforms keeps it under −40 dB.
6. **The FILTER scanner follows its line.** Cutoff trace vs the field along
   the blended line, max error < 2 %.
7. **Silence in, silence out; determinism memcmp; every factory patch and
   every specimen bounded and audible; 8 voices × 4 unison — 32 readers, the
   maximum — under 15 % of a core (measured 11.0 %).**

## 8. Build order

1. Engine + Specimens + bench, headless — items 1–7.
2. Plugin shell (from High Tide), lines/specimen state, patch files, factory.
3. Panel: gantry (WebGL volume), tomography, monitor, controls; verify LIVE
   over CDP (the missing-`</script>` lesson).
4. Manual, landing page, zip, app.json, manifest, install, push — the full
   release, as always.

## 9. Decisions — 2026-09-04

- Its own instrument, not a High Tide mode (WAVE-SCAFFOLD-DESIGN.md §6).
- One SCAN for all three scanners. Blend positions, never samples.
- Lines are edited on the tomography plane; the 3D view is for looking.
- The brain is the show-piece; SPINE is the instrument's bread.

## 10. What the build taught — 2026-09-04

Every line here is a number the bench, the panel probe or a live CDP session
produced. The engine and the specimens went in first and cleanly (38 checks);
everything below is from the panel and the plug-in shell.

**`bs::Line` and `juce::Line<T>` are ambiguous, and only the plug-in can
see it.** The engine and its bench never include JUCE, so the clash appeared
for the first time in `PluginProcessor.cpp`, which has `using namespace bs`
and every JUCE header — twenty errors from one name. Qualify the type at every
use in a file that has both. The same file also called
`DynamicObject::getProperty` with a default argument; it takes one.

**A raymarched volume needs a gradient or it is fog, whatever else is tuned.**
Three rounds went into the opacity before the real fault was admitted: there
was no light on the tissue. A central-difference normal plus a headlight and a
rim term turned the specimen from haze into an object, and did more than any
transfer-function change before it.

**A scanned object and a field that fills the cube want OPPOSITE transfer
functions.** A ramp shows a brain whole and turns a waveform volume into an
opaque brick; a band shows the waveform's surface and hides the brain's skull.
So it is a control — SOLID or SURFACE — chosen from the histogram when a
specimen loads (SOLID if more than a third of the cube is air) and overridable
from the panel. The window is auto-set with it, the way a scanner picks a
window per protocol, and both sliders move so the panel shows the truth.

**The opacity was eight times too high, and arithmetic said so.** A ray crosses
the head in about 56 steps, so a ghost that still shows the lines inside it
wants a per-step alpha near 0.016, not 0.13. DENSITY now runs `0.15 + 6 d²`,
which measures as transmittance 0.87 / 0.44 / 0.05 across the slider.

**One window serves two views, so it has to suit both.** Pushing the window's
floor down to the 2nd percentile made the gantry read better and blew the SLICE
out — everything above the 58th percentile clipped to white and the cortex
detail went with it. p05 to p85 satisfies both.

**"The head renders too small" was false, and only a measurement settled it.**
Three separate attempts to find a scale bug failed because the first probe ran
with a stale specimen's window (a band correctly makes a *constant* field
invisible — the probe lied before the code did). Rendering a solid volume and
the bare wireframe from the same camera gave identical bounds, fw 0.272 fh
0.394, matching the analytic `(0.5/2.05)/0.62`. The geometry was exact
throughout; what was small was the *bright* part, because a squared ramp took
the outer tissue's a = 0.19 down to 0.037.

**A menu's closer must not eat the click it was armed for.** A document-level
`pointerdown` closer removes the menu before the item's `click` ever fires;
a pointerdown inside the menu has to re-arm instead of closing.

**The panel is proven three ways, and they are not interchangeable.**
`node --check` on the extracted script (which cannot see an unclosed tag —
B2311.104's lesson); `test/uiprobe.js`, which loads the real page in headless
Chrome and feeds it the processor's own vocabulary, 24 checks including that a
straight line of four collinear points reads straight to five decimals, i.e.
the page evaluates a line exactly as `Engine::Line::at` does; and a live CDP
session against the standalone, which is the only one that proves WebView2,
the bridge, the 262 144-texel volume stream and the audio actually work
together.

**Hints are not optional here.** Every parameter carries the gloss the
processor already sends, and the probe fails if any control lacks one — 69
hints, placed beside the control and never over it (High Tide's round, applied
from the start).

## 11. The import — 260904.2

Peter asked whether a real open CT could be a specimen. The honest answer was
that the DSP is nothing (`Volume::build` is the whole seam), that at 64³ a real
brain would be MUSHIER than the procedural CORTEX because cortical sulci are
below the sampling limit, and that the licence and the face-in-the-render
questions are clearance work rather than code — so an IMPORT is worth more than
a bundled scan, and subsumes it. He said build it.

**It overrides the specimen dial; it does not join it.** A tenth entry in a
nine-slot normalised parameter would silently re-point every saved patch —
1.0 means CORTEX today and would mean IMPORTED tomorrow — in project state as
well as in patch files, with nothing to migrate from. So the import lives in
the state blob beside the lines, the volume slot carries a sentinel, and no
existing patch changes meaning.

**Three things decide whether an import is any good, and none is the file
format.** Each is a bench check with a number.

- *Anisotropy.* A clinical CT is ~0.5 mm in plane and 1–5 mm between slices.
  Resampling by index squashes the body by exactly that ratio. Bench: a volume
  of 80 × 80 × 20 voxels at 0.5 × 0.5 × 2.0 mm is physically cubic, so it must
  fill the cube, and a sphere in it must measure the same on all three axes.
  It reads 48 / 48 / 44 — the 4 is partial volume in the source, one voxel of
  the coarse axis — where an index-based resampler would read about 12.
- *Decimation.* 512 × 512 × 300 into 64³ is ~8 × 8 × 5 source voxels each.
  Aliasing introduced here is in the specimen for good. Bench: a one-voxel
  checkerboard decimated 4:1 flattens to sd 0.00005; point sampling leaves 0.5.
- *Outliers.* One surgical clip at 3000 HU crushes every brain voxel into the
  bottom 2 % of a min-to-max window. Bench: one voxel at 30000 among values of
  0–100 gives a window of 0–103.

**A reader must be right or refuse out loud.** Compressed DICOM transfer
syntaxes and NIfTI-2 are named and refused with the remedy (`dcm2niix`), never
guessed at. Both readers are round-tripped in the bench from bytes synthesised
in memory, so the parsing is measured rather than mocked.

**Slice order comes from position, never from the filename.** A directory
listing is not slice order, and getting it wrong shuffles or mirrors the body
with no other symptom. Slices are sorted along the slice normal (from the cross
product of ImageOrientationPatient) and the slice spacing is derived from the
positions rather than believed from the tag. Bench: five slices fed in the
order 8, 0, 4, 12, 16 come back as 0, 4, 8, 12, 16 with dz = 4.00 mm.

**The bug the live test found, that nothing else could.** `emitVolume` began
`if (v.specimen < 0 ...) return;` — the guard for "nothing built yet", which
is −1. `SPEC_IMPORTED` is −2, so an imported volume was built, played and
reported correctly while the PANEL was never sent it: the slice and the gantry
went on showing SPINE. Every static check passed. Only reading the page's own
copy of the volume back over CDP showed the checksum had not moved.

**A modal file chooser cannot be driven by a probe**, so the same operation
also exists with the path handed in (`{k:"importPath"}`). That is what makes
the feature verifiable at all, and it is what a file dropped on the panel would
call.

**What it changes about the instrument.** With a CT loaded, WINDOW and LEVEL
stop being a metaphor and become a radiographer's window width and level in
Hounsfield units. And it need not be a brain: any volumetric data is a
specimen — a micro-CT of a fossil has the same novelty and none of the licence
or privacy questions.
