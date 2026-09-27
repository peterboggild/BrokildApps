// A probe mode that prints every internal stage's peak for every preset, so a
// stage that saturates upstream of the limiter can be seen rather than guessed.
const fs = require("fs");
const p = "C:/Users/peter/b/ThirtyThousandYears/test/probe.cpp";
let s = fs.readFileSync(p, "utf8");
const a = `    return 0;
}`;
const b = `    else if (what == "stages")
    {
        // every preset, drone on plus two keys: the peak at every internal stage
        std::printf ("%-34s", "preset");
        for (int k = 0; k < Engine::NUM_STAGES; ++k) std::printf ("%10s", Engine::stageName (k));
        std::printf ("%8s\\n", "limit");
        for (int i = 0; i < numPresets(); ++i)
        {
            Engine e; applyPreset (i, e.p, e.macro, e.scene, e.sceneSet, e.life.slots, e.life.mseg);
            e.prepare (48000.0, 512); e.p[P_drone] = 1;
            e.noteOn (45, 0.9f); e.noteOn (52, 0.85f);
            render (e, 3.0);
            for (auto& v : e.stagePeak) v = 0.0f;
            render (e, 4.0);
            std::printf ("%-34s", preset (i).name);
            for (int k = 0; k < Engine::NUM_STAGES; ++k) std::printf ("%10.2f", e.stagePeak[k]);
            std::printf ("%8.2f\\n", e.limReduction);
        }
    }
    return 0;
}`;
if (s.split(a).length !== 2) { console.log("ANCHOR MISS"); process.exit(1); }
s = s.replace(a, b);
fs.writeFileSync(p, s);
console.log("probe mode added");
