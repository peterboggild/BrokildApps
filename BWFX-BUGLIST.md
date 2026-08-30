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

### 9. KIERANATOR: A/B pages and a LAST step — SHIPPED in BWFX 1.6.0
Peter 2026-08-27, and **his model supersedes the one I proposed in the first
two versions of this entry.** He described it as:

> Two bars, A and B. The 1 or 2 bars would then double the length to 4 bars
> if I have both A and B. Default is 16 as last step, corresponding to
> patterns distributed over 1 bar if 1 bar is selected, and 2 bars if 2 bars
> is selected.

That is better than what I suggested, and for a concrete reason: I wanted to
reinterpret BARS as a step DIVISION, which needed a migration table to stay
compatible. His keeps BARS meaning exactly what it means today — how long a
page is stretched over — and gets the extra length from the second PAGE
instead. So:

| BARS | page A | page B | LAST 16 | LAST 32 |
|------|--------|--------|---------|---------|
| 1    | 1 bar  | 1 bar  | 1 bar (today exactly) | 2 bars |
| 2    | 2 bars | 2 bars | 2 bars (today exactly) | 4 bars |

**Compatibility falls out for free**: an existing pattern is LAST 16 with an
empty page B, which is bit-for-bit what it does now. No reinterpretation, no
migration table, nothing to get wrong. LAST still gives the polyrhythm —
set it to 12 or 25 and the loop stops agreeing with the bar.

Two things from reading the module that still hold:

  * the pattern is read **once per block**, so use a two-slot buffer with an
    atomic index rather than packing harder. 32 steps x 3 bits is 96 bits and
    will not fit the uint64 it uses today;
  * `getExtra` must return a **byte-identical string** for a pattern nobody
    has edited — not merely one that sounds the same — or every rack blob in
    every saved project changes and the memcmp checks fail for no musical
    reason. So: 16 characters out whenever steps 17-32 are empty, 32 only
    once something is written there.

A/B is an editing page, not a second pattern: the pedal is too narrow for 32
cells at once.

### 10. KIERANATOR: a RANDOM brush — SHIPPED in BWFX 1.6.0
Peter: "could you make a new FX, RANDOM, that selects a random one of the
other effects."

A ninth brush that resolves, per hit, to one of the eight real ones. Two
details decide whether it is usable or a mess:

  * **Seed it from the position**, the way the sequencer's probability is
    seeded — from bar and step. Then a bar is reproducible while still
    differing from the bar before, a re-render gives the same audio, and the
    bench can test it at all. A free-running rand() would be none of those.
  * **Exclude RETRIG-into-itself and the silent combinations**, or the brush
    will sometimes land on the one that does nothing audible and read as a
    dropout rather than a choice.

Costs one value in the 3-bit step field, which currently holds 0-7 — RANDOM
would be the eighth non-empty value and the field is already wide enough.

### 11. KIERANATOR: a CHAOS slider — SHIPPED in BWFX 1.6.0
Peter: "a CHAOS slider that introduces variabilities in the parameters,
depending on how far it is set to the right, as well as with increasing
frequency making a single temporary effect application on a certain slot."

Two behaviours on one knob, which is right — they are the same idea at two
scales:

  1. **Parameter jitter.** Each time a step fires, that effect's own
     parameters are perturbed by up to +/- CHAOS percent. Applied at the
     STEP, not continuously, so it is a different-sounding glitch each time
     rather than a wobble.
  2. **Uninvited glitches.** With a probability rising with CHAOS, a step
     that is OFF fires a single temporary effect anyway. That is the half
     that makes it feel alive rather than merely loose.

Seeded from bar and step like everything else here, so it is reproducible.
At 0 it must be **exactly** inert — the bench should memcmp a render at
CHAOS 0 against the same render with the parameter absent, the way the
empty-rack check works.

### 12. KIERANATOR: colour the brush keys — SHIPPED in BWFX 1.6.0
Peter: "colour the selection buttons below the 16 sequence in the same
colours but dimmer (GT GATE etc). They highlight when selected and get a
slight halo."

Each brush gets a hue; the key carries it at low saturation, comes to full
strength when selected, and takes a soft glow. The steps painted with that
brush should carry the same hue, which is the real win: a pattern becomes
readable at a glance instead of being sixteen identical lit cells.

Worth doing at the same time as 9-11, since all four touch the same fragment
and the same editor.

### 13. SAVE and LOAD a rack to disk — SPEC, awaiting go
Peter 2026-08-27: "can I also push Save preset and Load preset in the BWFX
module. Is this even possible, given the architecture?"

**Yes, and the architecture was already built for it** — most of the parts
exist. What is missing is only the file dialog and two buttons.

Already there:

  * `Rack::toJson()` returns the WHOLE rack as one string — modules, their
    parameters, presence, order, mix, the SPECTRA characters and any opaque
    module state such as the KIERANATOR grid. That is the file.
  * `Rack::fromJson()` reads it back, and is deliberately tolerant: unknown
    keys are ignored and missing ones take defaults, which the bench checks
    with a synthetic future blob. So a rack saved by a newer BWFX loads into
    an older one minus the modules it has never heard of, rather than
    failing.
  * the adapter already carries an op that applies a whole blob (`"blob"`,
    added for the built-in presets), and the fragment already has a PRESETS
    row that uses it.

What has to be added, and where:

  * **Nothing in the core.** It stays JUCE-free; file I/O has no business
    there.
  * **The adapter** (`adapter/bwfx_juce.h`) gains saveRackAs and openRack,
    written ONCE so all seven synths get them from the same code — which is
    the whole reason the adapter exists.
  * **The fragment** gains SAVE and LOAD beside the PRESETS row, and two ops
    the adapter answers.

**The design decision that matters: the folder must be SHARED between the
synths, not per-plugin.** A rack is portable — that is the entire promise of
BWFX, and the reason a user rack is worth more than a per-plugin preset. A
rack built in Black Rider should open in Full Metal Racket. So:
`Documents/Brokild/World FX racks`, one folder, extension `.bwfx`, remembered
in a PropertiesFile under Brokild/WorldFX rather than under any one synth.

Two things to get right:

  * **FileChooser lifetime.** `launchAsync` needs the chooser to outlive the
    call, and the adapter is header-only free functions. A function-local
    static would be destroyed by a second instance opening a dialog. Either
    hand the adapter the host's existing `activeChooser` (every synth has
    one already, for its own patches), or keep a small static list in the
    adapter that drops finished ones. The first is tidier and is seven
    one-line edits.
  * **A rack carrying armed CHARACTERS, loaded into a host that does not
    consume the world-mod bus.** Mars Wars and Hairfryer are effects: they
    have no voices to detune. The characters would arm and only their
    PRIVATE audio would be heard. That is already how it behaves and the
    overlay already shows it via busLive, but a loaded rack should say so
    in the notice line rather than leave someone wondering why a character
    sounds thin.

Worth pairing with a **rack blob in the clipboard** — copy and paste as text
is often faster than a file dialog, and it costs one more op.

### 14. BWFX MACROS — automating the rack. SHIPPED in BWFX 1.7.0
Peter 2026-08-28: "is there some way that the BWFX of the synths can be
automatised as well as the regular synth parameters?" — and, after the
options were laid out, "lets go with the macro solution".

**Why there is a gap at all.** Every Brokild synth already exposes every one
of its own parameters to the host (Clone Wars exposes hundreds). The rack is
the exception: it is a string-keyed opaque blob, deliberately NOT host
parameters, and that is what lets a BWFX release add ROTARY, KIERANATOR, LAST
and CHAOS and reach all seven synths by rebuild alone. So the rack is the one
part of these instruments a DAW cannot draw a line on.

#### Two arguments from the first draft that did not survive

Recorded because the conclusion should not rest on them:

  * *"Exposing every rack parameter would break saved projects' automation
    on a BWFX update."* **Wrong.** JUCE derives a VST3 parameter id from its
    string id, so appending parameters leaves existing bindings intact.
  * *"125 entries would be a wall to scroll past."* **Weak.** Peter: Ableton's
    Configure adds only the parameters you actually touch, and other DAWs
    cope with large lists routinely. He also points out that plugins change
    under existing automation all the time and everyone lives with it.

Full exposure is therefore a more reasonable option than the first draft
allowed, and if macros ever prove too small it is the honest next step. It
was not chosen because it is a permanent commitment (an exposed knob is
frozen forever, MkII replacements included), because module ORDER and the
KIERANATOR grid could never be parameters anyway, and because macros are the
small reversible thing that covers the actual use.

A macro is also NOT the same feature as exposure: exposure is access, one
lane per control; a macro is modulation, one gesture moving several things
with depth and polarity. If both ever exist, macros ADD to the parameter
value the way the Mars Wars patch bay adds to a knob, and they compose
cleanly.

---

#### The design

**Five host parameters, `BWFX MACRO 1..5`** — and nothing else — declared once in
`adapter/bwfx_juce.h` by one loop, so every synth gets them and no synth has
per-plugin work. Automatable, saved in APVTS state like any other parameter —
which means a `.fmrkit` or a synth preset picks up the values for free.

**A macro ADDS. It does not set.**

```
effective = clamp( knob + SUM(macro_i * depth_i * (hi - lo)), lo, hi )
```

That is the Mars Wars patch-bay model and it buys three things: the knob
keeps showing the patch's own value and still works by hand; automation on a
macro never fights the knob; and a macro at 0 with nothing assigned is exact
identity, so the empty-rack bit-transparency contract is untouched.

**It applies in exactly ONE place** — `Module::getParam()` returns
`pv[i] + macroOff[i]` — so every module, including ones not yet written, gets
it free. Same pattern as `inputDuck` and `setTempo`. It feeds each module's
existing smoother, so there is no second path (the tempo-sync lesson).

#### Layout

A **MACROS rail across the bottom of the rack window**, between `.bwfx-cols`
and `.bwfx-foot`: full width, eight slim faders, always visible rather than
behind a mode — they have to be reachable while playing. Each fader carries a
badge with the number of things it moves; hovering lists them.

#### Assigning — one gesture, no menus

  * click a macro's name to **arm** it: it lights, and every assignable
    control in the rack picks up a thin ring in that macro's colour;
  * **click a knob** -> assigned at +100 % depth;
  * **drag its ring** -> depth, -100 % to +100 %;
  * **click it again** -> unassigned;
  * **Esc, or the macro name again** -> done.

Assignable: every module parameter, every module's PRESENCE (the most useful
target of the lot), and the rack MIX.

#### Storage — the split that keeps this small

| | Lives in | Why |
|---|---|---|
| macro **values** | host parameters | that IS the feature; and they land in patches for free |
| macro **assignments** | the rack blob, one `"m"` key | they are rack structure: they travel with a patch, with a saved rack, and across synths |

Nothing is owned twice. That is what stops this being a real layer of
complexity rather than a small one, and it is the trap that has cost time in
this fleet before (Photo-Synth's SPECTRA snapshot restore).

Two consequences to build in deliberately:

  * **morph leaves assignments alone** — they are structure, not values. Same
    rule as the Full Metal Racket morph fix;
  * **stepped parameters CAN be assigned** (a macro flipping ECHO's sync
    division is genuinely useful); the sum rounds. This is not a
    contradiction of the morph decision: there, stepping happened to
    everything automatically; here it is one deliberate act per assignment.

#### The count is FIVE, and it is frozen the day it ships

Peter, 2026-08-28: **"four should be enough"**, then **"can the fifth macro
be defaulted to wet/dry, and then possible to change?"** — so FIVE, the fifth
pre-assigned rather than special-cased. Settled.

I had argued for eight; four is the better call. A macro carries several
destinations with independent depth and polarity, so four is not four moves
— it is four *gestures*, and a patch rarely has more than a couple worth
automating. Four also keeps the permanent cost of this feature to five lanes
in every synth's parameter list rather than nine, forever. The count can
never grow later without changing the list, which is the one thing the whole
design exists to avoid, so it is worth having been asked and answered before
a line is written.

Host parameter NAMES are fixed at construction ("BWFX MACRO 1") because hosts
cache them — no user renaming. The overlay showing what each one moves is the
useful half of that anyway.

#### MACRO 5 ships assigned to the dry/wet

Peter, 2026-08-28: "can the fifth macro be defaulted to wet/dry, and then
possible to change? I'd like access to a global effects slider from
automation" — and, ruling out what I had misread the first time: **"there
should be no volume or output level — only dry/wet via macro as now"**.

So: five identical macros, and MACRO 5 carries one default assignment to RACK
MIX. Nothing is special-cased — it is an ordinary assignment that happens to
be there out of the box, and clicking it away frees a fifth macro like any
other.

**This is the simplification that matters.** Reaching the dry/wet THROUGH a
macro means MIX never becomes a host parameter, so it **never changes
ownership**: it stays in the blob, `setMix` is untouched, the overlay slider
still writes the rack directly, and the whole two-sources-of-truth question
raised by the earlier draft simply does not arise. The five macros are the
entire new host surface. Everything else about the rack is exactly as it is
today.

There is deliberately **no rack VOLUME and no output level** — I invented one
from a misreading and Peter ruled it out. If a gain trim is ever wanted it is
a new conversation, not a reserved slot.

#### Macros are NEUTRAL AT ZERO

The one contract question, and it is frozen with the parameters: what does a
macro at 0 mean?

**Decided: 0 means the patch exactly as saved.** Every other neutral in this
system works that way — an empty rack, a presence, CHAOS, the world-mod bus —
and a rack at rest should always sound like its patch. The alternative (the
Ableton model, where a macro OWNS its destinations over a range and their
knobs grey out) is coherent, and arguably more familiar, but it means a macro
at rest is a state the patch never described.

The consequence for MACRO 5 is worth stating plainly, because it reads
backwards at first: its default assignment is **MIX at -100 %**. At rest the
rack is at the patch's own mix — "as now" — and pushing the macro UP pulls
the effects OUT. That is a better live gesture than adding effects that were
not in the patch, but it is the opposite direction from what "global effects
slider" suggests, so it is the one thing to look at again before the
parameters are declared. Flipping it is a one-character change to the default
assignment's depth; flipping it AFTER shipping is not, because the meaning of
every saved automation lane would invert.

#### Total new surface

  * 5 parameters, one loop in `bwfx_juce.h`. That is the ENTIRE new host
    surface — nothing else about the rack becomes a parameter;
  * one `"m"` key in the blob for the assignments;
  * one rail in `bwfx-rack.js` plus the arm mode. Every existing control
    stays exactly where it is and keeps its current ownership;
  * `Rack::setMacro(i, v)`, a per-module offset array, and two rack-level
    offsets (MIX and each module's PRESENCE, which are not module params).

Nothing changes if nothing is assigned, and the empty-rack memcmp still
holds — which the bench should assert, alongside: a every macro at 0 is identity, a
macro at 100 with +100 % depth reaches the parameter's top, depth polarity
works both ways, an assignment to a disabled module is inert, and assignments
survive the blob round trip.

### 15. The macro hint line is styled wrong — FIXED 2026-08-30

It looked like a design error because it WAS one, not a styling choice. The
`.bwfx-machint` rule had been spliced into the middle of an unclosed
`.bwfx-dep.neg`, so under CSS nesting it resolved to `.bwfx-dep.neg
.bwfx-machint` and matched nothing at all — the line inherited system-ui at
the browser default 16px beside 9.5px monospace captions. No restyling would
have shown up until the braces were repaired. Now 9.5px mono at .14em like
its neighbours, muted at rest and accent-bright while a macro is armed and
waiting. `test/check-css.js` added: it proves the braces balance and that no
ordinary rule is nested inside another — the check that would have caught
this the day it landed. (Third insertion-anchor bug in this codebase.)
Peter 2026-08-29: "the CLICK A MACRO... sentence in the BWFX macro window
should be styled like the rest of the text. Right now it looks out of place,
capital white letters. Make it brighter or yellow if you want it to pop, but
not larger — it looks like a design error."

He is right and it is mine: I added `.bwfx-machint` with the rack's own
caption idiom (uppercase, wide letter-spacing) but at a size and weight that
reads as a heading rather than as a note. Next to the pedal labels it looks
like something went wrong rather than like an instruction.

The fix is to make it agree with `.bwfx-foot`, which is the overlay's
established voice for a line of guidance at the bottom of the window:

```
.bwfx-foot{padding:8px 18px 2px;font:10px ui-monospace,Menlo,monospace;
           letter-spacing:.14em;color:#4d625e;...}
.bwfx-machint{padding:0 16px 8px;font:9.5px ui-monospace,Menlo,monospace;
              letter-spacing:.13em;color:#4d625e;min-height:13px}   <- today
```

They are already close, so the offence is not the size — it is that the text
is SHOUTED. Two changes:

  * **drop the capitals.** Sentence case, the way the rest of the overlay
    writes prose. The uppercase idiom in this panel belongs to LABELS (RACK
    MIX, PRESETS, MACROS), not to sentences, and using it for a sentence is
    exactly what makes it read as a design error;
  * **pick the colour deliberately.** Peter offers brighter or yellow. The
    overlay already has an accent (`--bwfx-acc`, the host's own colour) and
    a muted caption grey. A third hue would be a new thing to justify, so
    the honest choice is the accent at low strength while a macro is ARMED
    (it is then a live instruction and should pop) and the caption grey when
    it is not (it is then just a reminder). That also uses colour to say
    something true, rather than for emphasis.

Do NOT make it larger — his instruction, and correct: it is the least
important text in the window.

One line of CSS and one of text. Batch it with the next BWFX pass.

### 16. SPECTRA characters must not move the pitch — FIXED 2026-08-30

Measured, there were two mechanisms and they were not equally guilty.

* **pitchSag was the real offender.** Hosts key it to the smoothed GATE, so
  it scoops every note in and droops it out — a single held note included,
  whichever voice it lands on. It is never heard as an effect, only as an
  instrument that will not stay in tune. TAPE shipped 0.315 semitones of it
  and DARK DRONE 0.25; both now default to ZERO, knob retained.
* **DARK DRONE's DRIFT wrote detune as well as filter** — a slow wander of
  the tuning itself, which is exactly the complaint. It now wanders the
  COLOUR only, which is what a drifting drone should be.
* **Kept deliberately:** CLUSTER (hosts fan detuneCents per voice,
  `fan = fmod(vi*0.618+0.5,1)*2-1`, so voice 0 gets exactly 0 — it is an
  ensemble WIDTH, not an offset) and tape WOW (zero-mean, its own knob, and
  a tape emulation without it is not one).
* The two built-in presets that baked a sag lose it (SEANCE, THE SWARM).
  **User racks are untouched:** `Rack::toJson` writes every character
  parameter explicitly, so a saved blob carries its own sag and ignores the
  default — the Kemper rule is satisfied for free.

Measured: tape sag 0.315 -> 0.000 with filterMul still 0.860; dark drone
detune steady 24.0..24.0 c (was wandering) with filterMul still moving
0.997..1.096; tape wow -4.88..+4.77 c, mean -0.074 c. Bench 409 checks — four
re-aimed and a new one stating the rule over EVERY character. DARK DRONE's
subtitle no longer promises a sag it no longer does.
Peter 2026-08-29: "tape seances dull, and dark drones drift, changes the pitch -
that is rarely helpful. Please see if you can avoid the FX changing the pitch,
unless its a pitch shifter as such."
The offenders are on the world-mod bus:
- TAPE SEANCE `pitchSag` (keyed to the gate envelope) audibly bends held/dying
  notes flat — the PS2 heritage behaviour, but on the bus it lands on every
  synth's voices.
- DARK DRONE's drift writes `detuneCents` per walk step; hosts fan det across
  voices (golden-angle), but the walk's DC component still reads as the note
  slowly wandering off pitch. (The PS2 rule already on file: un-fanned det
  MOVES THE NOTE.)
Spec:
- `detuneCents` on the bus becomes strictly zero-mean: characters emit only a
  SPREAD (fanned symmetrically, centre voice pinned to 0 — the fineCents
  lesson from PS2 .16), never an offset. Dark Drone's drift then modulates
  spread width + filterMul only.
- `pitchSag` default becomes 0 for TAPE SEANCE; its wobble survives as the
  wow (already slewed) which is symmetric around true pitch. If a character
  wants sag, it must be an explicit knob the user raised, and the tooltip
  says it bends pitch.
- Bench: for every character armed at defaults, render a held note and assert
  the fundamental stays within ±3 cents of the unarmed engine (Goertzel).
- Kemper rule: old blobs that saved a nonzero SAG keep their sound; only the
  DEFAULTS and the drift law change. Verify with a saved-blob round trip.

### 17. Tempo changes wreck the synced effects — SPEC, awaiting go

Peter 2026-08-30: "when changing DAW tempo, a lot of the synced effects do
not follow. If i start at 120 bpm all sound good when adding echoes,
kieranator, stutters — but changing tempo in daw just creates a mess."

Reproduced offline (`test/tempoprobe.cpp`, a fake host whose ppq advances at
the current tempo, exactly as JUCE's playhead does). There are **four**
separate faults, and the loudest one is not the one the wording suggests.

#### 17a. ECHO splices on EVERY tempo change (the audible one)

The timing is not the problem: measured across 120->90, 120->150 and 120->121
the echo lands on the new grid immediately and exactly (0.6667 / 0.4000 /
0.4959 s against a 1/4 note). What is wrong is the transition.

| change | worst sample step, settled | around the change |
|---|---|---|
| 120 -> 90 | 0.0229 | **1.4674** (64x) |
| 120 -> 150 | 0.0229 | **1.5195** (66x) |
| 120 -> 121 | 0.0229 | **0.1638** (7x) |

Even a one-BPM nudge is an audible tick. Cause, in `bwfx_modules.cpp:248-263`:
the snap test is `abs(ms - lastSyncMs) > lastSyncMs * 0.5f` — a 50 % change in
delay TIME. Since time is inversely proportional to tempo, that only fires
below old/1.5 or above 2x, so every ordinary tempo move falls under it and
GLIDES instead. And the glide is not smooth: the time is evaluated once per
32-sample sub-block and held constant across it, so each sub-block teleports
the read pointer by ~115 samples inside a 32-sample window — a hard splice
every sub-block for the whole glide, fed back through the feedback path.
Snapping (the other branch) splices once instead. Both are wrong.

**Fix:** a crossfaded tap. When the synced time changes, fade the old read
position out and the new one in over ~20-30 ms instead of moving one pointer.
Clickless AND on the new grid at once, so the Black Rider lesson ("gliding in
lands the first echo early") is honoured without the splice.

#### 17b. Nothing but the KIERANATOR knows where the BAR is

This is the one that matches "do not follow". Grepping the module file:
```
1235:    void setClock (double ppq, bool playing) override { ... }   // KIERANATOR
```
That is the ONLY override in the whole rack. ECHO, GATE and HARMONIC receive
a BPM and nothing else, so they match the tempo's PERIOD but their PHASE is
wherever it happened to land — they are free-running LFOs whose rate is
tempo-derived, not bar-locked oscillators. That is exactly why it "sounds
good at 120": you set it up by ear at one tempo, and after any change the
phase relationship to the bar is arbitrary. The bench never noticed because
it only ever measures the gate PERIOD, never its phase against the bar
(`bench.cpp:477-479`).

**Fix:** the rate-based modules already receive `setClock`; they should use it,
deriving phase from ppq when a host clock exists and free-running only when
it does not.

#### 17c. Seven of nine hosts never send the bar at all

Only Full Metal Racket and Artefact B2311.22 call `bwfxRack.setTransport`.
Photo-Synth 2, Escape Room, Blade Ruiner, Black Rider, Hairfryer and Clone
Wars call `setBpm` only, so `ppqIn` stays at its init -1 and the KIERANATOR
takes its free-run branch permanently — it is never bar-locked in any of
them. And the two hosts that DO call it pass `ppq ? *ppq : 0.0`, telling the
rack "you are exactly at bar 0" every block when the host has no position,
where the contract says <0 means unknown.

#### 17d. Three smaller hazards found on the way

* **A stale BPM is held forever.** Every host nests the call inside
  `if (auto bpm = pos->getBpm())`, so a block with no tempo simply does not
  call `setBpm` and the rack keeps the old value. There is no "no clock any
  more" path back to FREE.
* **An unguarded 0 BPM causes two discontinuities.** Five hosts pass the
  host's value with no range check; `Rack::setBpm` clamps out-of-range to 0,
  which drops the rack to FREE for that block, fires ECHO's hard snap to the
  KNOB value, then snaps back the next block. Some hosts do report 0 while
  relocating. Black Rider and FMR already guard (20..999); the rest should.
* **`EchoDelay::reset()` does not clear `lastSyncMs`** (only `prepare()` does),
  so a module toggled off across a tempo change returns with a stale value
  and glides from the old delay time.

#### 17e. KIERANATOR: the grid is right, the voices are not

Measured, the step grid does follow (0.125 s steps at 120 BPM, 0.167 at 90,
0.100 at 150 — all correct). But `stepLen`/`repLen` are recomputed every
sub-block while a firing voice's counters (`Voice::t`, `Voice::start`) are raw
sample counts captured at the step boundary. So mid-step: RETRIG's
`pos = v.t % repLen` jumps, TAPESTOP re-derives its whole closed-form ramp
against a new `stepLen`, SHUFFLE's `shuffleBack` was computed from the OLD
one and now disagrees, and PITCH's `span` changes under an `fmod`. Slowing
down can also reach further back than the 10 s capture ring and read stale
audio. A firing voice must keep the geometry it started with.

#### Why this survived

**No test in the repo ever changes tempo mid-render.** Every sync test sets a
BPM once and renders (`bench.cpp` :433, :458, :489, :650, :1509, :1522). The fix
must add tempo-change coverage: timing after the change, worst sample step
across it, and phase against the bar for every synced module.

Probe kept at `test/tempoprobe.cpp` (target `tempoprobe`).
