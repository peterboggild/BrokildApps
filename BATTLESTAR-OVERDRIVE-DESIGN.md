# BATTLESTAR OVERDRIVE — design

An overdrive with six knobs, a CRT and a fuel tank. Garage rock that dances:
American diner, psychedelia, retro sci-fi and a disco floor, mounted on the
outside of a spacecraft.

Plugin code `BsOd`. PRODUCT_NAME "Battlestar Overdrive". JUCE 8 VST3 +
Standalone, `IS_SYNTH FALSE`, category `Fx` / `Distortion`. Native-first: the
APVTS is the single source of truth and `Source/ui/ui.html` is a pure view, as
in Martian Gain and Hairfryer. **No BWFX** — this is an effect, and BWFX is a
rack of effects (the same call already made for Martian Gain and Hairfryer).

The panel decal came first and is the specification. Six pots, one screen, one
fuel tube, one button: **everything the plugin does has to fit that, and
nothing may need a control the metal does not have.** That constraint is the
reason there is no quality switch, no oversampling menu and no output trim —
the plugin has to make those decisions itself.

---

## 1. The six controls

| Pot | Parameter | What it does |
|---|---|---|
| MIX | `mix` | dry ↔ driven, linear blend, latency-compensated dry |
| THRUST | `thrust` | drive into the engine |
| ANTITHRUST | `antithrust` | retro-thrust: an inverted comb before the drive plus a choke after it |
| ENGINE | `engine` | 8 drive circuits, stepped, increasingly rabid |
| SPACE | `space` | one macro across tape delay → hall → shimmer → harmonic tremolo |
| SPECTRUM | `spectrum` | the Orange Thunderverb Shape law |

Plus `autorefill` (the red button) and the fuel tank, which is state, not a
parameter — see §5.

### MIX
**Linear, not equal-power.** Dry and wet here are correlated, and a cos/sin
crossfade of correlated signals sums to +3 dB at mid-mix — the bug already
paid for once in BWFX TUBE. Equal-power is for uncorrelated sources.

The oversampler's FIR cascade delays the wet path, so the dry path is delayed
by a **measured** compensation (an impulse pushed through the oversampler at
`prepare`), or a mid-mix setting combs. SPACE's delay and reverb are *meant*
to be late and are not compensated.

### THRUST
Pre-gain into the shaper, `10^(1.8·thrust)` ≈ 0 → +36 dB, scaled per engine so
the rabid ones do not simply arrive sooner.

### ANTITHRUST — the one Peter left open
Not a comb *or* extra clipping but the thing the name describes: **retro-thrust
fighting the drive.** Two parts, both rising together:

- an **inverted comb before the shaper**, whose delay *shortens* as the knob
  rises — 12 ms → 0.36 ms — so it travels from a loose flangey ring to a tight
  nasal honk, with a little feedback (`0.55·a³`, bounded well below 1) so it
  starts to ring metallically at the top. Pre-shaper on purpose: the distortion
  then emphasises what survives between the notches, which is what makes it
  vocal rather than merely phasey. Post-shaper it would just be a flanger.
- a **choke after the shaper**: an envelope-driven gain reduction, fast attack,
  medium release, that pulls down harder the harder the signal pushes, with a
  little lowpass closing alongside it — the engine bogging down under braking.

At `antithrust == 0` the whole block is branched around, so a patch that does
not use it is bit-identical to one compiled without it.

### ENGINE — eight circuits
Ported from Martian Gain's shaper bank, which is bench-proven, then retuned and
level-matched here. Stepped, not continuous: the screen names the engine, and a
name that slides between two values means nothing.

| # | Name | Origin | Character |
|---|---|---|---|
| 0 | IDLE BURN | WARM/VALVE | soft, asymmetric, flatters anything |
| 1 | ION DRIVE | OVERDRIVE | the classic soft knee |
| 2 | PLASMA COIL | TAPE | saturating with memory, compressing |
| 3 | AFTERBURNER | FUZZ | asymmetric clip, squashed and rude |
| 4 | RAZOR WING | RAZOR | tanh into hard clip, edge |
| 5 | WARP FOLD | FOLD | wavefolder |
| 6 | HYPERDRIVE | CHEBYSHEV | harmonic injection |
| 7 | SUPERNOVA | ANNIHILATE | fold, crush and chaos |

Each engine carries its own drive law and output trim, and the trims are
**measured** by the bench, not modelled — the Martian Gain principle. The name
appears on the CRT in blue holographic letters and fades over ~2 s after the
knob is released, brightening to a glow as it goes.

Fixed 4× oversampling. There is no quality control on the metal, so the plugin
does not get to ask.

### SPECTRUM — the Thunderverb Shape law
One knob sweeping the whole spectrum, mids against the ends:

- **CCW** mid-focused and present; highs rolled off and smooth; bass tucked.
- **Noon** close to flat, mids just beginning to dip.
- **CW** mids scooped, bass and treble both boosted — the wide hollow one.

Built as three interlocked bands on one parameter: a peaking mid at 650 Hz
moving +7 → −12 dB, a 120 Hz low shelf −4 → +7 dB, a 3.2 kHz high shelf
−8 → +7 dB, plus a treble rolloff that only exists in the bottom third of the
sweep. Broadband level is normalised across the sweep so the knob changes
tone, not loudness — an EQ that gets louder as you turn it always sounds
"better" for the wrong reason.

The metal says SPECTRUM, so that is the label; the law is the Shape control.

### SPACE — one knob, four effects, overlapping
A macro, not a selector. Stages fade across each other so every position is a
usable sound and the boundaries are not audible as switches.

| `p` | stage |
|---|---|
| 0.00 – 0.45 | **tape delay**, time 30 ms → 600 ms → 30 ms across the stage, feedback rising to 50 %, HF loss and wow in the loop |
| 0.28 – 0.72 | **large reverb**, RT60 growing 0.8 s → 6 s |
| 0.50 – 0.85 | **shimmer**, an octave-up path fading into the reverb's own feedback |
| 0.70 – 1.00 | **harmonic tremolo**, split at 800 Hz with the halves modulated in opposite phase, turning into true-pitch vibrato at the very top |

At `p == 0` every stage is off and the path is bypassed exactly.

The delay time sweeping up and back down is Peter's spec and is the good part:
it means the middle of the stage is a long dub throw and both ends are a slap,
so the knob passes *through* the useful delay range instead of ending on it.

---

## 2. Fuel, and why the gauge means something

A gauge that only decorates is a sticker. **Thrust burns fuel.** The burn rate
rises with drive, with how rabid the engine is, and with how hard the signal is
hitting; refill is constant. So the tank finds an equilibrium: play loud and it
sits low, play quietly and it recovers. It cannot strand the player at empty,
which a pure drain would.

As the tank falls the engine sags — headroom drops, the top end goes, and below
about 18 % it begins to **misfire**: short stochastic dropouts that get more
frequent as it empties. This is a real and beloved sound; it is what a dying
9 V battery does to a Fuzz Face.

**AUTOREFILL defaults ON**, and when it is on the tank is pinned at full, the
sag term is exactly 1.0, and the signal path is arithmetically identical to a
build with no fuel model at all. The sputtering is opt-in, so a fresh instance
is a normal, predictable overdrive.

---

## 3. The screen

A `<canvas>` at the measured glass rect (`assets/panel-geometry.json`), drawn
every frame from data the processor pushes: level, drive, fuel, engine.

The panel's own glass — its curved highlight, its scratches and its dirt — is
lifted into `screen-glass.jpg` by `tools/ingest-decals.ps1` and laid over the
canvas with `mix-blend-mode: screen`. **The visuals therefore sit under the real
glass rather than replacing it**, which is what keeps the screen photographic
instead of looking like a rectangle of CSS pasted onto a photograph.

Content: a starfield that streaks faster as THRUST rises, the waveform drawn as
a thrust vector, the engine name on selection, and the whole raster dimming and
rolling when the tank runs dry.

---

## 4. Art and geometry

Four sheets delivered: the panel, two full knob sets (**chrome**, with a steel
bezel, and **orange**, painted), and 15 fuel levels.

`assets/panel-geometry.json` is the single source of truth for where everything
sits, in panel pixels (1536 × 1024, exactly 3:2). It was measured
(`tools/find-features.ps1`, `tools/profile-sheets.ps1`) and then **verified by
compositing and looking** (`tools/composite-test.ps1`) — an image generator hits
no exact coordinate, so the art is fitted to the table and never the reverse.

Pots: left column x = 264, right x = 1252; rows y = 326 / 517 / 708. Knobs are
drawn at 120 px so they sit inside their tick arcs and clear the labels; at the
150 px first guess they swallowed both.

**Chrome is the default** because the steel ring matches the screen bezel and
the fuel tube's collars, so the knobs read as separate objects; the orange set
dissolves into the yellow field. Orange ships as a switchable skin — both
sheets are in the tree either way.

---

## 5. What the bench has to prove

- every engine bounded, silent in, silent out, and level-matched to within a
  measured tolerance across the THRUST sweep at **two input levels** (a static
  compensation calibrated at one amplitude cannot hold for a saturator)
- `antithrust == 0`, `space == 0` and autorefill-on each bit-identical to the
  path without them (memcmp)
- the SPECTRUM law: mid cut and end boost monotonic across the sweep, and
  broadband level flat within tolerance
- SPACE: delay time actually traces 30 → 600 → 30 ms; reverb tail grows;
  shimmer puts energy an octave up; tremolo halves move in opposite phase
- fuel: burns under drive, recovers when quiet, reaches equilibrium rather than
  stranding, misfires only below threshold, pinned at full under autorefill
- aliasing floor with 4× oversampling, DC at the output, and cost

Every measurement gets its own engine instance — rendering twice through one
leaves its delay lines primed against a fresh reference (the BWFX macro lesson).
