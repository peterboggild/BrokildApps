# Gravity Well — buglist / wishlist

Collect, don't implement: entries wait here until Peter says go.

---

## 1. The panel is an HTML app, not a VST3 — DONE 260927.1 (the cockpit)

Peter, 2026-09-27: *"sound is great — but sliders and long scrolling lists
is not at level with the other VST3 … organise the controls as knobs in
sections … glowing HUD style buttons … place the most important on the
front around the graphics, and have a page with more advanced controls."*
Reference: Thirty Thousand Years (396 params, six tabs, 64x72 knob cells).

**Possible with the current architecture, and entirely page-side.** The
processor already sends the whole parameter table in `initialState` and the
page decides the layout, so no engine, processor or parameter change: every
saved preset and project is untouched. The work is in `tools/build-ui.js`
(the shell and the bridge); the well stays authored in `well-demo.html`.

### Pages (proposed)

| page | what lives there |
|---|---|
| **WELL** (front) | the well, large, centre. Round it in two wings: the six macros as big knobs (MASS DEPTH ENERGY HORIZON TIME SPACE); A SHAPE, A LEVEL, SUB; HORIZON A circuit + CUTOFF + RESONANCE + ENV; DRIVE type + amount; AMP A/D/S/R; OUT. ~24 controls — enough to make every preset your own without leaving the page. |
| **SOURCES** | oscillators A and B, sync / PM / ring, sub, noise, the accretion disk |
| **HORIZONS** | filter banks A and B in full, routing (series / parallel / split), split point, blend, the singularity |
| **MOTION** | ENV2, ENV3, three LFOs, random, chirp — and their RADII, drawn as rings on a small well so where a modulator sits is visible, not a number |
| **MATRIX** | the eight slots as rows: source, destination, amount |
| **OUTPUT** | voicing, glide, bend range, velocity, vintage, tilt, trim, and the BWFX globe |

Oscilloscope stays along the foot of every page (no sequencer - see item 2).

### Controls

- **Knob**: SVG, not a canvas each (cheap next to the WebGL well). A HUD ring:
  cyan arc = value, **amber arc = live modulation** (the engine already
  publishes `modValue`), glow on hover/drag. Vertical drag, shift = fine,
  double-click = default, wheel. Value readout in the HUD strip while touched.
- **Choice params** (circuit, filter mode, routing, drive type, LFO shape,
  sync, voicing): segmented glowing HUD buttons, never a `<select>`.
- Section plates with thin HUD frames and letter-spaced headings, like the
  splash.

### Keep / gates

- Keep the orphan rule: any parameter no page claims still gets a knob on
  OUTPUT, so a new parameter can never be invisible.
- `uiprobe.js` must assert: every parameter appears on **exactly one** page;
  nothing clipped on any page at the default window size; tabs switch; the
  well still receives MASS; zero JS errors. Then shoot every page and LOOK.
- Rebuild the plugin, reinstall, grep the installed bytes, re-shoot the
  landing page plates.

Estimate: the layout map and the knob/HUD components are the bulk; no DSP.

---

## 2. Lose the sequencer - DONE 260927.1: reading (a), off the panel

Peter, 2026-09-27: *"we will have to lose the sequencer --- sorry about that."*

Two readings, and they are different jobs:

- **(a) off the panel only** - engine keeps it, the DAW plays the notes. Page-side, zero risk.
- **(b) out of the instrument** - removes the eight `seq_*` host parameters, the step data in the state blob, the pattern in every preset, and the SEQ source in the matrix. The published 260926.1 already carries those parameters, so this is a parameter-list change; do it now, before anyone has projects that use them.

**Recommendation: (b), and keep the 303 credible from a MIDI clip the way hardware clones do** - the two things the sequencer was really for move onto the keyboard:

- **ACCENT = velocity above a threshold** (e.g. > 100), feeding the SAME accumulating accent circuit, so consecutive loud notes still build the squelch.
- **SLIDE = overlapping notes** (legato): already half there - a note arriving while another is held glides instead of retriggering. Make the slide time the 303 fixed ~60 ms when accent-style legato is on.

Then the ACID presets lose their patterns but not their character, and the bench accent-accumulation check drives it with velocities instead of steps.

---

## Shipped in 260927.1

- **The cockpit**: viewscreen well, left pillar = CUTOFF (biggest knob) + RESONANCE + CIRCUIT on every page (Peter: cutoff is the bread and butter of live bass), right pillar = the six macros, seven consoles on the desk. SVG HUD knobs, choices as glowing buttons, long lists as steppers.
- **Sequencer off the panel (a)**: engine keeps it for the host; no factory preset turns it on (bench-guarded, one check per preset). The accumulating accent was fed by the sequencer, so the copy no longer claims it.
- **Three shipped panel bugs fixed**: (1) the page used the wrong kind numbering - choices were SLIDERS, which is why A ENGINE / B ENGINE flickered (slider sent 0.37, the plug-in stored a whole choice, the echo sent it back mid-drag); (2) native BWFX state went to BWFX.state(), a getter, so the overlay showed a different rack from the one playing; (3) the well lights orbited at hard-coded demo radii, ignoring every RADIUS knob.
- **Display laws** are one function now (expLaw): filters 18*1400^v, LFO rates 0.02*2000^v, times 0.0005*12000^v. Panel and host both showed 20*1200^v for every HZ parameter.
- The processor echo remembers what it STORED, not what the page asked for.
- Gates: gwtest 126, gwhost 18, uiprobe 20 (fixture from `gwprobe table`, i.e. from the engine; both true window sizes).

### Not done / open

- Nobody has played the cockpit in a DAW yet.
- Accent from velocity (item 2 option b) is NOT built - a MIDI clip gets velocity-to-level only.

## Shipped in 260927.3 (Peter's "wow" batch + the control audit)

- **HUD line displays** either side of the consoles, every one derived from the parameters through the engine's own formulas or from the live values it streams: REACTOR (one cycle of A and B, sub against the fundamental, coloured noise), HORIZONS (response of A, B and what is heard for the routing; the signal path, with the singularity lane), CHRONO (the three envelopes by Env::tick's segments; live traces), ORBITS (the LFO shapes with rate or division; live traces), MATRIX (a live network: nodes for every source and destination, labels only on the used ones, links breathing with their source), OUTPUT (L/R peak + rms, amber when the soft ceiling is shaping the peaks; level history). HELM carries the filter response and the meters.
- **WORLD FX** is a tab between OUTPUT and SETTINGS (the header button is gone). **SETTINGS**: PANEL BRIGHTNESS (a filter on the whole ship) and HUD LINES (glow strength), remembered per computer.
- Fonts one step up (labels 9, values 10, headings 9, tabs 10). The matrix steppers take one cell, so a slot is one row.
- **Stars**: two layers at least a pixel wide (the finest old layer was a third of a pixel and shimmered like LEDs), twinkle a tenth of the old rate and depth, a quarter of the old parallax, most faint and a few bright, crowded into a clumpy band with dust lanes.
- **LFO TEMPO**: the rate knob reads the division the engine plays (4/1 .. 1/32 with D and T), locked to the bar while the transport runs.
- **CROSS ON kept the pitch jump**: +1201 cents, now within 3 cents (sync ratio folded to [1,2)).
- **Dead wires found by the per-control audit and fixed**: PAN, LEVEL, WHEEL, PRESSURE (matrix), B WIDTH, NOISE COLOUR, DISK ORBIT, VINTAGE, TIME, SPACE. ACCENT and SEQ left the source list (the sequencer is off the panel). ENV2 and ENV3 ran 16x too slow (timed per sample, ticked per control block) - fixed, which makes every filter-envelope patch snappier; bank re-levelled.
- ENV 2 is labelled FILTER ENVELOPE (both horizons' ENV knobs read it); ENV 3 is ENV 3 FREE. A MODE a circuit ignores is struck through.
- Gates: gwtest 136, gwhost 18, uiprobe 68. The bench audit renders every control against a context where it should matter: 126 controls, none inert.

### Open, for Peter

1. **MASS** is near-inaudible on plain patches (it dilates clocks, which only shows when something moves). Keep, or give it a direct audible hook?
2. **REDSHIFT** only acts in the FORMANT circuit. Intended, or should it tilt every circuit?
3. **SUB OCTAVE 1 plays at the note's own pitch**; only 2 is an octave down (the engine divides the note by the value). Fixing it drops the sub an octave in almost every preset you have approved, so it is NOT changed. Options: relabel (AT PITCH / -1 OCT), or make 1 = -1 oct and 2 = -2 oct and re-voice.

## Shipped in 260927.4 (Peter's answers to the three questions + his play-through)

- **SUB OCTAVE** now counts octaves below oscillator A (1 = one down, 2 = two); it used to divide the note, so 1 sounded at pitch. SUB ONLY re-voiced (A up an octave, so its sub sits where it did), ROUND DUB's sub 0.95 -> 0.70. Bank re-levelled: 1.4 dB spread.
- **MASS past noon is heard directly**: both cutoffs sink up to 1.8 octaves and the sub gains up to +50 %. At or below noon: bit-identical (bench memcmp). Measured past noon: harmonics 6-20 -15.9 dB, sub +6.9 dB.
- **REDSHIFT reaches every circuit** (cutoff scaled by the redshift factor, blended by the knob so REDSHIFT 0 is the old sound; FORMANT keeps its own law). No circuit greyed - it makes sense on all seven, comb included. The knob dims while there is no mass in the well, because then it has nothing to act on.
- **GROWL "stepped"**: the vowel path a-e-i-o-u dropped F2 1450 Hz between i and o; now u-o-a-e-i, F2 only rises. Matrix routes to A/B CUTOFF are interpolated per sample (they stepped once per control block).
- **CPU: SWARM 89 % -> 10 % of a core.** The WAVETABLE engine summed up to 64 sin() + exp() per harmonic per unison voice per sample; it now reads precomputed band-limited tables (8 recipes x 17 shapes x 7 bandwidths, 7.8 MB shared by every instance), within -56 dB of the additive recipe. Denormal-free timing of all 32 presets: everything else 3.7-9.4 %. Bench gate: SWARM < 20 %, the heaviest machine the panel allows (DUO, two notes, 8 disk voices, both oscillators wavetable, drive, ringdown, ring mod) < 40 % - measured 20.7 %.
- Stars: the hash was fract(sin(big)), which loses float precision on a GPU and lined cells up in rows of dots; now a sin-free hash, and a little sparser. The grid is drawn with weight and a faint glow (seven passes of a small mesh).
- HUD LINES: scales line weight as well as glow, runs 30-300 %, default 150 %, and SETTINGS shows two live displays.
- Gates: gwtest 145, gwhost 18, uiprobe 72.

## DONE 260927.6 - was AWAITING GO (2026-09-27, Peter's play-through after 260927.4)

### A. The signal chain: DRIVE position, and the amp envelope before the filter
Measured with `gwprobe envcouple` (GW_BARE=1 isolates the chain: no singularity, ringdown or MASS tracking).
- **The AMP envelope changes the tone, because it is applied BEFORE the filters and the drive** (`sum *= env1` ahead of `runFilter`), and they are nonlinear. With the filter envelope depth at 0, the brightness moves between the strike and the sustain by **8.3 dB on LADDER**, 2.9 on SVF, 0.8 on DIODE, and the loud strike comes out DARKER (the ladder's saturation squashes the highs when hit hard). That is what "the AMP envelope is doing filter envelope" is.
- Proposed order: oscillators -> DRIVE (PRE, new default) -> HORIZONS -> DRIVE (POST, the current position) -> AMP (VCA) -> ringdown/space. The amp envelope then only changes level, and the filters always see the full signal.
- DRIVE gets a PRE / POST switch in its panel (one drive, not two - cheaper and simpler; two drives can come later if wanted). **Kemper**: the factory presets that use drive (drv_amt > 0) are stored at POST so they keep their sound; only new instances and presets without drive default to PRE. The measured power-neutral trims are re-measured for the PRE position.
- Moving the VCA after the filter changes EVERY preset a little (the filter now sees full level on quiet sustains) - the bank gets re-levelled and re-listened. Worth doing both in one pass since both reorder the chain.

### B. The FILTER envelope does nothing on most presets
- It is wired correctly, but its DEPTH lives on the HORIZONS page (A ENV / B ENV) and defaults to 0. **Only 7 of 32 presets turn it up**, so on 25 presets the FILTER ENVELOPE knobs on CHRONO move nothing.
- Also: in DUO the filter envelope is taken from voice 0 only, so the second voice's filter follows the first voice's envelope. Should be per voice.
- Proposal (Peter): show the two built-in routings as the first rows of the MATRIX, drawn in the network - AMP ENVELOPE -> LEVEL, FILTER ENVELOPE -> A CUT / B CUT with their depths - reconfigurable: the filter envelope's target (A, B, A+B, or any matrix destination) and depth editable there and mirrored by a DEPTH knob on the FILTER ENVELOPE console itself, so the envelope and its amount sit together. The AMP route stays on LEVEL (re-targeting it away from the VCA would leave notes sounding for ever) but its depth/velocity is shown.
- Consider a small default depth (+30 %) for NEW instances only, so the filter envelope is audible out of the box; presets keep what they store.

### C. TIME is not neutral at noon for the LFOs
- `macroTime = 0.25 + 1.75*v` gives x1.125 at v = 0.5, so every free LFO and RANDOM runs 12.5 % fast at the default. The envelope half (`4^(-2(v-0.5))`) IS exactly 1 at noon. Fix: the same exponential law for both (x0.25..x4), exactly 1 at noon.

## Shipped in 260927.5 / .6 (the batch above, plus Peter's mid-build asks)

- A: DRIVE has a POSITION switch, PRE (default) or POST; the amp (VCA) sits after the
  filters and the drive. Two measured trim tables, one per position. Twelve factory
  patches that set DRIVE keep POST, so they sound as voiced.
- B: every voice has its own filter envelope (DUO shared voice 0's). All 32 factory
  patches wire it; the ones that did not use it got a null shape. The MATRIX draws the
  built-in routes dashed (amp env -> LEVEL, filter env -> each cutoff at its depth).
- C: TIME is neutral at noon (x1/4 .. x4); synced LFOs ignore it.
- OCTAVE is +-3 on A and B. ORBITS: each LFO has TARGET + AMT, shown in the MATRIX.
- CORRECTION to what was reported earlier: the "amp envelope moves the tone 8.3 dB"
  figure was mostly an onset-window artefact. Measured again over a 0.15 s window it
  was near 0 dB; after the reorder the amp moves the tone <= 0.5 dB.
- THE PITCH WHEEL WAS BROKEN: BEND RANGE was applied in the processor AND in the
  engine, and divided by 12 twice - a full throw at BEND 2 moved 0.33 semitone.
  Fixed; plus TRANSPOSE (host parameter, +-1 = +-1 octave), both smoothed at control
  rate. Bench measures both (150 checks).
- USER SLOTS 32-199: SAVE asks for a slot (first free offered, a taken slot says what
  it replaces), writes "NNN NAME.json" in Documents\Brokild patches\Gravity Well,
  shared by every project; the factory bank 00-31 cannot be written. Old unnumbered
  saves still load and list as "--". Header arrows step factory then user slots.
- The flight manual (14 pp, landscape) ships in the zip and on the landing page.
- Gravity Well joined the Brokild Collection (12: eight instruments, four effects).

### Collected, awaiting go
- The foot oscilloscope draws raw level, so at -12 dB it is almost a flat line. An
  auto-gain (normalise to a slow peak follower) would make it readable. Display only.
- A hardware-style wheel pair drawn in the KEYS foot (pitch bend + TRANSPOSE) - today
  TRANSPOSE is a knob on OUTPUT and automatable/MIDI-learnable from the host.

### Collected 2026-09-27: the patch chart with many user patches (awaiting go)
Rendered live with 100 fake user patches: every one appears, names fit, but YOURS is one
240 px column that scrolls (2316 px of content, about 10 rows visible) with the browser's
default WHITE scrollbar, which breaks the cockpit look. Usable, not good at 100+.
Proposal: YOURS gets its own tab/page in the chart - a grid of numbered cells 032-199 in
columns like the factory ones, 40 per page (5 pages, arrows), empty slots shown dim so
the numbering is visible and a slot can be clicked to save into it. Plus a themed thin
scrollbar anywhere a scroll remains. Panel only; no engine or file change.

### DONE 260927.7: the patch chart for many user patches
Tabs FACTORY 00-31 / YOURS 032-199; YOURS is a grid of the numbered slots, 42 per page
(four pages), filled slots load, EMPTY slots are drawn dim and clicking one opens SAVE
aimed at that number; unnumbered legacy files in a strip below; themed scrollbar.
Verified live with 100 fake patches: 42 cells a page, nothing clipped, no scrollbar,
empty slot 140 opened SAVE on SLOT 140, zero JS errors.

### Collected 2026-09-27: a "tc" click at the end of the release on ringdown patches (awaiting go)
Peter hears it on RINGDOWN, HAWKING and DOOM WEIGHT - exactly the factory patches with
RINGDOWN on (k16 0.45, k26 0.80, k31 0.65; k28 EVENT HORIZON 0.70 too).
Cause, from the code: the ringdown resonator bank sits AFTER the amp and rings on with
its own decay (longer with more MASS), but the voice is switched off the moment the amp
envelope ends (EngineImpl.inl: `if (!v.e1.active() && !v.held) { v.on = false; return 0; }`),
so whatever the body is still ringing is cut to zero in one sample.
Fix: keep the voice alive after the envelope ends until the ringdown's output has decayed
below about -90 dB (with a short safety fade), so the body finishes ringing on its own;
bench check = the last sample step at voice end on each ringdown preset, against the
note's own settled step.

### DONE 260927.8: the ringdown release click, and the patch chart as the place to save
- THE CLICK, fixed and measured. It was on all FIVE ringdown patches (COMB TUNED has
  RINGDOWN 0.20 too): a voice was switched off the sample the amp envelope ended, cutting
  the body mid-ring - measured jumps of -25 dB (COMB TUNED), -35 (HAWKING), -45 (DOOM
  WEIGHT), -59 (RINGDOWN), -66 (EVENT HORIZON). Now the body rings out alone (source and
  filters idle, only the modes tick) until below -100 dB, with a 60 s ceiling that fades.
  Bench `testVoiceEndsQuietly` plays and releases every preset to its last voice and
  reads the voice's own jump to silence (Engine::stopJump): worst now -93 dB, and it
  failed on the old code for exactly the five.
- The first version of that check read the OUTPUT in the 10 ms before the stop, which
  also contains the end of an ordinary steep fade; reading the voice's own jump at the
  sample it stops is what separates a cut from a fade.
- THE CHART: one place to load, save and clear. Tabs FACTORY / YOURS / SAVE; the slots are
  a 6 x 7 window that slides a COLUMN (arrows, Left/Right) or a PAGE (double arrows,
  PageUp/PageDown) over 032-199; SAVE names the sound, a click aims at a slot (amber),
  SAVE or REPLACE; double-click an empty slot saves straight there; CLEAR SLOT takes two
  clicks and moves the file into a Deleted folder the scan never reads. Brighter type.
- A real bug the live test caught: the save name was read from the header text, which
  the "saved X in slot N" notice had overwritten, so a re-save was named "X in slot N".
  The header is now set only by the page, and the current name lives in S.curName.
