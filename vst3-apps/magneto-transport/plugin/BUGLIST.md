# Magneto Transport — buglist / ideas

Collected, not built, until Peter says go (house rule).

---

## 1. HEAD PATTERN — heads 2 and 3 snap to synced positions (Peter, 2026-10-10) — APPROVED, built in stage 8

*"is there a way to get head 2 and 3 to swap to relevant positions for different
standard synced positions, from which they can be moved or nudged?"*

- A **HEAD PATTERN** selector + SET button beside SYNC. Each pattern is a set of
  ratios to the TEMPO HEAD's distance: 1:2:3 (Space Echo), 1:1.5:2 (dotted),
  1:1.333:2 (triplet feel), 1:2:4 (doubling), 1:1.5:3, 1:3:4, golden 1:1.618:2.618.
  With SYNC on, every head then lands on a note value.
- **A one-shot snap, not a lock**: it writes the three position controls once and
  lets go, so a knob nudge or a drag on the rail just works afterwards. (A lock
  would be two owners for one value — the BWFX macro lesson.)
- The carriages **travel** there with their real inertia and top speed, so a
  pattern change is audible as a Doppler glide of every echo in flight.
- Heads cannot pass each other, but they are identical, so a pattern assigns its
  positions to the heads **in rail order**; the tempo head keeps its own position
  and the other two are placed around it.
- If a pattern does not fit the rail (1.5–24 cm) at the current tempo head
  distance, the panel says which head is out of reach rather than silently
  clamping it.
- Bench: after SET, each head's delay equals its ratio × the tempo head's delay
  within 1 us once the carriages settle; a nudge afterwards moves only that head.

## 2. LOOPER — a third transport mode (Peter, 2026-10-10) — APPROVED, built in stage 7

*"is there a way to make a third mode, which is a tape looper mode? If you can find
a consistent, realistic and excellent way of doing it you have more than my blessing"*

The consistent way is that a tape looper IS the closed loop of FLUX MEMORY with
the record path turned into a performance control. Nothing in it is a buffer: the
loop is the tape, layers are magnetisation, and every transport action does what
the machine would.

- **TRANSPORT = REEL | LOOP | LOOPER.**
- **REC**: record + bias on, erase head at full - the first pass writes fresh tape.
- **OVERDUB**: record + bias on, erase head OFF (or at FLUX MEMORY's partial level).
  Old layers survive, but AC bias partially erases what it records over, so each
  pass the older layers fade by however much the bias physically takes off -
  measured in the stage 2 reference, not a feedback knob.
- **PLAY**: record AND bias off, erase off - the loop plays unchanged forever
  (wear only, if enabled). This is Freeze.
- **STOP / START**: the capstan motor really stops and starts, so the loop dives
  and rises in pitch with the motor's inertia (stage 5) - the tape-stop is physics.
- **CLEAR**: one full pass of the erase head.
- **Loop length in bars**: with SYNC on the loop is cut so its period is N bars at
  the current tape speed (0.5-6 m); out of range, the panel says so.
- The playback heads still read the loop, so the echo, feedback and Doppler all
  work ON TOP of the loop.
- **No undo**: tape has none. Said plainly in the manual rather than faked.
- Relay clunks on every transport button when the mechanical sounds arrive (stage 10).
- Bench: an overdub layer's decay per pass equals the reference model's bias-erasure
  figure; PLAY holds a loop bit-exact for 10 minutes with wear off; STOP's pitch
  dive follows the motor's measured spin-down.

## 3. FEEDBACK runs away at about 30 % (Peter, 2026-10-11) — BUILT 2026-10-11 (261011.7): both fixes

*"feedback self-oscillate already at 30% or so..."*

Measured on the working tree (`test/fbprobe.cpp` -> `mtfb`: a burst, then silence,
onset found by bisection; one pass's response in 1/3 octaves):

| heads feeding back | onset (FEEDBACK) |
|---|---|
| 1 head, 19 cm/s | 50 % |
| 2 heads, 19 cm/s | **25 %** |
| 3 heads, 19 cm/s | 17 % |
| 1 head, 4.75 / 76 cm/s | 52 % / 63 % |

Two causes, both by design rather than a fault, and together too hot:

- `FEEDBACK_MAX` = 1.5 puts unity loop gain at 67 % for one head on a flat chain,
  and the playback head's contour bump adds +2.3 dB at 40 Hz at 19 cm/s (+2.0 dB
  at 25 Hz at 9.5), so one head runs away at 50 % - into a low thud, as real tape
  echoes do.
- The feedback wire sums the heads, so every echo arriving in phase adds: two
  heads halve the onset, three take it to a third. Physically right (a Space Echo
  runs away sooner in its multi-head modes), but it leaves most of the knob as
  runaway.

Proposed (one or both):

- **Normalise the feedback bus by the sum of the active heads' levels**, so FEEDBACK
  means the loop gain of the heads' in-phase sum whatever is switched on, and the
  onset sits at the same place with one, two or three heads.
- **`FEEDBACK_MAX` 1.5 -> about 1.1**, putting a single head's onset near 70 % at
  19 cm/s (83 % at 4.75 and 76 cm/s) while 100 % still runs away at every speed
  (the bench's check).

Bench to add: the onset with 1, 2 and 3 heads lands within a few % of the intended
place at 4.75, 19 and 76 cm/s.

**Built (261011.7).** `FEEDBACK_MAX` 1.1, and the wire is divided by the NUMBER of heads
that are on rather than the sum of their levels: dividing by the levels would make a
head turned down feed back MORE of itself, which reads as backwards on a knob. Measured
(`mtfb`): 67.9 / 68.1 / 68.1 % with one, two, three heads at 19 cm/s; 71.0 % at 4.75 and
86.4 % at 76 cm/s (the head contour bump is lower down at the slow speed, the bass
falls off the playback head at the fast one). 100 % still runs away everywhere. Bench
section 5: runs away between 60 and 82 % with one, two or three heads.

## 4. Tape-machine use: record and replay with no echo (Peter, 2026-10-11) — BUILT 2026-10-11 as REPRO (261011.6)

*"can the Magneto Transport be used as a tape-recording simulation when the head 1
2 and 3 are off?"*

With all three heads off nothing reads the tape: the wet output is silent and only
the dry signal is heard. What works now: HEAD 1 on at its nearest (1.5 cm), the
others off, FEEDBACK 0, KILL DRY, MIX 100 %. Everything the tape does is then on the
signal (stock, bias, saturation, EQ, hiss, modulation noise, dropouts, wow and
flutter), but late by 1.5 cm / speed: 79 ms at 19 cm/s, 39 ms at 38, 20 ms at 76.
The plug-in does not report it, so the track sounds late unless nudged by hand.

Proposed: a **REPRO** mode that reports the playback head's delay to the host as
latency, so the DAW compensates and the track stays in time - a tape machine on an
insert. Details to settle:

- latency is fixed while the mode is on (hosts dislike a latency that changes), so
  the head's position and the speed are locked while REPRO is on, or a change is
  only taken up when the transport stops;
- wow and flutter still move the replay around that fixed latency, which is the
  real thing;
- the record head cannot replay what it is writing (it writes at the gap's
  trailing edge), so zero delay is not physical; a replay head 1.5 cm on is the
  machine's own layout.

## 5. OK -> BROKEN: one slider for "bad tape, bad machine" (Peter, 2026-10-11) — BUILT 2026-10-11 (261011.8): an automatable host parameter, a slider in MECHANICS

*"a slider to amplify all deteriorative settings (from 'Ok' to 'Broken', with 'Ok' as
default)... this clearly breaks the realism a bit... is there a good way to do that?"*

**Honest assessment: it works well, and it does not have to break the realism.**
Every deterioration in this model is a cause (a worn capstan, a lifted tape, shed
oxide, a weak erase head), so BROKEN would not add an effect on top: it moves the
causes towards a machine in genuinely bad shape. A real neglected echo unit does
sound like that. The danger is a different one: most of what a bad tape does is
*subtractive* (lost treble, lost level, holes), so a slider that only scales the
existing knobs gets duller and quieter rather than more spectacular. The far end
needs a few more faults to be fun, and all of them are real.

What it moves (from the knobs as set, towards a fault end; at OK it is exactly the
knobs, bit for bit):

- transport: CONDITION, CAPSTAN, REELS, SCRAPE up (wow, flutter, azimuth, squeal);
- tape: WEAR up (lost oxide, lift, roughness, dropouts), the stock's own defects
  scaled with it;
- electronics: the bias oscillator sagging (under-bias: distortion, lost treble,
  and old recordings surviving the overwrite), the erase head weakening (ghosts of
  earlier passes in LOOP);

and three faults that exist only past the middle, each a real failure:

- **crinkled tape**: a creased stretch that comes round with the loop or passes
  once on a reel - a burst of dropouts and flutter;
- **head clog**: oxide building up on a head, then clearing - the treble drops
  away for seconds at a time;
- **motor hunting**: the speed servo oscillating - slow pitch swings.

The panel shows each affected knob's effective position (a second marker), so the
slider is seen to work (the BWFX macro lesson: a macro you cannot see working is
indistinguishable from a broken one). No hidden gain: a broken tape is quieter,
and says so.

**Built (261011.8).** `broken`, 0-100 %, automatable, a horizontal slider in MECHANICS
reading OK / a percentage / BROKEN and naming the faults while they act (CLOG, HUNT,
CRINKLE). In a straight line from the knobs: CONDITION to 100 %, CAPSTAN, REELS and
SCRAPE to 200 %, WEAR to 100 %, BIAS sagging 6 dB, and on a loop the erase head down to
0.55 of its field (1.24 Hc at full: the wipe stops being clean). A red dot on each of
the six knobs shows where it acts. Past 50 %: head clog (up to 6 um of oxide built over
2-8 s on each playback head, held, then breaking loose), motor hunting (the capstan's
speed swinging up to +-1.5 % near 0.75 Hz), and a crinkled 5 cm stretch (one on a loop,
one in most 2.5 m of a reel: folds every ~2 mm lifting and twisting the tape, a jolt in
the speed and harder stick-slip as it passes the heads). The crinkle lifts the tape at
the playback heads only; its effect on recording is not modelled.
Bench section 22: at 40 % bit-identical to the same knobs turned by hand; faults only
past the middle; hunting 4.5 % peak to peak at 1.34 s a cycle; clog swings 10 kHz by
16.5 dB while 500 Hz moves 0.1 dB, and clears; the crinkle dips once a loop period
(spacing within 5 ms); the erase head 1.24 Hc on a loop, 2.25 on a reel; BROKEN 100 %
plays 4.9 dB quieter than OK, never louder.

## 6. THE MANUAL UNDER THE KNOB: educational tooltips (Peter, 2026-10-11) — APPROVED 2026-10-11: built with the stage 8 panel

*"a style-compatible popup window - like a both use and technical tooltip - which not
just informs but also educates... explaining the technicalities of what lies under the
hood... a 'Tooltips' in the settings so it can be switched off, with a slider for the
time after hovering until the tooltip appears... MathML equations and
images/screenshots/renderings... for the tape geeks to geek out, and for everybody to
see that this is something really different."*

Every control and every part of the machine drawing (reels, capstan, pinch roller,
erase head, record head, the three playback heads, the tape itself) gets a card in
two layers:

- **What it does for the sound**: one or two sentences, visible at once.
- **Under the hood**: the physics as modelled, an equation, a figure, and the
  measured number that proves it - e.g. BIAS: the record process as a Preisach
  medium under an AC field, the bias curve with the operating point marked, "one
  pass of bias takes −34.5 dB off an old recording (reference model)". Plus a plain
  line on what is NOT modelled, where it matters: honesty is part of the show.

Design points:

- **MathML renders natively in WebView2** (Chromium has MathML Core since 109; the
  runtime is evergreen and statically loaded, the Thin Walls lesson): no maths library.
  Equations authored in a TeX subset and converted to MathML at build time, so the
  source stays readable.
- **Figures are generated, never drawn**: the reference (`mtref`) and the bench write
  SVGs (hysteresis loop, anhysteretic curve, the bias curve, the record zone, the
  Karlqvist field, gap and spacing loss, the erase transition, the particle-noise
  spectrum, wow and flutter). Where it says more, a **live** mini-plot drawn from the
  engine's own state: the bias curve with today's BIAS marked, delay = distance / speed
  for the heads as they stand, the erase head's transition with ERASE marked.
- **Every number quoted is generated too**: the bench writes `tips-numbers.json` and the
  cards read it, so a retuned model cannot leave a stale claim on screen (a number
  typed beside the thing it describes goes stale - the fleet's oldest lesson).
- **One source for the cards and the manual**: the handbook (stage 10) prints the same
  content.
- **Settings** (a gear in the header): TOOLTIPS on/off and a HOVER DELAY slider
  (0.2-3 s). A user preference, not a patch: kept in the plug-in's settings file,
  not the project. Click a card to pin it and read on; Esc or a click away closes.
- Placed beside the control, never over it or the pointer (the High Tide hint rule),
  styled with the panel; long cards scroll inside a fixed size.
- Belongs with the real panel (stage 8): the cards' positions and art follow it.

Bench and probe: every parameter and every machine part has a card; every equation
parses; every figure loads; every number on a card exists in `tips-numbers.json`;
the card never overlaps the control it explains; the delay slider is honoured.

## 7. FLUX MEMORY / LOOPER controls: one subpanel, consistent, and say when inert (Peter, 2026-10-11) — BUILT 2026-10-11 (261011.7)

*"Perhaps the 'looper'-related controls should be in one subpanel and consistent... some
indication that the looper controls do not do anything when LOOPER transport is not
selected?"*

What is inconsistent today (read off `Engine.cpp`'s FLUX MEMORY block, 261011.6):

- **ERASE** does nothing on a REEL (a reel always wipes clean) and nothing in LOOPER
  (OVERDUB forces the erase head off, REC forces it full). It only works in LOOP.
- **FREEZE** only works in LOOP; in LOOPER, PLAY does the same thing under another name.
- **CLEAR**, the **LOOPER** states and **LOOP BARS** only work in LOOPER.
- **LOOP LENGTH** does nothing on a REEL, and is overridden by LOOP BARS when SYNC is on.
- **FEEDBACK** does nothing while recording is off (FREEZE / PLAY).
- Nothing on the panel dims when it is inert.
- **ERASE's travel is badly spread**: the audible transition (an old pass kept to an old
  pass gone) lies between about 40 % and 62 % of the knob; the bottom 40 % does nothing.

Proposed:

- **TRANSPORT = REEL | LOOP** (LOOPER is not a separate machine: it is the loop's
  record controls). One **LOOP** subpanel holds everything that belongs to the loop:
  LOOP LENGTH and LOOP BARS, ERASE, a state switch **REC | DUB | PLAY**, CLEAR.
  - REC: record on, erase head full (a fresh take every pass).
  - DUB: record on, erase head at the ERASE knob (the normal running loop; at ERASE
    100 % it is a clean echo, exactly today's LOOP default).
  - PLAY: record and bias off, erase off (today's FREEZE and LOOPER PLAY, one control).
  - CLEAR: one full pass of the erase head.
- On a REEL the whole subpanel dims, with one line why ("a reel brings fresh tape");
  FEEDBACK dims while PLAY is on.
- ERASE's knob respread so its travel follows the audible effect (the field still the
  cause; the taper chosen from the measured erase transition).
- Not released yet, so no saved projects to migrate.

**Built (261011.7)** as proposed. MOTOR RUN moved to TRANSPORT (it stops the capstan on
a reel too). ERASE: 0 off, 10 % 0.92 Hc, 45 % 1.10 Hc (the reference's 6 dB a pass),
90 % 1.57 Hc (60 dB), 100 % 2.25 Hc; on the machine one pass loses 3 / 8 / 22 / 42 dB
at 25 / 50 / 75 / 90 % (gentler than the reference in the middle: BUGLIST 8). The panel
dims, but does not lock, what is inert, and says why in one line; FEEDBACK dims in
PLAY. A tempo head dragged after LOOP BARS cut the loop is reported as the bars the
loop lasts now. Bench: the taper, REC/PLAY overriding ERASE, the knob spreading the
loss, and a REEL ignoring the whole panel bit for bit.

## 8. OVERDUB layers vanish in one pass: is bias erasure too strong? (Peter, 2026-10-11) — investigated, awaiting go

*"not sure overdub is working"*

It is doing what the model computes, and that is the trouble. One pass of the record
head (erase off, record on, silence going in) at the operating bias, 19 cm/s, read
through the playback head (`test/biasprobe.cpp` -> `mtbias`):

| BIAS | 60 Hz | 200 Hz | 1 kHz | 3 kHz | 8 kHz |
|---|---|---|---|---|---|
| 0 dB | −31.6 | −31.7 | −32.7 | −36.1 | −44.9 |
| −3 dB | −16.5 | −16.6 | −17.2 | −18.6 | −22.4 |
| −6 dB | −14.5 | −14.5 | −14.8 | −15.6 | −17.0 |

So an overdubbed layer is gone after one pass and OVERDUB sounds like REC. The
reference agrees with the engine here (−34.5 dB at 1 kHz), so this is the MODEL, not
the real-time port.

Why I doubt it: tape users report that recording over a track with the erase head
disabled kills mostly the treble, with the bass surviving much better, and that
sound-on-sound loops fade gradually. The model shows the tilt (8 kHz goes 13 dB further
than 60 Hz) but erases the bass almost as hard as the mids.

The likely missing physics: the reference's Preisach medium has its particles'
interactions only as a spread of switching fields (the Student-t tail), with no
MEAN-FIELD term. In a real coating, the already magnetised medium's own field acts on
every particle during the bias pass like a small recording signal (the moving-Preisach
term, H_eff = H + alpha M). At long wavelengths nothing opposes it, so the bias re-records
part of the old pattern instead of wiping it; at short wavelengths the coating's
demagnetising field (growing with 1/wavelength) cancels it, so treble is erased. That is
exactly the reported behaviour: bass survives, treble goes.

Proposed (Fable-level, stage 2 territory):

- Add the mean-field term to the reference, with the demagnetising factor by
  wavelength; refit nu and alpha together so MOL, the bias curve and the 456 sheet still
  hold; measure bias erasure by wavelength.
- In real time: the old magnetisation's long-wave part, as the record head sees it,
  added to the recording signal (`s_eff = s + alpha * LP(M_old)`, the low-pass being the
  demagnetisation's own wavelength law), so the tables stay three-dimensional.
- Bench: overdub survival by frequency against the new reference; a layer's fade over
  ten passes; MOL and the bias curve unmoved within their tolerances.
- If published measurements of bias erasure turn up, they decide alpha; until then the
  only anchor is the qualitative report, and I will say so.
