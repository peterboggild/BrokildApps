# 1984 — the plates

Every picture the handbook and the landing page reference, what it has to
show, and the panel state to set before shooting it.

Shoot from the **live standalone** over CDP (`tools/live.ps1` starts it with a
debugging port; `tools/cdp.js` drives it), then convert with `tools/tojpg.ps1`.
Composite onto the page colour **`#08090c`** — JPEG has no alpha, and the
handbook's sheets are that near-black, so a plate on any other ground shows a
seam at its edge.

**Before shooting anything:** load the patch named in the row and let the
display settle on its name. A plate of a panel mid-edit, with a value readout
where the patch name should be, reads as a mistake rather than as a detail.

---

## The manual — `docs/manual/img/`

Every slot in `manual.html` has a **fixed height in millimetres** and the image
is `object-fit: contain`, so nothing is ever cropped or stretched and a plate
that has not been shot yet still holds its page open. Shooting close to the
stated aspect avoids letterboxing inside the box.

| file | page | box | aspect to aim for | what it must show | state to set |
|---|---|---|---|---|---|
| `cover.jpg` | 1 (cover) | full bleed, lower 58 % of the sheet, cropped `center 55%` | **wide, 3:1 or wider** | The deck at an angle or straight on, filling the frame. It is banded across the lower page with a fade over it, so the top third of the shot will be dimmed — put the sliders, the display and the tape window in the **middle band**, not at the top. | `VANGELIS CHOIR`, tape **ON**, a chord held so the voice lamps are lit and the display is drawing envelopes. |
| `panel.jpg` | 2 | full width, **54 mm** | **~5:1** (the whole deck) | The entire panel, straight on, nothing cropped: both rank rows, the display, the chain row along the bottom, the tape window. This is the establishing shot and the only place the reader sees the whole instrument. | `CS STRINGS`, tape **ON**, one chord held. Ensemble and hall visibly up. |
| `rank.jpg` | 4 | 38 % column, **44 mm** | **~4:3** | **One rank strip only** — rank I, from the wave sliders through tuning, HPF, LPF, both envelopes, to level and pan. Close enough to read the silkscreen. | `BLADE BRASS`. No note held; the sliders are the subject. |
| `modrow.jpg` | 7 | 38 % column, **44 mm** | **~4:3** | The modulation row: sub-oscillator (shape, rate, delay, phase), the ring modulator, poly-mod, with the touch and wheel depths under them. | `POLY-MOD SWEEP` — its poly-mod depths are up, so the row is not a picture of five controls at zero. |
| `chain.jpg` | 10 | 38 % column, **40 mm** | **~3:2** | The chain row left to right: DRIVE, ENSEMBLE, CHOIR, TAPE, HALL. All five groups in one frame. | `VANGELIS CHOIR` — choir at 90 %, CATHEDRAL ensemble, an 8 s hall, so every group has something set. |
| `tape.jpg` | 11 | 38 % column, **36 mm** | **~3:2** | The tape window with the **reels turning**, plus the wow/flutter needle off centre. A still frame of stopped reels is the wrong picture. | `VHS MEMORY`, a chord held, tape mode **VHS**. Shoot while a note is sounding so the needle has moved. |
| `vfd.jpg` | 14 | 38 % column, **28 mm** | **~4:1** (a wide strip) | The amber display alone, cropped to the glass: the patch number, the patch name, the category, the envelope traces and the eight voice lamps. | `HEAVEN AND HELL`, a six-note chord held so six of the eight lamps are lit. |
| `display.jpg` | 14 | 38 % column, **40 mm** | **~3:2** | The display **and the patch controls around it** — the dial, SAVE / OPEN / RANDOM, the browser. Wider than `vfd.jpg`; this one is about the controls, that one about the glass. | `TUBULAR DREAM` on the dial, nothing held, so the display is showing a patch name rather than a value. |
| `bwfx.jpg` | 15 | 38 % column, **48 mm** | **~3:4 (taller than wide)** | The Brokild World FX overlay open: the FX column with **two or three modules armed and in a visible order**, the SPECTRA column with one character armed, the five macros along the foot, and the rack mix in the header. | Open the rack with the globe, arm **ECHO** and **SPACE**, arm **TAPE SEANCE** in the SPECTRA column. **Open, wait a beat, then click** — `BWFX.open()` sends an init and the native state echo re-renders the whole list, so clicks made in the same tick are wiped. |

## The landing page — `BrokildApps/vst3-apps/1984/img/`

Three files, and the page will render without them (each is in a fixed-ratio
box with a dark ground) but it looks unfinished.

| file | where | aspect | what it must show | state to set |
|---|---|---|---|---|
| `panel.jpg` | hero, full width | **~5:1** | The whole deck. Can be the same shot as the manual's `panel.jpg`. | `CS STRINGS`, tape on, a chord held. |
| `rank.jpg` | "The two ranks", half width | **~3:2** | **Both rank rows together**, so the two-ranks-per-voice claim is visible — not the single strip the manual uses. | `BLADE BRASS`. Rank II's FINE visibly off centre if the panel shows it. |
| `chain.jpg` | "The chain", half width | **~3:2** | The chain row, same framing as the manual's. | `VHS MEMORY`, tape mode VHS, a chord held. |

## Also needed for the release, not shot here

| file | where | size | what |
|---|---|---|---|
| `assets/app-previews/1984.jpg` | the front-page card | **1200 wide**, ~16:10 | A crop of the hero that reads at card size: the display, one rank row and the tape window. `app.json` already points at it. |

---

---

## What the delivered plates measure, and two that want a second pass

Shot 2026-09-22. Every slot in `manual.html` is now sized to its plate's own
aspect, so nothing is letterboxed. **If a plate is re-shot at a different
shape, the slot's `.h**` / `.w**` classes have to move with it** — the numbers
are paired in the stylesheet with a comment saying so.

| file | measured | aspect | slot |
|---|---|---|---|
| `cover.jpg` | 2400 x 1470 | 1.63 | full-bleed band, `object-fit: cover` |
| `panel.jpg` | 2400 x 1470 | 1.63 | c-42 column, 68 mm high |
| `rank.jpg` | 2400 x 279 | 8.60 | 206 x 24 mm, centred |
| `modrow.jpg` | 2400 x 192 | 12.50 | 212 x 17 mm, centred |
| `chain.jpg` | 2400 x 192 | 12.50 | 212 x 17 mm, centred |
| `vfd.jpg` | 2292 x 324 | 7.07 | 198 x 28 mm, centred |
| `display.jpg` | 2400 x 174 | 13.79 | full width, 19 mm high |
| `tape.jpg` | 528 x 352 | 1.50 | 41 x 27 mm, centred |
| `bwfx-wide.jpg` | 2136 x 1372 | 1.56 | c-46 column, 78 mm high |
| `bwfx.jpg` | 1040 x 1372 | 0.76 | not used by the manual |

**`tape.jpg` wants re-shooting.** Its own `TAPE` nameplate is clipped off the
top edge and a sliver of the control to its right bleeds into the frame, so
the plate reads as a bad crop rather than as the tape window. Re-crop with the
whole nameplate in and the neighbour out; keep it near 3:2 and the slot needs
no change.

**`bwfx-wide.jpg` stops above the macro row**, so the caption on page 15 does
not mention the macros. If the plate is re-shot tall enough to include them,
say so in the caption — the manual's only claim about that picture should be
what is in it.

## Shooting notes

- **The overlay-arming trap.** Setting the panel up and shooting in the same
  CDP job produces a rack with nothing armed, and it looks like a UI bug. Split
  it: open → wait → click → wait → shoot.
- **A jobs.json setup expression cannot contain unescaped double quotes** —
  use unquoted attribute selectors.
- **Kill `msedgewebview2` before relaunching** with a debugging port. WebView2
  joins a running browser process for the same user-data folder and ignores the
  new arguments; the panel then comes back blank.
- **Check the shot before converting.** A plate is the one thing in this
  document no measurement can check.
