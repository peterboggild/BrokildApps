/*  Gain structure, round 6: measure the loudest SCENE, not the opening bar.

    Two faults in round 5's own measurement, both of them the same mistake in
    different clothes - measuring the instrument through something that had
    already altered what I was trying to measure:

    1. Four presets read peak exactly 1.500, which is not a coincidence: it is
       the output stage's ±1.5 safety clamp. The probe took its peak from the
       RENDERED take, downstream of that clamp, so the hottest presets were
       measured after being hard-clipped - and their loudest-2s figure is then
       understated by however much was cut off. noLimit now bypasses the clamp
       as well, so what is measured is what the patch asks for.

    2. A preset built to travel - THE ARCHIVE BURNS is ten minutes of HISTORY -
       was judged entirely on its opening state, with h_pos at zero, and asked
       for +24 dB. Of course it did: it is designed to start almost silent. A
       patch's loudness is the loudness of its LOUDEST SCENE, so the probe now
       measures at h_pos 0 and again at h_pos 1 and keeps the louder.

    The boost is capped at +6 dB. Matching the quiet end exactly is not the
    goal and would flatten pieces whose quietness is the point; the goal is
    that no preset drives the limiter and none is lost against its neighbours.
*/
const fs = require("fs");
const F = p => "C:/Users/peter/b/ThirtyThousandYears/" + p;
const edits = [];

edits.push([F("Source/Environment.h"), [
  [`            if (! noLimit) lim.tick (l, r);
            l = clampf (l, -1.5f, 1.5f); r = clampf (r, -1.5f, 1.5f);`,
   `            if (! noLimit) { lim.tick (l, r); l = clampf (l, -1.5f, 1.5f); r = clampf (r, -1.5f, 1.5f); }`],
]]);

edits.push([F("test/probe.cpp"), [
  [`        const float tgtRms = 0.20f, pkCeil = 0.75f;
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
            }`,
   `        const float tgtRms = 0.20f, pkCeil = 0.75f;
        std::printf ("%-34s%9s%9s%9s%9s%8s\\n", "preset", "peak", "loud2s", "crest", "trim dB", "trim");
        for (int i = 0; i < numPresets(); ++i)
        {
            float pk = 0.0f, loud = 0.0f;
            /*  A patch's loudness is its LOUDEST SCENE. Measuring only h_pos 0
                asks a piece written to grow over ten minutes how loud it is in
                its first bar, and then offers to boost it 24 dB. */
            for (float hp : { 0.0f, 1.0f })
            {
                Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
                e.prepare (48000.0, 512); e.p[P_drone] = 1; e.out.noLimit = true;
                e.p[P_h_on] = e.sceneSet[1] || e.sceneSet[2] || e.sceneSet[3] ? 1.0f : 0.0f;
                e.p[P_h_pos] = hp; e.p[P_h_hold] = 1.0f;
                e.out.preLimPeak = 0.0f;
                e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
                Take t = render (e, 14.0);
                const int n = t.n(), W = 96000, H = 12000;
                pk = std::max (pk, e.out.preLimPeak);
                for (int s0 = 0; s0 + W <= n; s0 += H)
                {
                    double e2 = 0; for (int k = s0; k < s0 + W; ++k) { const float l = t.L[(size_t) k], r = t.R[(size_t) k]; e2 += l * l + r * r; }
                    loud = std::max (loud, (float) std::sqrt (e2 / (2 * W)));
                }
            }`],
  [`            const float trimDb = std::max (-24.0f, std::min (8.0f, std::min (dbR, dbP)));`,
   `            /*  +6 dB is as far as the quiet end is lifted: matching it exactly
                would flatten pieces whose quietness is the point. */
            const float trimDb = std::max (-24.0f, std::min (6.0f, std::min (dbR, dbP)));`],
]]);

let miss = [];
const loaded = edits.map (([f, es]) => {
  let s = fs.readFileSync (f, "utf8");
  for (const [a] of es) { const n = s.split (a).length - 1; if (n !== 1) miss.push (n + "x in " + f.split("/").pop() + ": " + a.slice (0, 60).replace (/\n/g, " ")); }
  return s;
});
if (miss.length) { console.log ("ANCHOR MISS:\n" + miss.join("\n")); process.exit (1); }
edits.forEach (([f, es], k) => { let s = loaded[k]; for (const [a, b] of es) s = s.replace (a, b); fs.writeFileSync (f, s); });
console.log ("gain structure round 6 patched");
