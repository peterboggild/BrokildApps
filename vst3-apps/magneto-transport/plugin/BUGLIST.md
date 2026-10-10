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
