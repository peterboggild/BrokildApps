# 06 — The real-time record process (stage 3)

## The tape

Per track, **four depth slabs** of 3.5 µm each (centres 1.75 / 5.25 / 8.75 /
12.25 µm below the surface), stored at 0.5 / 1 / 2 / 4 µm resolution: a deep
slab cannot hold a short wavelength (its recording zone is longer, and stage 4's
thickness loss would remove it anyway). Each cell holds its slab's remanence
M/M_s, with 1 = M_r. 30 MB per metre of stereo loop.

## The record head

1. The record signal becomes a deep-gap field (0 VU peak = `HS_0VU` = 0.233 H_c at the 19.05 cm/s alignment)
   and is written onto a short **field ring** at the gap, at 0.5 µm resolution,
   each cell the signal's mean over its own transit (Catmull-Rom between samples,
   integrated exactly).
2. A slab cell is **finished** once the tape has carried it past its recording
   zone: the field ring is averaged with that slab's **zone kernel** — a
   Gaussian at the measured centroid downstream of the gap, with a width fitted
   to the measured kernel's own transfer over 6–40 µm wavelengths — and the new
   remanence is **G[v][A][M_old][s_zone]**, where A = h_b · `LAYER_FMAX[l]` is
   the bias the slab sees, v the tape speed and M_old what the cell held.
   Log-interpolated in v and A, linear in M_old, and in s from a 1025-point
   resample of the 41-point table, made once per bias or speed change.
   The cells are **point samples**: playback reconstructs each slab with a
   smooth curve through them, so the kernel takes no box for the cell width
   (a box here as well cost 0.5 dB at 15 kHz at 19 cm/s against the reference).
   Where a slab's own Nyquist frequency (speed / two cells) falls inside the
   audio band — the 2 µm slab below 9.5 cm/s, the 4 µm slab below 19 cm/s — its
   kernel is widened to be 60 dB down there, faded in as that Nyquist drops from
   24 to 16 kHz. Without it a 15 kHz tone at 4.75 cm/s folded to −25 dB; with it,
   −86 dB.
3. G is antisymmetrised (G(−M, −s) = −G(M, s)), so silence on erased tape
   records exactly zero and table noise cannot add DC.

The erase head (full field in stage 3) and virgin REEL tape set every slab to
the demagnetised state, M = 0.

## Playback (stage 3)

Each head reads every slab (Catmull-Rom integrated over the span crossed in the
sample) and averages them — the long-wavelength flux. Stage 4 replaces the
equal average with the thickness-loss weighting e^{−k y} per slab plus spacing
and gap loss.

## Alignment (what a technician does to a real machine)

At operating bias and the machine's **nominal speed, 19.05 cm/s** (7.5 ips, the
classic studio echo speed): record level so 0 VU = 250 nWb/m (the table's
`HS_0VU`), and repro level so 250 nWb/m = 0 VU (`outK`, computed at prepare
from the slices). The heads read at the gain-weighted zone centroid (`xRefUm`)
of the *current* speed, so that the delay is head distance / speed at every
speed. Off the operating bias or the nominal speed, level, distortion and
delay move — as they do on a real machine.

## Speed

The record process is not speed-independent: a slower tape takes more bias
cycles through the zone and records more completely. Measured by the reference
at a fixed bias and field (its own 38.1 cm/s alignment), so the tables carry a
**speed axis** at 4.76 / 9.53 / 19.05 / 38.1 / 76.2 cm/s, log-interpolated:

| | real time | reference |
|---|---|---|
| sensitivity at 19.05 re 38.1 | +0.63 dB | +0.63 dB |
| sensitivity at 76.2 re 38.1 | −1.27 dB | −1.25 dB |
| HD3 at +6 / +10 dB, 19.05 | 1.28 / 3.28 % | 1.38 / 3.42 % |
| HD3 at +6 / +10 dB, 76.2 | 0.50 / 1.78 % | 0.54 / 1.87 % |
| recording loss 10 / 15 kHz, 19.05 | −1.15 / −2.56 | −1.13 / −2.38 |
| recording loss 10 / 15 kHz, 76.2 | −0.13 / −0.37 | −0.10 / −0.23 |

A speed change rebuilds the record head (once per 0.2 % of speed), so a
varispeed glide records with the right physics all the way.

## Feedback

Uncapped: `FEEDBACK_MAX` = 1.5 small-signal loop gain at 100 % with one head at
0 dB. Past unity the loop self-oscillates and is held by the tape running out of
remanence: the output cannot exceed full remanence through the repro alignment
(+0.16 dBFS with one head at 0 dB, since 0 VU is −18 dBFS and full remanence is
+18.3 dB above 250 nWb/m).

## Measured against the reference (bench section 13, 38.1 cm/s, 1 kHz, the reference's bias and field)

| | real time | reference | |
|---|---|---|---|
| level transfer −20..+16 dB | | | within **0.026 dB** |
| MOL | +12.35 dB re 185 | +12.27 | within 0.08 dB (sheet +12.3) |
| HD3 +6..+16 dB | | | within 0.4 dB |
| **HD3 at 0 VU** | **0.096 %** | **0.143 %** | **−3.5 dB: known deviation** |
| recording loss to 15 kHz | −0.79 at 15 k | −0.72 | within 0.07 dB |
| bias curve 1.5..8 H_c | | | within 0.19 dB |

**The known deviation**: at 0 VU the third harmonic is 3.5 dB lower than the
reference's (−60 vs −57 dB re the fundamental). Ruled out by measurement: the
table's s resolution (41 → 81 points moved it 0.001 %), depth sampling (point
layers → slab averages left it), bias phase (averaged). Both models carry the
~0.06 % level-independent component of the finite bias decay; at 0 VU it and
the curve's own third harmonic are comparable, and the DC-table method does not
reproduce how they combine in the AC process. Above 0 VU, where the character
lives, they agree.

## Not yet covered

- Programme material (drums, voice) against reference renders — the §6
  tolerance; needs a `--render` mode in the reference.
- The erase head is full-field only; FLUX MEMORY's partial erase (stage 7) needs
  erase-head tables (the 60 µm gap's field profile differs from the record head's).

## Cost

14.8 % of one core at 48 kHz stereo, 76 cm/s, three heads with feedback (the worst case): about 8 % recording the cells, 5 % reading them, 2 % the rest. The kernel is cut at 4 σ (6·10⁻⁵ of its mass) and both tracks share one pass over the taps.
