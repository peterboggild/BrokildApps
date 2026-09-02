# Artefact B2311.1 — the voicing, measured

*Written 2026-09-02 from the music PC, after Peter listened to the shipped
object and said the sounds were "relatively uniformly tiny noise pops".*

That verdict is accepted and so is the diagnosis that went with it: the
instrument only ever occupies the rhythm end of its own thesis. This document
is the next step — the four proposed changes built as a standalone mechanism,
run, and measured, so that what goes into the plug-in goes in with numbers
attached rather than with hope.

**Everything below is a figure this prototype printed.** Nothing is quoted from
the shipped engine, which is not on this machine.

---

## 0 · What this is, and what it is not

`proto/b2311-1-voicing/counting.cpp` — one file, no dependencies, `g++ -O2`.

It is a **reconstruction of the mechanism from the design document**, not the
shipped code. The B2311.1 source tree lives at `C:\Users\peter\b\ArtefactB2311_1`
on the work PC and has no git remote (see `FLEET-CONTINUITY.md`), so it could
not be read. The reconstruction was calibrated until it reproduced the shipped
object's own published measurements — around 11 000 firings a second free
running, cascades from a single unit to all 9216, most of the take silent —
and the calibration lands at 14 879 firings/s, cascades to 9216, 56 % silent.
Close enough to argue from; not the same program.

So: **the mechanisms below are real results about this mechanism.** Whether the
shipped engine behaves identically is a thing to confirm when the tree is back,
not something this document can settle. Where a number would change a design
decision, it is worth re-measuring in the plug-in before trusting it.

The body is the same shape as the object's: 9216 units on a four-dimensional
lattice, 32 x 32 seen and 3 x 3 unseen, eight neighbours each, phases rising at
their own rates on a concave map, firing and shoving.

---

## 1 · The change that matters most, and it was not on the list

### Conduction has a speed, and that alone makes size and duration the same thing

The shipped object resolves a cascade inside a fraction of one lattice step —
about 1.1 ms — whether the cascade takes five units or all 9216. Give the shove
a travel time and a cascade that reaches *d* hops away takes *d* delays to do
it. Median duration, by how large the cascade was:

| conduction delay | 2–9 units | 10–99 | 100–999 | 1000+ |
|---|---|---|---|---|
| **0 (as shipped)** | **0.0 ms** | **0.0 ms** | **0.0 ms** | **0.0 ms** |
| 0.5 ms | 1.0 ms | 3.0 ms | 6.0 ms | 14.5 ms |
| 2 ms | 4.0 ms | 10.0 ms | 26.0 ms | 80.0 ms |
| 6 ms | 12.0 ms | 30.0 ms | 84.0 ms | **240.0 ms** |

The top row is the complaint, as a number: every event the same length, from
one unit to nine thousand. Every other row is a body in which a large event is
a *different sound* from a small one rather than a denser one — and nobody
chose those lengths either, which is the same claim the design document already
makes about cascade size.

The largest events run to about **40 conduction delays**, the very longest to
about **110**.

### And it introduced a law, found the hard way

The first run at 2 ms delay produced 1 998 320 firings a second and a single
"cascade" lasting the entire eight-second take. Conduction with a delay closes
a loop: a shove leaves, travels, and something comes back. If a unit is ready
to fire again before the return arrives, the body never stops.

The threshold was measured, at three conduction speeds spanning a factor of
four:

| refractory | delay 1 ms | delay 2 ms | delay 4 ms |
|---|---|---|---|
| 3.0 x delay | 492 597 firings/s | 491 431 | 426 696 |
| **3.1 x delay** | **23 605** | **27 768** | **28 300** |
| 3.2 x delay | 24 406 | 26 550 | 30 638 |

**3.05 ± 0.05 conduction delays**, in the same place at every speed — so it is a
property of the lattice's loops, not of a setting. The refractory is therefore
**not an independent control**: it has a floor, and the conduction speed sets
it. The prototype enforces this rather than letting a user find it.

---

## 2 · Does it reach pitch? Yes — and not the way the plan assumed

The plan was to open the rate range from 40 Hz into the audio range and let
individual units fire fast. **That is not what makes a tone.** Units firing
fast and independently make more hash, not a note: at a rate band of 480–1440 Hz
the body produces 7.9 million firings a second, cascades of one unit, and a
point-process spectrum standing 50 x above its own median — which is noise.

What makes a tone is the body **locking**. Mirollo–Strogatz is the reason: a
population of pulse-coupled units with a concave rise pulls together, and what
it pulls together *onto* is its fastest member. Measured as firings per second
against N x the top of the rate band:

| rate band | firings/s | ÷ 9216 | the counting's own spectrum |
|---|---|---|---|
| 0.02–1.5 Hz | 15 487 | 1.7 | 72 x median — aperiodic, a rhythm |
| 3–12 Hz | 110 592 | **12.0** | 1 782 x |
| 30–90 Hz | 829 440 | **90.0** | 180 907 x |
| 60–180 Hz | 1 658 880 | **180.0** | **1 264 887 x** |
| 120–360 Hz | 2 405 051 | 261.0 | 964 x — lock lost |
| 240–720 Hz | 3 970 374 | 430.9 | 45 x — noise |

Where it locks, it locks **exactly**: every unit in the body firing at the top
of its own rate band, to five figures. The object sounds a note, the note is
`ratehi`, and there is still no oscillator anywhere in it — which is the thesis,
demonstrated rather than asserted.

### Where the lock breaks, and why

At the refractory. A shove that arrives while a unit is refractory is discarded,
so once the note's period falls below the refractory *every* shove is discarded
and the body free-runs. Predicted ceiling: **1 / refractory**. Tested:

| refractory | predicted ceiling | 180 Hz | 360 Hz | 720 Hz | 1440 Hz |
|---|---|---|---|---|---|
| 4 ms | 250 Hz | locked | free | free | free |
| 2 ms | 500 Hz | locked | **locked** | free | free |
| 1 ms | 1000 Hz | locked | locked | **locked** | free |

Every cell as predicted, with no exceptions.

### Which gives the object its one real constraint

The refractory has a floor of 3.05 conduction delays, and the pitch ceiling is
one over the refractory. So:

> **longest event x highest note ≈ 13**

A body whose largest cascades run a quarter of a second tops out around 55 Hz.
A body that can sound 1.6 kHz has events of about 8 milliseconds. Both were
measured; neither is a limitation to be engineered away, because they are the
same number seen twice.

That is worth taking as a *feature*, not a compromise. It means **one control —
conduction speed — carries the object across all three of its scales**: slow
conduction gives long, low, sustaining events; fast conduction gives short,
high, tonal ones; and the middle is where a phrase has both. That is a better
realisation of "rhythm, pitch and colour are one quantity" than a rate range
would have been, and it comes out of the mechanism rather than being imposed on
it.

---

## 3 · The parity sign flip is the reason nothing is low

Suspected of cancelling the coherent structure that would give a cascade a
note. It does, and the measurement is unambiguous — but only once cascades take
time, which is why this had to be measured after change 1 and not before.

| regime | parity | crest | spectral centroid | raw peak |
|---|---|---|---|---|
| A. as shipped, no conduction | on | 21.9 | 1979 Hz | 36.0 |
| | off | 100.9 | 2467 Hz | 3221.3 |
| B. conduction 2 ms | on | 18.2 | 2083 Hz | 103.2 |
| | off | 16.5 | 2325 Hz | 117.3 |
| C. conduction 2 ms, kernel from 30 Hz | on | 18.3 | 1552 Hz | 75.4 |
| | off | 20.8 | 1303 Hz | 274.3 |
| D. locked, kernel from 30 Hz | on | 5.4 | **2552 Hz** | 52.4 |
| | off | 2.9 | **266 Hz** | 1208.3 |

Read row D. With the sign flip on, a body whose units are acting *together*
puts its energy at 2.5 kHz. With it off, the same body — same seed, same
firings, the point process bit-identical — puts it at 266 Hz, a factor of 9.6
lower, at 23 times the amplitude.

**The flip does not colour the sound. It removes the low end of it**, by
cancelling exactly the part of a cascade that was coherent. "Nothing in it is
low" and "uniformly tiny noise pops" are the same fault, and this is it.

Row A is the trap, and the reason this is stated carefully: **without**
conduction, turning parity off makes thousands of same-sign impulses land in one
sample. That is an impulse, its spectrum is flat, and the centroid goes *up*.
Parity and conduction have to change together or the measurement lies about
both.

---

## 4 · The deep register, and the kernel per unit

The kernel is now a property of the unit — led by its position in the two axes
the section does not show, tilted a little by its own rate — so a cascade
crossing the body sweeps colour as it travels, and a large event changes while
it happens rather than merely lasting longer. It is implemented as a bank the
units are mapped into, which costs the bank and not 9216 filters.

Dropping the floor of that bank, with everything else held:

| kernel floor | spectral centroid |
|---|---|
| 1000 Hz (shipped range) | 2673 Hz |
| 300 Hz | 1966 Hz |
| 90 Hz | 1668 Hz |
| 30 Hz | 1303 Hz |
| 12 Hz | 1040 Hz |

A low kernel is also given a proportionally longer decay, because a struck body
does that and a fixed decay makes every register sound the same age.

**On Peter's open question — percussive or sustaining.** It does not need
deciding as a separate feature. Take 5 in the renders holds cascades for a
quarter of a second at 96 % of the take above -60 dB, and that is the same
mechanism at slow conduction, not a resonator bolted on. The object sustains
when its conduction is slow and strikes when it is fast, which keeps it a
distance from what B2311.22 does with resonant bodies.

---

## 5 · The finding that had to survive, and did

The shipped bench's one deliberately-kept failure: a harder pulse gathers
*less* of the object onto the beat. Re-measured, at 3 pulses a second:

| grip | gathering R | first third → last third |
|---|---|---|
| 0.05 | **0.266** | 0.362 → 0.310 |
| 0.10 | 0.181 | 0.088 → **0.275** |
| 0.20 | 0.046 | 0.056 → 0.028 |
| 0.35 | 0.042 | 0.044 → 0.069 |
| 0.55 | **0.034** | 0.028 → 0.062 |

An eightfold fall from gentlest to hardest — the shipped object's was 3.3-fold.
The rebuild does not break the object: **it can still be led and it still
cannot be forced**, and rather more sharply than before.

---

## 6 · What it costs

The architecture is event-driven — a unit is looked at only when something
happens to it — and the queue is indexed, because at audio rates it turns over
millions of times a second and a linear scan would decide the design by itself.

| regime | events/s | speed on one core |
|---|---|---|
| rhythm, no conduction | 0.12 M | 127 x realtime |
| rhythm, conduction 2 ms | 0.18 M | 51 x realtime |
| locked at 90 Hz | 6.6 M | 1.5 x realtime |
| locked at 180 Hz | 13 M | 0.8 x realtime |

**The rhythm end is free and the pitch end is not.** A locked body at 180 Hz is
9216 units each firing 180 times a second, and no cleverness removes those
1.7 million events. Options, in the order I would try them: cap the rate band
so the pitch ceiling is reached with fewer units awake; drop the lattice to
2304 units when conduction is fast, since a locked body is doing the same thing
everywhere anyway; or accept that the top of the range is a render-only region.
This needs deciding before it is built, not after.

---

## 7 · Seven takes

`proto/b2311-1-voicing/render.sh`, `renders/`. Same specimen throughout, one
thing changed per take, so what you hear between two of them is the thing named
in the filename.

| take | what changed | crest |
|---|---|---|
| 01 as shipped | instantaneous cascades, one high kernel band, parity on | 17.9 |
| 02 cascades take time | conduction 2 ms | 14.1 |
| 03 low register | kernel floor 1 kHz → 30 Hz | 11.4 |
| 04 parity off | the sign flip removed | 17.4 |
| 05 long and slow | conduction 6 ms, kernel from 20 Hz | 9.8 |
| 06 crossing into pitch | rate band raised to 30–90 Hz until the body locks | **2.6** |
| 07 led by a pulse | 3 Hz, gently | **34.2** |

Take 6 is the one to listen to first: it is the thesis audible for the first
time. Take 5 is the one that answers "longer and lower".

---

## 8 · What I would build, in this order

1. **Conduction delay**, with the refractory floored at 3.5 x it. One new
   control, and it is the one that carries the object across its three scales.
   Everything else in this document depends on it.
2. **Parity off** — or better, a control, since row A shows it is not simply
   wrong, it is wrong *with conduction*. Measure it in the plug-in before
   deciding whether it survives as a control or goes.
3. **Kernel floor down to about 20 Hz**, with decay scaled to the kernel.
4. **The rate band opened**, last, and with the cost decision above made first.
   It is the change that gives the least per unit of risk, which is the reverse
   of what the plan assumed.

And one thing to check in the shipped engine the moment the tree is reachable:
whether its refractory already blocks shoves the way this one does. If it does,
its pitch ceiling is `1 / dead` today, and that single number explains why the
40 Hz rate ceiling was never the real limit.
