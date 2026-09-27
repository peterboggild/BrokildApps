# Photo-Synth2 — feature parity vs. the original browser app

The plugin embeds the **original `index.html` verbatim** (UI, styling, all
interaction logic); only the WebAudio layer was replaced by a thin bridge that
forwards the exact same parameter targets, smoothing time-constants and voice
messages to a C++ engine that is a line-by-line port of the original DSP.

## Identical

- **UI/UX**: all four photo pads with the procedurally generated demo photos,
  loupe cursor, latch, per-pad motion Rec (gesture / two-finger line / circle),
  Play–Edit modes, pad size, Swap 1↔2, desktop console layout + Zoom slider,
  toasts, spectrum scope (bars + filter curve, analytic — same math).
- **Keyboard**: docked canvas keyboard, octave shift, Hold, glissando,
  computer-key rows `zsxdcvgbhnjm` / `q2w3er5t6y7u`.
- **Voice DSP** (exact port of `synth-worklet.js`): two PolyBLEP morphing
  oscillators (sine→triangle→saw/pulse), analogue drift, PWM vibrato, ZDF Moog
  ladder with saturating core, five filter modes incl. notch and tuned comb,
  sub-oscillator, Env→Pitch sag, tape stop, ADSR with smoothstep shaping,
  2× oversampling with 3-tap decimation, idle voice skip.
- **Voice pool**: 16 voices; Mono/Unison 1–12 stacked ring-sampled voices;
  Polyphonic up to 4 notes × 1–4 oscillators with oldest-note stealing; stereo
  width by hue or cursor position (WebAudio StereoPanner pan law replicated).
- **FX chain**: same seven modules with the same parameters, mixing laws
  (equal-power sin/cos), smoothing constants and orderability:
  tube saturation (identical biased-tanh curve, 4× oversampled, DC block, tone,
  RMS loudness compensation), stereo delay (per-channel lines, damping, low
  cut, tape shaper, wow LFO, L↔R offset, feedback to 112 %, Freeze),
  convolution reverb (dual convolvers with 50 ms crossfade; room/hall/plate/
  spring/reverse impulses generated with the **identical deterministic
  algorithm**, plus WebAudio ConvolverNode normalization), 4-stage phaser with
  feedback, dual-line chorus (opposite-phase modulation, dark wet), stutter
  gate (same smoothed pulse curve), lofi (exact port: crush, bit grit, hiss,
  crackle, dirt). Disabled modules are skipped entirely (real CPU savings).
- **Additive engine** (10 partials + WebAudio-convention biquad, Q-in-dB for
  low/high-pass) as the selectable second engine.
- **Dark Drone**: sub level/octave, cluster detune, Env→Pitch, drift random
  walk, Freeze, Halt — unchanged (drift runs in the UI, see limits below).
- **MIDI file player**: same parser, tempo map, mono reduction + true chords
  in poly mode, loop, tempo scaling; **Render MIDI → WAV** is sample-accurate
  via the same event schedule, run by a private native engine at 48 kHz.
- **Presets**: on-device (IndexedDB in the WebView profile) with photo
  snapshots, plus single-file export/import.

## Adapted for the plugin

- **Recording**: captured natively at the plugin output (same 180 s cap),
  saved as 24-bit WAV through a native save dialog (a WebView cannot start
  downloads). Preset export likewise uses a native save dialog.
- **DAW integration (new)**: host MIDI notes are handled sample-accurately in
  C++ (mono and poly allocation mirroring the UI logic); ~39 Edit sliders are
  exposed as automatable VST3 parameters; the complete state — every control,
  FX order/power, cursors, motions and ≤640 px JPEG copies of the four photos
  — is embedded in the DAW project and restored on reload.
- **Compressor**: WebAudio's DynamicsCompressor safety stage is approximated
  (same threshold −6 dB, knee 10, ratio 3, attack 6 ms, release 250 ms, fixed
  makeup ≈ +2.4 dB).
- **Dialogs**: WebView2 suppresses `prompt()`/`confirm()`, so preset saves
  auto-name (`Preset <timestamp>`) when the prompt is unavailable and preset
  Delete acts immediately.
- Random components (oscillator drift, lofi noise/crackle, reverb impulse
  noise) use the same algorithms with an LCG; the reverb impulses are
  bit-deterministic, the drift/noise statistically identical.

## Deliberate plugin customizations (2026-08-20, on request)

### Fixed 2026-08-21

- **Zoom overflow** (reported as "completely broken panel"): a remembered
  user zoom was applied into windows that cannot hold it. Applied zoom is now
  clamped to fit-width; the request is honoured again when the window grows.
- **Glass Cathedral** no longer switches to the additive engine (that replaced
  the oscillator); it colours the existing tone only.
- **Cubic Catmull-Rom** interpolation on delay and chorus taps (~8 dB lower
  interpolation error on bright material -- audible on tape wow); **phaser**
  sweep recomputed every 32 samples instead of once per host block.

- **The window is dark and branded from the first frame**: the editor paints its
  own loading screen (`paintSplash`) and keeps the page parked off-screen until
  it reports `uiready`. WebView2 needs about a second to start its browser
  process before any page code runs, and the options background colour is only
  applied once its controller exists, so nothing inside the page can cover that
  gap. Note the browser must stay `setVisible (true)` --
  `emitEventIfBrowserIsVisible` tests it, and hiding the component would cut the
  page off from the state handshake.

- **White flash on opening the window** (about 1.5 s). WebView2 paints white
  until the page produces its first frame, so the editor now passes
  `withBackgroundColour (0xff0c0a07)`, and the page carries a dark
  "PHOTO·SYNTH·2 / reloading" splash in its initial markup -- nothing there
  waits on script. It comes down two frames after the patch is on screen,
  with a 250 ms timeout fallback (a page that is not composited fires no
  animation frames) and a 6 s backstop.

- **Build id**: `PS_BUILD_ID` in CMakeLists (date plus that day's build
  number, e.g. `260821.2`) is compiled in, sent to the page with the initial
  state, and shown on the splash, in the About panel and as the title
  tooltip. Bump the trailing number for another build the same day.

- **Reopening the editor showed the demo photos and their sound first**, then
  swapped in the patch a few seconds later, with the cursor and motions
  arriving late. Two changes:
  1. The processor no longer waits to be asked. `emitInitialState()` is
     repeated from `timerService()` every ~100 ms until the page answers with
     `stateack` (`uiHasState`, cleared whenever an editor attaches). An emit
     made before the browser is on screen is simply dropped, which is what
     made the old ask-once-and-hope handshake take seconds.
  2. Boot no longer loads the demo photos straight away. They are held back
     until the stored state lands (`loadDemosForEmptyPads()` then fills only
     the pads the patch did not carry), with a 1.5 s grace period for the
     case where nothing ever answers. A pad with no photo shows its hint for
     that moment instead of the wrong picture making the wrong sound.
  Measured on the reopen path with the bridge unavailable for 0/800/2500 ms:
  the demo photos are never shown, and the patch is on screen with its cursor
  and motion at 101/456/1537 ms. A fresh instance still gets its demos, at
  52 ms.

- **Output wandered off zero** (a slow DC offset, visible as the whole waveform
  drifting up and down with a nine-second period, at up to 90 % of the
  signal's own peak-to-peak). Two sources, both at the 0.11 Hz pulse-width
  LFO:
  1. `Voice::shape` integrated the band-limited square to make its triangle.
     A pulse of width `pw` carries a DC term of `2*pw - 1`, and `pw` is swept
     by the width LFO, so the leaky integrator accumulated that term into a
     large slow offset. It now integrates the square about its own mean
     (`sq - (2*pw - 1)`) -- the AC shape is untouched.
  2. The pulse wave carries the same DC term directly. A 5 Hz one-pole DC
     blocker now sits on each voice output, and a second on the master ahead
     of the gain and limiter (the tube stage saturates asymmetrically and the
     delay/reverb loops can integrate an offset of their own).
  Measured on the shipped worklet with a seeded drift walk so the two builds
  are comparable sample by sample: centre-line wander 0.48/0.84/1.11 p-p ->
  0.0001/0.0001/0.0002. On the worst-affected patches the audible tone comes
  back 2-6 dB louder, because the offset had been pushing the ladder's tanh
  into compression. Cost: 0.07 dB at 20 Hz.

- Reopening the editor lost every cursor, armed motion and latch. Two faults,
  both fixed:
  1. Cursor and motion are stored as fractions of the pad and can only be made
     real once the pad has a size, but the only thing that ever measured a pad
     was the render loop. A reopened editor answers `getstate` before its first
     animation frame (a WebView that has not composited fires none), so the
     restore silently dropped them and never retried. `applyPreset` now
     measures the pad itself (`Pad.prototype.setGeometryFrom`) and, if it still
     has no size, parks the request on the pad for `resize()` to replay.
  2. The live-state push was a plain 1200 ms debounce called from `pushAudio`.
     A replaying motion calls that every frame, so the timer was pushed back
     for ever and the state was never written at all; and the editor started
     saving its blank defaults before it had read the stored state back,
     overwriting it. Pushes are now gated on the restore having completed
     (`stateReady`), coalesce at 600 ms with a hard 2 s ceiling, honour a
     parked restore when capturing, and flush on hide/pagehide. `getstate` is
     retried until answered instead of being asked once and silently lost.

### Fixed 2026-08-20

- Switching Play ↔ Edit resizes the four photo pads (the columns are laid
  out differently), and everything a pad held in canvas pixels stayed where it
  was: the cursor landed on a different colour and the sound jumped. Pads now
  rescale their cursor, armed motion, replaying motion and in-progress motion
  recording with the canvas (`Pad.prototype.rescale`), so the relative position
  — and the sound — survives the swap. The same applies to the Pad size
  slider and any window resize.

- **Waveform select** (Sound → Voice): Photo-morph (original behaviour,
  default) or fixed band-limited Sine / Triangle / Sawtooth / Square for both
  oscillators; the additive engine spectrum and the scope bars follow.
- **Transparent output limiter** replaces the Sound on / Silence buttons in
  the transport (they are hidden, not removed — sound still auto-starts on
  the first touch). Pure downward peak riding (1.5 ms attack, 150 ms release,
  threshold 0…−9 dB with the amount), no makeup gain; default 40 %.
  Automatable as `limiter`.
- **Tempo sync** on the delay and the stutter (plugin only): a Sync button and a
  note division (1/1 to 1/32, dotted and triplet) per module. The host BPM
  arrives with the `clock` event and drives the module's own Time / Rate
  slider, so the slider stays the single source of truth -- automation,
  presets and saved state need no special case, and the value on screen is the
  value in use. The slider is disabled while synced. Limitation: the tempo is
  followed by the page, so with the editor closed the last synced value is
  held rather than tracking a tempo change.

- **SPECTRA combine** (260821.6): the one-at-a-time guard is gone; arming
  stacks with snapshot unwinding (release restores exactly what that
  character changed, later characters re-applied). Contributions from every
  armed character are combined per frame. Tape feedback capped at 70 %.
- **Filter Q factor** (`filterQ`): 0.25x-4x exponential multiplier on the
  colour-driven resonance, all modes incl. notch/band/comb and the additive
  biquad Q; 50 % = original. Low = classic Moog-style ladder, unchanged.

- **Six Spectrum characters** (build 260821.4): Dark Drone + Psychedelic Pink,
  Industrial Black, Glass Cathedral, Tape Seance, Insect Choir. One armed at a
  time -- the exclusivity is a single guard in `specArm()`; relax it there to
  let characters coexist. A character = a snapshot of the controls it moves
  (restored exactly on release), a set of values applied through normal input
  events, and optionally a `tick` writing into `specMod` -- hooks beside the
  drift hooks (`specDetAdd/specCutMul/specPanAdd/specDelayMul/specLevelMul/
  specSagAmt/specMod.cluster` in `fineCents`). Armed via illuminated rocker
  switches (`.spec-rocker`, per-module `--lens` colour). Identical in the
  browser app (minus the plugin-only state pushes).

- **Spectrum panel** (below Effects): a home for character modules — things that
  change how every voice is built rather than sitting in the signal path. Each
  wears its own enclosure (`.spec-module[data-spec=...]`), so a new character is
  markup plus one CSS block; the shared shell only handles arm and collapse
  (`initSpecModules`). **Dark Drone** moved here out of Sound as the first one,
  with a sinister skin (near-black plate, blood-red edge that lights when armed,
  serif nameplate). Every control id is unchanged, so presets, automation and
  state are unaffected.
- **Global tuning** (Sound → Pitch & scale): **Tune** ±12 semitones,
  continuous (no step of its own, so it can sit between notes), and **Fine**
  ±100 cents. Both offset every note the synth plays — base note, keyboard,
  DAW MIDI and an imported MIDI file — and move the base frequency only, so a
  held note glides instead of retriggering. Automatable as `tune` and `fine`;
  unlike the other UI-driven controls they also apply with the editor closed,
  because host MIDI notes become a frequency inside the engine
  (`Engine::hostTune`, kept in sync by `syncHostTune()`).
- **Tuner** (Sound → Pitch & scale, "Match to"): a live readout of the note being
  played plus the distance to a chosen note name, and a **Match** button that
  moves Tune / Fine so the sounding note becomes that one — always by the
  shortest path (never more than a tritone, never whole octaves). It reads the
  frequency the synth is synthesising (`toneFreq()`) rather than analysing the
  output, so it is exact on a pitch that is moving under a recorded motion and
  needs no analysis window. The correction is split whole-semitones into Tune
  and the remainder into Fine, so it stays visible and automatable; **Reset**
  returns both to concert pitch. Preset control id `tuneTarget`.
- **Defaults**: FX-photo channels R→Reverb length, G→Delay mix, B→Tube drive;
  only Tube, Delay and Reverb powered at start; Boutique skin default;
  Edit panels ordered Sound · Effects · Motion · Presets · File · How it works.
- **Removed for the plugin**: Camera and Live buttons on all four pads
  (Photos + Demo remain), and the explanatory note under each FX module.
- Photos are persisted everywhere state goes: DAW project (host save/reload),
  device presets (IndexedDB) and exported/imported `.photosynth.json` files.

## Omitted / limited

- **Camera capture and Live camera** are out of scope in the plugin: the
  buttons remain and fail gracefully (original toast); "Camera"/"Photos" both
  open a file dialog. Image loading from disk is fully supported.
- **Editor-closed behaviour**: the engine keeps sounding with its last state
  and host MIDI + most automation work natively, but motion loops, the Dark
  Drone drift walk, FX-photo modulation and photo re-sampling need the editor
  window open (they are UI-thread gestures). Automation of `voices`,
  `polyNotes`, `polyOsc`, `octaves`, `spread`, `legato`, `darkCluster`,
  `drift`, `driftSpeed` also applies on the next editor open.
- The offline render fixes its rate at 48 kHz (as the original did).
- The offline MIDI render takes the filter cut-off as a snapshot, so **Key follow**
  does not track note by note there, exactly as photo drift and the Spectra
  modulations do not. Live playback and the audio recorder follow normally.
