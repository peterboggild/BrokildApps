# GRAVITY WELL — the bassline synth

**Status: DESIGN ONLY, 2026-09-26.** No tree, no code. Peter's brief is at the
bottom, verbatim in substance. Read this before writing a line.

Plugin code **`GrvW`** (checked unique against all 24 in the fleet),
PRODUCT_NAME "Gravity Well", IS_SYNTH TRUE, VST3 + Standalone, BWFX rack +
5 macros (it is an instrument, so it gets the rack — the Martian Gain /
Hairfryer rule cuts the other way here).

---

## 0. What it is, and what it is NOT

**It is the bassline synth the fleet does not have**: monophonic-first,
built for techno, dub, dubstep, doom metal, industrial and electronica,
with the classics done properly and one original idea carrying the weight.

Three plug-ins in the fleet are close enough that the difference has to be
stated, or this becomes a second copy of something:

* **High Tide** is a ball rolling in a potential well, and its motion IS the
  waveform — terrain wavetable synthesis. **Gravity Well must never do
  that.** Here the well shapes TIME and SPECTRUM; it never draws the wave.
  If a design meeting drifts towards "the orbit is the oscillator", it has
  drifted into High Tide and must stop.
* **Artefact B2311.104** is already "deep/bass", but it is deliberately an
  alien artefact whose note is a thermal order. Gravity Well is the
  *musical* bass synth .104 deliberately refused to be. That is a clean
  split and worth keeping clean.
* **Black Rider** is the vintage monosynth. Gravity Well REUSES its
  oscillators and its three filter circuits (Brain Scan set that precedent
  and it worked). That is inheritance, not overlap.

---

## 1. THE ONE IDEA: mass curves time

Everything below follows from a single scalar.

**MASS** is derived per voice from signal energy, note velocity and a manual
offset. It sets a Schwarzschild radius:

    rs = MASS_normalised * R_MAX        (rs = 0 -> flat space, nothing happens)

Every consequence uses the real formula, not a look-alike curve.

### 1.1 Time dilation — the modulation topology

Every modulator (LFOs, envelopes, the stochastic sources) sits at a
**RADIUS** `r`. Its rate is scaled by the Schwarzschild time-dilation
factor:

    rate_effective = rate_nominal * sqrt(1 - rs / r)          for r > rs
    rate_effective = 0                                        for r <= rs

This is the whole innovation, and it is worth being precise about why it
beats a global "mod rate" scalar:

* **It is radius-ORDERED.** A shared scalar moves every modulator by the
  same factor and the patch sounds identical, slower. `sqrt(1 - rs/r)` is
  strongly nonlinear in `r`, so the RATIOS between modulators change as
  mass rises. Movement re-orders itself.
* **It is dynamics-linked.** Play harder, the well deepens, the movement
  slows and thickens; back off and it lifts. One gesture, no automation.
* **It has a dramatic, musical, physically honest special case.** A
  modulator that reaches the horizon **FREEZES and holds its last value**.
  Not a mute, not a gate — the formula genuinely goes to zero there. A
  filter LFO stopping dead at the bottom of a dig-in, then resuming as you
  release, is a sound you cannot get any other way without drawing it by
  hand.
* **It collapses a matrix into a place.** You do not dial ten modulation
  rate-and-depth pairs; you put modulators at radii and then move MASS.

Crossing back out of the horizon must be smooth: a frozen modulator resumes
**from its held phase**, never from zero, or every release clicks.

### 1.2 Redshift — spectrum, NEVER pitch

Gravitational redshift is the same factor:

    f_observed = f_emitted * sqrt(1 - rs / r)

Applied to the fundamental this would pull every note flat, which on a bass
instrument is fatal. So it is applied to a **FORMANT BANK** — a bank of
resonant filters shaping the harmonics the oscillator already produced —
while `f0` is untouched. The sound gets enormous and dark and stays in
tune.

This is the single most important engineering decision in the instrument.
**Pitch and spectrum must be separable or the central idea cannot be used
at musically interesting depths.** Hairfryer already proves the fleet can
do this (warped LPC moves formants +-35 % with pitch untouched); a filter
bank is the cheaper route and is enough here.

### 1.3 Ringdown — the body

A perturbed black hole rings at quasi-normal modes: complex frequencies,
inharmonic, each with its own damping. For Schwarzschild the fundamental
l=2 mode is `M*omega ~ 0.3737 - 0.0890i`, and overtones damp faster.

Two properties make this the right body for this instrument:

* the ratios are **genuinely inharmonic** and not a designer's guess, so
  the body is dark and metallic rather than organ-like;
* **`omega ∝ 1/M` and `tau ∝ M`** — more mass rings LOWER and LONGER.

So the resonator bank deepens and lengthens with the same knob. Excited by
the note transient (and optionally by the live signal), it is the doom /
industrial weight, and it is free once MASS exists.

### 1.4 The accretion disk — unison

Unison voices sit at radii and orbit at Keplerian rates `Omega ∝ r^(-3/2)`.
Detune comes from orbital velocity, and stereo placement rotates at those
rates. The spread law is neither linear nor exponential, which is why it
does not sound like every other unison — it clusters characteristically,
and the shear between adjacent radii is the beating.

### 1.5 The chirp — a modulation SHAPE, not a button

Inspiral: `f(t) ∝ (t_c - t)^(-3/8)`. Offered as an envelope/LFO **shape**
alongside the ordinary ones, so it can drive pitch, cutoff or anything
else. Deliberately not a one-shot "DROP" button: as a shape it is a tool,
as a button it is a gimmick.

---

## 2. THE SOURCE

Sound quality is the stated deciding factor, so this is where the budget
goes. Bass is cheap per voice; 4x oversampling throughout is affordable.

* **Two main oscillators + SUB**, polyBLEP, 4x oversampled. Ported from
  Black Rider (`b`'s oscillators are the fleet's best and Peter named them).
* **Hard SYNC** with the measured-jump BLEP and one-sample delayed output
  (Black Rider's, which is already correct).
* **Phase modulation, never FM** — PM is unconditionally stable and
  sample-rate independent (the Blade Ruiner lesson).
* **A wavetable oscillator** as an alternative per slot: properly
  mip-mapped and band-limited, so the digital option is as clean as the
  analogue one. This is what answers "digital like the monosynth" and the
  Massive end of the brief.
* **VINTAGE** — Black Rider's drift/jitter/tolerance knob. A little
  imperfection keeps a held bass alive.
* **The SUB must be phase-coherent with the fundamental**, or it partially
  cancels instead of reinforcing. This is the commonest silent fault in
  bass synths and it is worth a bench check of its own.

---

## 3. THE HORIZONS — two filter sections

Two sections, each picking one circuit, plus a routing choice.

**Circuits** (seven, deliberately not twenty):

| name | what it is | why it earns a slot |
|---|---|---|
| LADDER | Moog 4-pole ZDF, tanh at the summing node | the warm classic |
| GROWL | K35 Sallen-Key ZDF, diode clipper in the feedback | MS-20 aggression |
| SCREAM | K35 hot variant, self-oscillates from noon | acid, playable resonance |
| SVF | TPT state-variable, LP/BP/HP/notch | the clean modern one, and the resonant HP that keeps bass out of mud |
| DIODE | 18 dB diode ladder with the 303 feedback path | the fabled acid character |
| COMB | tuned, positive/negative, damped | Peter asked; tunable to the note |
| FORMANT | morphable vowel bank | talking bass, dubstep growl, industrial |

LADDER / GROWL / SCREAM port from Black Rider unchanged.

**Routing: SERIES / PARALLEL / SPLIT.**

SPLIT is the one that matters for this instrument and is the one Massive
does not do elegantly: a crossover sends lows to one section and highs to
the other. It is how you destroy the mids and keep the bottom solid, which
is exactly the doom / dubstep requirement, and it is what people currently
do by hand with multiband racks.

---

## 4. THE SINGULARITY — the protected fundamental

A band below a chosen frequency is routed **around** the whole filter and
drive chain and re-added at the end. The core that nothing can destroy.

This is not a gimmick; it is what every good bass engineer does manually,
and building it in removes an entire class of failure ("I found the sound
but it lost its weight"). Naming it after the thing at the centre that
nothing escapes is apt and instantly understood.

Its level is one of the six macros (DEPTH).

---

## 5. THE PANEL

One screen, no tabs. The layout follows the signal.

    +-----------------------------------------------------------+
    |  MASS   DEPTH   ENERGY   HORIZON   TIME   SPACE           |   six macros
    +-------------------+-------------------+-------------------+
    |  SOURCE           |   THE WELL        |  HORIZONS         |
    |  osc 1 / osc 2    |   (live render)   |  section A        |
    |  sub / noise      |                   |  section B        |
    |  disk (unison)    |   + SCOPE         |  routing          |
    |  singularity      |                   |  singularity xover|
    +-------------------+-------------------+-------------------+
    |  TIME — the modulators: envelopes, LFOs, chirp, random    |
    +-----------------------------------------------------------+

### 5.1 The six macros are the answer to "not 100 knobs"

Peter's own vocabulary, each a curated multi-parameter gesture (the
Battlestar SPACE precedent — one knob across five effects, overlapping):

| macro | what it moves |
|---|---|
| **MASS** | the well: dilation, redshift, ringdown depth. The core knob. |
| **DEPTH** | sub weight and the SINGULARITY level — the low-end structure |
| **ENERGY** | drive and saturation across the chain; the grind |
| **HORIZON** | both filter cutoffs together, and the split point |
| **TIME** | global modulation clock (before dilation scales it) |
| **SPACE** | width, disk spread, ambience |

Six knobs get you most of the way. Everything under them is still there
and still a real control — the hierarchy is about prominence, not hiding.

### 5.2 The well IS the modulation matrix

No grid. Two gestures:

1. **Drag a modulator onto the well** to set its radius — that sets its
   time dilation, and you can SEE which ones are near the horizon.
2. **Drag from a modulator to a control** to assign it, with depth from the
   drag distance. (The BWFX macro-assign interaction, which works.)

### 5.3 The two views are separate, and that is deliberate

**A metaphor must never make an instrument lie.** So:

* **A real oscilloscope** — honest, calibrated, triggerable, undistorted.
  Peter asked to see waveforms; a scope bent by the art is useless.
* **THE WELL** — the animation, beside it, sharing the same signal.

### 5.4 The well animation, with the actual maths

WebGL2, and every element is a real formula rather than a styling choice:

* **Flamm's paraboloid** as the surface: `z = 2*sqrt(rs*(r - rs))`. Flat at
  MASS 0, dimpling as mass rises. This is the textbook embedding diagram,
  so it is both correct and recognisable.
* **The horizon** as a black disc at `rs`, with the **photon sphere** ring
  at `1.5*rs`.
* **Gravitational lensing** of a background starfield in the fragment
  shader — deflection `alpha ~ 4GM/(c^2 b)`. Cheap, and it is what makes
  the image unmistakably a black hole rather than a dark circle.
* **Relativistic beaming** on the accretion disk: the approaching side is
  brighter. This asymmetry is why the Interstellar and EHT images look the
  way they do, and it costs one dot product.
* **The modulators are orbiting bodies** at their own radii, moving at
  their dilated rates — so the picture is also the readout. A frozen
  modulator visibly stops at the horizon.
* **It pulses with the signal** because `rs` is driven by the live MASS
  follower. Nothing is faked to the beat.

### 5.5 The view is a READOUT OF THE SOUND, not only of the modulators

Peter, 2026-09-26: the view should not just reflect LFOs and maybe the
envelope but also the bass itself - at least level, perhaps spectral content
- maybe through distortion of the starry sky.  He is right, and there is a
physically exact version of each, so none of it has to be faked:

* **Level is already there, and it is the spine.** MASS is derived from
  signal energy, so rs IS the live level: the well visibly deepens as the
  bass hits.  Nothing to invent - it is the same number the DSP uses.
* **GRAVITATIONAL WAVES carry the transients.** An accelerating mass
  radiates, so a transient launches an outgoing ripple through the funnel
  mesh, decaying as 1/r.  Each note visibly shakes spacetime and the wave
  runs away outward.  This is the one that will read most strongly and it is
  nearly free: one term added to z in the grid vertex shader,
  A*sin(k*r - w*t)*exp(-r/L), with A set by a transient detector.
* **SPECTRAL CENTROID colours the accretion disk.** A real disk is hotter and
  bluer further in; here a dark, filtered sound leaves it dull red and a
  bright one drives it white-hot.  So the disk reports TIMBRE while the well
  reports LEVEL - two different pictures of two different things.
* **High-frequency energy shakes the sky.** Peter’s own suggestion: the
  starfield gains a small shear and twinkle with treble content.  Kept SMALL
  deliberately - it is seasoning, not signal, and a background that thrashes
  makes the readout harder to read rather than richer.

The rule that governs all four: **the picture may never lie about the sound.**
Anything that moves must be driven by a quantity the DSP actually computes.

No decals needed — this is computed form, like B2311.22's tissue. Peter
asked for animation rather than decals and the physics supplies it.

---

## 6. SOUND-QUALITY COMMITMENTS

These are the fleet's own measured lessons, written down in advance so they
are not re-learned:

* **4x oversampling through every nonlinearity**, polyphase half-band from
  `bwfx_dsp.h`.
* **ADAA on the saturators.** Kickstart measured hard-clip aliasing at
  **-62.5 -> -90.2 dB at the same 4x** with first-order ADAA. Free 28 dB.
* **ZDF/TPT filters throughout** — no naive digital resonance.
* **DC blocked AFTER the ceiling**, with the hard bound LAST (B2311.104:
  an odd saturator on an asymmetric wave has non-zero mean, and a blocker's
  transient overshoot escapes a ceiling placed before it).
* **THE COMB TRAP, and it is serious at bass frequencies.** Any
  interpolation inside a recirculating delay is a lowpass applied hundreds
  of times a second — Thin Walls measured 3-5x of the late field's energy
  lost and a quarter off every 4 kHz decay. **Integer delays in the loop,
  fractional correction outside it**, or a tuned comb on a low note will be
  both dull and flat.
* **Tuning is a bench number, not a hope.** Every filter self-oscillates at
  0 cents; every comb tunes within a couple of cents. Black Rider needed a
  piecewise-in-K prewarp because the clipper's drag GROWS with resonance —
  expect the same here and measure it rather than assuming one constant.
* **A level-matched wet path**: MIX at 0 must be the input, bit-exact where
  the arithmetic allows.

---

## 7. THE BENCH (written before the code, as usual)

`gwtest` must measure the CLAIMS, not just boundedness — the Martian Gain
rule that a patch cable wired to nothing passes every bounded test:

1. **Dilation is the formula.** A modulator at radius r under mass m runs at
   `sqrt(1 - rs/r)` of nominal, measured, across a sweep of both.
2. **The horizon really freezes**, and resuming does not click (broadband
   energy above 6 kHz at the crossing, the Thin Walls click metric).
3. **Redshift moves the spectrum and NOT the pitch** — centroid falls by
   the predicted factor while f0 holds within a cent. This is the check
   that protects the central idea.
4. **Ringdown ratios are the QNM ratios**, and `omega ∝ 1/M`, `tau ∝ M`
   across a mass sweep.
5. **MASS 0 is an ordinary synth** — memcmp-identical to a build that never
   had the well, so the Kemper rule holds and nothing is forced.
6. **The SINGULARITY survives** everything: full drive, both filters
   closed, and the protected band is still there at the predicted level.
7. **The sub is phase-coherent** with the fundamental (measured
   reinforcement, not cancellation).
8. Tuning, aliasing, DC, 300 random machines bounded, four sample rates.
9. **"Bounded is not stable"** (Battlestar): stop the input and watch, for
   every feedback path — the comb, the ringdown bank and the disk.

Plus `gwhost` (the real wrapper in a console host) and `gwshot` / a CDP
panel probe, because a panel is only proven live.

---

## 8. OPEN QUESTIONS FOR PETER

1. **Mono or paraphonic?** A bassline synth is monophonic by tradition, and
   MASS-as-signal-energy is cleanest with one voice. But doom and drone
   want dyads. Proposal: **mono with a 2-voice option**, glide/legato
   first-class.
2. **Should MASS be automatable as well as signal-derived?** It should be
   both — a host parameter that SUMS with the follower — but the two-owners
   trap from the BWFX macro round says say so explicitly up front.
3. **Sequencer?** The 303 is half sequencer. The fleet already has one in
   Full Metal Racket. Proposal: **no sequencer, but accent and slide as
   first-class per-note inputs** — that is the part of the 303 that makes
   the sound, and the DAW does the rest.
4. **How far should redshift be allowed to go?** Past a point a formant
   bank pulled far down stops being weight and starts being a cartoon.
   Wants a listening decision, not a number I pick.

---

## 9. THE BRIEF (Peter, 2026-09-26)

The last of the plugins, and a worthy end to the series. A bassline synth
that embraces the classics — Black Rider's oscillators and Moog/MS-20
filters, the 303, the modern Massive end — with real innovation in
MODULATION. Not overcomplicated (Massive is daunting; 100 knobs is not the
goal) but refined enough for intricate basslines: techno, dub, some
dubstep, and particularly doom metal, industrial and electronica. **Sound
quality is king** — top-line oscillators, analogue-like or digital. Beyond
the classic filters, modern ones including comb, and at least two filter
banks that can run parallel or in series. An oscilloscope to see the
waveforms. Black-hole visual design and vocabulary (mass, depth, energy,
horizon, time, space), with live animation in the panel — not decals — that
pulses with the bass and suggests a gravitational well, using real physics
and maths.

And the question the design has to answer:

> How can the known and loved be united with something original, which is
> not just a gadget, but truly helps shorten the path from a basic sound to
> deep/rich/complex and highly inspiring one?

**The answer this design gives: you do not dial complexity, you add mass.**
