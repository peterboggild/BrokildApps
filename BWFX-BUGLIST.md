# BWFX — bug list & idea collection (batch builds)

Peter's standing workflow, same as the Clone Wars BUGLIST: fixes, improvements
and feature ideas for the Brokild World FX system collect HERE instead of
forcing a build each. When Peter says go, the open items ship in one pass —
and because BWFX is compiled into every synth, one pass reaches all seven
products on their next rebuild. Add findings under each item while
investigating; move shipped items to DONE with the BWFX version that carried
them. (Synth-specific issues stay on that synth's own list — this file is for
the world rack: its modules, the overlay, the rack machinery, SPECTRA.)

## Open

### 2. SPECTRA characters — phase C still open (A shipped 1.3.0, B in 1.4.0)
Remaining:
- **C. The other six host mappings (hours each).** Five bus inputs per
  voice engine + bench additions per synth. PS2 LAST, with an open
  question flagged for Peter: it would then carry two spectra systems
  (native panel + world rack) — whether the native panel retires is a
  separate per-synth decision, like native-FX retirement. Note the bar
  phase B set: audio characters (pink/black/glass) already work in every
  host — phase C is only about the pure modulators (dark drone, tape,
  insect) and the modulation half of pink/glass.

### 3. Manuals do not mention BWFX yet
All seven PDF manuals predate the rack (deliberately skipped in the
2026-08-26 rollout, Peter's instruction). One shared "The World Rack"
section could be written once and dropped into each manual's pipeline;
landing-page prose could carry one feature line each.

## Done

### 2b. SPECTRA phase B + built-in presets — SHIPPED in BWFX 1.4.0
The four remaining PS2 characters, split by what honestly ports (Peter's
"cautiously" + "no change to functioning synths" + "presets built in"):
- **DARK DRONE** (cluster/sag/drift/drift-time): pure bus modulator with
  PS2's faithful drift law (tau 300*0.1^(v/100) s, OU walk, cutMul
  2^(c*d*1.2), det d*10 c). The sub-oscillator did NOT port — no note on
  the bus, and an audio-domain octaver on polyphonic material is mush;
  skipping beat faking (documented in the handover).
- **PSYCHEDELIC PINK** (swirl/smear/bloom): the three swirl LFOs
  (x1/x0.618/x0.29) on the bus + a private phaser->chorus->reverse-reverb
  wash. PS2's whole-image pan swing became a breathing width (the bus
  carries spread, hosts fan it per voice).
- **INDUSTRIAL BLACK** (grind/chop/clang): pure FX character — private
  crusher->tube->stutter->tape-echo, PS2 gain discipline kept (satGain
  law 10^(v/32) = 0.625*v dB), GRIT noise pinned 0 for silence-safety.
  The comb filter stayed behind on purpose (PS2's self-oscillation spot).
- **GLASS CATHEDRAL** (halo/shine): private hall + the 0.05 Hz ±2.2 c
  breath. AIR (attack shaping) needs the host envelope — dropped rather
  than left as a lying knob.
Machinery: characters may own audio DSP (Character::hasAudio/prepare/
resetAudio/processAudio/service) reusing the rack's own module classes
privately (PrivateFx); the rack crossfades them by presence with the same
env pattern as pedals, BEFORE the chain (possession first, pedals shape
it). **Audio characters therefore work in EVERY host today** — the
overlay shows them everywhere (per-character `audio` flag), pure
modulators only where busLive. The additive contract held: unarmed =
memcmp bit-identical, presence 0 = bit-transparent, silence-safe,
deterministic, disarm click-bounded.
**Built-in presets**: 10 curated sparse rack blobs in C++
(numPresets/presetName/presetBlob, presetsJson; `bwfxtest --presets`
generates the fragment's DEFAULT_PRESETS), applied through the new
adapter op "blob" -> fromJson — the same tolerant path patches ride.
PRESETS select in the overlay header. Bench 358 checks ALL CLEAR
(every preset bounded + silence-safe, pattern preset round-trips its
KIERANATOR grid); UI probe 16/16; cwtest ALL PASS.

### 2a. SPECTRA phase A — SHIPPED in BWFX 1.3.0
The character rack is live: registry (Descriptor reused, kMaxChars 8),
TAPE SEANCE (wobble/sag/dull) + INSECT SWARM (swarm/flutter/skitter),
world-mod bus {detuneCents, panSpread, tremDepth+tremRate, pitchSag,
filterMul} published as atomics, combination rules as measured in PS2
(det/pan add, muls multiply, trem depth unions, sag max), PRESENCE
scales a character's grip and rides the morph (characters defect at 0.5
like modules). Trem ships as depth+rate so each HOST fans per-voice
phases (golden-angle). PS2's 60 Hz per-tick walks converted to
time-constant form so characters sound the same at any tick rate.
First host mapping = Clone Wars 260826.5: five bus inputs across the 16
clones, sag keyed to a 50 ms smoothed gate, all guarded by wmActive —
neutral bus proven bit-identical by memcmp (render_test scenario 8).
Hosts declare the mapping with setWorldModConsumed(true); the overlay
shows the live SPECTRA rack only there (busLive), a placeholder plate
everywhere else. Bench 306 checks ALL CLEAR, UI probe 16/16.

### 4b. ROTARY — SHIPPED in BWFX 1.3.0
A Leslie with real inertia: 12 dB/oct crossover at 800 Hz with horn EQ,
counter-rotating horn and drum, each rotor its own AM (beam sharpening +
back-lobe) and doppler FM on a Catmull-Rom line, belt wobble, stereo mic
pair. SLOW/FAST/BRAKE with independent per-rotor ramp times (horn ~1 s,
drum ~4 s) — the bench MEASURES both ramp windows and the doppler depth
via a normalized-autocorrelation envelope-rate estimator (probes at
3 kHz/100 Hz, off the crossover, after the 1 kHz probe caught the real
horn−drum beat through crossover leakage).

### 5. KIERANATOR (the GLITCHER) — SHIPPED in BWFX 1.3.0
Step-sequenced havoc, self-contained: a 10 s rolling capture buffer that
every effect is a way of READING — RETRIG, TAPESTOP, REVERSE, SHUFFLE,
PITCH, CRUSH, GATE per step, 16 steps over 1 or 2 bars, 5 ms raised-
cosine edges, host-transport aligned (Rack::setTransport) with an
internal clock fallback. Pattern lives in the module's opaque `extra`
blob (16×3 bits in one atomic); the fragment grew its first custom pedal
editor — a paintable step grid with 8 brushes. Named for the late
Kieran Foster (dblue), author of Glitch 2, with a tribute line on the
pedal (one line in the fragment; Peter may retire it later). Own build
from scratch — concept homage only, no code or UI copied.
Machinery that shipped with it (useful beyond this module): setTransport
(ppq+bpm+playing, extrapolated per sub-block), Module::getExtra/setExtra
with "x" in the blob and morph defection at 0.5, Descriptor::custom.

### 4-survey. Parked by Peter ("not now" — do not re-propose, kept for
the record): FLANGER, plain TREMOLO/AUTO-PAN, standalone TONE EQ, FILTER
pedal (Black Rider circuits), FREEZE, RING MOD, micro-pitch/octaver,
granular. Available whenever wanted.

### 1. Host-tempo sync — SHIPPED in BWFX 1.2.0
SYNC (FREE|2/1..1/32) + FEEL (STRAIGHT|TRIPLET|DOTTED) on ECHO, GATE and
the new HARMONIC. Drives the SAME smoother the knob does — no second code
path. Engaging sync or a big tempo jump SNAPS the smoother (gliding in
lands the first echo early, the Black Rider lesson); tempo changes glide,
which is the tape bend you want. ECHO buffer grew to 5 s so 2/1 is real at
normal tempos. Rack::setBpm + Module::setTempo carry the clock; all seven
adapters feed it (Mars Wars, Escape Room and Hairfryer gained a playhead
read). Measured at 120 BPM: echo 1/4 = 0.5000 s, triplet 0.3333, dotted
0.7500; gate 1/4 = 0.5000. FREE and no-host-clock are bit-identical to
1.1.0, proven by memcmp.

### 4a. LOFI / STRIP / SHIMMER / HARMONIC — SHIPPED in BWFX 1.2.0
GRIT (PS2 lofi port; NOISE defaults 0 so a fresh unit is still silent in /
silent out), STRIP (Hairfryer soft-knee comp with its two-stage gain
smoothing — fast catch, slow breathe — plus a 5-band musical EQ, one COMP
macro opening threshold and ratio together), SHIMMER (Blade Ruiner FDN-8
with the octave-up folded INTO the loop, ceilSoft in the loop per the
runaway lesson), HARMONIC (brownface split at 800 Hz with the halves
tremolo'd in opposite phase, plus TREM and true-pitch VIBRATO).
kMaxParams went 8 -> 16 for STRIP; old blobs round-trip unchanged. Biquad
gained RBJ shelves. The UI DEFAULT_DESC is now GENERATED from the C++
(bwfxtest --desc), never typed. Bench 260 checks ALL CLEAR, UI probe 21/21.
