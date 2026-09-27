"use strict";
const fs = require("fs");
const P = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/ARTEFACT-B2311-DESIGN.md";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : String.fromCharCode(10);
const BT = String.fromCharCode(96);

const A = "## 13. THE INTERLOCUTOR " + String.fromCharCode(8212) + " communication by spectra (SPEC, awaiting go)";
const B = "## 13. THE INTERLOCUTOR " + String.fromCharCode(8212) + " communication by spectra (BUILT 2026-08-30, 260830.1)";
if (s.split(A).length - 1 !== 1) { console.error("heading anchor not found"); process.exit(1); }
s = s.split(A).join(B);

const add = [
"",
"### 13.1 As built (all three layers, 260830.1)",
"Params: OTHER (catalog number, non-automatable), CONVERSE, COMMUNION,",
"PLASTICITY. CONVERSE 0 is bit-identical to the pre-interlocutor engine.",
"",
"* Every specimen has a fixed natural register, " + BT + "Specimen::voiceHz" + BT + " (58-195 Hz,",
"  from the catalog number alone). THIS is what makes kinship mean anything:",
"  a second body that simply followed your note would understand every pair",
"  equally well, and mutual intelligibility would be a tautology.",
"* " + BT + "spectralKinship(a,b)" + BT + " = the mean over b partials of a Gaussian proximity",
"  to a nearest partial (70 cents, the half-power width of the Q-25 ears).",
"* Listening = one 2-pole resonator per mode of the other body, unity peak",
"  gain, driven by the real audio. Kinship as the ENGINE hears it = the mean",
"  fraction of its own modes that actually rang.",
"* Reply = 0.55 x rank-translated speaker profile + 0.45 x what physically",
"  absorbed; loudness " + BT + "converse * 90 * sqrt(sentPow) * (0.30 + 0.70*kin)" + BT + ",",
"  capped. Fires after 0.18 s of your silence, or INTERJECTS after 5 s if you",
"  are still playing. (Keyed on elapsed time alone an interjection talks to",
"  an empty room for a minute after you stop, and worse, never lets the body",
"  fall quiet enough to heal - which is why no accent was ever applied.)",
"* Membrane = a true product, " + BT + "clamp(commune * 55 * spk * il, +-0.30)" + BT + ".",
"* Plasticity: " + BT + "scar[e] += rate * sum_k e_k |v_k(a) v_k(b)|" + BT + " every 8th control",
"  tick; applied on the message thread as bounded edge multipliers",
"  [0.40, 2.60] plus a full re-solve, PER BODY at that body own rest (the",
"  played one is busy only while a voice sounds, the listener only while it",
"  is speaking - one shared gate never opens). Saved on the APVTS state as",
"  accA/accB; forgotten when a body is dialled in by hand, kept when a",
"  session is restored.",
"",
"### 13.2 Measured, not asserted",
"answers 0.00000 -> 0.00418 rms | ladder overlap self 1.000 vs least-kin",
"0.180 | kin answer 2.2x a stranger | a dark utterance draws a 386 Hz reply,",
"a bright one 406 Hz | membrane 16 % of the sound with both ringing,",
"difference tone x1.95 against a control frequency x0.87, and BIT-IDENTICAL",
"with no partner whatever COMMUNION is | 53.9 cents of accent in 30 s at",
"PLASTICITY 1 (19 steps, bounded, still a solvable body), exactly 0 at",
"PLASTICITY 0. Bench 1831 checks ALL CLEAR.",
"",
"### 13.3 The mute-specimen bug this uncovered",
"Three of 300 catalog entries were SILENT and eleven nearly so. The 4D body",
"took eigenvectors 1-4 as its spatial axes with min/max normalisation; a",
"LOCALISED eigenvector (entirely ordinary in tree and community topologies)",
"then put 67 of 68 nodes at one coordinate, so the body had no extent along",
"w, the slab caught nothing, and every mode weighed zero. Fixed at the cause",
"- the axes are now the four most DELOCALISED of the lowest dozen modes,",
"normalised by median / 10-90 percentile / tanh - and at the symptom: the",
"slab widens until it holds tissue, so no anatomy can ever be mute. Ratios",
"come from eigenvalues, so no specimen VOICE changed; only the bodies did.",
"**A spectral embedding must select for delocalisation. The lowest four",
"eigenvectors are not automatically axes.**",
""].join(NL);

fs.writeFileSync(P, s + add);
console.log("design doc §13 updated");
