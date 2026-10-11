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

FLUX MEMORY's ghosts and DUB's layers fade by physics, as the brief asked. The
control that makes them last is the one a technician would reach for.

**Open (BUGLIST 8):** at the operating bias this means DUB at ERASE 0 loses a
layer in one pass (−31 to −45 dB from 60 Hz to 8 kHz), so it sounds like REC. Tape
users report that recording over a track with the erase head off leaves the old
material audible for several passes, with the bass surviving longest. The model
lacks the particles' interaction field (a moving-Preisach term) and the
wavelength-dependent demagnetisation that would let long wavelengths survive.

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
| REC | on | full: fresh tape every pass |
| DUB | on | at the ERASE knob (100 % is the plain echo) |
| PLAY | off | off: the loop plays unchanged; FEEDBACK has nothing to write |
| CLEAR (momentary) | off | full, for exactly one pass of the loop |

DUB is the loop's normal running state, so the default (DUB at ERASE 100 %) is the
plain tape-loop echo.

**LOOP BARS:** with SYNC on, the loop is cut to N bars of 4/4 at the tempo head's
note value. The length is N × 4 / (note in beats) × the tempo head's distance,
independent of the tempo: the motor's speed follows the tempo, so the loop stays
N bars when the tempo moves. It is latched when the bars, the note or the tempo
head change, never when a head is dragged, since a new length is a new loop (a
new tape). Out of the 0.5–6 m range, the panel says so. A tempo head dragged after
the cut changes the speed (SYNC keeps the head on the beat), so the loop stops being
N bars; the readout gives the bars it lasts now until BARS is set again.

**No undo.** Tape has none.

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
