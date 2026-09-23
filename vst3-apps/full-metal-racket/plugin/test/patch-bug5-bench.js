/*  Bench for FMR buglist 5 (PUNCH). The spec names the three tests it has to
    pass, and two interactions it says to MEASURE rather than assume: with
    RAIL SAG (the opposite gesture — the rail dips as punch lifts) and with
    the ceilings (a transient boost pushed into a limiter is how slam works,
    so they are coupled by design).
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
const miss = [];

const GROUP = [
"//==============================================================================",
"static void groupPunch()",
"{",
'    std::printf ("\\n-- 26. PUNCH ----------------------------------------------------\\n");',
"",
"    auto hit = [] (int ch, float punch, int n) -> Buf",
"    {",
"        Engine e; fresh (e);",
"        e.p.g[GP_PUNCH] = punch;",
"        Buf b (n);",
"        e.trigger (ch, 0.9f);",
"        render (e, b);",
"        return b;",
"    };",
"    auto rms = [] (const Buf& b, int from, int to) -> double",
"    {",
"        double a = 0; int n = 0;",
"        for (int i = from; i < to && i < (int) b.L.size(); ++i) { a += (double) b.L[i] * b.L[i]; ++n; }",
"        return n ? std::sqrt (a / n) : 0.0;",
"    };",
"",
"    /*  EXACTLY ABSENT AT ZERO. The same contract RAIL SAG, BLEED, AGE and",
"        the BWFX macros keep: g is an IEEE-exact 1.0f, so the machine is",
"        bit-identical to one whose PUNCH was never touched. */",
"    {",
"        Engine a; fresh (a);",
"        Engine b; fresh (b); b.p.g[GP_PUNCH] = 0.0f;",
"        Buf x (24000), y (24000);",
"        a.trigger (0, 0.9f); render (a, x);",
"        b.trigger (0, 0.9f); render (b, y);",
"        ok (std::memcmp (x.L.data(), y.L.data(), x.L.size() * sizeof (float)) == 0,",
'            "PUNCH at zero is not bit-identical");',
"    }",
"",
"    //  the strike lifts and the body leans: that is what slam is",
"    {",
"        Buf off = hit (0, 0.0f, 24000), on = hit (0, 1.0f, 24000);",
"        const double aOff = rms (off, 0, 200), aOn = rms (on, 0, 200);",
"        const double bOff = rms (off, 4000, 20000), bOn = rms (on, 4000, 20000);",
"        const double dAtt = 20.0 * std::log10 ((aOn + 1e-12) / (aOff + 1e-12));",
"        const double dSus = 20.0 * std::log10 ((bOn + 1e-12) / (bOff + 1e-12));",
'        std::printf ("    kick: strike %+.2f dB, body %+.2f dB\\n", dAtt, dSus);',
"        ok (dAtt > 1.5, \"PUNCH does not lift the strike\", dAtt, 1.5);",
"        ok (dSus < -0.5, \"PUNCH does not lean on the body\", dSus, -0.5);",
"    }",
"",
"    /*  ...but a cymbal ringing for seconds must keep its decay. The sustain",
"        half is nearly off on the metal, or the control reads as a broken",
"        release rather than as punch. */",
"    {",
"        Buf off = hit (10, 0.0f, 96000), on = hit (10, 1.0f, 96000);",
"        const double tOff = rms (off, 40000, 90000), tOn = rms (on, 40000, 90000);",
"        const double d = 20.0 * std::log10 ((tOn + 1e-12) / (tOff + 1e-12));",
'        std::printf ("    cymbal tail: %+.2f dB\\n", d);',
"        ok (d > -1.5, \"PUNCH collapses the cymbal decay\", d, -1.5);",
"    }",
"",
"    /*  PUNCH and RAIL SAG are opposite gestures acting on the same first",
"        milliseconds — the spec says to measure the interaction rather than",
"        assume it. Together they must stay bounded and must not cancel. */",
"    {",
"        Engine e; fresh (e);",
"        e.p.g[GP_PUNCH] = 1.0f;",
"        e.p.g[GP_SAG]   = 1.0f;",
"        Buf b (48000);",
"        for (int k = 0; k < 12; ++k) e.trigger (k, 1.0f);",
"        render (e, b);",
'        std::printf ("    punch + full rail sag, whole kit: peak %.3f\\n", (double) peak (b));',
"        ok (finiteAll (b), \"punch with rail sag went non-finite\");",
"        ok (peak (b) <= 1.0f, \"punch with rail sag broke the ceiling\", peak (b), 1.0);",
"        Buf q = hit (0, 0.0f, 48000);",
"        ok (std::fabs ((double) peak (b) - (double) peak (q)) > 1e-4,",
'            "punch and rail sag cancelled each other exactly");',
"    }",
"",
"    //  and into the ceilings, at the loudest thing the machine can do",
"    {",
"        Engine e; fresh (e);",
"        e.p.g[GP_PUNCH]  = 1.0f;",
"        e.p.g[GP_VOLUME] = 1.0f;",
"        Buf b (48000);",
"        for (int r = 0; r < 4; ++r)",
"            for (int k = 0; k < 12; ++k) e.trigger (k, 1.0f);",
"        render (e, b);",
'        std::printf ("    punch into the ceilings: peak %.3f\\n", (double) peak (b));',
"        ok (finiteAll (b), \"punch into the ceilings went non-finite\");",
"        ok (peak (b) <= 1.0f, \"punch into the ceilings clipped\", peak (b), 1.0);",
"    }",
"}",
"",
""].join(NL);

const A1 = "//==============================================================================" + NL + "static void groupSeeds()";
if (s.split(A1).length - 1 !== 1) miss.push("group anchor x" + (s.split(A1).length - 1));
else s = s.replace(A1, GROUP + A1);

const A2 = "    groupMorph();";
if (s.split(A2).length - 1 !== 1) miss.push("call anchor x" + (s.split(A2).length - 1));
else s = s.replace(A2, A2 + NL + "    groupPunch();");

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench: PUNCH group added");
