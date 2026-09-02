# PROXIMS CENTAURI B · ARTEFACT B2311.1

*Design document. Written before the build, amended with as-built measurements.*

> Commissioned 2026-09-02. The brief, in Peter's words: an object, possibly
> multidimensional, which on heating emits strange rhythmic pulses — complex,
> made of shorter and longer almost-percussive elements, sounding like nothing
> heard on earth and certainly not like drums. It may be a timekeeper of sorts,
> counting some kind of event. Exposed to an external pulse it *seems to ease
> towards synchronising*, yet several layers keep their own timings, correlated
> by the object seen in the 2D cross-section. Interacting with the display
> changes the rhythms drastically, but always in relation to the imposed pulse.
> **The sounds do not come from oscillators — something else, maybe mathematics.**
> The researchers have established nothing about its purpose: art, science or
> communication; alive, machine, artwork or instrument; whether sound is its
> function or a side-effect. Or words. Or the ticks of an alien clock.

Two sibling artefacts exist. Neither was reused. **B2311.22** is graph-Laplacian
eigenmodes, a body with organs, a second listening body. **B2311.67** is
quasiperiodic cut-and-project, singular continuous spectra, diffraction. Every
mechanism below comes from a third place, and two candidates were built and
thrown away before one earned its place.

---

## 0 · The thesis

Human sound is made by things that **vibrate**. This object makes sound by
things that **happen**.

There is no oscillator anywhere in it. The waveform is a *point process* — a
list of moments at which something occurred — and rhythm, pitch and timbre are
not three properties but one, the rate of events, read at three timescales.

| events per second | what a human calls it |
|---|---|
| 0.1 – 20 | rhythm |
| 20 – 20 000 | pitch |
| irregular | noise |

Nothing in the instrument knows which of those it is making. That is the whole
idea: the same mechanism, sped up, stops being a beat and becomes a tone, and
the object does not distinguish between them because there is no reason to.

---

## 1 · Two mechanisms built, measured, and rejected

Neither was rejected on taste.

### The abelian sandpile

Genuinely a counter — it counts grains, and its avalanches are its output; the
sizes are power-law without anyone choosing them. On a four-dimensional lattice
it does not work, because four dimensions is *above the upper critical
dimension* for this model and the pile drains through eight neighbours faster
than it can build:

    median avalanche        1 toppling
    largest in 40 000       286
    decade ratios           6.02, 19.27   (a power law keeps them alike)
    events in 8 s driving   3

There is nothing to hear. Discarded.

### The excitable medium

Waves through a medium, spirals holding their own periods. This one is alive,
affordable and has a real intrinsic rhythm — and is **completely deaf to an
external pulse**, which is the one thing the brief insists on.

    phase concentration R   0.001   at every pacemaker size to 1089 sites
    its own rate            3.75 Hz, unmoved by any pulse at any ratio

The reason is in the same measurement: **719 firings per step**, a medium
saturated with its own waves. An imposed wave has no rest tissue to travel
through, so the object cannot be led. Discarded.

---

## 2 · The mechanism: pulse-coupled counting

A lattice of integrate-and-fire units. Each one **counts**: a phase rises at its
own natural rate, and on reaching the top the unit *fires*, resets, and shoves
each of its neighbours' phases forward by a fixed amount.

A shove can carry a neighbour over its own top. That neighbour fires in the same
instant, shoving its neighbours in turn, and firings **cascade**. A cascade is a
percussive event, and its length is not designed by anybody — measured:

| coupling | largest cascade | cascades/s |
|---|---|---|
| 0.00 (uncoupled) | 35 | 1200 |
| 0.02 | 38 | 1200 |
| 0.06 | **87** | 1200 |
| 0.15 | **1098** | 705 |

At weak coupling the object ticks; at strong coupling it convulses. Between
them is where it makes phrases.

### Why this and not something else

Because entrainment is what the mathematics of this object *is about*. Mirollo
and Strogatz proved that pulse-coupled units with a concave rise synchronise —
so leaning towards an external pulse is not a feature to be engineered here, it
is the theorem. What has to be engineered is the **opposite**: enough spread in
the natural rates that the object keeps several timings at once and only *leans*
towards the imposed one rather than collapsing onto it.

That is exactly the brief. "It seems to ease towards synchronising, but there
also seem to be several layers following different timings."

### And it does ease

Phase concentration *R* is the measurement: for every firing, its phase against
the imposed beat, averaged as unit vectors. R = 0 is indifference; R = 1 is a
drum machine. Measured over eight seconds, first third against last third:

| imposed pulse | R, first third | R, last third | |
|---|---|---|---|
| 1 Hz | 0.094 | 0.087 | ignores it |
| 2 Hz | 0.085 | **0.158** | leans |
| 3 Hz | 0.128 | **0.439** | leans |
| 5 Hz | 0.096 | **0.153** | leans |
| 8 Hz | 0.280 | **0.684** | captured |

**The relationship builds over the take.** That is "eases", measured, and it is
partial at almost every rate — the object leans towards your clock and keeps
its own business at the same time.

How hard the pulse shoves is a usable control, and it saturates rather than
running away: R = 0.106, 0.297, 0.439, 0.455 for shoves of 0.05, 0.15, 0.35, 0.70.

### Cost

**0.6 % of one core** for 9216 units. The earlier excitable medium was rewritten
event-driven for the same reason and this inherits it: a unit only has to be
looked at when its phase crosses, and a shove only touches eight neighbours.

---

## 3 · How it makes sound

A firing is a moment. The waveform is the list of moments. What turns that from
a click into a *timbre* is where the moment falls **inside** the step:

> a unit's offset within the step is its position along a projection of the four
> axes.

So a cascade sweeping across the lattice lays its firings out in time in the
order they stand in space, and the ear hears the cascade's **geometry** — its
extent, its direction, its raggedness — as pitch and colour. Turn the projection
and the sound changes, with no oscillator parameter existing to be turned.

Sign comes from the parity of a unit's coordinates, so a smooth cascade partly
cancels and a ragged one does not. Amplitude comes from the unseen axis, pan
from the visible one. Those four numbers are the only route from the four
dimensions to the ear, and they are enough.

---

## 4 · The fiction

**Accession B2311.1 — the lowest number in the field record, catalogued last.**

It was picked up in the first week, from scree on the approach to Kell Rille,
and logged as a nodule. It sat in a crate for two years.

What made anyone look again was the survey's own clocks. Two independent
timing units stored in the same shelter, which should have drifted apart at
their own rates, did not: over eleven weeks they stayed together to within a
figure that has no innocent explanation. The object had been counting the whole
time, and it had been counting *at* them.

That is also the physics, and the survey noticed the coincidence before it
noticed anything else: whatever else this thing is, it pulls other clocks
towards its own. Nothing was established about why. The report keeps the
question open in the register it keeps such things:

- whether the pulses are its function or a by-product of some other function;
- whether it counts something outside itself or only its own state;
- whether being joinable — a rhythm another body can fall into — is the point,
  or an accident of how it is built;
- and whether art, instrument, machine, message and clock are four possibilities
  or one thing the survey has four words for.

The frame carries a reading in kelvin, as B2311.67 does. Cold, it is silent.

---

## 5 · As built (260902.1)

Shipped the day it was designed, and published to the collection alongside its
two siblings.

### What the bench holds

| claim | measured |
|---|---|
| cold, it does not count | 77 K peak **0.000000**, the loop skipped entirely |
| warming makes it count faster | 7226 → 13640 → 28826 → **69360** firings/s |
| conduction sizes the events | largest cascade **138 → 835** |
| a note transposes the counting | 6044 → 11546 → **22252** firings/s, 2× an octave |
| it eases towards a pulse | R builds across a take at most tempi |
| it can be led, not shoved | gentle **0.705**, violent **0.502** |
| it keeps its own time | 11053 firings/s with the transport stopped |
| bounded | worst peak **0.9368** hot and flat out |
| 256 specimens | all distinct; each plays identically twice |

13 checks, ALL CLEAR.

### Texture or pulse — the regime that made it an instrument

At the first defaults it fired eleven thousand times a second, which the thesis
says is a *tone* and the brief did not ask for. A pulse needs silence around it.
Conduction turned out to be the switch:

    0.30    2–11 % of steps silent     a texture
    0.60    70–75 % silent             beginning to clump
    0.80    91–97 % silent             RHYTHM, whole-lattice cascades

The shipped renders sit at 99.5 % silent with six distinct recurring intervals.

### The one that was not a bug

The bench asserted that a harder grip gathers more of the object onto the beat.
It does not: R peaks at a grip of 0.20 and has fallen to 0.158 by 0.55. Once the
lattice has organised itself, a gentle nudge aligns the clusters and a violent
shove scatters them.

Which is the brief's own word, *eases*, turning out to be the physics — it can
be led, and it cannot be forced. The test now asserts that shape. Leaving it
failing for a commit rather than weakening it is what kept the finding.

### Faults the panel found in its own author

- **The count was invented.** The page added `lastCascade` every frame, and that
  value is never cleared, so it counted one event thirty times a second and
  displayed a briskly rising number for an object that might have been asleep.
- **The flares were invisible.** Heat decayed in 30 ms; the panel samples every
  33. Nearly every firing fell between two frames — the object sounded and the
  picture stayed still.
- **The meter never moved**, because it averaged a block and this object is
  silent for ninety-nine per cent of its steps. Peak, held.
- **"Send the rate field when it changes"** sent it once before the page was
  listening and then never again. B2311.67 learned the same lesson about its
  initial state; this is the second time the shape has cost a round.

### The layers, made visible

A unit's hue is its natural rate. Units near each other in rate run together, so
the banding across the face *is* the object's own organisation — the brief's
"several layers correlated by the object" made lookable-at rather than left as
structure nobody could see.

### Published

`vst3-apps/proxima-centauri-b/` — the page is three objects now, not two with a
block appended: every "neither", "both" and "2 of 2" was changed by exact match.
Five passages, each figure quoted one the bench or renderer printed. The archive
is verified from the inside — opened after writing, the plug-in loaded out of
it, build id read from those bytes.

### Left open, honestly

- **No user patch folder.** `check-names.js` reports it. The 256 specimens are
  the preset system; patch *files* are not written.
- **Decals not yet made.** Brief for Peter at `assets/ab1-decals/BRIEF.md`; the
  dial face is the one that would change the panel most.
- **The section-is-timbre test measures the wrong thing in this regime.** With
  the object now firing in rare bursts, its crossing-rate probe reads event rate
  rather than colour. It passes, and it is not measuring what its name says.
