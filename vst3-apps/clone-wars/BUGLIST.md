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

## Done

(nothing yet — fixes land here with their build number)
