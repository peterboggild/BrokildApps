# DECISIONS

Physics and engineering choices, newest last. Each says what was chosen, what it
cost, and what would make it wrong.

---

## 2026-10-10 — the tape is a medium, 0.5 µm cells

Chosen: magnetisation stored per µm of tape, heads read and write at physical
positions (`docs/physics/01-tape-medium.md`). Rejected: a sample-domain delay
line with a computed delay time, which is what every tape-delay plugin does and
which would have to fake every effect this brief wants to emerge.

Cost: 8 MB per metre of stereo loop (96 MB at 6 m). The tape is allocated to
the CURRENT need on the message thread (a REEL window is 6.4 MB) and handed to
the audio thread at a splice, so a default instance stays small. First drafted
at 1 µm with linear interpolation; rejected because the grid alone lost ~8 dB at
20 kHz at the slowest speed, which would eat the real-time-vs-reference
tolerance before any physics ran.

Wrong if: a later stage needs more per-cell state than remanence (see the next
entry) — then memory grows with it.

## 2026-10-10 — one number per cell, even with Preisach

AC-biased recording leaves the coating anhysteretic, which a Preisach model
characterises by its remanence alone; the history staircase matters only while
the cell is inside the record zone, where it is computed, not stored. To be
checked against the stage 2 reference before stage 3 depends on it.

## 2026-10-10 — reference level

−18 dBFS RMS 1 kHz = 0 VU = **250 nWb/m** rms fluxivity. One unit of sample
amplitude = 250 / 10^(−18/20) = 1985.9 nWb/m at the default record level.
250 is an elevated studio operating level, appropriate for the 1970s
high-output stock that will be the default; the low-output vintage stock will
reach saturation sooner from the same input, which is the point.

## 2026-10-10 — internal rate and the dry path

The wet path runs at ≈ 88.2–96 kHz: ×2 at 44.1/48 k, ×1 at 88.2 k and above
(176.4/192 k hosts run ×1 too; the tape does not care). 63-tap half-band pair
(the fleet's, from `bwfx_dsp.h` via Rite of Passage). The dry path stays at the
host rate and the plugin reports **zero latency**; the half-band pair's delay is
compensated by reading each playback head the corresponding length of tape
*ahead* (`v · delay`), which is exact at constant speed. Measured in the bench,
not taken from the comment.

## 2026-10-10 — stage 1 feedback cannot self-oscillate

Containment must come from tape saturation (stage 3). Until then the loop gain is
capped: effective feedback = control × 0.97 / (largest boost of the tone stage).
No clipper, no limiter. Self-oscillation arrives with the hysteresis model.

## 2026-10-10 — parameter table with member pointers

Every host parameter is one row: id, name, range, default, kind, and a pointer to
its `Params` member. The APVTS layout, the processor's read, the initial state
for the panel and the bench all walk it, so there is no read order to get wrong
(the Hairfryer pattern), and rows can be added anywhere.

## 2026-10-10 — heads cannot pass each other

Three playback heads on one rail touch the tape on the same line, so a carriage
that runs into its neighbour STOPS against it (centres no closer than 1.2 cm);
the neighbour holds its place. The control keeps asking for where it was set;
the head's delay readout and the machine view show where it really is.
Rejected: heads passing through each other, which the first panel draw showed
and which no rail can do. To be confirmed with Peter at listening gate A.

## 2026-10-10 — stage 1 grid response, and the two corrections that made it flat

The writer reconstructs the record signal between samples with a Catmull-Rom
cubic (exact per-cell integrals), and a fixed 3-tap FIR undoes the read
integration, which is a box in time one internal sample wide at any speed.
Before: -1.19 dB at 16 kHz at every speed (straight-line writer + box).
After: -0.12 dB at 16 kHz at 19 and 76 cm/s; -0.65 dB at 4.75 cm/s, where the
cell transit is about one sample and stage 4's gap and spacing losses will be
tens of dB anyway.

## 2026-10-10 — input latency is paid at the record head

The internal latency splits into an input half (up half-band) and an output half
(down half-band, read box, correction FIR). The input half is compensated by
writing each sample where the tape WAS when it entered the box (a 64-sample
history of the odometer), the output half by reading ahead. Compensating the
whole latency at playback speed was measured 69 us wrong for an echo recorded
during a motor glide; split, 3 us.

## 2026-10-10 — the product is "Magneto Transport", title case

Peter: "for consistency, it should be Magneto Transport (not capital letters)".
PRODUCT_NAME, panel, splash, installer entry and docs renamed; BRIEF.md keeps his
original wording because it is verbatim. PLUGIN_CODE MgTr is unchanged, and the
class-id bytes were COMPARED in the old and new DLLs (identical in all three
occurrences), so a project saved with the caps build opens the renamed one.
Windows paths are case-insensitive, so the caps bundles were moved to
Quarantine before installing - installing over them would have kept the caps
folder name.

## 2026-10-11 — the reference's interaction field: squareness is NOT the knob

First density: coercivity lognormal + Gaussian interaction field, with squareness
0.80 supplied by interactions alone. That forced the interaction width to
0.78 Hc and the model was far too soft: MOL 1150 nWb/m against the sheet's 760,
and no rising part on the bias curve. In an oriented tape squareness comes mostly
from particle orientation, which in a scalar model is a moment factor, not an
interaction. Now: interaction FWHM 0.40 Hc (literature order for gamma-ferric),
Student-t tail shape fitted to the sheet's MOL, bulk S used only for the flux
scale. MOL then lands within 0.3 dB. Cost: the DCD-derivative SFD is 0.52, broader
than real tape, a known limitation of the classical model.

## 2026-10-11 — one number per cell is DEAD; stage 3 carries depth

The reference's test 6: three pre-states with identical flux, overwritten
identically, leave residuals of -106, -19 and -8.5 dB depending on how deep the
old recording went. What survives an overwrite is what lies below the new bias's
reach. So the real-time tape needs a depth structure per cell. Decision for
stage 3: **four layers per cell, each at its own resolution** - the deep layers
physically cannot hold short wavelengths (their record zone is long AND playback
thickness loss removes them), so the surface layer keeps 0.5 um cells and each
deeper layer doubles the pitch (0.5, 1, 2, 4 um). Memory 30 MB per metre of
stereo loop (180 MB at 6 m, 60 MB at the 2 m default) against 16 MB/m for one
layer. The design doc's "8 MB per metre" was wrong by two: one layer, two tracks,
0.5 um cells is 16 MB/m.

## 2026-10-11 — the stage 3 record model will be a TABLE measured by the reference

Per layer the record process is a function G(A, s, M_old): bias amplitude the
layer sees, local signal field at the recording zone, the layer's previous
remanence -> new remanence, plus a zone position and length per layer and bias
(the HF recording loss). The reference program grows a `--tables` mode that
measures G on a grid for each stock. Stage 3 interpolates it. A fitted formula
was rejected: the test-6 coupling between old and new state is exactly what a
formula would smooth over.

## 2026-10-11 — open: the sheet's two distortion numbers

The sheet gives HD3 0.1 % at 185 nWb/m and 3 % at 760. One tail parameter fits
the second and leaves the first at 0.19 %. A two-parameter density (flat core,
heavy tails) or a mean-field term would fit both. Left open: -55 dB instead of
-60 dB of third harmonic 2.6 dB under 0 VU is not what decides the sound, and
the brief says to report what is imperfect rather than tune it in.

## 2026-10-11 — stage 3's record model, as the reference measured it

Per cell, four layers (1.75 / 5.25 / 8.75 / 12.25 um, at 0.5 / 1 / 2 / 4 um
resolution). Per layer the record head does: s_zone = Gaussian kernel (centroid
ZONE_X and rms ZONE_W for the layer's bias amplitude A) applied to the deep-gap
signal field along the tape; then M_new = G[A][M_old][s_zone], quadrilinear.
A = hb * LAYER_FMAX. The kernel's centroid is a per-layer DELAY (the layer
records a few um downstream of the gap, more at higher bias), and the layers'
different centroids are most of the HF recording loss - keep them. Validation
target: RESP_DB per layer and bias, then the section 6 tolerances against
mtref renders. Rejected: a single depth-averaged kernel (loses the phase
between layers) and a fitted formula for G (smooths the old-new coupling that
test 6 showed).

---

# Stage 3 (2026-10-11)

## The reference's fresh tape was SATURATED — every stage 2 figure re-measured

A default Preisach staircase starts at negative saturation, and the stage 2
reference recorded every measurement onto tape in that state. No recorder does:
an erase head leaves tape AC-demagnetised. Found when the real-time model, whose
tables start from demagnetised tape, read 2 dB below the reference at 0 VU while
agreeing with it layer by layer. Fixed with `Stair::demag`; sections 2-6 of
05-reference-results.md re-measured. The largest consequence: bias erasure is
**-42 dB per pass, not -18** (the saturated deep layers had been surviving).

## The interaction tail is fitted on the full record process

On demagnetised tape the ideal anhysteretic curve is 1.3 dB optimistic about
MOL. nu is now bisected on the whole chain - align bias the sheet's way (10 kHz
3 dB past its peak), set 0 VU, find 3 % HD3 - until MOL = 760 nWb/m: nu = 2.059.
HD3 at 185 nWb/m then reads 0.070 % against the sheet's 0.10 % (open, 3 dB).

## Table entries are averaged over 8 bias phases

One cell sees one bias phase; its result carries a phase-specific offset that
differs from entry to entry, and the first real-time curve had a non-monotonic
third harmonic because of it. A real tape averages the phases.

## Four layers, each a SLAB averaged over 4 sub-depths

Point layers at the slab centres were 1.4 dB off the converged reference near
the bias threshold (the recording depth moves through the coating with bias).
Each layer's tables are now the average over 4 sub-depths of its 3.5 um slab -
16 depths, finer than the converged 8 - at no runtime cost. Bias curve error
fell from 1.25 to 0.19 dB. The price: an overdub sees the slab's average
history, not its depth profile inside the slab.

## The tables have a SPEED axis; the machine is aligned at 19.05 cm/s

At a fixed bias and field the reference records +0.6 dB at 19 cm/s and -1.3 dB
at 76 cm/s relative to 38.1 (a slower tape sees more bias cycles in the zone).
Tables measured at 4.76 / 9.53 / 19.05 / 38.1 / 76.2 cm/s, log-interpolated,
rebuilt as the motor glides (when the speed moves 0.2 %). The machine is aligned
at its nominal 19.05 cm/s, so unity at defaults holds; at other speeds level and
distortion move with the physics. The rail's zero is the operating-bias zone
centroid at the CURRENT speed, so delay = distance / speed at every speed.

## The real-time record process

Field ring at the gap -> per-slab zone kernel (measured centroid, fitted width;
see below) -> G[v][A][M_old][s] with the cell's old
remanence. G antisymmetrised (exact silence, no DC). Slices resampled to a
1025-point uniform s grid once per rebuild, so a cell costs one multiply, not
an asinh. Repro aligned so 250 nWb/m = 0 VU at operating bias and 19.05 cm/s.

## Feedback is uncapped

FEEDBACK_MAX = 1.5 small-signal loop gain at 100 %. Past unity the loop
self-oscillates and the tape holds it: with one head at 0 dB the output cannot
exceed full remanence, +0.16 dBFS (0 VU is -18 dBFS and full remanence is
+18.3 dB above 250 nWb/m).

## Known deviation, kept visible: HD3 at 0 VU

Real time 0.096 %, reference 0.143 % at 38.1 cm/s (-60 vs -57 dB re the
fundamental). Ruled out: table s resolution (41 -> 81 points: 0.001 % change),
depth sampling (slabs), bias phase (averaged). Above 0 VU they agree within
0.4 dB. The bench prints it as a KNOWN DEVIATION line, not a pass.

## Engine memory off the stack

The resampled slices and field rings are ~250 KB; held in the object they
overflowed the bench's 1 MB stack (exit 127, output lost). Now heap-allocated.

## Slab cells are point samples, and anti-aliased only where they must be

The first speed run lost 0.5 dB more at 15 kHz / 19 cm/s than the reference.
The zone shape was not the cause: a Gaussian width fitted to the kernel's own
transfer (6-40 um wavelengths, now what the tables carry) differs from its rms
by under 5 %. The cause was the cell width counted twice: the kernel took a box
for it, and playback already reconstructs each slab with a smooth curve through
its cells. Dropping the box: 19 cm/s within 0.18 dB, 38.1 within 0.07 dB.

The deepest slab carries ~3 % of the output, not the 0.2 % its table gain
suggests, so its coarse 4 um cells matter. Point-sampled, the coarse slabs
folded a 15 kHz tone at 4.75 cm/s to -25 dB. So a slab whose own Nyquist
(speed / two cells) falls inside the audio band gets its kernel widened to be
60 dB down there, faded in from 24 to 16 kHz: only the 2 um slab below 9.5 cm/s
and the 4 um slab below 19 cm/s. Folding now -86 dB, asserted in the bench;
nothing changes at 19 cm/s and above.

## Timing is checked with small signals

The sync check drove a +7 dB pulse and read 2.1 us late at 8 cm/s: at that level
the slabs' weights shift with the nonlinearity and so does the centroid. The
rail is defined at small signal (as the section 1 delay check is), so the sync
check now uses the same small pulse: 0.5000000 s.

## Cost

Kernel cut at 4 sigma (6e-5 of its mass) and both tracks in one pass over the
taps: 14.8 % of a core at the worst case (76 cm/s, three heads, feedback);
recording the cells ~8 %, reading them ~5 %.

## Stage 4: each slab is read through its own kernel (not one loss for the coating)

The playback head weights depth by e^{-k y}. One loss applied to the
depth-averaged tape reads 10.4 dB low at 15 kHz / 19 cm/s, because recording
loss has already emptied the deep slabs of treble. So each slab gets its exact
kernel, h(x) = [ln(x^2+bot^2) - ln(x^2+top^2)] / (2 pi D): zero-phase like the
physics. Pole cascades were measured and rejected (phase the head does not have;
5-7 dB off on the deep slabs). The kernel's 1/x^2 tail cannot be truncated: it
IS the thickness loss reaching into the bass (a cusp at k = 0). Near taps on
each slab's own grid, the tail as geometric boxes from prefix sums, one combined
track beyond the deepest slab's reach. Details: docs/physics/07-playback.md.

## Four uniform slabs get a fitted effective spacing

The reference composed the same recording through 4 uniform slabs and through
16 depths: 0.01-0.06 dB apart at 1 kHz, but the slabs read bright at the top
(+0.44 dB at 15 kHz, 19 cm/s), linearly in k. A spacing, so the correction is a
spacing: +0.10 um at 19-38 cm/s, fitted per speed by the reference into
stock/pb_456.h. Not an equaliser, and not typed here.

## The replay side is aligned per speed

Thickness loss costs level at 1 kHz too (-1.5 dB at 19 cm/s, -5.2 at 4.76). The
repro trim is set per speed at operating bias from the read's own discrete
response, so 1 kHz stays at unity whatever the speed (-0.006 dB). The record
side keeps its one alignment, so the tape's speed dependence stays audible.

## Bench tests that assumed a flat medium

The splice test used 500 Hz and read +0.72 dB: the thickness tilt, not a fault.
Unity is defined at the 1 kHz alignment, so it now uses 1 kHz. Sections 13-14
compare the record process with the reference's depth-averaged flux, so they
run with the head losses bypassed (Engine::setPlaybackLoss, bench only).

## Stage 4b: the equalisers follow a real alignment

Replay = the standard (IEC default, NAB alternative; time constants per speed,
log-interpolated) plus an HF trim for the playback head's own spacing and gap
(at most +10 dB), as a technician sets it with a calibration tape. Record
pre-emphasis = what makes the short-circuit flux follow the standard (at most
+20 dB), in the record amplifier so treble eats headroom. Both computed from
the machine's own small-signal model, realised as minimum-phase shelf cascades,
interpolated between speed buckets. Measured flat within 0.25 dB 40 Hz-15 kHz
from 19 cm/s up. Their phase is authentic, so timing tests run with them out.

## The self-oscillation claim moved to where it lives

"The output can never exceed full remanence" was true of the flat medium. With
replay equalisation the output may peak above that level (the harmonics of a
saturated tape are lifted, +2.8 dBFS at the test setting), so the check now
asserts what holds the loop: the tape's remanence never passes full, and the
oscillation has stopped growing. It settles near 414 Hz.

## Cost is measured best of three

The bench machine is not idle; one timing wandered by +-1.5 % of a core and
straddled the 15 % line. The engine's cost is the time it needs.

## Stage 4c: head bump, crosstalk, playback amplifier

The contour effect of a finite head is modelled from the potential steps at
its outer edges, H(k) = 1 - cos(kL) s(k): bump and cut toward DC, moving with
speed. Geometry assumed (the reference has infinite poles): L = 2 mm,
sigma = 1.05 mm. Not aligned away. Crosstalk: fringing over the guard band
plus a flat leak, symmetric (a minimum-phase lowpass moved an echo by 6 us
even at -50 dB). Playback amplifier rail +2 dBFS, on the output after
decimation (a clipped waveform overshoots the half-band filter otherwise).
Azimuth stays at zero for this well-kept machine: it belongs with wear.

## Near taps stay at 4x the slab's depth

3x saved CPU and measurably cost accuracy (0.27 dB off the reference at 19 cm/s,
a splice step, slow-speed flatness). Not worth it.

## CPU is reported, with a hard ceiling

Design section 6 sets CPU as targets with actuals reported. The same binary
reads 12.5-15.2 % across runs as the clock moves, even timed on the thread's own
CPU time, so the bench reports against 15 % and fails only past 20 %.

## Stage 5: transport mechanics from causes

The capstan motor is J w' = -K delta - D (w - target) for both kinds: an AC
synchronous motor (synchronising torque on angle, damper on slip; hunts at
about 1 Hz) or a DC servo (a PI loop on its tachometer). Wow and flutter are
the physical causes the controls set: capstan and roller runout at their
rotation rates, bearing roughness (3-30 Hz), motor ripple (twice the mains,
or the tachometer's slot rate), reel eccentricity and holdback friction as
tension, i.e. strain (EA about 1000 N), and stick-slip at each head as a local
displacement around the span's 3.2 kHz resonance. All deterministic from a
fixed seed, so a bounce reproduces. CONDITION 0 is exactly an ideal transport.

## Scrape is 50 nm worn out, not 3 nm

At 3 nm the sidebands around a tone sat below the hiss and SCRAPE did nothing
audible. 50 nm rms worn out puts them at about -55 dB around a 9 kHz tone,
which is the fuzz a dirty head gives; a weighted W&F meter still barely sees
it, as it should.

## The equalisers and record tables follow the motor, not the flutter

They take the motor's own speed. Re-deriving a table every flutter cycle
would be wrong as well as expensive: the flutter moves the tape under the
heads; the electronics do not change with it.

## The W&F meter waits out a speed change

A record-to-play meter reads a speed change as an enormous deviation for as
long as the lag lasts (3.4 % on the panel after a 38 -> 19 cm/s change,
decaying over seconds). A real meter would peg too, but a reading that says
the machine is worn when it is gliding is wrong information. It shows
SETTLING until the motor is within 0.3 % and the lag plus a second has passed,
then averages from scratch. The bench checks it through a 19 -> 38 cm/s step.

## The panel stays a placeholder

CONDITION and the three causes sit in a compact MECHANICS group with the W&F
reading; the real panel is stage 8.

## Stage 6: four stocks on three coatings

The vintage and budget coatings are their own reference runs (mtref --stock=),
not scalings of the studio tables: their particles answer a field differently
(wider coercivity spread, more interaction, poorer orientation), and that is
the record process. SHEDDING is the studio coating decayed: 12 % of the
particles gone, on the studio alignment, so it plays 1.1 dB quieter. All four
are 14 um thick, so the real-time slabs and head kernels are shared; a thinner
coating's extra bass loss is not modelled.

## The machine is aligned to each fresh stock

Bias at the coating's operating point, record level to its operating level
(250 nWb/m studio and shedding, 185 vintage and budget), record treble for its
surface. That is what a technician does on loading a new type of tape. A stock
change threads a new reel through the loop splice: old magnetisation written to
another coating's scale is not read back on this one.

## Both new coatings' MOL rest on the fit's floor

The interaction tail nu hit its floor (1.2) before reaching the MOL asked for
(vintage 420, budget 520 nWb/m); the models give 470 and 560. Those are the
numbers quoted and tested. Widening nu's range would need a re-examination of
the Student-t family's regime, for no audible gain.

## The hiss is particle statistics, written onto the tape

One draw per cell per recording, variance (1 - m^2) / N. It is on the tape, so
three heads hear the same particles and the feedback re-records them. It is
seeded by the unwrapped coordinate, so a loop's next pass is a new draw: the
bias re-randomises the orientations every time the tape is recorded over. The
first particle sizes (0.4 x 0.06 um) put the studio tape 4 dB noisier than 456;
a 1970s high-output oxide is nearer 0.28 x 0.045 um, which lands at -61.9 dBA re
250 nWb/m at 19 cm/s.

## The slabs' noise needs a colour

One value per slab column is noise coherent through 3.5 um of depth; real
particles are independent at every depth, and the head hears the shallow ones
better. Uncorrected, the hiss was 1.5 dB low at 20 kHz at 19 cm/s and 3.5 dB low
where a 5 um wavelength is read. A 5-tap FIR per slab along its cells restores
the continuous coating's spectrum (and undoes the slabs' signal spacing
correction, which does not apply to noise). Checked against a first-principles
spectrum written independently in the bench: within 0.6 dB, 250 Hz - 16 kHz.

## The spacing filter is exact, and its taps come from the response

A lift dd is a Poisson kernel in space, a Cauchy kernel in time: exp(-k dd)
exactly. Binning the continuous kernel into taps added a sample's box (-0.6 dB
at 20 kHz at 96 kHz); the taps are now the exact response's inverse transform,
the truncated tails' mass restored as a low-frequency fill. What a kernel this
short cannot carry is the cusp at DC: up to 0.39 dB less loss at 2 kHz for the
deepest lifts. Stated and bounded in the bench.

## The shaper reads ahead

A symmetric FIR delays; each head reads HeadShaper::R samples ahead of where it
is, at the tape's speed past it, and the shaper hands the sample back R samples
later. Every delay check in the bench is unchanged.

## Cost: the shaper in two stages, what is inaudible skipped

The first version convolved a 33-tap kernel with an 8-tap fractional delay at
every update and cost 5 % of a core on a worn transport. Two stages (the
spacing / box kernel, then the tracks' shift) make an update a table blend;
components that change nothing below 20 kHz by 0.05 dB are left out; one hash
gives both tracks' particle draws. Against the stage 5 engine timed side by side
on the same (busy) machine: the default 16.3 against 16.4 %, the worst case
(shedding, CONDITION 100) 16.6 - 17.9 against 16.4 %.

## Azimuth follows the transport's condition

A freshly serviced machine is aligned within about a minute of arc; a worn one
up to 8 arcmin per head, net of the record head's own tilt. The tape's weave
adds a slow part under REELS. It is a property of the machine, so it lives with
CONDITION rather than with the tape's WEAR.

## ACCUMULATE's wear belongs to a loop

Wear accumulated on a loop is kept with that stock and loop length, and saved
in the project; a different loop is a fresh tape. Resetting it at every splice
would lose it whenever a host restores state after preparing the plugin (the
restored loop arrives as a "change" and splices).

## Two new coatings' equalisers fit within 0.6 dB, the studio's within 0.5

The hard spot for every coating is the slowest speed's band edge (6 kHz at
4.6 cm/s), where the replay trim climbs toward the gap null. The studio coating
fits within 0.44 dB (the flatness claim); vintage 0.55 and budget 0.58, asserted
under 0.75.

## The placeholder panel has two rows

Seven groups do not fit one row. The machine view is shown at 87 % (its own
coordinates unchanged, so the head dragging still maps), and the groups wrap.
The real panel is stage 8.

## Stage 7: the erase head is the record process with no signal

What a cell keeps under the erase head is the reference's G at s = 0 and the
Karlqvist field at its depth, tabulated per layer for the current field and speed.
No separate erasure law: the erase head and bias erasure are the same physics as
recording, which is the point of a medium model. Past 2.2 Hc it simply wipes, and
at zero it is exactly off (the ramp snaps, since a hair of field would nudge every
cell of a frozen loop on every pass).

## The tables' old states are what the machine leaves

Bias erasure is G's dependence on the old remanence, so the tables have to know
what an old recording is. Prepared as ideal anhysteretic states (a 5 Hc decaying
field), they magnetised hard particles no later bias pass reaches: the real-time
bias erasure read −25 dB against the reference's −34.5. Now each old state is a DC
level recorded at the operating bias, per sub-depth and bias phase, bisected to the
slab's mean remanence, on a per-slab grid spanning what +12 dB over 0 VU records
there (the deep slab's span is a quarter of the top's). With 64 amplitude points:
−31.3 dB. Fresh-tape recording is untouched, so every earlier figure stands.

**Not yet within the bench's tolerance**: bias erasure −31.3 against −34.5, recording
over with 3 kHz −36.2 against −44.6, and the erase head 16.6 dB gentle at 1.5 Hc.
These wait for BUGLIST 8, because that work changes the reference itself (the
particles' interaction field), and fitting the real time to the current reference
first would be work thrown away.

## REPRO reports where head 1 actually is

A tape machine's delay is head 1's distance over the speed. The carriages keep
1.2 cm apart, so heads 2 and 3 can push head 1 back from its knob; REPRO measures the
settled carriage (velocity and position both still) and reports that to the host,
from the message thread. The dry path goes through a ring of the same length so a
blend of tape and dry lines up, crossfaded over 20 ms when the length changes.
Heads 2 and 3 and the feedback are off in REPRO. Bench: reported within half a sample
and the tape arrives within 0.6 samples, at three rates and three speeds, near, far
and pushed back.

## FEEDBACK is divided by the number of heads on

Echoes arriving in phase add on the feedback wire, so two heads at 0 dB halved the
runaway point and three took it to a third (25 % and 17 %). The wire is divided by
the NUMBER of heads that are on, not by the sum of their levels: dividing by the
levels would make a head turned down feed back more of itself. FEEDBACK_MAX 1.5 ->
1.1 puts the runaway near 68 % at 19 cm/s with one, two or three heads (71 % at
4.75, 86 % at 76 cm/s); 100 % still runs away everywhere.

## TRANSPORT is REEL | LOOP, and the loop's controls live together

LOOPER was the same loop with its record path made a control, so it is the loop's
LOOP STATE (REC | DUB | PLAY) rather than a third machine, and FREEZE went (it was
PLAY). DUB records with the erase head at the ERASE knob, so DUB at 100 % is the old
LOOP and DUB at 0 the old OVERDUB. ERASE's taper follows the effect (0 off, then
geometric in the reference's one-pass loss from 0.6 to 60 dB over 90 % of the knob,
then on to a wipe). On a REEL every loop control is inert, bit for bit. Not
released, so there are no projects to migrate.

## Every pass of the loop's heads is one shot

The reference, asked directly (mtref --dub), takes exactly the same from a take on a
second identical pass as on the first: an erase field or a bias reaches what it
reaches, and the survivors are what it cannot. One remanence number per cell had no
memory of that and faded a layer on every pass, 35 dB where the physics takes 3.5
once. The tape store now carries the committed part of each cell, the retention paid
on it (a quarter-dB index) and the reach the material on top was recorded at;
the pass's loss comes from two reference tables (the committed take's and the
overdub's, because an overwrite depends on how deep the old recording went) and is
applied only above what was paid. 2.25 x the memory. The first-stage erase LUT, read
off the record head's G with the record head's depth profile, is gone with it: a
60 um gap's field is nearly uniform through the coating, and the pair table is
generated with the erase head's own profile (16.6 dB off at 1.5 Hc before).

## DUB is sound-on-sound, because that is what tape can do

Overdubs do not stack on one track: what is recorded at a bias is replaced by the
next pass at that bias (the reference's E3, -24 dB at 0 VU at half bias). What
survives is a take recorded at a HIGHER bias than the pass: at half bias it keeps
-3.5 dB, permanently, bass best. So DUB drops the bias 6 dB and keeps the erase head
at the ERASE knob: the take committed in REC stays under what is played, and what is
played is the latest overdub with a ghost of the one before. REC (erase full, the set
bias) is the default LOOP STATE, bit-identical to the plain echo it was. The
mean-field term the design had listed was examined and dropped (10-flux-memory.md).
A looper's indefinite stacking is FEEDBACK's job, which is how tape musicians did it.

## OK -> BROKEN moves causes, not effects

Every deterioration here is already a cause, so BROKEN moves the causes in a straight
line from where the knobs are towards a wreck (CONDITION, CAPSTAN, REELS, SCRAPE, WEAR
up, the bias oscillator sagging, the erase head weakening on a loop), and the panel
puts a dot on each knob where it acts. Below the middle it is exactly those knobs
turned by hand, bit for bit, including at start-up (prepare seeds the bias smoother and
the medium from the effective knobs). A bad tape is mostly subtractive, so the far end
adds three real failures past the middle, each a cause too: oxide clogging a playback
head (spacing, so the treble goes first), the speed control hunting (a limit cycle
through the motor's own dynamics), and a crinkled stretch (spacing, lost coating and a
twist across the gap, plus a speed jolt and harder stick-slip as it passes the heads).
The erase head only weakens on a loop: on a reel it meets blank tape, and weakening it
there would let the ring buffer's previous lap through, which no reel has.

## Stage 8: the panel is generated around three data files

The page is a pure view (docs/PROTOCOL.md). The tooltip cards (BUGLIST 6) read three
files the plug-in embeds, and none is typed by hand: `tips.json` from
`docs/tips/tips.src.json` by `tools/build-tips.js` (TeX to MathML Core, which
WebView2 renders natively; it fails on a missing card, a bad equation, an unknown figure
or number); `tips-numbers.json` written by the bench (`mttest --tips=`), every
measured number a card quotes set beside the check that measures it; and
`figdata.json` merged by `tools/build-figdata.js` from the reference
(`mtref --figdata`: hysteresis, anhysteretic, bias curve, DUB survival), the bench
(`mttest --figdata`: record zones, hiss, W&F, distortion against speed, EQ curves,
the erase transition and the knob's field) and closed forms (Karlqvist, playback losses,
delay geometry, the feedback loop). Figures carry a live marker bound to a parameter or
the telemetry. A retuned model cannot leave a stale claim on a card.

## The quality tiers change the internal rate only

ECO runs the tape at the host rate, NORMAL at 2x below 80 kHz, HIGH at 4x below 80 kHz
(two half-band stages each way). A change threads a fresh tape (the internal rate is
the tape's clock); it is saved with the project and remembered as the user's default.
**Measured at the worst case (76 cm/s, three heads, feedback, hiss): ECO about 15 %,
NORMAL about 20 %, HIGH about 30 % of one core.** ECO saves a quarter, not half: at
76 cm/s most of the work is per centimetre of tape (cells recorded and read, 2.8 M a
second), which the internal rate does not touch. The design's ECO target of 8 % is
not met; at 19 cm/s NORMAL is about 12 %. Aliasing of a 9 kHz tone 12 dB over 0 VU at
48 kHz: ECO -37, NORMAL -63, HIGH -63 dBFS (what is left is not oversampler aliasing:
the same spur shows at a 96 kHz host with no decimation).

## A nested call is not a sequence

The first 4x cascade drooped 1.3 dB at 9 kHz and 3 dB at 20 kHz and aliased worse than
2x, because `d.down (d2.down (a, b), d2.down (c, d))` leaves the order of the two
stateful calls to the compiler, and MSVC evaluated the later sample first. Sequenced
into named temporaries it is flat within 0.01 dB. The fleet learned this in Hats Off;
`test/hbcheck.cpp` now measures the cascade alone.

## Bypass is ours, and stays in time

The host's bypass is the plug-in's BYPASS parameter (`getBypassParameter`): a 20 ms
crossfade to the dry signal delayed by exactly the latency the host is told, so a
bypass in REPRO stays in time; the tape runs on underneath. Measured bit-exact in the
bench and within 1e-6 through the real VST3 wrapper (`test/host`).

## Presets are levelled to what goes in

24 factory presets in six groups, also the host's programs. Each preset's OUTPUT is
written by `test/presetlevel.cpp` into `Source/PresetTrims.h`: K-weighted loudness of
a synthetic band played through it equal to the programme's own, so a bypass A/B is a
level match. Levelling to the default machine instead would have put the tape-only
presets 4 dB above unity. User presets are JSON files in the house folder
(`Documents\Brokild patches\Magneto Transport`); a deleted one moves to
`Deleted`. BYPASS is never part of a preset.

## HEAD PATTERN snaps in rail order

SET writes the three position parameters once, from the pattern's ratios around the
tempo head, in RAIL order (from where the carriages actually are: heads cannot pass, so
a head parked to the right of another must take the larger ratio). The carriages then
travel with their own inertia. The panel shows live which heads a pattern would push
off the rail.

## The smoothing audit can fail

Every continuous control jumped across 80 % of its range mid-tone, energy above 8 kHz in
5 ms windows at the jump against the rest of the render: all at or below 1x. The metric's
own self-test (an unsmoothed 6 dB step on the same sine) first read 1x, because the jump
fell exactly on a zero crossing of the 440 Hz tone; moved off it, a real click reads 98x.

## The cost gate reads a warm CPU

At the end of the full bench (about twenty minutes of load) NORMAL read 20.7 % and
failed the 20 % ceiling; run alone straight after, the same binary read 17.2 % and
19.5 %. The part-by-part probe (mtcost) found no regression: the medium's hiss is
about 5 % of a core at 76 cm/s, as in stage 6. The ceiling stays; a cost failure at
the end of a full run is re-measured with `mttest -s cost` before it is believed.
