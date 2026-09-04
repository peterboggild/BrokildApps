# HIGH TIDE — design

*A Brokild synth. Written 2026-09-03 from a conversation with Peter, before any
code, in the house manner: every claim made here is something `test/bench.cpp`
measures against the real engine.*

*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.2, tree
`C:\Users\peter\b\HighTide`, 283 bench checks ALL CLEAR, panel probe 14/14,
verified live over CDP, installed in both houses, published at
`vst3-apps/high-tide/`. What was built matches this document except where §12
says otherwise; §12 and §13 are the record of what the build taught.*

---

## 0. The one-line thesis

**Stop storing waveforms. Store the landscape they fall out of, and let a body
roll through it.**

A wavetable synth stores the answer: a frame is one cycle of numbers, read by a
phasor at the pitch. High Tide stores the question: a frame is a BOWL, the
waveform is what a mass does when dropped into it, and the waveform is never
stored anywhere. Position is not a number you set but the place the mass
currently is. Morphing is travel across terrain. The tide decides how much of
that terrain is in the way.

## 1. What a wavetable is, and what this refuses

Every wavetable synth rests on one axiom — a frame is waveform data, read by a
phasor — and everything else (position, morphing, mip-maps, the 3D stack of
frames) follows from it. High Tide keeps the *picture* (a stack of shapes along
a position axis, a knob that travels it) and refuses the axiom:

- **No stored waveform.** There is no buffer of samples anywhere in the voice.
  The bench proves it by having nothing to erase.
- **No phasor.** The reader is a mass with position, velocity, energy and
  history, integrated by Newton's law at audio rate.
- **No interpolation between frames.** The shapes between two valleys are the
  shape of the pass between them, which the sculptor carves.
- **No mapped velocity, no filter, no envelope on the timbre.** Strike energy
  is brightness because a hard hit climbs higher up the walls; decay is the
  mass sinking toward the floor, where every bowl is locally a parabola, so
  every note ends as a sine. These are consequences, not mappings.

Prior art, so it is not mistaken for either: **wave terrain synthesis**
(Mitsuhashi 1982, Borgonovo & Haus 1986) reads a static surface with a phasor
path — the terrain is the table, the reader is still a phasor. **Scanned
synthesis** (Verplank, Mathews, Shaw 2000) makes the *table* a slow physical
object and reads it with a phasor. High Tide is the complement of both: the
table is static, the *reader* is the physical object.

## 2. The still terrain

### 2.1 The medium

A heightmap `U(x, z)`. `x` is the swing axis — the axis a cycle happens along —
and `z` is the position axis, the one the old wavetable's frames were stacked
on. Height is potential energy. Proposed resolution 1024 × 128, float, bicubic,
with the gradient precomputed so the force is C¹ (a kink in the force is a
click). A patch's terrain is a 16-bit grey PNG; see §5.

Time is the ball's own. The terrain is defined so that the **reference bowl**
`U = ½x²` swings at one radian per unit of ball time; a note at `f` Hz runs
the ball's clock at `2πf` — the phasor increment of a wavetable, reappearing
as the ball's clock rate. A ball at rest on the floor at `x = 0` with energy
`E` reaches `x = ±sqrt(2E)` in the reference bowl; full scale is `E = ½`. The
terrain extends past `|x| = 1` so a hard strike has somewhere to go.

### 2.2 The body

Per voice: `x, ẋ` (audio rate, symplectic velocity-Verlet at 2× oversampling
— energy must not drift over a held note) and `z, ż` (slow, heavily damped).

    ẍ = −∂U/∂x − γ ẋ + F_rock(t)                          (§3)
    z̈ = −∂U_eff/∂z − Γ ż + k_t · (z_pin(t) − z)           (§4)

- **Strike.** Note-on gives the ball energy `E = E_max · vel²` (a kick at the
  floor, or a displacement — a SETTING). Retrigger adds to what is there.
- **Damping** `γ` is the release: exponential energy loss, the ball sinking.
- **Three output taps**: position `x`, velocity `ẋ`, force `−∂U/∂x`. Each is
  the same motion at a different spectral tilt, and the force tap escapes the
  unimodality limit of §2.5 — a wiggly wall gives a wiggly force while the
  position stays a clean swing.
- **Unison** = several balls in one terrain with a weak mutual repulsion.
  Detune is not a parameter: it is a difference in strike energy in a bowl that
  is not isochronous. They can lock, and they can collide.

### 2.3 The feature-to-sound dictionary

This is the sculptor's vocabulary, and the point of the whole design: every
terrain feature has one fixed audible meaning.

| feature | sound |
|---|---|
| floor of a valley | the note and its quiet timbre; its curvature is the pitch; every decay ends here |
| walls | the loud timbre — steep is bright and square-ish, gentle is round |
| a shelf halfway up a wall | a timbre that exists only at mezzo-forte |
| a pit in the floor | a pulse: the ball dives through and comes out with a velocity spike each pass |
| two pits with a hump between | an octave regime that opens when the strike clears the hump (the cycle gains a dwell at the saddle, then a second hump) |
| asymmetry between the walls | even harmonics; mirror the bowl and they vanish |
| a ridge along `z` | a morph threshold; its height is the force needed to cross |
| the pass over a ridge | the sound of the transition — wide and it strolls, a notch and it snaps |
| a tilt along `z` | the direction timbre drifts when the playing gets loud (a wide `x` swing leaks energy into `z`) |
| roughness | grain — with a floor to how fine it may be (§5.3) |

### 2.4 Pitch — exact by construction, or bent on purpose

Landau & Lifshitz §12: the period of a 1-D well depends only on its **width
function** `w(U) = x₊(U) − x₋(U)`. Any bowl whose width at every height equals
the reference parabola's is exactly isochronous — the walls may be sheared
however the sculptor likes, the waveform is far from a sine, and the period
does not depend on how hard it was hit.

So **TUNE LOCK** is a sculpting mode (§5.1): edit the left wall and the right
wall follows so the width function is preserved. With it off, the departure
from the width rule is drawn on the bowl as a coloured band, and that band IS
how much the pitch bends with the strike. **TENSION** is not a knob; it is
something you can see you have carved. A valley may deliberately carry a
constant width factor `c` (pitch `1/c`) — a valley tuned a fifth up.

For bowls that break the rule, and for the moving-terrain regimes of §3, a
**SERVO** measures the last period from same-sign zero crossings and eases the
clock toward the commanded one, one cycle of lag — the B2311.104 way. TUNING =
LOCK | SERVO | FREE.

### 2.5 The honest limits of a still terrain

- A frictionless ball retraces its own path, so a still terrain only makes
  waveforms that read the same backwards. A **saw is not such a wave**: it is
  stick-slip, dragged slowly up a slope and let go. Saws belong to the moving
  terrain (§3), where friction and a drive exist.
- A 1-D trajectory has one maximum and one minimum per cycle, so not every
  waveform is a bowl. Serum tables cannot be imported wholesale (§5.4).
- A steady cycle in one dimension is periodic, hence harmonic. The alienness
  of the still terrain lives in thresholds, passes and dynamics, not in the
  held tone. Non-harmonic steady tones come from §3.

## 3. The moving terrain — where the nonlinear waveforms live

A mass on a still 1-D terrain can never be chaotic; that theorem is what makes
§2 playable. Add exactly one ingredient and the whole bifurcation literature
opens, and every candidate ingredient is a *visible thing on the terrain*.

### 3.1 ROCK (phase 1 of this section — build first)

The whole landscape tilts like a see-saw: `F_rock = A · sin(2π r φ)` along
`x`, where `φ` is the note phase and `r` a ratio (1, 2, ½, ⅓, 3/2, or FREE in
Hz). `A` is ROCK, the bifurcation parameter. **BREATH** is the parametric
sibling: `U → (1 + ε sin 2π r φ) U`, bowls steepening and relaxing in time
(the Mathieu equation for a parabola: energy pumped at `r = 2`, exact
sub-harmonics at `r = 2/n`).

In a double well as ROCK rises: one pit, clean tone → two rocks per figure,
**an exact octave down and hollow** → four, eight, the cascade closes → the
ball hops between pits at times nobody chose. Inside the chaos, windows — a
place where the ball settles into a three-rock figure (Hairfryer's guttural was
this window in the logistic map; here it is a place on the terrain). This is
the kind of chaos wanted: not white noise. The rock frequency stays as a strong
line and the disorder is how the ball disagrees with it. A roar with a note in
it.

Two things make ROCK playable rather than a toy:
- **The rock rate is the note**, so the pitch survives the chaos, because the
  drive *is* the pitch, and the bowl only decides how obedient the ball is.
- **A rock at the note pumps an isochronous bowl** until friction balances it,
  so a rocked note *sustains* like a bowed string and decays down the bowl
  when the rocking stops. Strikes are plucks; the rock is the bow; ROCK depth
  is the bifurcation knob.

### 3.2 The 2-D bowl (phase 2 — the forte chaos)

Give the ball a second swing axis `y`, no drive, energy conserved. Low energy:
quasi-periodic, two frequencies set by the bowl's two curvatures; their ratio
is authored (1 = a note, √2 = a shimmer with no beat period, 3/2 = a fifth
folded into the wave). Add asymmetry (Hénon–Heiles cubic terms) and above a
threshold energy the regular motion breaks into a chaotic sea with islands.

The bifurcation parameter is then **energy, i.e. how hard you struck**, and
friction drains energy — so **every note is a descent through the bifurcation
diagram**: struck into chaos, decaying through mixed motion into quasi-periodic
shimmer, ending as a sine at the floor. Nothing switches; the chaos lives in
the upper walls where only forte reaches. Harder than ROCK (in the sea there is
no pitch; in the regular regime there are two), so it is phase 2.

### 3.3 Not built, noted

Self-drive by delay (the ball's past tilting the terrain — the .104 route) and
unison balls driving each other into chaos. Real; overlap with the fleet.

### 3.4 How the "bifurcation wavetable" folds in

The position axis need not be a row of *different* bowls. A valley can run
along `z` as the *same* bowl under increasing ROCK, so pushing the ball along
`z` walks it along the bifurcation tree and the ridges between valleys are the
bifurcation points. The display fuses the two iconic pictures: sample the
ball's turning heights each cycle against ROCK and **the bifurcation tree draws
itself on screen as the player sweeps**. A preset is a named place on the tree.

## 4. PINS, the timeline, and the TIDE (Peter, 2026-09-03)

Peter's request: keyframes as in a video editor ("Pins"), on a timeline that
runs from note-on, showing what the envelopes and LFOs are doing, so that
sweeps through the terrain are authored and consistent note to note.

### 4.1 The tension, and the resolution

In §2 position is where the ball *is*, moved by forces, with inertia and
terrain in the way. A keyframe says "at time t, be at position P". If a pin
commanded the coordinate, the terrain's physics (ridges, passes, overshoot)
would be bypassed; if the physics kept full authority, a pin could never
guarantee arrival. Both halves are wanted.

**A pin is a target, not a coordinate.** The pin track defines a moving target
`z_pin(t)`; the ball is tethered to it by a line of strength `k_t` = **HOLD**.
The terrain decides how the ball gets there. Then:

- **TIDE** floods the relief. Let `R(z) = min_x U(x, z)` be the floor height
  of each bowl along the position axis — the *relief*. The effective terrain is

      U_eff(x, z) = U(x, z) − R(z) + max(R(z), T)

  which leaves every bowl's shape untouched (the `x`-force is unchanged) and
  floods everything on the position axis below the water line `T`. A ridge
  under water is not there. At TIDE 1 the whole relief is submerged and the
  ball may be towed anywhere; at TIDE 0 every ridge stands and the ball is
  stranded in whatever valley it is in unless HOLD can drag it over. The
  bowls are never flooded, because they are what the ball sounds with.

So the two extremes are both legitimate instruments: high tide + high hold is
the video editor (the wake equals the pin path to within the tether's lag);
low tide + low hold is the pure landscape of §2. Everything between is a
terrain that partly has its way. The name is the mechanic.

### 4.2 The timeline

Runs from note-on, per voice. Lanes, each drawn as the *actual curve against
time* (an ADSR as its shape, an LFO as its waveform at its rate), so a pin can
be dropped at the LFO's peak or at the end of the decay:

- AMP (the output VCA — kept, for DAW sanity, see §9)
- MOD envelope(s), LFO 1–2 (SYNC / FEEL as the fleet does it)
- TIDE, ROCK (their own lanes, so they can rise and fall over a note — a
  rising tide during the attack = the note starts grounded and is freed as it
  swells; a tidal LFO = surge)
- PINS: the pin track, `z_pin(t)` with per-pin ease (HOLD | LINEAR | EASE |
  SPRING). A pin may also carry a TIDE and a ROCK value.

Two anchors, like the ADSR itself: pins in the attack/decay region count from
**note-on**; pins after the release marker count from **note-off**. A **LOOP
region** inside the sustain cycles its pins while the note is held (seconds or
beats) — that is how a hand-authored LFO along the terrain is made. Velocity
may optionally scale HOLD or TIDE (harder = freer). MPE: pressure → TIDE
(pressing floats the ball), slide → a push along `z`.

### 4.3 The WAKE — authored vs actual

The last note's real `z(t)` is recorded and drawn over the pin path on the
timeline, and as a trail on the terrain. The sculptor sees at once where the
terrain won and where the tide carried. (The fleet's standing lesson: a
control can be working and still look broken — the display must show the
physics, not the intention.)

## 5. Sculpting

### 5.1 Tools

Push / pull with a band-limited brush; smooth; dig a pit; raise a ridge; tilt;
mirror; **stamp** (the old frames become stamps: sine bowl, box, pit bowl,
double well, shelf); **relief** (edit `R(z)` alone — the passes — with the
tide line shown); **TUNE LOCK** (§2.4). Sculpting while the ball rolls is
performing: raising the floor under it throws it up the walls, so every touch
sounds — the rule that made B2311.22 feel alive, free here.

### 5.2 Terrains come from anywhere a heightmap does

The patch's terrain is a 16-bit grey PNG, import and export. Photoshop,
Blender and World Machine are wavetable editors the day this ships. A
photograph the Photo Synth way: one luminance row is a bowl profile, the image
is a landscape of bowls along its height. Real elevation data, with the right
lesson built in: a mountain range is fractally rough and aliases until it is
smoothed.

### 5.3 The brush floor — anti-aliasing as a sculpting rule

The ball cannot see a bump narrower than the distance it travels in one
oversampled sample at its top speed, `v_max · dt_os`, and a bump it half-sees
is aliasing. The brush refuses to carve below ~4× that width, and the floor is
drawn as a grid. Since `v_max` depends on the loudest strike allowed, the floor
is a property of the patch's dynamic range — which is true, and which a
sculptor can reason about.

### 5.4 Importing old waveforms — a door with a bouncer

A single cycle can be inverted into the bowl that would produce it,
`U(x) = E − ½ v(x)²` with `v(x)` read off the rising half, but only if it has
one peak and one trough and reads the same backwards. The importer averages
the two halves and shows the part it dropped.

## 6. Panel

- **Top: the terrain**, WebGL, with the ball(s), the wake trail, the water at
  the tide line, pins as stakes. What you see is the state, not a picture of
  it. Sculpt rail on the left; taps, tuning, rock, tide, hold on the right.
- **Bottom: the timeline** of §4.2, playhead of the most recent voice, wake
  overlay, zoom, SYNC.
- **Optional: the bifurcation view** of §3.4, accumulated live.
- Identity: not decided. A sea-chart / tide-table register suggests itself —
  chart lines, soundings, a tide gauge — but nothing is drawn yet.
- House rules: one teal BWFX globe, five macros, patches in
  `Documents\Brokild patches\High Tide`, the loading splash, the About modal.

## 7. Architecture

- JUCE 8, VST3 + Standalone, native-first as Hairfryer/Black Rider: `SPECS[]`
  parameter table walked by the APVTS, processBlock, randomiser, initialState
  and the bench, so a read-order bug is inexpressible.
- **The terrain and the pins are state, not host parameters** — one opaque
  blob (PNG bytes + pin list) alongside the APVTS XML, the way the BWFX rack
  rides. Host-automatable surface: the SPECS params and the five BWFX macros.
- PRODUCT_NAME `High Tide`; PLUGIN_CODE proposed **`HiTd`** (checked 2026-09-03 against
  every `b\*\CMakeLists.txt` — A104 Ab01 Ab22 Ab67 BldR BlkR EscR FmRk Hfry
  MrsW Psy2 — plus Clone Wars' Cwar: unique); tree `C:\Users\peter\b\HighTide`; private repo
  `brokild-high-tide` the day it exists (`b\` is in no cloud — committing IS
  the backup). Install only through `install-fleet.ps1`, Brokild collection
  folder.
- Cost: a voice is four state variables and a bicubic read per oversampled
  sample. 16 voices × 4 unison should sit well under 5 % of a core.
- BWFX rack after the voice sum; SPECTRA world-mod bus consumed (det fanned
  golden-angle across unison balls, sag on the smoothed gate, filterMul on a
  radiation lowpass, neutral bus memcmp-identical, as everywhere).

## 8. The bench — `test/bench.cpp`, plain C++, no JUCE

Numbers are what the design promises; "bounded and busy" proves nothing.

1. **Nothing to erase.** The voice has no sample buffer; the reference-bowl
   terrain gives a sine at −80 dB THD at 2× OS.
2. **Isochrone.** A tune-locked, wildly sheared bowl holds pitch within 2 cents
   from `E = 0.005` to `E = 0.5` (40 dB).
3. **Energy.** Undamped ball, 60 s, energy drift < 0.1 % (symplectic).
4. **The dictionary, item by item.** Box → triangle / square harmonic ratios;
   pit → centroid up and a pulse in the velocity tap; asymmetry → even
   harmonics appear, mirror → they vanish below −60 dB; double well → the
   cycle gains a saddle dwell above threshold and the octave regime opens.
5. **ROCK.** Period-2 at exactly `f/2` (Goertzel); the cascade reaches chaos,
   proven as chaos — two runs 1e-9 apart diverge to order one — not merely as
   busy; ROCK 0 is memcmp-identical to the still terrain.
6. **Relief and tide.** Crossing time of a ridge grows monotonically with its
   height and falls with TIDE; at TIDE 1 it is independent of the relief; at
   TIDE 0 with HOLD below the ridge's slope the ball never crosses.
7. **Pins.** Same velocity → same wake (memcmp, two notes); at HOLD 1 the wake
   reaches every pin within the tether time constant of its time; the loop
   region's period is exact; note-off anchoring holds under a long sustain.
8. **Aliasing.** Roughest allowed terrain, loudest strike: alias floor
   < −60 dB. Carving below the brush floor must be refused, not merely warned.
9. **Import.** Waveform → bowl → waveform round trip < −40 dB for a legal
   cycle; an illegal one reports what it dropped.
10. **Cost.** 16 × 4 balls with rock and pins < 5 % of a core.

## 9. Open questions — decided when built, not before

- Is the amplitude envelope physical (rock pump vs friction) or a VCA? The
  recommendation is BOTH: the VCA for DAW sanity, the energy path for the
  sound, each a lane on the timeline.
- Should TIDE, at its extreme, be allowed to flood the *bowls* too ("drown" =
  every valley becomes a box, a uniform sea)? Physically consistent, musically
  a reset state; kept out of phase 1.
- Unison repulsion law and whether balls may pass through each other.
- The 2-D bowl (§3.2) and its two-pitch problem.
- Visual identity.

## 10. Build order (when Peter says go)

1. Still terrain, one ball, three taps, tune lock, servo, strike, damping —
   bench items 1–4. Headless heightmaps, no panel.
2. ROCK / BREATH — item 5.
3. Relief, TIDE, HOLD, pins, the two anchors, the loop — items 6–7.
4. Panel: terrain view + timeline + wake; sculpt tools; PNG in/out; brush
   floor — item 8. Verify LIVE over CDP, not by static checks (the
   missing-`</script>` lesson).
5. Imports (photo, DEM, waveform) — item 9. BWFX, macros, SPECTRA, patches.
6. Manual, landing page, zip, app.json, manifest — the full release, as always.
7. Phase 2: the 2-D bowl.

## 11. Decisions — 2026-09-03

- The name is **High Tide** (Peter).
- Keyframes are called **Pins**. A pin is a **target**, never a coordinate.
- The **tide floods the relief only**; bowls are never flooded in phase 1.
- Still terrain first, ROCK second, the 2-D bowl third.
- 2026-09-03 evening, Peter: "Can you do it?" — sound first, workflow second,
  graphics original and professional third; decals ordered through a folder
  ChatGPT can pick up (`assets/high-tide-decals/BRIEF.md`).

## 12. What the build taught — 2026-09-04

Each of these is a bench number, not an opinion.

- **Sub-step where the terrain is stiff, and decide from where the ball is
  GOING.** A wall bounce resolved by one sample per step depends on the phase
  of the sample grid; the period jitters and a box bowl read **+18 dB** of
  non-harmonic energy. Velocity Verlet with the step subdivided so
  ω·dt ≤ 0.10 fixed most of it — but only once the budget was decided from a
  look-ahead position: decided from where the ball WAS, it jumped clean into
  the wall (or over a pit narrower than the step) before sub-stepping engaged.
- **Levels of detail are the terrain synth's mip-map.** A fast ball cannot see
  a feature narrower than its step, and a feature it half-sees is jitter
  however finely the step is subdivided. Each note is handed the terrain
  blurred along x to about its own step (σ = 2^L cells, five levels). A
  parabola blurs to the same parabola plus a constant, so the reference bowl's
  pitch is untouched. Cell-scale roughness went from +26 dB to −82 dB.
- **A hard knee cannot be integrated.** A stiffness JUMP crossed at an
  arbitrary phase gives an O(dt²) energy error per crossing, whatever the
  sub-step count (measured +37 dB on a hard box). The factory BOX has a
  softplus knee over the brush floor (0.04); the brush's raised cosine obeys
  the same rule. Walls 40× the reference bowl's stiffness at B5 still read
  −28 dB at 4x: reported, not promised.
- **4x is the default.** The pit bowl reads −53 dB at 2x and −63 dB at 4x; a
  four-note chord costs 2.8 % of a core at 4x. 2x is the economy.
- **A probe that measures harmonics of the NOTE lies about a bowl that plays
  its OWN pitch.** Every non-isochronous bowl (box, pit, double well) is
  measured against its actual fundamental. Half the first bench run's failures
  were this.
- **The servo must SETTLE, not multiply.** The measured period is in ball time
  and does not move with the clock, so `clock *= ratio` ran to the clamp
  (+489 c). `clock = target^g · clock^(1−g)`.
- **Unison balls must not collide.** A short-range repulsion at every crossing
  (they cross twice a cycle) measured 1254 cents of "detune" in a parabola. The
  push is now weak and long-range (18 c at full SPREAD); energy scatter is
  ±30 %, and in a box that is still hundreds of cents, because in a box energy
  is pitch.
- **BREATH needs finite power.** A parametric pump on an isochronous bowl has
  nothing to detune it from the 2:1 resonance and grows to the edge of the
  world (SEICHE measured at the ceiling). The pump's grip now falls with the
  ball's energy; the level it settles at is ROCK against FRICTION.
- **A mono fall-back is not a strike.** Releasing the upper of two held notes
  re-kicked the ball and quadrupled its energy; the ball hit the world's edge
  and the pitch read 147 Hz for a 110 Hz note.
- **Lanes set in the same block as a note must be visible to that note.**
  The pending-lane swap happened only at the top of `process()`, so a fresh
  note started in the wrong valley and two identical factory loads rendered
  differently. `noteOn` and `drop` pull the pending lanes first.
- **A bare `char*` gloss is Latin-1 to JUCE.** The em dash in a parameter's
  gloss reached the panel as three bytes of mojibake, in a plate. Wrap the
  table's strings in `CharPointer_UTF8`.
- Not built, deliberately: the 2-D bowl (§3.2), the bifurcation view (§3.4),
  decals (ordered, slot in through the panel's `--decal-*` variables), the
  MPE mapping. The mod wheel and channel pressure lift the TIDE.

## 13. THE CENTRE LINE — the discovery of the second round (260904.2)

Peter asked for "less chaotic starter sounds ... a safe starting point". The
design as written offered one isochronous family, the linear shear, which is
thin ground for seventeen patches. The way out was already in §2.4 and had not
been read properly. Landau's result is about the WIDTH function alone, so write
a bowl as

    x±(h) = c(h) ± sqrt(2h)

and the width is the reference parabola's **whatever c does**. The bowl's
CENTRE LINE may wander with height however it likes and the pitch does not
move. c is the timbre; the pitch is exact by construction.

- A straight c is the sheared bowl (even harmonics). A **wavy** c gives a wave
  nothing like a sine and still exactly in tune — that is what the starters are
  made of. The only constraint is that the walls stay monotone,
  |c'(h)| < 1/sqrt(2h), which at full scale means |c'| < 1.
- **Morphing along z must blend the CENTRE LINES, not the heights.** The
  average of two isochronous bowls' heights has neither one's width and is not
  isochronous; blending c keeps the whole sweep in tune. That is what makes
  SWEEP LEAD (a filter sweep with no filter) and MORPH PAD possible.
- The construction generalises the design's own claim: TUNE LOCK is the
  c-preserving edit, and the red tension band is |c'| running out of room.

Four measured lessons from the same round:

- **The balls' mutual push is what makes a unison patch drift with the
  strike.** A patch at SPREAD 0.45 drifted 6.8 cents between a brushed and a
  hammered key; at SPREAD 0.20, 0.76 cents. The push was there to stop the
  balls sitting on top of each other, which is DETUNE's job now, so it was
  halved and the drift went to 1.2 cents.
- **A box bowl under heavy friction cannot be held in tune by the servo.** As
  the ball sinks onto the flat floor the period grows without bound and the
  servo runs out of range — CLAV measured 35 cents flat within a second. It is
  built from a wavy centre line instead, which cannot go out of tune at all.
- **ballPeriod is measured in BALL time, so it cannot see DETUNE** — a detuned
  ball's clock scales that time too and its period reads 2π whatever the
  detune. The first version of the detune check measured exactly nothing.
  Detune is heard as BEATING, so the check measures beating: 1.00× envelope
  swing at zero, 21.9× at 40 %.
- **Where a block is placed is part of whether it works.** The hints block was
  appended near the end of the page and `initHints()` called from the boot
  line above it; function declarations hoist but `const` does not, so the call
  threw on the temporal dead zone and took the whole script with it. Moving the
  block above its first use was the fix. Same family as the missing
  `</script>`: only a live page shows it.
