# MARMALADE 120 — design

The Orange OR120 Graphic MkII of 1972, simulated as one circuit, sample by
sample: the 50 Hz mains at the power transformer, the bridge rectifier, every
filter and dropper, both ECC83s, all four EL34s, the output transformer and
the speaker's electrical load. No waveshaper, no curve, no made-up response
anywhere. The name and panel are its own, so it carries no one's trademark
(VOICE is the six-way FAC switch).

**Source.** The three 1972 sheets (preamp, output amp, power supply) as
redrawn by B. Hickmott in 2004. Every part on them is in the netlist with the
value printed there.

**The rule: accuracy over voicing.** No value is chosen for how it sounds.
Each netlist line says where its value comes from. What a schematic never
gives (the transformers, the choke, the speaker) is estimated and marked as
such. Where the model and a documented figure disagree, the bench reports it
(`[KNOWN]`) instead of bending a value to agree.

Plugin code `Mm12`. PRODUCT_NAME "Marmalade 120". JUCE 8 VST3 + Standalone,
`IS_SYNTH FALSE`, category `Fx` / `Distortion`. Native-first: the APVTS is the
single source of truth and `Source/ui/ui.html` is a pure view (the Hairfryer
and Battlestar pattern). **No BWFX.**

```
plugin/Source/Circuit.{h,cpp}    netlist -> MNA -> DK-method solver (knows nothing about amps)
plugin/Source/Or120Netlist.h     the amplifier, mains to speaker, provenance on every line
plugin/Source/Engine.{h,cpp}     oversampling, knobs -> parts, the mains, the warm-up
plugin/test/bench.cpp            mmtest: 24 checks on rendered audio, plus [KNOWN] notes
plugin/test/spicecheck.cpp       the whole amp against ngspice, from the same warmed-up state
plugin/test/render.cpp           mmrender: a WAV through the engine (and an IR), to listen
plugin/test/biasset.cpp          mmbias: finds the bias winding for a target idle current
plugin/test/el34fit/             the Mullard EL34 sheet digitised, and the fit
plugin/test/host/                mmhost: loads the built VST3 the way a DAW does
```

---

## 1. The circuit, from the sheets

### Preamp

- **V1a.** One input: 68k into the grid, 1M grid leak, 220k plate, 2k2
  cathode with 50 µ.
- **Bass and treble, between the stages.** 68n couples V1a into a Baxandall
  with **1M log** pots:
  - bass: 100k, the pot bridged by 2n2 (upper part) and 22n (lower part),
    then 22k to ground;
  - treble: 1n5, the pot, 10n to ground;
  - 100k from the bass wiper to the treble wiper, which is the output.
- **GAIN.** 1M log, straight off the tone stack, with a **330p bright cap**
  across its upper part.
- **V1b.** As V1a, then 68n into **FAC**: a series chain 4n7 – 4n7 – 2n2 –
  1n – 330p that the six-way switch taps. Each position is the chain's series
  value up to its tap: 68n, 4.40n, 2.27n, 1.12n, 0.53n, 0.20n.

### Output amp

- **Input.** The preamp reaches the driver's grid through 100k, and through
  the reverb send and return (100k + 100k), normalled when nothing is plugged
  in. The grid leak is 220k.
- **Driver.** 100k plate from C+ with 1n to C+, 1k5 cathode.
- **Feedback.** From the 16 Ω tap, through 15k, into the driver's cathode.
- **BOOST.** A 1k lin rheostat, a 2 mH choke and 0.47 µ from the driver's
  cathode to ground: a resonant bypass at 5.2 kHz, which also shunts the
  feedback there.
- **Cathodyne, AC-coupled.** 68n in, 100k plate, 1k5 bias, 100k to ground,
  and a 1M grid leak bootstrapped to the 1k5/100k junction.
- **To the output valves.** Each output goes through 68n, onto a 220k bias
  feed from N.B, then 2k4 per valve. **There is no master volume.**
- **Four EL34s.** 1k per screen from B+, cathodes grounded.
- **Output transformer.** Centre tap on A+; 4/8/16 Ω taps; the speaker on 8.

### Power supply

- **HT.** A bridge of 8 × 1N4005 (two in series per arm), then:
  - A+: 2 × 100 µ in series (50 µ);
  - a choke to B+: 2 × 32 µ in series (16 µ), with a 2 × 100k bleeder;
  - 33k 2 W to C+ (16 µ), and 33k 2 W to D+ (16 µ).
- **Bias.** A centre-tapped winding, two 1N4005s, 22 µ – 1k – 22 µ, and a 22k
  load. **No adjustment on the sheet.**

### Interpretations

Where the sheet leaves a choice, the netlist says which was taken:

- **The input jacks.** The redraw itself says their wiring is uncertain. One
  input is modelled, and the other jack's 68k is left open.
- **The send/return normalling.** The jack switching is drawn but not
  labelled.
- **FAC order.** The redraw says it may be drawn from the switching side. The
  drawn order is used: position 1 full, 6 thin.

### What the schematic does not say

**Measured and published** (`[meas]`, `[rep]`, `[doc]`):

- **Output transformer windings**, measured on an OR120:
  - primary halves 14.7 Ω (orange-black, outer) and 11.9 Ω (red-black,
    inner);
  - secondary sections 1.5 Ω and 1.3 Ω, taken as about 0.7 Ω at the 8 Ω
    tap. How the sections combine isn't given, and which half feeds which
    plate is an interpretation.
- **Output transformer impedance**: 1800 Ω plate to plate (the replica
  transformers' spec: Tube-Town, Mercury Magnetics), so 15 : 1 to the 8 Ω
  tap.
- **B+ 490 – 500 V** in the original amps. `mmbias` sets the HT winding for
  495 V, which comes out at 369.5 V rms.

**A cross-check nothing was tuned to.** With B+ on its documented value and
the sheet's 33k droppers, the other two rails land by themselves at C+
376.5 V and D+ 332.2 V, against the ~370 V and ~330 V of the '74 drawing.

**Set as a technician would** (`[est]`):

- **Bias winding: 33.6 V rms each half.** There's no pot on the amp, so the
  winding is the bias. It was set by `mmbias` for 35 mA a valve at idle
  (−43.7 V).

**Still unknown** (`[est]`):

- the output transformer's leakage, magnetising inductance and core loss;
- the choke (10 H, 120 Ω);
  - the windings' resistance, leakage and capacitance to the core;
  - the 4×12's electrical model;
  - the BOOST choke's resistance.

| Node (idle, averaged over the ripple) | Simulated |
|---|---|
| A+ (OT centre tap) | 495.7 V |
| B+ / C+ / D+ | 492.8 / 376.5 / 332.2 V |
| V1a plate (220k) | 184.5 V |
| driver plate (100k) | 247.8 V |
| cathodyne plate–cathode | 195.0 V |
| EL34 idle, per valve | 35.0 mA at −43.7 V, 17.3 W |

---

## 2. The solver

`Circuit` parses a SPICE-like netlist and stamps it into modified nodal
analysis. Capacitors and inductors are discretised with a θ-method companion
model (θ = 0.55), and the system is reduced to the nonlinear state-space form
of the DK method (Yeh; Holters & Zölzer):

```
p  = D x + E u          what the devices see of the linear circuit
v  = p + K i(v)         Newton, only as wide as the device ports
x' = A x + B u + C i    the states
y  = Do x + Eo u + Fo i
```

**Size.** One system. Its 20 nonlinear ports are 4 triodes × 2, 2 EL34 pairs
× 3, and 6 diodes. Moving a pot rebuilds the matrices; the states carry over.

**Devices:**

- **Triodes**: Dempwolf & Zölzer's model (DAFx 2011), fitted to *measured*
  ECC83 curves. Cathode current is shared between plate and grid,
  `Ip = Ik − Ig`.
- **EL34**: fitted here to the **Mullard EL34 data sheet** (1960/64).
  - **Data**: pages C1–C3 were digitised at 300 dpi and checked against the
    sheet's own tabulated points.
  - **Model**: built like the triode. The grid and screen set a cathode
    current, and the plate's knee divides it between plate and screen, so the
    two always add up to the cathode current. The plate's share never passes
    1, so the screen can't source current at the 1 kV plate swings this amp
    makes.
  - **Accuracy**: 10.4% RMS over every point at 10 mA and up, against 35% for
    Koren's published fit (whose screen current was ~60% low).
  - **Grid current**: the sheet's onset (+0.3 µA at −1.3 V) sets its
    softness; its size at a positive grid is still estimated.
  - **Reproducing it**: `test/el34fit/`.
- **Diodes**: the published 1N400x Shockley parameters; two in series is the
  same diode with n doubled.
- **Every interelectrode capacitance**, from the datasheets.

**Warm-up.** The rails' slowest filter is about a second, so 5 s of mains run
at 16 kHz. The solver then **changes rate exactly**: every capacitor's
voltage and current and every inductor's current and voltage carry across,
because the θ-method state is `x = Gv + βi` (capacitor) or `x = i + βGv`
(inductor) and only G depends on the rate. A last 0.1 s settles at the real
rate.

### Things the bench and spicecheck found, and what was done

- **Kilovolt ringing on per-plate leakage.** Fix: leakage is referred to the
  OT secondary.
- **Trapezoidal ringing at Nyquist on stiff switching.** Fix: θ = 0.55.
- **Koren's pentode conducts backwards** below 0 V on the plate. Fix: the
  plate voltage goes through a smooth floor first.
- **Real 900 V edges in one sample.** Steps are capped at 200 V, damped by a
  line search on the residual, and re-solved by continuation in `p` if Newton
  still fails. Continuation is judged by its final sub-step only.
- **The predictor.** Last sample's factored Jacobian predicts the step for the
  price of one back-substitution. Newton averages about 1–2 iterations per
  sample.
- **A NaN residual read as converged**, via `std::max (0, NaN)`.
- **The floating HT winding stalled Newton.** With the bridge off, its common
  mode hung on diode leakage alone. Fix: the winding's capacitance to the
  grounded core, 1 nF each end. That is physically real, and it cut Newton
  from 3.6 iterations per sample to 0.2 at idle.
- **The diode's overflow guard was a fixed 1.2 V knee.** That is harmless for
  one diode, but it capped each two-diode bridge arm at ~36 Ω and starved the
  reservoir. Fix: the knee is set at 1 kA instead. spicecheck caught it.
- **Nested stateful calls aliased HIGH.** `down (dn2.down(a,b), dn2.down(c,d))`
  leaves the argument order unspecified, and GCC ran the second call first.
  Fix: two statements.
- **Real hum looks like aliasing.** So aliasing is measured with the mains
  held at DC (`Params::mains`, bench only).
- **A one-row offset.** With `uic`, ngspice's first row is t = 1/fs. Rows are
  aligned by timestamp.

## 3. Verified against ngspice

`spicecheck` runs the whole amp, mains to speaker, in both solvers:

1. The solver warms the amp up exactly as the plugin does.
2. It hands ngspice its state: every node voltage, every inductor current,
   and the mains at the same phase.
3. Both run the same 60 ms. ngspice's step is capped at an eighth of the
   solver's sample, with the device equations term for term as B-sources and
   ngspice's own diode.

The solver runs at θ = 0.55:

| Case | Rate | Speaker | B+ ripple |
|---|---|---|---|
| idle: hum and ripple alone | 96k | 2.9 % ¹ | 0.09 % |
| clean, 100 mV | 96k | 0.88 % | 0.07 % |
| crunch, gain 3 o'clock, 300 mV | 96k | 1.3 % | 0.06 % |
| tone stack at its extremes, bright | 96k | 6.2 % | 0.20 % |
| … the same | 192k | 1.9 % | 0.10 % |
| flat out: gain and BOOST full | 96k | 2.5 % | 0.10 % |
| … the same | 192k | 0.69 % | 0.03 % |

¹ A few millivolts of hum, near ngspice's own precision on 500 V nodes.

NORMAL (96k) is within ~1.3% for normal playing and reaches 6% on the
brightest hard clipping. HIGH (192k) cuts that to about a third: the error
converges with rate.

## 4. Around the circuit (Engine)

- **Oversampling.** The circuit runs at ≥ 88.2 kHz on NORMAL (2× at
  44.1/48 kHz) and ≥ 176.4 kHz on HIGH (4×). Latency: 31 and 47 samples.
- **Input.** 0 dBFS is 1 V peak at the jack, with a 5 Hz DC block.
- **Knobs** become parts:
  - GAIN, BASS and TREBLE are log (10% at noon), counter-clockwise end on the
    cut side, so the tone stack is not flat at noon, as the real one isn't;
  - BOOST is the 1k lin rheostat;
  - FAC is the chain's six series values.
- **The mains**: a 240 V, 50 Hz sine into the primary; the windings are
  ratios in the netlist.
- **No master.** The power amp always gets what the preamp gives it. The
  OUTPUT trim (−24…+12 dB, default −6) is digital, after the amp.
- **CAB is a measured impulse response or nothing.** LOAD IR, unit-energy,
  convolved after the amp; `mmrender ir=…` does exactly the same. Without an
  IR the output is the 8 Ω tap.

## 5. Measured (mmtest)

- **Rated power: 109 W at 10% THD**, against the documented 120 W (−9%). The
  figure has moved as the data has:
  - 194 W on the reconstructed values;
  - 154 W with the schematic's values and Koren's EL34;
  - 126 W with the EL34 fitted to Mullard's sheet;
  - 109 W with the measured output transformer and the documented B+.
- **Sag**: 55 V flat out, 0.2 V at bedroom level.
- **Hum** at idle, every knob up: −100 dBFS at 100 Hz.
- **GAIN** takes THD from −54 dB to −9 dB.
- **The controls**, as frequency response, with a 10 mV sine:
  - BASS: 80 Hz re 1 kHz goes −14.8 → −3.1 dB;
  - TREBLE: 5 kHz re 1 kHz goes −0.2 → +13.5 dB;
  - VOICE 1 → 6: 150 Hz re 3 kHz goes −18 → −40 dB;
  - BOOST: 5.2 kHz re 1 kHz goes +11.9 → +20.2 dB.
- **Aliasing** of a clipped A6 at the raw tap: −37 dB on NORMAL, −48 dB on
  HIGH.
- **Cost** on the build container's slow, shared core: about 90–110% of one
  core on NORMAL and 170–210% on HIGH, per instance. Twenty nonlinear ports,
  six of them rectifier diodes, is the price of the real supply. A modern
  desktop core is several times faster, but on this machine NORMAL is only
  just real-time.

## 6. Not modelled (yet)

- Output-transformer core saturation and hysteresis: the core is linear, with
  loss.
- The speaker as a measured driver, and any acoustics (an IR does that).
- Valve noise and microphony; mismatched output valves; ageing.
- Heater-cathode leakage hum (the heaters are not in the circuit).
- The second input and the reverb loop with something plugged in.

## 7. What would make it more accurate, in order

1. **What is left of the transformers, measured**:
   - the output transformer's leakage (LCR meter on the primary, secondary
     shorted) and magnetising inductance (secondary open);
   - the HT secondary's resistance and the bias winding's voltage;
   - the choke's inductance and resistance.
2. **EL34 grid current at a positive grid**, from a curve tracer (uTracer).
   It is the one part of the valve not on the sheet.
3. **A measured IR** of the cabinet and mic you want, loaded in CAB.
