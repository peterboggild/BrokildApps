# Magneto Transport manual - plates

Every plate is referenced from `manual.html` as `img/<name>.jpg` (so it goes in
`plugin/docs/manual/img/`). The manual draws each plate in a FIXED box with
`object-fit: contain` on the panel's ink, so a plate of a slightly different aspect
letterboxes instead of moving the layout; the box size is given per plate so the shot
can be framed to fill it. Shoot at 2x (the deck is 1600 x 900 deck units), convert
to JPEG q88 as the fleet does, and shoot from the LIVE standalone over CDP.

Setup notation: parameter ids and physical values from `Source/Params.h`
(`mode=1 looper=1 erase=30` means set those parameters, everything else default).
Start every plate from a fresh instance (defaults; the "Studio Slap" preset is NOT
the default) unless a preset is named. Selectors: the header is `#head`, the machine
is `canvas#machine`, the whole deck is `#deck`, a plate is the `.grp` whose `h3`
text is the plate name (the loop plate also has `id="loopGrp"`), a card is `#card`,
the preset list is `#presetMenu`.

Where a plate must show signal (tape colour, VU needles, the TAPE meter, the W&F
reading, head lamps), feed the standalone a sustained programme (a looping WAV or
the standalone's input) for at least ten seconds first, so the tape between the
heads carries a recording and the W&F meter has left SETTLING. Cards must be
closed in every shot except the two card plates (move the pointer off the deck).

| # | file | box in manual | what it must show | setup |
|---|---|---|---|---|
| 1 | `machine-reel.jpg` | 128.5 x 42.4 mm (3.03:1); also the cover, cropped to 2.4:1 by `object-fit: cover` | The machine in REEL: both reels with their packs, erase, record and the three heads on the rail, the capstan, the tape coloured by a recording. Keep the centre of interest in the middle third (the cover crops the sides). | defaults, then `h2on=1 h3on=1` so three heads are lit; programme playing; crop `canvas#machine` |
| 2 | `panel.jpg` | 164 x 94 mm (16:9) | The whole deck as a fresh instance opens, with signal: VU needles up, tape coloured, W&F reading a number. | defaults; programme playing; crop `#deck` |
| 3 | `header.jpg` | 265 x 10.6 mm (25:1) | The header strip: wordmark, speed window (19.00 cm/s, 7.48 ips), lamps, BYPASS, both VUs, TAPE meter, preset window, gear, build 261011.11. | defaults; programme playing; crop `#head` |
| 4 | `machine-loop.jpg` | 128.5 x 42.4 mm (3.03:1) | The machine in LOOP: the loop tank instead of reels, the loop running past the heads, the tape coloured by what is on it. | `mode=1 looplen=2 looper=1 erase=30`; record a phrase for one loop period (about 10.5 s at 19 cm/s) first, then shoot while it plays; crop `canvas#machine` |
| 5 | `rec.jpg` | 100.7 x 63 mm (1.59:1) | The RECORD plate: INPUT DRIVE, RECORD LEVEL, BIAS, then EQUALISATION and INPUT. | defaults; crop the `.grp` with h3 `RECORD` |
| 6 | `transport.jpg` | 165 x 55 mm (3.0:1) | The TRANSPORT plate with SYNC on: TAPE SPEED, SYNC showing 1/4, MOTOR RUN, REEL/LOOP, TEMPO HEAD, HEAD PATTERN 1:2:3 with SET and "FITS THE RAIL". | `sync=7 synchead=0 h1pos=4` (host tempo 120 BPM in the standalone if it has one; otherwise SYNC with whatever tempo it reports, and say so in the caption check); crop the `.grp` with h3 `TRANSPORT` |
| 7 | `loopbars.jpg` | 128.5 x 45.2 mm (2.84:1) | The LOOP plate with LOOP BARS asking for a count that does not fit, and the readout naming the counts that do ("2 bars at 1/4 from 4.0 cm is 0.32 m, outside the 0.5-6 m loop: 4 to 16 bars fit"). | `mode=1 sync=7 synchead=0 h1pos=4 bars=2`; check the readout text says the counts that fit before shooting; crop `#loopGrp` |
| 8 | `heads.jpg` | 150 x 48.4 mm (3.1:1) | PLAYBACK HEADS with all three heads on, each delay window reading its milliseconds (about 211 / 421 / 632 ms at 19 cm/s). | factory preset **Space Echo 1:2:3**; wait until the carriages have settled (delay windows steady); crop the `.grp` with h3 `PLAYBACK HEADS` |
| 9 | `feedback.jpg` | 100.7 x 75 mm (1.34:1) | The FEEDBACK plate: FEEDBACK (big knob), BASS, TREBLE. | `feedback=0.45 treble=-3` (the Space Echo setting, so the knobs are not all at rest); crop the `.grp` with h3 `FEEDBACK` |
| 10 | `mechanics.jpg` | 132 x 53.4 mm (2.47:1) | MECHANICS with OK / BROKEN past the middle: red dots on CONDITION, CAPSTAN, REELS, SCRAPE showing where BROKEN acts, the W&F window reading a number, and at least one fault name (CLOG, HUNT or CRINKLE) on the slider. | `broken=70`; programme playing; the faults come and go - poll until the BROKEN slider's text carries a fault name and the W&F window is not "settling", then shoot; crop the `.grp` with h3 `MECHANICS` |
| 11 | `tape.jpg` | 100.7 x 56 mm (1.79:1) | The TAPE plate: PARTICLES, WEAR, ACCUMULATE, and the four-way stock switch on STUDIO. | defaults; crop the `.grp` with h3 `TAPE` |
| 12 | `loop.jpg` | 140 x 49.3 mm (2.84:1) | The LOOP / FLUX MEMORY plate in DUB: LOOP LENGTH 2.00 m, BARS FREE, ERASE 30 %, LOOP STATE on DUB, CLEAR, and the readout "LOOP DUB / recording / erase head x.xx Hc / 2.00 m loop" with the explanation line under it. | `mode=1 looplen=2 looper=1 erase=30`; programme playing; crop `#loopGrp` |
| 13 | `card-dub.jpg` | about 128 x 66 mm, contain | The LOOP STATE card pinned open, scrolled to the top: the plain words, then UNDER THE HOOD with the three facts and the DUB figure. | `mode=1 looper=1`; open card `looper`, pinned (hover the LOOP STATE switch past the hover delay, then click the card); crop `#card` |
| 14 | `output.jpg` | 100.7 x 50 mm (2.01:1) | The OUTPUT plate with REPRO on, its button reading "REPRO 210 ms" (or whatever head 1's delay is). MIX, OUTPUT, MECH SOUNDS, KILL DRY, REPRO. | `repro=1 mix=1 h1pos=4` (speed default 19); crop the `.grp` with h3 `OUTPUT` |
| 15 | `presets.jpg` | about 119 x 52 mm, contain | The preset list open: factory presets in their six groups, with at least one user preset if one can be saved first under a neutral name (for example "My Slap"; never a name from Peter's own folder). | click `#pName`; crop `#presetMenu` |
| 16 | `card-bias.jpg` | about 100.7 x 126 mm, contain | The BIAS card pinned open, scrolled to the top: the plain words, UNDER THE HOOD, the bias-curve figure with its live marker, the measured numbers. | defaults; open card `bias`, pinned; crop `#card`. If the card is taller than its 470 px cap and scrolls, shoot it at the top (the caption only promises the figure and numbers are on it). |

Notes for the shooter:

- The card plates (13, 16) are the only shots with a card open. In every other shot,
  make sure no card has opened (cards open after the hover delay; switch TOOLTIPS off in
  the gear menu while shooting the others, and back on for 13 and 16).
- Plates 1, 2, 3, 4, 10 and 12 carry live values: shoot them after the programme has
  run long enough that nothing reads "--" or "settling".
- `object-fit: contain` means a plate never overflows its box; the manual's overflow
  check (Gravity Well's `tools/shoot-manual.ps1`) still applies to the text.
- The standalone used for shooting picks up Smart App Control nudge bytes in place;
  shoot from a COPY and zip a clean link (the house rule).

## Landing page images

Referenced from `vst3-apps/magneto-transport/index.html` as `img/<name>.jpg`, i.e.
`vst3-apps/magneto-transport/img/`. All are copies of manual plates, re-exported wider:

| file | source | notes |
|---|---|---|
| `hero.jpg` | plate 2 `panel.jpg` (the whole `#deck`, 16:9) | full width of the page (about 950 px); export at 1900 px wide |
| `machine-loop.jpg` | plate 4 | gallery, half width |
| `mechanics.jpg` | plate 10 | gallery, half width |
| `loop.jpg` | plate 12 | gallery, half width |
| `card-bias.jpg` | plate 16 | gallery, half width; it is tall, which is fine in the gallery |

Also needed for the front-page card and the social tags (not an `img/` file):
`assets/app-previews/magneto-transport.jpg`, 1280 x 720, from plate 2 (the
`og:image` tags in `index.html` declare 1280 x 720).
