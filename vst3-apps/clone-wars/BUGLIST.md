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

## Done

(nothing yet — fixes land here with their build number)
