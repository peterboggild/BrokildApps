# DECISIONS

Physics and engineering choices, newest last. Each says what was chosen, what it
cost, and what would make it wrong.

---

## 2026-10-10 — the tape is a medium, 0.5 µm cells

Chosen: magnetisation stored per µm of tape, heads read and write at physical
positions (`docs/physics/01-tape-medium.md`). Rejected: a sample-domain delay
line with a computed delay time, which is what every tape-delay plugin does and
which would have to fake every effect this brief wants to emerge.

Cost: 8 MB per metre of stereo loop (96 MB at 6 m). The tape is allocated to
the CURRENT need on the message thread (a REEL window is 6.4 MB) and handed to
the audio thread at a splice, so a default instance stays small. First drafted
at 1 µm with linear interpolation; rejected because the grid alone lost ~8 dB at
20 kHz at the slowest speed, which would eat the real-time-vs-reference
tolerance before any physics ran.

Wrong if: a later stage needs more per-cell state than remanence (see the next
entry) — then memory grows with it.

## 2026-10-10 — one number per cell, even with Preisach

AC-biased recording leaves the coating anhysteretic, which a Preisach model
characterises by its remanence alone; the history staircase matters only while
the cell is inside the record zone, where it is computed, not stored. To be
checked against the stage 2 reference before stage 3 depends on it.

## 2026-10-10 — reference level

−18 dBFS RMS 1 kHz = 0 VU = **250 nWb/m** rms fluxivity. One unit of sample
amplitude = 250 / 10^(−18/20) = 1985.9 nWb/m at the default record level.
250 is an elevated studio operating level, appropriate for the 1970s
high-output stock that will be the default; the low-output vintage stock will
reach saturation sooner from the same input, which is the point.

## 2026-10-10 — internal rate and the dry path

The wet path runs at ≈ 88.2–96 kHz: ×2 at 44.1/48 k, ×1 at 88.2 k and above
(176.4/192 k hosts run ×1 too; the tape does not care). 63-tap half-band pair
(the fleet's, from `bwfx_dsp.h` via Rite of Passage). The dry path stays at the
host rate and the plugin reports **zero latency**; the half-band pair's delay is
compensated by reading each playback head the corresponding length of tape
*ahead* (`v · delay`), which is exact at constant speed. Measured in the bench,
not taken from the comment.

## 2026-10-10 — stage 1 feedback cannot self-oscillate

Containment must come from tape saturation (stage 3). Until then the loop gain is
capped: effective feedback = control × 0.97 / (largest boost of the tone stage).
No clipper, no limiter. Self-oscillation arrives with the hysteresis model.

## 2026-10-10 — parameter table with member pointers

Every host parameter is one row: id, name, range, default, kind, and a pointer to
its `Params` member. The APVTS layout, the processor's read, the initial state
for the panel and the bench all walk it, so there is no read order to get wrong
(the Hairfryer pattern), and rows can be added anywhere.

## 2026-10-10 — heads cannot pass each other

Three playback heads on one rail touch the tape on the same line, so a carriage
that runs into its neighbour STOPS against it (centres no closer than 1.2 cm);
the neighbour holds its place. The control keeps asking for where it was set;
the head's delay readout and the machine view show where it really is.
Rejected: heads passing through each other, which the first panel draw showed
and which no rail can do. To be confirmed with Peter at listening gate A.

## 2026-10-10 — stage 1 grid response, and the two corrections that made it flat

The writer reconstructs the record signal between samples with a Catmull-Rom
cubic (exact per-cell integrals), and a fixed 3-tap FIR undoes the read
integration, which is a box in time one internal sample wide at any speed.
Before: -1.19 dB at 16 kHz at every speed (straight-line writer + box).
After: -0.12 dB at 16 kHz at 19 and 76 cm/s; -0.65 dB at 4.75 cm/s, where the
cell transit is about one sample and stage 4's gap and spacing losses will be
tens of dB anyway.

## 2026-10-10 — input latency is paid at the record head

The internal latency splits into an input half (up half-band) and an output half
(down half-band, read box, correction FIR). The input half is compensated by
writing each sample where the tape WAS when it entered the box (a 64-sample
history of the odometer), the output half by reading ahead. Compensating the
whole latency at playback speed was measured 69 us wrong for an echo recorded
during a motor glide; split, 3 us.
