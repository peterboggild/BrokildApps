/*  Bench for buglist 16. Four existing checks assert exactly the behaviour
    Peter asked us to remove — they were right for the old contract and are
    wrong for the new one. Rewritten, plus the check that states the rule
    itself: NO character bends the note at its own defaults, and the one
    pitch modulator we keep (tape WOW) is provably zero-mean.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
const P = "C:/Users/peter/b/BrokildWorldFX/test/bench.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
function sub(a, b, tag) {
  const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(A).join(B);
}

//  1. tape, alone: inert in PITCH, alive in colour
sub(`        CHECK (a.pitchSag > 0.25f && a.filterMul < 0.95f, "tape character inert");`,
`        CHECK (a.pitchSag == 0.0f, "tape bends the note at its defaults (%.3f)",
               (double) a.pitchSag);
        CHECK (a.filterMul < 0.95f, "tape character inert");`, "tape alone");

//  2. combination: sag still combines when a user asks for it
sub(`        CHECK (w.pitchSag > 0.25f, "tape sag lost in combination");`,
`        //  sag is opt-in now, so ask for it before checking it combines
        r.setCharParam (cTape, 1, 40.0f);
        renderRack (r, L.data(), R.data(), N);
        CHECK (r.worldMod().pitchSag > 0.25f, "tape sag lost in combination");`, "combination");

//  3+4. dark drone: no sag, and a STEADY cluster width rather than a wander
sub(`        CHECK (std::abs (w.pitchSag - 0.25f) < 1e-3f, "dark drone sag wrong");
        CHECK (detMin > 20.0f && detMax < 28.5f && detMax > detMin + 0.05f,
               "dark drone cluster+drift detune out of range");`,
`        CHECK (w.pitchSag == 0.0f, "dark drone bends the note at its defaults (%.3f)",
               (double) w.pitchSag);
        //  CLUSTER is an ensemble width the hosts fan across voices, so it is
        //  allowed to be non-zero — but it must be STEADY. Drift wandering it
        //  was the tuning wandering, which is the bug.
        CHECK (std::abs (detMin - 24.0f) < 1e-3f && std::abs (detMax - 24.0f) < 1e-3f,
               "dark drone detune is not a steady cluster width (%.2f..%.2f)",
               (double) detMin, (double) detMax);`, "dark drone");

//  5. the rule itself, over every character
const ANCHOR = "    // pink: the three LFOs breathe width, tuning and filter";
if (s.indexOf(ANCHOR) < 0) miss.push("rule anchor x0");
const RULE = [
"    /*  BUGLIST 16, the rule itself: an effect may colour, widen, tremble and",
"        filter, but it may not put the instrument out of tune. At its own",
"        defaults NO character writes pitchSag, and the one pitch modulator we",
"        keep — tape WOW — is zero-mean, so the note always comes back. */",
"    {",
'        std::printf ("   -- no character bends the note at its defaults\\n");',
"        for (int c = 0; c < numCharacters(); ++c)",
"        {",
"            Rack r;",
"            r.prepare (fs, 512);",
"            r.setCharArmed (c, true);",
"            renderRack (r, L.data(), R.data(), N);",
"            const WorldMod w = r.worldMod();",
"            CHECK (w.pitchSag == 0.0f,",
'                   "%s bends the note at its defaults (sag %.3f)",',
"                   characterDescriptor (c).name, (double) w.pitchSag);",
"        }",
"",
"        //  tape WOW: a full wow cycle is 2 s at 0.5 Hz — its mean must be ~0",
"        Rack r;",
"        r.prepare (fs, 512);",
"        r.setCharArmed (cTape, true);",
"        double sum = 0; int n = 0; float lo = 1e9f, hi = -1e9f;",
"        for (int i = 0; i < 40; ++i)                    // 40 x 0.1 s = 4 s",
"        {",
"            renderRack (r, L.data(), R.data(), (int) (fs * 0.1));",
"            const float d = r.worldMod().detuneCents;",
"            sum += d; ++n; lo = std::min (lo, d); hi = std::max (hi, d);",
"        }",
"        const double mean = sum / n;",
'        std::printf ("   tape wow over 4 s: %.2f .. %.2f c, mean %.3f c\\n",',
"                     (double) lo, (double) hi, mean);",
"        CHECK (hi - lo > 1.0f, \"tape wow is not modulating at all\");",
"        CHECK (std::abs (mean) < 1.0, \"tape wow is not zero-mean (%.2f c)\", mean);",
"    }",
"",
""].join(NL);
if (!miss.length) s = s.replace(ANCHOR, RULE + ANCHOR);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench: 4 checks re-aimed, plus the rule over every character");
