# PROXIMS CENTAURI B · ARTEFACT B2311.67

*Design document. Written before the build, amended with as-built measurements.*

> Commissioned 2026-08-30. The brief: a synthesiser that looks, sounds and is
> *operated* in a way that does not resemble anything human — while remaining a
> VST3 a person can play with a mouse. Explicitly: explore the negative space
> between human knowledge; incorporate the fourth dimension, with the mouse
> wheel dragging the instrument through our cross-section like a soap bubble
> through a plane; no Giger; graphics dazzling, intricate, gorgeous, *realistic*.
>
> A sibling artefact (B2311.22) exists from the same brief. It was not read,
> opened or referenced. Where my working notes already summarised it, that
> summary was used only as a list of things **not** to do: no graph-Laplacian
> eigenmodes, no metaball tissue, no creature/organ/anatomy metaphor, no
> stir/stroke/press gesture grammar, no second listening body, no Hebbian
> scars. Every mechanism below comes from a different branch of physics.

---

## 0 · The thesis

Human sound has exactly two categories, and mathematics knows a third.

| | spectrum | what it is | who has made it |
|---|---|---|---|
| tone | **pure point** (discrete) | strings, pipes, bells, every oscillator | everyone, for 40 000 years |
| noise | **absolutely continuous** | breath, cymbals, hiss, chaos | everyone |
| — | **singular continuous** | a Cantor set of measure zero, with self-similar gaps; states neither localised nor extended, decaying by *power laws* rather than exponentials | **nobody** |

The third row is not a curiosity. It is the generic spectrum of **quasiperiodic
media** — matter that is perfectly ordered and never repeats. It has been
measured in real solids, and it has never been an instrument.

That is the negative space the brief asked for. Not weirder noise, not stranger
FM: a category of sound that is lawful, structured, definitely *not* random, and
that no human ear has been trained on, because no human material does it.

## 1 · Why this is genuinely four-dimensional

The artefact is a **four-dimensional cubic lattice, ℤ⁴**, decorated.

An octagonal quasicrystal — the Ammann–Beenker tiling — *is* a two-dimensional
cross-section of ℤ⁴. This is not analogy. The construction is:

- split ℝ⁴ into two planes, E∥ (what we see) and E⊥ (what we do not),
  by the eightfold rotation acting on the lattice;
- keep the lattice point **n** if its shadow π⊥(**n**) falls inside a window
  **W** — the octagon that the unit hypercube casts into E⊥;
- draw the survivors' shadows π∥(**n**) in our plane.

Out falls a tiling of squares and 45° rhombi with eightfold symmetry, ordered,
aperiodic, and computed to the last vertex from four integers.

**Everything the player does is a four-dimensional act.**

- The **wheel** translates the window: `π⊥(n) − γ ∈ W`. That is literally the
  cut plane sliding through the fourth dimension. The tiling reorganises by
  **phason flips** — the real physics of that motion, local rearrangements that
  ripple, matter entering and leaving existence at the window's rim.
- The window has a **soft rim**, so a site does not blink out: its couplings
  fade to nothing as it leaves the slab. A soap bubble crossing a plane does not
  vanish; its circle shrinks to a point. So does a site here.
- A site's **mass is a function of its ⊥ position** — of *where it stands in the
  fourth dimension*, which is the only thing that determines its local
  environment. Move through w and the material's own composition changes.
- **Pinning** a vertex pushes the window's rim outward in that vertex's ⊥
  direction. The player *deforms the four-dimensional body*, and thereafter
  every slice of it — past, future, elsewhere — is different. The instrument's
  memory is not on the surface; it is in the dimension we cannot see.

## 2 · The two engines are Fourier transforms of one another

The artefact sounds in two aspects, and they are the same object read in direct
and reciprocal space. Both are derived, not designed.

### FILAMENT — direct space

Draw a line across the tiling. The vertices it passes through, ordered, are a
quasiperiodic chain. Give each a mass from the ⊥-field and each bond a stiffness
from its length, and run the actual wave equation on it at audio rate:

```
  a_i = [ k_{i-1}(u_{i-1} − u_i) + k_i(u_{i+1} − u_i) ] / m_i − ζ_i v_i
```

That chain's spectrum is the Cantor set. Its eigenstates are *critical*: energy
injected at one point spreads **anomalously** — as t^β with 0 < β < 1, neither
diffusive nor ballistic — so the sound arrives smeared by a power law. No
reverb, no resonator, no physical model in any studio decays like that. It is
the sound of a material that cannot decide whether it conducts.

Chain lengths are **Pell numbers** — 5, 12, 29, 70, 169, 408 — the silver-mean
analogue of Fibonacci, which is this tiling's own sequence.

### STAR — reciprocal space

The diffraction pattern of the same line is a dense set of Bragg peaks at
frequencies `f₀·|p + q√2|` for integers p, q. Its intensities are exact,
because the Fourier transform of a zonotope window is a *product of sinc
factors*, one per generator:

```
  Ŵ(q) = Π_j  sinc( q · v_j / 2 )
```

The rule this produces is the strangest and most beautiful in the instrument:

> **A partial is loud when its Galois conjugate is small.**
> The partner of `p + q√2` is `p − q√2`. That number is the partial's shadow in
> the fourth dimension, and it decides whether you hear it.

And when the player travels — when γ moves — **every partial's phase advances at
its own rate, 2π(p − q√2)·γ**. A hundred partials rotating at a hundred
incommensurate rates. There is no chorus, phaser, or spectral effect that does
this, because the rates are not a modulator's; they are the coordinates of each
partial in a dimension we are looking at edge-on.

So: **what you see is the slice, what you hear is a wave on it, and the shimmer
is its diffraction.** Sight and sound are two projections of one body.

## 3 · Tuning derived from the artefact, not from us

The Ammann–Beenker tiling is self-similar under inflation by the **silver ratio
δ = 1 + √2 ≈ 2.414**. That is the object's own octave. Not 2:1 — 2.414:1,
which is 15.14 semitones.

The keyboard therefore climbs in the **silver word**: two step sizes, L and S,
in the ratio 1 + √2, laid in the aperiodic order the tiling itself generates
(L → LLS, S → L). Seventeen keys per silver octave (12 L, 5 S — the ratio
12/5 = 2.4 is the best small rational approximation to δ). Play a shape, move it
seventeen keys, and it is *not* the same shape: the interval pattern never
repeats, exactly as the tiling never repeats.

A **CONFORMANCE** control bends this continuously toward 12-tone equal
temperament, because the brief also says a human must be able to use it. The
compromise is legible rather than hidden: you can see how far you have bent the
artefact toward yourself.

## 4 · The interface: not a panel, the object

There are **no knobs, no sliders, no numeric fields, and no words on the
artefact.** There is a rectangle, because our format has one — so the rectangle
is dressed as *ours*: a collector's crate, industrial, stencilled, human. The
artefact inside it is an octagon, and the corners of the frame are visibly dead
space. The format's limitation is made part of the fiction rather than hidden.

Controls, in order of how obvious they are:

1. **TRAVERSE** — the wheel, anywhere. Depth in the fourth dimension. This is
   the commissioning brief's soap bubble, implemented as the thing it actually is.
2. **BEARING** — you are travelling in a *plane*, not along a line, so the
   direction of travel is itself a control: a winding ring at the rim.
3. **THE CUT** — a luminous line across the tiling. Its ends are draggable. It
   decides which vertices become the sounding chain. Moving it does not adjust a
   parameter; it chooses *where in the body you are listening*.
4. **ARRESTS** — click a vertex to pin it. The window deforms. The tiling
   changes everywhere, permanently, including in slices you have not visited.
   Shift-click releases. This is the interface changing because of what you did.
5. **ROSETTES** — the scalar controls, as **winding** figures at the rim. They
   have no minimum and no maximum and no pointer: you wind them, and they answer
   with an eightfold interference figure you read as a *pattern*, not a number.
   A human knob has stops and a scale; this has neither, and that is the point.
6. **THE APERTURE FIGURE** — a second, small octagon showing E⊥ itself: the
   window and the cloud of ⊥ shadows. The fourth dimension displayed directly,
   as a crystallographer would draw it. Dragging inside it moves γ.
7. **THE GRADE** — the keyboard, as a strip of unequal cells: wide for L,
   narrow for S. The silver word made playable and visible.

**The decoding strip.** A thin line of small human type along the crate — *not
on the artefact* — naming whatever the pointer is over and what it reads. The
human words are ours, written on our packaging. The object never speaks.

## 5 · The catalogue

A **habit** (crystal habit — a specimen's characteristic form) is not a preset
in the human sense. It is a **decoration of the fourth dimension**: a scalar
field over the acceptance window, plus the window's own shape, the cut's
direction, the damping law, and the balance of the two aspects.

Because the mass field is a function on W, and W lives in E⊥, *a habit is
literally a colouring of the dimension we cannot see* — and the same field
colours the tiles on screen. What you are looking at is the preset.

256 habits, generated deterministically, bench-checked for distinctness by
spectral fingerprint. They carry no names, because nothing here is named: an
index, two invariants, and a colour. Alien objects are catalogued, not
christened.

## 6 · What makes the graphics *real* rather than styled

Realism was a stated priority, so nothing is drawn "in a style". Every pixel
comes from a physical model:

- **Structural colour.** The tiles are iridescent by thin-film interference,
  computed from an optical path length that *is* the site's ⊥ coordinate. Hue
  therefore encodes distance in the fourth dimension. This is how real
  colour-without-pigment works — beetle elytra, opal, nacre — and it changes
  correctly with viewing angle.
- **Anisotropic specular** aligned to the eight tile-edge directions, so the
  surface throws an eightfold glint that swings as the light moves.
- **Ammann bars** — the tiling's own five families of exactly straight lines,
  which are a true geometric property of Ammann–Beenker and are startling to
  look at — drawn as fine luminous filaments.
- **The diffraction star**, overlaid: the real reciprocal-space image, dense,
  eightfold, with intensities from the exact window transform.
- **Live acoustic luminance**: per-site displacement streamed from the engine,
  so the sound is visibly running along the filament.

No decal, bitmap or generated artwork is required. Everything is computed, which
is the only way it can *keep being correct* as the slice moves. (If, later, we
want micro-detail — a surface roughness field for the crate's metal — that is
the one place a supplied texture would help.)

---

## Build log

*(as-built notes, measurements and bugs are appended here during the build)*

### 2026-08-30 · first prototype, build 260830.1

Built at `C:\Users\peter\b\ArtefactB2311_67`; installed to
`Common Files\VST3\Brokild\Proxima Centauri B findings\`. Bench: 47 checks.

**What the bench measures, because the claim needs measuring:** gaps wider than
4/2/1/0.5 % of the range come out 3/5/9/14 for the artefact, 2/2/2/2 for a
period-two crystal, 0/0/1/7 for a disordered chain of the same two letters —
a count that grows as you look finer and survives the chain growing six times
longer, which is what a Cantor spectrum IS. Wavepacket spreading: 0.999
ballistic, **0.763 the artefact**, 0.300 localised. The star's partials measure
1.001, 1.417, 2.005, 2.420, 3.007, 3.425 against 1, √2, 2, 1+√2, 3, 2+√2.

**Bugs worth keeping, in the order they cost time:**

- **A three-deep buffer rotation declared two deep.** `chains[2]` with a writer
  publishing to `(cur+1)%3` trampled everything after it in the object. Found
  with AddressSanitizer after an hour of reading code that was correct.
- **The probe lied, four times.** A gap finder whose threshold sat below the
  mean level spacing reported 222 "gaps" in a crystal that has one. A transport
  measurement taken after the wavepacket had hit the wall gave β < 0 for a
  uniform chain. A broadband packet measures β = 1 in any medium, because the
  long-wavelength end always travels. And taking the argmax of a spectrum to
  test scaling reported a factor of 0.51 for a spectrum that had scaled
  perfectly. **Every one of these looked like a fault in the instrument.**
- **LOSS as a raw per-step coefficient** made the artefact's memory depend on
  the note played, and worked out at ninety milliseconds by default — the
  fundamental had died before the measuring window opened.
- **The cavity was positive feedback.** Injecting the shared bus as a force
  while listening seventeen sites away is non-collocated velocity feedback,
  which is not dissipative; into a chain of Q≈900 it reached 147× full scale.
  A shared body is a SPRING. And the spring has to read the displacement of the
  current substep: written against last sample's value it is a negative
  stiffness at the top of the band, and habits with a long decay rang up to ten
  kilohertz after the note was released.
- **Travelling pumped energy, and the route did not matter.** Each individual
  path was found and fixed — the stiffness normalisation taken after the
  occupancies rather than before, a grounding spring that pulled arriving
  matter to zero while its neighbours swung, a relaxation toward the local mean
  that adds energy whenever a heavy site sits between light ones. So the
  constraint is now imposed on the total, once: **dragging the body through our
  slice may take energy out of the sound and may never put energy in.**
- **The pins did nothing, twice.** The enumeration box was sized from the
  undeformed window, so an arrest pushed the acceptance rim out to hold a site
  and the search never looked that far — the window grew, the body grew a lobe,
  and the one site the pin existed to keep was the one site lost. Then the cap
  on the bulge was too tight to survive a useful travel. A pin now holds, shows
  its strain, and tears loose when the body will not carry it.
- **Every specimen faded into the same background** (Peter's report). It had to:
  a held note is carried by the star, and the star's peaks were placed by the
  window's transform — and the reciprocal module of an octagonal quasicrystal
  belongs to the LATTICE, not to what decorates it. The intensities are now the
  structure factor of the actual sounding chain, which also makes the star
  exactly rather than approximately the Fourier transform of the filament.
  Calibrating that needed care: the mean spacing of a *finite* chain is not the
  chain's mean spacing (1.16809 against 4−2√2 = 1.17157, three parts in a
  thousand), and over seventy spacings that drifts a fifth of a cycle and
  cancels the fundamental. The fundamental is now found, not assumed.
- **Stepped rosettes could not be wound.** Rounding the value on every pointer
  move throws the increment away, so EXTENT, APERTURES and BEND RANGE were dead
  while the other thirty-six worked. Wind continuously; round on the way out.

**The look took six passes and the lesson was the same each time:** shading each
tile independently gives a mosaic however carefully each tile is made. What
makes a picture read as an object is illumination and colour varying on the
scale of the OBJECT. Also: build normals analytically. Screen-space derivatives
of a height field with fine detail alias, and a sharp specular on top of that
turns the chamfers into white confetti that looks like broken hardware.
