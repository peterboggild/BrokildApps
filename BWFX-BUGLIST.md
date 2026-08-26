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

### 1. FEATURE: host-tempo sync for ECHO and GATE (Peter, 2026-08-26)
Both modules gain two choice params, defaults = legacy sound:
- `sync`: FREE | 2/1 | 1/1 | 1/2 | 1/4 | 1/8 | 1/16 | 1/32 (default FREE)
- `feel`: STRAIGHT | TRIPLET | DOTTED (default STRAIGHT; T = x2/3, D = x1.5)
When sync != FREE, ECHO's TIME and GATE's RATE follow the division against
the host clock (ECHO: seconds per division, clamped into the 1.5 s buffer —
2/1 at slow tempos must clamp, not wrap; GATE: division -> Hz).
Engineering notes (the Black Rider lessons apply verbatim):
- The rack does not receive the host clock yet: add `Rack::setBpm(double)`
  (atomic, read at sub-block rate) and one `bwfxRack.setBpm(...)` line per
  host adapter from its playhead (five of seven already fetch BPM for their
  own sync; Escape Room and Hairfryer need the three playhead lines added).
- A synced delay must initialise its time-smoother ON the synced value when
  sync engages or the tempo jumps — otherwise the first echo lands early
  mid-glide. Tempo CHANGES glide through the existing smoother (tape-style
  pitch bend on the tail, as the tape character should).
- Bench: Goertzel proof that a synced echo lands on the grid at a known BPM,
  a tempo-step render stays inside the click bound, FREE renders
  bit-identical to today's module (the Kemper rule, proven not assumed).

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

## Done

(nothing yet — the founding rollout itself is logged in BWFX-HANDOVER.md)
