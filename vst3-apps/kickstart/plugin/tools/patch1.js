// one-off patch (2026-09-25): ADAA drive, comp look-ahead, no DC blocker,
// snapping smoothers on the first block, a louder and longer click.
// Validates every anchor before writing anything.
const fs = require("fs");
const path = require("path");
const root = path.resolve(__dirname, "..");
function patch (p, E)
{
    let s = fs.readFileSync (p, "utf8");
    const miss = E.filter (([a]) => s.split (a).length !== 2).map (([a]) => a.slice (0, 70));
    if (miss.length) { console.log ("ABORT " + p, miss); process.exit (1); }
    for (const [a, b] of E) s = s.replace (a, () => b);
    return s;
}
const H = path.join (root, "engine/ks_engine.h"), C = path.join (root, "engine/ks_engine.cpp");

const h = patch (H, [
["    //  the latency of the decimator, in host samples\n    static constexpr int latencySamples() { return 23; }",
 "    //  the decimator (23 host samples) plus the compressor's look-ahead\n    //  (1.5 ms), constant whatever the settings so a host compensates once\n    static constexpr int kDecimatorLatency = 23;\n    int latencySamples() const { return kDecimatorLatency + laN; }"],
["    float dcX = 0.0f, dcY = 0.0f, dcR = 0.9999f;", "    double adaaPrev = 0.0;          // first-order antiderivative anti-aliasing"],
["    int dqMask = 0, holdN = 1;", "    int dqMask = 0, holdN = 1;\n    std::vector<float> laBuf;       // the compressor's look-ahead\n    int laN = 0, laPos = 0;\n    bool fresh = true;              // snap the smoothed controls on the first block"],
["float driveShape (int engine, float x, float k);", "float driveShape (int engine, float x, float k);\nfloat driveADAA  (int engine, float x, double xPrev, float k);   // first-order ADAA"],
]);

const ADAA_NS = `namespace
{
    double antiderivative (int e, double u, double a)
    {
        switch (e)
        {
            case ENG_RAZOR:
                return (1.0 - a) * logcosh (u) + a * (std::abs (u) <= 1.0 ? 0.5 * u * u : std::abs (u) - 0.5);
            case ENG_IDLE:
            {
                const double s = 0.6 + 0.4 * a;
                const double ft = u >= 0.0 ? logcosh (u) : s / a * logcosh (a * u);
                const double fc = std::abs (u) <= 1.0 ? 1.5 * (0.5 * u * u - u * u * u * u / 12.0)
                                                      : 0.625 + std::abs (u) - 1.0;
                return 0.3 * fc + 0.7 * ft;
            }
            case ENG_HYPER:
            {
                const double s = a * 3.0;
                const double w2 = std::max (0.0, 1.0 - std::abs (s)), w3 = std::max (0.0, 1.0 - std::abs (s - 1.0));
                const double w4 = std::max (0.0, 1.0 - std::abs (s - 2.0)), w5 = std::max (0.0, 1.0 - std::abs (s - 3.0));
                const double g = 0.9 / (w2 + w3 + w4 + w5 + 1.0e-6);
                auto P = [&] (double c)
                {
                    const double c2 = c * c, c3 = c2 * c, c4 = c2 * c2;
                    return 0.175 * c2 + g * (w2 * (2.0 / 3.0) * c3 + w3 * (c4 - 1.5 * c2)
                                             + w4 * (1.6 * c4 * c - (8.0 / 3.0) * c3)
                                             + w5 * ((8.0 / 3.0) * c4 * c2 - 5.0 * c4 + 2.5 * c2));
                };
                auto p = [&] (double c)
                {
                    const double c2 = c * c;
                    return 0.35 * c + g * (w2 * 2.0 * c2 + w3 * c * (4.0 * c2 - 3.0) + w4 * (8.0 * c2 * c2 - 8.0 * c2)
                                           + w5 * c * (16.0 * c2 * c2 - 20.0 * c2 + 5.0));
                };
                if (u > 1.0)  return P (1.0) + p (1.0) * (u - 1.0);
                if (u < -1.0) return P (-1.0) + p (-1.0) * (u + 1.0);
                return P (u);
            }
            default: return 0.0;
        }
    }
}

float driveADAA (int engine, float x, double xPrev, float k)
{
    if (engine == ENG_NOVA) return driveShape (engine, x, k);
    const double dx = (double) x - xPrev;
    if (std::abs (dx) < 1.0e-6)
        return driveShape (engine, (float) (0.5 * ((double) x + xPrev)), k);
    const double a = kDrive[engine].character;
    return (float) ((antiderivative (engine, (double) k * x, a) - antiderivative (engine, (double) k * xPrev, a)) / ((double) k * dx));
}

float driveMaxFor (int e)`;

const c = patch (C, [
["    dcR = (float) (1.0 - 2.0 * PI * 2.0 / fo);\n", ""],
["    holdN = std::max (1, (int) std::lround (0.012 * fs));",
 "    //  look-ahead 1.5 ms; the detector's window spans it plus 10 ms of hold,\n    //  longer than half a period of any kick fundamental, so a steady tone\n    //  is seen at its peak and the gain does not ripple\n    laN = std::max (1, (int) std::lround (0.0015 * fs));\n    laBuf.assign ((size_t) laN, 0.0f);\n    holdN = laN + std::max (1, (int) std::lround (0.010 * fs));"],
["    dcX = dcY = 0.0f;", "    adaaPrev = 0.0;\n    std::fill (laBuf.begin(), laBuf.end(), 0.0f); laPos = 0;\n    fresh = true;"],
["    P = p;\n", "    P = p;\n    if (fresh)\n    {\n        //  a preset loaded before the first audio should not ramp in\n        driveSm = p.drive; roomSm = p.room;\n        colourSm = p.colour >= 0.999f ? 1.0f : p.colour;\n        levelSm = p.level == 0.0f ? 1.0f : db2lin (p.level);\n        fresh = false;\n    }\n"],
["v.ceK  = std::exp (-1.0 / (lenMs / 3.0 * 0.001 * fo));", "v.ceK  = std::exp (-1.0 / (lenMs / 2.5 * 0.001 * fo));"],
["    const float makeup = compOn ? -gc (-1.0f) * 0.8f : 0.0f;",
 "    //  make-up restores most of what the compressor takes from a signal at\n    //  -6 dBFS: enough that turning COMP up does not turn the kick down\n    const float makeup = compOn ? -gc (-6.0f) * 0.7f : 0.0f;"],
["        if (compOn)\n        {\n            const float ax = std::abs (y);",
 "        //  the audio is always delayed by the look-ahead, so the latency is\n        //  the same with the compressor in or out\n        const float ynow = y;\n        y = laBuf[(size_t) laPos];\n        laBuf[(size_t) laPos] = ynow;\n        if (++laPos >= laN) laPos = 0;\n        if (compOn)\n        {\n            const float ax = std::abs (ynow);"],
["    const float clickAmt = clampf (p.click, 0.0f, 1.0f) * 0.9f;", "    const float clickAmt = clampf (p.click, 0.0f, 1.0f) * 1.2f;"],
["        driveSm += aSm * (p.drive - driveSm);", "        const float xIn = x;\n        driveSm += aSm * (p.drive - driveSm);"],
["            const float yv = driveShape (eng, x, k) * driveTrimFor (eng, driveSm);",
 "            const float yv = driveADAA (eng, x, adaaPrev, k) * driveTrimFor (eng, driveSm);"],
["        // DC: asymmetric drive leaves some, and a hit that starts at zero\n        // phase carries a little by nature. 2 Hz, the one always-on filter.\n        {\n            const float yd = x - dcX + dcR * dcY;\n            dcX = x; dcY = (std::abs (yd) < 1.0e-9f) ? 0.0f : yd;    // -180 dB: flushed, so a kick ENDS\n            x = dcY;\n        }\n",
 "        adaaPrev = xIn;\n        /*  NO DC blocker. A hit that starts at zero phase carries some DC by\n            nature, and a 2 Hz high-pass turned it into a subsonic hump about\n            30 dB down that outlived the kick by a second - measured, and it\n            made DECAY read twice as long as it is. A kick is a transient; its\n            low end is left alone. */\n"],
["    //  a panel preview starts from the settled state of the smoothed controls\n    e->driveSm = p.drive; e->colourSm = p.colour; e->roomSm = p.room;\n    e->levelSm = p.level == 0.0f ? 1.0f : db2lin (p.level);\n", ""],
["    constexpr int TRIM_PTS = 17;",
 "    //  ---- first-order antiderivative anti-aliasing (ADAA) ----------------\n    //  y = (G(k x) - G(k x')) / (k (x - x')), G the antiderivative of the\n    //  shaper. A hard clip at 4x still folds its highest harmonics back;\n    //  this takes a large part of that away for the three smooth engines.\n    //  SUPERNOVA quantises on purpose - its aliasing is its sound - so it\n    //  runs plain.\n    inline double logcosh (double u) { u = std::abs (u); return u + std::log1p (std::exp (-2.0 * u)) - 0.6931471805599453; }\n\n    constexpr int TRIM_PTS = 17;"],
["float driveMaxFor (int e)", ADAA_NS],
]);
fs.writeFileSync (H, h);
fs.writeFileSync (C, c);
console.log ("ok");
