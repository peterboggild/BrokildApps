# KICKSTART — the kick drum synthesiser

Built 2026-09-25 from Peter's brief: *"the perfect kick synthesizer VST3 based on
synthesis, not samples. It should cover all from realistic/acoustic, to classic
analog (the MusicRadar ten), modern/gabber/dubstep, and compressor, transient
control and drive (use 4 drives from Battlestar Overdrive). Try to not make
individual simulation synths of all the different styles, but find ways to cover
this vast ground with a manageable number of controls. Make presets covering the
different styles."*

Source `vst3-apps/kickstart/plugin/` (in the website repo, the Clone Wars shape),
built to `C:\Users\peter\b\_build\Kickstart\plugin`. Plugin code **`Kick`**,
PRODUCT_NAME **Kickstart**, VST3 + Standalone, Instrument/Drum, build id
`KS_BUILD_ID` in CMakeLists (**260925.1**). Patches in
`Documents\Brokild patches\Kickstart`. Installed to `Brokild collection` in both
houses. **No BWFX**: the kick has its own ROOM, and a kick synth is a channel
strip, not a synth voice that wants a pedalboard.

## 1. The idea: axes, not simulations

Every kick drum is a pitched body that is struck. What differs between an acoustic
kick, an 808, a 909 and a gabber kick is WHERE they sit on a handful of physical
axes, so the instrument is those axes and nothing else:

| axis | control | acoustic | 808 | 909 | modern |
|---|---|---|---|---|---|
| how far the pitch falls | SWEEP | 2–6 st | 3–4 st | 24–30 st | 20–36 st |
| how fast | BEND | 12–25 ms | 40–60 ms | 18–22 ms | 6–35 ms |
| how the body dies | DECAY + CURVE | exponential | long exponential | exponential | held, then gone |
| what the body is | WAVE | sine | sine | triangle | triangle → square |
| how much real drum head | SKIN | 40–85 % | 0 | 0 | 0 |
| what the beater is | CLICK + TONE | felt / wood | soft | noise click | tick |
| which decade | GRIT + ROOM | room | — | — | room → rumble |

Then the processing every kick gets on a mixing desk: TRANSIENT (attack/sustain),
DRIVE (four Battlestar engines + COLOUR), COMP (+SPEED), OUT (LEVEL, VELO, KEY).
**21 parameters.** The 8- and 12-bit sample machines on the MusicRadar list
(LinnDrum, DMX, Linn 9000, SP-12, MPC60) are what made GRIT earn its place.

## 2. The engine (engine/ks_engine.*, JUCE-free)

Voice at 4× the host rate → GRIT → TRANSIENT → ROOM → DRIVE → COLOUR → two
63-tap half-bands → COMP (host rate, 1.5 ms look-ahead) → LEVEL → soft ceiling.

- **Body**: one phase, sine/triangle/tanh-square morph; pitch
  `f = PITCH · 2^((SWEEP·e^(−t/BEND) + 2.5·SKIN·vel·ae(t)) / 12)`; amplitude
  `ae = exp(−6.9 · (t/DECAY)^(1+7·CURVE))` — every CURVE reaches −60 dB at DECAY,
  CURVE only moves where the energy sits.
- **SKIN**: seven circular-membrane modes at the Bessel-zero ratios
  (1.593 2.136 2.295 2.653 2.917 3.156 3.500), each decaying faster the higher it
  is, plus the tension glide (a real head is sharp while loud).
- **CLICK**: a tuned burst (sine → cosine start as TONE rises) plus band-passed
  noise, 180 Hz–11.5 kHz, 14 → 2.3 ms, most noise mid-range (the 909).
- **TRANSIENT** reads the voices' own envelopes (SPL's two differences on exact
  envelopes), so it cannot ripple on a 40 Hz kick.
- **Drive**: IDLE BURN, HYPERDRIVE, RAZOR WING, SUPERNOVA, ported from Battlestar;
  trim table MEASURED at kick levels (0.2/0.45/0.8); first-order **ADAA** on the
  three smooth engines (SUPERNOVA quantises on purpose and runs plain).
- **Comp**: sliding-max detector over look-ahead + 10 ms hold, soft knee,
  threshold/ratio on one knob, make-up = 0.7 of the reduction at −6 dBFS.
- 2-voice choke (3 ms) on retrigger. Latency 23 + 1.5 ms = 95 samples at 48 k,
  constant whatever the settings.

## 3. Measured (test/bench.cpp, 52 checks; hosttest 47; shoteditor 4)

Tuning worst 0.04 c (4 pitches × 3 rates) · sweep start 99.5 Hz for 104 wanted in
the first 30 ms of a slow bend · DECAY within 10 % over 3 curves × 3 lengths ·
latency measured 95.00 by phase · retrigger step 0.0110 vs 0.0123 for the hits
apart · SKIN mode +23 dB, glide +1.9 st · WAVE 3rd harmonic −151 / −19 / −11 dB ·
click centroid 162 / 1659 / 4759 Hz · ATTACK ±, SUSTAIN ± each ≥ 4 dB, smooth-gain
residual −79 dB · ROOM +43 dB of tail · drives level-matched within 2.4 dB ·
HYPERDRIVE +47 dB 2nd harmonic · **aliasing −90 dB** (square through RAZOR WING at
100 %, 52 Hz) and −86 dB at 416 Hz · GRIT +45 dB off-harmonic · COLOUR −17 dB at
6 kHz · COMP head-to-tail 21.5 → 9.6 dB · velocity 13.6 dB, VELO 0 memcmp-identical
· 31 presets + 300 random kicks bounded · ~4 % of one core with everything on.
Presets levelled BY MEASUREMENT (tools/level-presets.js): −10 dB RMS over the first
150 ms, no clean kick above −1 dBFS; the acoustic bank sits 1–5 dB under, peak-bound
by the beater.

## 4. What the build taught

- **A DC blocker on a kick is a flaw, not hygiene.** A hit that starts at zero
  phase carries DC by nature; a 2 Hz high-pass turned it into a subsonic hump
  ~30 dB down that outlived the kick by a second and made DECAY read twice as
  long as it is. Removed.
- **A compressor without look-ahead on a kick is a peak generator**: the onset
  sails through with the full make-up gain into the ceiling. 1.5 ms of look-ahead
  at a constant latency fixed it. And crest factor is the wrong measure of a kick
  compressor; the head-to-tail span is what it changes.
- **ADAA took the hard-clip aliasing from −62.5 to −90.2 dB** at the same 4×.
- **Smoothed controls must snap on the first block**, or a preset loaded before
  the first audio ramps in on the first hit (the bench's COLOUR check caught it).
- **Fixing the compressor moved the latency, and two probes then read the pre-ring
  before the onset.** Any probe window must start after the latency.
- **More compression made the acoustic presets QUIETER** under a peak-bound
  levelling rule: a slow attack lets the beater through and the make-up then
  lifts it into the limit. Fast SPEED catches it inside the look-ahead.
- The bash heredoc ate backslashes three more times (a patch script, the fleet
  installer entry — where JS turned `\b` into a real backspace). Files via the
  Write tool.
