/*  MARMALADE 120 - the amplifier, as netlists

    A 120 W British head of the early-seventies "pictures, not words" school:
    two ECC83s and four EL34s, solid-state rectified, fixed bias. The topology
    is the documented one: two gain stages with GAIN between them, a six-way
    capacitor switch (FAC) into a passive Baxandall bass/treble, HF DRIVE
    between the tone stack and the third stage, that stage DC-coupled into a
    cathodyne (split-load) phase inverter - the cathodyne is the signature, it
    distorts lopsidedly and only just has the swing to drive the EL34s - then
    a post-inverter master volume, four EL34s and an output transformer into a
    closed-back 4x12.

    The VALUES are a reconstruction, not a copy of a service sheet: where a
    part was not documented it was chosen to land the operating points and
    voltages the circuit is known for (about 330 V and 370 V on the small-
    signal rails, the driver's plate low enough for the DC-coupled cathodyne
    to sit in the middle of its range). Every value is here in plain text, so
    a schematic read later is a matter of editing numbers, and the bench
    re-checks the solver against ngspice whatever they are.

    The amp is solved as two circuits, cut where the real one is least
    disturbed by the cut:

      PREAMP   guitar -> V1a -> GAIN -> V1b -> FAC -> tone stack -> HF DRIVE
               -> the third stage's grid. The third stage's input capacitance
               (its Miller capacitance, which with the tone stack's high source
               impedance is the preamp's top-end roll-off) is kept here as a
               fixed capacitor, because that is where it does its work.
      POWER    the third stage, the cathodyne, MASTER, the EL34s, the output
               transformer, the speaker's electrical impedance, the global
               negative feedback and the whole power supply - everything that
               sags together is solved together.

    The preamp's supply is the power section's last filter node, handed across
    each sample: the preamp sags when the output stage draws.

    Inputs and runtime parts are referred to by name from Engine.cpp; the
    "pot" resistors' values here are only their mid-travel defaults.
*/
#pragma once

namespace mm
{

static const char* const OR120_PREAMP = R"NET(
* MARMALADE 120 preamp
Vin    in    0     IN 0             ; the guitar, in volts
Vbp    bpre  0     IN 1             ; the preamp rail, from the power section

* V1a - first gain stage
Rin    in    g1    68k              ; grid stopper
Rgl1   g1    0     1meg
X1a    p1    g1    k1    12ax7
Ra1    bpre  p1    100k
Rk1    k1    0     1.5k
Ck1    k1    0     25u

* GAIN - 1M log, between the first two stages
Cc1    p1    ga    22n
Rgaina ga    gw    500k             ; pot, top half
Rgainb gw    0     500k             ; pot, bottom half
Rst2   gw    g2    33k

* V1b - second gain stage
X1b    p2    g2    k2    12ax7
Ra2    bpre  p2    100k
Rk2    k2    0     1.5k
Ck2    k2    0     1u

* FAC - one of six coupling capacitors into the tone stack
Cfac   p2    tin   10n

* passive Baxandall: bass
Rb1    tin   ba    100k
Rbassa ba    bw    500k             ; pot, top half
Rbassb bw    bb    500k             ; pot, bottom half
Rb2    bb    0     10k
Cb1    ba    bw    1n
Cb2    bw    bb    10n
Rb3    bw    tout  100k

* passive Baxandall: treble (its wiper is the output)
Ct1    tin   ta    330p
Rtreba ta    tout  500k             ; pot, top half
Rtrebb tout  tb    500k             ; pot, bottom half
Ct2    tb    0     3.3n

* HF DRIVE - a treble path around the series loss into the third stage
Rhf    tout  g3    470k
Chf    tout  hfa   470p
Rhfpot hfa   g3    500k             ; pot as a rheostat, 1M .. 1k
Rgl3   g3    0     470k
Cmil   g3    0     150p             ; the third stage's input (Miller) capacitance

.out   drive g3
.out   ipre  I(Vbp)
.out   p1    p1
.out   p2    p2
)NET";

static const char* const OR120_POWER = R"NET(
* MARMALADE 120 power section
Vdrv   drv   0     IN 0             ; the preamp's output
Rst3   drv   g3    10k

* V2a - third stage / cathodyne driver, DC coupled to it
X2a    p3    g3    k3    12ax7
Ra3    b3    p3    470k             ; high, so the DC-coupled cathodyne sits low enough to swing
Rk3    k3    kf    1.5k
Ck3    k3    kf    25u
Rkf    kf    0     470
Rnfb   spk   kf    47k              ; global negative feedback from the 8 ohm tap

* V2b - cathodyne phase inverter
X2b    pc    p3    kc    12ax7
Rpc    b3    pc    100k
Rkc    kc    0     100k

* MASTER - dual gang after the inverter, its far end on the bias supply
Cpa    pc    ma    100n
Cpb    kc    mb    100n
Rmasaa ma    mwa   110k             ; pot, top half
Rmasab mwa   bias  110k             ; pot, bottom half
Rmasba mb    mwb   110k
Rmasbb mwb   bias  110k
Vbias  bias  0     DC -48
Rgsa   mwa   g4a   1.1k             ; 2k2 grid stoppers, two valves in parallel
Rgsb   mwb   g4b   1.1k

* the four EL34s, as two matched pairs
X4a    pa    sa    g4a   0     el34x2
X4b    pb    sb    g4b   0     el34x2
Rsca   b2    sa    500              ; 1k screen resistors, paralleled
Rscb   b2    sb    500

* output transformer: 1k7 plate to plate into 8 ohms, centre tapped. The two
* primary halves are wound together and tightly coupled, so the leakage is
* referred to the secondary: a cut-off valve's half then follows its partner
* through the core, as it does in the iron, instead of ringing on its own
* leakage at a kilovolt (which is what a per-plate leakage model was measured
* doing, and is not what a real OT does).
Rwa    pa    ota   45               ; winding resistance, each half
Rwb    pb    otb   45
Cpp    pa    pb    1n               ; interwinding capacitance: also what the valves
*                                    ; push a step of current into, so it must stay
Ta     b1    ota   sec0  0   7.29
Tb     otb   b1    sec0  0   7.29
Lmag   sec0  0     140m             ; magnetising inductance, referred
Rcore  sec0  0     470              ; core loss, referred
Lleak  sec0  sec   60u              ; leakage (about 13 mH primary), referred
Rsec   sec   spk   0.25

* the 4x12, as the amplifier sees it: voice coil and mechanical resonance
Rvc    spk   s1    6.4
Lvc    s1    s2    0.7m
Rvce   s1    s2    20               ; eddy loss across the coil inductance
Rmot   s2    0     35
Lmot   s2    0     20.6m
Cmot   s2    0     152u

* solid-state supply: transformer and diodes as a source behind a resistance
Vtx    raw   0     DC 525
Rtx    raw   b1    80
C1     b1    0     100u
Rd2    b1    b2    470
C2     b2    0     50u
Rd3    b2    b3    27k
C3     b3    0     32u
Rd4    b3    b4    10k
C4     b4    0     32u
Rpre   b4    0     110k             ; the preamp's draw

.out   spk   spk
.out   cone  s2
.out   b4    b4
.out   b1    b1
.out   pa    pa
.out   pb    pb
)NET";

} // namespace mm
