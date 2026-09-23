/*  Measured, then fixed:

    1. THE MEMBRANE WAS INAUDIBLE — 0.6 % of the output, and no sum tone
       above the noise floor. The product of two quiet signals is a very
       quiet signal; a ring modulator needs real gain. Now 55x with a bounded
       contribution, so the third voice is a voice.

    2. IT ONLY ANSWERED IN SILENCES, so a held drone was met with nothing at
       all — and the membrane, which needs both bodies sounding at once, could
       almost never engage. It now also INTERJECTS: if you have been going on
       for five seconds it answers over you. Conversation, not turn-taking.

    3. THE KINSHIP MAPPING SATURATED (kin*5 clamped to 1 for both kin and
       strangers), so kin and stranger got identical reply amplitudes even
       though the engine measured 0.556 against 0.279. Scaled to use the
       range the quantity actually occupies.

    4. THE PLAYED BODY DRIFTED FIVE TIMES FASTER THAN THE LISTENER — 116
       cents in thirty seconds at full plasticity. The fiction is that
       LISTENING is what remodels, and an instrument that retunes itself by a
       semitone while you play it reads as a fault, not a feature. The
       speaker's share is scaled back and the default lowered, so the accent
       is a slow discovery rather than an ambush.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    float quietFor = 0.0f;                     // seconds since it went quiet`,
`    float quietFor = 0.0f;                     // seconds since it went quiet
    float sinceReply = 0.0f;                   // seconds since it last spoke`, "since"],
[`        sentAcc = sentPow = spkEnv = quietFor = 0.0f;`,
 `        sentAcc = sentPow = spkEnv = quietFor = sinceReply = 0.0f;`, "silence"],
[`    float plastic   = 0.15f;   // how much listening remodels the listener`,
 `    float plastic   = 0.10f;   // how much listening remodels the listener`, "plastic def"]
]);

const wC = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

[`    { "plastic",   "PLASTICITY", 0.15f, KP_PCT, 0, 1, &Params::plastic },`,
 `    { "plastic",   "PLASTICITY", 0.10f, KP_PCT, 0, 1, &Params::plastic },`, "plastic spec"],

//  it interjects as well as answers
[`    //  ---- the reply ----
    const bool wantReply = (! il.speaking)
                        && il.quietFor > 0.18f
                        && il.sentPow > 2.0e-7f
                        && heardTot > 0.0f;`,
`    //  ---- the reply ----
    il.sinceReply += dt;
    /*  It answers in your silences — and INTERJECTS if you have been going
        on. A body that only ever spoke into gaps would be mute under a held
        drone, and the membrane, which needs both voices at once, would
        almost never exist. */
    const bool heardEnough = il.sentPow > 2.0e-7f && heardTot > 0.0f;
    const bool wantReply = (! il.speaking) && heardEnough
                        && (il.quietFor > 0.18f || il.sinceReply > 5.0f);`, "interject"],

[`        //  loudness: kin answer strongly, strangers faintly but never mutely
        const float kin = clampf (il.lastKin * 5.0f, 0.0f, 1.0f);
        const float A = clamp01 (p.converse) * 16.0f * std::sqrt (il.sentPow)
                      * (0.30f + 0.70f * kin);`,
`        //  loudness: kin answer strongly, strangers faintly but never mutely.
        //  Measured range of lastKin across pairs is ~0.15..0.65, so the
        //  scaling has to live there or every pair sounds equally understood.
        const float kin = clampf (il.lastKin * 1.5f, 0.0f, 1.0f);
        const float A = clamp01 (p.converse) * 16.0f * std::sqrt (il.sentPow)
                      * (0.30f + 0.70f * kin);`, "kin map"],

[`        il.sentPow *= 0.15f;
        il.speaking = true;`,
`        il.sentPow *= 0.15f;
        il.speaking = true;
        il.sinceReply = 0.0f;`, "reset since"],

//  a membrane you can hear
[`            if (p.commune > 0.0f)
            {
                const float m = p.commune * 13.0f * spk * 0.5f * (ilL + ilR);
                outL += m; outR += m;
            }`,
`            if (p.commune > 0.0f)
            {
                //  a true product, so the sidebands are real; bounded, so a
                //  loud pair cannot detonate it
                const float m = clampf (p.commune * 55.0f * spk * 0.5f * (ilL + ilR),
                                        -0.30f, 0.30f);
                outL += m; outR += m;
            }`, "membrane gain"],

//  listening remodels the LISTENER; playing marks the played body far less
[`            accumulateScar (s, modeE.data(), scarA, rate);`,
`            //  the fiction, and the balance: LISTENING is what remodels. The
            //  played body is marked by use too, but a fifth as fast.
            accumulateScar (s, modeE.data(), scarA, rate * 0.2f);`, "speaker rate"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wC();
console.log("membrane audible; it interjects; kinship maps; the accent is patient");
