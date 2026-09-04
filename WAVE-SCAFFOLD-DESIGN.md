# WAVE SCAFFOLD — a 3D field read along morphing paths (design only)

Peter's idea, 2026-09-04, in his words: a wavetable synth uses a 2D concept to
expand the essential 1D audio file. Imagine a 3D waveform format, where the
sound played is extracted from a line path in 3D space, morphing into another
line. Straight lines at the bottom would behave like a conventional wavetable —
but the lines need not be straight: curved, segmented, even with loops, since
the waveform is whatever the amplitude is along the line. Any set of such 3D
lines could be anchors, and the playing cursor moves between them, morphing on
the way.

Working title WAVE SCAFFOLD (Peter's to rename). **No tree, no repo, no code.**
Collect-don't-implement applies. Status: design note only.

## 0 · What it is, in one line

A scalar field `F : R^3 -> R` (the material) and a parametric curve
`g : [0,1) -> R^3` (a scaffold). One cycle of the waveform is

    w(s) = F(g(s)),   s = the phase, 0..1 per cycle,

so pitch is how fast `s` runs and the timbre is what the field does along the
curve. A wavetable is the special case F(x, y) on a plane with g a horizontal
straight line at height y; morphing = moving y. Everything below is what you
get by letting g be any curve and F be a volume.

## 1 · Honest lineage

This family exists and has a name for the 2D case: **wave terrain synthesis**
(Mitsuhashi 1982; Roads' Computer Music Tutorial has a chapter) — a terrain
z = f(x, y) scanned along an orbit, usually an ellipse or Lissajous figure.
**Scanned synthesis** (Verplank, Mathews, Shaw 2000) is the dynamic cousin: the
thing being scanned is a slowly evolving physical object. High Tide
(HIGH-TIDE-DESIGN.md) is a wave-terrain descendant too, with the reader a MASS
whose path is Newton's, not the player's.

What is genuinely new in Peter's version, and worth building for, is the
combination of three things none of those have:

1. **a 3D field**, so a scaffold has a whole volume of material to be placed in
   and moved through, not a surface;
2. **arbitrary topology of the read path** — loops, open segments, chains,
   self-crossings — with the geometry deciding the waveform's character (§2);
3. **morphing the PATH, not the waveform.** A wavetable crossfades two sampled
   frames; here the cursor blends the anchors' GEOMETRY and the intermediate
   path is then read from the field. The midway path passes through material
   neither anchor visited. The morph goes somewhere instead of fading between
   two places. This is the strongest idea in it.

## 2 · What the geometry gives for free (no extra mechanism)

| shape of the scaffold | what the waveform does | why |
|---|---|---|
| closed loop | periodic with no edge, no click at the cycle boundary | g(1) = g(0), so w(1) = w(0) |
| open segment | a hard edge once per cycle, sawtooth-like 1/n harmonics — brightness | the jump F(g(1)) − F(g(0)) |
| a chain of segments (corners) | a kink per corner: extra edges at fixed phases | derivative discontinuities in g |
| self-crossing (figure-eight, knot) | the same value revisited at two phases — a built-in symmetry in the waveform | F is single-valued at the crossing |
| non-uniform speed along the path | phase distortion — the same shape read faster here, slower there | s -> g(s) reparametrised |
| small loop / large loop | narrow-band vs wide-band, if the field is uniform | how much material the loop crosses |
| the whole scaffold translated or rotated through the volume | timbre evolves continuously while the INSTRUMENT (the shape) stays the same | a slice of a larger object — the artefact family's theme in one more form |

So the player has two independent gestures: **deform the scaffold** (change the
instrument) and **move the scaffold through the field** (change what it is made
of). Both are fully visible: a volume render with a glowing curve, and the
waveform IS the brightness along the curve. Sight = sound, exactly.

## 3 · The traps (all known, all cheap)

- **Aliasing.** A field with R texels of resolution along a path of length L
  texels carries up to ~L/2 harmonics; at pitch p the top harmonic is p·L/2.
  With a 128^3 volume and a path spanning ~100 texels that aliases above
  ~C5 (480 Hz). Fixes, in order of honesty: a mip pyramid of the field
  (128^3, 64^3, 32^3 …) chosen per note by p·L; or render the periodic cycle to
  a table per note and use ordinary wavetable mipmapping; or 4x oversample and
  lowpass. The field itself should be C1-smooth (tricubic, not trilinear, or
  a smooth procedural field), or every texel boundary is a corner.
- **DC.** The mean of F along the path is the waveform's offset, and it moves
  with every morph and every drift. A 5 Hz blocker at the output, as PS2.
- **Morphs through nothing.** Path-blending can carry the intermediate path
  through a flat region of the field (silence in the middle of a morph). That
  is a property, not a bug, but the field must be authored so it is rare — or
  the morph space shows the field's energy along the interpolated path so the
  player sees the valley coming.
- **Correspondence.** Blending two scaffolds needs a shared parameter s; two
  closed loops blend cleanly at equal s, an open segment and a loop still
  blend (the loop degenerates), but the START POINT of a loop matters — rotate
  it and the blend twists. Give every scaffold an explicit start mark the
  player can slide.
- **Cost.** One tricubic read per sample per voice (64 texels; trilinear 8).
  A 128^3 float volume is 8 MB. Trivial on CPU.

## 4 · Open choices (Peter's, not mine)

- **What the field is.** Procedural noise (smooth, infinite, no authoring);
  a stack of wavetables (backward compatible: the bottom slice IS a
  wavetable); a photograph or a photo-stack (PHOTO SYNTH's own move, one
  dimension up); a physical field (a standing-wave pattern, a potential, a
  diffraction volume); or a slowly EVOLVING field (scanned synthesis proper:
  the volume is a simulation and the scaffolds read it as it runs).
- **How paths are made.** Drawn (a 3D gesture is awkward with a mouse — draw
  in a plane, wheel for depth, exactly the artefacts' wheel); random walks
  smoothed into splines; or **derived from the field**: streamlines of grad F
  are the paths of fastest change (bright, edgy), iso-lines are silence,
  ridge-lines are the loudest possible loops. Paths that come from the
  material are the least arbitrary "random lines".
- **The morph space.** A vector pad with anchors placed at will and inverse-
  distance weights; or a 1D chain of anchors (the wavetable's position knob,
  familiar); or the cursor on its own physics (High Tide's ball) — which would
  make this a MODE of High Tide rather than a sibling: same terrain family,
  authored scaffolds instead of Newton.
- **A 4D field with the volume as its slice** — then the artefacts' wheel
  applies here too, and the family gets a fifth member. Tempting; not needed
  for v1.

## 5 · Build order, when and if go is given

1. Offline bench first (plain C++): a procedural C1 field, closed-loop and
   open-segment scaffolds, path-blend vs waveform-crossfade rendered side by
   side, the aliasing law measured against a mip pyramid, DC measured.
   The claim to test: the midway path of two anchors has spectral content
   neither anchor has (compare its spectrum against the two crossfade
   spectra — if it is always between them, the idea is only a wavetable with
   extra steps).
2. Engine: 8 voices, per-voice phase -> path read, mip select per note, DC
   block, the usual envelope; scaffolds as Catmull-Rom control points with a
   start mark; morph space as a 2D pad.
3. Panel: volume render (WebGL, raymarched, dim), glowing scaffolds, the
   cursor, the read waveform drawn on the curve itself.
4. Then decide whether it is High Tide's sibling or its second mode.

## 6 · A mode of High Tide, or its own thing? (asked 2026-09-04, High Tide already built)

Recommendation: **its own thing, a sibling in the terrain family**, sharing
High Tide's workflow but not its engine. The reasons are structural, not taste:

1. **Opposite axioms.** High Tide's identity is "no stored waveform, no
   phasor: the reader is a mass under Newton". A scaffold is read by a phasor
   along a curve — exactly the reader High Tide refuses. As a mode it would
   contradict the thesis the product is named for, and every bench claim
   ("nothing to erase", "the period is the bowl's") would need a qualifier.
2. **Different alias law.** High Tide's reader speed depends on strike energy,
   so its anti-aliasing is a per-note blur along x chosen from the ball's own
   step (§12 of its design). A scaffold's read speed is fixed by the pitch and
   the path length, so its mip level is static per note — simpler, exact, and
   a different code path. Nothing to share but the lesson.
3. **The mode would be two synths in one binary.** A 3D volume needs a
   raymarched renderer (High Tide's is a 2D heightmap), a different state blob
   (a volume, not a 16-bit PNG), different sculpting (3D brushes), and its own
   parameter set. Only the chassis would be common, and the manual would
   double. The fleet's own rule for a different algorithm is a sibling, never
   a replacement or a mode.

And a reframing that matters for the decision: dimension count is not the
axis of esotericism here. High Tide is esoteric in its READER (physics, chaos,
descent through a bifurcation tree); the scaffold is esoteric in its PATH
(topology, path-blending) but its reader is conventional — deterministic,
exactly in tune, easy to alias-control. It is the more approachable of the
two, closer to a wavetable than High Tide will ever be, and should not be
buried as the wild mode inside the wild synth. If anything it is the family's
entry instrument.

**What to share, concretely:**
- the TIMELINE with PINS and HOLD — a pin as a target the morph cursor eases
  toward is exactly the scaffold's morph-space workflow; port the mechanic;
- the LOD discipline and the brush-floor rule (as rules, re-derived for 3D);
- the chassis every Brokild synth has (BWFX rack, macros, SPECTRA, patches);
- a **bridge**: a High Tide terrain is a 2D field, i.e. one slice of a
  scaffold volume — "export this terrain as a slice" and the two products
  hand material to each other.

If the one-product route is still preferred for the shelf, the least bad way
is a second reader type behind the same SPECS table with the volume as an
extra state blob; the cost is the renderer and the sculpting, not the engine.
Not recommended.
