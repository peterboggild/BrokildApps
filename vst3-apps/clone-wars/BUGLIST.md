# Clone Wars — bug list (batch fixes)

Peter's standing workflow: bugs collect HERE instead of forcing a build each.
When Peter says go, fix the open items in one pass, one build, one push.
Add findings under each item while investigating; move fixed items to DONE
with the build number that fixed them.

## Open

(nothing)

## Done

### 4. BWFX - Brokild World FX rack (first citizen) - SHIPPED in 260826.1
The BWFX repo exists (BrokildWorldFX/ at the repo root + canonical checkout
at C:\Users\peter\b\BrokildWorldFX): six founding modules extracted from
Photo-Synth 2 (TUBE/SWEEP/ENSEMBLE/GATE/ECHO/SPACE, own partitioned FFT
convolver), descriptor-driven rack + overlay UI, bench 203 checks ALL CLEAR.
Clone Wars carries it behind the one teal globe button; empty rack proven
bit-identical in cwtest; verified live over CDP. SPECTRA characters and the
other five synths are the next BWFX passes (see BWFX-HANDOVER.md).

### 1. Footage glide - FIXED in 260825.17
Footage changes now jump the glide state by the footMult ratio: the octave
is instant, a genuine note-glide in progress survives. The cross-channel
report (muted clone kicking glide on the soloed one) was ENTRAIN: a muted
neighbour still runs its oscillator and phase-pulls the audible clone, so
its footage GLIDE was audible through the pull; with footage instant the
symptom is gone (the pull itself is by design).

### 2. Phantom release note - FIXED in 260825.17
Sticky seats: redistribution happens only when a note ARRIVES (or the note
mode changes); a pure release just gates the vanished note's clones off at
their own pitch. UNISON keeps mono last-note re-aim by design. New bench
scenario proves the release tail holds C3+C4 with the worst in-between
partial 37 dB down (it used to BE the phantom).

### 3. Shift-drag marquee - SHIPPED in 260825.17
Rubber-band on empty deck: sweeps join their row's lock group (per-row
groups), ctrl-sweep removes, rail lamps refresh, zoom-safe coordinates.
Headless: one sweep locked 60 controls.
