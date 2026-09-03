# ARTEFACT B2311.104 — buglist / awaiting go

House rule: items are collected here and built when Peter says go, in batches.

## 1. THE TEMPERATURE LAW IS A BOWL AND MUST BECOME A SLOPE (Peter, 2026-09-03)

**His reports, verbatim where it matters:**
- "the middle temperatures are the calmest ones, whereas often lowering the
  temperature makes the sound more wild and aggressive — that does not make so
  much sense" (true of the opening/default sound).
- "it just seems to mirror around 77 K … going further down corresponds to
  making everything more noise and alive. Temperature slider should go just to
  77 K, and not 're-heat' for low temperatures."
- Below ~350 K the sound should turn slightly LESS dissonant, align more to the
  MIDI notes (more playable), modulations should calm down, and notes should
  STOP when input is removed. Right now a played tone "seems to activate some
  kind of drone mode, a self-supplying state."

**Diagnosis (exact, from Engine.cpp):**
- `eAuto = max(0, |ln(T/T_amb)|/θ_on − 1)` — the auto-song disequilibrium is an
  ABSOLUTE log-ratio, so it is symmetric about ambient: a conduit far COLDER
  than its surroundings sings exactly as one far hotter does. That symmetry is
  the mirror Peter found.
- A commanded note's own servo heat feeds back into that same term: order a
  pitch whose target temperature is far from ambient and the note carries
  itself past onset — the "self-supplying state". After release the heat leaks
  over seconds, and while the ratio stays past onset the conduit drones on.
- At the default 234 K most targets are NEAR ambient → calm. At either extreme
  of the huge 2.7–1216 K range every target is FAR from ambient → wild. Hence
  the bowl with its calm floor in the middle.
- Nothing scales TURBULENCE / STIFFNESS / the hunt with ambient, so cold gets
  the full chaos machinery on top.

**The fix (one axis: cold = order, hot = disorder — which is literally
thermodynamics, and matches .1 "cold it does not count" and .67 "at 77 K the
effect is absent"):**
1. Range becomes **77–800 K like the siblings**, default stays 234 K. (The
   sqrt-T passive retune then spans ×0.57–×1.85 instead of ×0.11–×2.28, which
   also shrinks every commanded thermal move.)
2. Define `warmth` from ambient (0 at 77 K → 1 at 800 K, musically shaped so
   ~350 K is still restrained). Scale by it, monotonically:
   - `eAuto` (auto-song / drone / traffic audibility) ∝ warmth² — cold, a
     released note STOPS; hot, the grid carries on its own.
   - effective TURBULENCE ∝ (small + warmth) — cold is clean even with the
     knob up; the knob's full range lives in the heat.
   - STIFFNESS mode gain ∝ (0.3 + 0.7·warmth) — less dissonant below ~350 K.
   - hunt/discipline wobble and the sag ∝ warmth — cold obeys the MIDI note.
3. **A conduit under order must not count its own servo heat as thermal
   excess.** While commanded, excess is measured against the ORDER's target,
   not against ambient; only orphaned heat (traffic, conduction, released
   notes) can auto-sing — and only scaled by warmth. This kills the
   self-supplying loop at every temperature.
4. Bench: a MONOTONICITY sweep (post-release sustain, spectral flatness,
   detune spread must be non-decreasing in ambient), "cold stops" (note off at
   150 K → −40 dB within a fixed time), "hot drones" (same phrase at 700 K
   sings on), and the existing clean-regime tuning checks unchanged.

**Status: awaiting go.**

## 2. THE SITE LAYER (cross-artefact coupling) — see PROXIMA-SITE-DESIGN.md

Global climate, the site pulse, and the DISTANCE control, shared by all four
findings. Specced separately because it spans the collection. Awaiting go.
