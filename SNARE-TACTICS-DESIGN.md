# SNARE TACTICS — the snare drum synthesiser

Built 2026-09-26 from Peter's brief: *"a VST3 like Kickstart ... with similar
attention to the variety and key parameters for snare synthesis ... acoustic
sounding, modern compressed and authentic drum machine snares. Find out what
are the most important parameters for state of the art snare synthesis ...
versatility for electronic, dub, industrial, digital hardcore and techno ...
match the Kickstart in style and layout, but call it Snare Tactics."*

Source `vst3-apps/snare-tactics/plugin/` (in the website repo, the Kickstart
shape), built to `C:\Users\peter\b\_build\SnareTactics\plugin`. Plugin code
**`Snrt`** (checked unique across the fleet), PRODUCT_NAME **Snare Tactics**,
VST3 + Standalone, Instrument/Drum, build id `ST_BUILD_ID` in CMakeLists.
Patches in `Documents\Brokild patches\Snare Tactics`. **No BWFX**, same call
as Kickstart: the space it needs (room, gated room, dub echo) is built in.

## 1. What a snare actually is (the research, condensed)

A snare is three sound sources that are physically coupled, and every
classic synthesised snare is a simplification of that coupling:

1. **The batter head** — a circular membrane. Its partials sit at the Bessel
   zero ratios 1, 1.59, 2.14, 2.30, 2.65, 2.92, 3.16, 3.50, 3.60 ... and WHERE
   it is struck decides which ones speak: a centre hit excites only the
   axisymmetric (0,n) modes, an off-centre hit brings in the (m,n) modes with
   m > 0 — the overtone "ring" engineers damp with gel or tape. A hard hit
   raises the head's tension, so the pitch starts sharp and falls. The 808 and
   909 replace the membrane with two oscillators near 1 : 1.8 (bridged-T
   resonators on the 808, triangles with a pitch envelope on the 909).
2. **The snare wires** — steel coils on the resonant head. They are driven by
   the resonant head's motion, so they (a) start a fraction of a millisecond
   AFTER the stick lands, (b) buzz AT the head's frequency when loose (they
   slap against the head once per cycle), and (c) keep sizzling
   sympathetically for as long as the head rings. Tight wires are short,
   bright and crisp; loose wires are long, dark and rattly. Every drum
   machine models them as filtered noise with its own decay (the 808's
   SNAPPY); what the noise-only model misses is the buzz and the lag.
3. **The stick and the rim** — the stick's own click, and on a rim shot the
   shell and hoop ring as well: a set of short, bright, inharmonic
   resonances. A cross-stick is almost only that, with the head choked.

Then what the electronic genres added: the **hand clap** (the 808/909 clap is
noise through a band-pass, retriggered three or four times ~10 ms apart,
then a longer tail), the **gated room** (the 80s: a compressed, gated
ambience — the sound of industrial and of every big 80s snare), the **dub
echo** (a snare thrown into a tape delay that feeds back through a filter,
ping-ponging), and the sample-machine **grit** (SP-1200, Akai, the Amen).

## 2. The controls: axes, not simulations

Thirty controls in eight sections. As with Kickstart, the 808, the 909, a
studio snare and a gabber snare are points in the same space.

| section | controls | what it is |
|---|---|---|
| HEAD | TUNE, DROP, BEND, DECAY, WAVE, SKIN, RING | the tonal body: pitch, pitch fall and its speed, decay, the electronic pair's wave (sine→triangle→square), electronic pair ↔ membrane modes, head damping (RING) |
| WIRES | WIRES, SIZZLE, TENSION, AIR | the snare: level, decay, loose (dark, long, buzzing at the head's pitch, lagging, sympathetic) ↔ tight (bright, short, crisp), top end |
| HIT | STRIKE, STICK, CLAP, SPREAD | where it is struck (centre → edge → rim shot), the stick's click, the clap layer, the clap's burst spacing |
| ERA | GRIT, ROOM, GATE | sample-machine grit; a room; the 80s gate that cuts it |
| ECHO | ECHO, TIME | the dub echo: tape, filtered, ping-pong, synced to the host tempo |
| TRANSIENT | ATTACK, SUSTAIN | read from the voice's own envelopes (Kickstart's method) |
| DRIVE | ENGINE, DRIVE, COLOUR | four Battlestar Overdrive engines with ADAA, level-matched |
| COMP / OUT | COMP, SPEED, LEVEL, VELO, KEYS | look-ahead compressor; output; velocity; FIXED / GM KIT / CHROMATIC |

**Articulations** are part of the instrument, not separate patches: in GM KIT
mode note 38 is the snare, 40 a rim shot, 37 a cross-stick and 39 the clap,
each drawn from the same controls. The pad on the panel has a rim ring (rim
shot) and two buttons (X-STICK, CLAP).

## 3. The engine (engine/st_engine.*, JUCE-free)

Four voices at 4× → GRIT → TRANSIENT → ROOM (8-line FDN, pre-diffused) →
GATE (keyed by the hit itself, no threshold) → DRIVE → COLOUR → two 63-tap
half-bands → COMP (1.5 ms look-ahead) → ECHO (stereo ping-pong) → LEVEL →
soft ceiling. Mono up to the echo; the echo is where it becomes stereo.

31 parameters in ONE `SPECS[]` table (`engine/st_params.h`), 44 presets + Init
in seven banks (ACOUSTIC, MACHINES, TECHNO, DUB, INDUSTRIAL, HARDCORE, MODERN),
levelled by measurement (`tools/level-presets.js`: −15 dB RMS over 150 ms, no
clean peak above −1 dBFS). Voices: 4 audible, 6 slots, the oldest fades in 3 ms.
A small hit queue, so two notes on one sample (a flam, a GM chord) both land.

## 4. Measured (test/bench.cpp 72 checks; hosttest 72; shoteditor 12)

Tuning 0.000 c (4 notes × 3 rates), membrane (0,1) mode exact · DROP starts
where it says · CHROMATIC exact · DECAY within 5.2 %, SIZZLE within 1.5 % ·
latency 95 samples measured = reported · SKIN: pair partial −9.6 → −80 dB,
(0,2) mode −72 → −19 dB · STRIKE: (1,1) mode −74 → −15 dB centre→edge · RING:
+61 dB of overtone at 150–250 ms · TENSION: centroid 3.7 → 7.8 kHz, buzz at the
head's pitch −0.6 vs −45.9 dB, lag loose 3.6 / tight 0.4 ms, loose wires ring on
with the head (−30 vs −300 dB) · AIR tilt +26 dB · ghost note +7 dB wirier ·
rim shot +15 dB of shell, cross-stick −22 dB of wires · clap 1 burst / 4 bursts
by SPREAD · GATE: untouched before (−0.18 dB), −300 dB after, reopens · ECHO
ping-pongs, repeat at 375.4 ms (375) and 500.6 ms at 90 BPM (500), full ECHO
dies away · drives level-matched within 4.1 dB, aliasing −89.9 / −87.9 dB at
200 / 420 Hz through RAZOR WING 100 % · VELO 0 memcmp-identical · 45 presets +
300 random bounded · about 10 % of a core with everything on at 8 hits/s.
Host: GM KIT map, host tempo reaches the echo (a 90 BPM quarter at 667 ms),
mono without echo, a mono bus works, programs load exactly, state and patches
round-trip.

## 5. What the build taught

- **An aliasing probe needs a steady tone, and a snare has none.** The first
  reading was −59 dB; the spectrum showed pairs of lines 15 Hz either side of
  each harmonic — the harmonics of a hard clipper broadened by the tone's own
  DECAY, landing just outside the ±5-bin tolerance. 420 Hz measured *better*
  than 200 Hz, which real aliasing never does; that was the tell. Kickstart held
  its tone flat with CURVE; the snare got a bench-only tap (`benchSteady`)
  freezing the head's glide AND decay. Real figure −90 dB.
- **The electronic pair's 1 : 1.83 makes any "off-harmonic energy" metric
  useless**: intermodulation of two unrelated tones fills the spectrum. Measure
  aliasing on the membrane's single mode.
- **A noise source seeded per hit breaks a triangle-inequality click test**:
  the second hit of a pair is not the same noise as that hit played alone. The
  retrigger test runs on the deterministic head.
- **TENSION first moved only the high-pass, and AIR's low-pass dominated the
  centroid** (6.8 → 7.9 kHz, nearly nothing). Loose wires are darker, so tension
  scales the low-pass too: 3.7 → 7.8 kHz.
- **Noise normalised by bandwidth makes AIR a tilt**, not a level: the check
  is the top against the body of the noise.
- **A levelling pass converges slowly on ceiling-bound presets**: the soft
  ceiling reads 0.00 dBFS for any true overshoot, so each pass can take ~1 dB.
  Twelve passes; nothing now above −0.9 dBFS.
- **The display window sized from the −60 dB point is two-thirds empty** on a
  snare; three quarters of it plus a little room reads right.
- **The manual's cover mark `/\` rendered as the letter A** ("ASNARE TACTICS").
  No overflow gate can see it; only looking did. Replaced by an inline SVG of the
  panel's crossed sticks.
- **The collection pages had shipped Black Rider's `<title>` and description**
  since the page builder took Black Rider's whole `<head>` as its shell — every
  link preview called the collection a monosynth. Fixed in
  `build-collection-pages.js`: a page built from another page's shell replaces
  the head's identity.
- The bash heredoc ate backslashes four more times (sed patterns, a node
  anchor, a manual edit, a JS string). Files and anchors with backslashes go
  through the Edit/Write tools.
