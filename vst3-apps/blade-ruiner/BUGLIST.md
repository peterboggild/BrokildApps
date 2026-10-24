# Blade Ruiner — bug list (batch fixes)

Bugs, fixes and ideas collect HERE instead of forcing a build each. When Peter
says go, the open items ship in one pass, one build. Move fixed items to DONE
with the build number.

## Open

(nothing open)

## Done

### 1. MIDI ROUTING: keys to DECKARD, REPLICANT, both, or split — DONE in 261024.2 (Peter, 2026-10-24)

Peter: "a mode allowing me to channel midi key signals to either Deckard,
Replicant or both. That would override the robotic autoplay of Replicant, in
case midi is channeled to replicant. Additionally, a split function allowing
midi i.e. from C4 and up to go to replicant (with an octave switch) and H3 and
below to go to deckard, where the split key is selectable." Then: HOLD off by
default, autoplay off whenever keys go to REPLICANT, B3-style names fine.

**What shipped:**
- Footer, beside the keyboard: **KEYS TO** `DECKARD | REPLICANT | BOTH | SPLIT`
  (host param `keys`, default DECKARD), **SPLIT** stepper with the note name
  (param `split`, default C4 = 60: B3 and below to DECKARD, C4 and up to
  REPLICANT; dimmed unless SPLIT) and **LEARN** (the next key pressed, MIDI or
  the panel keyboard, becomes the split key and is not played; the panel says
  where the split landed). The panel keyboard tints the REPLICANT keys in SPLIT.
- REPLICANT page, LAYER group: **PLAYED** switch `PLUCK | HOLD` (param
  `rphold`, default PLUCK). CLOCK dims while REPLICANT is played, PLAYED dims
  while it is not. The existing OCTAVE stepper shifts played notes too.
- Whenever KEYS sends keys to REPLICANT (REPLICANT, BOTH, SPLIT) its pattern
  and heartbeat do not run, keys down or not. Each key fires the FM voice at
  its pitch through the same path (ratio, index, band-pass, crush, stutter,
  pan); velocity sets the level. One voice: the newest key sounds, letting go
  returns to the newest key still down.
- Each key remembers where its note-on went, so its note-off goes the same way
  even if KEYS or SPLIT KEY moved meanwhile. LOS ANGELES follows the DECKARD
  hand in SPLIT, every key otherwise.
- The panel's own keyboard now reaches the engine through a lock-free queue
  drained on the audio thread (it used to call the engine from the UI thread,
  a race that predates this item).

**Verified:** `brtest` ALL CLEAR with 12 new KEYS checks (autoplay silent
when played; A4 lands on 440 Hz; OCTAVE +1 on 880; PLUCK dies while held,
HOLD sustains and lets go; mono newest-key and fall-back; SPLIT B3 -> DECKARD
only, C4 -> REPLICANT only; BOTH plays both; note-off after the split key
moved still ends the note). `paramcheck.js` ALL CLEAR (50/50). KEYS = DECKARD
is **bit-identical** to the 261024.1 engine: memcmp in 3 scenarios (plain,
L.A. + REPLICANT KEYED, KEYED + OCTAVE + glide) x 44.1/48/96 kHz, all three
layers on, chords, overlaps and an all-notes-off. Panel: no JS errors, no
overflow, footer and LAYER group measured in headless Chrome.

**Not done:** the manual and the landing page do not describe KEYS yet.
