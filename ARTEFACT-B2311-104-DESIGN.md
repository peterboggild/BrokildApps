# ARTEFACT B2311.104 — design

*Proxims Centauri B · the newest finding. Accession 104. Written 2026-09-02,
before the first line of engine code, in the house manner: the claims are made
here, and `test/bench.cpp` measures every one of them against the real engine.*

---

## 1. What was found

A web. Not a body, not a lattice, not a crystal — a NETWORK: long slender
conduits meeting at junctions, and the whole of it lying on a curved surface
that our three dimensions cut through, exactly as the hand meets B2311.22 and
B2311.67 through a moving section. What confounded the survey is that it shares
almost nothing with the other three accessions. No counting. No connectedness
made audible. No forbidden order. What it does, continuously and without being
asked, is MOVE ENERGY — and the moving of it is a sound, and the sound is deep.

The working hypothesis — held loosely, and the survey says so — is that this
is a DISTRIBUTION device: that its builders moved power as low-frequency
acoustic waves along these conduits, the way we move it as alternating current,
and that what we hear is their grid, still carrying. The counter-argument is
audible to anyone who listens for an hour: no power engineer tolerates this
much beauty in a bus bar. Perhaps for them the two were never different things.

The survey names the suspected builders the SONORIANS, in want of better.

## 2. The refusals (what this artefact will not reuse)

Stated first because the siblings each own their mechanism:

- **No eigen-solve, no modal or additive synthesis.** B2311.22 is the graph
  whose sound is its Laplacian spectrum. Nothing here decomposes anything into
  modes; the engine is time-domain from end to end.
- **No counting, no firing moments.** That is B2311.1.
- **No quasiperiodic media, no singular continuous spectra.** That is B2311.67.
- **No metaballs, no creature, no second listening body.**
- **No oscillator as a commanded object.** There is no phase accumulator in
  this instrument that produces a waveform. Where oscillation occurs, it is a
  LIMIT CYCLE: a feedback loop whose gain has been carried across unity by a
  physical condition, starting from a real transient, saturating on its own
  nonlinearity. Below the condition the same loop is a resonator that only
  decays. The bench proves both halves.

## 3. The thesis: sound as energy transport

The mechanism is real physics, chosen because it makes the survey's hypothesis
literally plausible: **thermoacoustics**. On Earth, a duct with a temperature
gradient across a stack will, past a critical gradient, spontaneously sing at
the duct's own resonance — the Rijke tube, the thermoacoustic engine. Heat in,
deep acoustic power out. It is the one known way sound IS a power grid.

Three consequences structure the whole instrument:

1. **Onset.** A conduit is silent until the disequilibrium across its cell
   crosses a threshold. Below it, struck, it rings and dies. Above it, it
   sings from nothing, growing to a saturation amplitude. ONSET is a control.

2. **Temperature is pitch.** The speed of sound goes as sqrt(T), and a duct's
   resonance is c/2L. So the way to command a pitch is to command a
   temperature — and that is what a MIDI note is here: a thermal order.
   The conduit heats toward the temperature whose resonance is the note;
   attack is heating rate (FLUX), portamento is thermal inertia (MASS),
   release is cooling (LEAK). A cold artefact is sluggish and dark because
   heating it takes longer. Every glide is physics, not an envelope.

3. **Deep by construction.** Long conduits. The catalogue is generated so that
   passive resonances span roughly 24–260 Hz — the artefact lives where bass
   lives, and its harmonics are made by wave steepening (STEEPENING), the way
   a loud wave in a real resonator leans forward and brightens.

## 4. The geometry: a web on the 3-sphere

The conduits lie on S³ — the curved 4D surface — as geodesic arcs between
seeded junction nodes (34–46 nodes, ~3 nearest neighbours each, capped at 72
conduits, connectivity enforced). A conduit's LENGTH is its geodesic angle,
and length is pitch: f_passive = scale / angle, clipped into the bass range.

The panel is a **stereographic projection** of the rotated 3-sphere: every
geodesic arc projects to a circular arc, and a slow isoclinic double-rotation
(PRECESSION) turns the web through itself continuously — the silhouette is
never twice the same and no 3D object could cast it. The mouse wheel pushes
the SECTION — the slice of the 4th coordinate that is acoustically present —
exactly as on .22 and .67. VEIL is the slice's thickness. A conduit outside
the section keeps its HEAT (thermal memory: come back later and it remembers)
but does not reach the ear.

Distinctness and alienness are bench numbers, as in .22: no specimen's pitch
set may sit within 18 cents RMS of any single harmonic series, nor within 15
cents RMS of the 12-TET grid; the generator re-salts deterministically until
the margin clears (cap 80). 128 specimens, all pairs distinct, a number is a
specimen for good.

## 5. The engine (time domain, all of it)

Per conduit: one delay loop (round trip = the resonance), cubic Catmull-Rom
fractional tap, DC blocker, one-pole loss filter, thermoacoustic gain element,
cubic soft saturation (bounded by construction; only low-order harmonics per
pass, which is what keeps aliasing down without oversampling — measured, see
bench). End types are seeded per conduit: open-open (all harmonics) or
closed-open (negative feedback, odd harmonics, quarter-wave — a hollower
voice at half the loop length). The tuning formula subtracts the measured
phase delay of the loop filters at the target frequency, so the steady state
is exact; the bench holds it to a few cents.

**Gain = disequilibrium.** Two sources add: the DEMAND of a held note (the
envelope a command opens — velocity overdrives it), and THERMAL EXCESS beyond
onset (|log(T/T_ambient)| past the ONSET threshold). The second is what makes
the grid able to sing on its own: TRAFFIC injects wandering heat packets, and
if they cross onset — hot ambient, low ONSET, busy TRAFFIC — distant conduits
carry without being played. At the defaults they do not, and the bench asserts
a fresh instance renders EXACT zero.

**Sub-onset chuffing.** Approaching onset, a real thermoacoustic engine
stammers — discrete relaxation thumps that accelerate as it spools up. CHUFF
is that: deterministic timed pulses into the loop, each ringing the conduit's
own resonance, quickening as the demand builds, gone once the song holds.
A soft strike at a cold ambient is a rhythm before it is a note.

**The web is coupled twice.** Heat CONDUCTS between conduits sharing a
junction (slow — playing one note warms its neighbours, retunes them, can
carry them over onset: sympathetic song). Sound BLEEDS across junctions
(fast — JUNCTION BLEED, one-sample-delayed, stability bench-checked).
Energy transfer is not a metaphor in this instrument; it is the signal path.

**Stereo is the geometry.** A conduit's pan is the projected screen position
of its midpoint under the same rotation the panel draws — as the body
precesses, the image and the stereo field turn together, one source of truth.

Output: sum of in-section conduits → DC blocker → fixed 16 Hz subsonic guard
→ hard-knee ceiling (identity below 0.85, tanh above — the B2311.1 lesson,
kept). No noise is injected anywhere in any loop (the house rule).

**BWFX**: full rack after the engine, five macros (declared AND pushed —
`pushMacros` every block; see the .1 buglist), and the SPECTRA world-mod bus
CONSUMED: detune fanned golden-angle across conduits, sag keyed to the
smoothed gate, trem on radiation with fanned phases, filterMul on loop loss,
all behind a neutrality guard so an untouched rack is memcmp-identical.

## 6. Temperature, and the operating point

AMBIENT runs 2.7 K to 1216 K, default **234 K** — the equilibrium temperature
of Proxima Centauri b. Ambient retunes the whole passive grid as sqrt(T)
(commanded notes stay in tune — the servo aims at absolute pitch; what moves
is everything you did NOT command), scales the loss filter (cold = dark),
and scales heating rates (cold = slow to obey). At 2.7 K the web barely
moves and a command takes seconds to arrive. Near 234 K — and only there —
the traffic falls into a repeating seven-stroke figure. The survey noticed;
the findings report records it as an oddly stable operating point and
declines to speculate further.

## 7. The panel

Full-bleed, computed, nothing drawn by hand: the projected web on near-black.
Heat glows VIOLET (not blackbody — this is not our chemistry); acoustic power
runs along a singing conduit as AMBER pulses in the direction of transfer —
energy at rest and energy moving are two different lights. Out-of-section
conduits are ghost-thin. Junctions bloom when a packet passes. The point at
infinity — the projection pole — is drawn as a small void; **triple-click it**
and the survey's pencilled field annotations appear over the controls (the
Escape Room precedent: the legend is earned, not offered).

Controls are deliberately non-human: a rail of unlabeled capillary gauges,
each marked only with a procedural glyph; the ledger strip translates while
you touch one. Wheel = SECTION, shift-wheel = VEIL, drag empty space =
double-rotate the projection, drag a conduit = pour heat into it (it
glissandos, and may cross onset), tap one = strike it (it rings — a touched
thing always answers, the .22 lesson). DAW automation names are honest
English; only the panel refuses to explain itself.

## 8. The bench (what must be measured, before shipping)

Catalogue: build all 128; connected; pitch span ≥ 3 octaves inside 20–300 Hz;
all-pairs distinct; harmonic-series floor ≥ 18 c RMS; 12-TET floor ≥ 15 c.
Silence: defaults, no notes, 10 s → EXACT zero. Determinism: memcmp.
Onset both ways: high ONSET + hot + full TRAFFIC still silent when it should
be; low ONSET + hot self-sings above −50 dB. A struck conduit below onset
decays; above, grows from the same strike.
Pitch: commanded notes within a few cents after settle, across specimens and
sample rates; TUNE +12 doubles; bend ±2 st; ambient shift 234→936 K doubles
the PASSIVE ring pitch (sqrt law) while a HELD note stays put.
Thermal: time-to-pitch grows with MASS (ratio > 3); release tracks LEAK;
RETENTION 1 still sings 5 s after release; conduction measurably warms a
neighbour and crosstalk feeds it (factor > 3 vs off).
Chuff: discrete quickening thumps at low velocity, none at CHUFF 0.
Steepening: harmonic growth with STEEPENING and with velocity (> 10 dB span);
aliasing floor at worst settings better than −55 dB below the fundamental.
Stability: 300 random machines × random notes, bounded, finite, DC < 1e-4.
Neutral world-mod bus → byte-identical. CPU with FTZ, % of one core, stated.

The vacuous-window rule from .1/.67 applies to every check: a window that
cannot fail proves nothing; every threshold above was chosen to be failable.

## 9. Ship list

Tree `C:\Users\peter\b\ArtefactB2311_104`, own git repo. PLUGIN_CODE `A104`
(Ab01/Ab22/Ab67 taken; three digits do not fit the Ab pattern), PRODUCT_NAME
"Artefact B2311.104", IS_SYNTH, VST3 + Standalone, build id `AB104_BUILD_ID`.
Fleet installer + check-names entries; installed to both houses under
"Proxima Centauri B findings"; SAC probed at the installed path.
Landing page section + recorded passages (mp3) + findings report section +
regenerated PDF + zip verified by loading the plug-in out of the archive +
app.json copy update ("4 of 4 recovered"). No file patches (the .1 precedent:
the specimen dial and the DAW project are the patch).
