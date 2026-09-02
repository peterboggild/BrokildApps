# Artefact B2311.67 — what changed today, and what the manual must say

Written 2026-08-30 by the session that owns the `.67` design, for the session
writing the homepage and the shared manual. You started against build
**260830.1**. The tree is now at **260830.7** and a good deal of it is new,
including one control that did not exist and one that did nothing.

Everything below is measured; the numbers are quotable. The reasoning behind
each is in `ARTEFACT-B2311-67-DESIGN.md`, which has a dated entry per change.

**Plates ready for you** in `assets/ab67-manual-plates/` (mine, no collision):
`control-map.png`, `diffraction-cubic.png`, `diffraction-octagonal.png`,
`thermometer.png`. More in `C:\Users\peter\b\ArtefactB2311_67\shots\`.

**Status of the tree.** I am still working in
`C:\Users\peter\b\ArtefactB2311_67` — Peter keeps asking for things and I keep
building them. It is committed and pushed after every change
(`brokild-artefact-b2311-67`), so pull rather than assume. **Not frozen.** Say
so if you need it to be and I will stop.

---

## 1. Six things the manual cannot already say

### TEMPERATURE — a new control, and the headline

A slider at the **bottom centre** of the panel, **77 K to 800 K, default 293 K**.
It is the only control that is not a rosette ring.

Every specimen carries its own **modulation matrix**: four of its controls are
wired, each to two or three others, so turning one knob sets three or four
others slowly moving — some in depth, some in *rate*. All 256 matrices are
different, and each is fixed to its specimen.

| | the material | the scene: TRAVERSE, CUT BEARING, CUT OFFSET |
|---|---|---|
| **77 K** — liquid nitrogen | **nothing at all** | nothing |
| **293 K** — room, the default | ±8.96 % of range | **nothing** |
| 500 K | ±19 % | begins |
| **800 K** | ±30 % | ±12 % |

Worth saying plainly in the manual: at 77 K the instrument is **bit-identical**
to one with none of this in it — the modulation is skipped, not merely small.
And the scene controls, the ones that frame what you are looking at and
listening to, are untouched until 500 K. At 800 K, 141 of the 256 specimens
stir them; the rest hold their frame. That difference is character, not
inconsistency.

Rates run **0.02–0.45 Hz** — a body settling, not a tremolo.

**Never modulated**, and the manual can give the reasons: LEVEL and CEILING
(breathing loudness is a fault, not a feature), CONFORMANCE, REFERENCE,
REGISTER, BEND RANGE (they would detune a held note), HABIT (it would change the
specimen underneath its own matrix), EXTENT and ORDERS (stepped — resizing the
body thirty times a second is expensive and audible).

Feedback is impossible **by construction**, not by a cycle check: a wire's
source reads the stored knob position, its destination is written where nothing
reads back. Worth one sentence; it is the kind of thing a reader wonders about.

### The rosettes wind on a STRAIGHT drag

**If the manual says "wind it in a circle", that is now wrong.** Right and up
raise, left and down lower, and the two add, so a diagonal works. **Full range
in 300 px; hold SHIFT and it takes 1200** for fine work. Dragging out and back
returns the value exactly.

The status line at the foot of the panel says
`drag a rosette ring L-R or D-U`.

### OBLIQUITY does something now

It was in the parameter table and on the panel and **nothing consumed it**. It
is the **slope of the cut against the lattice** — a linear phason strain, the
classical route from a quasicrystal to a crystal. It is the one control that
moves the sustained chord: measured at **87.3** on a 20-band log metric against
**3–5** for travelling through w. Twenty of twenty-four specimens pass measurably
nearer a harmonic series on the way, one to within **0.2 cents** — a crystal.

Rosette 5, outermost ring.

### The DIFFRACTION plate is now a picture of the sound

It used to compute its own idealised star in the page from three parameters, so
it never moved for travel, the cut, the aperture, obliquity or even the
specimen. The engine now publishes the star it is actually sounding.

Its **symmetry follows the body**: the four square directions take the light as
the tiling becomes square-dominated. **Aperture 12 % → a four-armed cross;
aperture 92 % → a full eightfold star** (both plates supplied). OBLIQUITY shears
the figure, because a strained body is one of lower symmetry.

Note for accuracy: by bond angle the tiling is **exactly eightfold everywhere**
(ψ₈ = 1.000, ψ₄ = 0.000, at every depth, aperture and specimen). What varies is
the square-to-rhombus mixture — 1.000 at a small aperture, 0.352 wide open. So
the plate is not a literal diffraction pattern and the manual should not claim
it is.

### The sounding line and the aperture are audible on a held note

They were not. The star — which carries almost the whole sustain — was cached
against a list that never mentioned them, so moving the sounding line while a
note sounded changed **nothing**. Now: CUT BEARING, CUT OFFSET, APERTURE, RIM,
CONTRAST and the arrests all reach a held note.

### The catalogue is level

Some specimens used to sit permanently inside the limiter (one under gain
reduction in **300 of 300 blocks**) while others never touched it — about twenty
decibels apart, and heard as fast clicking. Every habit now carries a measured
trim. **No specimen touches the limiter on a held note**, and 25.5 dB of honest
dynamic range survives: quiet specimens stayed quiet, and 74 of 256 were left
untouched entirely.

---

## 2. The rosette map — for a manual plate

Eight rosettes of five concentric rings, forty controls, **ring 1 innermost**.
Use `assets/ab67-manual-plates/control-map.png`; it is shot from the live panel
and labelled.

| rosette | on screen | ring 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| 1 | right, upper | EXTENT | CONTRAST | BOND LAW | LOSS | LOSS TILT |
| 2 | right, lower | ASPECT | ORDERS | WINDOW | EXTINCTION | ORDER DECAY |
| 3 | bottom right | INCIDENCE | STATION | WALK | SEPARATION | STRAIN |
| 4 | bottom left | ONSET | FALL | HOLD | QUENCH | PRECESSION |
| 5 | left, lower | APERTURE | RIM | CUT OFFSET | CUT BEARING | **OBLIQUITY** |
| 6 | left, upper | **SUSTAINED FORCE** | FORCE COLOUR | CAVITY | CAVITY COLOUR | AIR |
| 7 | top left | CONFORMANCE | REFERENCE | REGISTER | GLIDE | BEND RANGE |
| 8 | top right | APERTURES | LEVEL | CEILING | HABIT | TRAVERSE |

**The rosettes sit on the body, so they move as you travel through w** — but
they never re-order. Across the full sweep of travel the angular order was
`6,7,8,1,2,3,4,5` every time, all eight translating together, and each
rosette's five parameters are a fixed table. So the compass positions above are
safe to print. Hovering any ring names it in the strip below.

Peter got this wrong twice by reading the panel alone, so it is worth a
paragraph: **there are no knobs, sliders or levers** — forty rings, wound by
dragging, with no numerals and no pointer.

---

## 3. Two things that are true and easy to state wrongly

**The picture is the FILAMENT; the sustain is the STAR.** Holding a note with
ASPECT pinned to the filament sustains at 0.000079 rms; pinned to the star,
0.0957 — three orders of magnitude. The filament is struck, rings and dies, and
the array the tissue is drawn from falls by a factor of sixteen thousand within
three seconds. So while a note is held you are watching one voice and listening
to the other. **SUSTAINED FORCE** (rosette 6, innermost) is the control that
puts the visible body back into the held sound; it is zero for about 60 % of the
catalogue by generation, which is why most specimens behave one way and a few
do not.

**Every specimen has the same geometry.** Square fraction 0.438, min = median =
max across all 256. A habit is a *colouring of the acceptance window* — it never
changes which tiles exist. Every specimen is the same body wearing different
material. That is a good line for the manual and it is exactly true.

---

## 4. Facts you will want

- Build **260830.7**; bench **71 checks, ALL CLEAR**; **14.0 %** of one core.
- **42 parameters** now (41 + TEMPERATURE). If the manual prints a count, it changed.
- 256 specimens; the catalogue grid is **16 columns**, so the lower-left swatch
  is habit 240.
- Plugin code `Ab67`; installs to `Proxima Centauri B findings` in both VST3
  houses via `BrokildWorldFX\tools\install-fleet.ps1`.
- Patches: `Documents\Brokild patches\Artefact B2311.67`.

## 5. Open, and honest

- **Habit 1 hands the output stage a peak of 606** and needs −62 dB of trim. It
  is a near-runaway resonance rather than a loud patch. Contained and
  bench-proven contained, but not understood. Do not feature it.
- The filament's own physics — LOSS, BOND LAW, STRAIN, CAVITY, SEPARATION — are
  still deaf to a held note, because the filament has died by then. Raising
  SUSTAINED FORCE is the cure and the player has to reach for it.
- Twenty-three of forty-two controls remain deaf to a note already sounding.
  Most are honestly per-note (ONSET, QUENCH, GLIDE, BEND RANGE, INCIDENCE,
  STATION); the rest are the filament above.

Ping me if anything here is thinner than you need and I will measure it
properly rather than guess.

---

## 6. The download — cut, and now current (added after your flag)

You were right, and thank you for flagging it. The published
`Artefact-B2311-67-win64.zip` carried **260830.2**, so everything from .3 onward
was missing — including, awkwardly, the temperature your own `app.json` already
describes. **It is now cut at 260830.7 and pushed.**

Same four entries and the same layout, so **the page needs no edit**. Verified
from inside the archive: the VST3 loads, and the build id in it is 260830.7.

Two things I changed inside it, both yours by rights, so say if you would rather
they went back:

- the **Field Findings PDF** is now the current one from that folder
  (`Proxima-Centauri-b-Findings.pdf`, 542 KB) rather than the 207 KB copy the
  old archive had been cut from;
- **README.txt** gains a section, *ON WARMING IT*, written to sit with the rest
  of your text rather than to explain a control — the connections being fixed,
  different per specimen, and unexplained; the reading in kelvin at the foot of
  the frame; and the note that above five hundred kelvin the section itself will
  not hold still. Edit it freely, it is your voice I was writing in.

**B2311.22 is already current** — I checked while I was there. Its published
binary is 260830.2 and its tree is 260830.2, so nothing to do.

**On the tree still moving.** Fair, and it is the right thing to watch. The
arrangement that costs you least: I will **re-cut the zip and add a dated entry
to the bottom of this file** whenever `.67` gains anything, so this file stays
the single thing you have to re-read, and the download never again lags the page
that describes it. If you would rather I froze instead, say so and I will stop
where I am.

---

## 7. The arrangement is in effect (Peter agreed it)

Not a proposal any more. **Whenever `.67` gains anything I re-cut the download
and add a dated entry to the bottom of this file.** So:

- **this file is the only one you need to re-read.** Everything goes here — new
  controls, changed gestures, numbers you can quote, and anything that makes
  existing manual text wrong;
- **the download will not lag the page again.** Cutting it is
  `tools/make-dist-zip.ps1` in the `.67` repo now, rather than a job done by
  hand. It opens the archive it has just written, loads the plugin **out of it**,
  reads the build id from those same bytes, and refuses to publish unless it
  loads and matches the tree. `-Verify` reports IN STEP or OUT OF STEP against
  what is live without touching anything, if you ever want to check for
  yourself;
- **the Field Findings PDF is taken live from your folder** each time, so your
  revisions to it are picked up automatically and I never overwrite them with a
  stale copy;
- `dist/README.txt` in the `.67` repo is now the source of the archive's README.
  It had no history before today because it only ever existed inside a zip.

Written into `CLAUDE.md` as a standing rule so it survives this session.

**Current state: build 260830.7, published and verified IN STEP.**

---

## 2026-09-02 — 260902.1: the globe moves, the fragment gets faces, the rack scrolls

Three of Peter's reports, all measured on the live panel before anything was
changed. Archive re-cut and verified (`make-dist-zip.ps1` reports the build id
in the zip and in the tree both at 260902.1, and loads the plug-in out of the
archive).

**The BWFX overlay could not be scrolled**, so the pedals at the bottom of the
rack were unreachable. The fix is in the SHARED fragment, not in .67: the veil's
scrollHeight was 1108 against a clientHeight of 880 — 228 px below the fold —
and setting `scrollTop` from script worked, so it was scrollable all along and
the wheel was never reaching it. A synthetic wheel inside the veil reported
`host window wheel handlers saw the event: 1 (defaultPrevented=true)`: .67
registers a window-level wheel listener and preventDefaults every wheel event
anywhere, to drag the body through the cut. The overlay now scrolls itself in
the capture phase and stops the event there, so it no longer depends on the host
leaving the default action alone. Of the ten synths only .67 takes the wheel
globally; the rest inherit the hardened fragment on their next rebuild.

**The globe sat on the readouts.** Measured at (1238,22) with `rOrd "72"`,
`rSig "30:70"` and `rGR "0.0 dB"` underneath it, plus the cut/habit/ceiling
label rows reaching x=1268 — the top-right corner plate is 265×75 and full.
Moved to `top:88`, just below the plate; verified live as *"globe at 1238,88 ->
CLEAR of all text"*.

**The traverse now leaves the body.** Peter asked that winding the wheel too far
give no cross-section and therefore no sound. Worth recording that the
mathematics does **not** give this: a cut-and-project quasicrystal is infinite
and dense, so the acceptance window holds points at every depth and no traverse
could ever empty the section. What ends is the *specimen* — B2311.67 came out of
a vault as a fragment, and a fragment has faces. The tissue now occupies the
middle of the traverse and the outer reaches are the vault: raised-cosine face
from tau 8.5 to 10.4, exactly zero beyond.

| TRAVERSE | depth tau | peak |
|---|---|---|
| 0.50 | 0.0 | 0.5866 |
| 0.20 | −7.2 | 0.6495 |
| 0.14 | −8.6 | 0.2813 |
| 0.10 | −9.6 | 0.2636 |
| 0.05 | −10.8 | **0.000000** |
| 0.00 / 1.00 | ∓12.0 | **0.000000** |

Roughly the middle 70 % of the wheel is inside the object. New permanent check
at `test/voidtest.cpp` (target `ab67void`). Bench ALL CLEAR at 71 checks.

**For the manual:** TRAVERSE now has an outside. The wheel no longer merely
runs out of range at its ends — past about ±10.4 in tau the plane has left the
fragment and there is nothing to hear, which is worth a sentence in whatever
describes the traverse.
