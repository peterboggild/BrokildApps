# FULL METAL RACKET — design

A twelve-channel analogue drum machine with a sample layer per channel, a
32-step Electribe-style sequencer, and BWFX as its only effects. Brokild
family, JUCE 8 VST3 + Standalone, `IS_SYNTH TRUE`, MIDI in, category
Instrument / Drum. Plugin code `FmRk`. Build id `FM_BUILD_ID`.

Read this before writing any of it. Written 2026-08-26 from Peter's brief.

*Name*: Kubrick pun in the family vein (Blade Ruiner, Clone Wars, Mars
Wars), and it happens to describe the machine — the metal core drives the
hats and cymbals, and a racket is what it makes. Alternates if it doesn't
land: **DRUM FICTION**, **THE DRUMMING MAN**, **BOOM TOWN**.

---

## 0. The one-line thesis

Every analogue drum machine ever built treats its twelve voices as twelve
separate circuits that happen to share an output jack. **Full Metal Racket
treats them as one instrument**: they ring each other, they share a power
rail, they live in one shell, and they age together. That is the whole
design idea, and everything wild below falls out of it.

---

## 1. The twelve channels

Peter's list, with the one thing it was missing:

| # | Channel | Models | Notes |
|---|---------|--------|-------|
| 1 | **BD 1** | PING / PUNCH / MEMBRANE | |
| 2 | **BD 2** | PING / PUNCH / MEMBRANE | |
| 3 | **SD 1** | SHELL / WIRES / CLAP | |
| 4 | **SD 2** | SHELL / WIRES / CLAP | CLAP model = a clap without spending a channel |
| 5–8 | **TOM 1–4** | MEMBRANE / CONGA / BLOCK | one KIT TUNE knob tunes all four to a scale |
| 9 | **HAT** | METAL / RING / NOISE | **dual voice: closed + open, two MIDI notes, self-choking** |
| 10 | **CYM 1** | CRASH / RIDE / SPLASH | three-band decay |
| 11 | **CYM 2** | CRASH / RIDE / SPLASH | |
| 12 | **FX** | BELL / WOOD / ZAP / VOX | the wildcard step |

The hat is the correction. One "HH channel" is not a hi-hat — a hi-hat is
an *instrument with a pedal*, and its whole musical function is that the
open one is cut off by the closed one. So channel 9 is one circuit with two
triggers, two decays, and a hard internal choke. Cymbals get an assignable
choke group too (a crash choked by a hand is a real gesture).

---

## 2. The synthesis — how it gets to be excellent

Not a clone of anything. The excellence comes from getting the *mechanisms*
right, and there are only six of them. Every voice is assembled from the
same kit of blocks, which is why the machine sounds like one machine.

### 2.1 The blocks

**A. Nonlinear resonator.** A two-pole state-variable ping, but with
**amplitude-dependent damping** — Q softens as the amplitude grows, the way
a transistor limits a twin-T pushed near self-oscillation. This one detail
is the difference between "a sine with an exponential envelope" and a kick
drum: the decay is not a straight line on a dB plot, it *starts* fast and
then hangs on, and it does that differently at different levels. Get this
right first; four of the twelve channels live on it.

**B. Tension nonlinearity.** A struck membrane is stiffer when it is
displaced further, so a hard hit **starts sharp and falls to pitch**. Real
machines fake this with a pitch envelope. We have the actual mechanism:
feed the resonator's own amplitude back into its frequency. One knob,
`TENSION`, and toms suddenly behave like drums instead of like tuned
beeps. This is the single most audible "why does this sound better" lever
in the machine.

**C. Coupled modes.** Two or three resonators with a settable inharmonic
ratio, exchanging energy. A membrane's modes are not harmonic (1 : 1.59 :
2.14 for an ideal circular one) and the ratio is what tells your ear
"drum" rather than "tone". `MODE` sweeps from harmonic (tuned, useful for
bass) through the ideal membrane to clangorous.

**D. Metal core.** Six square oscillators at deliberately awkward ratios,
the 808 idea, but with `SPREAD` controlling how inharmonic the cluster is
and `RATIO` choosing the family. Everything metallic — hats, both cymbals,
the bell — is this core through different filters and decays.

**E. Multi-band decay.** The reason most software cymbals sound like a hat
with the decay knob up: a real cymbal's bands do not decay together. Three
bands, three decay times. Cheap, and transformative.

**F. Excitation.** The trigger *is* the attack. A shaped pulse (width,
asymmetry, a diode-ish knee) drives everything, and a separate CLICK path
— a few milliseconds of bright filtered noise or pulse — sits alongside the
body with its own level and tone. Most of what people hear as "punch" is
this path, not the body.

### 2.2 Assembly, per family

- **BD PING** — one A with a fat B, driven by F. Decay knob walks it toward
  self-oscillation; the amplitude damping is what stops it.
- **BD PUNCH** — triangle/sine oscillator + a fast exponential pitch drop +
  a saturating VCA + independent click. The modern kick.
- **BD MEMBRANE** — C with two coupled modes and heavy B. Does the things
  no analogue machine does: a kick that pitches *up* when hit hard.
- **SD SHELL** — two A bodies a fourth apart + noise through a bandpass,
  one SNAP balance.
- **SD WIRES** — the snare bed properly: noise through a bank of narrow
  bandpasses with their own rattle decay, and a **buzz threshold** so the
  wires only rattle above a level. Ghost notes stop rattling on their own.
  Nobody does this and it is the sound of a real snare at low velocity.
- **SD CLAP** — a burst of four re-triggers with decreasing spacing plus a
  short reverberant tail; the spacing is a knob.
- **TOM** — C + B, three models by decay/ratio preset.
- **HAT / CYM / BELL** — D through E, high-passed, with attack time > 0 for
  a crash swell.
- **FX WOOD** — A at 1–2 ms with a huge F share. Rimshot, claves, block.
- **FX ZAP / VOX** — an oscillator with a large bipolar pitch sweep; VOX is
  a pair of formant bandpasses.

### 2.3 Per channel, always

TUNE · DECAY · TONE · DRIVE · LEVEL · PAN on the face; in the deep panel:
the model's own controls, the pitch envelope drawn as an editable curve,
velocity mapping, choke group, output routing, sample slot.

**Velocity does three things, not one.** Harder is louder, *brighter*, and
slightly *sharper* — that is what a drum does. Three depths, and the
defaults are already musical.

---

## 3. The sample layer, and the good idea in it

Every channel has a sample slot. WAV/AIFF, dropped on the strip, embedded
in the patch (the Photo-Synth lesson — a project that loses its assets is
a broken project; cap it, warn above it, offer 16-bit mono conversion).

Three routings, and the third is the reason to have it at all:

1. **MIX** — sample and analogue side by side, one BLEND knob.
2. **THROUGH** — the sample plays into the channel's own VCA, drive and
   tone stages, so it glues instead of sitting on top.
3. **EXCITE** — *the sample replaces the trigger pulse*. Your sampled stick
   hit, coin, door slam or breath becomes the excitation that rings the
   analogue resonator. A tom struck by a matchbox. This turns the sample
   layer from a crutch into the most creative thing in the machine, and it
   is exactly the hybrid Peter's brief implies without naming.

Plus: start offset, tune (tracked or fixed), reverse, one-shot or gated,
and a `DUCK` that lets the sample transient own the first 20 ms while the
analogue body comes up under it.

---

## 4. The wild parts

Peter asked what would be wild but *makes sense for beatmaking*. Six, in
the order I would build them.

### 4.1 THE WEB — a bleed matrix
Hit a snare in a real room and the toms ring. Hit a kick and the hats
buzz. **Any channel's trigger can excite any other channel's resonator at
a low level** — excite, not trigger: it rings the circuit without firing
its envelope. Drawn on the panel as threads between the twelve strips,
Mars Wars patch-bay style.

Why it matters: this is the thing bus compression is trying to fake. It
makes twelve separate circuits behave like one kit in one room, and it
costs almost nothing because the resonators already exist.

Alongside it, **KIT BODY**: one shared resonator every channel feeds, with
its own tuning and decay. The shell the whole machine sits in. Not a
reverb — BWFX SPACE is the reverb — a coupling.

### 4.2 RAIL SAG — the shared power supply
Real machines share a rail; a hard hit pulls it down and *everything* dips
for a few milliseconds — level **and pitch**. That pitch component is why
sag glues in a way a compressor cannot. One knob, and at zero it is exactly
absent (the BWFX additivity rule applies inside this plugin too).

### 4.3 REBOUND — a stick that bounces
One knob per channel that turns a single trigger into a physically
modelled bounce: decreasing intervals, decreasing velocity, the way a
stick actually behaves. Low = a flam. Middle = a drag. High = a buzz roll.
One control, three techniques that are otherwise a nightmare to program.

### 4.4 KIT MORPH — A/B across the whole machine
A fader between two saved kits, morphing every continuous parameter and
stepping the switched ones at staggered thresholds. Idiomatic to the
family — this is Clone Wars' THE WAR and BWFX's `applyMorph`, and the code
already exists in `Rack::applyMorph` to copy the pattern from. Automatable,
and the single best thing to put under a knob for a live take.

### 4.5 KEY MODE — a tuned drum machine
Any channel switchable to chromatic play: MIDI notes track the resonator
properly, decay scales with pitch (a low note rings longer, like a real
one), and the pitch envelope scales too. A tuned kick as a bassline is the
most-used trick in modern music and almost no drum machine does it
*correctly*. With the MEMBRANE model in harmonic MODE this channel becomes
a genuinely good bass synth.

### 4.6 THE HAND — humanising that isn't jitter
Random ±ms is not human. A drummer has **limbs**: assign channels to four
limbs, and a limb cannot play two things at once — the second hit
displaces, which is where flams come from. Add per-subdivision push/pull
(hats ahead, snare behind) and velocity carry-over from the previous hit
on the same limb. Two knobs, `FEEL` and `GRIP`, and the machine breathes
instead of stuttering.

### Also, quieter but no less real
**AGE / TOLERANCE** — per-channel component tolerances, drifting slowly, so
no two instances are identical and no two hits are (the Black Rider VINTAGE
and Clone Wars UNIT AGE lineage). At zero, bit-identical determinism.

---

## 5. The sequencer

32 steps, Electribe idiom, but **MIDI is the primary input** — the
sequencer only runs when armed, so it never fights the DAW.

- **16 keys + PAGE A/B**, with a 32-wide LED ribbon above showing both
  pages at once. Best of the Electribe and the two-row machines.
- **Per lane**: length 1–32, clock divide, direction (fwd/rev/pendulum/
  random), and **its own swing**. Per-channel length gives polymeter for
  free; per-channel swing is how grooves actually work (swung hats over a
  straight kick).
- **Per step**: on/off, accent, velocity, microtiming, probability,
  condition (1:2, 1:4, FILL, NOT-FILL, PREV), ratchet count with a level/
  pitch ramp, and **parameter locks** — any channel control, per step.
- **Motion lanes**: Electribe motion sequence, recorded live from a knob.
- 64 patterns per kit, chainable, plus a simple song row.
- **MIDI recording into the sequencer**, and MIDI *out* of it, so it can
  drive other instruments.
- Copy/paste for steps, lanes, patterns and kits. Undo.

---

## 6. Panel and graphics

**Wide horizontal machine, cream panel, wood cheeks, amber/orange/red
LEDs, silver knobs with red pointers, black silkscreen legends.** The
fleet is dark (Black Rider, Escape Room, Mars Wars); this one should be the
1980s desk object that lights up the room.

Layout, top to bottom:

1. **Header** — nameplate, LED tempo readout, transport, KIT name, the
   **KIT MORPH** fader, the BWFX globe, LIGHT slider (family standard),
   MIDI indicator.
2. **The twelve strips**, full width, all knobs visible at once — that
   tactile wall of controls is the whole joy of the tradition. Six knobs,
   a MODEL selector, a trigger pad, sample LED, mute/solo, DEEP button.
3. **The DEEP panel** — the selected channel's full circuit, drawn: pitch
   envelope as a curve, sample waveform, model-specific controls.
4. **THE WEB** — the bleed matrix as threads across the twelve strips,
   with RAIL SAG, KIT BODY and AGE beside it.
5. **The sequencer** — ribbon, 16 keys, page toggle, lane settings, step
   editor, pattern/song.

### Who draws what
I build every interactive part in CSS/SVG, as in all seven existing
plugins — it has to be live, themeable and pixel-exact, and a raster panel
cannot be. **ChatGPT is the right tool for the raster decals**: panel
texture, nameplate, printed instruction card, knob caps, worn metal
plates. Clone Wars already has `tools/embed-decals.py` for exactly this.
Prompts are in section 10.

---

## 7. Architecture

Native-first, as Mars Wars / Black Rider / Hairfryer:

- **`SPECS[]` parameter table** in `Engine.cpp` (id, name, default, kind,
  range, member accessor) walked by the APVTS layout, `processBlock`,
  randomiser, `initialState` and the bench. Hairfryer's structure — it
  makes the read-order class of bug inexpressible. ~130 automatable
  params: 12 × 9 channel + ~22 global.
- **Patterns, kits, samples and the WEB are NOT host params** — far too
  many. They live in a string-keyed opaque blob, exactly as BWFX rack
  state does, saved in the APVTS XML as an attribute and in patch files.
  Tolerant `fromJson`, empty = defaults, so future versions never break
  old projects.
- Page is a pure view. Protocol `{k:"p",id,v}` up; `initialState`,
  `hostParam`, `meter`, `seqPos`, `notice` down. The `hello` handshake
  from Hairfryer (page announces itself on every boot, processor clears
  `uiHasState`) — port it from the start.
- **Polyphony 4 per channel** (a hat roll overlaps itself), 48 voices
  worst case. These are two or three biquads each; it is nothing.
- **Oversampling 1×/2×/4×** for the nonlinear resonator and the drive
  stages.
- **Multi-bus output**: main stereo + 12 mono aux. Essential for
  beatmaking and the first thing anyone will ask for. Per-channel switch:
  to the rack, or direct to its own out.
- **BWFX**: `rack.prepare` / `process` / `service` on the main bus, blob in
  state and in kit files, 15 Hz processor-side timer, `worldMod()` mapped.

### What SPECTRA does to a drum machine
The world-mod bus maps naturally and this is worth stating, because one of
its rules inverts here:

- `detuneCents` → per-channel resonator tuning, golden-angle fanned across
  the twelve so the kit spreads rather than transposes.
- `panSpread` → the twelve channels spread across the field.
- `tremDepth/Rate` → per-channel level, fanned.
- `filterMul` → every tone/bandpass centre.
- `pitchSag` → **adds to the pitch envelope depth**. The house rule is
  "never key sag to the amp envelope" — that rule exists because a
  *sustained* note sits below unity and would go permanently flat. A drum
  has no sustain; it is a decaying transient, and pitch falling with the
  decay is not a bug, it is what a drum does. So here the amp envelope is
  the correct source, and the code comment must say why the rule is being
  set aside.

`setWorldModConsumed(true)`, and the neutral bus must be memcmp-identical.

---

## 8. The things the brief forgot, all of which are essential

1. **Open + closed hi-hat with a choke.** Section 1.
2. **Assignable choke groups** beyond the hat.
3. **Multi-outs.** Section 7.
4. **Velocity must do more than volume.** Section 2.3.
5. **An editable MIDI note map** with a GM-ish default and MIDI learn.
   Hardcoded note numbers make a drum machine unusable in twenty minutes.
6. **Samples embedded in the project**, with a size cap and a warning.
7. **A fresh instance must be silent**, and a fresh instance must sound
   *good* — the boot kit is the demo, and most people never load another.
   Budget real time for it.
8. **Copy/paste and undo** across steps, lanes, patterns and kits.
9. **Latency**: the drive stages and any lookahead limiting must report
   honest PDC, and the sequencer must be sample-accurate against the host
   PPQ, not block-quantised. State the number in the manual.
10. **The offline bench**, non-negotiable in this family (section 9).
11. **Kit library**: 40 factory kits minimum, generated from a seed the
    way Black Rider's 200 patches and Blade Ruiner's 1000 moods are, so
    every one is distinct by construction and the bench can prove it.
12. **CPU budget** measured and published, at 12 channels all firing at 4×.

---

## 9. The bench — `test/`, plain C++, no JUCE

This family's rule: if it isn't measured it isn't done. What to assert:

- Silence in → **exact digital zero** out with no triggers.
- Every voice bounded at every extreme; every model, every knob.
- **The nonlinear resonator does not run away** at max decay × max drive ×
  max Q — this machine has resonators deliberately parked near
  self-oscillation, which is the Escape Room / Black Rider feedback lesson.
- Decay is monotone: successive RMS windows must fall, every model.
- **Tuning**: Goertzel on the BD fundamental equals the TUNE setting;
  KEY MODE two octaves = ratio 2.00 exactly.
- **THE WEB works, not merely is bounded** — the Mars Wars patch-bay
  lesson. Render with and without each source→dest thread and measure the
  difference; a thread wired to nothing would otherwise pass every test.
- **RAIL SAG** measurably ducks, and at 0 is memcmp-identical.
- **Choke**: an open hat is silent within N ms of a closed hat.
- **REBOUND** produces the right number of hits at the right spacing.
- **Sequencer timing**: each step lands within one sample of its host PPQ
  position, at four sample rates and four tempos; swing measured in ms;
  polymeter lane lengths verified over a long render.
- **Determinism**: same seed, same triggers → bit-identical, with AGE at 0.
  With AGE up, varies but stays bounded.
- **Every factory kit**: bounded, audible, distinct from every other.
- **BWFX empty rack**: memcmp-identical to no rack at all.
- CPU at 12 channels × 4 voices × 4× oversampling.

And the two traps this family keeps falling into, worth writing on the
wall before starting: **render long enough to reach the thing under test**
(the KIERANATOR bar-length bug), and **make sure the probe can see the
effect** (pink's 24.6 s LFO measured over 12 s). A vacuous pass is worse
than a failure.

---

## 10. Decal prompts for ChatGPT

Raster assets only; everything interactive is CSS/SVG. Ask for PNG with
transparency, 4× the on-screen size, flat-on and undistorted.

**Panel texture** — "A seamless, flat-on photographic texture of a 1980s
Japanese drum machine's painted steel front panel in warm cream-beige
(#e8dcc4), very slight orange-peel paint grain, faint even wear, no
lettering, no hardware, no shadows, no perspective. Evenly lit, tileable,
4096×4096."

**Nameplate** — "A rectangular brushed-aluminium badge, flat-on, reading
FULL METAL RACKET in a condensed 1980s industrial sans-serif, deeply
etched and ink-filled black, with a thin bevelled edge and two tiny
countersunk screw holes. Transparent background, no shadow, no
perspective, 2400×400."

**Knob cap** — "A single flat-on top-down photograph of a small silver
aluminium synthesiser knob cap with fine concentric brushing, a knurled
skirt, and one flat red pointer line from centre to edge. Transparent
background, perfectly circular, centred, no shadow, no perspective, no
panel, 1024×1024."

**Wood cheek** — "A flat-on photograph of a solid walnut end cheek for a
1980s synthesiser: warm mid-brown, straight open grain running vertically,
one softly rounded front edge, satin oil finish. No hardware, no shadow,
no perspective, 1200×2400."

**Instruction card** — "A small silkscreened instruction plate for a
vintage drum machine: matte black rectangle with fine white 1980s
technical sans-serif legends and thin white rule lines, laid out as a
compact reference table, slightly worn ink. Flat-on, transparent outside
the plate, no perspective, 2000×1200." (Legends replaced in code — ask for
the plate, not the text.)

**Front-page preview** — the family pipeline shoots this from the live
plugin over CDP, 16:9, not generated.

---

## 11. Build order

1. Skeleton: CMake, `SPECS[]`, APVTS, WebView2 page, `hello`, empty panel,
   BWFX rack wired in. Bench harness compiling and running from day one.
2. Blocks A–F with their unit tests. This is where the quality lives; do
   not move on until the resonator's damping and TENSION measure right.
3. The twelve channels and their models. Boot kit.
4. Sample layer, all three routings.
5. Sequencer + MIDI (map, learn, record, out), sample-accurate.
6. THE WEB, RAIL SAG, KIT BODY, AGE.
7. KIT MORPH, KEY MODE, REBOUND, THE HAND.
8. Panel proper, decals, DEEP panels, the web drawing.
9. 40 factory kits from seeds; bench proves them distinct.
10. Manual, landing page, `app.json`, preview, zip, push.

Steps 1–3 are the plugin. Everything after 3 is why it is worth building.

---

## 12. Open decisions for Peter

Nothing here blocks a start; these are the forks worth his opinion when we
get to them.

- **Name** — FULL METAL RACKET, or one of the three alternates.
- **Panel colour** — cream/wood as specified, or dark to match the fleet.
- **Factory kit count** and whether kits are seed-generated (Black Rider
  style) or hand-made.
- **Sample size cap** per channel, and whether to auto-convert to 16-bit
  mono on import.
- **Is the FX channel one wildcard or a second hat?** Twelve is what he
  asked for; the FX channel is the more interesting answer.
