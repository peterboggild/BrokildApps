# 07 — Playback (stage 4)

## What the playback head sees

A ring playback head with gap g_p, at spacing d_p from the tape surface, reading
a coating whose magnetisation varies along the tape (x) and with depth (y below
the surface). By reciprocity (Bertram ch. 3, Wallace), a sinusoidal
magnetisation of wavenumber k = 2π/λ at depth y contributes in proportion to

    e^{−k (d_p + y)}                                (spacing / depth loss)

and the whole head output is multiplied by the gap function

    sin(k g_p / 2) / (k g_p / 2)                    (gap loss)

summed over depth as complex amplitudes. d_p = 0.3 µm, g_p = 2.5 µm,
coating 14 µm: the reference's numbers (04-record-process.md).

## Each slab carries its own loss — the cheap shortcut is 10 dB wrong

The engine stores four slabs (0–3.5, 3.5–7, 7–10.5, 10.5–14 µm). For a slab
from depth `top` to `bot` (head spacing included), the depth integral is exact:

    L_slab(k) = (e^{−k·top} − e^{−k·bot}) / (k (bot − top))

The tempting shortcut — read the depth-AVERAGED magnetisation and apply one
loss for the whole coating — fails exactly where it matters. Recording loss has
already emptied the deep slabs of treble, so at short wavelengths almost all the
signal is in slab 0. Its true weight is e^{−k·0.3}…e^{−k·3.8}; the whole-coating
loss averages down to 14 µm. At 15 kHz and 19 cm/s (λ = 12.7 µm) the shortcut
reads **10.4 dB too low**. So every slab is read with its own kernel.

## The kernel in space

e^{−k a} is the transform of a Lorentzian of half-width a. Averaged over the
slab's depth:

    h_slab(x) = [ ln(x² + bot²) − ln(x² + top²) ] / (2π (bot − top))

finite at x = 0, unit area, symmetric — so the head response is **zero-phase**,
as the physics is. A pole cascade (minimum-phase IIR) was rejected for two
reasons, both measured: it adds phase the head does not have, and an exponential
roll-off in frequency cannot be followed by poles (4 poles: 5–7 dB off for the
deep slabs within 50 dB of their own level).

**The tail cannot be truncated.** At small k, L_slab ≈ 1 − k·(top+bot)/2: a
cusp, linear in |k|. That is the thickness loss reaching down into the bass
(−0.4 dB at 200 Hz at 19 cm/s for the coating), and in space it IS the 1/x²
tail of h_slab. Any finite kernel is smooth at k = 0 and misses it: truncated
and renormalised, a 294-tap kernel was still 0.85 dB off at 200 µm.

## How the engine reads a slab

- **Near taps** on the slab's own grid (0.5 / 1 / 2 / 4 µm), out to 4× the slab's
  bottom depth plus the boxes' half-widths (88 / 74 / 52 / 34 taps at 48 kHz).
  The kernel is point-sampled at the cells (they are point samples) with the
  gap box (2.5 µm) and the output sample's transit box (the anti-alias the read
  always had) convolved in exactly, through the kernel's second antiderivative.
- **Tail boxes** beyond that, geometric (ratio 1.5), read as box means from
  coarse (4 µm) **prefix sums**. Each slab has exact boxes out to a common edge
  (where the deepest slab's near region ends, ~66 µm); beyond it every slab's
  kernel is c_l/x² with c_l = (bot² − top²)/(2πD), so ONE combined track
  C = Σ c_l·M_l carries all four tails (worst mismatch 3.7 % of the deepest
  slab's tail). The last box takes all remaining mass; the cusp lost beyond a
  few mm is under 0.01 dB even at 20 Hz / 76 cm/s. The prefix sums live in a
  52 cm ring indexed by the unwrapped coarse coordinate: 8 MB whatever the loop.
- Taps depend on the transit (speed) and the head's fractional position: tables
  per **speed bucket** (log-spaced, 4.5 % apart) × **64 phases**, built at
  prepare, so nothing is designed on the audio thread.

Measured on the analytic model (sum of the four slabs, equal content, each slab
inside its own band): long wavelengths agree within 0.003; the residual
0.3–0.8 dB sits at 4–6 µm on the coarse slabs' grid edges, where the test
overstates their content — the record zone and the stage 3 anti-alias rule
remove it. The real check is against the reference's playback (below).

## What 4 slabs cannot do, and the correction

Inside a slab the magnetisation still varies with depth (the surface records
short wavelengths better than 3.5 µm down), and the head weights the top of the
slab most. Four uniform slabs therefore under-read the treble a little.
`mtref --playback` measures exactly this: the same recording read over 16
depths and composed through 4 uniform slabs, at five speeds. Measured: at 1 kHz
the two agree within 0.01–0.06 dB; at the top the slabs read bright (+0.44 dB
at 15 kHz, 19 cm/s; +0.25 at 38; +0.07 at 76) — and the sign was the opposite
of what I predicted. The error is linear in k: the signature of a spacing. So
the correction is physical, not an equaliser: a per-speed **effective extra
head spacing** fitted by the reference (+0.10 µm at 19–38 cm/s, +0.06 at 76,
+0.03 at 9.5, −0.04 at 4.76; fit residual ≤ 0.07 dB from 9.5 cm/s up, 0.17 at
4.76), written to `stock/pb_456.h` and applied in the near taps (the tails act
at long wavelengths, where it is nil).

## The repro level

The head loses treble AND some level at 1 kHz (−1.5 dB at 19 cm/s, −5.2 at
4.76: the thickness loss). The replay side is aligned per speed, at operating
bias: the trim is computed from the read's OWN discrete response (its taps and
boxes, not the closed form), each slab weighted by its small-signal gain and
its zone, and the 0 VU-vs-small-signal weighting measured once at the alignment.
Unity at 1 kHz: −0.006 dB at every sample rate. Off the operating bias the
playback-weighted level moves, physically.

## Measured against the reference (bench section 15, −20 dB, dB re 1 kHz)

| speed | worst, to −20 dB | at 10 / 15 kHz |
|---|---|---|
| 4.76 cm/s | 0.16 dB | 10 kHz is −25 dB: **0.48 dB off — known deviation** |
| 9.53 | 0.09 | 0.09 / 0.21 |
| 19.05 | 0.08 | 0.00 / 0.07 |
| 38.1 | 0.06 | 0.06 / 0.05 |
| 76.2 | 0.06 | 0.02 / 0.06 |

At 4.76 cm/s the record-only error is larger (deep slabs' depth average, partly
the stage 3 anti-alias rule), but the head removes those slabs' treble by
40–80 dB, so it does not carry into playback one to one; the bench prints it as
a diagnostic line, not a check.

## Cost

14.4 % of one core at the worst case (48 kHz, 76 cm/s, three heads, feedback;
the measurement itself wanders by about ±1.5 %). It took the combined tail
(136 prefix reads per head per sample down to ~40) and SSE dot products for the
record kernel and the near taps (float lanes; under 100 products, ~−130 dB).
Kernel tables: 49 speed buckets × 64 phases, built once per internal rate and
shared by every instance.

## Replay and record equalisation (IEC default, NAB alternative)

On flux, the replay equaliser of a standard is (1 + jωτ₂) · jωτ₁/(1 + jωτ₁):
flat mid-band, a +6 dB/oct lift above 1/(2πτ₂), a cut below 1/(2πτ₁).

| speed | IEC τ₂ / τ₁ | NAB τ₂ / τ₁ |
|---|---|---|
| 76.2 cm/s | 35 µs / ∞ | 50 µs / 3180 µs |
| 38.1 | 35 / ∞ | 50 / 3180 |
| 19.05 | 70 / ∞ | 50 / 3180 |
| 9.53 | 90 / 3180 | 90 / 3180 |
| 4.76 | 120 / 1590 | 90 / 3180 |

The speed is continuous, so the time constants are interpolated in log speed:
the machine is aligned for the speed you set. The record equaliser
(pre-emphasis, before the record head, so treble eats headroom as on tape) is
set the way a technician sets it — flat at −20 dB — and is computed from the
machine's own small-signal model: record zones (tables), slab kernels, gap.
