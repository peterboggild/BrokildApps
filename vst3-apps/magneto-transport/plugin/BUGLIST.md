# Magneto Transport — buglist / ideas

Collected, not built, until Peter says go (house rule).

---

## 1. HEAD PATTERN — heads 2 and 3 snap to synced positions (Peter, 2026-10-10) — awaiting go

*"is there a way to get head 2 and 3 to swap to relevant positions for different
standard synced positions, from which they can be moved or nudged?"*

- A **HEAD PATTERN** selector + SET button beside SYNC. Each pattern is a set of
  ratios to the TEMPO HEAD's distance: 1:2:3 (Space Echo), 1:1.5:2 (dotted),
  1:1.333:2 (triplet feel), 1:2:4 (doubling), 1:1.5:3, 1:3:4, golden 1:1.618:2.618.
  With SYNC on, every head then lands on a note value.
- **A one-shot snap, not a lock**: it writes the three position controls once and
  lets go, so a knob nudge or a drag on the rail just works afterwards. (A lock
  would be two owners for one value — the BWFX macro lesson.)
- The carriages **travel** there with their real inertia and top speed, so a
  pattern change is audible as a Doppler glide of every echo in flight.
- Heads cannot pass each other, but they are identical, so a pattern assigns its
  positions to the heads **in rail order**; the tempo head keeps its own position
  and the other two are placed around it.
- If a pattern does not fit the rail (1.5–24 cm) at the current tempo head
  distance, the panel says which head is out of reach rather than silently
  clamping it.
- Bench: after SET, each head's delay equals its ratio × the tempo head's delay
  within 1 us once the carriages settle; a nudge afterwards moves only that head.

## 2. LOOPER — a third transport mode (Peter, 2026-10-10) — APPROVED, built in stage 7

*"is there a way to make a third mode, which is a tape looper mode? If you can find
a consistent, realistic and excellent way of doing it you have more than my blessing"*

The consistent way is that a tape looper IS the closed loop of FLUX MEMORY with
the record path turned into a performance control. Nothing in it is a buffer: the
loop is the tape, layers are magnetisation, and every transport action does what
the machine would.

- **TRANSPORT = REEL | LOOP | LOOPER.**
- **REC**: record + bias on, erase head at full - the first pass writes fresh tape.
- **OVERDUB**: record + bias on, erase head OFF (or at FLUX MEMORY's partial level).
  Old layers survive, but AC bias partially erases what it records over, so each
  pass the older layers fade by however much the bias physically takes off -
  measured in the stage 2 reference, not a feedback knob.
- **PLAY**: record AND bias off, erase off - the loop plays unchanged forever
  (wear only, if enabled). This is Freeze.
- **STOP / START**: the capstan motor really stops and starts, so the loop dives
  and rises in pitch with the motor's inertia (stage 5) - the tape-stop is physics.
- **CLEAR**: one full pass of the erase head.
- **Loop length in bars**: with SYNC on the loop is cut so its period is N bars at
  the current tape speed (0.5-6 m); out of range, the panel says so.
- The playback heads still read the loop, so the echo, feedback and Doppler all
  work ON TOP of the loop.
- **No undo**: tape has none. Said plainly in the manual rather than faked.
- Relay clunks on every transport button when the mechanical sounds arrive (stage 10).
- Bench: an overdub layer's decay per pass equals the reference model's bias-erasure
  figure; PLAY holds a loop bit-exact for 10 minutes with wear off; STOP's pitch
  dive follows the motor's measured spin-down.
