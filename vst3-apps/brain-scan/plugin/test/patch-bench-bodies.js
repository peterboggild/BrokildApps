// bench section 10 — the second head, split lines, scan spread, the MOD line
// on the read, and the bodies measured as anatomy.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "bench.cpp");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 60)}...], found ${n}`); return; }
  s = s.split(f).join(t);
};

rep(String.raw`#include "../Source/Import.h"`, String.raw`#include "../Source/Import.h"
#include "../Source/Anatomy.h"`);

rep(String.raw`    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`,
    String.raw`    //==========================================================================
    head ("10 - the second head, split lines, scan spread, the MOD line on the read, the bodies (260905.1)");
    {
        auto thdOf = [&] (const std::vector<double>& m, double f, int kmax)
        {
            const double h1 = harmMag (m, f, 1, N);
            double hs = 0; for (int k = 2; k <= kmax; ++k) { const double h = harmMag (m, f, k, N); hs += h * h; }
            return dB (std::sqrt (hs) / std::max (1e-12, h1));
        };
        //  ---- the second head at 1x, half a cycle behind: the odd harmonics cancel ----
        {
            Line six[NLINES]; straightAll (six, 0.8f, 0.0f);            // SPINE, all harmonics
            double oe[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.head2 = (float) q; e->p.head2Ratio = 0.5f; e->p.head2Phase = 0.5f;
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                double odd = 0, even = 0;
                for (int k = 1; k <= 9; ++k) { const double h = harmMag (m, f0, k, N); if (k & 1) odd += h * h; else even += h * h; }
                oe[q] = dB (std::sqrt (odd / std::max (1e-30, even)));
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "odd/even: %.1f dB with one head, %.1f dB with the second head half a cycle behind", oe[0], oe[1]);
            std::printf ("  %s\n", d);
            ok (oe[0] > 3.0 && oe[1] < -25.0, "a second head at 1x, PHASE 50 %: the odd harmonics cancel (a fixed-interval double, no second voice)", d);
        }
        //  ---- the second head off the integers: a partial that is not a harmonic ----
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);            // SINUS
            Engine* e = fresh (0, six);
            e->p.head2 = 1.0f; e->p.head2Ratio = 0.5f + std::log2 (1.5f) / 4.0f;   // 1.5x exactly
            Take t = render (*e, 57, 0.8);
            const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
            const double h1 = harmMag (m, f0, 1, N), p15 = harmMag (m, f0 * 1.5, 1, N), h2 = harmMag (m, f0, 2, N);
            char d[128]; std::snprintf (d, sizeof d, "at HEAD RATIO 1.5x: the partial at 1.5 f0 sits %.1f dB from f0, the 2nd harmonic %.1f dB", dB (p15 / h1), dB (h2 / h1));
            std::printf ("  %s\n", d);
            ok (dB (p15 / h1) > -6.0 && dB (h2 / h1) < -40.0, "a partial at 1.5 f0, and no 2nd harmonic: inharmonic by the read, not by the field", d);
            delete e;
            //  and at 4x on the brightest field at C5, coarsened a level, it does not alias
            Line sb[NLINES]; straightAll (sb, 1.0f, 0.0f);
            Engine* e2 = fresh (1, sb);
            e2->p.head2 = 1.0f; e2->p.head2Ratio = 1.0f; e2->p.grain = 1.0f;
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            Take t2 = render (*e2, 72, 0.8);
            const auto m2 = spectrum (t2.L, (size_t) (0.3 * SR), N);
            const double fl = aliasFloorDb (m2, fC5, N);
            std::snprintf (d, sizeof d, "HEAD RATIO 4x, SPINE y = 1, GRAIN 1, C5: non-harmonic floor %.1f dB", fl);
            std::printf ("  %s\n", d);
            ok (fl < -40.0, "the fast head reads a coarser level and stays 40 dB clean", d);
            delete e2;
        }
        //  ---- a SPLIT line: two segments, two edges, both band-limited ------
        {
            Line a; a.n = 4; a.split = 2;
            a.p[0] = { 0.05f, 0.30f, 0.0f }; a.p[1] = { 0.50f, 0.30f, 0.0f };
            a.p[2] = { 0.50f, 0.90f, 0.0f }; a.p[3] = { 0.95f, 0.90f, 0.0f };
            const Vec3 before = a.at (0.49999f), after = a.at (0.5f), end = a.at (0.99999f), start = a.at (0.0f);
            char d[160];
            std::snprintf (d, sizeof d, "at 0.49999 -> (%.3f, %.3f), at 0.5 -> (%.3f, %.3f); at 0 (%.3f, %.3f), at 1 (%.3f, %.3f)",
                           before.x, before.y, after.x, after.y, start.x, start.y, end.x, end.y);
            std::printf ("  %s\n", d);
            ok (std::abs (before.y - 0.30f) < 0.01f && std::abs (after.y - 0.90f) < 0.01f && std::abs (before.x - 0.5f) < 0.01f
                && std::abs (start.x - 0.05f) < 0.01f && std::abs (end.x - 0.95f) < 0.01f,
                "the two halves of the cycle read the two segments, end to end", d);
            Line six[NLINES]; for (int i = 0; i < NLINES; ++i) six[i] = a;
            const double fC5 = 440.0 * std::pow (2.0, (72 - 69) / 12.0);
            double fl[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (0, six);                                  // SINUS: the amplitude jumps at both edges
                e->setBlep (q == 0);
                Take t = render (*e, 72, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                fl[q] = aliasFloorDb (m, fC5, N);
                delete e;
            }
            std::snprintf (d, sizeof d, "SINUS on a split line at C5: non-harmonic %.1f dB with both edges band-limited, %.1f dB without", fl[0], fl[1]);
            std::printf ("  %s\n", d);
            ok (fl[0] < -40.0 && fl[1] - fl[0] > 10.0, "the second edge, at half a cycle, gets its own polyBLEP and it is worth > 10 dB", d);
        }
        //  ---- SCAN SPREAD: unison readers on different scans, bounded --------
        {
            Line six[NLINES]; straightAll (six, 0.2f, 0.0f);
            for (int i = 0; i < NLINES; i += 2) six[i + 1] = Line::straight ({ 0.0f, 0.9f, 0.0f }, { 1.0f, 0.9f, 0.0f });
            Take t[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (1, six);
                e->p.unison = 1.0f; e->p.scan = 0.5f; e->p.uniScan = q == 0 ? 0.0f : 0.7f;
                t[q] = render (*e, 57, 0.6);
                delete e;
            }
            double diff = 0, pk = 0;
            for (size_t i = (size_t) (0.2 * SR); i < t[0].L.size(); ++i) { diff += (double) (t[0].L[i] - t[1].L[i]) * (t[0].L[i] - t[1].L[i]); pk = std::max (pk, (double) std::abs (t[1].L[i])); }
            const double r0 = rms (t[0].L, (size_t) (0.2 * SR), t[0].L.size());
            const double dr = std::sqrt (diff / (double) (t[0].L.size() - (size_t) (0.2 * SR)));
            char d[128]; std::snprintf (d, sizeof d, "four readers, SCAN SPREAD 0 -> 0.7: the output changes by %.1f %% rms, peak %.3f", dr / r0 * 100.0, pk);
            std::printf ("  %s\n", d);
            ok (dr / r0 > 0.05 && pk <= 1.0, "the readers spread through the body, and it stays bounded", d);
        }
        //  ---- the MOD line on the window ------------------------------------
        {
            Line six[NLINES]; straightAll (six, 0.5f, 0.0f);
            //  a MOD line that reads one place: the trough of the sine, modV about -0.68
            six[L_MOD_A] = Line::straight ({ 0.75f, 0.5f, 0.0f }, { 0.75f, 0.5f, 0.0f });
            six[L_MOD_B] = six[L_MOD_A];
            double thd[2];
            for (int q = 0; q < 2; ++q)
            {
                Engine* e = fresh (0, six);
                e->p.modContrast = q == 0 ? 0.5f : 0.0f;                    // centred, then -100 %
                Take t = render (*e, 57, 0.8);
                const auto m = spectrum (t.L, (size_t) (0.3 * SR), N);
                thd[q] = thdOf (m, f0, 24);
                delete e;
            }
            char d[128]; std::snprintf (d, sizeof d, "SINUS THD: MOD>WINDOW centred %.1f dB, at -100 %% with the MOD line in the trough %.1f dB", thd[0], thd[1]);
            std::printf ("  %s\n", d);
            ok (thd[0] < -60.0 && thd[1] > -25.0, "centred, the MOD line leaves the window alone; turned up, the tissue decides how hard it is read", d);
        }
        //  ---- the bodies: anatomy, measured ----------------------------------
        {
            ok (an::toUnit (-1000.0f) == 0.0f && an::toUnit (2000.0f) == 1.0f && std::abs (an::toUnit (40.0f) - 0.3467f) < 0.001f,
                "the Hounsfield scale: air at 0, enamel at 1, soft tissue at 0.347");
            struct Want { int spec; const char* name; double airMin, boneMin, boneMax; bool enamel; };
            const Want wants[] = {
                { 6,  "THORAX",   0.20, 0.006, 0.12, false },
                { 7,  "SKULL",    0.55, 0.02,  0.25, true  },
                { 8,  "CORTEX",   0.25, 0.02,  0.25, true  },
                { 12, "VERTEBRA", 0.00, 0.03,  0.35, false },
                { 13, "FEMUR",    0.10, 0.02,  0.25, false },
                { 14, "JAW",      0.05, 0.03,  0.30, true  },
            };
            std::vector<float> cube ((size_t) VN * VN * VN);
            for (const Want& w : wants)
            {
                ok (specimenIsHu (w.spec), "built in HU", specimenName (w.spec));
                buildSpecimen (w.spec, cube.data());
                size_t air = 0, bone = 0, enamel = 0;
                for (float v : cube) { if (v < 0.05f) ++air; if (v > 0.6f) ++bone; if (v >= 0.98f) ++enamel; }
                const double fa = (double) air / (double) cube.size(), fb = (double) bone / (double) cube.size(), fe = (double) enamel / (double) cube.size();
                char d[160]; std::snprintf (d, sizeof d, "%-8s air %.1f %%, bone %.1f %%, enamel %.3f %%", w.name, fa * 100, fb * 100, fe * 100);
                std::printf ("  %s\n", d);
                ok (fa >= w.airMin && fb >= w.boneMin && fb <= w.boneMax && (! w.enamel || fe > 0.0002),
                    "the tissue fractions of a body: air where there is air, a few per cent of bone, enamel where there are teeth", d);
            }
            //  the skull is closed: rays from the centre of the head cross bone
            {
                buildSpecimen (7, cube.data());
                auto at = [&] (float x, float y, float z) { const int i = (int) (x * VN), j = (int) (y * VN), k = (int) (z * VN);
                    return cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i]; };
                const float dirs[5][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };
                int crossed = 0;
                for (auto& dd : dirs)
                {
                    float mx = 0;
                    for (float t = 0.0f; t < 0.49f; t += 0.004f) mx = std::max (mx, at (0.5f + dd[0] * t, 0.48f + dd[1] * t, 0.55f + dd[2] * t));
                    if (mx > 0.6f) ++crossed;
                }
                char d[96]; std::snprintf (d, sizeof d, "%d of 5 rays from the centre of the head cross bone", crossed);
                std::printf ("  %s\n", d);
                ok (crossed == 5, "the skull is a closed vault", d);
                //  and it is not solid: the middle of the SKULL specimen is air (no soft tissue)
                ok (at (0.5f, 0.48f, 0.55f) < 0.05f, "SKULL holds no soft tissue: the cranium is empty");
                buildSpecimen (8, cube.data());
                ok (std::abs (at (0.5f, 0.48f, 0.55f) - an::toUnit (30.0f)) < 0.03f, "CORTEX holds a brain: white matter at the same place");
            }
            //  the thorax has two lungs
            {
                buildSpecimen (6, cube.data());
                const int k = (int) (0.55f * VN);
                size_t left = 0, right = 0, n = 0;
                for (int j = 0; j < VN; ++j) for (int i = 0; i < VN; ++i)
                {
                    const float v = cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i];
                    const float x = ((float) i + 0.5f) / VN, y = ((float) j + 0.5f) / VN;
                    const bool inChest = (x - 0.5f) * (x - 0.5f) / (0.43f * 0.43f) + (y - 0.5f) * (y - 0.5f) / (0.29f * 0.29f) < 0.64f;
                    if (! inChest) continue;
                    ++n;
                    if (v < 0.12f) { if (x < 0.45f) ++left; if (x > 0.55f) ++right; }
                }
                char d[96]; std::snprintf (d, sizeof d, "air inside the chest at mid-height: %.1f %% on one side, %.1f %% on the other", 100.0 * left / n, 100.0 * right / n);
                std::printf ("  %s\n", d);
                ok (left > n / 10 && right > n / 10, "two lungs", d);
            }
            //  the vertebra has a canal with bone either side
            {
                buildSpecimen (12, cube.data());
                auto at = [&] (float x, float y, float z) { const int i = (int) (x * VN), j = (int) (y * VN), k = (int) (z * VN);
                    return cube[((size_t) k * VN + (size_t) j) * VN + (size_t) i]; };
                const float canal = at (0.5f, 0.40f, 0.5f), l = at (0.5f - 0.133f, 0.408f, 0.5f), r = at (0.5f + 0.133f, 0.408f, 0.5f);
                char d[128]; std::snprintf (d, sizeof d, "canal %.3f (CSF is %.3f), pedicles %.2f / %.2f", canal, an::toUnit (8.0f), l, r);
                std::printf ("  %s\n", d);
                ok (std::abs (canal - an::toUnit (8.0f)) < 0.03f && l > 0.6f && r > 0.6f, "a canal of CSF between two pedicles of bone", d);
            }
        }
        //  ---- cost with the second head on -----------------------------------
        {
            Line six[NLINES]; Params p; factory (2).build (six, p);
            p.unison = 1.0f; p.head2 = 1.0f; p.head2Ratio = 0.6f; p.grain = 1.0f; p.contrast = 0.5f;
            Engine eng; eng.p = p; eng.prepare (SR, BLK); eng.setLines (six); eng.service();
            for (int v = 0; v < 8; ++v) eng.noteOn (36 + v * 5, 0.9f);
            std::vector<float> l (BLK), r (BLK);
            const auto t0 = std::chrono::steady_clock::now();
            const int nb = (int) (5.0 * SR / BLK);
            for (int b = 0; b < nb; ++b) eng.process (l.data(), r.data(), BLK);
            const double wall = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            char d[96]; std::snprintf (d, sizeof d, "%.2f %% of one core, 32 readers each with a second head, window on", wall / 5.0 * 100.0);
            std::printf ("  %s\n", d);
            ok (wall / 5.0 < 0.28, "64 heads under 28 % of a core", d);
        }
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`);

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("bench: section 10 added");
