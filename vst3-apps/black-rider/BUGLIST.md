# Black Rider — bug list (batch fixes)

Peter's standing workflow: bugs, fixes and ideas collect HERE instead of
forcing a build each. When Peter says go, the open items ship in one pass,
one build. Add findings under each item while investigating; move fixed
items to DONE with the build number. World-rack (BWFX) items go on
BWFX-BUGLIST.md at the repo root instead.

## Open

(nothing open)

## Done

### Real legato — FIXED in 261010.1 (Peter, 2026-10-10: "bends on overlapping notes and not on non-overlapping notes")

The envelopes were already legato; GLIDE was not. Pitch was reset only when the voice had
gone fully silent, so a detached note played inside the previous note's RELEASE tail glided
too, and the LEGATO switch did nothing to glide at all - against the manual, which already
promised legato glide. Now, with LEGATO on, a note glides only when a key (or the pedal) is
still holding a voice when it goes down; LEGATO off keeps portamento on every note. POLY
follows the same rule. Bench `[legato]`: mono and unison, overlap 155 Hz (gliding), detached
in a 0.7 release 220.1 Hz (snapped), LEGATO off 155 Hz. Fails on the old engine (155 Hz).

### LADDER filter seems to lower the volume — FIXED in 261010.1

**LADDER sits a mean 12.0 dB below GROWL** across CUT 0.25-0.90 x PEAK 0.0-0.9
(`test/levelprobe.cpp`, target `bklevel`; EG1>CUTOFF, KEY TRACK and LFO>CUTOFF all
pinned to zero, because KEY TRACK's 0.5 default is what wrecked the equivalent Brain Scan
probe). Same picture at FILTER DRIVE 0.0 and at the shipped 0.2.

**Two separate things are in that 12 dB, and only one of them is a fault.**

*Legitimate:* at PEAK 0.00 the deficit is -1.7 dB wide open and -22 dB nearly closed. A
4-pole eats far more than a 2-pole at the same corner. That is the filter being a ladder.

*The fault:* as resonance rises, GROWL and SCREAM get LOUDER and LADDER gets QUIETER.
At CUT 0.64, PEAK 0.00 -> 0.90:

| | PEAK 0.00 | 0.30 | 0.60 | 0.90 |
|---|---|---|---|---|
| GROWL | -15.5 | -14.7 | -14.3 | **-14.0** |
| LADDER | -17.6 | -22.9 | -25.1 | **-26.3** |

GROWL gains 1.5 dB; LADDER loses 8.7 dB. That is the classic uncompensated ladder: the
passband of `G^4 / (1 + k G^4)` falls as `1/(1+k)`, so resonance buys its peak by
taking the whole band down with it. -8.7 dB at the top of the dial is `k` near 4, which
is about right. Raising resonance should not make the instrument quieter.

**The fix is the standard one and it is named in the original entry**: makeup rising with
`k` so the passband holds. It is NOT built, because it is Peter's call:

- It only moves LADDER where PEAK is up, i.e. toward what a player expects.
- But **seeds are a promise** - every one of the 200 that uses LADDER changes level.
- Precedent cuts toward fixing it: Peter's own ruling on the Clone Wars filter pass was
  "much better that the filters sound good and right than the existing presets are the same".

The original entry follows.
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


**Fixed 2026-10-10 (Peter asked again): the output is scaled by (1 + k)**, so the
passband holds at unity like the Sallen-Key models and resonance rises above a level low
end. Chosen by sweep (`b/_build/ladtune`): output compensation 1.0 took the mean deficit
-9.7 -> -1.3 dB; input-side compensation only recovered half (the summing-node tanh eats it).
Bench `[ladder]` at CUT 0.64: LADDER - GROWL -2.7 / -0.2 / +0.1 / +1.0 dB at PEAK
0 / .3 / .6 / .9 (was -2.0 / -8.4 / -11.1 / -12.5). Self-oscillation now ~4 dB above
SCREAM's (it was 11 dB below). What remains near a closed cutoff is the honest part: 24
dB/oct eats more of a note sitting at the corner. Every seed that uses LADDER is louder,
on Peter's Clone Wars ruling quoted above. Landing page, manual and README no longer say
the ladder thins the bass.
