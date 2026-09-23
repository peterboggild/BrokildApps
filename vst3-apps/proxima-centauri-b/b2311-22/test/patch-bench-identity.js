/*  Remove the heredoc-mangled section (literal newlines inside its printf
    string literals — the Bash tool ate the backslashes, the standing
    CLAUDE.md trap) and re-insert it correctly.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/ArtefactB2311/test/bench.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : String.fromCharCode(10);

const START = "    //  ---- 11b. Peter's report: specimens must DIFFER, gravity must LIVE --";
const END = "    //  ---- 12. WAKE=0 is exactly inert; wake wanders deterministically ---";
const i = s.indexOf(START);
const j = s.indexOf(END);
if (i < 0 || j < 0) { console.error("bounds not found"); process.exit(1); }

const NEW = [
"    //  ---- 11b. Peter's report: specimens must DIFFER, gravity must LIVE --",
"    {",
'        std::printf ("-- identity: two specimens differ; gravity reshapes a held note\\n");',
"        auto renderSpec = [&] (int cat, std::vector<float>& L)",
"        {",
"            Engine e;",
"            e.p.specimen = (float) cat; e.p.transit = 0; e.p.wake = 0;",
"            e.loadSpecimen (cat);",
"            e.prepare (fs, 512);",
"            const int N = 48000;",
"            L.assign (N, 0);",
"            std::vector<float> R (N);",
"            render (e, { { 0, 0, 57, 0.9f } }, L.data(), R.data(), N);",
"        };",
"        std::vector<float> s0, s1;",
"        renderSpec (0, s0);",
"        renderSpec (37, s1);",
"        double d = 0, t = 0;",
"        for (int i2 = 12000; i2 < 48000; ++i2)",
"        {",
"            d += std::fabs ((double) s0[i2] - s1[i2]);",
"            t += std::max (std::fabs ((double) s0[i2]), std::fabs ((double) s1[i2]));",
"        }",
"        const double relSpec = t > 0 ? d / t : 0;",
'        std::printf ("   specimen 0 vs 37: relative difference %.2f\\n", relSpec);',
'        CHECK (relSpec > 0.5, "specimens sound alike (%.2f)", relSpec);',
"",
"        //  gravity moved MID-NOTE must reshape the spectrum of the held note",
"        auto renderGrav = [&] (bool move, std::vector<float>& L)",
"        {",
"            Engine e;",
"            e.p.transit = 0; e.p.wake = 0; e.p.metabolism = 0;",
"            e.prepare (fs, 512);",
"            const int N = 96000;",
"            L.assign (N, 0);",
"            std::vector<float> R (N);",
"            e.noteOn (57, 0.9f);",
"            int done = 0;",
"            while (done < N)",
"            {",
"                if (move && done >= 48000) e.p.gravity = 0.95f;",
"                const int m = std::min (512, N - done);",
"                e.process (L.data() + done, R.data() + done, m);",
"                done += m;",
"            }",
"        };",
"        std::vector<float> gStill, gMoved;",
"        renderGrav (false, gStill);",
"        renderGrav (true, gMoved);",
"        double gd = 0, gt = 0;",
"        for (int i2 = 60000; i2 < 96000; ++i2)",
"        {",
"            gd += std::fabs ((double) gStill[i2] - gMoved[i2]);",
"            gt += std::max (std::fabs ((double) gStill[i2]), std::fabs ((double) gMoved[i2]));",
"        }",
"        const double relG = gt > 0 ? gd / gt : 0;",
'        std::printf ("   gravity moved mid-note: relative difference %.2f\\n", relG);',
'        CHECK (relG > 0.2, "gravity does not live on a held note (%.2f)", relG);',
"    }",
"",
""].join(NL);

s = s.slice(0, i) + NEW + s.slice(j);
fs.writeFileSync(P, s);
console.log("section repaired");
