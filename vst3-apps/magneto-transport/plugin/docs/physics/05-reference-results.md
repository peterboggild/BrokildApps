# 05 — Reference model results (stage 2, 2026-10-11)

`reference/mtref.cpp`, run at full settings: 0.5 µm cells, 8 layers, 4.5 mm of
tape per measurement, 38.1 cm/s, 150 kHz bias, record head 10 µm gap / 0.3 µm
spacing, playback head 2.5 µm / 0.3 µm, erase head 60 µm. Stock: the 456-class
default of 03-hysteresis.md. Every number below is a measurement of the model;
"sheet" is the Quantegy 456 data sheet.

## 1. The medium

| | value |
|---|---|
| particle coercivity | lognormal, σ_c = 0.12 (assumed) |
| interaction field | Student-t, FWHM 0.40 H_c (assumed), tail ν = **3.73** (fitted: HD3 = 3 % at the sheet's MOL on the ideal anhysteretic curve) |
| bulk H_c | 1.005 (1 by construction; the 0.5 % is the quadrature) |
| Preisach squareness | 0.9935 (orientation supplies the rest; bulk S = 0.80 only sets the flux scale) |
| DCD-derivative SFD | 0.52 — **a known limitation of the classical model**: an interaction width that fits the anhysteretic curve makes the remanence curve broader than real tape (~0.35). A moving (mean-field) Preisach would reconcile them; not needed for anything audible yet |
| anhysteretic susceptibility χ·H_c/M_r | 5.3 |
| flux at M_r over 14 µm | 2044 nWb/m — the AC-recordable maximum; 0 VU (250 rms) peaks at 17 % of it |
| Everett table vs direct quadrature | worst 3.3e−6 |
| minor-loop closure | M identical to 12 digits after 10 000 cycles; staircase 1 entry |
| decaying-AC anhysteretic vs closed form 2Φ_u(s)−1 | agree to 4 decimals at six levels |

## 2. Bias curve (signal deep-gap field 0.08 H_c; dB re 250 nWb/m rms)

| h_b/H_c | 1 kHz avg | 1 kHz play | 10 kHz avg | 10 kHz play |
|---|---|---|---|---|
| 0.5 | −32.6 | −33.1 | −32.7 | −36.8 |
| 1.0 | −14.2 | −14.5 | −14.3 | −17.2 |
| 1.5 | −8.5 | −8.9 | −8.9 | **−12.8** (10 kHz peak) |
| 2.0 | −7.7 | −8.3 | −8.2 | −13.6 |
| 3.0 | **−7.2** (1 kHz peak) | −8.1 | −7.9 | −15.6 |
| 4.0 | −7.7 | −8.8 | −8.5 | −17.3 |
| 6.0 | −10.6 | −11.7 | −10.9 | −20.3 |
| 10.0 | −14.9 | −15.9 | −15.8 | −25.0 |
| 20.0 | −20.6 | −21.7 | −24.1 | −33.0 |

The classic shape: 10 kHz peaks at a lower bias than 1 kHz, because the deep
layers that bias reaches late contribute nothing at 10 kHz (thickness loss) and
the surface layers' recording zone moves out as bias rises. **Operating bias**,
defined the way the sheet aligns it (10 kHz 3 dB past its peak): **h_b = 3.11**,
which costs 1 dB at 1 kHz against the 1 kHz peak.

## 3. Level sweep, 1 kHz, operating bias

0 VU = deep-gap signal field 0.186 H_c = 6 % of the bias.

| dB re 0 VU | flux rms | HD3 % | THD % |
|---|---|---|---|
| −20 | 25.3 | 0.003 | 0.12 |
| −10 | 79.8 | 0.034 | 0.38 |
| −6 | 126.3 | 0.086 | 0.61 |
| 0 | 250.0 | 0.337 | 1.24 |
| +6 | 484.6 | 1.28 | 2.67 |
| +10 | 728.7 | 2.94 | 4.64 |
| +12 | 876.8 | 4.32 | 6.16 |
| +16 | 1200.0 | 8.45 | 10.6 |

- **MOL (3 % third harmonic): 736 nWb/m rms = +12.0 dB re 185. Sheet: +12.3 dB (760).** Within 0.3 dB, and the fit was made on the ideal curve, so the full process landed there on its own.
- **HD3 at 185 nWb/m: 0.19 %. Sheet: 0.10 %.** The one shape parameter cannot hit both numbers: the sheet's pair means the real curve is flatter at low level and sharper at the knee than a Student-t interaction field gives. Open item: a two-parameter interaction density (flat core + heavy tails) or the mean-field term. Audibly this is a third harmonic at −55 dB instead of −60 dB, 2.6 dB below 0 VU.
- THD includes 0.12 % of **bias-structure residue** at every level (the 2nd..10th harmonic sum does not fall below it): the bias phase ripple at 2.5 µm wavelength aliased by the 0.5 µm cells into the measurement band. Physical, and removed by playback gap loss on a real machine; the depth-averaged readout has no gap.
- Layer convergence: 8 vs 16 layers, 0 VU flux −0.05 dB, HD3 0.8 % relative. Converged.

## 4. Response vs frequency (−20 dB, dB re each setting's own 1 kHz)

| f | 0.7× rec | 1.0× rec | 1.4× rec | 0.7× play | 1.0× play | 1.4× play |
|---|---|---|---|---|---|---|
| 2 k | −0.02 | −0.02 | −0.02 | −0.67 | −0.91 | −1.07 |
| 5 k | −0.13 | −0.17 | −0.16 | −2.63 | −3.57 | −4.11 |
| 10 k | −0.53 | −0.71 | −0.66 | −5.79 | −7.65 | −8.60 |
| 15 k | −1.21 | −1.64 | −1.45 | −8.76 | −11.17 | −12.33 |
| 20 k | −2.22 | −2.99 | −2.46 | −11.48 | −14.13 | −15.44 |

"rec" is the recording loss alone (depth-averaged flux). "play" adds the playback
head: thickness loss dominates (14 µm coating at 38 µm wavelength), spacing and
gap are small at 15 ips; stage 4's equalisation restores this. Over-bias
(1.4×) loses a further 1.0 dB at 10 kHz and 1.3 dB at 20 kHz through playback;
under-bias gains — the physics the brief asked to emerge, modest at 15 ips.

## 5. Erasure (0 VU 1 kHz; residual re the original)

| pass | residual |
|---|---|
| bias only, record head | **−18.0 dB** |
| overdub with 3 kHz at 0 VU | −18.8 dB |
| overdub with 3 kHz at −20 dB | −18.0 dB |
| overdub at 0 VU, HALF bias | −2.4 dB |
| erase head h_e = 0.5 / 0.75 / 1.0 / 1.25 / 1.5 / 2.0 / 4 / 40 | −0.0 / −0.2 / −3.1 / −19.1 / −53.2 / −161 / −189 / −189 dB |

**Bias erasure is −18 dB per pass**: what survives is in the layers the record
head's bias cannot reach. That is FLUX MEMORY's and the LOOPER's overdub decay with
the erase head off. The erase head's transition is **h_e between 0.75 and 1.75**
(−0.2 to −53 dB); above 2 the loop is clean to the floor. FLUX MEMORY's ERASE
control maps onto that range.

## 6. Does an overwrite depend on more than the flux? YES.

Three pre-states with identical 1 kHz flux (250 nWb/m), overwritten identically:

| pre-state | 1 kHz residual | new 3 kHz tone |
|---|---|---|
| recorded at operating bias | −18.8 dB | 222 nWb/m |
| recorded at 0.6× bias (shallow) | **−106 dB** | 248 |
| recorded at 1.6× bias (deep) | **−8.5 dB** | 201 |

What survives an overwrite is the part of the old recording that lies deeper
than the new bias reaches, and that depends on how deep the old one went — not
on its flux. The new tone's level moves 1.8 dB with the pre-state too. **The
one-number-per-cell idea (DECISIONS 2026-10-10) is dead**; stage 3 carries depth
per cell (DECISIONS 2026-10-11).

## 7. Cost

A record pass over 9000 cells × 8 layers: 0.08 s on 20 threads = **7× real
time at 38.1 cm/s**, i.e. one core would run ~140× slower than real time. The
reference is for measuring, not playing — stage 3's model is what plays.

## 8. The stage 3 record tables (`mtref --tables`, `Source/stock/stock_456.h`)

Four real-time layers at 1.75 / 5.25 / 8.75 / 12.25 µm. Per layer:

- **G[A][M_old][s]** — new remanence for a peak bias amplitude A at the layer
  (28 values, 0.1–8 H_c), a previous anhysteretic remanence M_old (11 values) and
  a deep-gap signal field s held over the pass (41 values, dense near 0). At
  0 VU and DC the four-layer table sums to 246 nWb/m rms against the full
  1 kHz readout's 250 (−0.13 dB).
- **the recording zone** from a 1.2 µm Gaussian pulse at −19 dB, read back minus
  the same cells with no pulse (the bias-phase ripple cancels exactly), pulse
  width deconvolved. At operating bias:

| layer | A | centroid downstream | rms | gain dM/ds | table slope | Gaussian predicts at 20 kHz | measured |
|---|---|---|---|---|---|---|---|
| 0 (1.75 µm) | 2.34 | 5.6 µm | 0.45 µm | 1.07 | 1.18 | −0.10 dB | −0.09 |
| 1 (5.25) | 1.45 | 4.5 | 1.12 | 1.41 | 1.41 | −0.59 | −0.50 |
| 2 (8.75) | 1.00 | 2.9 | 1.45 | 0.49 | 0.51 | −1.00 | −1.83 |
| 3 (12.25) | 0.75 | 2.2 | 1.37 | 0.002 | 0.002 | −0.89 | −0.31 |

- **RESP_DB[bias][layer][f]** — the per-layer small-signal response at 0.7×, 1×
  and 1.4× operating bias, the direct validation target.

What this says: a Gaussian kernel with the measured centroid and rms reproduces
the recording loss of a properly biased layer; at the recording threshold
(layer 2) it under-predicts by 0.8 dB at 20 kHz on a layer that carries 15 % of
the gain. **The per-layer centroids differ by up to 3 µm, which is 57° of phase
at 20 kHz** — the depth sum's −3 dB in section 4 is largely layers cancelling,
not any one layer's width, and a model that keeps a zone position per layer
gets that for free.

Not in the tables: speed. The kernel is geometry (µm) and the bias extrema fall
every 1.27 µm at 38.1 cm/s, 2.5 µm at 76 cm/s; if the stage 3 tolerance test at
76 cm/s misses, the tables grow a speed axis.

The first probe (a 0.5 µm pulse at −45 dB) returned negative gains and an
−811 µm centroid: shorter than one bias half-cycle, so the process could not
sample it, and the bias-phase ripple of each cell swamped it. A probe can lie
before the model does — the table slope was the independent check that caught it.
