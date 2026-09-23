# THIRTY THOUSAND YEARS — the drone world

*A Brokild instrument. Design record, written before the first line of DSP and
kept honest by the bench afterwards. Every number in §14 is a measurement.*

Peter's brief (2026-09-23, "Research-informed master build prompt"): an
exceptional VST3 drone synthesizer and evolving soundscape instrument for
humanity's darkest hour — Asimov's long dark, Gibson's estrangement, the
controlled societies of Bradbury and Huxley, the crushing physicality of Cult
of Luna and LLNN. Four interacting sound strata (MASS, SIGNAL, MEMORY,
STRUCTURE) and a shared ENVIRONMENT that participates in generation; a LIFE
network that derives slow control from the engine's own audio; a HISTORY
control that travels through four scenes over minutes; eight curated macros
(MASS DREAD VIOLENCE INSTABILITY CONTAMINATION DISTANCE LIFE HUMANITY); drone,
instrument and excitation modes; sixteen hero patches and a bank of at least
forty-eight. "Its central artistic proposition: play the forces that make a
sonic world live, deteriorate, resist, and transform."

The brief carries its own priorities — sound, then playable control, then
stability and recall, then coherent workflow, then visual identity, then
breadth — and this record follows them.

---

## 1. Identity

| | |
|---|---|
| Product name | **Thirty Thousand Years** (bundle `Thirty Thousand Years.vst3`, patches in `Documents\Brokild patches\Thirty Thousand Years`) |
| Source tree | `C:\Users\peter\b\ThirtyThousandYears` |
| Plugin code | `T30k` (Brkd + T30k; unique in the fleet — checked against all nineteen) |
| Formats | VST3 + Standalone, JUCE 8, IS_SYNTH, MIDI in, one stereo AUX input bus (excitation / sidechain) |
| Build id | `TTY_BUILD_ID` in CMakeLists (bump AND re-run `cmake -S . -B build`) |
| World rack | BWFX rack + the five macros after the engine's own ENVIRONMENT (additive, default-empty, the house rule); SPECTRA bus consumed by MASS and SIGNAL |
| Backup | private repo `brokild-thirty-thousand-years` |
| Design | this file; `PROTOCOL.md` in the tree is the page ↔ native contract |

## 2. What it is, in one paragraph

Eight voices, each of which is a whole world in miniature: a MASS layer (two
virtual-analogue oscillators with a sub, slow beating in hertz separate from
detune in cents, correlated drift, a ladder or state-variable filter with
saturation in its state), a SIGNAL layer (two morphing wavetables, a two-
operator phase-modulation pair, ring modulation, and a 64-partial additive bank
whose upper partials can lose their harmonic alignment while the fundamental
holds), a MEMORY layer (a granular reader over a procedural or captured or
imported source, and an STFT resynthesis with freeze, smear, thinning and
displacement, both under one EROSION that turns a recognisable source into
residue), and a STRUCTURE layer (a modal resonator bank or a comb/waveguide,
excited by impulses, noise, a bowed friction model, or any other stratum; a
STRESS that carries it from a coherent object through interacting resonances
to fracture). The four go into the ENVIRONMENT: a mixer, two insert lanes of
reorderable processors, a spatial send with an early-reflection + FDN reverb
whose APPARENT SCALE is separate from its amount, and an explicit FEEDBACK LOOP
that returns, filtered and shifted and saturated, into a chosen stratum's input
— so the room is a generator. LIFE is the modulation: LFOs, envelopes,
stochastic sources, followers, event lanes, a 32-slot matrix, and the LIFE
NETWORK that reads energy, brightness and transients off the engine's own audio
and feeds them back as slow control with hysteresis and refractory periods.
HISTORY is four scenes and one control that travels between them over seconds
to thirty minutes. Then the Brokild World FX rack.

## 3. The signal path

```
                 ┌───────────── per voice (×8) ──────────────┐
  MIDI / drone → │ MASS   osc1 osc2 sub → filter → amp env   │─┐
                 │ SIGNAL wt1 wt2 pm ring add → filt → env    │ │
                 │ STRUCTURE exciter → modes/comb → stress    │─┤   (STRUCTURE is
                 └───────────────────────────────────────────┘ │    excited by MASS,
  MEMORY (global) grains + spectral, pitch-keyed to voices ────┤    SIGNAL, MEMORY,
                                                               │    the loop, or ext)
  ENVIRONMENT  4 channels (mute/solo/gain/pan/width/hp/lp,   ◄─┘
               env send, loop send)
      → insert lane A (3 slots: SAT FOLD MULTI SHIFT DELAY COMB CRUSH)
      → mix bus ──────────────────────────────┬─→ insert lane B → SPATIAL (early + FDN,
                                              │                    SCALE, DISTANCE) ─┐
                                              └─→ FEEDBACK LOOP (delay, hp/lp, shift,  │
                                                  sat, damp) → returns INTO a stratum  │
      ← sum ← bass mono ← ceiling limiter (2 ms lookahead) ← ─────────────────────────┘
      → BWFX rack → out
```

Ordinary routing is acyclic. The only cycle is the FEEDBACK LOOP node, whose
delay, bounds and energy meter are explicit.

## 4. The strata

### MASS — the thing beneath the ground
Two polyBLEP oscillators (sine, triangle, saw, variable pulse), hard sync,
detune in cents AND a separate BEAT in hertz (osc 2 runs at f + beat, so a
bass at 55 Hz can beat at exactly 0.3 Hz whatever the note), unison up to
three, a sub one or two octaves down (sine or square) with REINFORCE (its
second and third harmonics added at a fixed proportion so weight survives a
small speaker). Drift is an Ornstein–Uhlenbeck walk per oscillator with a
correlation time (DRIFT TIME, 0.2–60 s) and a range (DRIFT, cents); pulse
width and amplitude walk the same way. Filter: a ZDF ladder (tanh at the
summing node) or a TPT state-variable (LP/BP/HP/NOTCH) with a tanh on the
band-pass state so resonance stays a note. Filter and amplifier ADSRs.

### SIGNAL — something has learned to think
Two wavetable oscillators over eight original tables of eight frames each
(GEOMETRIC, MACHINE, FORMANT, HOLLOW, STEPPED, METALLIC, SIREN, ORGANISM),
mip-mapped by FFT band-limiting (one level per octave) and interpolated
linearly between frames and mip levels. A two-operator PM pair (ratio, index,
feedback) and a ring modulator between the two wavetables. A 64-partial
additive bank: SPREAD (harmonic ↔ inharmonic, f_k = f·k^(1+s)), ODD/EVEN,
TILT, CLUSTER (partials gather toward the fundamental), GAPS (a comb of
missing partials), MOTION (per-partial slow walks), FUNDAMENTAL (the first
partial is held in place while the rest drift). A frequency SHIFT in signed
hertz (fine near zero: cubic law) and a separate PITCH shift in semitones —
the two are different operations and have different controls. INTERFERENCE is
a compensated macro: PM index, ring amount, partial spread and shift together,
level-matched by a measured RMS tracker.

### MEMORY — the world remembering itself
Sources: four procedural factory sources built at prepare (VOICE — formant
vowels over a glottal pulse; BROADCAST — a carrier with a repeated synthetic
message and static; CHOIR — a detuned stack of formant voices; MACHINE — a
cyclic industrial figure), a CAPTURE ring (30 s, preallocated; pre-loop,
post-environment or external input; REMEMBER makes it persistent and saved
with the patch), and an IMPORT (WAV, decoded on the message thread, handed
over by an atomic pointer swap). Granular: position, scan speed (signed),
region, pitch (semitones + cents, optional key-follow), duration 5 ms–2 s,
density 0.5–200 grains/s, window (HANN TUKEY TRI EXPO REVEXPO SOFTRECT),
jitter, pitch spread, stereo scatter, reverse probability, scheduling
(CLOCKED IRREGULAR CLUSTERED). A bounded pool of 96 grains; overload steals
the quietest. Spectral: a 2048-point STFT (hop 512, Hann, WOLA-normalised)
with FREEZE, SMEAR, TILT, THIN, DISPLACE (Hz) and EVOLVE (0 = frozen
magnitudes with running phase; 1 = phase-vocoder resynthesis that keeps
moving). Latency of the spectral path is reported to the panel. EROSION is
one control over four components that remain separately reachable: bandwidth
loss, dropout, fragmentation, spectral simplification.

### STRUCTURE — architecture under stress
Modal bank (up to 48 modes, 2-pole resonators, one set per voice) from
templates PLATE, CABLE, BEAM, CAVITY, GLASS whose mode ratios and decay laws
are DESIGNED resonances, not mechanical simulations — the manual says so.
TUBE and COMB are a Karplus–Strong waveguide and a resonant comb. Controls:
pitch, stiffness (inharmonic stretch), damping, density, excitation position,
pickup position, material (frequency-dependent damping), coupling (modes feed
each other through a tanh), drive. Exciters: IMPULSE, NOISE BURST, FRICTION
(a stick–slip bow: a hyperbolic friction curve against the pickup velocity —
documented as a simplified friction model and tested to sustain), or any of
MASS, SIGNAL, MEMORY, the LOOP return or the AUX input. STRESS moves the
bank from linear through mode coupling to fracture bursts. STRIKE injects
energy (a message, a MIDI note when STRIKE ON NOTE is set, or an event lane);
SUSTAIN keeps friction going without erasing the strike.

## 5. The ENVIRONMENT
Four channels with mute, solo, gain, pan, width, high/low cut, spatial send,
loop send. Insert lane A (three slots on the mix bus) and lane B (three slots
on the spatial send) each hold any of: SAT (asymmetric, level-matched by
measured RMS), FOLD (soft → tearing), MULTI (three-band with a protected low
band), SHIFT (Hilbert single-sideband, signed hertz), DELAY (TAPE mode sweeps
pitch with time, CLEAN mode crossfades two heads), COMB (resonant comb +
allpass diffusion), CRUSH (bits and rate with a defined anti-alias mode).
The spatial section is early reflections (8 taps set by SCALE) into an
8-line FDN built from a Householder lossless matrix with per-line losses,
frequency-dependent decay (low/high), modulation, FREEZE, and pre-delay;
APPARENT SCALE moves the early spacing, the FDN line lengths, the modulation
rate and the decay balance together; DISTANCE moves the direct/diffuse
balance and a spectral roll-off, and nothing about width. The FEEDBACK LOOP:
send, return, delay (1 ms–2 s), hp/lp, shift, saturation, damping, and a
RETURN TO selector (MIX, STRUCTURE exciter, MEMORY capture input, SIGNAL
ring input). Energy is metered and shown. Protection lives inside the loop
(DC blocker, tanh bound, finite-value recovery) and at the output (a 2 ms
lookahead ceiling limiter, metered, latency reported). PANIC fades the
output over 20 ms, clears everything, switches the drone off and stays
stopped until a note or the drone switch restarts it.

## 6. LIFE
Eight LFOs (six shapes, 0.0008–50 Hz so a cycle reaches twenty minutes,
free or synced, global or per-voice), four ADSRs, four multi-segment
envelopes (page-owned blobs, up to 8 segments each), four stochastic sources
(SMOOTH, WALK with restoring force, S&H, PROBABILITY, CLUSTER, CHAOS — a
bounded Lorenz-like trajectory with a numeric guard), two followers, four
event lanes (probability, rate, refractory, target: STRIKE / GRAIN BURST /
FREEZE / EROSION PULSE / LOOP KICK / SIGNAL RESET). The 32-slot matrix (a
blob, not host parameters: modulation must not flood host automation) with
signed depth, offset, curve, slew, range clamp and a secondary VIA source.
Modulation is applied as an OFFSET on the host base value at control rate
(every 32 samples), clamped to the parameter's range; the base value never
moves. The LIFE NETWORK: energy, brightness and transient detectors on each
stratum and on the mix, smoothed, with hysteresis and refractory periods;
AUTONOMY sets how far the world may drift from its edited state, COUPLING
scales every cross-stratum term, RECOVERY pulls the drift back toward the
edited state. HOLD LIFE freezes every modulator's state while the audio goes
on.

## 7. HISTORY
Four scenes (AWAKENING, OCCUPATION, COLLAPSE, AFTERMATH by default; renamable).
A scene stores the MUSICAL parameters — everything except output calibration,
quality, PANIC, file state, the drone switch and the seeds. HISTORY (0..1)
interpolates between adjacent scenes along an editable path (each scene's
position on the line); pitch-like parameters interpolate in semitones, times
and frequencies logarithmically, levels in dB, LIST/switch parameters step at
the midpoint. AUTO mode runs HISTORY over a duration (1 s–30 min, free time or
synced to bars) once, looped or ping-pong. HOLD HISTORY stops the traversal;
HOLD LIFE freezes modulators; FREEZE AUDIO is the MEMORY spectral freeze. A
SEED per subsystem and a DETERMINISTIC switch: on transport start the seeds
are re-applied and the modulators re-phased, so an offline render from the
same start reproduces to within the documented tolerance (§14).

## 8. Macros
Eight, each with up to eight destinations (parameter, depth, range), stored
in the patch and remappable. VIOLENCE is level-matched by the saturators'
own trackers so it does not simply get louder. HUMANITY is patch-specific by
construction: its destinations are chosen per preset.

## 9. Modes
DRONE (explicit switch, root, chord, latch, per-stratum participation; a
fresh instance opens SILENT), INSTRUMENT (POLY / MONO / LEGATO, up to 8
voices, velocity, bend, sustain, aftertouch, MPE), EXCITATION (the AUX input
excites STRUCTURE and/or feeds MEMORY's capture). Drone plus keys share the
eight voices: the chord takes its size, the keys the rest.

## 10. Tuning
Semitones, cents; scale constraint (nine scales), ROOT LOCK (the drone root
and the lowest voice stay put when INSTABILITY moves the rest), Scala import
(a `.scl` file, parsed natively, stored in the patch).

## 11. Panel
A console built to outlive its civilisation. Graphite, bone-white, muted
amber, oxidised metal, a warning vermilion, a cold cyan for spectral data.
1440×900 logical, scaled. Header: title, patch, save/load, A/B, undo, mutate,
quality, PANIC. Centre: the four strata with their activity; HISTORY and the
spectral record (the STFT of the output, drawn as strata of rock); the eight
macros; output and emergency. Detailed editing in SOURCES, LIFE, HISTORY,
ENVIRONMENT, MIX views. Every poetic label has a plain subtitle. The page is
a pure view of the native state (PROTOCOL.md).

## 12. Engineering
Native-first (the Black Rider / 1984 pattern): one `SPECS[]` table in
`Params.cpp` drives the APVTS layout, the engine copy, the presets, the
matrix's destination list, the scene interpolation law and the page. No
allocation, lock, file or log in `process()`; every buffer is sized at
prepare. Message-thread work (imports, captures made persistent, source
builds) hands over by atomic pointer swap and reclaims off the audio thread.
Quality LIVE/STUDIO/RENDER = 1×/2×/4× oversampling around the ENVIRONMENT's
nonlinear lane and the grain/partial budgets. Latency reported once at
prepare and stable during playback. Bench in `test/bench.cpp` (plain C++, no
JUCE), console host in `test/host`, panel gate in `test/uiprobe.js`, live
CDP in `tools/`.

## 13. Requirements ledger
Kept in `REQUIREMENTS.md` in the tree: every line of the brief, with DONE /
DEFERRED / BLOCKED and the reason.

## 14. What the build taught (measured)

Bench `test/bench.cpp` (`ttytest`, plain C++, this machine at 48 kHz unless
stated), console host `test/host` (`ttyhost`, the real VST3 wrapper), build
260923.1.

**Tuning and the two kinds of detune.** A2 (note 45) measures 110.00 Hz
(±2 cents) at 44.1 / 48 / 88.2 / 96 kHz. BEAT 0.5 Hz produces an envelope
minimum every 2.00 s at 110 Hz and again at 220 Hz — the beat is in hertz,
so it does not scale with the note, which is the point of having it apart
from cents. FREQ SHIFT +100 Hz moves 110 → 210 Hz (a constant; 210 Hz
carries 30× the power of any residue at 110), PITCH SHIFT +12 moves 110 →
220 (a ratio). The two are different operations and measured as such.

**The additive bank.** SPREAD 0: 220 and 330 Hz present, 270 absent (100×).
SPREAD +50 %: partial 2 lands at 110·2^1.3 = 270.8 Hz while FUNDAMENTAL
100 % holds partial 1 at 110.0; with FUNDAMENTAL 0 partial 1 is still 110
because k = 1 has no stretch. GAPS 100 % leaves the fundamental alone (50×).

**The probe lied before the code did, four times** (the standing family):
note 57 is 220 Hz, not 110 — the bench had "A2" wrong and reported the
engine an octave low at one rate and right at another; a parabolic
interpolation on a log-spaced Goertzel grid was 7 cents off in both
directions (replaced by a 0.5-cent fine scan — Goertzel evaluates the DFT
exactly at any frequency, so no interpolation is needed); two voices of the
same note (a 4 s release tail from the previous noteOn) cancelled the odd
partials of the additive bank and read as "harmonics missing"; and an event
lane's `fired` flag is true for one tick, so sampling it every 480 samples
counted 0 of 20 fires (counted with `fireCount` now).

**Three real faults the first run found.** (1) The Niemitalo Hilbert pair's
delayed branch LEADS by 90°, so `re·cos − im·sin` shifted a 440 Hz sine DOWN
to 340; the up-shift is `re·cos + im·sin`. (2) The modal resonator was
normalised for CONTINUOUS excitation (g = (1−r)·sin ω), which made a 3 ms
strike inaudible (0.0005 rms). Now g = sin ω / 4 so an impulse rings at a
quarter of its size whatever the decay, and continuous exciters are scaled
by the mean loss (1−r)·40 so their steady-state gain lands near unity.
(3) The half-band UPSAMPLER stepped both polyphase branches through the
input one tap per sample: measured, 2× oversampling was WORSE than none
(folded 3rd harmonic −15.8 dB vs −17.1 dB at 1×). The odd-n taps of a
half-band are zero except the centre, so the aligned output is a pure delay
and only the other branch is a real FIR over the LOW-rate history. After:
−80.0 dB at 2×, −76.2 dB at 4× (the Oversampler alone, tanh(8x) on a
12 544 Hz sine); through the whole SAT lane −17.3 dB LIVE, −76.1 dB RENDER.

**`fsin`.** The 5th-order polynomial overshot 1.0 by 0.45 % (1.00452 at the
peak), enough for an LFO at depth 0.3 to exceed its ±0.3 range and fail the
matrix check. The 7th-order term brings the peak to 1.00004.

**The saturator's tracker.** The measured-RMS level match (the Martian Gain
rule) clamped at 0.35× and a drive of 25× pushed the output to a square
wave: +12.6 dB. Drive is 12× now and the clamp 0.12–2.8×: DRIVE 0 → 100 %
changes the level by under 2.5 dB.

**Grains.** With 7.4 grains/s of 100 ms the overlap is 0.74, so the
old √(1+0.5·overlap) normalisation took energy from a stream that had none
to spare; now nothing is removed below one grain of overlap and 1/√overlap
above.

**The space.** Decay set to 2.0 s measures 2.04 s (−29.5 dB/s from the 1–2 s
slope). APPARENT SCALE at 0.9 puts early energy at 100–125 ms after the note
where scale 0.1 has none. The loop at full send, return and saturation with
no damping for 30 s: peak 0.64, rms 0.166 at 5–10 s and 0.168 at 25–30 s —
bounded and not growing.

**Silence and stopping.** A fresh instance with the drone off is exactly
silent (peak 0). PANIC is exactly silent 100 ms later — the output stage's
own filter tails had to be zeroed too (5e−12 is not "stopped"). Two
deterministic renders of the flagship from the same transport start are
bit-identical over 6 s.

**Random machines and the soak.** 120 random valid parameter sets with the
drone on and a note, random block sizes 64–460: all finite, worst peak
0.63. The flagship with VIOLENCE 0.7, CONTAMINATION 0.6 and LIFE 0.8 through
a 60 s HISTORY loop for 90 s: peak 0.15, rms 0.038 at 10–20 s and 0.032 at
80–90 s. All 48 presets load, sound and stay under the ceiling.

**Levels.** The first gain structure put most presets at −30…−40 dBFS rms;
after raising the default LEVEL to 0.6, VOLUME to 0.7 and the voice gains
by 30 % the flagship demo sits at −13.5 dBFS with a peak of 0.88, the quiet
scene at −37.5 (by design), the bass phrase at −24.

**Cost (this machine, one core).** COLD START, one note 1.9 %; the
flagship with drone + 2 notes (6 voices) 14.7 %; 8 voices with every macro
up 21.6 %; THE SEA IS MADE OF IRON drone 5.4 %.

**The host.** `ttyhost` on the built bundle: instrument, 400 parameters
offered (395 + the five BWFX macros; 2 481 with JUCE's emulated MIDI CCs),
every one automatable except QUALITY, SEED, DETERMINISTIC and PATCH, an AUX
input bus, latency 96 samples, exactly silent with no note, a note-on plays
(peak 0.115), DRONE through the host plays (0.138), CUTOFF set to 0.25 reads
back and shows 112.47 Hz, state (105 KB: values, rack, macros, slots,
shapes, scenes, Scala) restores a moved value, a second instance
instantiates, 44.1 and 96 kHz at 128-sample blocks play finite. 16 checks.

## 15 · The gain structure round (2026-09-23), and the decals

Peter, after playing it: *"i think that there are gain-structure issues, in
terms of many presets that seem to contain clipping effects although the total
output is below 0 dB"*. He was right, it was two faults one after the other,
and every existing check passed throughout because they all measured the
engine and none measured the BANK against the output stage.

**Fault one: the filter was distorting at DRIVE ZERO.** `FilterPair::set` wrote
`sat = 0.6 + drive * 3.0`, so the ladder's tanh saw 0.6x the signal with the
drive knob shut. Measured on a single sine with the filter wide open: **THD
-34.8 dB**, and **-21.3 dB** for two oscillators at full, which is audible
grit on a patch nobody asked to be dirty. Two changes fixed it and both are
worth keeping:

* the drive law became `sat = 0.02 + 5.5 * DRIVE^2`, which is the identity at
  rest and a fuzz at the top;
* the filter was given an **operating level**, `IN_SCALE = 0.55` in and
  `1/0.55` out, the way a desk stages gain. Since the filter is linear at rest
  this is exactly transparent, and it is what stops two oscillators summing to
  1.6 and driving a saturator that was never asked for.
* the ladder's own bound became `ceilSoft(..., 2.0)` rather than a
  drive-dependent tanh, so a self-oscillating resonance is still held at a
  musical amplitude without colouring every ordinary note on the way.

Measured after: **-87.2 dB** on a sine, **-86.6** at full level, **-84.0** for
two oscillators at full, and DRIVE at 100 % is still **-9.0 dB**, i.e. as dirty
as it ever was. Resonance still sings: LADDER lifts **+21.5 dB** over RES 0.

**Fault two, which fault one had been hiding: the limiter was doing the
compressing.** Removing the filter's accidental gain reduction let the real
levels show, and they were far too hot - `Limiter::reduction` is
INSTANTANEOUS, so the **0.58** reading at the end of a four-second settled
drone is **7.5 dB of continuous gain reduction**, not a transient being
caught. A limiter squashing a drone for its whole length while the output
meter reads under 0 dBFS is precisely "clipping effects below 0 dB".

Three things had to be true before that could even be measured:

* **A stage meter has to be measured where its name says.** The ST_PRE_LIMIT
  tap sat BEFORE the master gain, so a preset could report "pre-limit 0.65"
  and "out 0.84" in the same row. It is taken inside `OutputStage` now,
  immediately before `lim.tick`.
* **`OutputStage::noLimit`** (probe only) bypasses the limiter AND the +-1.5
  safety clamp, because the question is what a patch asks for, not what the
  limiter allows. Four presets were reading peak exactly 1.500 - the clamp,
  not a coincidence.
* **The window has to fit the bank.** A first pass discarded three seconds and
  measured the next four, and reported NAKED STRUCTURE at exactly 0.000. It is
  "a struck plate and nothing else": the probe had discarded the whole sound.
  Four more sparse presets read 0.05-0.15 for the same reason. The metric that
  fits a bank of infinite drones AND one-shot gestures is **24 s from note-on,
  loudest 2 s rms**, measured at **h_pos 0 and h_pos 1 and the louder kept** -
  a piece written to grow over ten minutes is not judged on its first bar.

**PATCH TRIM** (`outtrim`) is the fix: -24..+8 dB, written by the preset,
travelling through HISTORY. Its default is v = 0.75, which lands on **exactly
0.0 dB** (0.75 and 32 are both exact in binary, so `dbGain` returns exactly
1.0f) - so a preset that never mentions it is bit-identical to one written
before it existed. It exists because VOLUME belongs to the player: a preset
moving the player's fader is two owners for one control, which is the bug the
BWFX macro rollout cost four rounds to find.

**A round that achieved nothing, recorded because the reasoning was plausible
and wrong.** Between the two faults I trimmed the voices and raised VOLUME to
compensate - and the master is applied INSIDE the output stage, upstream of
the limiter, so the limiter saw exactly what it had before. STRESSED PLATE
went 0.50 -> 0.47. Moving gain from one side of a limiter to the other side of
nothing changes nothing.

Result across the 48: **limiter reduction 0.000 on every preset** (worst was
0.58), nothing reaches the clamp, worst peak asked of the output stage 0.750,
and the bank's loudness spread **33 dB -> 16.8 dB**. New permanent bench
section **[12] the bank against the output stage**, 4 checks, which is what
would have caught the complaint.

### The decals (delivered 2026-09-23, all six)

Identified by measurement rather than by the order they arrived
(`tools/decal-id.ps1` reports mean luminance, bone fraction and corner alpha);
ingested by `tools/ingest-decals.ps1`, which crops every part to its measured
content box, mirror-tiles the two grounds into seamless squares, and scales
the glass to the alpha it was ordered at. Corrections applied and recorded:
panel luminance x1.03 to 14 %, plate x0.74 to 20 %, glass alpha x0.64 to 35 %.
Only the wordmark arrived padded (50 % of its canvas), which is normal for
type on alpha. Every letter was inside the canvas - the 1984 fault did not
recur. 2.0 MB for the six, DLL 9.4 MB.

Four faults, every one found by LOOKING at a render of the live plugin:

* **`file(GLOB)` runs at CONFIGURE time**, and the decals folder was created
  afterwards - so all six were silently absent from a build that reported
  success. CMake now prints the count it compiled in.
* **`color:transparent` on a parent cannot silence a child that sets its own
  colour**: the oxide `<b>` inside the h1 went on painting a ghost "YEARS"
  beside the artwork.
* **`background-size:cover` stretches a ground tile to each panel's aspect**,
  so the machined grain ran at a different angle and scale on every sub-panel.
  A ground repeats; it never stretches.
* **A per-panel overflow check cannot see a ROW overflowing the deck.** Adding
  PATCH TRIM to the foot's OUTPUT panel took the row to 1470 px in a 1440 px
  deck and cut BASS MONO off at the edge, with all 89 panel checks passing.
  There is a row check now, proven by re-breaking it. (PATCH TRIM moved to the
  MIX view's OUTPUT panel, where the patch's other levels live.)

And two stale counts of the same family: **`test/params.json` is generated
from the table and goes stale exactly like a typed count** - adding a
parameter left the panel gate happily reporting "395 controls for 395
parameters" with 396 in the table, so it now fails if `Params.h` is newer than
the fixture; and the host test's hardcoded `np == 400` is derived from
`NUM_PARAMS + 5` now instead of being bumped.

## 16 · The panel round (2026-09-23): a cramped row, and a headline feature that was dead

Peter played it, said it sounded great, and then reported two things in the
same breath: *"this row of buttons are a bit cramped, blocking the text and
number values"* and *"how do i use History, i am not sure its active?"*,
followed by *"I tried to switch it on, enter the different scenes, but could
hear or see no movement, motion or evolution"*.

### The macro row was squashing its own children

`.mac` is a flex column with an 84 px content box, and its children declare
12 + 52 + 18 + 8 = **90 px**. Flex did what flex does. Measured on the live
panel: the macro NAME was rendering at **8 px of its declared 12**, and the
value readout was clipped by the bottom of the knob's own 52 px box - which is
exactly "blocking the text and number values".

The repair is `.mac > *{flex:none}`; the rest is finding the room honestly:

* the gloss dropped from a two-line box to one. **Measured every gloss on one
  line: 55-99 px against 159 px of cell** - the second line was never used and
  was 9 px of pure waste.
* the control box went 52 -> 56, which is the knob (44) plus the value line
  (11) plus a pixel, so the value cannot be clipped by its own container.
* the row went 86 -> 96 out of the deck's **own measured spare 12 px**
  (children 828 + gaps 32 + padding 28 = 888 in a 900 px deck), so nothing
  else on the deck moved.

Cell budget afterwards: 89 px of box against 85 px of children.

### And the header had been overflowing since the decal round

The same measurement pass found `#head` reporting **h=74, scrollH=86**. The
QUALITY control is a generic `.ctl` and those are 72 px tall, so `#hbtns`
wanted 98 px in a 74 px header and drew its top button row **12 px above the
header's own box**. A header control gets a header-sized box now. Nobody had
reported it, and no check had looked: the panel gate gained a row-overflow
check in the decal round but the header's *contents* were never measured.

### HISTORY: the mechanism was perfect and the feature was dead

Confirmed end to end in the running plugin before changing anything. On preset
0, with the switch armed, the four scenes interpolate the feedback send

    pos 0.00  0.0000      pos 0.33  0.1189
    pos 0.67  0.2491      pos 1.00  0.1500

which are the four stored values exactly, and with HISTORY off the same sweep
moves nothing. The engine bench had always covered this - it interpolates,
AUTO runs, HOLD stops - **but the bench sets `scene[]` directly in C++ and
never exercises the path the panel uses**, which is the path Peter used.

The fault was not in the mechanism at all:

* **`h_on` defaults OFF**, and it is `F_NS`, so scene interpolation can never
  set it either;
* **eleven presets store four scenes each and not one of them armed it** - a
  whole journey written into the patch that nobody could hear;
* and **nothing on the panel said any of this**. A slider that moves and
  changes nothing is indistinguishable from a broken one.

Ten presets are armed now (the eleventh "match" was my own scanner hitting a
C++ array initialiser - see below), and every HISTORY panel reports its own
state in its title bar, where a slogan used to be: no scenes stored, one scene
stored, switched off, travelling by hand, or running. It costs no height,
because the panel header already had a note element.

**The status line first went UNDER the panel and overflowed both HISTORY boxes
(182 > 166 and 230 > 222).** A panel that already measures to its box has no
room for an extra line; the title bar did.

### Three self-inflicted faults in one round, all the same family

1. **A scanner that matches `{ "` matches C++ as well as data.** Arming the
   presets, my entry finder hit `static const char* N[NUM_MACROS] = { "MASS",
   ...`, counted quotes to the seventh and wrote `h_on=1 ` into the macro name
   table - producing `"h_on=1 INSTABILITY"`, which would have broken
   macro-by-name lookup in preset parsing. Caught by **reading the file
   afterwards**, not by the script's success message, which said 11 armed when
   the true answer was 10.
2. **A null anchor threw on the first tick.** `p._ti.querySelector("i")`
   returns null for a panel declared without a note, and the null then threw
   inside `syncHist`, taking the page from 90 checks to 61. Create the element
   rather than pushing the null.
3. **Two identical anchors in one patch script.** After the first edit, the
   second anchor matched twice. Include enough preceding context that each
   anchor is unique in the ORIGINAL text, since the script validates them all
   before writing any.

### The bench had been leaning on a preset default

Arming preset 0 broke two LIFE NETWORK checks, and the bench was right to
fail: the test sets `l_coupling` in `p` and preset 0 now ships scenes that
quite correctly override it. The test pins `h_on = 0` now, because it is
measuring the network and not HISTORY. **Set every parameter a probe does not
mean to test** - and note the general point it exposes: with HISTORY armed, a
patch's `p` value for any scene parameter is not what you will hear.
