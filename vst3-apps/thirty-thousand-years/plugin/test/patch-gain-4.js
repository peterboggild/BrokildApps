/*  Gain structure, round 4: a patch owns its own level.

    Round 3 was wrong in an instructive way. It trimmed the voices and then
    raised the master VOLUME to put the level back - and the master is applied
    INSIDE the output stage, upstream of the limiter, so the limiter saw
    exactly what it saw before. STRESSED PLATE went 0.50 -> 0.47. Moving gain
    from one side of a limiter to the other side of nothing changes nothing.

    The meters said so and I misread them: the ST_PRE_LIMIT tap sat BEFORE the
    master gain, so a preset could read "pre-limit 0.65" and "out 0.84" at the
    same time. A stage meter that is not measured where its name says is worse
    than no meter. It is measured inside OutputStage now, immediately before
    lim.tick, after volume, bass mono and width.

    And the number that matters was being under-read: Limiter::reduction is
    INSTANTANEOUS, so the 0.47 at the end of a four-second steady drone is not
    a transient being caught, it is 5.5 dB of continuous gain reduction. A
    limiter compressing a drone for its whole length, with the output meter
    reading under 0 dBFS, is exactly "clipping effects although the total
    output is below 0 dB".

    Two changes, and then a measured level pass over the bank:

      * VOLUME's default goes back to 0.70. It is the player's fader and it
        should sit near unity; it is not the place to fix a patch.
      * PATCH TRIM (outtrim) is new: -24..0 dB, default exactly 0 dB, written
        by the preset. It can only attenuate, so a preset that does not
        mention it is bit-identical, and no preset can use it to get loud.
        This is what makes the bank level-matchable at all - before it, the
        only master gain was the one the player owns, and a preset moving that
        would be two owners for one control.
      * OutputStage::noLimit, set by the probe only, so the real level of a
        patch can be measured instead of the limiter's opinion of it.
*/
const fs = require("fs");
const F = p => "C:/Users/peter/b/ThirtyThousandYears/" + p;
const edits = [];   // [file, [[find, repl], ...]]

/* ---- the table: master fader back to unity, patch trim added ------------ */
edits.push([F("Source/Params.h"), [
  [`    X(volume,     "volume",     "VOLUME",          0.82f, KP_VOL,  0, 0,     F_NS) \\`,
   `    X(volume,     "volume",     "VOLUME",          0.7f,  KP_VOL,  0, 0,     F_NS) \\
    /*  PATCH TRIM belongs to the patch; VOLUME belongs to the player. It can
        only attenuate and its default is exactly 0 dB, so a preset that never
        mentions it is bit-identical to one written before it existed. */ \\
    X(outtrim,    "outtrim",    "PATCH TRIM",      1.0f,  KP_DB,   -24, 0,  F_NONE) \\`],
]]);

/* ---- the output stage: the trim, an honest meter, a measurement bypass -- */
edits.push([F("Source/Environment.h"), [
  [`    Biquad bmLoL[2], bmLoR[2], bmHiL[2], bmHiR[2]; OnePole distLpL, distLpR; Limiter lim; double sr = 48000.0; float lastBm = -1;
    float peakL = 0.0f, peakR = 0.0f, rms = 0.0f;`,
   `    Biquad bmLoL[2], bmLoR[2], bmHiL[2], bmHiR[2]; OnePole distLpL, distLpR; Limiter lim; double sr = 48000.0; float lastBm = -1;
    float peakL = 0.0f, peakR = 0.0f, rms = 0.0f;
    /*  Measured where its name says: after volume and trim, immediately before
        the limiter. The engine's own tap used to be upstream of the master
        gain, which let a preset report a pre-limiter peak below the ceiling
        while the limiter was pulling 5 dB. */
    float preLimPeak = 0.0f;
    bool noLimit = false;           // probe only: measure the patch, not the limiter`],
  [`        const float vol = lawVol (p[P_volume]) * duckGain;`,
   `        const float vol = lawVol (p[P_volume]) * dbGain (lawDb (paramSpec (P_outtrim), p[P_outtrim])) * duckGain;`],
  [`            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
            l = m + s; r = m - s;
            lim.tick (l, r);`,
   `            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
            l = m + s; r = m - s;
            preLimPeak = std::max (preLimPeak, std::max (std::abs (l), std::abs (r)));
            if (! noLimit) lim.tick (l, r);`],
]]);

/* ---- the engine: report the honest pre-limit peak ---------------------- */
edits.push([F("Source/Engine.cpp"), [
  [`        stageHit (stagePeak[ST_PRE_LIMIT], mixL, mixR, m);
        out.process (mixL, mixR, m, p, duckGain);`,
   `        out.process (mixL, mixR, m, p, duckGain);
        stagePeak[ST_PRE_LIMIT] = std::max (stagePeak[ST_PRE_LIMIT], out.preLimPeak);`],
]]);

/* ---- the probe: measure the level a patch really asks for --------------- */
edits.push([F("test/probe.cpp"), [
  [`    else if (what == "thd")`,
   `    else if (what == "levels")
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
    }
    else if (what == "thd")`],
]]);

let miss = [];
const loaded = edits.map (([f, es]) => {
  let s = fs.readFileSync (f, "utf8");
  for (const [a] of es) { const n = s.split (a).length - 1; if (n !== 1) miss.push (n + "x in " + f.split("/").pop() + ": " + a.slice (0, 60).replace (/\n/g, " ")); }
  return s;
});
if (miss.length) { console.log ("ANCHOR MISS:\n" + miss.join("\n")); process.exit (1); }
edits.forEach (([f, es], k) => { let s = loaded[k]; for (const [a, b] of es) s = s.replace (a, b); fs.writeFileSync (f, s); });
console.log ("gain structure round 4 patched (" + edits.length + " files)");
