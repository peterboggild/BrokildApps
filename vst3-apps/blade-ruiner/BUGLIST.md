# Blade Ruiner — bug list (batch fixes)

Bugs, fixes and ideas collect HERE instead of forcing a build each. When Peter
says go, the open items ship in one pass, one build. Move fixed items to DONE
with the build number.

## Open

### 1. MIDI ROUTING: keys to DECKARD, REPLICANT, both, or split (Peter, 2026-10-24) — awaiting go

Peter: "a mode allowing me to channel midi key signals to either Deckard,
Replicant or both. That would override the robotic autoplay of Replicant, in
case midi is channeled to replicant. Additionally, a split function allowing
midi i.e. from C4 and up to go to replicant (with an octave switch) and H3 and
below to go to deckard, where the split key is selectable."

**How it works today (read from Engine.cpp):** every note goes to DECKARD
(`Engine::noteOn`, 8-voice poly). REPLICANT never plays a note: it runs its
16-step pattern, and with GATE = KEYED the held notes only start/stop it and
transpose it by `lowestHeld - 48`. LOS ANGELES (KEYED) follows the lowest held
note. One `held[]` list serves all three.

**Spec:**
- New param **KEYS** (stepped): `DECKARD` | `REPLICANT` | `BOTH` | `SPLIT`.
  Default `DECKARD` = today's behaviour exactly, including REPLICANT's KEYED
  transpose (bench: memcmp against the current engine).
- **REPLICANT played**: when it receives keys, the sequencer stops and each
  note-on fires the FM voice at that pitch (same timbre path: ratio, index,
  crush, stutter, band-pass, pan). REPLICANT is one voice, so it is MONO,
  last-note priority; releasing back to a still-held note retriggers it.
  Its envelope is a one-shot (sustain 0), so a held key plucks and decays.
  **HOLD** switch (sustain while the key is down), **OFF by default**
  (Peter, 2026-10-24). Velocity -> level (and a little FM index).
  **Autoplay is OFF whenever KEYS routes to REPLICANT** (REPLICANT, BOTH,
  SPLIT) — with no key down it is silent, it does not fall back to the
  pattern (Peter, 2026-10-24). The heartbeat is off too. Only KEYS = DECKARD
  runs the pattern, exactly as today.
- **BOTH**: every note to DECKARD and REPLICANT (REPLICANT takes the most
  recent one, being mono).
- **SPLIT**: notes >= SPLIT KEY go to REPLICANT, below to DECKARD.
  - **SPLIT KEY** (note number, default C4 = 60, so B3/H3 and below is
    DECKARD). Settable by a LEARN button: press it, play a key.
  - **REPLICANT OCTAVE** -2..+2 for the upper zone (the existing OCTAVE knob
    already shifts REPLICANT; reuse it rather than add a second one, if it
    reads clearly on the panel).
  - A note-off must go to the layer its note-on went to, even if the split
    key moved in between (remember the zone per held note), or a note hangs.
- **LOS ANGELES** follows the DECKARD zone's lowest note in SPLIT (the drone
  is the bass of the chord), all notes otherwise.
- The "at least one layer always playing" rule is untouched.
- Panel: a four-position switch + split-key readout (note name, e.g. `C4`)
  + LEARN, near the keyboard; the on-screen keyboard shows the split point.
  Note names in English (`B3`) — fine by Peter; the split key is selectable
  anyway (value + LEARN).

**Bench:** default bit-identical; REPLICANT played lands on the asked pitch
(Goertzel); no step advances while played; SPLIT sends 59 to DECKARD only and
60 to REPLICANT only; a note-off after the split key moves still releases;
mono last-note priority; no pattern step fires in REPLICANT/BOTH/SPLIT with
no key down; HOLD off plucks, HOLD on sustains while held.

**Effort:** engine + params + panel + bench, about half a day. No DSP risk;
the risk is hanging notes, which the bench covers.

## Done

(nothing yet)
