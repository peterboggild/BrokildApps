// one-off bench corrections (2026-09-25). Validates every anchor first.
const fs = require("fs");
const path = require("path");
const p = path.resolve(__dirname, "../test/bench.cpp");
let s = fs.readFileSync(p, "utf8");
const E = [
// latency: now decimator + look-ahead, and no DC blocker lead any more
["        const double dcLead = std::atan (2.0 / f);\n        const double samples = (lag + dcLead) / (2 * PI * f / fs);\n        std::printf (\"  latency %.2f samples (reported %d)\\n\", samples, Engine::latencySamples());\n        CHECK (std::abs (samples - Engine::latencySamples()) < 0.6, \"the latency is %.2f samples, not %d\", samples, Engine::latencySamples());",
 "        Engine le; le.prepare (fs, 256);\n        const int L = le.latencySamples();\n        const double samples = lag / (2 * PI * f / fs);\n        std::printf (\"  latency %.2f samples (reported %d: decimator + 1.5 ms look-ahead)\\n\", samples, L);\n        CHECK (std::abs (samples - L) < 0.6, \"the latency is %.2f samples, not %d\", samples, L);"],
// click: measure what a click IS - fast change - by the first difference
["        const double hfa = rmsDb (xa, 0, 240), hfb = rmsDb (xb, 0, 240);\n        std::printf (\"  CLICK 0 -> 100: first 5 ms %.1f -> %.1f dB\\n\", hfa, hfb);",
 "        auto diffDb = [] (const std::vector<float>& x, int a, int b)\n        { double e = 0; for (int i = a + 1; i < b; ++i) { const double d = x[(size_t) i] - x[(size_t) i - 1]; e += d * d; } return 10 * std::log10 (e / (b - a) + 1e-30); };\n        const double hfa = diffDb (xa, 0, 480), hfb = diffDb (xb, 0, 480);\n        std::printf (\"  CLICK 0 -> 100: rate of change in the first 10 ms %.1f -> %.1f dB\\n\", hfa, hfb);"],
["        CHECK (hfb - hfa > 3.0, \"CLICK adds nothing at the attack\");",
 "        CHECK (hfb - hfa > 12.0, \"CLICK adds nothing at the attack\");"],
// transient smoothness: fit a SMOOTH gain (per 5 ms, interpolated) between
// the shaped and unshaped kick; what is left over is modulation distortion
["        //  and it is smooth: a steady-pitched shaped kick has no more\n        //  inharmonic content than the unshaped one\n        Params p = pure (80, 0, 1500); p.curve = 0.3f; p.attack = 1; p.sustain = 1;\n        Params q = p; q.attack = 0; q.sustain = 0;\n        const double ip = inharmonicDb (one (p, 48000.0, 0.6), 2400, 80, 48000.0);\n        const double iq = inharmonicDb (one (q, 48000.0, 0.6), 2400, 80, 48000.0);\n        std::printf (\"  transient shaping adds no distortion: off-harmonic energy %.1f dB (unshaped %.1f)\\n\", ip, iq);\n        CHECK (ip < iq + 6.0 && ip < -60.0, \"the transient shaper distorts (%.1f dB)\", ip);",
 "        //  and it is SMOOTH: the shaped kick is the unshaped one times a\n        //  gain that moves slowly. Fit that gain every 5 ms, interpolate, and\n        //  what is left is the modulation a rippling detector would add.\n        Params p = pure (60, 0, 1500); p.curve = 0.3f; p.attack = 0; p.sustain = 1;\n        Params q = p; q.sustain = 0;\n        const auto xs = one (p, 48000.0, 0.8), xu = one (q, 48000.0, 0.8);\n        const int B = 240, a0 = 4800, a1 = 36000;\n        std::vector<double> gb;\n        for (int b = a0; b + B <= a1; b += B)\n        { double n2 = 0, d2 = 0; for (int i = b; i < b + B; ++i) { n2 += (double) xs[(size_t) i] * xu[(size_t) i]; d2 += (double) xu[(size_t) i] * xu[(size_t) i]; } gb.push_back (n2 / (d2 + 1e-30)); }\n        double res = 0, sig = 0;\n        for (int i = a0 + B / 2; i < a1 - B; ++i)\n        {\n            const double fpos = (double) (i - a0 - B / 2) / B; const int k0 = (int) fpos; const double fr = fpos - k0;\n            const double g = gb[(size_t) k0] + (gb[(size_t) k0 + 1] - gb[(size_t) k0]) * fr;\n            const double e = xs[(size_t) i] - g * xu[(size_t) i];\n            res += e * e; sig += (double) xs[(size_t) i] * xs[(size_t) i];\n        }\n        const double rdb = 10 * std::log10 (res / sig + 1e-30);\n        std::printf (\"  SUSTAIN +100 is a smooth gain: residual after a 5 ms gain fit %.1f dB\\n\", rdb);\n        CHECK (rdb < -50.0, \"the transient shaper's gain ripples (%.1f dB)\", rdb);"],
// grit: like against like (both held flat)
["        const double ia = inharmonicDb (one (pure (80, 0, 3000)), 2400, 80, 48000.0);\n        Params gs = pure (80, 0, 3000); gs.curve = 1; gs.grit = 0.55f;",
 "        Params gc0 = pure (80, 0, 3000); gc0.curve = 1;\n        const double ia = inharmonicDb (one (gc0), 2400, 80, 48000.0);\n        Params gs = gc0; gs.grit = 0.55f;"],
// colour: 30 % puts the corner near 2.3 kHz; look above it
["        const double hfa = ampDb (xa, 0, 480, 3000, 48000.0), hfc = ampDb (xc, 0, 480, 3000, 48000.0);\n        std::printf (\"  COLOUR 30 %%: 3 kHz in the attack %+.1f dB\\n\", hfc - hfa);",
 "        const double hfa = ampDb (xa, 0, 480, 6000, 48000.0), hfc = ampDb (xc, 0, 480, 6000, 48000.0);\n        std::printf (\"  COLOUR 30 %%: 6 kHz in the attack %+.1f dB\\n\", hfc - hfa);"],
];
const miss = E.filter(([a]) => s.split(a).length !== 2).map(([a]) => a.slice(0, 80));
if (miss.length) { console.log("ABORT", miss); process.exit(1); }
for (const [a, b] of E) s = s.replace(a, () => b);
fs.writeFileSync(p, s);
console.log("ok");
