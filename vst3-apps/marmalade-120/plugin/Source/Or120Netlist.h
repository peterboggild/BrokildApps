/*  MARMALADE 120 - the amplifier, as a netlist

    A 120 W British head of the early-seventies "pictures, not words" school:
    two ECC83s and four EL34s, solid-state rectified, fixed bias. The 1972
    Graphic MkII: a gain stage, the bass/treble stack, GAIN with a bright
    cap, a second gain stage, the six-way FAC capacitor switch, a driver with
    the BOOST resonant cathode network and the global feedback, an AC-coupled
    cathodyne, four EL34s and an output transformer. No master volume.

    It is ONE circuit, mains to speaker: the 50 Hz mains drive the power
    transformer's windings, silicon diodes charge the reservoir, and every
    stage draws from the rail it really draws from, through the filter it
    really sits behind. Sag, ripple and hum are not added; they happen.

    The values are read off the 1972 schematics (B. Hickmott's redraw), tagged
    [sch] on every line. What a schematic never says - the transformers'
    voltages and impedances, the choke, the speaker - is tagged [est], and
    is the next thing to measure.

    Runtime parts (the pots, the FAC capacitor) are set by name from
    Engine.cpp; their values here are only mid-travel defaults.
*/
#pragma once

namespace mm
{

/*  OR120_AMP: the whole amplifier as ONE circuit.

    SOURCE: Orange OR120 Graphic MkII, early 1970s - preamp, output amp and
    power supply - as redrawn by B. Hickmott (2004) from the 1972 schematics.

    Provenance, on every part line:
      [sch]  read off those three sheets
      [int]  the sheets, interpreted where they leave a choice (said why)
      [ds]   from a datasheet or a published device model
      [est]  not on the sheets (transformers, choke, speaker): estimated
    Nothing is adjusted to make it sound or measure a particular way.

    Inputs:  IN 0  the guitar, volts at the jack
             IN 1  the mains, volts, at the power transformer's 240 V tap
*/
static const char* const OR120_AMP = R"NET(
* MARMALADE 120 - Orange OR120 Graphic MkII (1972), mains to speaker
Vin    in    0     IN 0             ; the guitar

*==== PREAMP (sheet 1) ===================================================
* one input used: the redraw says its jack wiring is uncertain, so the
* other jack's 68k is left open [int]
Rin    in    g1    68k              ; [sch]
Rgl1   g1    0     1meg             ; [sch]
X1a    p1    g1    k1    ecc83      ; [sch] 1/2 ECC83
Ra1    dp    p1    220k             ; [sch] from D+
Rk1    k1    0     2.2k             ; [sch]
Ck1    k1    0     50u              ; [sch]

* tone stack, between the two stages: bass and treble, both 1M log
Cc1    p1    t     68n              ; [sch]
Rb1    t     ba    100k             ; [sch]
Rbassa ba    bw    500k             ; [sch] 1M log pot, upper part (set by Engine)
Rbassb bw    bb    500k             ;       lower part
Cb1    ba    bw    2.2n             ; [sch]
Cb2    bw    bb    22n              ; [sch]
Rb2    bb    0     22k              ; [sch]
Rb3    bw    o     100k             ; [sch] bass wiper to the output
Ct1    t     ta    1.5n             ; [sch]
Rtreba ta    o     500k             ; [sch] 1M log pot; its wiper is the output
Rtrebb o     tb    500k
Ct2    tb    0     10n              ; [sch]

* GAIN, 1M log, with a 330p bright cap across its upper part
Rgaina o     g2    500k             ; [sch]
Rgainb g2    0     500k
Cbr    o     g2    330p             ; [sch]

X1b    p2    g2    k2    ecc83      ; [sch] 1/2 ECC83
Ra2    dp    p2    220k             ; [sch]
Rk2    k2    0     2.2k             ; [sch]
Ck2    k2    0     50u              ; [sch]

* FAC: 68n, then a series chain 4n7-4n7-2n2-1n-330p tapped by the switch;
* one capacitor here, the series value of the chain up to the tap [sch]
Cfac   p2    fo    68n              ;       (set by Engine)

*==== OUTPUT AMP (sheet 2) ===============================================
* the preamp into the driver's grid: 100k direct, and 100k + 100k through
* the reverb send and return, normalled by the return jack when nothing
* is plugged in [int: the jack switching is drawn, not labelled]
Rmix   fo    g3    100k             ; [sch]
Rsend  fo    snd   100k             ; [sch]
Rret   snd   g3    100k             ; [sch]
Rgl3   g3    0     220k             ; [sch]

* driver
X2a    p3    g3    k3    ecc83      ; [sch] 1/2 ECC83
Ra3    cp    p3    100k             ; [sch] from C+
Chc    cp    p3    1n               ; [sch]
Rk3    k3    0     1.5k             ; [sch]
Rfb    fb16  k3    15k              ; [sch] global feedback from the 16 ohm tap
* BOOST: a 1k lin rheostat, a 2 mH choke and 0.47u from the cathode to ground
Rbst   k3    bs1   500              ; [sch] 1k lin (set by Engine)
Lbst   bs1   bs2   2m               ; [sch]
Rbstl  bs2   bs3   8                ; [est] the choke's resistance
Cbst   bs3   0     0.47u            ; [sch]

* cathodyne, AC coupled, self-biased with a bootstrapped grid leak
Cpi    p3    g4    68n              ; [sch]
X2b    pc    g4    kc    ecc83      ; [sch] 1/2 ECC83
Rpc    cp    pc    100k             ; [sch]
Rkc    kc    kx    1.5k             ; [sch]
Rkx    kx    0     100k             ; [sch]
Rgl4   g4    kx    1meg             ; [sch]

* to the output valves: 68n, 220k bias feeds from N.B, 2k4 per valve
Cpa    pc    ga    68n              ; [sch]
Rbfa   ga    nb    220k             ; [sch]
Rgsa   ga    g5a   1.2k             ; [sch] 2k4 per valve, two in parallel
Cpb    kx    gb    68n              ; [sch]
Rbfb   gb    nb    220k             ; [sch]
Rgsb   gb    g5b   1.2k             ; [sch]

* four EL34, two a side, cathodes (and g3) grounded
X4a    pa    sa    g5a   0     el34x2   ; [sch] exact for matched valves
X4b    pb    sb    g5b   0     el34x2
Rsca   bp    sa    500              ; [sch] 1k per screen, from B+
Rscb   bp    sb    500

* output transformer, centre tap on A+; speaker on the 8 ohm tap, feedback
* from the 16 [sch]; everything about the iron itself is [est]
Rwa    pa    ota   45               ; [est] winding resistance, each half
Rwb    pb    otb   45
Cpp    pa    pb    1n               ; [est] interwinding capacitance
Ta     ap    ota   sec0  0   7.29   ; [est] 1k7 plate to plate : 8 ohms
Tb     otb   ap    sec0  0   7.29
Lmag   sec0  0     140m             ; [est] magnetising, referred
Rcore  sec0  0     470              ; [est] core loss, referred
Lleak  sec0  sec   60u              ; [est] leakage, referred
Rsec   sec   spk   0.25             ; [est]
Tfb    fb16  0     spk   0   1.41421 ; [sch] the 16 ohm tap: sqrt(16/8) the 8 ohm tap's voltage

* the 4x12 on the 8 ohm tap, as the amplifier sees it [est]
Rvc    spk   s1    6.4
Lvc    s1    s2    0.7m
Rvce   s1    s2    20
Rmot   s2    0     35
Lmot   s2    0     20.6m
Cmot   s2    0     152u

*==== POWER SUPPLY (sheet 3) =============================================
Vmains mn    0     IN 1             ; the 240 V tap of the primary

* HT: one winding into a bridge of 8 x 1N4005, two in series per arm
Tht    hta   htb   mn    0   1.5625 ; [est] 375 V rms secondary
Rht    hta   hx    20               ; [est] winding + reflected primary resistance
Lht    hx    hy    8m               ; [est] leakage
* the winding is not floating: each end has capacitance to the grounded
* core and screen. Left out, its common mode hangs on the diodes' leakage
* alone, and Newton was measured stalling at the edge of conduction.
Cwa    hta   0     1n               ; [est]
Cwb    htb   0     1n               ; [est]
D1     hy    ap    1n4005x2         ; [sch]
D2     htb   ap    1n4005x2
D3     0     hy    1n4005x2
D4     0     htb   1n4005x2
C1     ap    0     50u              ; [sch] 2 x 100u 450 V in series: A+
Lch    ap    chx   10               ; [est] the choke: 10 H
Rch    chx   bp    120              ; [est] and its resistance
C2     bp    0     16u              ; [sch] 2 x 32u in series: B+ (screens)
Rbl    bp    0     200k             ; [sch] 2 x 100k across them
Rd3    bp    cp    33k              ; [sch]
C3     cp    0     16u              ; [sch] C+ (driver, inverter)
Rd4    cp    dp    33k              ; [sch]
C4     dp    0     16u              ; [sch] D+ (preamp)

* bias: a centre-tapped winding, two 1N4005, 22u - 1k - 22u, 22k load. No
* adjustment on the sheet: the winding sets it.
Tba    bwa   0     mn    0   0.15478 ; [est] 37.1 V rms each half: 35 mA a valve idle, by test/biasset.cpp
Tbb    0     bwb   mn    0   0.15478
Rbwa   bwa   bwa2  50               ; [est]
Rbwb   bwb   bwb2  50
Dba    nr    bwa2  1n4005           ; [sch] cathodes to the winding: a negative supply
Dbb    nr    bwb2  1n4005
Cn1    nr    0     22u              ; [sch]
Rn     nr    nb    1k               ; [sch]
Cn2    nb    0     22u              ; [sch]
Rnl    nb    0     22k              ; [sch]

.out   spk   spk
.out   b1    ap
.out   b4    dp
)NET";

} // namespace mm
