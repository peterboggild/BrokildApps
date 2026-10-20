# MARMALADE 120 — design

A 120 W British valve head of the early-seventies "pictures, not words"
school, simulated as a circuit: every resistor, capacitor, inductor, both
ECC83s, all four EL34s, the output transformer, the speaker's electrical
impedance and the power supply - from the 50 Hz mains at the transformer to
the speaker terminals - are one circuit, solved sample by sample. No
waveshaper, no curve, no made-up response anywhere. The circuit is an
OR120-style topology.

**The rule: accuracy over voicing.** No value is chosen for how it sounds.
Each part is either documented, from a datasheet or published model, or an
estimate (typical of the period, landing the documented voltages), and the
netlist says which on every line. Where the model and a documented figure
disagree, the bench says so (`[KNOWN]`) rather than a value being bent to
agree. The name, panel and control names are its own, so it carries no
one's trademark (VOICE is the six-way capacitor switch, BITE is the
high-frequency drive network).

Plugin code `Mm12`. PRODUCT_NAME "Marmalade 120". JUCE 8 VST3 + Standalone,
`IS_SYNTH FALSE`, category `Fx` / `Distortion`. Native-first: the APVTS is the
single source of truth and `Source/ui/ui.html` is a pure view (the Hairfryer
and Battlestar pattern). **No BWFX.** This is an amplifier, and a rack of
effects after it would be the one part that is not a circuit.

```
plugin/Source/Circuit.{h,cpp}    netlist -> MNA -> DK-method solver (knows nothing about amps)
plugin/Source/Or120Netlist.h     the amplifier, mains to speaker, as one netlist, provenance per line
plugin/Source/Engine.{h,cpp}     oversampling, knobs -> parts, the mains, the warm-up
plugin/test/bench.cpp            mmtest: 25 checks on rendered audio, plus [KNOWN] discrepancies
plugin/test/spicecheck.cpp       the whole amp against ngspice, from the same warmed-up state
plugin/test/render.cpp           mmrender: a WAV through the engine, to listen
plugin/test/host/                mmhost: loads the built VST3 the way a DAW does
```

---

## 1. The circuit

The topology is the documented one, not a guess:

- **V1a**, first gain stage (ECC83), then **GAIN** (1M log) between the first
  two stages.
- **V1b**, second gain stage.
- **VOICE**, a six-way switch picking one of six coupling capacitors from V1b
  into the tone stack (47n, 22n, 10n, 4n7, 2n2, 1n). It moves the low cut from
  full to thin, which is what reads as a mid control.
- **Passive Baxandall** bass and treble, 1M linear pots.
- **BITE**, a treble path (470p + 1M rheostat) around the series loss between
  the tone stack and the third stage, so it raises both brightness and drive.
- **V2a**, third stage and driver, **DC-coupled** into a **cathodyne**
  (split-load) phase inverter, **V2b**. The cathodyne is the signature: it
  clips lopsidedly, and it only just has the swing to drive EL34s.
- **MASTER**, a 220k log dual gang after the inverter, with its far end on the
  bias supply.
- **Four EL34s**, fixed bias, solved as two matched pairs (exact for matched
  valves).
- **Output transformer**, 1k7 plate to plate into 8 Ω, centre tapped.
- A **4×12**, as the amplifier sees it: voice coil resistance and inductance
  (with eddy loss), and the cone's mechanical resonance (~90 Hz) reflected as
  a parallel RLC. Generic values [est]: no driver's published parameters
  were reachable.
- **The supply, from the mains**: a 375-0-375 V HT winding (each half behind
  its resistance and leakage inductance) into two 1N4007s and a 100 µF
  reservoir, then the screen, inverter and preamp rails down an RC ladder. The
  bias comes from its own 50 V winding, a diode, two filter stages and the
  set-bias divider. Ripple, sag and hum are what this circuit does, not
  effects.
- Light **global negative feedback** from the 8 Ω tap into the driver's cathode.

### Provenance

Every part line in `Or120Netlist.h` is tagged:

- `[doc]`: stated by a source. Topology; ECC83 ×2 and EL34 ×4; fixed bias;
  solid-state rectification; the DC-coupled cathodyne; six FAC positions;
  Baxandall bass/treble; HF Drive between stages 2 and 3; ~330 V and ~370 V
  rails; post-inverter master on the 1998/2001 sheets; 2k2 grid resistors on
  the '74 sheet.
- `[ds]`: from a datasheet or published model. Valves, diodes, capacitances.
- `[est]`: everything else, and that is most of the values.

The schematic archives (el34world, schematicheaven), the datasheet hosts and
the speaker maker's site were all blocked by the network policy of the
environment this was built in.

Two estimates were set *by measurement*, as a technician would set them, not
by ear:

- **The set-bias divider**: set for 35 mA a valve at idle (70% of 25 W), by
  bisection on the warmed-up amp (`test/biasset` in the history; −48.7 V).
- **The two rail droppers (36k, 18k)**: set to land the documented ~370 V and
  ~330 V rails (364.9 V, 326.7 V).

One estimate has a story: the driver's plate resistor (470k). A forum
recollection has 390k as a *raise* from stock, made to give the DC-coupled
cathodyne room. At 330k it had 66 V to swing and its plate side clipped at
14 W; at 470k it has 112 V. The real value is the first thing to read off a
schematic.

| Node | Target | Simulated (averaged over the ripple) |
|---|---|---|
| preamp rail | ~330 V [doc] | 326.7 V |
| inverter rail | ~370 V [doc] | 364.9 V |
| B+ at the OT | 480 – 530 V | 510.1 V |
| V1a plate | 180 – 260 V | 219.9 V |
| driver plate = cathodyne grid | low enough to swing | 125.7 V |
| cathodyne plate–cathode | room to swing | 112.5 V |
| EL34 idle, per valve | set to 35 mA | 35.1 mA at −48.7 V, 17.8 W |

---

## 2. The solver

`Circuit` parses a SPICE-like netlist and stamps it into modified nodal
analysis. Capacitors and inductors are discretised with a θ-method companion
model, and the system is reduced to the nonlinear state-space form of the DK
method (Yeh; Holters & Zölzer):

```
p  = D x + E u          what the tubes see of the linear circuit
v  = p + K i(v)         Newton, only as wide as the tube ports
x' = A x + B u + C i    the states
y  = Do x + Eo u + Fo i
```

The amplifier is ONE system: 55 nodes, 45 states, 17 nonlinear ports
(4 triodes × 2, 2 EL34 pairs × 3, 3 diodes). Moving a pot rebuilds the
matrices (one LU of the 61×61 MNA system); the states carry over.

**Devices**, each with its source:

- **Triodes**: Dempwolf & Zölzer's model (DAFx 2011), fitted by its authors
  to *measured* ECC83 curves, grid current included:
  - cathode current `Ik = G (softplus(C(Vpk/μ + Vgk))/C)^γ`;
  - grid current `Ig = Gg (softplus(Cg·Vgk)/Cg)^ξ + Ig0`;
  - the plate gets what the grid does not: `Ip = Ik − Ig`.

  This replaced Koren's triode, which is a fit to datasheet curves and treats
  plate and grid current as unrelated.
- **EL34**: Koren's published fit. Its grid current is Dempwolf-form with
  estimated parameters, because no published EL34 grid-current fit was
  reachable.
- **Diodes**: Shockley, with the published 1N4007 SPICE parameters.
- **All interelectrode capacitances**, from the sheets: ECC83 1.6 / 1.7 /
  0.46 pF; EL34 15.2 / 1.1 / 8.4 pF.

**Warm-up.** The rails' slowest filter is about a second, so a cold amp takes
seconds to come up. That warm-up runs at 16 kHz: 5 s of mains, a fraction of
a second of CPU. The solver then **changes rate exactly**: every capacitor
keeps its voltage and current, every inductor its current and voltage. The
θ-method state at a given rate is `x = Gv + βi` (capacitor) or `x = i + βGv`
(inductor), and only G depends on the rate. A last 0.1 s settles at the real
rate.

### Things the bench found, and what was done

- **Per-plate leakage inductance rang at a kilovolt.** With leakage in series
  with each plate, a valve cutting off fully forced the leakage current into
  a few pF. Fix: leakage is referred to the secondary, because the two halves
  of a real OT are wound together and tightly coupled.
- **The trapezoidal rule rings at Nyquist on stiff switching.** ngspice's own
  trapezoidal rule refused the same circuit, and only its Gear method ran it.
  Fix: θ = 0.55. Nyquist ringing then decays by a factor of 0.82 per sample,
  and the audio band stays within a fraction of a percent of the trapezoidal
  rule (see the spicecheck figures below).
- **Koren's pentode conducts backwards.** Its `atan(Vpk)` is odd, so a plate
  driven below ground (which the transformer does at full drive) pulled
  reverse current as hard as forward. Fix: the plate voltage first goes
  through a smooth floor (`2·softplus(v/2)`), with the same change made in the
  ngspice export. With that fix in, ngspice's trapezoidal rule ran the
  saturated case too.
- **Newton has to cross a real 900 V edge in one sample.** At full drive the
  plates swing that far in microseconds. ngspice gets through by cutting its
  timestep; a plugin cannot. Three measures:
  1. Steps are capped at 200 V.
  2. A step is damped by a line search on the residual.
  3. If Newton still fails, the sample is re-solved by **continuation**:
     `p` is walked from last sample's value to this one in 4, 32 or 256
     sub-steps. With the states fixed, the port voltages are a continuous
     function of `p`, so this is the same answer reached along the same
     branch.

  Continuation is judged by its final sub-step only. Intermediate sub-steps
  that stopped a hair short of tolerance were the last "failures" left, with
  final residuals under 1 µV. The bench runs 40 random settings with
  hard-picked notes and records 0 Newton failures.
- **The predictor is most of the speed.** Before each solve, the last
  factored Jacobian predicts the step, `v += (I − KJ)⁻¹ Δp`, at the cost of
  one back-substitution. That cut Newton from about 2.9 to about 1.1
  iterations per sample.
- **A NaN residual read as "converged"**, because `std::max(0, NaN)` returns 0.
  The residual norm is now written so that NaN fails.
- **Nested stateful calls aliased HIGH.** `dn1.down (dn2.down (a, b),
  dn2.down (c, d))` lets the compiler run the second `dn2.down` first, since
  argument order is unspecified, and GCC did. Every 4× block was folded back
  on itself, and HIGH measured *worse* than NORMAL. The fix is two statements.
- **Real hum looks like aliasing.** The rectifier's 100 Hz harmonics mix with
  a note onto the same frequency grid its aliases fall on. So aliasing is
  measured with the mains held at DC (`Params::mains`, bench only).

---

## 3. Verified against ngspice

`spicecheck` runs the whole amp, mains to speaker, in both solvers:

1. The solver warms the amp up exactly as the plugin does.
2. It hands ngspice its state as initial conditions: every node voltage,
   every inductor current, and the mains at the same phase.
3. Both then run the same 60 ms. ngspice's step is capped at an eighth of the
   solver's sample, with the device equations term for term as B-sources and
   ngspice's own diode. Rows are aligned by timestamp; with `uic`, ngspice's
   first row is t = 1/fs, and indexing rows instead read as a one-sample
   offset.

The solver runs at the plugin's θ = 0.55:

| Case | Rate | Speaker | B+ ripple |
|---|---|---|---|
| idle: hum and ripple alone | 96k | 1.5 % ¹ | 0.09 % |
| clean, 100 mV | 96k | 0.16 % | 0.06 % |
| crunch, gain 3 o'clock, 300 mV | 96k | 0.95 % | 0.06 % |
| tone stack at its extremes | 96k | 3.6 % | 0.20 % |
| … the same | 192k | 1.3 % | 0.08 % |
| flat out, 500 mV | 96k | 2.7 % | 0.09 % |
| … the same | 192k | 0.82 % | 0.03 % |

¹ About 0.4 mV of hum on the speaker, which is below ngspice's own precision
on 500 V nodes.

NORMAL (96k) is within 1% for normal playing and reaches 3–4% on bright, hard
clipping. HIGH (192k) is within about 1.3% everywhere.

## 4. Around the circuit (Engine)

- **Oversampling.** The circuit runs at ≥ 88.2 kHz on NORMAL (2× at
  44.1/48 kHz) and ≥ 176.4 kHz on HIGH (4×), using Battlestar's 63-tap
  half-bands. Latency is reported: 31 samples on NORMAL, 47 on HIGH.
- **Input.** 0 dBFS is 1 V peak at the jack. A hard-strummed humbucker peaks
  around there, and a DI recorded with headroom lands at 250 – 500 mV. A 5 Hz
  DC block sits in front, because the real jack is DC coupled to the grid.
- **Knobs** become parts: log pots follow 10% at half travel, a pot's ends are
  100 Ω, and the circuit is rebuilt only when a value really moves (smoothed
  over about 25 ms, checked every 32 samples).
- **The mains**: 50 Hz sines into the HT and bias windings at the oversampled
  rate, phase-continuous across blocks.
- **CAB is a measured impulse response, or nothing.** The plugin's LOAD IR
  loads a WAV/AIFF/FLAC (stereo is summed), scales it to unit energy and
  convolves it after the amp; `mmrender ir=…` does the same, exactly. Without
  an IR the output is the 8 Ω tap. An earlier version modelled the cone and a
  close mic with a fixed EQ chosen by numbers; it was removed, because it was
  the one made-up response in the plugin.
- **Safety.** If the output is ever non-finite or above 2 kV, the amp is
  "switched off and on" (re-solved to its operating point) and the block is
  muted. The bench's sweeps never trigger it.

## 5. Measured (mmtest)

- **[KNOWN] Rated power: 194 W at 10% THD, against the documented 120 W.**
  The output stage's estimated values (screen supply, OT ratio, B+) are the
  suspects. It is reported, not fixed by bending them.
- **Sag**: 43 V flat out, 0.2 V at bedroom level.
- **Hum** at idle with every knob up: −114 dBFS at 100 Hz.
- GAIN takes THD from −52 dB to −4 dB. Asymmetric clipping leaves a strong
  second harmonic (−7 dBc).
- TREBLE, BASS, VOICE, BITE and MASTER each move what they should:
  - centroids: TREBLE 323 → 6284 Hz, BASS 5244 → 2487 Hz,
    VOICE 4168 → 5410 Hz, BITE 4035 → 5811 Hz;
  - MASTER: 28 dB of level.
- **Aliasing** of a clipped A6 at the raw 8 Ω tap (no cabinet to hide it):
  −34 dB on NORMAL and −49 dB on HIGH, measured on the alias grid. At a
  96 kHz host it falls −34 → −49 → −57 dB as the circuit runs at 96, 192 and
  384 kHz. A cabinet IR's fall past 5 kHz removes most of what is left.
- **Cost** on the build container's core (slow, and shared): 60 – 90% of one
  core on NORMAL and 115 – 160% on HIGH, per instance, at 48 kHz. On this
  machine HIGH is for bouncing.

## 6. Not modelled (yet)

- Output-transformer core saturation and hysteresis: the core is linear, with
  loss. Needs the transformer's core data.
- The speaker as a measured driver: its electrical model is generic, and
  there is no acoustic model at all (an IR does that).
- Valve noise and microphony; mismatched output valves; valve ageing.
- Heaters: no heater-cathode leakage hum.

## 7. What would make it more accurate, in order

1. **The OR120 service sheet** (any year, says which), so every `[est]`
   becomes a `[doc]`. It will also settle the 194 W.
2. **The real transformers' data**: HT secondary voltage and resistance, OT
   primary impedance, leakage and core. Measured on an amp if not published.
3. **EL34 curves**, to fit the pentode and its grid current rather than use
   Koren's fit and estimates.
4. **A measured IR** of the cabinet and mic you want, loaded in CAB.
