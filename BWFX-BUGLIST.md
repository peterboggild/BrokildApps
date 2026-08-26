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

### 4. IDEA: module wishlist — founding set 2 (surveyed 2026-08-26)
What the six do not cover, ranked by proven-code reuse (the BWFX pattern:
extract, never invent). Capacity note: order packing holds 16 modules,
6 used — Tier 1+2 lands at 15.
- Tier 1 (in-house code, ~afternoon each): LOFI (PS2 Lofi crush/noise/
  dirt — the module deliberately left out of the founding six), FLANGER
  (BBD line + feedback, small delta from ENSEMBLE), TREMOLO/AUTO-PAN
  (smooth sibling of GATE + stereo pan; rides the future sync), COMPRESSOR
  (Hairfryer two-stage, simplified), TONE (Hairfryer 4-band RBJ EQ).
- Tier 2 (distinctive ports): SHIMMER (Blade Ruiner FDN-8 + octave-up in
  the loop — the reverb a convolver cannot be, complements SPACE), FILTER
  (Black Rider GROWL/SCREAM/LADDER + LFO + envelope-follower sweep =
  auto-wah and squelch on tuning-proven ZDF code), FREEZE (infinite hold,
  the drone-fleet pedal), RING MOD (Mars Wars, incl. the no-BIAS lesson).
- Tier 3 (new DSP, real projects): micro-pitch/detune + octaver (needs a
  pitch shifter, none in the fleet), granular texture (kipple as seed),
  rotary speaker.
- Suggested batch if Peter says go: LOFI, FLANGER, TREMOLO, FILTER,
  SHIMMER — closes modulation, tone and character gaps in one pass.

## Done

(nothing yet — the founding rollout itself is logged in BWFX-HANDOVER.md)
