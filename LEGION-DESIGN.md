# LEGION — design

*A Brokild vocal processor. Written 2026-09-16 from Peter's brief: a harmoniser
for singing AND screaming, pitch shifted with INDEPENDENT formant control, so a
second voice or a whole choir can be built out of the main voice and still sound
like a body that could have made it. Wet/dry mix. BWFX on the second voice.
Simple VST3 for now; aesthetics later.*

*STATUS 2026-09-16: ENGINE BUILT AND MEASURED, PLUGIN WRITTEN, NOT YET HEARD.
107 engine checks and 22 wrapper checks ALL CLEAR on Linux, and the VST3 links
and loads there; it has not been run in a DAW and no human has listened to it. Everything below marked "measured" comes from
`vocal-harmonizer/test/bench.cpp`, which anyone can rebuild and rerun in about
ten seconds. Everything marked "expected" is a claim waiting for Peter's ears.*

*The name is provisional — "my name is Legion, for we are many" (Mark 5:9), and
it is one line in `CMakeLists.txt` plus one in `Params.h` to change.*

---

## 0. The one-line thesis

**A voice is a source and a body. Shift them separately, or you have a
chipmunk.**

Play a vocal back faster and everything moves: the pitch, and the resonances of
the throat and mouth that say how big the singer is. The ear reads the second as
body size, so a voice shifted that way does not sound like the same person
singing higher — it sounds like a smaller person, or a cartoon. Every natural-
sounding shift in existence separates the two and moves only one. LEGION's
entire architecture is that separation, and both halves of it are knobs.

## 1. What is refused

- **No pitch tracking in the signal path.** A harmoniser that has to know the
  note cannot survive a scream, and a scream is half the brief. The shift is a
  RATIO applied to whatever is there — periodic, rough, chaotic or noise — so
  the engine never has to be right about the pitch in order to work. (An f0
  estimate does exist, and is used for one thing only: sizing the envelope
  lifter, §3. When it is wrong the envelope gets slightly coarser, and nothing
  else happens.)
- **No resampling.** That is the chipmunk, and it is available deliberately as
  the FOLLOW knob at 100 %, not as the default.
- **No "formant correction" bolted on after the fact.** Shifting everything and
  then filtering it back is two lossy passes and the formants never quite land.
  Source and body are separated ONCE, moved independently, and multiplied back
  together in the same frame.
- **No MIDI, no scale, no key.** v1 shifts by fixed intervals. Diatonic
  harmony needs a pitch tracker to be right about the note, which is the thing
  §1 just refused for the signal path; when it comes, it comes as a control
  layer on top, and screaming still works without it.

## 2. The whole plugin in one line

For each harmony voice, in every analysis frame:

    MAGNITUDE_out(f) = SOURCE(f / pitch) * TRACT(f / formant)

`SOURCE` is the glottis and the breath and the grit — whatever is exciting the
throat. `TRACT` is the resonance the throat imposes on it: the formants, the
body size, the vowel. Read the source at one rate and the tract at another and
they cannot interfere:

| pitch | formant | what comes out |
|---|---|---|
| 1 | 1 | the input, bit for bit (measured: **-93.9 dB** error) |
| 2 | 1 | an octave up, same body — the same singer, higher |
| 1 | 2 | same note, half the singer — a child's throat on an adult's pitch |
| 2 | 2 | resampling, the chipmunk — reachable, on purpose, via FOLLOW |

The FOLLOW knob is `formant = pitch^follow * 2^(offset/12)`: 0 keeps the body
still, 1 is plain resampling, and the interesting settings are in between —
a fifth up with FOLLOW around 30 % reads as a genuinely different, slightly
smaller singer rather than as the same person or as a cartoon.

## 3. Finding the body: the true envelope

The tract is a smooth curve through the tops of the harmonics. The obvious way
to find it — smooth the log spectrum (a cepstral lifter) — lands the curve in
the MIDDLE of the harmonics, not on top of them, and a curve that sits in the
middle drags the formants along with the pitch. So: the **true envelope**
(Röbel & Rodet 2005) — lifter, then take the maximum against the log spectrum,
then lifter again, six times or until nothing pokes more than 1 dB out. The
result hugs the peaks, which is what makes the two knobs independent at all.

The lifter is cut at `0.40 * fs / f0`, safely below the quefrency where the
harmonic comb lives, so the "envelope" can never start following individual
harmonics. That factor was swept: 0.40 is the best of 0.40 / 0.50 / 0.60.
Unvoiced frames — screams, breath, consonants — have no comb to avoid and get
the value for a 220 Hz voice; the max-iteration is stable without one.

**The one real limit, measured.** The tract can only be known where the voice
put a harmonic, and a high voice puts them far apart. The bench sweeps it:

| input f0 | formants move on a +7 st shift |
|---|---|
| 90 Hz | +0.03 st |
| 120 Hz | −0.04 st |
| 165 Hz | −0.01 st |
| 240 Hz | −0.16 st |
| 330 Hz | −0.29 st |

A third of a semitone at the top of a soprano's range, and the PITCH is exact
throughout — it is a colour error, not a tuning one. Every pitch shifter that
separates source from filter has this limit. It is in the bench so a regression
cannot hide in it.

## 4. Moving the source: peaks, not spectra

Pitch shifting in a phase vocoder is usually done by reading bin `k/pitch` for
every output bin — resampling the spectrum. That widens every main lobe by the
pitch ratio, and a widened lobe is not what the window makes of a sinusoid any
more. LEGION instead **translates** each spectral peak, with the whole region of
bins it owns, to `pitch * (its true frequency)`, keeping the lobe the window's
own shape.

Both were built and measured on the bench's shifted-tone test (a sine in must be
a sine out, one line, at the new frequency):

| variant | worst spur | worst single case |
|---|---|---|
| peak translation | **−39.0 dB** | 440 Hz +7 st: −45.4 dB |
| spectrum resampling | −36.4 dB | 440 Hz +7 st: −36.4 dB |

The translation is by a FRACTIONAL number of bins — whole part as an index
offset, remainder as a linear interpolation across the complex spectrum. Half a
bin is 23 Hz on the short window, and a lobe sitting half a bin from the
frequency its own phase accumulator is advancing at is an amplitude wobble;
fixing it is worth 1 to 3 dB of spur.

Two details that are not optional, both found by measurement:

- **The tract is applied as one scalar per peak**, the envelope where the
  harmonic is going over the envelope where it came from — not bin by bin. Bin
  by bin divides a lobe's tails by an envelope that has collapsed away from the
  peak; the bench measures the result as a spur **14 dB ABOVE the signal** on an
  isolated 1 kHz tone.
- **The analyser sign-flips every odd bin.** That un-does the half-window time
  offset and makes each main lobe a smooth complex function. Without it, the
  sub-bin interpolation above would be interpolating across a zero crossing.

## 5. Keeping it a voice: phase

Rotating each bin independently is what makes a phase vocoder sound like a
flanged whisper. LEGION locks phase across each peak's region — Laroche &
Dolson identity locking — so every bin keeps its original phase relationship to
its peak, and only the peak itself carries an accumulator.

What the accumulator holds is the **excess over the input's own phase**, not the
output phase. Both are the same mathematics (the excess grows by
`hop*(pitch-1)*ω` where the absolute phase grows by `hop*pitch*ω`), but the
excess is re-based on the input every frame, so a peak that wanders a bin
between frames cannot leave a constant phase offset behind. That wander is real
and happens constantly on a held vowel: with the absolute form the bench
measured unity transparency at **−11 dB**; with the excess form, **−93.9 dB**.
At pitch 1 the excess is identically zero and the output IS the input.

On a transient — spectral flux above 55 % of the previous frame's own size — the
accumulators are dropped and the input's own phase is used, which is what keeps
a consonant a consonant. Measured on a click: **−148 dB** of energy outside
±2 ms of where the click belongs.

## 6. The window, and the overlap

Hann, used for both analysis and synthesis, at **87.5 % overlap** (hop N/8).
Three sizes, switchable:

| DETAIL | window @48 k | latency | lowest f0 it can resolve |
|---|---|---|---|
| TIGHT | 1024 | 21.4 ms | 187 Hz |
| NATURAL | 2048 | 42.7 ms | 94 Hz |
| SMOOTH | 4096 | 85.4 ms | 47 Hz |

A window can only separate harmonics further apart than its own main lobe, and
Hann's is 4 bins wide — that is the last column, and it is why the switch
exists rather than a bug to fix. TIGHT is for high voices and consonant-heavy
material, SMOOTH for a low male voice or a growl. The window duration is held
roughly constant across sample rates (the base size doubles above 64 k and
again above 128 k), so DETAIL means the same thing at 96 k as at 44.1 k.

The overlap was the other measured choice. 75 % — the usual — puts the worst
shifted-tone spur at −32 dB; 87.5 % puts it at −39 dB, for about 7 % more of a
core on four voices. Seven decibels of artefact is the difference between a
second voice you can bury and one you can solo, so the CPU is spent.

**Rejected, and recorded so it is not tried again:** re-estimating the envelope
only every other hop saves 4 % of a core and costs 5 dB of purity, because a
tract gain that alternates between fresh and stale is amplitude modulation at
half the frame rate.

## 7. Four voices, one analysis

The expensive half of the work — transform, true envelope, f0, peak picking —
depends only on the INPUT, so it happens once per hop and every voice re-draws
from it. Four-part harmony costs about twice one voice, not four times.
Measured: **five to six times real time for four voices**, about 17 % of one
core at 48 k.

Per voice: PITCH (±24 st), FINE (±100 ct), FORMANT (±12 st), FOLLOW (0–100 %),
LEVEL, PAN, DELAY (0–120 ms).

**HUMANISE** is the difference between four voices and one voice that got
louder. Per voice, uncorrelated, refreshed once a frame and one-pole smoothed to
about a 1 Hz wander — the rate a singer's own tuning drifts at, not an LFO:
detune (±14 ct at full), level shimmer (±1.3 dB), a touch of tract jitter
(±0.35 st), and a fixed per-voice timing stagger (0 / 13 / 23 / 7 ms) on top of
whatever the DELAY knobs say. Measured: it takes the choir's L/R correlation
from mono down to 0.62.

## 8. The two buses, and where BWFX sits

The engine hands back the HARMONY bus and the DRY bus delayed by exactly the
analysis latency. Anything else and the second voice arrives 43 ms early
relative to the singer, which reads as a slap, not a choir. MIX crossfades them
equal-power; at MIX 0 the output is the input, delayed and otherwise untouched.

**BWFX defaults to the HARMONY bus, before the mix.** That is Peter's ask read
literally: put the rack on the second voice, grind it, drown it, gate it, and
leave the lead alone. `BWFX ON` switches it to MASTER for when the whole thing
should go through the pedals.

The integration is the standard four calls from BWFX-DESIGN.md: `prepare` in
`prepareToPlay`, `process` on the chosen bus, `service()` from a 15 Hz timer
that runs with the editor CLOSED, and the blob saved inside plugin state. The
five BWFX macros are host parameters via `bwfx_juce::addMacroParameters`. The
rack panel here is GENERATED from `bwfx::moduleDescriptor()` — a module added to
BWFX appears in LEGION on the next rebuild with no line of LEGION changing. It
is a plain native panel, not the shared `ui/bwfx-rack.js` overlay, because this
build has no web UI; when LEGION gets its face, it gets the globe and the teal
and the real overlay (BWFX-DESIGN.md §5), and the DSP side does not move.

The world-modulation bus is NOT consumed (`setWorldModConsumed` is left false),
so the SPECTRA rack correctly shows its "arriving" plate. Audio characters work
regardless; only the `tick()` half needs a mapping.

## 9. What the bench measures

`vocal-harmonizer/test/bench.cpp`, 107 checks, no JUCE, no audio device:

- the transform round-trips and a tone lands on one bin;
- Hann² overlap-adds to exactly 1 at the hop the engine uses;
- **the contract**: every voice off, and the harmony bus is EXACTLY zero while
  the dry bus is bit-identical to the input, delayed by the reported latency;
- silence in, silence out;
- unity transparency, −93.9 dB;
- pitch accuracy to better than 1 % at −12, −5, +4, +7 and +12 st;
- formants stay within 0.3 st under every one of those shifts;
- FORMANT moves the formants by what it says (±0.05 st at +6, −6, +3, −10, +12)
  and does not move f0;
- FOLLOW 1 is plain resampling, and is measured collapsing onto it;
- shifted-tone purity, nine cases, worst spur −39 dB;
- screams (jittered period, subharmonic, noise, hard clipping) stay bounded and
  finite at twelve pitch/formant combinations;
- a click comes out where the latency says, with −148 dB of smear;
- the output is byte-identical at block size 64 and 256;
- all three windows agree with each other on one voice all three can resolve,
  and TIGHT is measured FAILING below its own documented floor;
- the envelope limit of §3, swept across input pitch;
- four humanised voices: bounded, audible, and not mono;
- the cost.

And `vocal-harmonizer/test/hosttest.cpp`, 22 checks, which run the REAL wrapper
with no DAW, no audio device and no window — the half a DSP bench cannot reach:
the parameter table is complete and has no duplicate id; MIX 0 is the input
delayed by the reported latency and bit-identical; every DETAIL switch changes
the reported latency and still makes sound; **BWFX on HARMONY changes the
harmony by 0.372 and the dry bus by exactly zero**, and on MASTER changes the
output; parameters AND the rack blob survive a state round trip into a fresh
instance; and mono-in/stereo-out, stereo/stereo and mono/mono are accepted and
run while stereo-in/mono-out is refused.

The formant meter deserves its own note, because it was wrong first and the
engine took the blame. Cepstral smoothing of the output spectrum reads a
9-semitone formant shift where the two spectra differ by almost nothing, once
the harmonics are far enough apart that the lifter has to be very short. The
meter now samples the spectrum AT THE HARMONICS — the only frequencies where
the tract was ever measured — lays them on a log-frequency grid and finds the
translation that lines the two up. It is self-tested against a vowel whose
formants were moved by a known 7.02 st.

## 10. What is not there yet

- **Nobody has heard it.** The engine is measured, not auditioned. Everything in
  §2's right-hand column is a prediction until Peter puts a scream through it.
- **No scale or key**, §1. Fixed intervals only.
- **No face.** A plain JUCE panel, on purpose.
- **The rack panel is native, not the shared overlay**, §8.
- **No landing page, no manual, no release zip** — those need a Windows build,
  which needs the plugin to have been heard first.
- **The pitch knob is stepped in semitones and not automatable smoothly across a
  glide** — it is, but a large jump re-bases the phase accumulators rather than
  gliding. Fine for setting up a harmony, not a portamento instrument.

## 11. Prior art, so it is not mistaken for either

The **phase vocoder** (Flanagan & Golden 1966; Portnoff 1976) and its
peak-locking repair (Laroche & Dolson 1999) are the frame. The **true envelope**
(Röbel & Rodet 2005) is the tract estimator. Separating source from filter and
moving them independently is what **PSOLA** (Moulines & Charpentier 1990) does
in the time domain and what every serious vocal shifter does somehow; LEGION
does it in the frequency domain because the time-domain version needs pitch
marks, and pitch marks need a pitch, and §1 refused that.

What is particular here is not any one of those but the combination and where
the effort went: source-filter separation done ONCE inside a single frame,
peaks translated rather than spectra resampled, phase accumulated as an excess
over the input rather than absolutely, and a bench that argues with every one of
those choices in decibels.
