# 03 — Hysteresis of the coating: the Preisach model

## Why Preisach and not Jiles–Atherton

A tape loop with FLUX MEMORY is rewritten thousands of times, every pass a minor
loop driven by a decaying AC bias. Jiles–Atherton's minor loops do not close:
cycling the field between two fixed values drifts the magnetisation a little each
time, and ten thousand passes of a non-physical drift are not a tape, they are a
leak. The classical scalar Preisach model has the two properties this instrument
needs by construction: the **wiping-out property** (a larger excursion erases the
memory of the smaller ones before it) and the **congruency property** (every
minor loop between the same two fields has the same shape, so it closes exactly).
ChowTape chose JA because it never needs minor-loop closure; we do.

## The model

The coating is an ensemble of elementary bistable particles ("hysterons"), each a
rectangular loop switching up at a field α and down at β ≤ α. Equivalently each
has a coercivity h_c = (α − β)/2 ≥ 0 and an interaction (offset) field
h_u = (α + β)/2. We use the factorised density

    μ(h_c, h_u) = p_c(h_c) · p_u(h_u) / 2

- p_c lognormal with median H_c (the bulk coercivity — proved below) and log-width σ_c;
- p_u Gaussian, zero mean, width σ_u.
- The factor ½ is the Jacobian of (α, β) → (h_c, h_u); total mass 1.

Fields are normalised to the bulk coercivity (H_c = 1) throughout the reference.

The magnetisation is M/M_s = ∫∫ μ · s(α, β) dα dβ with s = ±1 the state of each
hysteron, and the state is a staircase in the (α, β) half-plane: the alternating
sequence of past field extrema that have not been wiped out,
M_1 > M_2 > … (maxima), m_1 < m_2 < … (minima), M_k > m_k. With the **Everett
function**

    F(α, β) = ∫∫_{β ≤ β' ≤ α' ≤ α} μ dα' dβ'
            = ∫_0^{(α−β)/2} p_c(c) [Φ_u(α − c) − Φ_u(β + c)] dc        (Φ_u the Gaussian CDF)

the magnetisation from negative saturation is

    M/M_s = −1 + 2 Σ_k [F(M_k, m_{k−1}) − F(M_k, m_k)] + (the open branch)

where m_0 = −∞ and the open branch adds 2F(h, m_n) when the field h is rising
from the last minimum, or 2[F(M_n, m_{n−1}) − F(M_n, h)] when falling from the
last maximum. F(x, x) = 0, so a staircase corner on the diagonal costs nothing.

Update rule for a new field h (the whole model is this rule plus F):
- rising from the last minimum: pop every pair whose maximum M_k ≤ h (wiped out),
  then h is the current field; a reversal (h below the current field) first
  pushes the current field as a new maximum;
- falling: mirror image, popping pairs whose minimum m_k ≥ h.

Because the state is a list of extrema, feeding the rule a point that is NOT an
extremum is harmless (it is absorbed); only MISSING an extremum changes the result.
That is what makes the record process cheap: only the turning points of the field
at a tape cell need computing, one per bias half-cycle (04-record-process.md).

## Closed-form checks the model must pass

With this density, from the symmetry of p_u:

- **Bulk coercivity** (descending major loop crosses zero): 1 − 2F(∞, H) = 0 at
  P(h_c − h_u < H) = ½, i.e. at the median of h_c, so H_c = median(p_c) exactly.
- **Squareness** S = M_r/M_s = 1 − 2 P(h_u > h_c): hysterons whose offset exceeds
  their coercivity switch back when the field is removed. S fixes σ_u.
- **SFD**: the width of the derivative of the DC-demagnetisation remanence curve
  M_r(H) = −1 + 2[F(H, −∞) − F(H, 0)], relative to the remanent coercivity. Set
  mainly by σ_c.
- **Anhysteretic curve** (a decaying AC field of amplitude A → 0 about a small
  steady field s, the idealised AC-bias record process): every hysteron with
  |h_u − s| < h_c ends up if h_u < s and down if h_u > s; those outside that band
  end in the direction of their offset. So

    M_anh(s)/M_s = 2 Φ_u(s) − 1 = erf(s / (σ_u √2))

  independent of p_c: **the anhysteretic curve is the integrated interaction-field
  distribution**, and its third-harmonic distortion at small signal is
  HD3 ≈ (s/σ_u)²/12. At 0 VU on the stock below s/σ_u ≈ 0.12 gives 0.13 % — the
  right order against the data sheet's 0.1 %, before any tuning. The reference
  measures the real process (finite decay rate, moving zone) against this ideal.
- **Minor-loop closure**: 10 000 cycles between two fixed fields return the same
  M to rounding. (Trivial for Preisach, the whole point against JA; printed anyway.)

## The stock

Ampex/Quantegy 456 (the "1970s high-output studio" default) from its data sheet:
coercivity 330 Oe → H_c = 26.3 kA/m; retentivity 1460 G → B_r = 0.146 T →
M_r = 116 kA/m; oxide 0.55 mil → δ = 14 µm; MOL +12.3 dB re 185 nWb/m at 15 ips;
third harmonic 0.1 % at 185 nWb/m. Not on the sheet, taken as typical of oriented
acicular γ-Fe₂O₃ and stated as assumptions: squareness 0.80 (so M_s = M_r/S =
145 kA/m), SFD 0.35. σ_c and σ_u are FITTED to those two, and MOL and the 0.1 %
are then predictions to be compared.

Full-remanence surface flux: Φ = μ0 M_r δ = 2040 nWb/m; the 250 nWb/m rms
reference sine peaks at 17 % of it, MOL (760 rms) at 53 %.

## Not modelled, said plainly

- Vector magnetisation and the perpendicular head-field component (acicular,
  oriented particles make the longitudinal scalar model a fair first model).
- Demagnetising fields of the recorded pattern (the moving-Preisach mean field).
  Candidate if the short-wavelength results disagree with data.
- Particle count / size distribution (stage 6 builds the noise on top of this).
- Temperature, time decay, print-through (dropped by decision).

## Sources

- Mayergoyz, *Mathematical Models of Hysteresis and their Applications*, 2nd ed., Elsevier 2003 — ch. 1 (classical Preisach: wiping-out, congruency, the Everett function and the staircase).
- Della Torre, *Magnetic Hysteresis*, IEEE Press 1999 — ch. 2–4 (Preisach for particulate media, interaction-field density, the anhysteretic curve).
- Bertram, *Theory of Magnetic Recording*, Cambridge 1994 — ch. 2 (particle switching, SFD definitions).
- Jiles & Atherton, J. Magn. Magn. Mater. 61 (1986) 48 — the model NOT used, and why.
- Quantegy 456 data sheet (as quoted; the PDF would not render here).
