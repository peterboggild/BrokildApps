# Legion — buglist / wishlist

Collect, don't implement: items wait here until Peter says go.

---

## 1. LEVELLER — a two-way vocal compressor — SHIPPED 260924.1 (2026-09-24)

Built as specced below: `engine/legion_leveller.{h,cpp}`, bench section 17 (static curve within 0.01 dB, steady-note gain modulation -151 dB, OFF bit-identical, noise under FLOOR lifted 0.00 dB, +35 dB step overshoot 0.40 dB at 44.1/48/96 k with TIGHT latency, 23 dB phrases -> 11.7 dB, 0.11 % of a core). One change from the spec: look-ahead window fixed at 10 ms rather than set by SPEED, because a moving ramp length would move the gain alignment; SPEED is the release (800 -> 60 ms).

Peter: "a vocal compressor that can also lift silent parts … works both from
the top and the bottom … not exactly a gain rider … if the extra voices are
not turned on, the signal path should be super clean — this compressor should
not colour the sound."

**Shape.** One gain computer, three regions on the input level:
- above TOP: downward compression (ratio);
- between FLOOR and TOP: untouched (or gently pulled toward the band);
- between FLOOR and the band's lower edge: upward compression (lift);
- below FLOOR: no lift at all, so breaths, room and bleed between phrases
  are NOT pumped up. This floor is what separates a useful upward compressor
  from a noise amplifier.
Controls kept few: TOP, LIFT (how far quiet parts come up), FLOOR, SPEED
(one knob driving attack/release, release program-dependent), plus a
gain-reduction/lift meter. Max lift capped (e.g. +12 dB).

**Where it sits (the part that makes it clean).** Detector reads the
UNDELAYED input; the gain is applied to the dry and harmony buses AFTER the
engine, where both are already delayed by the reported latency (42.7 ms at
NATURAL). So it gets a full-latency lookahead for free — no added latency,
no overshoot, no need for a clipper/ceiling. Same gain curve on both buses,
so the choir is levelled with the singer. Sidechain HPF (~100 Hz) and a
de-plosive tilt live in the DETECTOR only, never in the audio.

**"Does not colour."** Gain multiply only: no saturation, no audio-path
filter, stereo-linked, gain smoothed so it never modulates inside a cycle
(attack floor ~2 ms, no instantaneous peak tracking). Off = an IEEE-exact
multiply by 1.0 → bench memcmp against a build without it (Kemper rule).

**Bench, before it counts as done:** off is bit-identical; static in/out
curve matches the three regions to 0.1 dB; a sine through it at steady level
has THD at the float floor; a phrase with silent gaps shows the gap noise
lifted by ≤ 0.5 dB (FLOOR works); a step input shows no overshoot (lookahead
works); legionhost sees the new params; legionshot shows the panel fits.

## 2. Voices off is NOT a clean path — FIXED 260924.1 (2026-09-24)

With no voice on, dry -> 1 and wet -> 0 (20 ms ramp). legionhost proves the output equals the delayed input to the bit at MIX 50 and 100.

With every voice switched off and MIX at its default 50 %, the output is the
dry delayed signal × cos(45°) = **−3.0 dB**. The harmony bus is silent, so
MIX then acts as a volume knob on the singer. Proposal: when no voice is on,
ramp dry gain to 1 and wet to 0 (20 ms), so "voices off" means delay line ×
OUTPUT trim and nothing else. Note the BWFX rack on MASTER still processes
the signal — that is intended, but the "clean" claim holds only with the
rack empty or on HARMONY. Changes the level of existing projects that run
Legion with all voices off, so Peter's call.
