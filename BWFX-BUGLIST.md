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

### 2. SPECTRA characters — the plan (design decision 7; planned 2026-08-26)
The overlay's right half is a placeholder; the world-mod bus
{detuneCents, panSpread, tremolo, pitchSag, filterMul} exists and is
neutral. What ports and what does not: a PS2 SPECTRA = a host-control
MACRO (snapshots/rewrites PS2's own knobs — CANNOT port, stays PS2-native)
+ a LIVE MODULATOR (per-tick det/pan/trem/sag + sometimes own FX — ports
completely via the bus). A ported character sounds like THAT synth
possessed, by design. Three phases:
- **A. Machinery + Tape + Insect + Clone Wars mapping (1-2 sessions).**
  Character registry (descriptors, enclosures + rockers from PS2's CSS,
  arm/STACK semantics: arm order layers, last-armed wins, release
  unwinds), combination rules AS MEASURED in PS2 (det/pan add, muls
  multiply, trem element-wise product, sag/cluster max), worldMod() goes
  live, PRESENCE doubles as arm strength (morph then covers characters
  for free). First host mapping = Clone Wars: five bus inputs onto the 16
  clones — touches cw_core, so it lands WITH Goertzel bench checks (the
  6000-check bench stays green or it does not ship).
- **B. Remaining characters (a session).** Dark Drone, Pink, Black,
  Glass — characters owning FX (Black's feedback/comb, Glass's colours)
  carry that DSP privately inside their unit. All documented PS2 lessons
  apply: voice-0 detune rule, sag keyed to GATE not amp env, spread must
  not change the note, never noise inside a loop.
- **C. The other six host mappings (hours each).** Five bus inputs per
  voice engine + bench additions per synth. PS2 LAST, with an open
  question flagged for Peter: it would then carry two spectra systems
  (native panel + world rack) — whether the native panel retires is a
  separate per-synth decision, like native-FX retirement.

### 3. Manuals do not mention BWFX yet
All seven PDF manuals predate the rack (deliberately skipped in the
2026-08-26 rollout, Peter's instruction). One shared "The World Rack"
section could be written once and dropped into each manual's pipeline;
landing-page prose could carry one feature line each.

### 4. FEATURE: founding set 2 — Peter's selection (2026-08-26)
STATUS: four of five SHIPPED in BWFX 1.2.0 (LOFI/GRIT, STRIP, SHIMMER,
HARMONIC TREM/VIBRATO). **ROTARY is the one still open** — deliberately
left for a Fable session: it is the only from-scratch DSP in the set and
the feel of the inertia is taste plus measurement, not recipe.
Five new modules, chosen by Peter from the survey. Capacity: order packing
holds 16 modules; 6 + 5 = 11, fine. ONE machinery prerequisite: a comp+EQ
module needs ~10 params, so raise bwfx kMaxParams (8 -> 16). Safe: the
state blob is keyed by id, arrays just get roomier — prove old-blob
round-trip in the bench anyway.
- **LOFI** — port PS2's Lofi processor (crush / noise / dirt), the module
  deliberately left out of the founding six. Code exists; needed.
- **STRIP (comp + 5-band EQ, one pedal)** — Hairfryer's two-stage
  compressor plus its RBJ EQ grown to five bands, as a single channel-
  strip module. Suggested params: comp AMOUNT (macro over both stages),
  ATTACK, RELEASE, then five fixed musical bands gain-only +-12 dB
  (low shelf ~80, 250, 1k, 3.5k, high shelf ~10k). EQ post-comp.
- **SHIMMER** — Blade Ruiner's FDN-8 with the octave-up folded into the
  loop: the reverb a convolver cannot be, complements SPACE. Params:
  mix, size, decay, shimmer amount, tone. Mind the runaway lesson
  (ceilSoft in the loop, 60 s max-settings soak in the bench).
- **HARMONIC TREM / VIBRATO** — one modulation pedal, mode-switched:
  HARMONIC (brownface style: split ~800 Hz, low and high band tremolo'd
  in OPPOSITE phase — the swirl), TREM (plain), VIBRATO (true pitch via
  a modulated line, Catmull-Rom as always). Params: mode, rate, depth,
  mix. Must ride the item-1 sync system when that lands.
- **ROTARY** — a solid Leslie: horn + drum split ~800 Hz, each rotor its
  own AM + doppler FM (modulated delay) + stereo mic pair; SLOW / FAST /
  BRAKE control where the point is the INERTIA — independent ramp times
  per rotor (horn ~1 s, drum ~4-5 s) so speed CHANGES sound like a
  Leslie spinning up. The one genuinely new DSP build in the set — give
  it a bench that measures doppler depth and the two ramp times.
Surveyed and parked by Peter ("not now" — do not re-propose, they are
here for the record): FLANGER, plain TREMOLO/AUTO-PAN, standalone TONE
EQ, FILTER pedal (Black Rider circuits), FREEZE, RING MOD, micro-pitch/
octaver, granular, and these stay available whenever wanted.

### 5. FEATURE: GLITCHER — step-sequenced rhythmic effect switching
(Peter, 2026-08-26; inspired by dblue's Glitch 2 at illformed.com — our
own build from scratch, concept only, no code or UI copied. "It was
phenomenal": draw a pattern of different effects across a bar or two,
tune each effect, get rhythmic, glitchy but musical results.)
SELF-CONTAINED module (Peter's call — do not route the rack's own pedals
per step): a rolling capture buffer of the last 2 bars; every effect is a
way of READING it, so CPU is trivially light (no FFT, no formants — the
tape-style repitch grit is desired):
- 16-step pattern over 1 or 2 bars; per step one effect type (3 bits):
  NONE, RETRIG (buffer repeat, division + decay), TAPESTOP (pitch drop
  across the step — explicitly wanted), REVERSE (bar buffer backwards),
  SHUFFLE (jump to a random earlier slice), PITCH (fixed repitch read:
  octaves/fifth up/down), CRUSH (LOFI-lite in place), GATE (duty chop).
- Per-effect tuning knobs (1-2 macros each) + global: pattern LENGTH
  (1|2 bars), MIX. All reads Catmull-Rom; 5 ms crossfades at every slice
  edge (glitchy, never clicky). Free-runs on an internal clock when the
  host transport is stopped.
MACHINERY it needs (each piece useful beyond this module):
- Transport feed: `Rack::setTransport(ppq, bpm, playing)` — bar-aligned
  patterns need position, not just tempo. SUPERSEDES the setBpm half of
  item 1; build once, both items ride it.
- Opaque per-module extra state: a drawn pattern does not fit the knob
  descriptors — add a module `extra` blob (string, rides inside the
  module's entry in the rack JSON; unknown-key rules apply as ever).
- The fragment's FIRST custom pedal editor: a drawable step grid
  (effect per step, drag to paint), shipping inside bwfx-rack.js so it
  versions with the library. Descriptor-generated knobs still render
  below it for the per-effect tuning.
Bench: retrig period lands on the grid at a known BPM (Goertzel/period
measure), TAPESTOP measurably drops pitch across a step, slice edges stay
inside the click bound, pattern renders deterministic, silence in/out,
bounded. Estimate ~2 sessions including the machinery.

## Done

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
