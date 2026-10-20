# MARMALADE 120 — design

A 120 W British valve head of the early-seventies "pictures, not words"
school, simulated as a circuit: every resistor, capacitor, inductor, both
ECC83s, all four EL34s, the output transformer, the speaker's electrical
impedance and the power supply are solved together, sample by sample. No
waveshaper, no curve, no impulse response. The circuit is an OR120-style
topology. The name, panel and control names are its own, so it carries no
one's trademark (VOICE is the six-way capacitor switch, BITE is the
high-frequency drive network).

Plugin code `Mm12`. PRODUCT_NAME "Marmalade 120". JUCE 8 VST3 + Standalone,
`IS_SYNTH FALSE`, category `Fx` / `Distortion`. Native-first: the APVTS is the
single source of truth and `Source/ui/ui.html` is a pure view (the Hairfryer
and Battlestar pattern). **No BWFX.** This is an amplifier, and a rack of
effects after it would be the one part that is not a circuit.

```
plugin/Source/Circuit.{h,cpp}    netlist -> MNA -> DK-method solver (knows nothing about amps)
plugin/Source/Or120Netlist.h     the amplifier, as two netlists in plain text
plugin/Source/Engine.{h,cpp}     oversampling, knobs -> parts, sag hand-off, cone -> mic
plugin/test/bench.cpp            mmtest: 28 checks on rendered audio
plugin/test/spicecheck.cpp       the solver against ngspice on the same netlists
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
- A **closed 4×12**, modelled as the amp sees it: voice coil resistance and
  inductance (with eddy loss), and the cone's mechanical resonance (~90 Hz)
  reflected as a parallel RLC.
- **Solid-state supply**: a source behind 80 Ω into 100 µF, then the screen,
  inverter and preamp nodes down an RC ladder.
- Light **global negative feedback** from the 8 Ω tap into the driver's cathode.

**The values are a reconstruction, not a copy of a service sheet.** The
schematic archives were not reachable from where this was built. Where a part
was not documented, it was chosen to land the operating points the circuit is
known for, and the bench checks them:

| Node | Target | Simulated |
|---|---|---|
| preamp rail | ~330 V (documented) | 335.5 V |
| inverter rail | ~370 V (documented) | 366.0 V |
| B+ at the OT | 480 – 530 V | 511.6 V |
| V1a plate | 180 – 250 V | 229.7 V |
| driver plate = cathodyne grid | low enough to swing | 134.3 V |
| cathodyne plate–cathode | room to swing | 102.8 V |
| EL34 idle, per valve | 25 – 50 mA, under 25 W | 38.7 mA, 19.7 W |

Every value is plain text in `Or120Netlist.h`, so reading a real schematic
later is a matter of editing numbers. spicecheck re-validates the solver
whatever they are.

### The cut between the two circuits

The amp is solved as two circuits. The cut is placed where the real one is
least disturbed by it:

- **PREAMP** runs from the guitar to the third stage's grid (19 nodes,
  12 states, 4 tube ports). The third stage's Miller capacitance is kept here
  as a fixed 150 pF, because against the tone stack's high source impedance it
  is the preamp's top-end roll-off.
- **POWER** covers the third stage, the cathodyne, MASTER, the EL34s, the OT,
  the speaker, the feedback and the whole supply (30 nodes, 15 states,
  10 ports). Everything that sags together is solved together.

The preamp's rail is the power section's last filter node, handed across each
sample, so the preamp sags when the output stage draws. In the power section,
the preamp's own draw is a 110k resistor.

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

The per-sample cost is a handful of small matrix products plus a 4×4 and a
10×10 Newton. Moving a pot rebuilds the matrices (one LU of the MNA system);
the states carry over.

**Tubes** use Koren's published triode and pentode equations, with
Dempwolf-style grid current. Grid current is what makes an overdriven stage
block and bias-shift rather than simply clip. The only interelectrode
capacitance kept is grid-to-plate on the triodes (Miller). The rest act far
above audio, and were measured at a third of the solver's cost.

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

  The bench runs 40 random settings with hard-picked notes and records
  0 Newton failures.
- **The predictor is most of the speed.** Before each solve, the last
  factored Jacobian predicts the step, `v += (I − KJ)⁻¹ Δp`, at the cost of
  one back-substitution. That cut Newton from about 2.9 to about 1.1
  iterations per sample.
- **A NaN residual read as "converged"**, because `std::max(0, NaN)` returns 0.
  The residual norm is now written so that NaN fails.
- **The cathodyne needs a high driver plate resistor.** With 330k the
  DC-coupled cathodyne had only 66 V to swing, and its plate side clipped at
  14 W. With 470k (a forum recollection says 390k was an improvement) it has
  103 V, the two outputs are balanced, and the amp makes its rated power.

---

## 3. Verified against ngspice

`spicecheck` exports the same parsed netlist to ngspice, with the tube
equations written term for term as B-sources, and runs ngspice at a 0.65 µs
step cap. Both are driven with the same sine, and the AC-coupled waveforms
are compared over 40 ms:

| Case | Solver | Error |
|---|---|---|
| preamp, gain 3 o'clock, 100 mV | trapezoidal, 192k | 0.008 % |
| preamp, gain full, 1 V (hard clip) | trapezoidal, 192k | 0.09 % |
| preamp, bass 0 / treble 10, VOICE 1n | trapezoidal, 192k | 0.18 % |
| power, master half, 2 V | trapezoidal, 192k | 0.02 % |
| power, master full, 20 V (saturated) | trapezoidal, 192k | 0.20 % |
| preamp, gain full, 1 V | trapezoidal, 96k | 0.34 % |
| power, master full, 20 V | trapezoidal, 96k | 0.51 % |
| preamp, gain full, 1 V | **θ 0.55**, 96k | 0.46 % |
| power, master half, 2 V | **θ 0.55**, 96k | 0.07 % |
| power, full, 20 V at 1 kHz (flyback spikes) | **θ 0.55**, 96k | 6.3 % |

The last case is the brutal one: a square wave at full power, with plate
spikes that ngspice resolves at nanosecond steps. The fundamental and the
shape agree; the spike timing is where the 6 % comes from.

---

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
- **CAB.**
  - **Off** gives the 8 Ω tap as a line out, for your own impulse responses.
  - **On** takes the motional voltage across the speaker's mechanical RLC
    (the cone's velocity) and differentiates it (acceleration, which is what a
    cone radiates). Below resonance that already falls 12 dB/octave, as a
    sealed cabinet does. After that, a fixed response models the breakup and a
    close mic: +5 dB at 2.4 kHz, a −6 dB notch at 3.7 kHz, +2 dB box thump at
    110 Hz, and a steep fall from 5 kHz.

  **This is the one part of the plugin tuned by number rather than by ear,
  and the first thing to tune by ear.** On and off are matched at 1 kHz.
- **Safety.** If the output is ever non-finite or above 2 kV, the amp is
  "switched off and on" (re-solved to its operating point) and the block is
  muted. The bench's sweeps never trigger it.

## 5. Measured (mmtest)

- Rated power is **135 W at 10% THD**. The figure the vintage heads were
  rated at: with a high-gain driver, a cathodyne and little feedback, this is
  not a hi-fi amp.
- **Sag**: 52 V flat out, 0.1 V at bedroom level.
- GAIN takes THD from −42 dB to −4.5 dB. Asymmetric clipping leaves a strong
  second harmonic (−7 dBc).
- TREBLE, BASS, VOICE, BITE and MASTER each move what they should:
  - centroids: TREBLE 302 → 6033 Hz, BASS 5042 → 2182 Hz,
    VOICE 3861 → 5219 Hz, BITE 3728 → 5703 Hz;
  - MASTER: 27 dB of level.
- **Aliasing** on a clipped A6, GAIN full, through the cab: −52 dB on NORMAL,
  −80 dB on HIGH. Use HIGH for bouncing.
- **Cost** on the build container's core: about 35% of one core on NORMAL and
  about 65% on HIGH, per instance, at 48 kHz.

## 6. Not modelled (yet)

- Hum and rectifier ripple: the supply is DC behind a resistance.
- Valve noise and microphony.
- Output-transformer core saturation and hysteresis: the core is linear, with
  loss.
- Mismatched output valves and valve ageing.
- Cone breakup as physics: it is the fixed response in §4.

## 7. To do

1. Read the real OR120 service sheet and put its values into
   `Or120Netlist.h`. spicecheck and the operating-point checks then show
   what moved.
2. Tune CAB by ear against a real 4×12, or ship it alongside an IR loader.
3. Windows build, landing page, `app.json` and manifest entry when it ships.
   Until then it is in no workflow but `engines.yml`.
