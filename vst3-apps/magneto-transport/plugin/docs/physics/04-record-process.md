# 04 — The record process with AC bias (offline reference)

## Head field

Record head: ring head with gap g_r, modelled by the Karlqvist field. Along the
tape (x, with the gap centred at 0) and into the coating (y, measured from the
head surface), the longitudinal component is

    H_x(x, y) = (H_g/π) [ atan((g/2 + x)/y) + atan((g/2 − x)/y) ]

with H_g the deep-gap field, proportional to the head current. At the gap and the
surface H_x → H_g; far away H_x ≈ H_g g y / (π (x² + y²)), i.e. the field at a
deep layer falls off more slowly than at the surface — that is why bias "reaches
deeper" and why over-bias moves the surface layers' recording zone far out where
the gradient is weak.

The head current is bias plus signal, so with the shape f(x, y) = H_x/H_g:

    H(cell, t) = f(x_cell(t), y) · [ h_b sin(2π f_b t) + h_s s(t) ]

h_b = deep-gap bias amplitude, h_s = deep-gap signal field for s = 1, both in
units of H_c. Bias frequency 150 kHz (studio practice; the brief said ~100 kHz).

Geometry used: g_r = 10 µm, head-to-tape spacing d = 0.3 µm (an effective figure
that includes surface roughness), coating δ = 14 µm in 8 layers (centres at
d + (l + ½) δ/8), tape speed 38.1 cm/s (15 ips, the data sheet's condition).

## Advection

A cell at tape coordinate u is at x = v t − u. It enters the field at x = −X and
leaves at +X, where X is where the bias amplitude at the deepest layer has fallen
to 0.25 H_c: hysterons with coercivity below that have no population (p_c is
lognormal, ln 0.25 / σ_c is nine sigma), so the field beyond X changes nothing.
Each layer of each cell is an independent Preisach state (scalar model, no
cell–cell interaction), so cells run in parallel.

## Only the turning points matter

The Preisach state depends on the sequence of field extrema alone
(03-hysteresis.md). Within one bias half-cycle the field at the cell has at most
one interior extremum (bias dominates, f and s vary slowly), so the record
process is: for every bias half-cycle while the cell is in the zone, find the
maximum and the minimum of H(cell, t) on that half-cycle (nine samples, the best
refined by three parabolic steps to ~1e−8), and feed them to the staircase in
time order. A sample that is not a true extremum is absorbed by the update rule;
nothing else is approximated. This replaces a MHz-rate time integration by two
exact turning points per half-cycle, and it is exact rather than merely finer.

The bias phase a cell sees is its real phase (t = u/v), so cells half a bias
wavelength apart see opposite phases and the recorded magnetisation carries a
ripple at the bias wavelength (2.5 µm at 38.1 cm/s) — physical, and removed by
playback gap loss as it is on a real machine.

## What comes out

Per cell and layer, the remanent M/M_s after leaving the zone. Flux per unit
track width in nWb/m: Φ = μ0 M_s δ · mean over layers of (M/M_s) · 10⁹ at long
wavelength; for a playback head at wavelength λ (k = 2π/λ) each layer is weighted
by e^{−k(d_p + y_l)} · sinh(kΔ/2)/(kΔ/2) and the whole by the gap function
sin(k g_p/2)/(k g_p/2), summed as complex amplitudes (reciprocity with a Karlqvist
playback head; d_p = 0.3 µm, g_p = 2.5 µm). The long-wavelength readout is the
"recording" result; the playback-weighted one is what a machine measures.

## Erase head

The same machinery with a wide gap (60 µm), no signal, and a deep-gap AC field
h_e from 0 (off) to 40 H_c (full). Partial erasure is h_e between — FLUX MEMORY's
control. Bias erasure — recording over old material with bias on — is the record
pass itself with the old state as the starting staircase.

## Measured in the reference (05-reference-results.md)

1. The model's own loop: H_c, S, SFD, anhysteretic curve, minor-loop closure.
2. Bias curve: 1 kHz and 10 kHz output vs h_b; the operating bias defined the
   way the sheet does it — 10 kHz output 3 dB past its peak at 15 ips.
3. Level sweep at 1 kHz: THD, MOL (3 % third harmonic) vs the sheet's +12.3 dB re
   185 nWb/m, the third harmonic at reference level vs 0.1 %.
4. Wavelength response at under / operating / over bias.
5. Bias erasure and erase-head residual vs h_e.
6. The one-number-per-cell question: pre-states with equal flux but different
   histories, overwritten identically — how different are the results?
7. Cost per second of tape.
