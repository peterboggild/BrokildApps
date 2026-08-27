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

### 2-wave-2. PS2 retirement + MW/HF rack removal (Peter's decisions
2026-08-26 evening, recorded verbatim; build AFTER he has played with
phase C):
- **Mars Wars and Hairfryer are FX plugins, not synths — they do NOT
  get BWFX.** Their racks (added 1.1.0) are to be REMOVED COMPLETELY
  (button, overlay, processing, state); stale rack blobs in day-old
  projects are silently ignored (his OK on record).
- **PS2 retirement**: internal FX chain + native SPECTRA panels retire
  from the VST3 — **HARD CUT** (his call): old presets/projects load
  with the FX/SPECTRA portion gone, no migration. The **FX photo REMAPS
  to the rack**: R→SPACE length, G→ECHO mix, B→TUBE drive. The offline
  MIDI→WAV render must run through the rack or renders come out dry.
  The BROWSER twin (music-apps/photo-synth) stays exactly as it is —
  Peter: "a toy version of the professional (VST3)". Interim: PS2
  carries both spectra systems until this lands — known and accepted.

### 3. Manuals + homepage — DONE for the five SYNTHS (2026-08-27)
Peter: "update the homepage, so that the screenshots are accurate and the
manuals updated… the descriptions also need an update. Focus on the
synths, not the effects."
- **Manuals**: a shared "The world rack" chapter, written once, inserted
  into Photo-Synth 2 (07), Blade Ruiner (09), Black Rider (10) and
  Escape Room (12) in the family-A house style, and HAND-PORTED into
  Clone Wars (12) — family B is a different page model (dark hull,
  explicit `.page` divs, `.chap`/`.no`/`.lede`, no h3). Trailing chapters
  renumbered in DESCENDING order so numbers never collide. Three plates
  per chapter. PDFs rebuilt: 38 / 26 / 18 / 18 / 12 pages.
- **Screenshots**: every synth's hero plate re-shot from the live build
  (all predated the BWFX button) + a world-rack plate per synth + the
  front-page preview cards regenerated. Pipeline in the scratchpad:
  `shoot.js` (element capture over CDP), `shoot-one.sh` (launch → shoot →
  kill, with the SAC byte-append fallback), `tojpg.ps1`.
- **Descriptions**: all five app.json descriptions rewritten, and a "The
  world rack" section added to each landing page above the download
  block. Three app.json manual page counts were stale BEFORE this work
  (Escape Room claimed 15 for a 25-page PDF) and are now measured.
- **Mars Wars and Hairfryer deliberately untouched** — FX plugins, not
  synths; they lose the rack in wave 2, so documenting it would have
  been wrong twice.
Still open: nothing for the synths. When wave 2 lands, Photo-Synth's
chapters 05 (Spectra) and 06 (the effects rack) come OUT and its
description drops the last internal-FX references.

### 6. PARITY PASS — bring three modules up to Photo-Synth's originals
(Peter, 2026-08-27, hesitating about the wave-2 retirement: "the built in
versions are more powerful than the BWFX versions, better integrated. Is
there some way to have versions that are more similar to the original?")
**MEASURED, module by module — he is right about three and only three:**
- PARITY ALREADY: SWEEP vs PS2 phaser (both 4-stage, dry/wet == mix),
  ENSEMBLE vs PS2 chorus (both dual-line), GRIT vs PS2 lofi (a literal
  port, same three controls). GATE is RICHER than PS2's stutter (it has
  host sync). SPECTRA ported with measured coefficients, so the
  characters' modulation half is faithful.
- **TUBE is poorer**: PS2 has satDry, satWet, satPre and satToneF — four
  independent controls. BWFX has DRIVE and TONE, and DRIVE *is* the
  blend, so "full wet, gentle drive" and "subtle blend, hard drive" are
  both unreachable. Fix: an independent MIX. Kemper-safe shape is a
  LINK choice (DRIVE-LINKED default = today, MANUAL exposes the knob).
- **ECHO is poorer in CONTROL only, not in DSP**: it already has the
  damping low-pass, the loop high-pass and the wow inside it — they are
  just hardcoded per CLEAN/TAPE character (2600/4200 Hz, 120/25 Hz,
  0.0007). PS2 exposes dampF, dhpF and wowDepth as continuous knobs.
  Fix is nearly free: add DAMP, LOW CUT and WOW as **offsets** with
  default 0, so every existing rack is bit-identical. PS2's FREEZE
  (infinite hold) is genuinely missing — add it as a switch, default off.
- **SPACE is genuinely simpler**: PS2 runs TWO convolver slots with
  independent gains (rvG0/rvG1) and blends them, which is how its
  reverb morphs continuously between spaces. BWFX SPACE has ONE
  PartConv; its internal dual-spectra crossfade only covers click-free
  IR rebuilds, it is not a blend you can sit inside. Fix: a second slot
  plus CHARACTER B and BLEND (default 0 = today's sound exactly). This
  is the only one with a real cost — roughly 2x the reverb's CPU and
  memory when BLEND is engaged.
**Integration is NOT the problem it looks like.** The FX photo drives
R→reverbLength, G→delayMix, B→satGain — all continuous, and all three
exist as continuous BWFX params already, settable per block from the
host. The photo can drive the rack exactly as it drove the chain.
**The one thing that genuinely cannot port** is the other half of PS2's
SPECTRA: its characters are also host-control MACROS that rewrite PS2's
own panel (Industrial Black sets waveform=square, attack 2, release 8,
legato off and switches the filter to comb). A BWFX character publishes
a modulation bus and carries private FX; it cannot reach a host's own
parameters. Porting that needs a new "character macro" layer where a
character declares host-parameter targets and each adapter maps them —
per-synth work, which is exactly why phase A left it out.
**RECOMMENDATION: do this parity pass BEFORE any retirement, and split
the wave-2 decision in two.** Retiring PS2's FX chain is then a pure
win; retiring its native SPECTRA is a separate call, because the macro
half is a real loss until the macro layer exists. Nothing here is
urgent — the current state (both systems live in PS2) is stable.

## Done

### 2c. SPECTRA phase C — SHIPPED with BWFX 1.5.0 (fleet 260826.5, CW .7)
All five SYNTHS now map the world-mod bus (Mars Wars/Hairfryer excluded
by Peter's decision — FX plugins get no bus and lose the rack in wave 2):
- **Black Rider**: full per-voice mapping (detune fan, gate-keyed sag,
  per-voice trem, pan spread, filterMul on the working cutoff; a
  VCA-mode drone counts as held so it never sags while sounding).
  Bench section [12]: 6036 checks ALL CLEAR — sag measured exactly
  100 cents down in the release tail, in tune held.
- **Blade Ruiner**: DECKARD full per-voice; LA fans detune across its
  9-saw stack + filterMul on the smog + layer trem (sag skipped: a
  drone is held by definition and its env is an AMP env); REPLICANT
  filterMul + trem (detune skipped: one voice — un-fanned det would
  move the note). ALL CLEAR incl. the bare-drone filterMul probe
  (grains/rain bypass the smog and had floored the first measurement).
- **Escape Room**: the door IS the voice — detune/sag land on the
  per-voice filter frequency, filterMul on the CUT identity, trem+pan
  as per-voice gains at control-tick rate. ALL CLEAR (held resonance
  in tune, the −100 c twin dominating the release tail).
- **Photo-Synth 2**: detune fan + bus sag riding the SAME smoothed-gate
  multiplier as the native envPitch sag, filterMul on cutEff, trem+pan
  per voice at block rate. Neutral = exact-identity multipliers (×1.0,
  +0.0 are IEEE-exact) so no branch in the voice loop. NOTE: PS2 has no
  offline bench — verification is compile + code review + live CDP, not
  a rendered proof. Also fixed en route this wave: overlay reorder DRAG
  (Chromium releases pointer capture on mid-drag reparent — listeners
  moved to the document), off-state legibility (dim ember in the OFF
  rocker lens, off pedals 0.62→0.85 opacity), and TUBE level neutrality
  (linear correlated-blend + measured RMS auto-gain, worst settled
  deviation 0.84 dB across DRIVE at two input levels — was +3/6.4 dB).

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

### 7. PS2: retire the internal FX chain, KEEP native SPECTRA — DECIDED
2026-08-26 (Peter: "ok, lets lose the PS2 FX chain, and leave the SPECTRA
as is"). This splits BUGLIST 2-wave-2 exactly as recommended:

  * **GO** — remove Photo-Synth 2's own seven-module FX chain from the
    VST3. Hard cut, no migration (his earlier call). FX photo remaps to
    the rack: R -> SPACE length, G -> ECHO mix, B -> TUBE drive. The
    offline MIDI->WAV render must then run through `Rack::process`, which
    it does not today — that is the one piece of real work in this item.
    Do the item 6 PARITY PASS FIRST (TUBE MIX, ECHO damp/low-cut/wow/
    freeze, SPACE second slot) or the remap lands on a poorer reverb than
    the one it replaces.
  * **HOLD** — PS2's native SPECTRA panel stays exactly as it is. It is
    not only a modulator: each character is also a MACRO that rewrites
    PS2's own panel (Industrial Black sets waveform=square, attack 2,
    release 8, legato off, filter=comb). BWFX characters structurally
    cannot reach a host's own controls. Keeping it costs nothing — it
    already coexists with the BWFX character rack (verified: PS2 calls
    `setWorldModConsumed(true)`, so both racks are live and they stack).
  * The browser twin (music-apps/photo-synth) is untouched either way —
    "a toy version of the professional".

### 8. Host characters inside the BWFX drawer — SPEC, awaiting go
Peter: "Can the native SPECTRA appear also when BWFX is chosen?" They
already coexist, but in two different places on the panel. Proposal, and
it is general rather than a PS2 special case:

  * The adapter's state payload gains `hostChars: [{id,name,armed,
    presence,colour}]`, published by the host synth, plus a group title.
  * The fragment renders them in the character column under a divider
    ("THIS INSTRUMENT" above "BROKILD WORLD"), using the SAME rocker and
    PRESENCE control, and sends `{k:"hostchar",id,...}` back to the PAGE
    (not to the rack) — the host owns them, BWFX only draws them.
  * Zero DSP change, no blob change, nothing to migrate: a host that
    publishes no hostChars renders exactly today's drawer.
  * Wins twice: PS2 gets one place to arm any character, and any future
    synth with instrument-specific characters (a drum machine's per-kit
    macros, say) inherits the same drawer for free.

### 9. KIERANATOR: A/B pages to 32 steps, and a LAST step — SPEC, awaiting go
Peter 2026-08-27, while playing Full Metal Racket: "can I have an A and B
there as well, allowing up to 32 steps. Also, here I'd like a last step, to
create polyrhythms." He noted himself that rolling it out is a big job.

Why it is a big job: the pattern is **16 x 3 bits packed into one atomic
uint64** in the module's opaque `extra` blob, and that word is exactly 64
bits wide. Thirty-two steps does not fit — it needs two words, or a wider
representation, and the atomicity is the point: the audio thread reads the
pattern without a lock. Options, in order of preference:

  * TWO atomic uint64s, page A and page B, read as a pair. The audio side
    only ever needs the word for the page it is currently in, so a torn read
    across the pair is not observable — the same trick that made one word
    safe in the first place.
  * A LAST STEP (1..32) alongside, so the loop can end anywhere. That is what
    makes the polyrhythm, and it is cheap: the step index becomes
    `k % last` instead of `k % 16`.

Precedent worth copying: Full Metal Racket's sequencer already does exactly
this per lane, and the interaction question turned out to matter more than
the feature. LAST was there from the start as a lane length and Peter could
not find it, because it was drawn as dim text rather than as a control. The
three ways in that worked: DRAG the field for any value, CLICK it to jump
through musical lengths (1 2 3 4 6 8 12 16 24 32), and RIGHT-CLICK A STEP to
end the loop there — the last of those is the one that needs no explaining.

Compatibility: an existing blob carries one 16-step word. It must load as
page A with LAST = 16 and page B empty, so every KIERANATOR pattern already
saved sounds identical. That is the Kemper rule and it is not negotiable.
