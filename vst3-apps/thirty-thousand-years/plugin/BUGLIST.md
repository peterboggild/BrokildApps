# Thirty Thousand Years — the list

Collected, not built. Nothing here is started until Peter says go for a batch.

---

## 1. HISTORY should be able to restart when the keys are released and pressed again — AWAITING GO

**Peter, 2026-09-23:** *"so if i lift up the keys, and press down, shouldn't the
history start over? or maybe have that as an option in a meaningful way? i
think, as an option, it would make sense."*

He is right that it should be an option rather than the behaviour. HISTORY is
currently a machine that runs once you start it and keeps its position until
something moves it; for a played instrument, the journey belonging to the
*phrase* is at least as musical, and for a drone the present behaviour is the
right one. So it is a choice, not a change.

**The engine already computes what it needs.** `Engine.cpp` line 402 builds
`gateAny` every block — true while any voice is gated — and hands it to
`life.tick`. A restart is the RISING EDGE of that signal after silence, which
costs one remembered bool.

Shape it as a new control beside DRIVE, not as a mode of it, because it is
orthogonal to MANUAL/AUTO/AUTO SYNC:

    ON NOTE     HOLD        the journey keeps its position (today's behaviour, the default)
                REWIND      a first note after silence sends it back to the start
                REWIND+RUN  the same, and starts it travelling even if DRIVE is MANUAL

Three things to get right, each of which would otherwise be a complaint:

* **A chord is one gesture, not four.** Notes land a few milliseconds apart, so
  the edge needs a short window — the same idea as Black Rider's 80 ms strum
  window — or a four-note chord rewinds four times.
* **A release gap needs a floor.** Legato playing releases and re-presses
  within milliseconds; a rewind on every one of those is unusable. A settable
  gap, defaulting to something like 250 ms, decides what counts as silence.
* **It must not fight DRONE.** In DRONE mode there is no gate to speak of, so
  the option should read as inert there rather than silently doing nothing —
  and the HISTORY title bar already reports its own state, so it can say so.

Cheap to verify with what is already there: the live probe in
`test/auto3-jobs.json` samples the engine position against the slider, so the
check is a note-on, a wait, a note-off, a second note-on, and an assertion that
the position went back to zero (and, in the HOLD case, that it did not).

---

## 2. The landing page's hero is half empty above about 1200 px — cosmetic

The right-hand side of the hero is blank at desktop width. Every other page in
the fleet has the same shape, so this is a house question rather than a fault
of this one; the front page solved it with a live catalogue readout.
