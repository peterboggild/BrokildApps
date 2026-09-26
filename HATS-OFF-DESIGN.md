# HATS OFF — the hi-hat and cymbal synthesiser

Built 2026-09-26, the third of the trilogy after Kickstart and Snare Tactics.
Peter's brief: *"finish this amazing series - trilogy - with the third one
'Hats off', covering hats and cymbals, all synthesized. Cover every use from
old school vintage, to techno, dub, dubstep and acoustic (pay especial
attention to realism for the acoustic). Same style."*

Source `vst3-apps/hats-off/plugin/`, built to `C:\Users\peter\b\_build\HatsOff\plugin`.
Plugin code **`HtOf`** (checked unique), PRODUCT_NAME **Hats Off**, VST3 +
Standalone, Instrument/Drum, build id `HO_BUILD_ID`. Patches in
`Documents\Brokild patches\Hats Off`. No BWFX, the trilogy's call.

## 1. What a cymbal is (the research, condensed)

A cymbal is a thin bronze plate, and three things make it sound like one:

1. **Modal density.** Hundreds of inharmonic modes, sparse at the bottom and
   dense at the top, with many coming in near-degenerate PAIRS whose slow
   beating is the shimmer. A few modes cannot fake it; noise alone cannot
   either, because noise has no pitch and a cymbal plainly does (a ride has a
   note, a 20" and a 14" are different instruments).
2. **The energy cascade.** A thin plate is nonlinear: a hard hit puts energy
   into the low modes, and over tens of milliseconds it flows UP into the high
   ones. That is the crash's bloom — the sound gets BRIGHTER after the stick
   has gone — and it is what every sample-free cymbal usually misses.
3. **Where and with what.** The bell (the dome) rings a few strong, nearly
   tonal partials: the ride bell's ping. The bow gives the ride's defined stick
   with a wash behind it. The edge excites everything: the crash. A china's
   upturned edge adds a trashy, rough nonlinearity.

A **hi-hat** is two cymbals and a pedal. Closed, the plates damp each other
to a tick; half-open, they touch intermittently and SIZZLE; open, they ring;
the foot alone makes the CHICK; and closing the pedal CHOKES an open hat.

The drum machines did it with circuits: the **808** sums six square waves at
unrelated pitches (205.3, 304.4, 369.6, 522.7, 540 and 800 Hz) through a
band-pass; the **909** used 6-bit samples of real cymbals; the **CR-78** and
**606** leaned on noise. Hats Off's BRONZE control walks from that circuit to
the cast plate.

## 2. The controls: axes, not simulations

| section | controls |
|---|---|
| METAL | PITCH, SIZE (inches), BRONZE (808 circuit ↔ cast plate), DENSITY, BLOOM, TRASH, DECAY, NOISE |
| HAT | OPEN (closed hat ↔ free cymbal), SIZZLE (plate or rivet rattle), CHICK (the foot), CHOKE |
| HIT | STRIKE (bell ↔ bow ↔ edge), STICK |
| EQ | CUT, AIR |
| ERA | GRIT, ROOM, WIDTH |
| ECHO | ECHO, TIME (host tempo) |
| TRANSIENT · DRIVE · COMP · OUT | as Kickstart and Snare Tactics; KEYS = FIXED / GM KIT / CHROMATIC |

**Articulations**, GM KIT: 42 closed, 44 pedal, 46 open, 51/59 ride (as
dialled), 53 ride bell, 49/52/55/57 crash (edge). The pad: centre is the bell,
the rim is the edge; buttons CLOSED, PEDAL, OPEN.

## 3. The engine (engine/ho_engine.*, JUCE-free)

Voices at 4× (the modal bank is complex one-pole rotations, fixed per hit, so
it vectorises), stereo from the voice: each mode sits at its own place across
WIDTH, the way overheads hear a cymbal. EQ → GRIT → TRANSIENT → ROOM → DRIVE →
COLOUR → half-bands → COMP → ECHO → LEVEL → ceiling. At WIDTH 0 and ECHO 0 the
two channels are the same sample for sample.

Measured numbers and lessons follow in §4 and §5.

## 4. Measured (bench `hotest` 53, host `hohost` 76, panel `hoshot` 12 — all clear)

| what | number |
|---|---|
| lowest mode against 2900 / SIZE, 3 sizes × 3 rates | 0.000 cents |
| DECAY: a 1 kHz mode to −60 dB | within 0.3 % |
| OPEN 100 → 0: time to −40 dB | 1339 → 38 ms |
| CHOKE: open hat 400–600 ms after a closed hit | −112.8 dB (−28.3 without) |
| BRONZE 0 → 100: share of 2–12 kHz on the 808's lattice | 98 % → 16 % |
| DENSITY 0 → 100: sixth-octave bands 1–12 kHz filled | 10 → 22 of 22 |
| BLOOM 0 → 100: brightness at 60 ms over the first 10 ms | ×0.96 → ×1.51 |
| STRIKE bell → edge: centroid; −40 dB time | 4.3 → 8.5 kHz; 3.94 s vs 2.69 s |
| STICK 0 → 100: 3–16 kHz in the first 2 ms | +9.9 dB |
| WIDTH 100: L/R correlation | 0.34 (1.0 and bit-identical at 0) |
| aliasing, RAZOR WING full (drive at 8x) | −88.8 dB at 2 kHz, −84.3 dB at 4.7 kHz |
| drive level match, four engines | worst 1.4 dB |
| latency | 103 samples (23 decimator + 8 drive + 72 look-ahead) |
| cost: a 22-inch ride on eighths, six ringing, everything on | 11.1 % of one core |

Presets levelled to −17 dB RMS over the first 80 ms (peak ≤ −1 dBFS clean), three passes, converged.

## 5. Lessons

1. **Sixty sines draw a comb, and a cymbal is a continuum.** The demo spectrograms of the
   first complete build showed ~60 discrete horizontal lines above 5 kHz, where a real
   crash is a dense smear. Every bench check passed — coverage in sixth-octave bands
   cannot see gaps narrower than a sixth of an octave. The fix is the standard hybrid:
   modes below, and above max(1.5 kHz, 5·f_lowest) an 8-band noise WASH obeying the
   plate's own laws (decay by frequency ×0.9, strike tilt, closing damping, bloom).
   **Look at a spectrogram of the demo; a band-energy check measures the wrong thing.**
2. **The wash masked two knobs, and the checks said so.** TRASH (flatness doubles) failed
   because flatness cannot rise on something already flat — it is now measured at
   DENSITY 0.1 where the lines are sparse, and TRASH modulates the wash too. STICK fell
   to +3.4 dB because the wash arrived instantly; a real continuum is the plate's
   nonlinearity passing energy upwards, which takes a couple of ms, so the wash now
   starts at 0.3 and grows over 2.5 ms — the stick got its room back (+9.9 dB).
3. **`f(a(), b())` has no defined evaluation order.** Nested half-band calls fed samples
   out of order on MSVC (right-to-left) and aliased at −30 dB. Sequence stateful calls.
4. **A struck plate starts at rest.** Random mode phases were a step on sample one, a
   click that hid STICK entirely; all-zero phases summed coherently and pinned the
   ceiling. Phase 0 with ±1 mode-shape signs from the seed, and per-hit amplitude jitter.
5. **White noise at 4x is 80 % above hearing** — rattle and stick are band-passes.
6. **A per-voice field must be reset per voice** (`nWash = 0` before the plate block), or
   a circuit-only hit replays the last bronze hit's wash.
