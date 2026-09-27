// Second bench round: an exact peak finder (fine scan, no parabola), %d in a
// double-only formatter, the erosion bands, the preset silence rule, note 45
// in the sample-rate tests. Exact-count anchors; nothing written on a miss.
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/test/bench.cpp";
let s = fs.readFileSync(path, "utf8");
const edits = [
  ['static double peakHz (const Take& t, double lo, double hi, int from, int to, double stepCents = 5.0)\n{\n    double best = lo, bestP = -1; const double ratio = std::pow (2.0, stepCents / 1200.0);\n    for (double f = lo; f <= hi; f *= ratio) { const double p = goertzel (t, f, from, to); if (p > bestP) { bestP = p; best = f; } }\n    const double pl = goertzel (t, best / ratio, from, to), ph = goertzel (t, best * ratio, from, to);\n    const double d = 0.5 * (pl - ph) / std::max (1e-30, pl - 2 * bestP + ph);\n    return best * std::pow (ratio, clampf ((float) d, -1.0f, 1.0f));\n}',
   'static double peakHz (const Take& t, double lo, double hi, int from, int to, double stepCents = 10.0)\n{\n    // a coarse scan, then a 0.5-cent scan over +-1.5 steps: Goertzel evaluates the DFT exactly at any\n    // frequency, so no interpolation is needed (a parabola on a log grid was 7 cents off)\n    double best = lo, bestP = -1; const double ratio = std::pow (2.0, stepCents / 1200.0);\n    for (double f = lo; f <= hi; f *= ratio) { const double p = goertzel (t, f, from, to); if (p > bestP) { bestP = p; best = f; } }\n    double fine = best, fineP = bestP; const double r2 = std::pow (2.0, 0.5 / 1200.0);\n    for (double f = best * std::pow (ratio, -1.5); f <= best * std::pow (ratio, 1.5); f *= r2) { const double p = goertzel (t, f, from, to); if (p > fineP) { fineP = p; fine = f; } }\n    return fine;\n}'],
  ['fmt ("%.3f s (%d minima)", period, (double) minima.size())', 'fmt ("%.3f s (%.0f minima)", period, (double) minima.size())'],
  ['fmt ("rms %.4f, %d grains live", rms (t, t.n() / 2), (double) e->uiGrains)', 'fmt ("rms %.4f, %.0f grains live", rms (t, t.n() / 2), (double) e->uiGrains)'],
  ['fmt ("%d", (double) fires)', 'fmt ("%.0f", (double) fires)'],
  ['fmt ("%d", (double) fires)', 'fmt ("%.0f", (double) fires)'],
  ['fmt ("%d silent, %d over", (double) silent, (double) over)', 'fmt ("%.0f silent, %.0f over", (double) silent, (double) over)'],
  ['auto hfShare = [] (const Take& tk) { double lo = 0, hi = 0; for (double hz = 100; hz < 500; hz *= 1.1) lo += goertzel (tk, hz, 0, tk.n()); for (double hz = 2000; hz < 8000; hz *= 1.1) hi += goertzel (tk, hz, 0, tk.n()); return hi / std::max (1e-12, lo + hi); };',
   'auto hfShare = [] (const Take& tk) { double lo = 0, hi = 0; for (double hz = 80; hz < 350; hz *= 1.05) lo += goertzel (tk, hz, 0, tk.n()); for (double hz = 700; hz < 5000; hz *= 1.05) hi += goertzel (tk, hz, 0, tk.n()); return hi / std::max (1e-12, lo + hi); };'],
  ['Engine* x = fresh (sr); bareSine (*x); x->noteOn (57, 0.8f); Take tt = render (*x, 2.0);', 'Engine* x = fresh (sr); bareSine (*x); x->noteOn (45, 0.8f); Take tt = render (*x, 2.0);'],
  ['const float rr = rms (tt, tt.n() / 2), pk = peak (tt);', 'const float rr = std::max (rms (tt, 0, tt.n() / 2), rms (tt, tt.n() / 2)), pk = peak (tt);'],
];
let miss = [];
for (const [a] of edits) { const n = s.split(a).length - 1; if (n < 1) miss.push("0x: " + a.slice(0, 70)); }
if (miss.length) { console.log("ANCHOR MISS:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, b);
fs.writeFileSync(path, s);
console.log("bench patched, " + edits.length + " edits");
