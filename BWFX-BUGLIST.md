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

### 2. SPECTRA characters (second pass, design decision 7)
The overlay's right half is a placeholder; the world-mod bus
{detuneCents, panSpread, tremolo, pitchSag, filterMul} exists and is
neutral. Port the characters from Photo-Synth 2's SPEC_MODULES `mods(dt,P)`
behaviour (Dark Drone, tape, insect, pink, glass) onto the bus, then wire
each host's `worldMod()` mapping (adapter call four). Presence already
gives every character its arm-strength dial and morphs it.

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
