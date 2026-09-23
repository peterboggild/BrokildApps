/*  Gain structure, round 5: measure the bank in a way that fits the bank.

    Round 4's level probe said NAKED STRUCTURE measures 0.000 - silent. It is
    not silent. It is "a struck plate and nothing else" (st_exc=0, a strike at
    note-on), and the probe discarded the first three seconds and then measured
    the four after, i.e. everything except the sound. Four other presets read
    0.05-0.15 for the same reason: they are sparse or event-driven and nothing
    happened inside the window. The trap is already written in the house notes
    three times over and it caught this probe anyway.

    So one metric that fits a bank holding infinite drones AND one-shot
    gestures: render 24 s FROM NOTE-ON and take

      * peak over the whole render          -> what the limiter will see
      * the loudest 2 s rms window          -> how loud the patch actually is

    The loudest sustained moment is the right thing to match: for a drone it is
    the drone, for a strike it is the strike, and no window can miss it.

    Also fixed: PATCH TRIM could only attenuate, which is useless for half of a
    level pass - the bank spans 27 dB and the quiet end needs lifting. It is
    now -24..+8 dB with the default at v = 0.75, which lands on EXACTLY 0.0 dB
    (0.75 and 32 are both exact in binary, so dbGain returns exactly 1.0f and a
    preset that never mentions the trim is bit-identical).
*/
const fs = require("fs");
const F = p => "C:/Users/peter/b/ThirtyThousandYears/" + p;
const edits = [];

edits.push([F("Source/Params.h"), [
  [`    X(outtrim,    "outtrim",    "PATCH TRIM",      1.0f,  KP_DB,   -24, 0,  F_NONE) \\`,
   `    X(outtrim,    "outtrim",    "PATCH TRIM",      0.75f, KP_DB,   -24, 8,  F_NONE) \\`],
  [`        only attenuate and its default is exactly 0 dB, so a preset that never
        mentions it is bit-identical to one written before it existed. */ \\`,
   `        default is EXACTLY 0 dB - 0.75 and 32 are both exact in binary, so the
        law returns 0.0 and dbGain returns 1.0f - which is what makes a preset
        that never mentions it bit-identical to one written before it existed. */ \\`],
]]);

edits.push([F("test/probe.cpp"), [
  [`    else if (what == "levels")
    {
        /*  With the limiter bypassed, because the question is what the patch
            asks for, not what the limiter allows. TRIM is the attenuation that
            would land this patch on the target peak. */
        const float target = 0.62f;
        std::printf ("%-34s%9s%9s%9s%9s%8s\\n", "preset", "peak", "rms", "crest", "trim dB", "trim");
        for (int i = 0; i < numPresets(); ++i)
        {
            Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
            e.prepare (48000.0, 512); e.p[P_drone] = 1; e.out.noLimit = true;
            e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
            render (e, 3.0);
            e.out.preLimPeak = 0.0f;
            Take t = render (e, 4.0);
            const float pk = e.out.preLimPeak, rm = rms (t.L);
            const float db = 20 * std::log10 (target / std::max (1e-4f, pk));
            const float trimDb = std::min (0.0f, std::max (-24.0f, db));
            std::printf ("%-34s%9.3f%9.4f%9.1f%9.1f%8.3f\\n", preset (i).name, pk, rm,
                         20 * std::log10 ((pk + 1e-9f) / (rm + 1e-9f)), trimDb, (trimDb + 24.0f) / 24.0f);
        }
    }`,
   `    else if (what == "levels")
    {
        /*  The limiter is bypassed, because the question is what the patch asks
            for and not what the limiter allows. 24 s FROM NOTE-ON, so a struck
            gesture is inside the window as well as a drone: the loudest 2 s rms
            is the patch's real loudness either way. */
        const float tgtRms = 0.20f, pkCeil = 0.75f;
        std::printf ("%-34s%9s%9s%9s%9s%8s\\n", "preset", "peak", "loud2s", "crest", "trim dB", "trim");
        for (int i = 0; i < numPresets(); ++i)
        {
            Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
            e.prepare (48000.0, 512); e.p[P_drone] = 1; e.out.noLimit = true;
            e.out.preLimPeak = 0.0f;
            e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
            Take t = render (e, 24.0);
            const int n = t.n(), W = 96000, H = 12000;
            float pk = 0.0f, loud = 0.0f;
            for (int k = 0; k < n; ++k) pk = std::max (pk, std::max (std::abs (t.L[(size_t) k]), std::abs (t.R[(size_t) k])));
            for (int s0 = 0; s0 + W <= n; s0 += H)
            {
                double e2 = 0; for (int k = s0; k < s0 + W; ++k) { const float l = t.L[(size_t) k], r = t.R[(size_t) k]; e2 += l * l + r * r; }
                loud = std::max (loud, (float) std::sqrt (e2 / (2 * W)));
            }
            /*  Match on the loudest sustained moment, but never let the peak
                past the ceiling: whichever asks for less attenuation wins. */
            const float dbR = 20 * std::log10 (tgtRms / std::max (1e-5f, loud));
            const float dbP = 20 * std::log10 (pkCeil / std::max (1e-5f, pk));
            const float trimDb = std::max (-24.0f, std::min (8.0f, std::min (dbR, dbP)));
            std::printf ("%-34s%9.3f%9.4f%9.1f%9.1f%8.4f\\n", preset (i).name, pk, loud,
                         20 * std::log10 ((pk + 1e-9f) / (loud + 1e-9f)), trimDb, (trimDb + 24.0f) / 32.0f);
        }
    }`],
]]);

let miss = [];
const loaded = edits.map (([f, es]) => {
  let s = fs.readFileSync (f, "utf8");
  for (const [a] of es) { const n = s.split (a).length - 1; if (n !== 1) miss.push (n + "x in " + f.split("/").pop() + ": " + a.slice (0, 60).replace (/\n/g, " ")); }
  return s;
});
if (miss.length) { console.log ("ANCHOR MISS:\n" + miss.join("\n")); process.exit (1); }
edits.forEach (([f, es], k) => { let s = loaded[k]; for (const [a, b] of es) s = s.replace (a, b); fs.writeFileSync (f, s); });
console.log ("gain structure round 5 patched");
