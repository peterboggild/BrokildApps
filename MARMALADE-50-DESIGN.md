# MARMALADE 50 — design

The Orange Thunderverb 50 of 2006/07, simulated as one circuit, sample by
sample, with the Marmalade 120's solver. The whole circuit is in it:
- the 50 Hz mains at the power transformer;
- the full-wave rectifier, the choke and the four-stage dropper ladder;
- both three-stage ECC83 preamps and the relays that choose between them;
- the ECC81 FX loop and the ECC81 reverb driver and recovery;
- the long-tailed-pair phase splitter and the attenuator;
- two EL34s, the output transformer and the speaker.

The rule is the Marmalade 120's: values come from the drawings, and anything
the drawings don't give is tagged as an estimate. Nothing is tuned to sound a
particular way, and a disagreement gets reported rather than adjusted away.

- Plugin: `vst3-apps/marmalade-50/plugin` (VST3 + Standalone, JUCE 8)
- Netlist: `Source/Tv50Netlist.h`
- Solver (shared with the Marmalade 120): `vst3-apps/marmalade-120/plugin/Source/Circuit.*` and `Partition.*`
- Tests: `test/` — `mm50test` (bench), `mm50split` (split vs whole), `mm50spice` (whole vs ngspice), `mm50bias`, `mm50render`

## 1. The sources

The drawings are Orange's own factory schematics, not a redraw:

| Drawing | Issue | Date | Content |
|---|---|---|---|
| ORA-CD209 sheet 1 | 1.1 | 23 Nov 2006 | both preamps, loop, reverb, phase splitter |
| ORA-CD209 sheet 2 | 1.1 | 23 Nov 2006 | rail ladder (with test-point voltages), heaters, relays |
| ORA-CD212 | A | 23 Jun 2007 | 50 W power board: HT, bias, EL34s, OT wiring, jacks |

They were drawn by Paul Stevens, and the original circuit design is credited
to Adrian Emsley. Every netlist line is tagged [sch], [int], [ds], [doc] or
[est], as in the Marmalade 120.

### What the drawings print, and what they leave as "???"

The rail ladder carries test-point voltages:
- B (TP7) 390 V
- C (TP8) 317 V
- D (TP9) 270 V
- E (TP10) 245 V
- F (TP11) 218 V
- −BIAS (TP5) −50 V, "bias set fully anti-CW, tolerance ±10%"

The transformer voltages are printed as "???": the HT winding, the pink bias
lead, and rail A.

## 2. The circuit

### Preamps

**Preamp 1 (GAIN, SHAPE, VOLUME):**
- V10-A, V9-A and V9-B, with plate loads of 100k from F and E.
- Cathodes: 1k5 ‖ 22u, 2k2 ‖ 22u, and 820R ‖ 100u.
- GAIN is a dual A1M0, one gang ahead of each of the second and third stages, with 56p bright caps.
- The SHAPE network is DC-coupled to V9-B's plate: R52 100k, C35 56p, C34 and C33 22n, and RV2, a dual B500K, with R58 15k.

**Preamp 2 (GAIN, TREBLE, MIDDLE, BASS, VOLUME):**
- The same three-stage topology: V10-B, V8-A and V8-B.
- Cathode caps are 10u on the first two stages; C30 is 100p and C27 is 2n2.
- The tone stack is a treble/middle/bass network, also DC-coupled to the plate: R61 22k slope, 560p treble cap, 22n bass and middle caps, RV7 B250K treble, RV5 A500K bass (a rheostat), and RV6 B25K middle.

**The input** is one jack into both channels: 220n, a 1M leak, and a 68k stopper into each first grid. Both first stages always run.

**The channel relays.** Two G5V-1 relays ground the idle channel through 47R:
- RL4 at the top of its gain pot;
- RL1 at its volume's wiper.

The volumes mix through 220k and 270k into the loop's send valve.

### Loop, reverb, splitter

- **Loop send:** V7-A, an ECC81 cathode follower with its plate on D. The grid is biased off D through 220k/33k. The send goes through 82k with 68k to ground.
- **Loop return:** V7-B (56k plate, unbypassed 1k5 cathode). The return jack normals the send to the return, and plugging into it breaks that.
- **Reverb driver:** V6-A, an ECC81, drives the reverb transformer from C through 15k.
- **Reverb recovery:** V6-B (56k from F). It feeds RV9 (250KA, signal on the wiper) and 220k into the splitter's mix. RL2 grounds the recovery grid when the reverb is off.
- **Phase splitter:** V5, an ECC83 long-tailed pair:
  - plates 82k and 100k from C;
  - tail 680R, then 15k, then 100R;
  - feedback through R9 6k8 from the 8 Ω tap into the 15k/100R junction.
- **Attenuator:** RV10, a dual B1M0 across each splitter output, switched in by RL3. Clockwise attenuates (pin 3 is at 0 V).

### Power amp and supply (CD212)

**Output stage:**
- Two EL34s, g3 to the cathode at 0 V.
- Screens 1k 5 W from B; grid stoppers 1k5; 220k bias feeds from −BIAS.
- The OT's centre tap is on A. Feedback is taken from the 8 Ω tap (the brown lead).

**HT:**
- A centre-tapped winding (purple, purple, grey to 0 V) with two 1N5408 in series each side, and 10n across the winding.
- The reservoir is 2 × 220u in series, with 220k balancing.
- Then the choke (black to orange) to B: 100u and 220k.
- The ladder: 4k7 to C (100u), 4k7 to D (47u), 4k7 to E (47u), 10k to F (22u).

**Bias:**
- The pink lead, half wave through 220k 1 W and a 1N4007.
- Then 10u, 15k and 10u to −BIAS, then 47k plus the 47k preset to 0 V.

### Interpretations [int]

- **The 47R relay commons are drawn as two resistors,** one behind each contact. It is the same circuit whenever one contact is made, which is always, and it lets each channel be solved on its own.
- **Relay contacts** are 0.1 Ω made and 1 GΩ open.
- **The bias preset's wiper is tied to its pin 1,** so "fully anti-CW" is the whole 47k: the coldest setting.
- **The input jack's switch contacts** are open, because a guitar is plugged in.
- **The FX loop** is normalled, because nothing is in it.
- **Not simulated,** since no signal passes through them: the heaters (DC for the preamp valves), the relay supply, and the footswitch jacks.

### Estimated [est]: the next things to measure

| Part | Value used | Why |
|---|---|---|
| Output transformer | 3k4 plate to plate : 8 Ω, 40 Ω each half, Lp 31 H, leakage 60 µH | the usual 50 W EL34 pair; not on the drawings |
| Choke | 10 H, 100 Ω | not on the drawings |
| HT winding resistance | 15 Ω each half | not on the drawings |
| Reverb transformer | 22k5 : 8 Ω (53:1), 500 Ω primary, 10 H | not on the drawings |
| Tank coils | input 0.8 Ω + 1.2 mH (8 Ω at 1 kHz), output 2k2 | an Accutronics type-4 input; the tank code is unknown |
| Speaker | the Marmalade 120's 4x12 model | — |
| Bias preset setting | 10.2k of 47k: 35 mA a valve | how a technician biases it; the drawing only gives the cold end |

**The windings** are set from the drawings' own test points (`mm50bias`):
- The HT half-ratio, 1.17524 (282 V rms each half), lands B on 390 V with the preset fully anti-CW.
- The bias ratio, 1.34321 (322 V rms on the pink lead), lands −BIAS on −50 V.

## 3. The sheets' test points, against the circuit

With the two windings set as above, every other rail is the circuit's own
answer, from the valves' own currents:

| Rail | Simulated | Drawing | |
|---|---|---|---|
| B | 390.0 V | 390 V (TP7) | set |
| C | 317.7 V | 317 V (TP8) | +0.2% |
| D | 268.1 V | 270 V (TP9) | −0.7% |
| E | 240.0 V | 245 V (TP10) | −2.0% |
| F | 213.0 V | 218 V (TP11) | −2.3% |
| bias | −50.0 V | −50 V (TP5) | set |

All four are well inside the drawing's ±10%, and nothing was adjusted to get
there. Biased to 35 mA a valve, the amp runs at −32.5 V bias, A 384 V and B 381.5 V.

## 4. The solver, split into blocks

The whole circuit is 33 device ports and 95 states. Solved as one block it
costs about 3–3.5 cores at 88.2 kHz in the build container. `Partition`
(shared with the Marmalade 120) cuts the one netlist at named resistors into
blocks. It solves them every sample in signal order: a cut resistor sits in
both of its blocks, with its far end held at the far node's latest voltage.
The blocks are generated from the parsed netlist, not written by hand.

| Block | Contents | Ports |
|---|---|---|
| preamp | both channels, V7-A, rails D/E/F | 14 |
| loop | V7-B, V6-A, the reverb transformer | 4 |
| power | V6-B, V5, the EL34s, the OT, the speaker, the supply | 15 |

Every cut is at a stiff node:
- R5, the send follower's output;
- R17 and R24, into the D and F rails;
- R34, C into D;
- R1, the reverb driver's feed from C;
- R8, the loop's output into the splitter's 1M mix.

**Cutting at the volume wipers was tried and is wrong.** A wiper's impedance is
as high as the 220k behind it, and the one-sample lag on its load showed
at −36 dB.

`mm50split` checks the split against the whole circuit on a decaying chord at three levels:

| Guitar level | Split vs whole, below the signal | Rails |
|---|---|---|
| 0.05 V | −74.9 dB | within 3 mV |
| 0.3 V | −71.8 dB | within 2 mV |
| 1.0 V | −67.8 dB | within 4 mV |

The split costs about 1.0–1.3 cores (the test host measures 97–102% at
NORMAL), the same as the Marmalade 120. HIGH runs at about 2 cores, which is
for offline renders.

## 5. Verified against ngspice (`mm50spice`)

This is the whole netlist as one circuit, run the Marmalade 120's way: the
solver warms the amp up, then hands ngspice its state as initial conditions,
and both run 60 ms side by side.

| Case | Rate | Speaker error | B ripple error |
|---|---|---|---|
| idle | 96k | 0.33% | 6.2% (of 10 mV: ngspice's own precision) |
| preamp 2 clean, 100 mV | 96k | 0.80% | 0.03% |
| preamp 1 crunch, 100 mV | 96k | 1.28% | 0.04% |
| preamp 1 flat out, 500 mV | 96k | 5.13% | 0.26% |
| preamp 1 flat out, 500 mV | 192k | 1.62% | 0.09% |
| power amp from RETURN, saturated | 96k | 0.15% | 0.005% |

Zero Newton failures in any case. The flat-out error converges with the rate,
as it does on the Marmalade 120 (6% at 96k, 2% at 192k).

## 6. Measured (`mm50test`, 10 of 10)

**Idle:** each EL34 at 35.0 mA, bias −32.48 V.

**Power.** A generator in the loop's RETURN jack, at 1 kHz into the 8 Ω tap:
- The most power at the fundamental is 49.9 W, against Orange's 50 W rating. Total rms flat out is about 63 W.
- **[KNOWN]** THD reaches 10% at only about 4–5 W. The pair is class AB pentode with very little global feedback: about 1.5% of the 8 Ω tap reaches the splitter's tail. The EL34 pair's transconductance expands under drive (power law, γ 1.67): the valves alone give 5% H3 at the grid swing that makes 7 W. So 10% THD is not where this amp is rated. Saturated fundamental power is.

**Tone controls**, preamp 2: treble lifts 5 kHz, middle lifts 1 kHz, bass lifts 100 Hz.

**Relays.** An idle channel's knobs change nothing at the speaker. What does
get through from the jack with both channels down (−38 dB under a selected
channel at gain 2) is the rails. Both first stages always run, and rail F
also feeds the reverb recovery's plate. That's in the real circuit too.

## 7. Found along the way

- **The bench's "mains off" mode is invalid for this amp.** It holds the windings at their DC peak. This bias supply is half-wave negative and never charges under that hold, so the EL34s ran away to 446 mA. The mode is removed: this amp is always measured with its mains running.
- **The EL34 fit's screen share near cutoff is unphysical.** It's shared with the Marmalade 120. The fit's knee exponent came out negative (q = −0.95), so the screen takes 22–67% of the current below about 10 mA, where the sheet's screen-to-plate ratio stays near 1:7. A refit with q ≥ 0 fixes that, at 13.7% rms over the sheet against 10.4% (`test/el34fit`). It changes neither amp's distortion (checked on the power amp), so it isn't adopted yet. It is the next model change, together with re-biasing both amps.
- **ECC81 is interim.** It's the Dempwolf form fitted to Koren's published 12AT7: a fit to a fit, 9% rms at 1 mA and up (`test/ecc81fit`). No ECC81 data sheet could be reached from the build machine. The ECC81s carry the loop and the reverb, not the preamp's gain or the power stage.

## 8. Not modelled

- **The springs.** The reverb circuit is all there. The tank itself is a measured impulse response, loaded with LOAD TANK IR: the volts across its input coil in, the volts at its output out, as measured and not normalised. The engine takes the tank's return one 32-sample chunk ahead, so the IR goes in with its first chunk dropped, which is silence for any spring tank. Without a tank IR the springs are silent.
- **Heater-to-cathode hum, microphony, valve ageing.**

## 9. What would make it more accurate, in order

1. **A tank sweep.** A sine sweep into the tank's input (RCA) and a recording of its output, with the tank's code. Without it there is no reverb at all.
2. **The output transformer:** each primary half's and the secondary's resistances, and its impedance or turns ratio.
3. **The running voltages:** the HT and pink windings' AC volts, rail A, and TP5–TP11 on your amp. Each one is an independent check on §3.
4. **The board labels,** to confirm they are CD209 1.1 and CD212 A.
5. **An ECC81 data sheet,** for a fit from source like the EL34's.
6. **The choke's resistance; the speaker's impedance curve.**
