# Artefact B2311.1 — bug list & idea collection (batch builds)

Peter's standing workflow, same as the Clone Wars and BWFX lists: fixes,
improvements and feature ideas collect HERE instead of forcing a build each.
When Peter says go, the open items ship in one pass. Add findings under an item
while investigating; move shipped items to DONE with the build id that carried
them.

Source tree: `C:\Users\peter\b\ArtefactB2311_1` (own git repo, local only).
Design: `ARTEFACT-B2311-1-DESIGN.md`. Current build **260902.3**.

---

## Open

*(nothing open)*

---

## Done


### 1. Variety in event LENGTH and SPECTRAL WEIGHT — SHIPPED in 260902.2

> "i like it, but id also like the sounds it generated to have more variety in
> terms of lengths and spectral weight… Can you do that without just pasting
> basic oscillators on? Because thats what i am trying to avoid."

**Yes — and none of what follows is an oscillator.** Every proposal below is a
change to *damping*, to *timing*, or to *routing*. The waveform stays a point
process; what gains structure is the medium the points excite. That is the
distinction that matters: the object still makes sound by things that *happen*,
not by things that vibrate.

#### The diagnosis, measured

Probe kept at `test/colour.cpp` (target `ab1colour`; measures the kernel's
impulse response directly, and takes the RMS frequency of rendered audio in
43 ms windows — exact second moment of the power spectrum, no FFT).

**There is one body in this instrument, and 9216 units share it.** Every firing
anywhere in the lattice is a single-sample spike put through the same pair of
one-poles in `Engine::process` (`tail.z1/z2`, coefficients `a1` from AFTERSOUND
and `a2` from ABSORPTION) and through nothing else. So:

| | measured |
|---|---|
| event length at the defaults | **0.375 ms** to −20 dB, **1.19 ms** to −60 dB |
| longest event the instrument can reach, anywhere in its parameter space | **9.5 ms** to −60 dB — and only at the darkest setting it has (395 Hz) |
| colour gamut of those two knobs | 395 Hz … 19 327 Hz |
| **how much of that varies event to event within a take** | **none of it. It is one setting per patch.** |

So nothing this object makes has a body longer than about ten milliseconds,
ever, and inside a take every one of the thousands of events per second is the
same length and the same colour. All the length you hear is *density of clicks*;
all the dynamics are density too — measured over a 20 s take at the defaults,
loudness moves with a CV of **110 %** while colour moves **27 %**, and that 27 %
is a side effect of how the clicks are *spaced*, not of any click differing from
any other.

Across the catalogue the picture is better but the same in kind — eight
specimens sampled, colour centre from 3 760 to 10 840 Hz — so specimens do
differ from each other, while **within** a specimen the colour wanders only
14 % on average. The variety is between takes, not inside one.

The lattice, meanwhile, is doing plenty: cascade size ranges from 303 to 9216
across those same eight specimens, a 30× spread that currently changes only how
*loud* a moment is.

#### What the object already computes and then throws away

Three sources of variety exist in the engine, are already paid for, and never
reach the ear:

1. **A unit's own natural rate.** `rate[i]` spans 0.25–40 Hz as a *smooth field*
   across the lattice, so the body falls into regions — the hue layers already
   drawn on the panel. When a firing becomes sound, the engine consults the
   unit's depth (amplitude), its x (pan) and its coordinate parity (sign). **It
   never consults its rate.**
2. **Cascade size**, measured above at 30×. Used for loudness only.
3. **Cascade extent** — whether the firings stayed in one corner or swept the
   lattice. Not computed at all, though the coordinates are in hand.

#### Proposal, in the order I would build it

**(a) The body stops being one filter — a bank, indexed by the rate field.**
Replace the single `tail` with B ≈ 8 one-pole pairs. A firing is deposited into
the band its *own* unit belongs to, chosen by rank in the rate field. Slow
regions get a lower, longer-decaying body; fast regions a higher, shorter one —
which is what any real body does, high modes damping first. Consequences: a
cascade that crosses regions opens bright and settles dark, so an event acquires
spectral *evolution* rather than a fixed colour; and because the bands are the
regions already visible as hue on the panel, **you see which layer made the
sound**. Cost is ~8× the filter work on a 0.6 %-of-a-core instrument.

*The compatibility rule:* AFTERSOUND and ABSORPTION keep their present meaning
as the **centre** of the spread, and a new control — **STRATIFICATION** — sets
how far apart the bands sit. **At 0 the bank collapses to identical bands and
the output is exactly today's sound**, so all 256 specimens, both siblings'
demo takes and any saved patch are untouched. Kemper rule, as everywhere else
in the fleet.

*The constraint that keeps it honest:* the bands stay **one-poles** — broad, Q
below 1. Spread their centres and let the differing *decays* do the work. Raise
the Q and spread the centres and it stops being a body and becomes a bank of
tuned bars struck by the lattice, which is precisely the "pasted-on oscillator"
Peter is refusing. Written down here so it does not drift.

**(b) Length as an output of the lattice, not a knob.** Each band tracks its own
energy; the damping pole is scaled by it, so a hard-driven body rings longer
than a tapped one. A tick comes out dry, a convulsion rings. This is the Full
Metal Racket nonlinear resonator, and it puts event *length* where the design
already puts event *rhythm*: nobody chose it. (Sign and depth to be set by
measurement, not by taste — a first calibration that measures as "real but
inaudible" is worthless, per the FMR PUNCH lesson.)

**(c) Cascade extent drives the spread.** `spanx` is currently one global number
applied to every event. Track the fired set's bounding box in the four axes —
one extra pass over `firedScratch`, no new state — and scale the spread by it. A
local cascade lands tight and reads as a bright transient; a lattice-wide sweep
smears and reads dark. This is the stated thesis ("the ear hears the cascade's
geometry") finally including the cascade's *size*, where today only its *order*
is audible.

**(d) Fatigue.** Store each unit's last firing step; a unit that fires soon after
recovering contributes less and duller. Repeated hammering in one region dulls
and quietens, then recovers. Cheap (one int per unit), lowest priority of the
four.

**Recommended batch:** (a) + (c) together — they are the two that produce
audible variety in both axes Peter named, and (c) is nearly free once (a) is in.
(b) next, on its own, because it needs its own calibration pass. (d) last, or
never.

#### Also worth knowing before building

The two existing knobs are **not** two independent dimensions. Decay is set
almost entirely by ABSORPTION's pole, which also dominates the colour, so the
reachable region is a narrow band rather than a rectangle: longer is nearly
always darker. The bank in (a) is what makes length and colour separable at all.

---

#### AS BUILT (260902.2) — what changed against the proposal above

All three shipped, plus a bound the long mode turned out to need. `test/colour.cpp`
(target `ab1colour`) holds every number; the bench gained section 11 and is at
**20 checks, ALL CLEAR**.

**(a) STRATIFICATION — the bank, indexed by the rate field.** Eight one-pole
pairs; a unit's band comes from its own place in the rate field, computed in
`rebuildRates` beside the field itself. AFTERSOUND and ABSORPTION are the
CENTRE, STRATIFICATION the spread in octaves. Both poles of a band move
together, which time-scales its kernel rather than reshaping it, so the bands
are one body at eight sizes and not eight filters — and because that makes the
fast bands louder, the compensation is **measured** (impulse through the kernel,
peak read back) rather than modelled.

| STRATIFICATION | event length across the bank | colour across the bank |
|---|---|---|
| 0.00 | 1.188 … 1.188 ms | 3 755 … 3 755 Hz |
| 0.50 | 0.625 … 2.188 ms | 1 831 … 8 738 Hz |
| 1.00 | 0.312 … 3.979 ms | 955 … 19 943 Hz |

**(b) PERSISTENCE — and the first mechanism was WRONG.** The proposal said
"scale the damping pole by the band's energy", which is the obvious move and
does not work: measured, shrinking the slow pole moves the −20 dB point from
0.375 ms to **0.125 ms**. It lengthens the tail and lowers it faster than it
lengthens it, so the audible event gets SHORTER — the first build measured a
big/small length ratio of 0.77–0.98×, i.e. nothing. Scaling *both* poles does
lengthen it but costs 20 dB on exactly the loudest events, which is a
compressor. What shipped is a **parallel long mode**: the same body at a tenth
the speed, fed in proportion to how hard the band is being driven. A real body
has one, and only a hard strike reaches it.

Then the *drive measure* was wrong too: `env/slow` where `slow` followed the raw
drive pinned at its cap for every event, because in an object silent between
events every event is enormous against the average SAMPLE. Following the
envelope instead measures the average EVENT, which is the comparison wanted.

| PERSISTENCE | small events | big events | ratio |
|---|---|---|---|
| 0.00 | 0.40 ms | 0.63 ms | 1.59× |
| 0.45 | 0.31 ms | 0.81 ms | 2.61× |
| 0.85 | 0.29 ms | 1.09 ms | **3.78×** |

Events sorted by the energy of their first 3 ms — the cascade arriving, before
the long mode has said anything. **Sorting by PEAK is wrong** and cost a round:
TRAVERSE spreads a large cascade flatter, so a big event can have a lower peak
than a small tight one, and splitting on the peak sorts the events by the
opposite of what was meant.

**(c) TRAVERSE — the cascade's own reach sets its spread.** Bounding box of the
fired set over the four axes, computed in `advanceStep` where the list is
already in hand. At 0 the multiplier is exactly 1 and EXTENSION means what it
always meant.

**(d) Fatigue: not built**, as planned.

#### The bound the long mode needed

Section 11 caught a worst peak of **1.29**. The cause was pre-existing: the soft
ceiling asymptotes to `ceil/0.55`, which is **1.67** at a wide-open CEILING — a
control named for the level the object will not exceed, which could be exceeded
by two thirds. Nothing had ever driven it that hard; the long mode does.

The first fix raised the ceiling's own knee coefficient and **changed the whole
take by about a per cent** — a residual 40 dB down where there should have been
nothing. Any smooth saturator that binds at full scale also bends the quiet
signal. What shipped is a hard knee: identity below 0.85, tanh above, asymptotic
to 1.0. Exactly transparent for everything the object used to make. Worst peak
with all three flat out and hot is now **0.9992**.

#### Neutrality, cost, and what moved

- **All three at 0 is the 260902.1 instrument**, verified against the real thing
  — `git show HEAD:Source/Engine.*` compiled in beside the new engine under its
  own namespace, in `test/legacy/`. Worst sample difference **1.7e−6** on a peak
  of 0.662, residual **−119.3 dB**, and uniform across every level band, so it
  is float summation order and nothing else. Comparing the new engine with
  itself would have proved only that it is deterministic; that was the first
  version of this check and it was worthless.
- **Cost: 0.89 % → 1.20 % of one core.** The first measurement said 10.7 % — the
  bench does not flush denormals and the plug-in does (`ScopedNoDenormals`), and
  a bank of one-poles decaying to zero lives in the denormal range. Timing it
  without FTZ measures a machine the instrument never runs on.
- **The catalogue gained the three controls by APPENDED draws** in
  `applySpecimen`, so every earlier draw is bit-identical: specimen 90's rates,
  coupling, projection and dead time are exactly what they were, and a number is
  still a specimen for good. What it gained is a body with layers.
- **Colour movement within a take, across the catalogue: 14.1 % → 20.3 % mean**,
  and at the defaults **27.0 % → 69.8 %**.
- The five demo takes were re-rendered; the panel gained the three controls in
  THE BODY; installed to both houses and loading.

#### The bench check that had to change, and why it is not a loosened test

Check 4 ("it eases towards an imposed pulse") failed at the new defaults. The
counting had not changed at all — R is taken from the AUDIO envelope against
the beat, deliberately, and a tail that can now run to forty milliseconds lays
energy across every phase of the beat whatever the lattice is doing. **What was
being measured was the tail.** Checks 4 and 5 now run with the three body
controls shut, where the audio is the 260902.1 audio to −119 dB — and they
return the 260902.1 numbers to three decimal places (0.382/0.658, 0.445/0.450,
0.508/0.365, 0.406/0.272), which is itself the proof that entrainment is
untouched.

---

### 2. "They sound great but all noises are very short" — SHIPPED in 260902.3

Peter, 2026-09-02, on 260902.2; then *"still? Is there some way to give them
weight, some new control?"*, then *"it is unrealistically that the patterns stop
dead at 77 K… they should just be less active"*, then *"dragging or marking
areas should change these tiny clocks behavior permanently, or at least a
while"*, and *"you should redo at least half of the patches to demonstrate these
new patterns"*.

**He was right, and item 1 had under-delivered.** 260902.2 improved the length
RATIO between big and small events by 2.4× while the longest tail anywhere in the
instrument was still **47.8 ms**. A ratio cannot tell you that, and I had only
measured ratios. *A feature that measures as working can still be worthless —
check the magnitude, not the sign.* That is the FMR PUNCH lesson, already in
CLAUDE.md, and I walked straight into it.

#### ENCLOSURE was a dead control

Declared in SPECS, glossed "the room the crate makes around it", drawn on the
panel, randomised into all 256 specimens — and **never read by the engine**.
`p.space` appeared exactly twice in `Engine.cpp`: the parameter table and
`applySpecimen`. It is now a four-line feedback delay network, Householder mixed,
damped in the loop so the tail darkens as a real box does.

| ENCLOSURE | tail after the counting stops |
|---|---|
| 0.30 | 43.7 ms |
| 0.50 | 106.9 ms |
| 0.70 | 298.0 ms |
| 0.90 | **469.0 ms** |

#### WEIGHT — the new control, and four calibrations to get there

The object had **4.5 %** of its energy below 250 Hz and a spectral centre at
8.6 kHz. Nothing that thin sounds heavy however long it rings. WEIGHT is the
object's own low body: three resonators at 1, 1.47 and 2.09 — deliberately not a
harmonic series, so it arrives as a struck shell and not a note — pitched from
ABSORPTION and **not** from the played note, struck only in proportion to how
hard the object is being hit.

Every pass was measured, not judged:

1. **Peak-levelled against one band kernel** → 73.6 % of all energy below 250 Hz,
   centre dragged to 213 Hz. A Q-14 resonator rings 250× longer than a band, so
   equal peaks are wildly unequal energy.
2. **Energy-levelled against one band** → still 77.6 %. The low body hears the
   drive of all eight bands; each band hears only its own share.
3. **Energy-levelled against the whole bank** → 72.7 %, which fixed the coupling
   constant and made the last step arithmetic instead of guesswork.
4. **Struck by the SIGNED train** (parity cancellation) → WEIGHT did nothing at
   all, 4.5 % against 4.6 %. Over-corrected, and instructive: the parity rule
   governs what the object **radiates**, not what it is **struck by**. A blow is
   compressive — nine thousand impacts push the same way whatever the parity of
   their coordinates.

Shipped on magnitude with the trim solved from (2). At WEIGHT 0.85: **53 %**
below 250 Hz with the centre still at 5.4 kHz.

#### The 77 K cliff

Two cliffs stacked. The rate law `0.25 + 3.2w²` still counted at a **quarter
speed** at 77.1 K, and a hard `return` muted the object outright at 77.0 — busy
to silent across a tenth of a kelvin.

**The first fix was too clever and broke the instrument.** Replacing the law with
a smooth `3.45 w^1.55`, matched at 800 K and at room to within one per cent,
inverted BOTH entrainment checks — R fell across a take where it had risen. At
the default CONDUCTION the lattice sits near global synchrony and is exquisitely
sensitive to how its own rate stands against the imposed pulse. **One per cent
was too much.** What shipped keeps the old law exactly above 99 K and fades the
last twenty-two kelvin to zero; the same gate holds the imposed pulse, or a
frozen object would still be shoved into firing.

| kelvin | whole-body events in 20 s |
|---|---|
| 80 | 1.1 |
| 90 | 3.2 |
| 100 | 5.2 |
| 120 | 6.0 |
| 190 | 7.0 |

Worst single step 3.0×, and at 77 K still **exactly** silent. The early return is
gone, so cooling no longer cuts the body off mid-ring — which is also the only
reason the crate's tail could be measured at all.

#### MEMORY — a touch is remembered

A touch used to shove a phase and be over with. Now it MARKS the units it lands
on; they count faster while the mark lasts, and MEMORY sets the fade from about
two seconds to permanent. The panel tints marked cells green (sped up) or violet
(slowed), fading with the mark, so the trace is watchable.

**The first version was wired to nothing that mattered.** A mark changed only how
fast a unit counted — and at any real CONDUCTION the coupling pins the collective
rate and swallows it: a second after a drag the object differed by **1 %** and
MEMORY made no difference at all. The comment in the code already claimed a mark
"moves it into another layer… it speaks through a different body", and the code
did not do that, because `bandOf` is built from the NATURAL rate. The deposit now
shifts the band by the mark. A second after the touch ends the object sounds
**35.3 %** different in colour.

*Honest limit:* eight seconds on, a short MEMORY still differs from untouched by
11.5 % against a long memory's 14.2 %. Most of that residue is not memory — a
touch perturbs a near-chaotic lattice and it diverges whether the mark survives
or not. The check asserts the ordering and no more.

#### Half the catalogue redrawn

The old generator makes BUSY objects, several thousand firings a second, and at
that density events overlap so a long tail and a heavy blow are heard as texture
rather than as events. Everything built in .2 and .3 was therefore nearly
inaudible across the catalogue — not a fault in the mechanisms but in what the
specimens asked of them. **Odd-numbered specimens are now drawn sparse and
resonant**: slow counts so events stand apart, enough conduction for real
cascades, and a body given time and room. Split by the specimen NUMBER rather
than a draw, so it is legible from the dial; even numbers are the objects as they
were.

| spec | firings/s | energy <250 Hz | colour |
|---|---|---|---|
| 8 | 41 480 | 9.6 % | 5 960 Hz |
| **9** | 2 388 | **64.6 %** | **1 058 Hz** |
| 40 | 73 507 | 22.6 % | 7 718 Hz |
| **41** | 7 128 | **61.9 %** | **2 550 Hz** |
| 200 | 4 613 | 9.1 % | 7 382 Hz |
| **201** | 4 626 | **68.2 %** | **2 406 Hz** |

#### The bench lesson, for the third time

Check 4 failed again this round and **the counting had not moved a sample** —
because it neutralises the body controls and WEIGHT and ENCLOSURE had just joined
the body. R is read from the audio envelope against the beat, so anything that
lengthens an event lowers it. The rule is now stated once in `bench.cpp`: *if it
makes sound after the firing, it is shut off for the entrainment tests.* With
that, check 4 returns 0.382/0.658, 0.445/0.450, 0.508/0.365, 0.406/0.272 — the
260902.1 numbers to three decimals.

Section 12 then failed on 3072 firings/s, flat across four temperatures. 3072 is
exactly NUNIT⁄3 over a 3 s take: cold, the lattice fires as ONE body, and three
seconds cannot resolve anything finer than "it went off once". **The flatness was
the take length, not the object.**

**26 bench checks ALL CLEAR.** New panel probe `test/uiprobe.js` cross-checks
every engine parameter against the panel (and knows TEMPERATURE has its own
thermometer) — the guard against the working-but-invisible class. Six demo
passages re-rendered, a new one (*The Weight of It*, sparse specimen 41) added to
the landing page as OBS-17, the page's "at 77 K the count stops" claim corrected,
zip re-cut and verified byte-identical to the build, installed to both houses.
