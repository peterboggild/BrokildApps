# 05 — Reference model results (corrected 2026-10-11)

`reference/mtref.cpp` at full settings: 0.5 µm cells, 8 layers, 4.5 mm of tape
per measurement, 38.1 cm/s (the sheet's 15 ips), 150 kHz bias, record head 10 µm
gap / 0.3 µm spacing, playback head 2.5 µm / 0.3 µm, erase head 60 µm. Stock:
the 456-class default of 03-hysteresis.md. "Sheet" is the Quantegy 456 data sheet.

## What changed from the first run, and why it matters

The first stage 2 run (commit `01be407`) measured everything on tape that
**started at negative saturation**: a fresh Preisach staircase begins at
−H_s, and nobody demagnetised it. No recorder records onto tape in that state —
an erase head leaves it demagnetised. It was found in stage 3, when the
real-time model (whose tables start from demagnetised tape) disagreed with the
reference by 2 dB at 0 VU while agreeing with it layer by layer. Fixed: every
reference tape now starts AC-demagnetised (`Stair::demag`). Every number in
sections 2–6 moved, several a lot; the earlier figures in git history are wrong.

The interaction-field tail ν is now also fitted on the **full record process**
(align the bias the sheet's way, set 0 VU, find 3 % HD3) rather than on the
ideal anhysteretic curve, which on demagnetised tape is 1.3 dB optimistic.

## 1. The medium

| | value |
|---|---|
| particle coercivity | lognormal, σ_c = 0.12 (assumed) |
| interaction field | Student-t, FWHM 0.40 H_c (assumed), tail **ν = 2.059**, fitted on the full process to the sheet's MOL |
| anhysteretic susceptibility χ·H_c/M_r | 4.8 |
| SFD (DCD derivative) | 0.52 at ν = 3.7, broader with the fitted heavier tails — a known limit of the classical model (real tape ~0.35) |
| flux at M_r over 14 µm | 2044 nWb/m — the AC-recordable maximum |
| minor-loop closure | identical to 12 digits after 10 000 cycles |
| decaying-AC anhysteretic vs closed form | within 0.0012 at six levels |
| Everett table vs direct quadrature | worst 1.5e−4 (heavier tails, same grid) |

## 2. Bias curve (signal deep-gap field 0.08 H_c; dB re 250 nWb/m rms, depth-averaged)

| h_b/H_c | 1 kHz | 10 kHz |
|---|---|---|
| 0.5 | −152 | −195 |
| 1.0 | −31.6 | −31.8 |
| 1.5 | −13.8 | −14.0 |
| 2.0 | −11.8 | −12.0 |
| 3.0 | −10.4 | −10.7 |
| 4.0 | **−9.7** | −10.0 |
| 6.0 | −11.4 | −11.7 |
| 8.0 | −13.8 | −14.3 |
| 20.0 | −21.4 | −24.8 |

From demagnetised tape, too little bias records almost nothing — no hysteron
switches. 10 kHz through the playback head peaks at **h_b = 1.78**; the
**operating bias** (10 kHz 3 dB past its peak, the sheet's alignment) is
**h_b = 4.54 H_c**, within 0.5 dB of the 1 kHz peak.

## 3. Level sweep, 1 kHz, operating bias

0 VU = deep-gap signal field 0.245 H_c = 5.4 % of the bias.

| dB re 0 VU | out re 0 VU | HD3 % |
|---|---|---|
| −20 | −20.02 | 0.009 |
| −10 | −10.00 | 0.059 |
| −6 | −5.99 | 0.068 |
| 0 | 0.00 | 0.143 |
| +6 | +5.82 | 1.02 |
| +10 | +9.39 | 2.79 |
| +12 | +10.99 | 4.31 |
| +16 | +13.65 | 8.92 |

- **MOL (3 % HD3): 760 nWb/m rms = +12.27 dB re 185. Sheet: +12.3.** (Fitted.)
- **HD3 at 185 nWb/m: 0.070 %. Sheet: 0.10 %.** Not fitted; 3 dB under. Open.
- A ~0.06 % level-independent third harmonic sits under the curve's own from
  −12 to −2 dB: the finite bias decay (a few hundred cycles through the zone)
  leaves a small non-analytic structure at the origin of the transfer curve.
- 8 vs 16 layers: −0.05 dB, HD3 0.6 % relative. Converged.

## 4. Recording loss (depth-averaged, −20 dB, dB re the setting's own 1 kHz)

| f | 0.7× bias | 1.0× | 1.4× |
|---|---|---|---|
| 5 k | −0.06 | −0.08 | −0.07 |
| 10 k | −0.24 | −0.32 | −0.30 |
| 15 k | −0.55 | −0.72 | −0.68 |
| 20 k | −0.97 | −1.26 | −1.20 |

Through a playback head (thickness, spacing and gap loss added): −8.2 dB at
10 kHz, −15.0 at 20 kHz at operating bias — stage 4's equalisation territory.

## 5. Erasure (0 VU 1 kHz recording; residual re the original)

| pass | residual |
|---|---|
| **bias only, record head** | **−42.1 dB** |
| overdub with 3 kHz at 0 VU | −51.7 dB |
| overdub with 3 kHz at −20 dB | −42.5 dB |
| overdub at 0 VU, HALF bias | −4.9 dB |
| erase head h_e = 0.75 / 1.0 / 1.25 / 1.5 / 2 / 4 | −0.2 / −2.2 / −10.9 / −34.8 / −131 / −152 dB |

**Bias erasure is −42 dB per pass** (it read −18 dB on the saturated tape, most
of that the saturated deep layers surviving). That is FLUX MEMORY's and the
LOOPER's overdub decay with the erase head off — a layer lasts about one pass
audibly, much less than the first figure suggested, and FLUX MEMORY's partial
erase is what lets more survive. The erase head's transition is **h_e 0.75–2**.

## 6. An overwrite still depends on more than the flux

| pre-state (same 250 nWb/m at 1 kHz) | 1 kHz residual | new 3 kHz tone |
|---|---|---|
| recorded at operating bias | −51.7 dB | 249.2 |
| at 0.6× bias (shallow) | −127 dB | 249.1 |
| at 1.6× bias (deep) | −25.7 dB | 251.8 |

The residual depends on how deep the old recording went (101 dB spread); the
new tone barely (0.09 dB). Stage 3 carries depth (four slabs) for this reason.

## 7. Cost

9000 cells × 8 layers per record pass in 0.12 s on 20 threads = 10× real time
at 38.1 cm/s. For measuring, not playing.

## 8. The stage 3 tables (`mtref --tables`, `Source/stock/stock_456.h`)

Four real-time layers, each a **slab** of 3.5 µm whose tables are the average
over 4 sub-depths (16 depths in all), each entry also averaged over 8 bias
phases, written for ν = 2.0589. Per layer: G[A][M_old][s] on 28 × 11 × 81
points; the recording zone (centroid, rms, gain) per bias from a band-limited
pulse; the per-layer response at three biases. At operating bias:

| layer | A | zone centroid | rms | gain = table slope |
|---|---|---|---|---|
| 0 (0–3.5 µm) | 3.41 | 6.4 µm | 0.63 µm | 0.642 |
| 1 (3.5–7) | 2.12 | 6.8 | 0.89 | 0.904 |
| 2 (7–10.5) | 1.46 | 6.0 | 1.63 | 0.882 |
| 3 (10.5–14) | 1.10 | 3.9 | 1.84 | 0.364 |

How the real-time model uses them and how well it reproduces this document:
`06-realtime-record.md`.
