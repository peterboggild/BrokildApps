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

## The equalisers as built (stage 4b)

- **Targets** from the machine's own small-signal model at operating bias (each
  slab's record gain and zone with its phase, its exact slab response):
  replay = the standard's curve x an HF trim for the head's spacing and gap,
  record = what makes the short-circuit flux follow the standard. Both
  normalised at 1 kHz, so the 0 VU alignment and MOL do not move. The limits
  (+10 dB replay trim, +20 dB pre-emphasis) have 1.5 dB soft knees: a hard
  limit puts a kink in a target the shelves cannot follow.
- **Realised** as 12 first-order shelves (12 Hz–24 kHz, log-spaced) plus a gain,
  fitted by Levenberg–Marquardt in dB, bilinear with prewarping at each corner:
  minimum-phase, like the circuits. Fitted once per process for 2 standards ×
  49 speed buckets (warm-started bucket to bucket). The motor's speed picks a
  blend: the analogue gains of the two buckets either side are interpolated
  and rediscretised as the speed moves — on the slow-speed roll-off a 3 %
  speed mismatch was worth a dB.
- Tried and rejected, both measured: 16 shelves (ill-conditioned: neighbouring
  shelves trade huge opposite gains, and an interpolated set put a 50 %
  feedback loop past unity); band weighting and a smoothness penalty (each
  produced a steady treble droop at every speed).
- The record pre-emphasis sits in the record amplifier, after the input and
  feedback mix: boosted treble meets the tape's saturation like everything
  else. The echo send is taken after the replay equaliser.

**Measured (bench section 16, −20 dB, dB re 1 kHz):** flat within 0.25 dB from
40 Hz to 15 kHz at 19–76 cm/s, both standards; within 0.4 dB to 10 kHz at
9.5 cm/s and 0.5 dB to 6 kHz at 4.76 cm/s, falling above (the gap null near
19 kHz and the +10 dB trim limit, as on a real slow-speed machine). Fit error
within 0.44 dB in those bands. Self-oscillation at 100 % feedback settles near
414 Hz with the tape's remanence at 0.95 of full; the OUTPUT may now peak
above the full-remanence level (+2.8 dBFS there) because the replay
equaliser lifts the harmonics a saturated tape makes — on the tape, nothing
passes full. Transport-timing and reference sections run with the
equalisers out (their phase is real, and not the transport's).

## The head bump, crosstalk and the playback amplifier (stage 4c)

**Head bump (the contour effect).** A head of finite length reads long
wavelengths through its outer pole edges as well, with the opposite sign: the
magnetic potential steps by 2V across the gap and back by V at each outer
edge, so by reciprocity H(k) = 1 − cos(kL)·s(k), with s(k) = exp(−(kσ)²/2)
the smoothing of the field's return at the edges. A bump near the wavelength
2L and a cut toward DC (a finite head cannot read an infinitely long
wavelength), both moving with speed. The reference's head has infinite poles,
so L = 2 mm (a 4 mm face) and σ = 1.05 mm are stated assumptions; nobody
aligns a head bump away, so the equalisers do not touch it. Built from a
depth-sum prefix track (at millimetre wavelengths every slab weighs the same)
as Gaussians of nine boxes of σ/2: five boxes of σ aliased the Gaussian's
transform and drew a spurious +0.9 dB ripple at three times the bump's
frequency.

Measured (−20 dB, re 1 kHz): +2.5 dB at 40 Hz (19 cm/s), +2.6 at 81 Hz
(38 cm/s), +2.6 at 162 Hz (76 cm/s); −9 dB a quarter of the way down.

**Crosstalk**, at each head before its pan pot: the other track's depth sum
smoothed over the 0.75 mm guard band (fringing, −50 dB at long wavelengths)
and a flat leak inside the head (−65 dB). Symmetric like the head's field:
a one-pole lowpass first used for the fringing has group delay, and at
−50 dB it still moved a test echo by 6 µs. Measured −64 dB at 100 Hz,
−66 dB at 1 kHz (19 cm/s).

The bump and the fringing carry nothing above a few hundred hertz, so they
are evaluated every 8th internal sample at the position the head will then
have reached and interpolated between: no delay, and the measured bump is
identical to the per-sample version.

**Playback amplifier**: unity to 0 dBFS, a smooth tanh knee into a +2 dBFS
rail (20 dB over 0 VU). On the echo send at the internal rate; on the output
AFTER decimation, because the half-band filter overshoots a clipped
waveform by about a dB. A self-oscillation now peaks at +1.99 dBFS.

**Cost**: 12.5 % of one core at the worst case, timed on the thread's own CPU
time, best of five; the CPU's clock state still moves it by about ±1.5 %, so
the bench reports it against the 15 % target and fails only past 20 %.
