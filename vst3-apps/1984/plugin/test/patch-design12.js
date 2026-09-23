"use strict";
const fs = require("fs");
const p = "C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/1984-DESIGN.md";
let s = fs.readFileSync(p, "utf8");
const anchor = "## 12. What the build taught\n\n*(filled in as the bench answers)*\n";
if (s.split(anchor).length !== 2) { console.error("anchor miss"); process.exit(1); }
const sec = [
"## 12. What the build taught",
"",
"Bench `test/bench.cpp`, 86 checks ALL CLEAR at 48 kHz (also 44.1 and 96), 2026-09-22.",
"",
"- **Cost**: eight voices with every stage on, 2.5 % of one core at 1x, 4.6 % at 2x, 10.4 % at 4x.",
"- **Tuning**: both ranks within 1 cent at every footage; rank II at +25 cents lands on +25.0.",
"- **Aliasing** (saw at A6 = 1760 Hz, worst non-harmonic component below 20 kHz): under -40 dB at 1x, under -60 dB at 2x; a 5 % pulse at 2x under -50 dB; FUZZ at full drive at 2x under -36 dB. The decimator's own transition band sits above 20 kHz, which is why the audible band is what is measured.",
"- **Filters**: the SVF self-oscillates at 1000.0 Hz for a 1 kHz cutoff, the ladder at 996.5 Hz (0.35 %); both bounded. Resonance at 80 % puts the cutoff 14.5 dB over the octave above.",
"- **The IL/AL shape held only once the gate used the patch's own IL**: the envelope was gated with the level of the previous block and charged from zero instead of from IL, and the first bench run read 2182 Hz where 27 Hz was due. A gate must not read a level that set() has not delivered yet.",
"- **Voice mode at note-on**: the mode and the glide were derived at the top of process(), so a note-on arriving before the first block was allocated as POLY whatever the knob said; UNISON read \"one voice\". Derive at note-on too.",
"- **An asymmetric clipper with unequal tops is DC plus odd harmonics.** The first VALVE scaled its negative half by 0.78 and measured a second harmonic of -33 dB, the same as a clean sine. Even harmonics need an even term in the curve; b + 0.45 b^2/(1+|b|) is even, monotonic (slope never below 0.55) and measures -16 dB at 30 % drive.",
"- **A linear SVF with k > 0 only rings.** True self-oscillation needs the loop marginally unstable and the state limiter to hold it: the last five per cent of RESONANCE take k to -0.06.",
"- **Three ensemble taps of a modulated sine partly cancel** (-4.3 dB at full mix); the wet is normalised by taps^-0.75, between the amplitude and the power sum.",
"- **Tape hiss is gated on the signal follower** (5 ms up, 0.6 s down, exact zero below 1e-3), so an idle instrument is exactly silent and a synth on a tape still hisses while it plays. HISS 100 % / AGE 100 % measures -57 dBFS in the second after a note.",
"- **Tape wow** at 100 %, 0.5 Hz: 2.15 % peak-to-peak (the sine part alone predicts 1.1; the slewed walk adds the rest). Flutter at 100 %: 0.35 % p-p. With both at zero the pitch moves 0.0003 %.",
"- **The hall's RT60** measures 1.68 s for a 2 s setting; at maximum decay, size, shimmer and motion the tail is 97 dB lower 40 s after the note than in its first seconds: bounded AND falling.",
"- **Probes that lied before the code did** (the recurring class): a zero-crossing pitch estimator on a two-tap chorus (46 % \"wobble\" from cancellation nulls); a pulse source with a spectral null exactly at the formant under test (a 68 % pulse nulls its 15th harmonic, 1650 Hz); a pluck measured after it had decayed; a glide test reading voice 0 in a POLY allocation; a VINTAGE test on the one voice whose tolerance happened to be small; a sustain test reading past the end of its own buffer. Each was rewritten against the physics; none was loosened.",
""].join("\n");
s = s.replace(anchor, sec);
fs.writeFileSync(p, s);
console.log("section 12 written");
