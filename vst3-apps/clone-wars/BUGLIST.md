# Clone Wars — bug list (batch fixes)

Peter's standing workflow: bugs collect HERE instead of forcing a build each.
When Peter says go, fix the open items in one pass, one build, one push.
Add findings under each item while investigating; move fixed items to DONE
with the build number that fixed them.

## Open

### 1. Footage changes glide — they must be instant
Reported 2026-08-26 (build 260825.16).
- Changing FOOTAGE on a clone sweeps to the new octave through GLIDE instead
  of switching instantly. Footage is a register switch, not a performance
  slide; it must never invoke glide.
- Analysis (unverified): the engine has no per-voice "footage changed"
  detection — `footMult` feeds `targetHz` and `vc.freqCurrent` glides toward
  it like any pitch change. Likely fix: track each voice's last footage index
  and, when it changes, scale `freqCurrent` by the footMult ratio instantly
  (keeps any in-progress note glide intact while making the octave jump hard).
- Second observation, needs investigating during the fix: with one clone
  SOLOED, changing footage on a *muted* clone audibly kicks off a glide on
  the soloed clone. A muted clone's footage should be inaudible entirely.
  Suspects: row lock-group membership sending more than the clicked clone;
  or note re-assignment (`assignNotes`) reshuffling when a parameter changes;
  or the UI sending the row to more channels than intended. Reproduce first.

### 2. Phantom note on chord release - redistribution re-aims releasing clones
Reported 2026-08-26 (build 260825.16). DIAGNOSED - the fix is specified.
- Symptom: press two notes, sounds great; release both nearly simultaneously
  and a NEW note rings out in the release which is neither of the two played.
- Mechanism, traced in assignNotes() (cw_core.cpp): the held-note list is
  rebuilt every sub-block, and releasing note A a few milliseconds before
  note B leaves only B in the list - so ALL sixteen clones are redistributed
  onto B while still gated. The eight former-A clones stay gated (no edge, no
  retrigger) but their targetNote jumps to B, so they GLIDE from A toward B
  through the release gap. When B is released moments later, those clones are
  frozen MID-GLIDE (the released-notes-keep-their-pitch rule freezes
  freqCurrent when the gate drops) and their release tail rings at a pitch
  between A and B: the phantom note. Peter hears exactly this.
- Fix (Peter's instinct is the right rule): NEVER redistribute on release.
  Make the assignment sticky: redistribute the sixteen only when a note is
  ADDED to the sounding set (or on a fresh chord from silence); when a note
  leaves, the clones that held it simply gate off and release AT their note,
  and the clones on still-held notes keep exactly what they had. Sketch:
  keep the previous vAssign; if the new note list is a subset of the old
  (pure release), only clear gates of clones whose note vanished; full
  divideClones() redistribution runs on noteOn paths only. Exception: UNISON
  mode should keep its current behaviour (all clones re-aim at the remaining
  bottom note - that is classic mono last-note handling and sounds right).
- Test to add: render press A + B, release A, 50 ms later release B; Goertzel
  the release tail - energy must sit at A and B only, nothing in between.

### 3. FEATURE: shift-drag selection rectangle for lock groups
Requested 2026-08-26.
- Shift-drag starting on empty panel (outside any control) draws a selection
  rectangle; every per-clone control it sweeps joins its row's lock group -
  rubber-band selection of any part of a row (or several rows at once).
- Sketch: pointerdown with shiftKey where e.target is not a control and not
  inside a hatch -> spawn an absolutely-positioned dashed marquee div on the
  panel; on pointermove, resize it and hit-test every [data-row][data-ch]
  control by getBoundingClientRect intersection (live highlight via the
  existing .locked class preview); on pointerup, per ROW touched: add the
  swept controls to lockSet(row), apply .locked, call refreshLockTools().
  A rectangle spanning several rows locks each row as its own group - locks
  stay per-row, consistent with the whole locking model.
- No conflict with existing gestures: shift-click ON a control still toggles
  that control's membership; the marquee only arms from empty deck. Sweep
  over an already-locked control could REMOVE it (toggle semantics) or a
  second modifier could unlock - decide while building, prefer: plain sweep
  adds, sweep with ctrl held removes.

### 4. FEATURE (large): BWFX - Brokild World FX rack (first citizen)
Full design: BWFX-DESIGN.md at the BrokildApps repo root (settled 2026-08-26).
Note per the design: the rack lands ALONGSIDE the native four (additive,
default empty) - not replacing them on day one. And per the 2026-08-26
addendum: the rack is its OWN pop-up overlay behind a new BWFX button in
the shared accent colour - it does NOT go inside the EFFECT RACK hatch.
Requested 2026-08-26.
- Replace the current rack (SPRING / BBD / TAPE / DRIVE) with Photo-Synth 2's
  effects bank: the SAME pedal look and face plates, and the modules
  DRAG-REORDERABLE so the player chooses the chain order.
- Sources to port from (all on this machine):
  - DSP: C:/Users/peter/b/PhotoSynth Source/Engine.cpp - the 7-module FX
    chain (exact C++ port of the browser worklet: delay, chorus, phaser,
    stutter, saturation, convolution reverb, limiter). Mind the PhotoSynth
    lessons already learned there: never prepare Convolution under a
    message-thread impulse load (the irLock story), cubic Catmull-Rom on
    delay/chorus taps, satGain is exponential pre-gain.
  - Face plates: PhotoSynth Source/ui/ui.html pedal markup + CSS (pedal
    enclosures, .spec-rocker rocker-sm power switches, per-pedal skins).
- Chain ORDER must become part of the machine: a permutation stored in
  project state and user patches (NOT an automatable param - a permutation
  fights automation the way Blade Ruiner's mood did). Message {k:"fxorder",
  order:[...]} UI->processor; engine processes modules through an index
  table; crossfade ~30 ms on reorder so dragging never clicks.
- UI: pedals live in the EFFECT RACK hatch (standing rule: future FX go
  there); HTML5 drag or pointer-drag reorder with a gap indicator; the
  global FX MIX knob on the console stays and wraps the whole bank.
- Consequences to handle in the same pass: the factory archive generator and
  all 100 seeds voice the OLD four units (springmix/tapefdbk/bbddepth/
  driveamt...) - map old params onto the new bank or re-voice the archive;
  the offline bench FX assertions; feature-parity with what the seeds
  expect; the manual (already stale) documents the old rack.
- ARCHITECTURE (settled with Peter 2026-08-26): build this as **BrokildFX**,
  a shared SOURCE library (not runtime DLLs - SAC blocks fresh hashes and
  patches must stay a promise): one repo of FX modules, DSP in plain C++ +
  a face-plate convention, compiled INTO every Brokild plugin; changes mean
  rebuilding all synths (CI can automate). Patch policy, Kemper-style:
  a patch stores its rack (modules + order + settings), so a NEW module
  never touches existing patches; every new parameter DEFAULTS to legacy
  behaviour so old patches keep their sound through updates; genuine sound
  improvements and bug fixes flow to all patches; a genuinely different
  algorithm ships as a new mode or a MkII module alongside, never replacing.
- This is the largest open item - do it as its own build, not mixed into
  the small-bug pass.

## Done

(nothing yet — fixes land here with their build number)
