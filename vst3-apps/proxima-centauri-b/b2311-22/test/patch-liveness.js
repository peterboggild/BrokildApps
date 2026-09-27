/*  Peter: "I'm not sure doing anything to the screen changes the sound? I
    don't see a correlation with the changes of the specimens in the sound?"

    One root cause behind both, and my own bench had already said it: the
    anatomy probe had to measure "before migration equalises" — because
    MIGRATION WAS ~100x TOO FAST. At default METABOLISM the mode energies
    equalised across the network within a fraction of a second, so:

      * the strike profile — which IS the specimen's identity — was erased
        almost instantly: every note on every specimen converged toward the
        same uniform inharmonic wash;
      * DEPTH and GRAVITY (baked into the excitation) were eaten before the
        ear could register them, so stirring an organ seemed to do nothing.

    Fixes:
      1. migration becomes a crawl (0.18 -> 0.014): the strike identity
         survives for seconds at default and audibly TRAVELS at full
         metabolism — exploration, not evaporation;
      2. GRAVITY moves out of the excitation and becomes LIVE — a register
         lean on the realised amplitudes, renormalised inside the voice's
         energy budget so it changes colour, never loudness. Stirring it
         reshapes a held note immediately;
      3. rotors refresh every tick, so REVIVAL and MEMBRANE edits land on
         held notes with no stale-frequency corner cases;
      4. stir gains a little reach (full turn ~ 0.38 of range).
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

const wE = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.cpp", [

[`    const float coupleRate = p.metabolism * p.metabolism * 0.18f
                           * (1.0f + p.membrane * 1.5f);`,
`    /*  Migration is a crawl, deliberately: at default the strike profile
        survives for seconds (that profile IS the specimen speaking); at full
        METABOLISM it audibly travels within a second. The first build had
        this ~13x hotter and every note equalised into the same wash before
        the ear could hold it — the bench even said so (the anatomy probe had
        to measure "before migration equalises") and I read past it. */
    const float coupleRate = p.metabolism * p.metabolism * 0.014f
                           * (1.0f + p.membrane * 1.5f);`, "couple rate"],

[`    //  spectral tilt (GRAVITY) and warmth bias enter the excitation profile
    const float lean = (p.gravity - 0.5f) * 2.4f;
    float norm = 0.0f;
    for (int k = 0; k < s.nModes; ++k)
    {
        const float vk = s.vec[(size_t) k * s.nNodes + exNode];
        /*  A sharpened profile: touching a place on the body strongly favours
            the modes that actually live there. The 1.7 exponent is what makes
            DEPTH a journey rather than a tone control - hub and remote tissue
            must sound like different strikes on the same being. */
        float a = std::pow (std::fabs (vk) + 1.0e-4f, 1.7f) + 0.008f;
        a *= std::pow (s.ratio[k], -lean);
        a *= 1.0f + p.warmth * 1.6f * warmMem[(size_t) k];
        v.exciteW[(size_t) k] = a;
        norm += a * a;
    }`,
`    //  warmth bias enters the excitation profile; GRAVITY does NOT — it is
    //  LIVE now (see controlTick), so stirring it reshapes a held note
    float norm = 0.0f;
    for (int k = 0; k < s.nModes; ++k)
    {
        const float vk = s.vec[(size_t) k * s.nNodes + exNode];
        /*  A sharpened profile: touching a place on the body strongly favours
            the modes that actually live there. The 1.7 exponent is what makes
            DEPTH a journey rather than a tone control - hub and remote tissue
            must sound like different strikes on the same being. */
        float a = std::pow (std::fabs (vk) + 1.0e-4f, 1.7f) + 0.008f;
        a *= 1.0f + p.warmth * 1.6f * warmMem[(size_t) k];
        v.exciteW[(size_t) k] = a;
        norm += a * a;
    }`, "gravity out of trigger"],

[`        //  rotor refresh (only recomputes trig when something moves them)
        updateVoiceRotors (v, false);

        //  realised amplitude targets: energy x section x ears, ramped
        for (int k = 0; k < s.nModes; ++k)
        {
            float target = std::sqrt (std::max (0.0f, v.energy[(size_t) k]))
                         * sectW[(size_t) k];
            if (v.freq[(size_t) k] <= 0.0f) target = 0.0f;
            v.ampStep[(size_t) k] = (target - v.amp[(size_t) k]) / (float) kCtrl;
        }`,
`        //  rotors follow the moving spectrum every tick — cheap, and no
        //  stale-frequency corner when REVIVAL or MEMBRANE returns to zero
        updateVoiceRotors (v, true);

        /*  GRAVITY, live: a register lean on the realised amplitudes,
            renormalised inside the voice's own energy budget so it changes
            COLOUR, never loudness — the conservation law audible under a
            moving hand. tiltW is scratch, reused per tick. */
        const float lean = (p.gravity - 0.5f) * 2.4f;
        float eSum = 1e-12f, tSum = 1e-12f;
        for (int k = 0; k < s.nModes; ++k)
        {
            const float tw = std::pow (s.ratio[k], -lean);
            tiltW[(size_t) k] = tw;
            const float ek = std::max (0.0f, v.energy[(size_t) k]);
            eSum += ek;
            tSum += ek * tw * tw;
        }
        const float tiltNorm = std::sqrt (eSum / tSum);

        //  realised amplitude targets: energy x tilt x section, ramped
        for (int k = 0; k < s.nModes; ++k)
        {
            float target = std::sqrt (std::max (0.0f, v.energy[(size_t) k]))
                         * tiltW[(size_t) k] * tiltNorm
                         * sectW[(size_t) k];
            if (v.freq[(size_t) k] <= 0.0f) target = 0.0f;
            v.ampStep[(size_t) k] = (target - v.amp[(size_t) k]) / (float) kCtrl;
        }`, "live gravity"]
]);

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    //  warmth: long-term per-mode memory (global, deterministic from events)
    std::array<float, kMaxModes> warmMem {};`,
`    //  warmth: long-term per-mode memory (global, deterministic from events)
    std::array<float, kMaxModes> warmMem {};
    std::array<float, kMaxModes> tiltW {};     // per-tick gravity scratch`, "tilt scratch"]
]);

const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [
["sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.045);",
 "sendParam(g.id, (VAL[g.id] !== undefined ? VAL[g.id] : 0.5) + da * 0.06);", "stir reach"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wE(); wH(); wU();
console.log("migration 13x slower; gravity live + conserved; rotors fresh; stir potent");
