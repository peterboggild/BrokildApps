/*  MARMALADE 50 - the amplifier, as a netlist

    A 50 W British two-channel head of the mid-2000s: four ECC83s, two
    ECC81s and two EL34s, a valve-driven spring reverb, a valve FX loop, an
    attenuator after the phase splitter, and relays doing the switching.

    It is ONE circuit, mains to speaker, as the Marmalade 120 is: the mains
    drive the power transformer, silicon diodes charge the reservoir through
    a choke, and every valve draws from the rail it really draws from.

    SOURCE: the factory drawings -
        ORA-CD209 issue 1.1, 23 Nov 2006, "Valve preamp section" (sheets 1
            and 2: both preamps, loop, reverb, phase splitter; supplies,
            heaters, relays)
        ORA-CD212 issue A, 23 Jun 2007, "50W valve power board"
    drawn by Paul Stevens, original circuit design by Adrian Emsley.

    Provenance, on every part line:
      [sch]  read off those sheets
      [int]  the sheets, interpreted where they leave a choice (said why)
      [ds]   from a datasheet or a published device model
      [doc]  a voltage the sheets print (test points), used to set a winding
      [est]  not on the sheets: estimated, and the next thing to measure
    The transformers' voltages are printed "???" on the sheets themselves.

    Not simulated, because no signal passes through them: the DC heater
    supply and the relay supply (both off the heater winding), the
    footswitch jacks. The relays are their contacts, as resistances the
    engine sets. The input jack's switch contacts are open (a guitar is
    plugged in) and the FX loop is normalled (nothing in it).

    Inputs:  IN 0  the guitar, volts at the jack
             IN 1  the mains, volts, at the power transformer's 240 V tap
             IN 2  the reverb tank's output, volts (from an impulse response
                   of the tank driven by output "tank"; 0 without one)
             IN 3  a plug in the loop's RETURN jack (the bench's generator)
*/
#pragma once

namespace mm
{

static const char* const TV50_AMP = R"NET(
* MARMALADE 50 - Orange Thunderverb 50 (ORA-CD209 1.1, ORA-CD212 A), mains to speaker
Vin    in    0     IN 0             ; the guitar

*==== INPUT (CD209 sheet 1) ===============================================
C19    in    ix    220n             ; [sch]
R43    ix    0     1meg             ; [sch]
R45    ix    g10a  68k              ; [sch] to preamp 1
R47    ix    g10b  68k              ; [sch] to preamp 2

*==== PREAMP 1: GAIN (dual), SHAPE (dual), VOLUME ==========================
X10a   p10a  g10a  k10a  ecc83      ; [sch] V10-A
R36    fp    p10a  100k             ; [sch]
R44    k10a  0     1.5k             ; [sch]
C20    k10a  0     22u              ; [sch]
C24    p10a  c24   1n               ; [sch]
R53    c24   n1    220k             ; [sch]
R59    n1    0     470k             ; [sch]
Rg1a   n1    g9a   500k             ; [sch] RV1-B, A1M0, gain (set by Engine)
Rg1b   g9a   0     500k
C29    n1    g9a   56p              ; [sch]
X9a    p9a   g9a   k9a   ecc83      ; [sch] V9-A
R38    ep    p9a   100k             ; [sch]
R48    k9a   0     2.2k             ; [sch]
C22    k9a   0     22u              ; [sch]
C26    p9a   c26   4.7n             ; [sch]
R55    c26   n2    220k             ; [sch]
R57    n2    0     470k             ; [sch]
Rg2a   n2    g9b   500k             ; [sch] RV1-A, the gain's other gang
Rg2b   g9b   0     500k
X9b    p9b   g9b   k9b   ecc83      ; [sch] V9-B
R39    ep    p9b   100k             ; [sch]
R49    k9b   0     820              ; [sch]
C37    k9b   0     100u             ; [sch]
* the shape network, DC coupled to the plate: RV2 is a dual B500K
R52    p9b   s1    100k             ; [sch]
C35    p9b   s3    56p              ; [sch]
C34    s1    s4    22n              ; [sch]
C33    s1    s5    22n              ; [sch]
Rsha   s3    s6    250k             ; [sch] RV2-A, pins 3 - wiper (set by Engine)
Rshb   s6    s4    250k             ;        wiper - pin 1
Rshc   s4    s5    250k             ; [sch] RV2-B, pin 4 - wiper
Rshd   s5    0     250k             ;        wiper - pin 6
R58    s5    0     15k              ; [sch]
Rv1a   s6    v1w   250k             ; [sch] RV3, A500K, volume
Rv1b   v1w   0     250k

*==== PREAMP 2: GAIN (dual), TREBLE, MIDDLE, BASS, VOLUME ==================
X10b   p10b  g10b  k10b  ecc83      ; [sch] V10-B
R37    fp    p10b  100k             ; [sch]
R46    k10b  0     1.5k             ; [sch]
C21    k10b  0     10u              ; [sch]
C25    p10b  c25   1n               ; [sch]
R54    c25   n3    220k             ; [sch]
R62    n3    0     470k             ; [sch]
Rg3a   n3    g8a   500k             ; [sch] RV4-B, A1M0
Rg3b   g8a   0     500k
C30    n3    g8a   100p             ; [sch]
X8a    p8a   g8a   k8a   ecc83      ; [sch] V8-A
R40    ep    p8a   100k             ; [sch]
R50    k8a   0     2.2k             ; [sch]
C23    k8a   0     10u              ; [sch]
C27    p8a   c27   2.2n             ; [sch]
R56    c27   n4    220k             ; [sch]
R63    n4    0     470k             ; [sch]
Rg4a   n4    g8b   500k             ; [sch] RV4-A
Rg4b   g8b   0     500k
X8b    p8b   g8b   k8b   ecc83      ; [sch] V8-B
R41    ep    p8b   100k             ; [sch]
R51    k8b   0     820              ; [sch]
C38    k8b   0     100u             ; [sch]
* the treble/middle/bass stack, DC coupled to the plate
R61    p8b   tl    22k              ; [sch]
C36    p8b   t3    560p             ; [sch]
Rtra   t3    tw    125k             ; [sch] RV7, B250K, treble (set by Engine)
Rtrb   tw    tm    125k
C31    tl    tm    22n              ; [sch]
Rbas   tm    tb    250k             ; [sch] RV5, A500K, bass: wiper tied to pin 3, a rheostat
Rmia   tb    mw    12.5k            ; [sch] RV6, B25K, middle
Rmib   mw    0     12.5k
C32    tl    mw    22n              ; [sch]
Rv2a   tw    v2w   250k             ; [sch] RV8, A500K, volume
Rv2b   v2w   0     250k

*==== CHANNEL RELAYS (G5V-1): each grounds the idle channel ===============
* RL4 through 47R at the top of its gain pot, RL1 through 47R at its
* volume's wiper. Contacts set by Engine: 0.1 ohm made, 1G open [int]. The
* one 47R at each relay's common is drawn here as two, one behind each
* contact: the same circuit whenever one contact is made (always), and it
* keeps the two channels apart, so each can be solved on its own [int]
Rrl4x  n1    rl4a  1g               ; [sch] RL4-A to preamp 1
R60a   rl4a  0     47               ; [sch] R60
Rrl4y  n3    rl4b  0.1              ;        RL4-A to preamp 2
R60b   rl4b  0     47               ; [sch] R60
Rrl1x  v1w   rl1a  1g               ; [sch] RL1-A to preamp 1
R11a   rl1a  0     47               ; [sch] R11
Rrl1y  v2w   rl1b  0.1              ;        RL1-A to preamp 2
R11b   rl1b  0     47               ; [sch] R11

* the two volumes into the loop's send valve
R3     v1w   mx    220k             ; [sch]
R4     v2w   mx    270k             ; [sch]

*==== FX LOOP (ECC81) ======================================================
C3     mx    g7a   220n             ; [sch]
R15    g7a   bb    1meg             ; [sch]
R16    dp    bb    220k             ; [sch]
R13    bb    0     33k              ; [sch]
C11    bb    0     22u              ; [sch]
X7a    dp    g7a   k7a   ecc81      ; [sch] V7-A, cathode follower
R14    k7a   0     22k              ; [sch]
C4     k7a   c4    220n             ; [sch]
R5     c4    snd   82k              ; [sch]
R12    snd   0     68k              ; [sch] the send
* the return jack (J2) normals the send to the return when nothing is
* plugged into it; a plug breaks that and drives the return (the bench
* does, with a signal generator, as a technician measures the power amp).
* Contacts set by Engine [int: the jack switching is on CD212]
Rjn    snd   rtn   0.1              ; [sch] J2's normalling contact, made
Vjr    rtj   0     IN 3             ; a plug in J2 (a generator; 0 when none)
Rjr    rtj   rtn   1g               ;        not plugged in
C1     rtn   c1    220n             ; [sch]
R6     c1    g7b   68k              ; [sch]
R19    g7b   0     1meg             ; [sch]
X7b    p7b   g7b   k7b   ecc81      ; [sch] V7-B, the return
R17    dp    p7b   56k              ; [sch]
R18    k7b   0     1.5k             ; [sch]
C5     p7b   lo    220n             ; [sch] LOOP_OUT

*==== REVERB (ECC81) =======================================================
R20    lo    0     220k             ; [sch]
R22    lo    g6a   220k             ; [sch]
X6a    p6a   g6a   k6a   ecc81      ; [sch] V6-A, the driver
R21    k6a   0     1.5k             ; [sch]
C12    k6a   0     22u              ; [sch]
R1     cp    rt1   15k              ; [sch] from C to the transformer's red lead
* the reverb transformer, plate (blue) to tank (black/green): not on the
* sheets. A 22k5 : 8 ohm valve reverb transformer into an 8 ohm tank input
Rrvp   rt1   rt2   500              ; [est] primary resistance
Lrvm   rt2   p6a   10               ; [est] primary inductance
Trv    rt2   p6a   tk    0   53.0   ; [est] sqrt(22.5k / 8)
Rtkin  tk    tk2   0.8              ; [est] the tank's input coil: 8 ohm at 1 kHz
Ltkin  tk2   0     1.2m             ; [est]
* the tank's output into the recovery valve
Vrev   rv    0     IN 2             ; the springs, from an impulse response
Rtko   rv    g6b   2.2k             ; [est] the tank's output coil
Rrl2   g6b   0     1g               ; [sch] RL2-A grounds the grid when the reverb is off (set by Engine)
R7     g6b   0     1meg             ; [sch]
X6b    p6b   g6b   k6b   ecc81      ; [sch] V6-B, the recovery
R24    fp    p6b   56k              ; [sch]
R23    k6b   0     1.5k             ; [sch]
C13    k6b   0     22u              ; [sch]
C14    p6b   rw    22n              ; [sch]
Rrva   rv3   rw    125k             ; [sch] RV9, 250KA, reverb: the signal on the wiper
Rrvb   rw    0     125k             ;        (set by Engine)
R65    rv3   mx2   220k             ; [sch]

*==== PHASE SPLITTER (ECC83, long tail) ====================================
R8     lo    mx2   1meg             ; [sch]
R64    mx2   0     1meg             ; [sch]
C2     mx2   g5a   47n              ; [sch]
X5a    p5a   g5a   kt    ecc83      ; [sch] V5-A
X5b    p5b   g5b   kt    ecc83      ; [sch] V5-B
R26    cp    p5a   82k              ; [sch]
R31    cp    p5b   100k             ; [sch]
R27    kt    tl5   680              ; [sch]
R25    g5a   tl5   1meg             ; [sch]
R28    g5b   tl5   1meg             ; [sch]
R29    tl5   fbn   15k              ; [sch]
R30    fbn   0     100              ; [sch]
C7     g5b   fbn   100n             ; [sch]
R9     spk   fbn   6.8k             ; [sch] -FB, from the 8 ohm tap (CD212)

* the attenuator: RV10, a dual B1M0 across each output, switched in by RL3 (G6A)
C6     p5a   at1   100n             ; [sch]
Rata   at1   aw1   500k             ; [sch] RV10-A, pin 1 - wiper (set by Engine)
Ratb   aw1   0     500k             ;        wiper - pin 3
Rrl3a  at1   rl3b  0.1              ; [sch] RL3-B: straight through (set by Engine)
Rrl3b  aw1   rl3b  1g               ;        or from the wiper
C10    rl3b  ps1   100n             ; [sch]
C8     p5b   at2   100n             ; [sch]
Ratc   at2   aw2   500k             ; [sch] RV10-B
Ratd   aw2   0     500k
Rrl3c  at2   rl3a  0.1              ; [sch] RL3-A
Rrl3d  aw2   rl3a  1g
C9     rl3a  ps2   100n             ; [sch]

*==== POWER AMP (CD212) ====================================================
R14p   ps1   nb    220k             ; [sch] R14
R15p   ps2   nb    220k             ; [sch] R15
R12p   ps1   g1a   1.5k             ; [sch] R12
R16p   ps2   g1b   1.5k             ; [sch] R16
X1     pa    sa    g1a   0     el34 ; [sch] V1, g3 and cathode to 0 V
X2     pb    sb    g1b   0     el34 ; [sch] V2
R13p   bp    sa    1k               ; [sch] R13, 5 W
R17p   bp    sb    1k               ; [sch] R17, 5 W

* output transformer: yellow and black to the plates, white the centre tap
* on A; red 16, brown 8, black common. Not on the sheets: the usual
* 3k4 plate to plate of a 50 W EL34 pair [est]
Rwa    pa    ota   40               ; [est] half-primary resistance
Rwb    pb    otb   40               ; [est]
Cpp    pa    pb    1n               ; [est] interwinding capacitance
Ta     ap    ota   sec0  0   10.3078 ; [est] 3k4 : 8 ohms, 20.6 : 1
Tb     otb   ap    sec0  0   10.3078
Lmag   sec0  0     73m              ; [est] magnetising, referred (31 H plate to plate)
Rcore  sec0  0     250              ; [est] core loss, referred
Lleak  sec0  sec   60u              ; [est] leakage, referred
Rsec   sec   spk   0.5              ; [est] to the 8 ohm tap

* a 4x12 on the 8 ohm tap, as the amplifier sees it [est] (the Marmalade 120's)
Rvc    spk   s1x   6.4
Lvc    s1x   s2x   0.7m
Rvce   s1x   s2x   20
Rmot   s2x   0     35
Lmot   s2x   0     20.6m
Cmot   s2x   0     152u

*==== POWER SUPPLY (CD212, CD209 sheet 2) ==================================
Vmains mn    0     IN 1             ; the 240 V tap of the primary

* HT: a centre-tapped winding (purple, purple, grey to 0 V), two 1N5408
* in series each side, 10n across the winding
Tht1   hta   0     mn    0   1.17524    ; [doc] each half: set by test/biasset.cpp for B at 390 V (TP7)
Tht2   0     htb   mn    0   1.17524
Rhta   hta   hxa   15               ; [est] winding + reflected primary resistance, each half
Rhtb   htb   hxb   15
C5h    hta   htb   10n              ; [sch] C5
D2     hxa   ap    1n5408x2         ; [sch] D10, D2
D3     hxb   ap    1n5408x2         ; [sch] D11, D3
C4h    ap    cm    220u             ; [sch] C4, 220u 400 V
C8h    cm    0     220u             ; [sch] C8
R8h    ap    cm    220k             ; [sch]
R9h    cm    0     220k             ; [sch]
Lch    ap    chx   10               ; [est] the choke (black to orange)
Rch    chx   bp    100              ; [est]
C7h    bp    0     100u             ; [sch] B
R6h    bp    0     220k             ; [sch]
R10    bp    cp    4.7k             ; [sch] 4 W
C15    cp    0     100u             ; [sch] C: TP8, 317 V
R34    cp    dp    4.7k             ; [sch]
C16    dp    0     47u              ; [sch] D: TP9, 270 V
R35    dp    ep    4.7k             ; [sch]
C18    ep    0     47u              ; [sch] E: TP10, 245 V
R42    ep    fp    10k              ; [sch]
C17    fp    0     22u              ; [sch] F: TP11, 218 V

* bias: the pink lead, half wave through 220k, 10u - 15k - 10u, then 47k and
* the 47k preset to 0 V. "Bias set fully anti-CW": the whole 47k [int: the
* preset's wiper is tied to its pin 1]
Tbs    bw    0     mn    0   1.34321    ; [doc] set by test/biasset.cpp for -50 V at TP5
C3b    bw    0     10n              ; [sch] C3
R4b    bw    bx    220k             ; [sch] R4, 1 W
D1     nr    bx    1n4007           ; [sch] anode toward the reservoir: a negative supply
C2b    nr    0     10u              ; [sch]
R3b    nr    nb    15k              ; [sch]
C1b    nb    0     10u              ; [sch] -BIAS, TP5
R2b    nb    pr    47k              ; [sch]
Rp1    pr    0     10.2k            ; [sch] P1, the 47k preset: the sheet's test points are with it fully anti-CW
*                                    ; (all 47k, -50 V); turned from there to idle each EL34 at 35 mA [est], test/biasset.cpp

.out   spk   spk
.out   tank  tk
.out   a     ap
.out   b     bp
.out   c     cp
.out   d     dp
.out   e     ep
.out   f     fp
.out   bias  nb
)NET";

} // namespace mm
