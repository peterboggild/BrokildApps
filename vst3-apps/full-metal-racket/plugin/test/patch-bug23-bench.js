/*  Bench for FMR buglist 2. One existing check asserts the old contract —
    "morph at one is kit B EXACTLY" is no longer true of stepped parameters,
    by design — and the new rule needs stating: across a whole sweep, nothing
    stepped ever moves, and the five kit globals do.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
const miss = [];
function sub(a, b, tag) {
  const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

//  the endpoint check now speaks only for the parameters that morph
sub(`        Params w = e.kitA; w.g[GP_MORPH] = 1.0f;
        e.applyMorph (w);
        bool isB = true;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (s2.chan < 0 || s2.slot == CP_MUTE) continue;
            if (std::fabs (pvalue (w, s2) - pvalue (e.kitB, s2)) > 1e-6f) { isB = false; break; }
        }
        ok (isB, "morph at one is kit B exactly");`,
`        Params w = e.kitA; w.g[GP_MORPH] = 1.0f;
        e.applyMorph (w);
        bool isB = true;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (! morphable (s2)) continue;
            //  stepped choices deliberately do not travel (buglist 2b), so
            //  the endpoint is kit B in everything CONTINUOUS
            if (s2.kind == KP_LIST || s2.kind == KP_SW) continue;
            if (std::fabs (pvalue (w, s2) - pvalue (e.kitB, s2)) > 1e-6f) { isB = false; break; }
        }
        ok (isB, "morph at one is kit B in every continuous value");`, "endpoint");

//  the new rule
sub(`    //  with only one kit captured there is nothing to morph toward, and the
    //  fader must do NOTHING rather than sweep to silence`,
`    /*  BUGLIST 2b: a morph never STEPS. Across the whole sweep every
        KP_LIST / KP_SW must sit exactly where it was last set — a fader you
        would automate over eight bars must not hide cliffs inside it. */
    {
        int stepped = 0, moved = 0;
        Params base = e.kitA;
        for (int q = 0; q <= 20; ++q)
        {
            Params w = base;
            w.g[GP_MORPH] = (float) q / 20.0f;
            e.applyMorph (w);
            for (int i = 0; i < numParams(); ++i)
            {
                const PSpec& s2 = paramSpec (i);
                if (s2.kind != KP_LIST && s2.kind != KP_SW) continue;
                if (q == 0) ++stepped;
                if (pvalue (w, s2) != pvalue (base, s2)) ++moved;
            }
        }
        std::printf ("    %d stepped parameters, %d moved across a 21-point sweep\\n",
                     stepped, moved);
        ok (stepped > 0, "no stepped parameters found to check");
        ok (moved == 0, "a morph stepped something", (double) moved, 0.0);
    }

    /*  ...and the five globals the kit generator writes DO travel, or a
        morph moves the twelve voices and leaves the machine behind. */
    {
        Params w = e.kitA;
        w.g[GP_MORPH] = 0.5f;
        e.applyMorph (w);
        int movedG = 0;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s2 = paramSpec (i);
            if (s2.chan >= 0 || ! morphable (s2)) continue;
            if (std::fabs (pvalue (e.kitA, s2) - pvalue (e.kitB, s2)) < 1e-6f) continue;
            if (pvalue (w, s2) != pvalue (e.kitA, s2)) ++movedG;
        }
        ok (movedG > 0, "the kit globals do not morph");
    }

    //  with only one kit captured there is nothing to morph toward, and the
    //  fader must do NOTHING rather than sweep to silence`, "new rule");

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench: endpoint re-aimed, plus the never-steps rule and the globals check");
