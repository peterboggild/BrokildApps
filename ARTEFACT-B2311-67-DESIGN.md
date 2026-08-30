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

### 2026-08-30 later · the delivered graphics, and the provenance card

**Eight decals arrived and all eight are in use.** The artefact's own geometry
stays computed, as it must — it is a live cut through a four-dimensional
lattice and no bitmap could keep being correct as the player travels. What the
images supply is what the facets REFLECT, which until now was nothing.

- DARK MINERAL carries most of the body; ANODISED SPECTRUM is multiplied by the
  thin film (a film modulates the reflectance of whatever comes off the
  surface, so it belongs inside the reflection and not beside it); WET GLASS
  does the rim; COLD FIRE is an accent scaled by each habit's glint.
- The striations tile replaced the procedural sine — sampled rather than
  computed, so there is nothing left to alias.
- The crate wears the brushed aluminium, the wear mask over it, and the corner
  fittings. The crate is the one part that should look photographed: it is ours.

**The bug worth keeping:** a matcap is indexed by the normal in view space, and
this artefact is a nearly flat slab, so every facet pointed at the camera and
every tile sampled the same spot — the dead centre of the sphere, which on the
dark maps is almost black. The maps loaded correctly and contributed nothing.
A slab this wide is not seen down a single axis: the direction from the eye to
a point on it swings as you look across, and that is what sweeps a reflection
over a polished surface. Putting the eye at a finite height above the centre
and computing the view per point fixed it; the facet normal then supplies the
local break-up on top of the sweep, which is how a cut stone divides the work.

**PROVENANCE.** Both artefacts now carry a card: the globe, a pin at the find
site, a fainter mark at the sister specimen's site, and the field record.

B2311.22 was lifted from a dry cistern at Kell Rille on day 81 — a matched
PAIR, four metres apart, held at that separation by nothing anyone could see;
on lifting the first, the second changed pitch. That is the Interlocutor's own
physics told as a find. B2311.67 came 123 days later from a collapsed
evaporite vault at the Sabik Terminator, one of sixty-seven laid out on an
eightfold plan, each at the centre of its own void, none touching — and it was
logged as mineral until the survey's seismometers, left running beside the
crate, returned a spectrum with gaps at every scale.

The two cards cross-reference each other and share one globe, so the pair reads
as one collection. Nobody on the survey was an archaeologist.

### 2026-08-30 · the output stage, and why the sustain was so static

**Peter's two reports, both correct, both measured.**

*"The tone that remains while a key is held lives a quieter life."* Measured:
the filament keeps **0.00 %** of its own transient into the sustain — not less,
none. Everything visible on screen is the FILAMENT, and the filament is a
percussive voice: struck, rings, dies, and nothing re-excites it while a key is
held. The sustained tone is carried entirely by the STAR.

*"Even transiting through dimensional cross-sections leaves this background
dissonant chord almost unchanged."* Also correct, and mostly for a right
reason: the star's peaks sit at f0*(p + q*sqrt2), the reciprocal module of the
lattice. Travelling slides the WINDOW through w; it does not rotate the
lattice, so the module is untouched. Travel changes the structure factor —
the amplitudes — and that does move (3.35 at tau 3.8, 4.87 at tau 8.4 on a
20-band log metric, against 8.44 for the transient). Same chord, re-weighted.

The wrong reason is mine: **OBLIQUITY is a dead control.** It is in the
parameter table, is exposed to the host, and sits on rosette 4 ring 4 — and
grep finds it in exactly two places, the SPECS table and the Params struct.
Nothing consumes it. It is precisely the control this document promised in §1
("turn the cut plane and you pass through a devil's staircase between crystal
and quasicrystal"), and the slope of the cut is what sets the module. It is
therefore the one knob that would make the background chord move, and it was
never wired. Listed on the buglist, not built.

**THE OUTPUT STAGE.** What was here was a tanh waveshaper, and the giveaway
was in this bench all along: all 256 habits peaked at 0.9349, which is that
waveshaper's own asymptote. The output was not occasionally clipping, it was
sitting IN the clipper almost permanently. Peter's call — "a musical
compressor with a brickwall at the end" — is the right architecture, and it is
what is there now:

* **compressor**, soft knee, 3:1, threshold at 0.55 of the ceiling, with a
  fast and a slow release running together and the LOWER gain winning. That
  last part is what makes a compressor programme-dependent rather than merely
  quieter: one release either chatters on transients or drags through a
  phrase; two, taken as a minimum, do neither.
* **brickwall**, 4 ms look-ahead, gain reduction only, declared to the host as
  latency so it is compensated.

Two bugs on the way, both in the brickwall and both classics:

1. **The detector released during the look-ahead.** At a 40 ms release the
   envelope falls by exp(-192/1920) = 0.905 before the peak it saw actually
   emerges, so the gain is ten per cent too high exactly when it matters. It
   must HOLD across the window.
2. **Two cascaded smoothing poles chasing a moving target lag it.** At
   tau = la/5 a pair reaches only 96 % within the look-ahead where a single
   pole reaches 99.3 %, and the missing percent is overshoot. Track the target
   exactly, smooth once.

Measured after: driven flat out with three notes at full level the worst peak
is **0.9328 against a ceiling of 0.935** — under it. Played quietly the gain
reduction is **exactly 1.000000**, so below threshold the stage does nothing at
all. A held chord moves **1.66 dB**, so it does not pump. 52 checks all clear,
and the CPU went down, not up: 15.7 % of a core to 12.9 %.

### 2026-08-30 · OBLIQUITY: two wirings measured and thrown away

The dead control was wired two different ways and both were measured. Neither
is shipped, and the reason is worth keeping, because it says what the control
actually costs.

**First attempt — tilt the CHAIN.** A site's position along the cut becomes
`s = x_par + t*x_perp`, which for an axis cut is `(1+t)A + (1-t)B/sqrt2`: the
ratio of those coefficients is the slope, and it sweeps continuously. Measured
across four habits and sixteen tilts:

* one habit (37, bearing index 6) did not move **at all** — the conjugate term
  is constant along that bearing, so the knob was dead exactly where the
  instrument is most nearly periodic;
* the mean spacing jumped from 0.8975 to 0.6305 at the FIRST step and then sat
  flat — a discontinuity, not a sweep;
* the minimum gap fell to **0.0186**, and stiffness goes as `length^-bondExp`
  with bondExp 1.6, so that bond is some eight hundred times stiffer than its
  neighbours;
* cents-from-harmonic moved 8.0 to 8.5 across the whole range. **No crystal, no
  staircase.**

Adding a bounded oscillating term to the positions and re-sorting does not
change the slope, it scrambles the order.

**Second attempt — tilt the MODULE.** `lam = p + q*tau` with tau sweeping from
sqrt2 to 1, coincidences merged (at tau = 1 every one of the 1485 pairs lands
on an integer). This is arithmetically exactly right, and the endpoint proves
it: at tilt 1.000 the chord is **0.0 cents from a harmonic series** and the
lowest partial is exactly 1.0 — a crystal, an ordinary bell.

Everywhere else it is wrong, and the bench said so in one line: the chord moves
**62 units in the first 2.5 % of travel** and then merely wanders between 54
and 82 for the remaining 97.5 %.

That is not a staircase, it is a collapse, and the cause is structural. The
intensities are the structure factor of the chain, and at tau = sqrt2 the
module coincides with the chain's own module, so every partial sits exactly on
a Bragg peak. Move tau by anything at all and every partial falls off its Bragg
condition together, into the diffuse background. **The star stops being the
diffraction of the filament the moment the knob leaves zero** — which is the
one thing this instrument may not do, since "conjugate readings of the same
integers" is the whole design.

**What wiring it properly costs.** The two readings have to move together, and
the only way that is true for an arbitrary tilt is to stop enumerating the
module and instead FIND the peaks in the chain's own structure factor — scan k
across the audible range and take the local maxima. `amplitudeAt()` already
exists and the scan is about a millisecond, so the cost is not CPU. The cost is
that peak SELECTION changes for every specimen at tilt zero too, so all 256
shipped chords move, and the catalogue's alienness floor would have to be
re-measured and possibly re-seeded.

That is a decision about the instrument's own sound, so it is Peter's, not
mine. Until he takes it, OBLIQUITY stays on the buglist and stays dead — and a
dead control is at least honest, where a knob whose first two per cent does all
its work while claiming to be a devil's staircase is not.

### 2026-08-30 · OBLIQUITY: the third wiring, which is the one that works (build 260830.2)

Peter's call, given the cost: *wire it properly, accept the reseed.* Two
changes, and they only work together.

**The star now FINDS its partials instead of assuming them.** What was there
evaluated the chain's structure factor at ratios taken from the module
`Z + Z*sqrt2` — the module of the *unstrained* lattice. That is exactly right
while the cut lies flat, and wrong the instant it does not, which is why both
earlier attempts failed. So the scan runs across the structure factor, takes
its local maxima and refines each one: the answer is the true diffraction of
whatever chain is actually sounding. A phase recurrence, re-seeded from the
trigonometry every 256 steps, keeps 42 000 scan points × 170 sites down from
eight million sines to about 20 ms.

Two things had to be put back, and both were found by measuring rather than
reading:

* **the shadow rule.** Selecting on loudness alone filled the star with
  sub-audible rumble — 44 % of all partials fell below the fundamental, and
  habit 16's second, third and fourth loudest sat at lambda 0.082, 0.163 and
  0.120 with shadows of fourteen and twenty. A structure factor tends to unity
  as k tends to zero and the radiation rolloff then *multiplies* that forward
  beam by two and a half. The old code never showed this because its candidates
  were screened by the window's transform before the structure factor was
  consulted. Restored as the selection prior: **44.2 % → 1.9 %**, lowest ratio
  now 0.1716, which is 3 − 2·sqrt2 and a real module point.
* **the resolution limit.** Habit 8 reported partials at 0.99156 and 1.00848
  flanking its fundamental — 13.0 dB down at 1.42/169, which is the textbook
  first sidelobe of a rectangular window to three digits. Nothing closer than
  1.8/n is a second partial; it is the same peak seen again.

Sanity, measured against a control rather than asserted: in the range where the
module is densely covered, partials sit **0.0019** from it against **0.0085**
for random ratios — module-associated by four and a half times, while carrying
each specimen's own decoration.

**OBLIQUITY is a LINEAR PHASON STRAIN.** The acceptance window slides through
the unseen plane as you travel along the cut, so the far end of the filament is
judged by a different window from the near end. This is the classical route
from quasicrystal to rational approximant, and — the point — it moves *which
sites are there*, never *where they are*, so it has none of the pathologies the
first attempt had.

| | strain 0 → 0.06 |
|---|---|
| site count | constant — the filament never loses a site |
| smallest gap anywhere | 0.2929 (the first attempt reached 0.0186) |
| chord movement, worst habit | **87.3**, against 3–5 for travel |
| habits passing nearer harmonic | 20 of 24 |
| closest approach to harmonic | **0.2 cents** (worst at rest 15.5) |
| rebuild cost | 20 ms at rest, 27 ms at full, on the service thread |

**The assertion that failed, and was wrong.** The bench first demanded that the
fundamental always remain the loudest partial. It does not — and measuring the
*old* code settled who was at fault: on the enumerated star this replaced, the
loudest partial was something other than the fundamental for **8.3 %** of the
catalogue at rest (worst 3052 cents); on the new one it is 13.7 % (worst 2727).
This is how the artefact has always been, and the note itself cannot move in
any case, since every ratio is relative to the key that was struck. What the
bench holds instead is that leaning the cut does not make the exception the
rule: median excursion **0 cents**, 90th percentile 1526, beyond two octaves
**4.9 %**.

Also held: a strain of zero is the chain that was there before, to the last bit.

59 checks, ALL CLEAR. Installed at 260830.2; every bundle loads past Smart App
Control at both locations.

### 2026-08-30 · the self-check: how many controls are deaf while you play (260830.3)

Peter, three reports in a row: the sustained chord is much the same across the
catalogue, it does not evolve, it barely follows the screen — and then *"the
sounding line changes nothing in many of the catalogue entries."* All of it is
one defect with one cause, and none of it was guessed.

**You are watching the filament and listening to the star.** `visualState()`
returns `siteE[]`, a smoothed sum of the filament's displacement, and that is
the only sound-derived quantity the panel ever receives. Measured on a held
note at the specimens' own settings:

| habit | mean \|siteE\| at 0.2 s | 1.0 s | 3.0 s | 5.0 s |
|---|---|---|---|---|
| 8 | 0.448 | 0.0116 | 0.0000275 | 0.00000033 |
| 160 | 0.155 | 0.0000129 | 0.000000 | 0.000000 |

Meanwhile, holding a note with ASPECT pinned to each voice: all FILAMENT gives
a sustain of **0.000079** rms, all STAR **0.0957**. Three orders of magnitude.
The voice that drives the picture is silent through the whole sustain, and the
voice you hear has no visual representation at all.

**Why every specimen's chord is a cousin of every other.** The star's ratios
are the reciprocal module of the lattice — a property of Z^4 and the cut, not
of the specimen. Across the catalogue, lambda = 1 is among the loudest eight
partials of **151 of 168** specimens and carries **25.2 %** of all sustained
energy; sixteen ratios carry **61.7 %** of it; only ~294 distinct pitches (to
the cent) occur anywhere in 4032 partial slots. Only the loudnesses differ per
specimen. Measured against a control: median pairwise chord distance **0.693**,
against **1.420** for the same loudnesses on randomly scattered ratios. One
chord, re-voiced.

Held untouched, the chord moves **0.07–0.24** over three seconds, against
**0.81** for a whole change of specimen.

**THE SELF-CHECK.** Every parameter moved from its default *while a note is
held*, the change compared against what the untouched note did anyway. **28 of
41 controls were deaf.** The largest single cause was the star's staleness
test: a list of seven remembered parameters that never mentioned CUT BEARING,
CUT OFFSET, APERTURE, RIM, CONTRAST or the arrests — every one of them a thing
the player does to the body on screen. The chain was rebuilt every tick and
the star went on being the diffraction of a filament that no longer existed.

The fix is not a longer list, because a longer list is the same bug waiting.
The star is the chain's diffraction, so it is stale when the chain it was made
from is not the chain that is sounding — it asks the chain, which covers the
arrests and covers anything added later by construction. Result:

    CUT BEARING   0.0000 -> 1.5057        APERTURE   0.0000 -> 0.9896
    CUT OFFSET    0.0000 -> 1.3719        RIM        0.0000 -> 0.3623
    CONTRAST      0.0000 -> 1.3759

59 checks ALL CLEAR; 14.0 % of one core, from 13.3 %.

**What is still deaf, and which of it is honest.** ONSET, QUENCH, GLIDE, BEND
RANGE, INCIDENCE and STATION are per-note-event and correctly do nothing to a
note already sounding. The rest are not honest, and they share the root cause
above: LOSS, BOND LAW, LOSS TILT, STRAIN, WALK, SEPARATION, CAVITY, CAVITY
COLOUR are the filament's own physics, and the filament has died by the time
you reach for them.

**The lever that already exists.** SUSTAINED FORCE re-excites the filament —
the voice the panel draws. Measured: sustain rms **0.0094 -> 0.243** (26x),
chord distance from drive-zero **1.12**, which is larger than changing specimen
entirely (0.81), and it holds |siteE| at ~1.0 instead of 0.00003. The generator
sets it to zero for about 60 % of the catalogue (`r.uni() < 0.4`), which is
exactly why most specimens show this and a few do not.

**Not built, awaiting Peter's go** (his standing rule): raise SUSTAINED FORCE
across the catalogue so the visible voice is present in the sustain, which
would bring the filament's whole physics — envelope, damping, nonlinearity,
cavity — back under the hand while a note is held.

### 2026-08-30 · the DIFFRACTION plate is not a picture of the sound

Peter: *"does the diffraction pattern ever change, or is it static?"* Measured
live through the panel's own `setParam`, so the dirty flags really ran:

| control moved | plate signature |
|---|---|
| at rest | `110:13789425.0452` |
| TRAVERSE | unchanged |
| CUT BEARING + CUT OFFSET | unchanged |
| APERTURE, RIM, CONTRAST | unchanged |
| OBLIQUITY | unchanged |
| HABIT | unchanged |
| **WINDOW** | 12301213 — changes |
| **EXTINCTION** | 10969447 — changes |
| **ORDERS** | changes |

The source agrees exactly: `onParamChanged` raises `starDirty` for
`["starwid","startilt","peaks"]` and nothing else.

The cause is that the panel computes its own star in JavaScript —
`buildStar(wid, tilt, want)` enumerating `lam = p + q*sqrt2` with intensity
`|sinc(wid*cj)| * exp(-tilt*|cj|) / (0.35 + lam^1.15)`. That is the OLD,
idealised star: the module's envelope. It has no knowledge of the chain, of
the specimen, of travel, or of the strain — `dependsOnChain: false`,
`dependsOnHabit: false`.

Meanwhile the star you HEAR is now built in C++ from the structure factor of
the actual sounding chain. **So the plate labelled DIFFRACTION is not a
picture of what the instrument is playing**, and cannot become one while it is
computed from three parameters on the page. It is the same family of fault as
everything else found today: the display and the sound run on separate
descriptions of the body.

The honest fix is for the engine to publish its own star — it already has the
peaks, and `visualState()` is the precedent for streaming a body-derived array
to the panel — and for the plate to draw what was published. Specced, not
built; Peter's standing rule.

**Also confirmed, because it looked alarming and was not:** the rosettes slide
as the body moves, and moving them looks like the controls have been shuffled.
They have not. Across 25 travel positions spanning the full range, the angular
order of the eight rosettes was `6,7,8,1,2,3,4,5` every single time, never once
re-ordering; all eight translate together (measured 56 px, same direction); and
each rosette's five parameters are a fixed `const` table that nothing writes to.
A control map is therefore safe to draw by compass position —
`docs/control-map.png`.

### 2026-08-30 · what symmetry does the body actually have?

Peter, on the diffraction plate: *"many patterns have also cubic symmetry, and
this should be reflected in the pattern... it does not have to be a real
physics diffraction pattern."* Measured before answering.

**By bond orientation the tiling is exactly eightfold, always.** The
orientational order parameters over the edges in view:

    psi8 = 1.000    psi4 = 0.000

at every depth in w, at every aperture, for every specimen. So a literally
correct diffraction pattern WOULD be eightfold everywhere, and the plate's
hard-coded eight arms are not wrong in that sense. (psi4 cannot see it either
way: Ammann-Beenker's squares come in two orientations, 0 and 45 degrees, so
even an all-square patch averages to zero.)

**But what the eye reads as cubic does vary, and strongly — with APERTURE:**

| window scale | tiles in view | square fraction |
|---|---|---|
| 0.663 | 8 | **1.000** — nothing but squares |
| 0.775 | 64 | 0.750 |
| 0.888 | 304 | 0.526 |
| 1.000 | 640 | 0.438 |
| 1.450 | 3072 | 0.352 |

So at a small aperture the body genuinely *is* a lattice of squares and reads
cubic; opening it buries the squares in rhombi. Peter is right about what he is
seeing, and the handle is the square:rhombus mixture, not the bond angles.

Travel barely moves it (0.406 to 0.438 across the full sweep).

**And a fact worth having on its own: the geometry is IDENTICAL for all 256
specimens** — square fraction 0.438, min = median = max, across the catalogue.
A habit is a colouring of the acceptance window; it never changes which tiles
exist. Every specimen is the same body wearing different material. That is one
more face of the same thing Peter has been hearing.

**BUILT at 260830.4** (see below). The proposal was: weight the plate's eight arms by the measured square
fraction, so the four square directions dominate as the body becomes square-
dominated and even out as it does not; and let OBLIQUITY's strain visibly
distort it, since a strained approximant really does have lower symmetry. Both
are driven by measured properties of the body in view, which is what "in
agreement on a superficial level" should mean. It needs the engine to publish
its star first (see the previous entry).

### 2026-08-30 · the plate, built (260830.4)

Peter: *"ok build it."* Both halves, and both verified on the live panel.

**The engine publishes its star.** `Engine::star()` and `starGeneration()`
alongside the existing `patch()`; `emitBody` sends the peaks when the
generation moves rather than at 30 Hz, so the bridge stays light. The page
retires its own `buildStar` the moment the first one arrives — `starFromEngine`
guards the frame loop, and the old idealised star stands in only until then, so
the plate is never empty at boot. Measured live, the plate signature now moves
for every control it was deaf to:

| | signature |
|---|---|
| at rest | 18459379 |
| TRAVERSE | 21519775 |
| CUT BEARING | 23849750 |
| CUT OFFSET | 20111846 |
| OBLIQUITY | 24892026 |
| HABIT | 24210996 |

**The symmetry follows the tile mixture,** not the bond angles — because the
bond angles cannot express it. `Engine::squareFraction()` counts squares among
the tiles in view and rides on every body message; the four square directions
gain as it rises and even out as it falls, and OBLIQUITY shears the figure.
Verified live: **aperture 12 % → square fraction 1.000 → a four-armed cross;
aperture 92 % → 0.349 → a full eightfold star** (`shots/plate-cubic.png`,
`shots/plate-octagonal.png`).

59 checks ALL CLEAR.

### 2026-08-30 · the clicks on habit 240, and the gain structure (260830.5)

Peter: *"it sounds like the sound engine clips or glitches on some patches...
the lower left one in the catalogue has some fast clicks. Is that intentional
or some problem with the gain structure?"*

**Which patch.** The catalogue grid is `repeat(16, ...)`, 256 swatches, so the
lower-left swatch is **habit 240** — and it is the loudest patch in the
catalogue.

**What it is not.** Four suspects, each with a decisive test:

* *not the star rebuilding.* Today's staleness change makes the star rebuild
  whenever the chain changes, and the obvious worry was that a rebuild swaps
  every partial at once mid-note. Measured: **zero rebuilds during a held
  note**, on every habit sampled. The hypothesis was mine and it was wrong.
* *not a discontinuity.* Across all 256 habits, **0** have a sample step above
  eight times their own 99.9th percentile. The worst step ratio in the
  catalogue is 5.5 (habit 230).
* *not a per-block splice.* Of the 200 largest steps, **0** sit on a block
  boundary.
* *not limiter splatter.* Habit 240 at its own ceiling versus wide open gives
  high-frequency shares of **0.01692 and 0.01693** — identical. The limiter is
  not generating the distortion.

**What it is: the patch lives inside the limiter.**

| habit | blocks with gain reduction (of 300) | mean gr | min gr |
|---|---|---|---|
| **240** | **300** | 0.7466 (−2.5 dB) | 0.4312 (**−7.3 dB**) |
| 8 | 10 | 0.9954 | 0.8998 |
| 96 | **0** | 1.0000 | 1.0000 |

Habit 240 is under gain reduction in every single block of a three-second note,
averaging two and a half decibels and reaching seven. Habit 96 never touches the
limiter at all. That is a **twenty-decibel spread of loudness across the
catalogue**, and the limiter riding hard and fast on a dense inharmonic signal
is what is being heard as fast clicks.

It is worse on some notes than others — habit 240's minimum gain reduction runs
from 0.5566 at note 46 to **0.1255 (−18 dB) at note 58**, and its
high-frequency share rises six-fold across the keyboard (0.0083 to 0.0504).

So: not intentional, and not clipping. A gain-structure problem, exactly as
Peter guessed.

**BUILT at 260830.6** (see below). The proposal was: give each habit a level trim so no patch sits
permanently in the limiter — measured per habit at generation, the way Mars
Wars measures its auto-gain rather than modelling it. It changes the balance of
the whole catalogue, so it is his call.

### 2026-08-30 · the level trims, built (260830.6)

Peter: *"go do it!"*

Every habit is rendered across five notes, the peak the output stage is handed
**before it acts** is read, and the trim is whatever puts the worst of those on
0.50 — just under the compressor knee at 0.514. Applied at the output multiply,
after every nonlinearity in the instrument, so the pre-limiter peak scales
exactly with the trim and one pass is exact.

It only ever **reduces**. A quiet specimen is quiet because that is what it is;
levelling everything would flatten the catalogue. 74 of 256 are untouched, and
25.5 dB of honest dynamic range survives.

| | before | after |
|---|---|---|
| habit 240, blocks under gain reduction | **300 of 300** | 0 |
| habits spending half a note in the limiter | many | **0 of 256** |
| worst blocks limited in one note | 300 | **0 of 190** |
| pre-limiter peak | 0.0567 … **12.12** | 0.0267 … **0.5000** |

**The machinery, and why each piece exists.** `Engine::preLimitPeak`, because
the peak *after* the output stage is just the ceiling — the same number for
every patch, telling you nothing. `ab::habitTrim()` reading
`Source/HabitGain.h`. And `test/gaincal.cpp`, which generates that table and is
**idempotent**: it folds out the trim already compiled in, so running it on a
calibrated build reproduces the same table instead of trimming the trim. That
property was worth building — the first version was not idempotent and would
have silently squared the correction on the second run.

A measured table goes stale the moment anything moves the level, so **bench
section 8e** holds it: no specimen spends half a note in the limiter, none is
handed to the output stage far above target, and no trim reduces a specimen to
nothing. 62 checks ALL CLEAR.

**Left standing, and flagged:** habit 1 hands the output stage a peak of **606**
and needs −62 dB. That is a near-runaway resonance rather than a loud patch. The
trim contains it, and the bench now proves it is contained, but the specimen
itself deserves a look on its own terms.

### 2026-08-30 · TEMPERATURE and the per-specimen modulation matrix (260830.7)

Peter's brief: a rosette control turned up should set three or four **other**
controls moving — oscillating, or oscillating *faster* — with the wiring unique
to each entry in the catalogue, and a TEMPERATURE governing how much happens at
all. Nitrogen as now; room temperature the default; 800 K the most. Then, on
second thought: *"major scene controls should not be affected. I think. Maybe at
very high temperatures."*

**A feedback loop is not prevented, it is unrepresentable.** A wire's SOURCE
reads the **stored** parameter — the knob position the host and the panel hold —
and its DESTINATION is written into a separate effective array that nothing
reads back. One hop, always, and `apply (const float* stored, float* eff, float
kelvin)` is what enforces it. A matrix wiring A→B and B→A is then simply two
independent wires. Bounded excursion is kept as well, belt and braces, and the
bench measures the worst at **0.3000** of range against an allowance of 0.3000.

That is also exactly what was asked for: "increasing one of these rosette
controls can mean that 3-4 other parameters oscillate" describes a single hop
from a knob position, not a network. Four sources per specimen, each fanning to
two or three destinations — measured live at room temperature, **four**
parameters were moving at once (CONTRAST, FORCE COLOUR, ASPECT, FALL).

**The temperature law.**

| | material | scene (TRAVERSE, CUT BEARING, CUT OFFSET) |
|---|---|---|
| 77 K | **nothing, to the bit** | nothing |
| 293 K (default) | ±8.96 % of range | **0.000000** |
| 500 K | ±19 % | begins |
| 800 K | ±30 % | ±12 % |

77 K is not "nearly nothing": the offsets are skipped entirely, so a frozen
instrument is bit-identical to one built before any of this existed — memcmp
over four hundred steps in the bench, and confirmed live, **0 of 42** parameters
moving. At 800 K, **141 of 256** specimens stir the scene. Some bodies hold
their frame even when far too hot and others do not, which is character rather
than a fault.

**Never modulated**, and not out of squeamishness: LEVEL and CEILING would make
the instrument breathe in loudness, which is a fault and not a feature;
CONFORMANCE, REFERENCE, REGISTER and BEND RANGE would detune a held note; HABIT
would change the specimen underneath its own matrix; EXTENT and ORDERS are
stepped, and rebuilding the body to a different SIZE thirty times a second is
both expensive and audible.

**Where it lives, and why.** In the processor, not the engine — the engine's
parameters are refreshed wholesale from the host every block, so the three
places that did that now come through one `refreshParams`, stored values in and
effective values out. The engine never learns the difference and needed no
change.

**And the panel shows it.** The effective values ride back on the body message
and the rings are drawn from those, so a stirring body is one you can *see*
stirring — except the ring under your hand, which follows the hand, or it would
lag a frame behind the finger.

Bench section 8f holds all of it, including that the same specimen modulates
identically every time it is played, so a bounce matches what was heard.
71 checks ALL CLEAR, 14.0 % of one core.
