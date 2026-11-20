# DrumEQ — the acoustic drum channel

Peter's brief (2026-11-20): EQ and compression for each mic of an **acoustic** rock/metal kit — Kick In, Kick Out,
Snare Top, Snare Bottom, Tom 1–3, Hi-hat, Ride, Room, Overheads — each with its own presets and controls, following
common mixing advice. Lightweight. "Quality has to be spectacular." His decisions: **one plug-in with a SOURCE switch**,
**kit linking**, **the kit can be saved** ("once I'm happy with the settings for our drummer's kit, I'd like to save
that"), **Brokild collection**.

Source `vst3-apps/drumeq/plugin/`, built to `C:\Users\peter\b\_build\DrumEQ\{plugin,bench}`. Plugin code `DrEq`,
build id `DE_BUILD_ID` in CMakeLists (bump + re-run `cmake -S . -B`). Native JUCE panel in the drum family's
machine-art parts. Patches `Documents\Brokild patches\DrumEQ\<DRUM>\*.json`, kits `...\DrumEQ\Kits\*.drumkit`.

## Why one plug-in, not eleven and not one-with-eleven-slots

Each mic is its own track. One plug-in processing eleven tracks would need eleven inputs; Ableton gives a plug-in one
stereo input and one stereo sidechain. Eleven plug-ins would be eleven installs for the same engine. So: one plug-in
per track, the SOURCE switch re-labels and re-ranges the bands (BEATER 1.5–9 kHz on a kick, CRACK 1.8–9 kHz on a
snare…) and swaps the presets — and **the kit is shared memory between the instances**, which is what makes them one
instrument.

## The chain (zero latency)

TRIM → POLARITY → GATE (keyed band-pass, hysteresis 4 dB, hold, no look-ahead) → HPF/LPF (off / 12 / 24 dB) →
4 bands (band 1 and 4 shelf or bell) → COMPRESSOR (feed-forward, soft knee, key HPF — fully left = OFF, auto release,
parallel MIX) → TRIM. Every switch crossfades along an S over 10 ms; every knob is smoothed 20 ms. **Neutral is
bit-exact** (bench memcmp, and again through the wrapper).

## The filters — what "spectacular" bought, and the measured traps

Bilinear (cookbook) EQs cramp near Nyquist: a +6 dB bell at 12 kHz at 44.1 kHz is 2.72 dB off its analog curve.
DrumEQ designs above fs/32 with **matched poles** (impulse invariance) and a numerator that matches |H| at DC, Nyquist
and the centre exactly (Vicanek), falling back to a weighted least-squares fit only where the exact match would ask for
a negative |H|²; zeros reflected to minimum phase. Same bell: **0.49 dB**. Measured (bench §1): typical bands ≤ 0.83 dB
at 44.1, 0.78 at 48, 0.25 at 96 kHz; ±18 dB extremes ≤ 1.56 dB; every section stable over 5 rates × the knob range.

Traps, each found by measurement:
* Matched design loses double precision for narrow bands at LOW frequencies (|N|² near DC) → below fs/32 the cookbook
  (exact there).
* A bell's CUT has overdamped poles that impulse invariance places badly near Nyquist (17 dB error) → a cut is the
  inverted BOOST. A high shelf's boost / low shelf's cut put poles above the corner → designed as the opposite and
  inverted.
* The positivity test of the three-point fit: `4·B2·x(1−x)` has x² coefficient −4·B2, so the quadratic is CONVEX when
  B2 < 0. I had the sign backwards and invalid fits went through.
* High-pass: always cookbook. Low-pass: matched up to 0.4 fs, cookbook above (its skirt is past 20 kHz anyway).

## The kit (`src/KitLink.h`)

`Local\BrokildDrumEQKit`, the Proxima-site mechanism cut down: an instance slot (id, drum, heartbeat, stale after
2.5 s) and a DRUM slot per source (seqlock with CAS — two OH tracks cannot tear it) holding the last-changed settings and
preset name, plus a kit GENERATION. Runs on a processor-side 10 Hz timer (works with editors closed).
* **SAVE KIT** (any track) writes every drum a track is on now, plus whatever the last-loaded kit brought.
* **LOAD KIT** writes each drum into its slot stamped with a new generation; every LINKED instance takes its own drum
  if the load carried it. The loading track always takes it.
* A track switched to a drum the kit already has joins the kit's settings; otherwise the drum's STANDARD.
* LINK off: the track ignores kit loads (still publishes, so it is saved).
* An instance that just applied a kit records those values as already published, or it would republish and wipe the
  slot's generation before the other tracks read it.

## Presets (`engine/de_sources.cpp`)

Text (`id=value …`) over each drum's STANDARD, five per drum (55): Kick In STANDARD / ROCK PUNCH / METAL CLICK /
EXTREME CLICK / VINTAGE THUD; snares, toms (shared, each tom keeps its own BODY/BOX), hats, ride, room, OH likewise.
Six factory kits: STANDARD, CLASSIC ROCK, MODERN METAL, EXTREME METAL, VINTAGE ROCK, NATURAL. The bench parses every
one, checks every band sits inside its knob and that no preset equals STANDARD.

## FIND DRUM (`engine/de_find.cpp`)

Captures up to 6 s of the input; energy onsets (5 ms frames, +9 dB over the recent floor, 60 ms apart); a Hann window
from 12 ms after each stroke; magnitude spectra averaged (each stroke normalised); strongest peak in the drum's range,
parabolic in log magnitude. Moves the BODY/THUMP/SUB band there. Bench: kick 52 → 52.1, floor tom 82 → 82.2, rack tom
147 → 147.3, snare 196 → 196.5 Hz (strokes start 4 % sharp and fall back).

## FIND sets the levels (261120.2, BUGLIST 2)

Peter: most drum tracks are recorded quiet, and not in proportion to each other. Chasing the THRESHOLDS down to a quiet
track leaves them at the bottom of their knobs and every preset meaningless, so FIND sets the **INPUT** instead: the
median own hit peaks at **-12 dBFS** (`kTargetPeakDb`) on every track, and the presets' thresholds then mean what they
say. `measureLevels()` (de_find) reads the capture the way the engine does — broadband peak for the trim, the gate's own
detector (key band-pass, 10 ms peak) for the gate — and tells the drum's hits from spill (own = within 12 dB of the
loudest key-band hit). The gate threshold goes between the loudest spill and the quietest hit, its 4 dB hysteresis clear
of the spill. IN widened to -24..+36 dB, OUT to -36..+24. **KEEP TRACK LEVEL**: OUT gives back what IN took, so a mix
already balanced on the faders does not move. Unpitched drums (hats, ride, room, OH) get it as **SET LEVEL**.
Measured: a snare at x0.01 and x0.5 (34 dB apart) trims +27.9 / -6.1 dB and comes out at -9.0 dBFS both times.

## MASTER (261120.2, BUGLIST 1)

Peter: one panel to tune the whole kit instead of eleven. Any LINKED track can be MASTER (one per session; claiming
takes the role). The panel turns blue; the source row picks the drum to EDIT, HOME marks the track's own drum, which it
keeps playing. Editing another drum works on a stand-in processor (`ProxyTag`: parameters and presets only, no kit, no
timer, no audio) that the editor binds its knobs to; the master's 30 Hz tick pushes its changes as EDIT writes and pulls
the slot back when a track or a kit load changed it. Every linked track on that drum takes the edit **into its own
parameters**, so its project state holds it and removing the master changes nothing. The target track shows the master
what it hears (192 log bins + meters; with two tracks on one drum the louder shows) and answers FIND remotely. A drum
with no track can be set up and is saved in the kit. The master's own drum's changes are EDIT writes too, so a second
track on it follows. Block is v2 under a new name (`Local\BrokildDrumEQKit2`). Caveats: host automation on a follower's
parameter overrides the master there; the changes reach the host as the plug-in's own (undo steps, automation if armed).

## Gates

`detest` (bench, 113 checks), `dehost` (wrapper, kit, FIND levels, MASTER across four instances, 51), `deshot` (renders
the panel normal and as MASTER: 97 components, none overlapping or cut, the master's display shows the other track's
sound). Cost: eleven instances, everything on, 3 % of one core.

## Not done (2026-11-20)

Published 2026-11-21 as 261120.2: landing page `vst3-apps/drumeq/`, 8-page manual (`plugin/docs/manual/manual.html`,
gate `plugin/tools/shoot-manual.js`, plates from `deshot`'s renders via `plugin/tools/tojpg.ps1`), zip cut and checked
by `plugin/tools/make-zip.ps1` (release asset), and the fourteenth member of the Brokild Collection. Kick In/Out phase
alignment (needs a delay; polarity only for now). Synthetic test drums only — the presets want a listen on real
multitracks. Ideas waiting for a go: `vst3-apps/drumeq/BUGLIST.md` (LISTEN ALL, an ALL target, auto EQ steps).
