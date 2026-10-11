# Magneto Transport — design

The final project: a physically modelled tape echo in which every artefact comes
from simulated mechanics and magnetics, plus FLUX MEMORY, a variable erase head
on a finite tape loop.

Peter's brief is kept verbatim in `vst3-apps/magneto-transport/plugin/docs/BRIEF.md`.
This document is the brief **plus his answers of 2026-10-10**, the decisions those
answers force, the tolerances, and the build order. Where this document and the
brief disagree, this document wins: it is the later word.

Source: `vst3-apps/magneto-transport/plugin/` (built outside Dropbox at
`C:\Users\peter\b\_build\MagnetoTransport\`). Engineering log:
`plugin/docs/DECISIONS.md`. Physics notes, one per component, written before
the component is coded: `plugin/docs/physics/`.

---

## 1. Identity

| | |
|---|---|
| PRODUCT_NAME | **Magneto Transport** (also the patch folder name — house rule) |
| target | `MagnetoTransport` |
| plugin code | **`MgTr`** (checked unique against the 26 codes in the repo) |
| manufacturer | `Brkd` |
| formats | VST3 + Standalone |
| category | Fx / Delay |
| build id | `MT_BUILD_ID`, YYMMDD.n, shown on the panel |
| panel | WebView2 + canvas/WebGL, static WebView2 loader + runtime check (Thin Walls) |
| BWFX | **none** — it is an effect (the Martian Gain / Battlestar call) |
| collection | **undecided — Peter tests first.** Experimental or Brokild depending on whether it meets the demands |
| licence | AGPLv3 like the rest of the repo |

---

## 2. Peter's answers (2026-10-10)

| question | answer |
|---|---|
| hiss vs mastering-clean | **particle count is a control**, continuous down to effectively silent |
| real-time bias | **validated equivalent**: explicit bias in the offline reference only; real time uses anhysteretic recording with a bias-dependent recording zone, proven against it |
| print-through / self-demag | **dropped** (physically inaudible at loop timescales) |
| panel | WebView2 + canvas/WebGL |
| hysteresis | **Preisach-type** |
| mechanical sounds | last — but the transport exposes its state from stage 1 so they integrate without rework |
| reels vs loop | **two transport modes**: REEL (open reel, radius changes, endless) and LOOP (closed loop, FLUX MEMORY) |
| speeds | studio range **4.75–76 cm/s**, continuously variable |
| CPU | sound first; offer **ECO / NORMAL / HIGH** |
| prior art | nice to have, not first priority |
| reel end | effectively endless; quiet automatic rewind |
| tolerances | **I set them** (§6) |
| listening | **pause at four gates** (§7) |
| presets | ~24, grouped clean → strange, loudness-levelled |
| sync | the user chooses one playback head as the **tempo head**; the motor runs at the speed that puts it on the note value; tempo changes glide with motor inertia |
| repo | BrokildApps, `vst3-apps/magneto-transport/plugin/` |
| start | design doc, then stage 1 now |
| model split | I flag the hand-offs where the math is done |
| **name** | **"Magneto Transport"**, title case (2026-10-10, second round) |
| wow and flutter | **TRANSPORT CONDITION macro + three cause controls** (CAPSTAN = flutter, REELS/TENSION = wow, SCRAPE = head friction) and a measured DIN/IEC W&F meter. Causes only, never modulation depth |
| tape stocks | **all four**: 1970s high-output studio (456 class, default), vintage low-output, worn/shedding, budget/echo-machine |
| EQ standard | **selectable, IEC default** (NAB the alternative) |
| wear | **static knob + ACCUMULATE switch** (off by default): a LOOP physically wears per pass, saved with the project |
| capstan motor | **selectable, AC synchronous default** (DC servo the alternative) |
| LOOPER / FLUX MEMORY controls | **host parameters**: automatable and MIDI-learnable |
| HEAD PATTERN | **approved, built in stage 8** |
| looper | **approved** (third transport mode), built in stage 7 with FLUX MEMORY |
| GitHub | **install on this PC at every gate; push only when Peter says** |
| fresh instance | **a well-kept studio machine**: real 456 hiss (~-70 dB under 0 VU), freshly serviced transport (W&F ~0.03 %), optimum bias |
| mechanical sounds | **main output with their own level**, off by default |
| away for hours (2026-10-11) | **go on to stage 8 without the listening gates**, each stage its own commit + install so any gate can be heard and stepped back; then the stage 9 decal brief; stage 10 stays local; nothing pushed |
| model while away | **Fable** for the delicate math (stages 4-5, stage 7 erase tables) |

---

## 3. The architectural decision: the tape is a MEDIUM

The tape is simulated as **an array of magnetisation along its length**, not as a
delay line of samples. One cell = **0.5 µm** of tape, per track.

Everything the brief asks to "emerge" follows from that one choice:

- a head reads the material under it: **delay = distance / speed** is not a
  formula in the code, it is what happens;
- the motor changes speed while echoes are on the tape → they **pitch-shift**;
- a head moving on its rail reads a different place → **Doppler**, and reading
  backwards if it outruns the tape;
- gap, spacing and thickness losses are **spatial filters at the head**, so
  they scale with speed automatically;
- the tape's resolution is in micrometres, so behaviour is **independent of
  the host sample rate**;
- FLUX MEMORY is simply **not erasing a cell** before the record head reaches it.

Coordinates: the odometer `T(t)` is the length of tape that has passed the
capstan. A material point with tape coordinate `u` sits at path position
`x = T − u`; a head at path position `h` sees material `u = T − h`.

- **LOOP**: cells form a ring of the loop's length (0.5–6 m → 1–12 M cells/track).
- **REEL**: only the stretch between the supply side and the last playback head
  can ever be read again, so the reel is a ring the size of that window; cells
  are returned to **virgin** state as they come off the supply reel. The tape
  stock profile for the whole 730 m pack is procedural (a hash of position), so
  nothing of that length is stored.

Memory: 16 MB per metre of stereo loop for one layer; stage 3 carries four layers at halving resolution, 30 MB/m (180 MB at 6 m, 60 MB at the 2 m default), allocated to the current need. Noted in
DECISIONS.

Per-cell state: remanent magnetisation `M` (a fluxivity, nWb/m). Why one number
is enough even with Preisach: AC-biased recording leaves the medium in an
anhysteretic state, which the Preisach model characterises by its remanence
alone; the full staircase only matters *during* the record process, which is
computed at the head, not stored on the tape. (Stage 2 checks this claim against
the reference model before stage 3 relies on it.)

---

## 4. The machine

Fixed head order along the path: **supply → erase → record → playback 1–3 → capstan**.

- **Record head**: fixed, path 0.
- **Erase head**: fixed, 2.5 cm upstream of record.
- **Playback heads**: each on the rail, **1.5–24 cm** downstream of record, moved
  by a carriage with real inertia and a top speed. At 4.75 cm/s a 24 cm head is
  5 s of delay; at 76 cm/s a 1.5 cm head is 20 ms.
- **LOOP length 0.5–6 m.** The head span is clamped below the loop length.
- **Speed 4.75–76 cm/s**, set by the motor. Sync: tempo head + note value →
  target speed = distance / note duration, clamped to range (the panel says when
  a note value is out of reach).
- **Feedback** is a physical wire: selected playback heads → tone (bass/treble)
  → back into the record amplifier.
- **I/O**: mono→mono, mono→stereo, stereo→stereo, from the host layout plus an
  INPUT STEREO/MONO switch. Two-track head stack; crosstalk and azimuth come in
  stage 4.

Reference level: **−18 dBFS RMS sine at 1 kHz = 0 VU = 250 nWb/m** (rms
fluxivity). The tape stores nWb/m; the electronics convert at the documented
gains.

---

## 5. Physics plan, by stage component

Each gets a note in `docs/physics/` before code: equations, parameters, sources.

- **Hysteresis**: Preisach with a Lorentzian/Gaussian Everett distribution fitted
  to γ-Fe₂O₃ (coercivity, remanence, squareness, saturation). Minor loops close
  by construction — the reason it was chosen: FLUX MEMORY rewrites the same tape
  thousands of times, and Jiles–Atherton's open minor loops would drift.
- **Record process (reference)**: tape cells advected through the Karlqvist gap
  field of the record head, signal + AC bias at a realistic frequency (~100 kHz),
  integrated at MHz rates offline; remanence frozen where the field falls below
  the switching fields (the recording zone).
- **Record process (real time)**: anhysteretic recording: remanence = a function
  of the signal field at the recording zone, whose position and width move with
  bias — which is what makes over-bias lose treble and under-bias distort,
  physically. Fitted to and validated against the reference.
- **AC bias erases**: recording over old material with bias on attenuates it.
  This is what sets how fast FLUX MEMORY's ghosts fade (with the erase residue).
  Freeze turns bias off as well as signal.
- **Playback**: gap loss, spacing loss (exp(−2πd/λ)), thickness loss, head-contour
  bumps, azimuth, wear; IEC/NAB equalisation per speed (decision in stage 4 on how
  a continuous speed picks its time constants).
- **Transport**: capstan motor (AC synchronous or DC servo with a regulation
  loop), rotor inertia, cogging, eccentricity, bearings; tape as an elastic medium
  between contact points; stick-slip at the heads → scrape flutter; reels with
  changing radius and inertia (REEL mode).
- **Stock**: particle count and size distribution → hiss (statistical: every
  random draw has a variance derived from physical parameters and is **seeded by
  tape position**, so a reel behaves the same every time), coating thickness
  variation → modulation noise, surface roughness → spacing fluctuation and
  dropouts. Particle count is a user control down to effectively silent.
- **Erase head**: field strength and decay → incomplete erasure possible.

---

## 6. Tolerances (set by me, per Peter)

Reference = offline model (no shortcuts). Real time = what ships.

| quantity | tolerance | why |
|---|---|---|
| unity gain at reference, defaults | **±0.05 dB** | "no hidden gain" |
| frequency response, real time vs reference, −20 dB re ref, 20 Hz–16 kHz | **±0.25 dB** | below audible level differences |
| THD at 1 kHz and 100 Hz, −20…+12 dB re ref | **±1.5 dB** of the reference's figure; MOL within **±0.5 dB** | harmonic level, not phase, is what is heard |
| programme material (sines, sweeps, drums, voice) | 1/3-octave band levels within **0.5 dB**, 50 ms envelope error **< 0.5 dB rms** | hysteresis models null poorly sample by sample even when they sound identical; band levels and envelopes are the perceptual test |
| delay time | **< 1 µs**; pitch **< 0.5 cent** | |
| wow & flutter (DIN/IEC weighted) | within **10 %** of the reference | |
| noise spectrum | 1/3-octave within **1 dB** | |
| sample-rate identity (44.1–192 kHz) | the same tolerances as real time vs reference, below 16 kHz | |
| aliasing, 44.1 kHz, NORMAL, particle count at silent | **< −100 dBFS** | brief |
| CPU at 48 kHz stereo (targets; actuals reported) | ECO **< 8 %**, NORMAL **< 15 %**, HIGH **< 30 %** of one core | sound first |

---

## 7. Stages and gates

Every stage ends with its bench clear. **LISTEN** = stop, install, Peter listens.
**FABLE** = the delicate math is done; I say so for a model switch.

| # | stage | gate |
|---|---|---|
| 1 | tape as a medium, ideal heads, simple motor glide, both transport modes, the feedback wire, placeholder panel | **LISTEN A** — a working delay with real Doppler |
| 2 | physics notes + offline reference: Preisach ferric + explicit-bias record process | FABLE - **DONE 2026-10-11**, `docs/physics/05-reference-results.md` |
| 3 | real-time record process validated against 2; saturation contains feedback; bias live | **LISTEN B** - built 2026-10-11 (261011.2): bench 67/67, awaiting the listen |
| 4 | playback physics, EQ standards, electronics, gain staging, stereo crosstalk/azimuth | built 2026-10-11 (261011.3): playback within 0.16 dB of the reference, aligned machine flat within 0.25 dB |
| 5 | transport mechanics: motor, capstan, bearings, elastic tape, scrape flutter, reels | FABLE, **LISTEN C** - built 2026-10-11 (261011.4): bench 110/110, awaiting the listen |
| 6 | tape stock: particle statistics → hiss, modulation noise, dropouts; stocks, age, wear | built 2026-10-11 (261011.5): bench 141/141, hiss within 0.6 dB of the continuous coating |
| 7 | FLUX MEMORY: partial erase, bias-erase overwrite, the LOOP panel (REC / DUB / PLAY, CLEAR, LOOP BARS; BUGLIST 2 and 7); also REPRO (4), the feedback law (3), OK / BROKEN (5) | in progress 2026-10-11 (261011.8): built and benched apart from the erasure depths, which wait for the overdub physics (BUGLIST 8, Fable); **LISTEN D** after that |
| 8 | panel driven by the simulation, quality tiers, presets, smoothing, bypass, latency, host test, pluginval if available | |
| 9 | decal brief → **stop**; integrate on return | |
| 10 | mechanical sounds, manual, landing page, zip; README draft for approval | |

Prior-art check: when a slot appears, before any page says "never".

### Checks every stage inherits (from the fleet)

- **every control audit** — each control measurably changes the output in a context chosen for it;
- **stop the input and watch** — bounded is not stable;
- **render long enough to reach the effect under test** (a loop can be 30 s);
- **ear beats proxy metrics** — when Peter hears a fault, render one change per file;
- build id visible; probe the **installed** file past Smart App Control.

---

## 8. Stage 1 specifics (temporary, removed in later stages)

- Ideal record: the signal is written into the tape as its time-average over each
  cell's transit (a box filter; a real record zone replaces it in stage 3).
- Ideal playback: the head output is the tape's average over the span it crossed
  during the sample (the integration that keeps fast reads from aliasing; gap and
  spacing loss come in stage 4).
- **Feedback cannot self-oscillate in stage 1**: the loop gain is capped below 1,
  because containment must come from tape saturation (stage 3), not a limiter.
- Motor: a first-order speed response (0.35 s) standing in for the stage 5 motor.
- Erase: full. FLUX MEMORY is stage 7.
- Changing the loop length or the transport mode **splices a new loop** (fade,
  clear, resume) — honest, since a real machine cannot stretch a loop.

---

## 9. Stage 2 brief — the offline reference (written for the Fable session)

Stage 1 is done (bench 53/53, listened to and liked). Stage 2 is the delicate math.
**Write `docs/physics/03-hysteresis.md` and `04-record-process.md` first**, then the
reference program `plugin/reference/` (target `mtref`, plain C++, no JUCE, its own
CMake beside `test/`). It does not ship; stage 3 is fitted to it.

### The stock to model first: a 1970s high-output studio ferric (Ampex 456 class)

Published figures (Quantegy 456 data sheet as quoted by the searches below; the PDF
itself would not render here, so re-check if a readable copy turns up):

| | 456 | in model units |
|---|---|---|
| coercivity | 330 Oe | Hc = 26.3 kA/m |
| retentivity | 1460 G | Mr = 116 kA/m (Br / mu0) |
| oxide coating | 0.55 mil | delta = 14 um |
| MOL (3 % third harmonic, 1 kHz) | +12.3 dB re 185 nWb/m | 760 nWb/m rms |
| 3rd harmonic at reference level | 0.1 % | |
| reference level of the sheet | 185 nWb/m; measured at 15 ips | |

Plausibility, before any code: mu0 * Mr * delta = 4pi e-7 x 116e3 x 14e-6 = 2040 nWb/m
at long wavelength and full remanence. MOL 760 rms = 1075 peak = 53 % of that - the
right neighbourhood for 3 % third harmonic. Our 0 VU (250 nWb/m) is +2.6 dB re 185,
so MOL lands about +9.7 dB above OUR reference. Squareness and SFD are not on the
sheet: take squareness ~0.8 (Ms ~145 kA/m) and SFD ~0.35 as typical acicular
gamma-Fe2O3, state them as assumptions, and let the MOL and the 0.1 % figure be
what they are tested against.

Sources: [zZounds 456 listing](https://www.zzounds.com/item--AMX456173111),
[456 data sheet copy](http://stereo-hifi.com/wp-content/uploads/2017/10/Quantegy_456.pdf).

### The model

- **Preisach** with a factorised density: lognormal hysteron coercivity hc (median ~Hc,
  width from SFD) x Gaussian interaction field hu (sigma fitted to squareness).
  Precompute the **Everett function** E(alpha, beta) on a grid (bilinear lookup);
  per cell keep the **alternating-extremum staircase** (wiping-out rule), and
  M = Ms (2 A+ / A_total - 1). Minor loops close by construction - check it:
  10 000 cycles between fixed fields must return the same M to rounding.
- **Coating in depth layers** (~8 across 14 um), each with its own state per cell.
- **Record head = Karlqvist** field Hx(x, y) (gap ~6 um, spacing ~0.2 um),
  Hg proportional to (signal current + bias current). Bias **150 kHz** (studio
  practice; the brief said "~100 kHz"), integrated at >= 32 steps per bias cycle
  (~4.8 MHz). Each tape cell is advected through the field independently (cells do
  not interact in a scalar Preisach model), so it parallelises over cells.
- Ignore demagnetising fields and vertical-field (vector) effects in the reference,
  and SAY SO; both are candidates for later if the HF numbers disagree with data.

### What `mtref` must measure (each is a stage 3 target or a check)

1. The model's own major loop: Ms, Mr, Hc, squareness, SFD vs the targets above.
2. Bias curve: 1 kHz and 10 kHz output vs bias level; define 0 dB bias as the
   industry alignment (peak 10 kHz output, then overbias by the 456's +3 dB setting
   at 15 ips) and record where that lands.
3. 1 kHz sine at -20 .. +15 dB re reference: depth-averaged flux, THD, **MOL**,
   the 0.1 % at reference - against the sheet.
4. HF response vs wavelength at under / optimum / over bias (over-bias must lose
   treble because the recording zone moves deeper - the physical reason).
5. **Bias erasure**: record, then a second pass with bias only (no signal): the
   residual in dB. This is what FLUX MEMORY's and the LOOPER's overdub decay will be.
   Then the erase head (strong decaying AC): erase depth.
6. **The one-number-per-cell claim** (DECISIONS): two cells with the same remanence
   but different histories, recorded over identically with bias + signal - do they
   end the same? If not, stage 3 needs more state per cell.
7. Cost per second of tape at 0.5 um cells, so the stage 3 equivalent knows its budget.

### Traps already paid for (see CLAUDE.md for the long versions)

- Write patch scripts and anything with a backslash with the Write tool, never a
  bash heredoc or `node -e`.
- A failed build runs the STALE exe - check the build exit code before believing numbers.
- Run fresh exes through `tools/run-sac.sh` (Smart App Control).
- Render long enough to reach the effect under test; a probe can lie before the code does.

---

## 10. Stage 9 — the decal requests (Peter, 2026-10-10)

*"at the end when building amazing panel, you can make a precise list of decal
requests that i can have done with ChatGPT."* He has no ChatGPT subscription now
and will get the art made when the list exists, so the list must work on its own,
pasted cold, by someone who has seen none of this.

`plugin/docs/decal_brief.md`, written once the panel layout is frozen. Each request
carries what the brief asked for (sizes at 1x and 2x, transparency, file name,
placement, materials, one light direction, wear, hex palette, exact lettering,
separate layers and pivots, a master style paragraph) PLUS what the fleet learned:

- **Order isolated parts, never a registered faceplate**: an image generator cannot
  hit pixel coordinates. The panel's geometry table places the parts; the art is
  fitted to it.
- **Aspect is what matters, not resolution**; anything that rotates gets a square
  canvas with the pivot at the centre.
- **Ask the deliverer to measure the drawing's bounding box against the canvas**
  before writing "no deviations": two deliveries (High Tide, Rite of Passage) came
  back letterboxed while saying exactly that.
- Opaque parts state their background; transparent parts state that the alpha must
  be real (an "empty fuel tube" once came back 7 % opaque).
- **Lit/unlit and pressed/released pairs must be registered** - same canvas, same
  object position - or the state change jumps.
- Textures tile: no end caps on anything meant to repeat.
- Lettering that must be exact (controls, the wordmark "Magneto Transport", serial
  plate) is listed verbatim, with the note that the panel can letter it in code if
  the generator garbles it.
- On return: an ingest script crops to the measured content, checks alpha and pair
  registration, and reports per part before anything is wired in.

---

## 11. Stage 2 is done (2026-10-11) - what stage 3 inherits

Read `plugin/docs/physics/03-hysteresis.md`, `04-record-process.md`,
`05-reference-results.md` and the four DECISIONS entries of 2026-10-11. In one
paragraph: the reference lands MOL within 0.3 dB of the 456 sheet with no
tuning beyond one tail parameter; bias erasure is -18 dB per pass; the erase
head's transition is h_e 0.75-1.75; and an overwrite depends on how DEEP the old
recording went, so **the real-time tape carries four layers per cell at halving
resolution** and its record process is **a table G(A, s, M_old) per layer that
the reference measures** — DONE: `mtref --tables=<path>` writes
`plugin/Source/stock/stock_456.h` (G, the zone centroid/rms/gain per layer and
bias, RESP_DB per layer and bias; `05-reference-results.md` §8 and the DECISIONS
entry "stage 3's record model" say exactly how to use them).

**The math hand-off (Fable → Opus) is here.** Stage 3 is implementation:
the real-time tape with four layers at halving resolution; the record model as
Gaussian zone kernel + quadrilinear G lookup per layer; an erase head that
runs the same G at h_e (erase is the record process with no signal); bias as a
live panel control; feedback uncapped — tape saturation is what contains it
now, and the bench's stop-the-input check must still pass; the section 6
tolerances proven against `mtref` renders on sines, sweeps, drums and voice
(`mtref` needs a `--render in.wav` mode for that, ~1 µm pitch, minutes per
second of audio, which is fine for a validation set). Then LISTEN B.

Stage 3 traps to expect: the kernel is in µm, so at 4.75 cm/s a 1 µm kernel is
21 µs of audio and at 76 cm/s 1.3 µs — the zone convolution must be done on the
TAPE grid, not in samples; the deep layers' coarser cells mean the kernel
there is already wider than a cell, so write the band-limited average, never a
point; and 2 µm cells on layer 2 at 76 cm/s give a 190 kHz cell rate, still
far above the band, so the resolution halving is safe at every speed.

## 12. Stage 3 is built (2026-10-11, build 261011.2) - awaiting listening gate B

The real-time record head runs from the reference's tables, now with a SPEED
axis (4.76-76.2 cm/s), aligned at 19.05 cm/s. BIAS is live; feedback is uncapped
and held by tape saturation. Against the reference: level transfer within
0.026 dB, MOL within 0.07 dB, HD3 above 0 VU within 0.4 dB, recording loss
within 0.18 dB at 19 / 38 / 76 cm/s, speed sensitivity within 0.02 dB, bias
curve within 0.19 dB. Known deviation kept visible: HD3 at 0 VU is 3.4 dB low.
14.8 % of a core at the worst case. Full record: `plugin/docs/physics/06-realtime-record.md`
and `plugin/docs/DECISIONS.md`. Not yet covered: programme-material renders
against the reference; erase-head tables (stage 7).

## 13. Stages 4 and 5 are built (2026-10-11, builds 261011.3 and 261011.4) - awaiting listening gate C

Stage 4: playback from the depth slabs with the exact head kernel (gap, spacing,
thickness), the IEC/NAB equalisers fitted as minimum-phase shelf cascades, the
head bump, crosstalk and the playback amplifier. Against the reference the
playback is within 0.06-0.16 dB to -20 dB at every speed; the aligned machine is
flat within 0.25 dB from 40 Hz to 15 kHz at 19-76 cm/s for both standards.
Record: `plugin/docs/physics/07-playback.md`.

Stage 5: the capstan motor (AC synchronous or DC servo) and every cause of wow
and flutter as a physical cause - runout, bearings, motor ripple, reel tension
as strain, stick-slip at each head. A freshly serviced machine measures 0.036 %
weighted W&F, worn out 0.24 %, and the panel's meter agrees with a demodulated
tone within 10 %. Record: `plugin/docs/physics/08-transport.md`.

Both stages were built without the listening gates, as agreed (section 2): each
is its own commit and install, so either can be heard and stepped back.

## 14. Stage 6 is built (2026-10-11, build 261011.5)

Four stocks on three coatings (studio, vintage and budget are their own reference
runs; shedding is the studio tape decayed), the machine aligned to each fresh one.
Particle hiss written onto the tape, coloured per slab to the continuous coating's
spectrum (within 0.6 dB, 250 Hz - 16 kHz); PARTICLES down to exact silence;
modulation noise, roughness and dropouts as properties of the tape (a loop repeats
them, the hiss is new every pass); azimuth from the transport's condition; WEAR and
ACCUMULATE, the loop's wear saved with the project. The studio stock's hiss is
-61.9 dBA re 250 nWb/m at 19 cm/s, in line with two-track 456. Age is carried by
SHEDDING and WEAR rather than a separate control. Record: `plugin/docs/physics/09-stock.md`.
