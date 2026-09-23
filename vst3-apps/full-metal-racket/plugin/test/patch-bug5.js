/*  FMR BUGLIST 5 — PUNCH, a transient control.

    Peter: "Can you build a simple transient controller button into the FMR?
    Something to make it slam and punch through the mix when needed?"

    A transient designer normally has to GUESS: it watches the audio, decides
    where a hit began, and shapes an envelope around that estimate. Every
    artefact those things have — false triggers, pumping, smearing when two
    drums land together — comes from the guess being wrong.

    This machine does not have to guess. It TRIGGERS the voices, and
    Voice::age already counts samples since each hit. So the control is exact,
    per voice, sample-accurate, and cannot false-trigger: twelve drums landing
    on the same step each get their own correct envelope, which nothing on a
    master bus can do. That is the version worth building; a generic shaper
    on the output would just be a duplicate of BWFX's STRIP.

        a = exp(-age / tAtt)                    1 at the hit, ~0 after tAtt
        g = (1 + punch*kAtt*a) * (1 - punch*kSus*(1-a))

    A bigger front and a tighter back — the hit occupies less time and more of
    the moment it is in.

    tAtt is per FAMILY, not global: a kick's strike is over in about 4 ms, a
    snare's in 8, and the metal channels keep almost none of the SUSTAIN half,
    because a cymbal ringing for four seconds does not want its body pulled
    down by a control aimed at a kick — that reads as a broken decay, not as
    punch.

    Exactly absent at zero: g is an IEEE-exact 1.0f, so a rack at PUNCH 0 is
    bit-identical, the same contract RAIL SAG, BLEED, AGE and the BWFX macros
    keep. The bench memcmps it.
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

const wH = edit("C:/Users/peter/b/FullMetalRacket/Source/Engine.h", [
[`    GP_TEMPO,       // internal clock, used when the host gives none
    NGP`,
`    GP_TEMPO,       // internal clock, used when the host gives none
    GP_PUNCH,       // transient shaper, keyed to each voice's own trigger
    NGP`, "enum"]
]);

const wE = edit("C:/Users/peter/b/FullMetalRacket/Source/Engine.cpp", [

[`                    case GP_TEMPO:   r.def = 0.28f; break;      // ~120 BPM on the internal clock`,
`                    case GP_TEMPO:   r.def = 0.28f; break;      // ~120 BPM on the internal clock
                    case GP_PUNCH:   r.def = 0.0f;  break;      // exactly absent by default`, "table"],

//  the mechanism, from a number the voice already has
[`    // ---- the amp envelope --------------------------------------------------
    out *= v.amp.tick() * v.famGain;`,
`    /*  ---- PUNCH ----------------------------------------------------------
        Keyed to this voice's OWN trigger, so it is exact rather than
        detected. A lift on the strike and a lean on the body: the hit takes
        up less time and more of the moment it is in. Exactly 1.0f at zero —
        an IEEE-exact multiply, so the machine is bit-identical without it. */
    if (p.g[GP_PUNCH] > 0.0f)
    {
        const float pn = clamp01 (p.g[GP_PUNCH]);
        //  the strike is a different length in every family, and the metal
        //  keeps almost none of the sustain lean — a cymbal ringing for four
        //  seconds reads as a broken decay, not as punch
        float tAtt, kAtt, kSus;
        switch (v.fam)
        {
            case FAM_SNARE: tAtt = 0.0080f; kAtt = 1.15f; kSus = 0.34f; break;
            case FAM_TOM:   tAtt = 0.0090f; kAtt = 1.00f; kSus = 0.30f; break;
            case FAM_PERC:  tAtt = 0.0060f; kAtt = 1.05f; kSus = 0.26f; break;
            case FAM_METAL: tAtt = 0.0070f; kAtt = 0.85f; kSus = 0.06f; break;
            default:        tAtt = 0.0040f; kAtt = 1.30f; kSus = 0.38f; break;   // kick
        }
        const float a = std::exp (-(float) v.age / (tAtt * (float) fs));
        out *= (1.0f + pn * kAtt * a) * (1.0f - pn * kSus * (1.0f - a));
    }

    // ---- the amp envelope --------------------------------------------------
    out *= v.amp.tick() * v.famGain;`, "punch"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wH(); wE();
console.log("5: PUNCH, keyed to the trigger the machine already knows about");
