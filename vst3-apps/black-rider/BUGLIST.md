# Black Rider — bug list (batch fixes)

Peter's standing workflow: bugs, fixes and ideas collect HERE instead of
forcing a build each. When Peter says go, the open items ship in one pass,
one build. Add findings under each item while investigating; move fixed
items to DONE with the build number. World-rack (BWFX) items go on
BWFX-BUGLIST.md at the repo root instead.

## Open

### 1. LADDER filter seems to lower the volume (Peter, by ear, 2026-08-26)
Plausible on physics alone — a 4-pole 24 dB/oct lowpass eats far more top
end than the 2-pole K35 circuits at the same CUT, and a real transistor
ladder loses passband level as resonance rises (the bass-thinning noted in
the engine comments is the same mechanism). But "correct" is not the same
as "right": Clone Wars had this exact complaint ("any army on LADDER made
everything go quiet") and its fix landed LADDER within 0.7-2.3 dB of GROWL.
Diagnose by measurement, not by ear-matching: a levelprobe in test/
(house pattern: cw ladderprobe.cpp) rendering the same note through
GROWL / SCREAM / LADDER at matched CUT and PEAK, RMS per model across
the CUT range and across PEAK 0..1. If LADDER sits systematically low:
- resonance-compensated makeup (gain rising with k, the modern-ladder
  trick) and/or a fixed level trim at res 0, tuned so LADDER lands within
  ~1-2 dB of GROWL across the playable range;
- mind the tuning: the existing x0.9965 trim is a TUNING prewarp artefact,
  not a level control - do not conflate them;
- seeds are a promise: a level change alters every seed that uses LADDER.
  Precedent is Peter's own ruling on the Clone Wars filter pass ("much
  better that the filters sound good and right than the existing presets
  are the same") - but confirm before shipping, or remap seed levels.

## Done

(nothing yet)
