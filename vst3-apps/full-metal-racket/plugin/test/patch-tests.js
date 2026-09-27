// Three bench corrections. Two of the three are the PROBE being wrong, which
// in this family is at least as common as the engine being wrong:
//   * the tension check counted the toms' own deliberate pitch envelope as a
//     tension failure;
//   * the HAT LINK check compared dominant frequencies of a metal cluster
//     seen through IDENTICAL band filters, which mostly measures the filters;
//   * brightness as "share of energy above 2.5 kHz" flips sign on a kick,
//     whose click dominates that band at low velocity. Spectral centroid.
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/FullMetalRacket/test/test.cpp";
let s = fs.readFileSync(P, "utf8");
const miss = [];
function rep(a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  s = s.split(a).join(b);
}

// ---- 1. tension: the toms carry a fixed 10% pitch envelope of their own ----
rep(`    ok (withT > 25.0, "a hard hit starts sharp when TENSION is up", withT, 25.0);
    ok (std::fabs (without) < 12.0, "and does not when TENSION is zero", without, 0.0);`,
`    /*  Toms carry a deliberate fixed pitch envelope of their own (10%), so
        "tension zero" is not "pitch flat" — it is about +37 cents, and the
        first version of this check called that a failure. What TENSION must
        do is add a lot on top of it. */
    ok (withT > without + 60.0, "a hard hit starts sharp when TENSION is up", withT, without + 60.0);
    ok (without < 60.0, "and the pitch envelope alone stays modest", without, 60.0);`,
"tension thresholds");

// ---- 2. HAT LINK: compare against the same tune set by hand ---------------
rep(`        const double fl = dominant (linked, 48000.0, 1000, 12000, 300.0, 9000.0);
        const double fu = dominant (free_,  48000.0, 1000, 12000, 300.0, 9000.0);
        std::printf ("    OH dominant: linked %.0f Hz, unlinked %.0f Hz\\n", fl, fu);
        ok (fl > fu * 1.15, "LINKed, the open hat follows the closed hat's tune", fl, fu * 1.15);`,
`        /*  The exact test, rather than a proxy: a LINKed open hat must render
            EXACTLY as an unlinked one whose own TUNE was set to the closed
            hat's. Comparing dominant frequencies instead mostly measured the
            band filters, which LINK does not touch. */
        Engine m; fresh (m);
        m.p.g[GP_BLEED] = 0.0f; m.p.g[GP_BODY] = 0.0f; m.p.g[GP_AGE] = 0.0f;
        m.p.g[GP_HATLINK] = 0.0f;
        for (int c = 0; c < NCH; ++c) if (c != 9) m.p.ch[c][CP_MUTE] = 1.0f;
        m.p.ch[9][CP_TUNE] = 0.9f;                 // by hand, what LINK should do
        m.trigger (9, 1.0f);
        Buf manual (24000); render (m, manual);

        double dLinked = 0.0, dFree = 0.0;
        for (size_t i = 0; i < manual.L.size(); ++i)
        {
            dLinked += std::fabs (manual.L[i] - linked.L[i]);
            dFree   += std::fabs (manual.L[i] - free_.L[i]);
        }
        std::printf ("    OH vs a hand-tuned twin: LINKed %.3e, unlinked %.3e\\n", dLinked, dFree);
        ok (dLinked < 1e-6, "LINKed, the open hat renders as the closed hat's tune", dLinked, 0.0);
        ok (dFree > 1.0, "unlinked, it keeps its own tune", dFree, 1.0);`,
"hat link test");

// ---- 3. brightness: spectral centroid ------------------------------------
rep(`        auto bright = [] (const Buf& b) -> double
        {
            double hi = 0.0, tot = 0.0;
            for (double f = 300.0; f < 12000.0; f *= 1.25)
            {
                const double m = goertzel (b, f, 48000.0, 0, 12000);
                tot += m;
                if (f > 2500.0) hi += m;
            }
            return hi / std::max (1e-12, tot);
        };`,
`        /*  Spectral centroid, not "share above 2.5 kHz". The share measure
            flips sign on a kick: its click owns that band, and at low velocity
            the click is a LARGER fraction of a much smaller sound. The
            centroid asks the question actually being asked — where is the
            weight of this sound? */
        auto bright = [] (const Buf& b) -> double
        {
            double num = 0.0, den = 0.0;
            for (double f = 120.0; f < 14000.0; f *= 1.12)
            {
                const double m = goertzel (b, f, 48000.0, 0, 12000);
                num += m * f; den += m;
            }
            return num / std::max (1e-12, den);
        };`,
"brightness measure");

rep(`        std::printf ("    %-6s brightness: soft %.3f, hard %.3f\\n", channelName (c), bs, bh);
        ok (bh > bs * 1.02, (std::string (channelName (c)) + ": harder is brighter").c_str(), bh, bs * 1.02);`,
`        std::printf ("    %-6s centroid: soft %.0f Hz, hard %.0f Hz\\n", channelName (c), bs, bh);
        ok (bh > bs * 1.02, (std::string (channelName (c)) + ": harder is brighter").c_str(), bh, bs * 1.02);`,
"brightness print");

// ---- 4. a velocity probe, so the next disagreement is measured ------------
rep(`    if (argc > 1 && std::strcmp (argv[1], "--spec") == 0)
    {
        spectrumDump (0, 0.45f, 12000, 40000);
        return 0;
    }`,
`    if (argc > 1 && std::strcmp (argv[1], "--spec") == 0)
    {
        spectrumDump (0, 0.45f, 12000, 40000);
        return 0;
    }
    if (argc > 1 && std::strcmp (argv[1], "--vel") == 0)
    {
        std::printf ("peak against velocity (the nonlinear damping is meant to squash, but not this much)\\n");
        for (int c : { 0, 2, 4, 8, 11 })
        {
            std::printf ("  %-6s", channelName (c));
            float first = 0.0f;
            for (float v : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                Engine e; fresh (e);
                e.p.g[GP_BLEED] = 0.0f; e.p.g[GP_BODY] = 0.0f; e.p.g[GP_AGE] = 0.0f; e.p.g[GP_SAG] = 0.0f;
                for (int k = 0; k < NCH; ++k) if (k != c) e.p.ch[k][CP_MUTE] = 1.0f;
                e.trigger (c, v);
                Buf b (48000); render (e, b);
                if (first == 0.0f) first = peak (b);
                std::printf ("   v%.2f %.4f (%.2fx)", v, peak (b), peak (b) / std::max (1e-6f, first));
            }
            std::printf ("\\n");
        }
        return 0;
    }`,
"velocity probe");

if (miss.length) { console.error("ABORT:\\n  " + miss.join("\\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("bench patched OK");
