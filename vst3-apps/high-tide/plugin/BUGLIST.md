# HIGH TIDE — the list

Collected ideas, fixes and features. Nothing here is built until Peter says go
for a batch (the standing rule, 2026-08-26). Each entry is a spec, not a wish.

## Awaiting go

### 3. One decal still to fix: `ht-nameplate` (order sent 2026-09-04)
`BrokildApps/assets/high-tide-decals/REDO.md` now asks for that one part only.
Both deliveries drew the plate as a thin bar while the lettering kept its full
size, so the plate's own bottom edge cuts through the capitals — a composition
fault no crop can undo. The hook is ready: `--decal-nameplate` on `#plate`,
plus the two rules that hide the drawn name (kept in the patch script
`test/patch-decals-fix.js` if they need putting back).

`ht-knob` is perfect and has NO home: this panel has no rotary control, and
the one round thing it has — the TUNE LOCK switch dot — uses its colour to say
whether it is on, which a brass cap would hide. Keep it for a future revision.
`ht-cover` is a moonlit panorama band rather than a cover; it would suit a
header strip on the manual's first page, which nothing needs today.

## Shipped

- **260904.2 — hints on everything, and seventeen starters** (Peter, 2026-09-04,
  "please go do it").
  - **Hints.** 89 entries covering all 46 parameters, the 10 tools, the 5
    stamps, every header and rail button, all 7 timeline lanes and the terrain
    view. Each says what the control is, how it is meant to be used, and (for a
    parameter) its value against its real default. The popup appears after
    350 ms of hovering and is placed BESIDE its control — right of the left
    rail, left of the right rail, below the header, above the timeline — so it
    never covers the control or the pointer. A HINTS button in the header turns
    them off, remembered per machine. The panel probe fails if any control is
    missing one, and checks the popup does not overlap what it explains.
  - **Starters.** A STARTERS group ahead of the TERRAINS: SOFT KEYS, SUB,
    PLUCK BASS, WIDE BASS, SQUARE BASS, SAW LEAD, SQUARE LEAD, SWEEP LEAD,
    GLIDE LEAD, ORGAN, ELECTRIC PIANO, BELL, CLAV, SLOW PAD, MORPH PAD, GLASS,
    DRONE. A fresh instance opens on SOFT KEYS. Measured: every one within
    **3 cents** of the note at a brushed key and a hammered one, and within
    **7 dB** of each other.
  - **DETUNE**, a new parameter, because a unison without it is only thickness.
    Each ball's clock runs a little apart, fanned by the golden angle so the
    ball that plays the note has a fan of exactly zero and is never detuned.
    Zero is bit-identical to no detune at all.
  - Bench 283 checks ALL CLEAR; panel probe 14/14 with 89 hints.

- **260904.4 — three more decals, rescued rather than re-ordered.** The redo
  came back letterboxed exactly as before, but the DRAWINGS were good this
  time (the ground's luminance variation went 1 % -> 8 % of full scale), so the
  ingest takes them from this side: **ground** and **paper** are strips
  mirrored top-to-bottom into tiles that repeat without a seam — the
  chart-table surface and the paper under the timeline — and the **rose** is
  the top half of a circular rose, used rising out of the corner of the terrain
  view where only its upper quadrant is in frame. Dark ink on a dark panel is
  invisible, so the rose is filled THROUGH ITS OWN ALPHA with pale brass
  (`tintStencil`). Eleven of twelve parts are now in.

- **260904.3 — six decals into the panel.** The pearl IS the ball; the scope
  became a round instrument in a brass bezel; every point on the timeline is a
  brass chart tack and the release marker a red pennant; the gauges are glass
  tubes (the part is drawn standing and the gauges lie down, so the ingest
  turns it a quarter turn); two oak rails under the header and over the
  timeline. Each is applied only once its image has LOADED, so the panel
  without them is exactly the panel that shipped. 524 KB in the binary,
  served from BinaryData at `/decals/`.

- 260904.1 — the instrument (see `BrokildApps/HIGH-TIDE-DESIGN.md` §12).
