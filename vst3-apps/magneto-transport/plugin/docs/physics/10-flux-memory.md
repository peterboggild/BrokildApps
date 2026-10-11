# 10 — FLUX MEMORY and the LOOP panel (stage 7)

The brief's own idea: a variable-strength erase head on a finite physical tape
loop. With the erase head turned down, earlier recordings survive as remanent
magnetisation, come back round every loop period, and are recorded over through
the real hysteresis model. The loop's record path is also a performance control,
REC | DUB | PLAY (BUGLIST 2 and 7). Nothing in either is a buffer: the loop is the
tape, and every action does what the machine would.

Source: the stage 7 parts of `Source/Engine.cpp` (`buildErase`, `eraseSpan`, the
FLUX MEMORY block in `process`, the record relay in `finishCells`), `Source/Transport.cpp`
(the coast-down), and `reference/mtref.cpp` (the tables' old states). Bench
section 20.

## The erase head

The erase head is the record process with no signal: a 60 µm gap, its AC field
decaying as the tape leaves it. Each layer sees the field at its depth
(Karlqvist, (2/π) atan(g / 2y): 0.96 of the deep-gap field at the top slab, 0.75
at the bottom). What a cell keeps is the reference's G at s = 0 and that
amplitude, a function of what it had, tabulated per layer for the current field
and speed. Past 2.2 Hc the transition is over (the reference: −131 dB at 2 Hc)
and the head simply wipes the tape. At zero it lets the tape pass untouched.

ERASE moves the field, and its taper follows what the field does (BUGLIST 7: the
first, linear knob put the whole audible transition between 40 and 62 % of its
travel):

- 0 % switches the head off;
- 0–90 % walks the reference's one-pass erasure (the erase head alone over a 0 VU
  1 kHz recording at 19.05 cm/s) geometrically from 0.6 dB to 60 dB lost a pass,
  i.e. from a hundred passes to one: 0.92 Hc at 10 %, 1.10 at 45 % (6 dB a pass),
  1.57 at 90 %;
- 90–100 % takes the field on to 2.25 Hc, a clean wipe.

The real-time erasure is gentler than the reference in the middle of the
transition (one pass at 90 % loses 42 dB against the reference's 60), so on the
machine the knob runs 3 / 8 / 22 / 42 dB a pass at 25 / 50 / 75 / 90 %. That gap
is part of the overdub work (BUGLIST 8), which will also re-derive the taper if
the reference's transition moves.

## What the reference settled (2026-10-11, `mtref --dub`; 05-reference-results.md §5b)

Three facts, each measured rather than argued, decide what the loop can do:

1. **A field takes what it can reach and no more.** A second identical pass takes
   nothing further: the erase head at 1.25 H_c leaves −18.28 dB after one, two and
   three passes; the bias alone −34.50 after one and two. This is the Preisach
   wiping-out property: the survivors of a sweep are exactly the hysterons the sweep
   cannot reach, and the same sweep cannot reach them next time either. **Partial
   erasure and bias erasure are one shot, not a fade.** The first real-time engine
   applied its retention on every pass (one number per cell has no memory of which
   hysterons hold it) and so faded a layer by 35 dB where the physics takes 3.5 once.
2. **A take recorded at the operating bias survives passes at a LOWER bias,
   permanently.** Under bias-only passes at r × the operating bias: r 0.8 −11.8 dB,
   0.7 −8.2, 0.6 −5.6, **0.5 −3.5**, 0.4 −1.7, 0.35 −1.0 — once. The lower bias reaches
   the surface layers only, so the treble (which lives there) goes first: through
   the playback head, 200 Hz −3.6, 1 kHz −3.9, 5 kHz −6.2, 10 kHz −9.7 dB.
3. **What is recorded at a bias is replaced by the next pass at that bias.** A
   recording made at r 0.5 and swept at r 0.5 keeps −24.1 dB at 0 VU, −18.5 at +6,
   −12.8 at +12 dB (the fringe survivor grows with the signal's share of the bias).
   At the operating bias, −34.5 / −28.7 / −22.7. On one tape track, overdubs do not
   stack; sound-on-sound was always "the take stays, the overdub is temporary", and
   real machines got there by dropping the bias.

The mean-field (moving-Preisach) term the design listed as a candidate was examined
and dropped: during a sweep the hysterons being cycled average to zero and only the
frozen fringe can seed a re-recording, so the term rescales the fringe survivor by a
factor of order one and creates no survival of its own. The survival that exists is
geometric — depth and reach — and the classical model has it.

## DUB is sound-on-sound

In DUB the bias drops 6 dB (`DUB_BIAS_DB`, r 0.5: the take keeps −3.5 dB, the
overdub records 2.9 dB low with HD3 0.82 % against 0.35, 5 dB brighter at 10 kHz —
the sound-on-sound trade) and the erase head sits at the ERASE knob. The BIAS knob
still adds to it: at BIAS −3 in DUB (r 0.35) the take keeps −1.0 dB and an overdub's
ghost under the next pass is −12.6 dB, the Echoplex sound-on-sound character; at
BIAS +6 it is the operating bias again and the take is wiped as in REC.

## The real-time bookkeeping: one shot

Per cell and slab the tape store carries three things besides the remanence m that
the heads read (`TapeStore`: 2.25 × the memory of m alone, 67 MB per metre
of stereo loop):

- **md**, the committed part: what survives beyond the reach the heads have paid for;
- **rp**, the retention paid on it, a quarter-dB index (255 = gone);
- **ta**, the bias reach (an A-grid index) the material on top, m − md, was recorded at.

A pass (the erase head at h_e then the bias at slab amplitude A, no signal) is one
table lookup per slab, `R_DUB[v][l][h_e][A][M]`, generated by `mtref
--dubtables` from the record tables' own prep (a take at the operating bias): the
ratio it leaves. The committed part is cut only by what exceeds the retention already
paid, md ← md · R(k)/R(rp), rp ← k, and never cut again at that reach. Material on
top recorded at a higher reach than this pass is committed material now (the same
table); recorded at this reach it is replaced, its fringe kept as a ghost from a
second table, `R_TOP[v][l][h_e][A][level]`, whose prep is a DC level at that
very bias. Both survivors join md, having paid this pass. Then the fresh recording
G(A, 0, s) is written on top. In REC (erase full) every cell is wiped at the erase
head and the record step is the stage 3 arithmetic unchanged, bit for bit.

Two tables because an overwrite depends on how deep the old recording went
(05-reference-results §6): a take made at the operating bias survives a half-bias
sweep at −3.5 dB; a recording made at half bias survives the same sweep at −24 dB.

The erase head's partial field acts at the record head, through the pair table,
with the bias: the two are not separable (h_e 1.0 alone −3.0 dB, the bias at r 0.5
alone −3.5, together −4.6). Full is a wipe at the head, as before. The first
real-time erase head read the record head's G at s = 0 with the record head's
depth profile; a 60 µm gap's profile is nearly uniform through the coating, so its
transition read 16 dB too gentle at 1.5 H_c. The pair table is generated with the
erase head's own profile.

Interpolation across speed, h_e and A is in dB: the transition is 15 dB per grid
step of h_e, and linear in the ratio between points would be far too gentle. The
retention paid is recorded at the level the next pass will read it at (the ratio
moves with level) and never lowered, so a pass below what was paid takes nothing and
a return to the higher reach does not take it twice.

Measured against the reference at the alignment (bench section 20): the take −3.43 dB
once (−3.47), identical on passes 2 and 3; h_e 1.0 / 1.25 / 1.5 with the bias −4.6 /
−18.5 / −51.6 (−4.6 / −18.5 / −51.5); the erase head alone −0.1 / −3.1 / −18.7 /
−51.9 (−0.1 / −3.0 / −18.3 / −51.5); the overdub's ghost −25.0 at 0 VU, −13.2 at
+12 dB (−24.1 / −12.8); DUB records −2.95 dB (−2.9); by frequency through the
playback head 200 Hz −3.5, 1 kHz −3.9, 5 kHz −6.0, 10 kHz −9.1 (−3.6 / −3.9 / −6.2 /
−9.7). Known deviation: a tone's ghost under a pass at the operating bias reads
about 3 dB under the reference, because the overdub table's levels are DC and a
tone's cells sit below its peak where the ratio is lower; and the tables are bias
only, so a hot new signal widening the reach (the reference's −44.6 dB for a 0 VU
tone recorded over) is not reproduced — the ghost stays at the bias-only −35.

## Bias erasure, and why it governs everything

Recording over old material with the bias on erases it as well. This is the same
record process with the old state as the starting staircase, so in real time it
is G's dependence on the old remanence. At the machine's own alignment (19.05 cm/s,
the operating bias) the reference measures **−34.5 dB per pass**, against −4.1 dB at half
bias (at its 38.1 cm/s alignment, −42.1 and −4.9). **BIAS is therefore the overdub's
depth control:**

- at the operating bias, an old layer is about 35 dB down after one pass;
- at half bias (−6 dB on BIAS), it loses only about 4 dB a pass, and lives for
  many passes, at the cost of the distortion and treble that under-bias brings;
- the erase head adds to that, it never subtracts from it.

The control that makes a take last is the one a technician would reach for: the
bias. DUB turns it down for you.

## The tables had to know what an old recording is

The first real-time bias erasure read −25 dB against the reference's −42, and
the erase head's transition was 13 dB too gentle. The reference's tables had
prepared every old state as an ideal anhysteretic recording, set by a 5 Hc
decaying field, identical at every depth. That magnetises the hard particles
too, which no later bias pass reaches.

A real recording at the operating bias lives only in the particles that bias
could move: in the deep sub-depths, where the bias is weaker, those are fewer.
The next pass at the same bias then erases nearly all of it.

The tables are now generated with the old states the machine itself leaves:

- a DC level recorded by the record head at the operating bias and the table's
  own speed;
- per sub-depth and per bias phase;
- the level bisected for the slab's mean remanence.

Fresh-tape recording (old remanence 0) is untouched by this, so every stage 3–6
figure stands.

**This assumes an old recording was made at the operating bias**, which is the
machine's normal state. Material recorded at another bias (BIAS turned) is
overwritten as if it had been recorded at the operating bias: an approximation,
stated.

## PLAY

Record and bias off, erase off. The loop plays what is on it for as long as you
like; it changes only by wear, and only if ACCUMULATE is on. The record relay
switches bias and signal over about 5 ms, so a cell under the head at that
moment is blended, not cut. The erase field ramps over about 20 ms, and when
ramped off it is exactly off: a hair of field left on would nudge every cell of
a frozen loop on every pass.

## The LOOP panel

TRANSPORT = REEL | LOOP. Everything that belongs to the loop is in one group:
LOOP LENGTH, LOOP BARS, ERASE, LOOP STATE and CLEAR. On a REEL the whole group does
nothing (fresh tape arrives and the erase head is always full), and the bench
checks that bit for bit. The panel dims what is inert, with one line saying why,
but leaves it adjustable so a loop can be set up before it is switched in.

| LOOP STATE | record + bias | erase head |
|---|---|---|
| REC (default) | on, at the set bias | full: fresh tape every pass — the plain echo |
| DUB | on, 6 dB under the set bias | at the ERASE knob: the committed take stays, what you play is replaced by the next pass |
| PLAY | off | off: the loop plays unchanged; FEEDBACK has nothing to write |
| CLEAR (momentary) | off | full, for exactly one pass of the loop |

ERASE acts in DUB and acts once: it takes from the committed take what the field can
reach — 0 nothing, 45 % about 6 dB, 90 % about 60 dB, 100 % everything — and a second
pass at the same setting takes nothing more. Turning it up takes more; turning it
down brings nothing back.

**LOOP BARS:** with SYNC on, the loop is cut to N bars of 4/4 at the tempo head's
note value. The length is N × 4 / (note in beats) × the tempo head's distance,
independent of the tempo: the motor's speed follows the tempo, so the loop stays
N bars when the tempo moves. It is latched when the bars, the note or the tempo
head change, never when a head is dragged, since a new length is a new loop (a
new tape). Out of the 0.5–6 m range, the panel says so. A tempo head dragged after
the cut changes the speed (SYNC keeps the head on the beat), so the loop stops being
N bars; the readout gives the bars it lasts now until BARS is set again.

**No undo.** Tape has none — and what tape also does not have is a stacking overdub
on one track. The tape way to accumulate is FEEDBACK: head 1 re-recording what it
plays, at the head's distance.

## The motor stops

MOTOR RUN off cuts the capstan motor's power. The capstan and its flywheel
coast down on bearing drag (viscous, 2 s) and the pinch roller's friction
(6 cm/s²):

    w(t) = (w0 + 12) e^(-t/2) - 12   (cm/s)

so a stop from 19 cm/s takes about 1.9 s, and from 76 cm/s about 4 s.
Everything on the tape dives in pitch with it.

A playback head is a differentiator and the replay amplifier integrates; its
integrator stops near DC, a first-order low cut at 5 Hz. So a stopped tape gives
no output, as on a real machine, instead of the flux standing still at the head.

## The loop's splice

A loop has a joint. The two ends butt, leaving a 20 µm sliver with no oxide,
and a strip of splicing tape on the back lifts the tape 2 µm from the head over
about ±0.4 mm. It passes each head once a loop period: a short dropout, the
tick of a real tape loop. It is a property of the tape, so it is absent on a
reel and on the bench's ideal medium.

## Not modelled, said plainly

- Self-demagnetisation and print-through in a frozen loop: the design dropped
  them (inaudible at loop timescales). Only wear changes a frozen loop.
- The splice's mechanical bump at the capstan (a tension transient): only its
  magnetic effect is modelled.
- Old material recorded at a bias other than the operating bias is overwritten
  as if it had been recorded at the operating bias.
