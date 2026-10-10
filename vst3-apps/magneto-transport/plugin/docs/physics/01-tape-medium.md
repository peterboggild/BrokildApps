# 01 — The tape as a medium

## What is simulated

Magnetisation along the tape, one value per **cell of length Δx = 0.5 µm**, per
track. The value is the remanent surface fluxivity Φ in **nWb/m (rms-scaled:
a sine recorded at the reference level has rms 250 nWb/m)**.

Why 0.5 µm: the shortest wavelength a ferric tape usefully holds at the slowest
speed is a few µm (20 kHz at 4.75 cm/s is λ = 2.4 µm). The grid must be fine
enough that its own losses (the write averaging over one cell and the read
interpolation between cells) stay far below the physical gap and spacing losses
added in stage 4, or the real-time model cannot meet its ±0.25 dB tolerance
against the reference. At 1 µm with linear interpolation the grid alone lost
about 8 dB at 20 kHz at 4.75 cm/s; at 0.5 µm with Catmull-Rom interpolation it
is a few tenths. At 76 cm/s a sample at 88.2 kHz spans 17 cells, which is
still cheap to integrate.

## Coordinates

The odometer `T(t)` [µm] is the length of tape that has passed the capstan,
`dT/dt = v(t)`. A material point with tape coordinate `u` is at path position
`x = T − u`; a head fixed at path position `h` is over material

    u_h(t) = T(t) − h(t).

Nothing else is needed for the delay: an event written at `t₀` by the record
head (h = 0) is under a playback head at `h_p` when `T(t) − T(t₀) = h_p`, i.e.
after `h_p / v` at constant speed. A speed change while the event is on the tape
changes the time it takes and the rate at which the head scans it — which is a
pitch change of ratio `v_play / v_rec`. A head moving with velocity `ḣ` scans at
`v − ḣ` — Doppler, including reading backwards if `ḣ > v`.

## Storage

- **LOOP** (0.5–6 m): a ring of `L/Δx` cells; index = `u mod N`.
- **REEL**: a ring the size of the readable window (supply guide → last
  playback head + margin). Cells are reset to virgin when they come off the
  supply reel, so whatever the ring held one revolution ago never reaches a head.
  The stock profile of the whole pack is a hash of absolute position (stage 6).

`T` is kept modulo the ring length (an exact multiple of Δx) so it never grows;
it is a double, giving ~1e-7 µm resolution at the largest ring.

## Stage 1 record (ideal)

The record head writes into each cell the **time average of the record signal
over the cell's transit** of the record point. The signal is linear in time
between samples and the tape moves at constant speed within a sample, so every
piece is a trapezoid integrated exactly. This is a box filter in time of width
Δx/v — at the slowest speed a 95 kHz cell rate, i.e. a null far above the audio
band. Stage 3 replaces it with the physical recording zone.

## Stage 1 playback (ideal)

The head output for one sample is the **mean of the tape** (the Catmull-Rom
interpolant through the cell values, integrated in closed form) **over the span the head crossed during the
sample**: `(1/(u₁−u₀)) ∫ M(u) du`. That is the head's output integrated over the
sample period — the integration that keeps a fast read from folding. If the
head barely moved (|u₁−u₀| < 10⁻³ cells) it reads the interpolant at the point.
Stage 4 adds the gap, spacing and thickness losses as spatial filters.

## Sources

- Mee & Daniel, *Magnetic Recording Technology*, 2nd ed., McGraw-Hill 1996 — ch. 1–2 (recording geometry, wavelength λ = v/f).
- Bertram, *Theory of Magnetic Recording*, Cambridge 1994 — ch. 3 (playback as a spatial convolution).
