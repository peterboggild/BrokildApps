"use strict";
const fs = require("fs");
const P = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";
let s = fs.readFileSync(P, "utf8");
const NL = String.fromCharCode(10);

const anchor = "- **v1 graphics are fully computed";
const i = s.indexOf(anchor);
if (i < 0) { console.error("anchor missing"); process.exit(1); }

const add = [
"- 2026-08-30 **THE INTERLOCUTOR (design 13), build 260830.1** - a second body that listens, answers and is CHANGED by listening. Peter: \"go all three! lets make a unique instrument\". (1) sympathetic listening: each specimen has a FIXED natural register (Specimen::voiceHz, 58-195 Hz from the catalog number) - without that, a second body would just follow your note and \"kin converse, strangers are deaf\" would be a tautology; one 2-pole resonator per mode of the other body (Q 25, unity peak gain via rin=1-R) driven by the real audio; the reply takes its TIMBRE from what physically absorbed, its STRUCTURE from a rank translation of your utterance (ladders are sorted, so the rank map is a proportional index remap), its LOUDNESS from kinship. (2) the nonlinear MEMBRANE: the two voices multiply - sum/difference frequencies in neither catalog entry. (3) PLASTICITY: Hebbian edge scars + a full eigen re-solve on the message thread.",
"  - **Measured, because bounded proves nothing (the Mars Wars rule):** answers 0.00000 -> 0.00418 rms; ladder overlap self 1.000 vs least-kin 0.180; kin answer 2.2x a stranger; dark utterance -> 386 Hz reply, bright -> 406; membrane 16 % of the sound and BIT-IDENTICAL with no partner; 53.9 cents of accent in 30 s at PLASTICITY 1, exactly 0 at 0. Bench 1831.",
"  - **THREE OF 300 SPECIMENS WERE MUTE and eleven nearly so** - found only because the new bench section happened to pick specimen 3. The 4D body used eigenvectors 1-4 as spatial axes normalised min/max, and a LOCALISED eigenvector (ordinary in tree/community families) put 67 of 68 nodes at one coordinate: no extent in w, nothing in the slab, silence. Fix: axes = the four most DELOCALISED of the lowest dozen modes (rank by inverse participation ratio), normalised median / 10-90 percentile / tanh, plus a slab that widens until it holds tissue. **A spectral embedding must select for delocalisation; the lowest eigenvectors are not automatically axes.**",
"  - **The bug that cost the most: \"rest\" never arrived.** Scars reached 0.66 (13x the threshold) and not one accent applied, because remodelling waited for BOTH bodies silent while the interlocutor interjected forever (its trigger was elapsed time alone, and what it heard decays over 6 s). Two fixes: an interjection now requires that you are STILL PLAYING, and each body has its OWN gate. **When a feature never fires, instrument the CONDITION, not the effect** - exposing scarMax and atRest to the panel found it in one run.",
"  - Refactor that made plasticity possible: `deriveFromGraph` / `resolveSpecimen` split out of generateSpecimen, with the generator's three r.uni() draws hoisted INTO the Specimen in their original order so all 300 catalog entries stay bit-identical (proof: same worst residual 1.96e-10, same alienness floor, same max salt 36).",
"  - Panel: the second body is drawn as another creature - further off, its own clock, mostly ghost, condensing as it speaks; filaments cross the gap while the membrane is live; readout carries KIN and ACCENT. **Shift-click the survey field to CALL a body** (the field is laid out by spectral similarity, so where you click decides whether you will be understood). `emitOther()` had to join emitInitialState - a timer-only emit is lost whenever the page is not yet listening and then never repeats.",
"  - Reusable: `test/probe.cpp` + the `abprobe` CMake target (engine internals, the thing that settled every one of these), `test/run-bench.ps1` (SAC nudge-copy loop).",
""].join(NL);

fs.writeFileSync(P, s.slice(0, i) + add + s.slice(i));
console.log("CLAUDE.md updated");
