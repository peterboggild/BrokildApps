/*  MARMALADE 120 - the amplifier, as a netlist

    A 120 W British head of the early-seventies "pictures, not words" school:
    two ECC83s and four EL34s, solid-state rectified, fixed bias. The topology
    is the documented one: two gain stages with GAIN between them, a six-way
    capacitor switch (FAC) into a passive Baxandall bass/treble, HF DRIVE
    between the tone stack and the third stage, that stage DC-coupled into a
    cathodyne (split-load) phase inverter, a post-inverter master volume, four
    EL34s and an output transformer into a 4x12.

    It is ONE circuit, mains to speaker: the 50 Hz mains drive the power
    transformer's windings, silicon diodes charge the reservoir, and every
    stage draws from the rail it really draws from, through the filter it
    really sits behind. Sag, ripple and hum are not added; they happen.

    The VALUES are mostly estimates (tagged [est] on every line): the
    schematic archives were not reachable when this was written. They were
    chosen to be typical of the period and to land the voltages the sources
    do give. Nothing was chosen for how it sounds. A value read off the real
    service sheet replaces the [est] next to it, and test/spicecheck and the
    bench's operating-point checks show what moved.

    Runtime parts (the pots, the FAC capacitor) are set by name from
    Engine.cpp; their values here are only mid-travel defaults.
*/
#pragma once

namespace mm
{

/*  OR120_AMP: the whole amplifier as ONE circuit.

    Provenance, on every part line:
      [doc]  stated by a source (see MARMALADE-120-DESIGN.md, section 1)
      [ds]   from a datasheet or a published device model
      [est]  estimated: chosen to be typical of the period and to land the
             documented voltages. NOT from a schematic - replace from one.
    Nothing is adjusted to make it sound or measure a particular way.

    Inputs:  IN 0  the guitar, volts at the jack
             IN 1  the HT winding's EMF, each half (the engine's 50 Hz mains)
             IN 2  the bias winding's EMF
*/
static const char* const OR120_AMP = R"NET(
* MARMALADE 120 - the whole amplifier
Vin    in    0     IN 0             ; the guitar

*---- V1a, first gain stage -------------------------------------------- [doc] topology
Rin    in    g1    68k              ; [est] grid stopper
Rgl1   g1    0     1meg             ; [est]
X1a    p1    g1    k1    ecc83      ; [doc] ECC83
Ra1    b4    p1    100k             ; [est]
Rk1    k1    0     1.5k             ; [est]
Ck1    k1    0     25u              ; [est]

*---- GAIN, between the first two stages -------------------------------- [doc] position
Cc1    p1    ga    22n              ; [est]
Rgaina ga    gw    500k             ; [est] 1M log pot, top half
Rgainb gw    0     500k             ; [est] pot, bottom half
Rst2   gw    g2    33k              ; [est]

*---- V1b, second gain stage -------------------------------------------- [doc] topology
X1b    p2    g2    k2    ecc83      ; [doc] ECC83
Ra2    b4    p2    100k             ; [est]
Rk2    k2    0     1.5k             ; [est]
Ck2    k2    0     1u               ; [est]

*---- FAC: one of six coupling capacitors into the tone stack ------------ [doc] 6 positions
Cfac   p2    tin   10n              ; [est] values 47n..1n, see Engine.cpp

*---- passive Baxandall bass and treble --------------------------------- [doc] Baxandall
Rb1    tin   ba    100k             ; [est]
Rbassa ba    bw    500k             ; [est] 1M lin pot
Rbassb bw    bb    500k
Rb2    bb    0     10k              ; [est]
Cb1    ba    bw    1n               ; [est]
Cb2    bw    bb    10n              ; [est]
Rb3    bw    tout  100k             ; [est]
Ct1    tin   ta    330p             ; [est]
Rtreba ta    tout  500k             ; [est] 1M lin pot
Rtrebb tout  tb    500k
Ct2    tb    0     3.3n             ; [est]

*---- HF DRIVE, between the tone stack and the third stage -------------- [doc] position
Rhf    tout  g3    470k             ; [est]
Chf    tout  hfa   470p             ; [est]
Rhfpot hfa   g3    500k             ; [est] 1M lin as a rheostat
Rgl3   g3    0     470k             ; [est]

*---- V2a, third stage, DC coupled to the cathodyne --------------------- [doc] DC coupled
X2a    p3    g3    k3    ecc83      ; [doc] ECC83
Ra3    b3    p3    470k             ; [est] a forum note has 390k as a raise from stock;
*                                    ;       470k gives the cathodyne room to swing
Rk3    k3    kf    1.5k             ; [est]
Ck3    k3    kf    25u              ; [est]
Rkf    kf    0     470              ; [est]
Rnfb   spk   kf    47k              ; [est] global feedback from the 8 ohm tap

*---- V2b, cathodyne phase inverter ------------------------------------- [doc] cathodyne
X2b    pc    p3    kc    ecc83      ; [doc] ECC83
Rpc    b3    pc    100k             ; [est]
Rkc    kc    0     100k             ; [est]

*---- MASTER, dual gang after the inverter ------------------------------ [doc] post-PI (1998/2001 sheets)
Cpa    pc    ma    100n             ; [est]
Cpb    kc    mb    100n             ; [est]
Rmasaa ma    mwa   110k             ; [est] 220k log dual pot
Rmasab mwa   bias  110k
Rmasba mb    mwb   110k
Rmasbb mwb   bias  110k
Rgsa   mwa   g4a   1.1k             ; [doc] 2k2 grid resistor per valve (74 sheet), two in parallel
Rgsb   mwb   g4b   1.1k

*---- four EL34, fixed bias, each valve its own --------------------------- [doc] 4 x EL34, fixed bias
X4a    pa    sa    g4a   0     el34x2   ; two in parallel per side: exact for matched valves
X4b    pb    sb    g4b   0     el34x2
Rsca   b2    sa    500              ; [est] 1k per valve
Rscb   b2    sb    500

*---- output transformer, 1k7 plate to plate into 8 ohms ----------------- [est] all of it
Rwa    pa    ota   45               ; [est] winding resistance, each half
Rwb    pb    otb   45
Cpp    pa    pb    1n               ; [est] interwinding capacitance
Ta     b1    ota   sec0  0   7.29
Tb     otb   b1    sec0  0   7.29
Lmag   sec0  0     140m             ; [est] magnetising, referred to the secondary
Rcore  sec0  0     470              ; [est] core loss, referred
Lleak  sec0  sec   60u              ; [est] leakage, referred (about 13 mH primary)
Rsec   sec   spk   0.25             ; [est]

*---- the 4x12 as the amplifier sees it --------------------------------- [est] generic 12" x4
Rvc    spk   s1    6.4
Lvc    s1    s2    0.7m
Rvce   s1    s2    20
Rmot   s2    0     35
Lmot   s2    0     20.6m
Cmot   s2    0     152u

*---- HT supply: centre-tapped winding, two silicon diodes --------------- [doc] solid state
Vhta   hta   0     IN 1             ; [est] each half 375 V rms
Vhtb   0     htb   IN 1             ; the other half, in antiphase
Rhta   hta   hda   15               ; [est] winding + reflected primary, each half
Rhtb   htb   hdb   15
Lhta   hda   hdx   5m               ; [est] leakage, each half
Lhtb   hdb   hdy   5m
D1     hdx   b1    1n4007           ; [ds]
D2     hdy   b1    1n4007           ; [ds]
C1     b1    0     100u             ; [est] reservoir; the OT centre tap
Rd2    b1    b2    470              ; [est]
C2     b2    0     50u              ; [est] screens
Rd3    b2    b3    36k              ; [est] sets the ~370 V [doc] rail
C3     b3    0     32u              ; [est] driver and inverter, ~370 V [doc]
Rd4    b3    b4    18k              ; [est] sets the ~330 V [doc] rail
C4     b4    0     32u              ; [est] preamp, ~330 V [doc]

*---- bias: a winding, a diode, a filter, the set-bias divider ---------- [doc] adjustable fixed bias
Vbw    bwn   0     IN 2             ; [est] 50 V rms
Rbw    bwn   bwr   100              ; [est]
Db     bng   bwr   1n4007           ; [ds] conducts on the negative half
Cbr    bng   0     47u              ; [est]
Rbf    bng   bf    10k              ; [est]
Cbf    bf    0     47u              ; [est]
Rbp1   bf    bias  6.1k             ; [est] set-bias pot, upper part: SET, like a tech would,
Rbp2   bias  0     41.1k            ;   for 35 mA a valve at idle (70% of 25 W), by test/biasset
Cbias  bias  0     10u              ; [est]

.out   spk   spk
.out   b1    b1
.out   b4    b4
)NET";

} // namespace mm
