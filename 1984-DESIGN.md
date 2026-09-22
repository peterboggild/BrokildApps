# 1984 — the vintage polysynth

*A Brokild instrument. Design record, written before the first line of DSP and
kept honest by the bench afterwards. Every number in §12 is a measurement.*

Peter's brief (2026-09-22): "inspired by the synth part of Blade Ruiner, and the
iconic sounds inspired by Yamaha CS-80, the heavy rich analog tone, the Roland
VP-330, create a new, old, recreation and creation, of a vintage analog
polysynth, a full featured, not just capable of leads and brass, but able to
expand into stranger things territory (Prophet 5) ... exquisite sound quality,
but also the possibility of adding VHS/Tape flutter, distortion, saturation and
wow, with cinematic grandeur and vivid, emotional sound palette ... the ultimate
retro Vangelis synth ... Mind the dual-filter layout of the CS-80 eight voices."
Then: "Call it 1984." Then: "It doesn't have to look like either a CS-80 or a
Prophet synth, but it has to sound absolutely stunning."

So: the *architecture* is the CS-80's (eight voices, each of two RANKS with its
own oscillator, high-pass, low-pass, two envelopes and level; ring modulator;
per-voice sub-oscillator; touch response), the *strangeness* is the Prophet's
(poly-mod, sync, unison), the *choir* is the VP-330's (fixed formant bank
behind a three-phase ensemble), and the *time* is 1984: everything goes to tape
before it goes to the hall. The look is its own.

---

## 1. Identity

| | |
|---|---|
| Product name | **1984** (bundle `1984.vst3`, patches in `Documents\Brokild patches\1984`) |
| Source tree | `C:\Users\peter\b\Nineteen84` (a target cannot start with a digit) |
| Plugin code | `N984` (Brkd + N984; unique in the fleet) |
| Formats | VST3 + Standalone, JUCE 8, IS_SYNTH, MIDI in |
| Build id | `N84_BUILD_ID` in CMakeLists (bump AND re-run `cmake -S . -B build`) |
| World rack | BWFX rack + the five macros, SPECTRA bus consumed |
| Backup | private repo `brokild-1984` |

## 2. What it is, in one paragraph

Eight voices. Each voice is two complete synthesizer ranks (I and II) sharing a
keyboard, a ring modulator and a sub-oscillator. Each rank has a multi-wave
oscillator (saw, pulse with width and PWM, triangle, post-filter sine, noise),
a resonant 12 dB high-pass into a resonant low-pass that is either the 12 dB
state-variable kind (the CS-80's) or a 24 dB transistor ladder (the Prophet's),
a five-stage filter envelope with the CS-80's initial-level / attack-level
shape, an ADSR amplifier envelope, its own level and pan, and velocity into
both loudness and brightness. Rank I can be hard-synced to rank II (the
Prophet's way round: the audible rank is the slave, the silent master holds
the note) and both can be bent by poly-mod (rank II into rank I's pitch, width
and filter; the filter envelope into rank I's pitch and width — so the
envelope sweeps the synced rank). The eight voices go through one chain:
drive (in the oversampled domain, before decimation), ensemble (a three-phase
BBD chorus in three flavours), choir (a formant bank that morphs U→O→A→E→I),
tape (wow, flutter, saturation, age, dropouts, hiss, and a VHS mode), then
the hall (an eight-line FDN with a budgeted shimmer), then the Brokild World
FX rack.

## 3. The signal path

```
                 ┌── RANK I ── VCO(saw+pulse+tri+noise) ─ HPF12 ─ LPF(12|24) ─ VCA ─┐
  key ──► voice ─┤                                          ▲ FEG(IL/AL/A/D/R)        ├─ RING ─ pan ─┐
                 └── RANK II ─ VCO ── sync/polymod ──────── HPF12 ─ LPF ─── VCA ─────┘             │
                    (+ post-filter SINE per rank, + per-voice SUB-OSC LFO, + touch)                 │
  8 voices summed at 2x/4x ──► DRIVE ──► ENSEMBLE ──► CHOIR ──► TAPE/VHS ──► decimate ──► HALL ──► BWFX ──► out
```

Everything up to and including the tape runs at the oversampled rate (2x
default, 1x/4x selectable) so that the drive and the tape saturator cannot
alias; the hall runs at the base rate because it is linear and long.

## 4. The voice

### 4.1 Oscillator (per rank)
One phase accumulator, four band-limited outputs at once: saw (polyBLEP step
at the wrap), pulse (steps at the wrap and at the width crossing; width 50–95 %
plus PWM from the sub-oscillator and poly-mod), triangle (polyBLAMP slope
changes at 0 and ½ — integrating the pulse was rejected: the Photo-Synth DC
lesson), and a sine (a triangle-ish sine with 2 % second harmonic, the way a
diode-shaped one sounds). Each wave has its own level slider so a rank can be
saw + a little pulse + sine, which is the CS-80 brass recipe. Noise is white,
per voice, into the filter. Rank I's phase can be reset by rank II's wrap
(hard sync) with the discontinuity band-limited at the actual jump height —
the Black Rider mechanism, verbatim. Rank II is the master because the
poly-mod destinations are on rank I: SYNC LEAD is a silent rank II holding the
note while the envelope sweeps the audible, synced rank I. Feet 32/16/8/4/2, semitone, fine cents.

### 4.2 Filters (per rank)
HPF: 12 dB/oct TPT state-variable, resonant to self-oscillation, with a soft
limiter on the band-pass state so a screaming resonance stays a note instead
of a spike. LPF: the same SVF in CS mode, or the Black Rider ZDF ladder in
LADDER mode (tanh at the summing node, cubic per stage, tuning exact because
the linear loop is solved first). Cutoff = knob × 2^(env·6 oct + key track +
brilliance + velocity + touch + LFO + poly-mod). The filter envelope drives
BOTH cutoffs, as on the CS-80.

### 4.3 Envelopes (per rank)
Filter envelope: IL/AL/A/D/R. It jumps to IL at note-on, moves to AL over A
(RC shape), decays to zero over D (there is no sustain: the knobs are the
sustain), releases to zero over R. Both IL and AL are bipolar, so a filter can
open from below or close from above. Amplifier envelope: RC-shaped ADSR,
retrigger from the current level.

### 4.4 Touch
Velocity → rank level (VEL) and → brilliance (VEL→BRIL, up to +2 octaves and
+40 % envelope). Aftertouch (channel or polyphonic; the higher of the two
for the note) → pitch (up to +2 semitones), brilliance, level and
sub-oscillator depth. Mod wheel → vibrato depth and brilliance.

### 4.5 Sub-oscillator (per voice)
Seven shapes (sine, triangle, saw, ramp, square, S&H, smooth noise), 0.05–40
Hz, to pitch, pulse width, filter and amplifier, with a delay (fade-in) and a
phase mode: FREE (each voice its own phase — eight voices modulated in unison
sound like a synth on a tape; free-running they sound like a section),
RESET (from zero at note-on) or ONE (all voices share).

### 4.6 Ring modulator (per voice)
CARRIER: a sine at SPEED (0.1 Hz–4 kHz, or a ratio of the note when KEY is
on), DEPTH crossfading dry to dry×carrier, its own attack/decay envelope
sweeping the carrier frequency by MOD octaves — the CS-80 section. RANKS:
rank I × rank II, the Prophet/ARP kind.

### 4.7 Poly-mod (per voice)
Rank II's pre-filter output → rank I pitch (exponential FM at audio rate), →
rank I width, → both low-pass cutoffs (filter FM). Rank I's filter envelope →
rank I pitch, → rank I width. Set rank II's level to zero and it is a pure
modulator; leave it up and it is both.

### 4.8 Voice modes
POLY (8), DUO (two voices per note, 4-note poly, detuned and panned), UNISON
(all eight on one note, symmetric detune fan), MONO (last-note priority with
legato). Glide is portamento or glissando (semitone-quantised).

### 4.9 Vintage
One knob scales: per-voice, per-rank tuning tolerance; per-voice cutoff
offset; per-voice envelope time tolerance; slow OU drift of every oscillator;
fast jitter. A poly patch is eight slightly different synthesizers.

## 5. The chain

### 5.1 Drive
OFF / VALVE (asymmetric tanh with bias, even harmonics) / TAPE (soft knee,
symmetric, level-tracked) / FUZZ (hard, tilt EQ before) / FOLD (sine
folder). Tone is a post tilt. Runs in the oversampled domain; a level tracker
is NOT used (the Battlestar lesson: a measured makeup that chases the signal
is a compressor nobody asked for) — each mode carries a fixed trim measured at
the bench.

### 5.2 Ensemble
Three delay taps per channel, each modulated by the sum of a slow LFO (0.5 Hz
base) and a fast one (6 Hz base), phases 120° apart, the right channel offset
60°: the Solina/VP-330 circuit. CHORUS I (two taps, slow only), CHORUS II
(two taps, fast wobble — the CS-80's second chorus is a tremolo-ish flutter),
ENSEMBLE (three taps, both LFOs), CATHEDRAL (three taps, both LFOs, deeper,
wet path darkened as a BBD is). RATE scales both LFOs, DEPTH scales the
excursion, MIX is the wet share.

### 5.3 Choir
Five parallel band-pass sections at the formants of a vowel, interpolated
continuously across U-O-A-E-I (VOWEL), all shifted by REGISTER (±40 %), summed
and mixed by MIX, with AIR adding a 6 kHz shelf of breath. A pulse-rich rank
through this bank is the VP-330 human voice; through ENSEMBLE it is the choir.

### 5.4 Tape / VHS
A modulated delay (12 ms nominal) whose read position carries wow (a sine at
WOW RATE plus a slewed random walk — a wander never steps) and flutter (three
incommensurate sines at 4.3/8.7/13.1 Hz plus a fast OU component), followed
by a saturator with bias and a head bump, a high-frequency loss set by AGE,
dropouts (Poisson events, raised-cosine dips with extra HF loss) and hiss
(added after everything, never inside a loop). VHS mode swaps the flutter for
head-switching: a 50 Hz tracking wobble, a tiny tick per field, bandwidth
capped near 6 kHz with a resonant bump below it, and more frequent dropouts.

### 5.5 Hall
Eight-line FDN behind four diffusing allpasses, Hadamard mixing with sign
flips, per-line damping, pre-delay to 250 ms, SIZE scaling the lines
0.5–2.0×, DECAY as a real RT60 (0.2–20 s; g = 10^(−3L/(RT·fs)) per line), MOD
moving two lines by ±1 sample with a Hermite read (a little HF loss in the
tail is the price; it is bounded), SHIMMER an octave-up fold BEFORE the
damping and BUDGETED against the decay (L = shim·(1−g)·0.95 so g + L < 1 in
every phase — Battlestar Overdrive's lesson, bounded is not stable). Left
feeds the even lines, right the odd.

## 6. Parameters

One table (`SPECS[]` in Engine.cpp) walked by the APVTS layout, processBlock,
the patches, the randomiser, the initial-state push and the bench. ~130 host
parameters; `patch` (the factory selector) and `os` are non-automatable.
Ranks are prefixed `a_` and `b_`. Full list with kinds and ranges in
`PROTOCOL.md` in the tree; the page is told the whole table at boot and draws
from it.

## 7. Patches

Forty factory patches in `Patches.cpp` as sparse id→value lists over the
defaults, grouped: BRASS, STRINGS, CHOIR, LEAD, BASS, PAD, KEYS, BELLS,
STRANGE. Every one is measured bounded and audible by the bench. RANDOM rolls
a playable instrument inside musical ranges (never the master or the
performance controls). User patches are JSON in the house folder and carry
their own BWFX blob.

## 8. Panel

Its own design — "a late night in 1984": black anodised deck, cream silkscreen,
two rank rows of long-throw sliders with coloured caps (the album colours, not
the CS-80's), an amber VFD strip that names the patch and shows the filter
envelope and the voice lamps, chrome toggles, a tape window whose reels turn
when the tape is on. WebView2 page (`Source/ui/ui.html`), pure view, built
from the parameter table; decals slot in through `--decal-*` CSS variables
and the page is complete without them. Decal order: `1984-DECALS.md`.

## 9. Bench (`test/bench.cpp`)

Silence at rest; tuning ±1 cent for both ranks at every footage; aliasing
of a saw at 2x below −60 dB above 20 kHz-folded; SVF and ladder
self-oscillation frequency equals the cutoff; envelope times; the IL/AL shape;
velocity range; sync makes harmonics at the master; ring makes sum and
difference; poly-mod moves the spectrum; unison beats at the detune; glide
time; eight notes sound and a ninth steals the oldest; mono legato; ensemble
depth measured as pitch deviation; choir formant peaks; tape wow measured as
pitch deviation against the formula; flutter; dropouts counted; hiss level;
hall RT60 within 10 % of DECAY; hall STOP-THE-INPUT-AND-WATCH at maximum
shimmer and decay (the runaway test); DC; 200 random machines bounded;
determinism memcmp; 44.1/48/96 kHz; every parameter at both extremes; the
neutral world-mod bus byte-identical; cost.

## 10. Verification ladder

bench → `node test/uiprobe.js` (headless page against a stub bridge) →
`tools/live.ps1` (the standalone over CDP: parameters round-trip through the
native side, a note sounds, the tape reels turn) → installed bundle probed by
`install-fleet.ps1` → zip verified by loading the DLL out of the archive.

## 11. Not built, and why

MPE (channel pressure and poly pressure cover the CS-80's touch; per-note
bend is a later thing). A vocoder (the choir is a fixed bank by design — the
VP-330's — and a real vocoder needs a sidechain, which changes the bus
layout for one feature). Ribbon (the pitch wheel is the ribbon).

## 12. What the build taught

Bench `test/bench.cpp`, 86 checks ALL CLEAR at 48 kHz (also 44.1 and 96), 2026-09-22.

- **Cost**: eight voices with every stage on, 2.5 % of one core at 1x, 4.6 % at 2x, 10.4 % at 4x.
- **Tuning**: both ranks within 1 cent at every footage; rank II at +25 cents lands on +25.0.
- **Aliasing** (saw at A6 = 1760 Hz, worst non-harmonic component below 20 kHz): under -40 dB at 1x, under -60 dB at 2x; a 5 % pulse at 2x under -50 dB; FUZZ at full drive at 2x under -36 dB. The decimator's own transition band sits above 20 kHz, which is why the audible band is what is measured.
- **Filters**: the SVF self-oscillates at 1000.0 Hz for a 1 kHz cutoff, the ladder at 996.5 Hz (0.35 %); both bounded. Resonance at 80 % puts the cutoff 14.5 dB over the octave above.
- **The IL/AL shape held only once the gate used the patch's own IL**: the envelope was gated with the level of the previous block and charged from zero instead of from IL, and the first bench run read 2182 Hz where 27 Hz was due. A gate must not read a level that set() has not delivered yet.
- **Voice mode at note-on**: the mode and the glide were derived at the top of process(), so a note-on arriving before the first block was allocated as POLY whatever the knob said; UNISON read "one voice". Derive at note-on too.
- **An asymmetric clipper with unequal tops is DC plus odd harmonics.** The first VALVE scaled its negative half by 0.78 and measured a second harmonic of -33 dB, the same as a clean sine. Even harmonics need an even term in the curve; b + 0.45 b^2/(1+|b|) is even, monotonic (slope never below 0.55) and measures -16 dB at 30 % drive.
- **A linear SVF with k > 0 only rings.** True self-oscillation needs the loop marginally unstable and the state limiter to hold it: the last five per cent of RESONANCE take k to -0.06.
- **Three ensemble taps of a modulated sine partly cancel** (-4.3 dB at full mix); the wet is normalised by taps^-0.75, between the amplitude and the power sum.
- **Tape hiss is gated on the signal follower** (5 ms up, 0.6 s down, exact zero below 1e-3), so an idle instrument is exactly silent and a synth on a tape still hisses while it plays. HISS 100 % / AGE 100 % measures -57 dBFS in the second after a note.
- **Tape wow** at 100 %, 0.5 Hz: 2.15 % peak-to-peak (the sine part alone predicts 1.1; the slewed walk adds the rest). Flutter at 100 %: 0.35 % p-p. With both at zero the pitch moves 0.0003 %.
- **The hall's RT60** measures 1.68 s for a 2 s setting; at maximum decay, size, shimmer and motion the tail is 97 dB lower 40 s after the note than in its first seconds: bounded AND falling.
- **Probes that lied before the code did** (the recurring class): a zero-crossing pitch estimator on a two-tap chorus (46 % "wobble" from cancellation nulls); a pulse source with a spectral null exactly at the formant under test (a 68 % pulse nulls its 15th harmonic, 1650 Hz); a pluck measured after it had decayed; a glide test reading voice 0 in a POLY allocation; a VINTAGE test on the one voice whose tolerance happened to be small; a sustain test reading past the end of its own buffer. Each was rewritten against the physics; none was loosened.
