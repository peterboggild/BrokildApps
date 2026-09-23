// Engine internals under a magnifying glass. Not a gate - the thing that settles
// what a failing gate means.  probe direct|fdn|motion|hrtf
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdlib>
using namespace tw;

static void run (Engine& e, const Params& p, std::vector<float>& L, std::vector<float>& R, int n, int at)
{
    L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
    e.setParams (p);
    for (int i = 0; i < n; i += 128)
    {
        const int m = std::min (128, n - i);
        for (int k = 0; k < m; ++k) { const float v = (i + k == at) ? 1.0f : 0.0f; L[(size_t) (i + k)] = v; R[(size_t) (i + k)] = v; }
        e.process (&L[(size_t) i], &R[(size_t) i], m);
    }
}

int main (int argc, char** argv)
{
    const std::string what = argc > 1 ? argv[1] : "direct";
    const double fs = 48000.0;

    if (what == "hrtf")
    {
        Hrtf h; h.prepare (fs);
        std::vector<float> l ((size_t) h.numTaps()), r ((size_t) h.numTaps());
        for (float az : { 0.0f, 45.0f, 90.0f, 180.0f, 270.0f })
        {
            float it; h.lookup (az, 0, l.data(), r.data(), it);
            double el = 0, er = 0; int pl = 0, pr = 0;
            for (int i = 0; i < h.numTaps(); ++i) { el += l[(size_t) i] * l[(size_t) i]; er += r[(size_t) i] * r[(size_t) i]; if (std::abs (l[(size_t) i]) > std::abs (l[(size_t) pl])) pl = i; if (std::abs (r[(size_t) i]) > std::abs (r[(size_t) pr])) pr = i; }
            std::printf ("az %5.0f: itd %+7.1f samples  L energy %.3f peak@%d  R energy %.3f peak@%d  taps %d\n", az, it, el, pl, er, pr, h.numTaps());
            std::printf ("   L: "); for (int i = 0; i < 8; ++i) std::printf ("%+.3f ", l[(size_t) i]); std::printf ("\n   R: "); for (int i = 0; i < 8; ++i) std::printf ("%+.3f ", r[(size_t) i]); std::printf ("\n");
        }
        return 0;
    }

    if (what == "direct")
    {
        Engine e; e.prepare (fs, 256);
        Params p;
        p.material[2] = 0; p.door[0] = p.door[1] = p.door[2] = 0;
        p.lisX = 12; p.lisY = 4.5f; p.lisYaw = 90; p.src[0].type = 0;
        p.src[0].x = 14; p.src[0].y = 4.5f; p.src[0].z = EAR_HEIGHT;          // 2 m to the RIGHT of a north-facing listener
        p.earlyDb = -60; p.reverbDb = -60;
        std::vector<float> L, R; run (e, p, L, R, argc > 2 ? atoi (argv[2]) : 4000, 100);
        const Scene& sc = e.scene();
        std::printf ("paths in scene: %d (active %d)\n", sc.npaths, e.numActivePaths());
        for (int i = 0; i < sc.npaths; ++i) std::printf ("  kind %d pts %d db %.1f ms %.2f\n", (int) sc.paths[i].kind, sc.paths[i].npts, sc.paths[i].db, sc.paths[i].ms);
        for (int i = 0; i < MAX_PATHS; ++i) { const PathSlot& s = e.slot (i); if (! s.active) continue; float hm = 0, tm = 0, xm = 0; for (float v : s.hL) hm = std::max (hm, std::abs (v)); for (float v : s.hLTarget) tm = std::max (tm, std::abs (v)); for (float v : s.hist) xm = std::max (xm, std::abs (v)); std::printf ("  slot %2d kind %d key %u gain %.4f->%.4f delay %.1f filt.g %.3f k %.2f %.2f %.2f a1 %.4f |hL| %.3f |hLT| %.3f |hist| %.3f itd %.1f gL %.2f\n", i, (int) s.kind, s.key, s.gain, s.gainTarget, s.delay, s.filt.g, s.filt.k1, s.filt.k2, s.filt.k3, s.filt.a1, hm, tm, xm, s.itd, s.gL); if (s.kind == PathKind::Direct) break; }
        int first = -1; for (int i = 0; i < (int) L.size(); ++i) if (std::abs (L[(size_t) i]) > 1e-5f || std::abs (R[(size_t) i]) > 1e-5f) { first = i; break; }
        std::printf ("first output at %d (expected ~ %d)\n", first, 100 + (int) (2.0 / 343.0 * fs));
        for (int i = std::max (0, first - 2); i < std::min ((int) L.size(), first + 60); ++i) std::printf ("%5d  L %+.5f   R %+.5f\n", i, L[(size_t) i], R[(size_t) i]);
        return 0;
    }

    if (what == "fdn")
    {
        for (int open = 0; open < 2; ++open)
        {
            Engine e; e.prepare (fs, 256);
            Params p; p.material[0] = p.material[1] = p.material[2] = 2;
            p.door[0] = p.door[1] = p.door[2] = open ? 1.0f : 0.0f;
            p.src[0].x = 2; p.src[0].y = 2; p.lisX = 4; p.lisY = 3; p.lisYaw = 180; p.src[0].type = 0;
            p.directDb = -60; p.earlyDb = -60;
            std::vector<float> L, R; run (e, p, L, R, (int) (6 * fs), 2400);   /* past the fresh-path gain ramp */
            std::printf ("doors %s:", open ? "open" : "shut");
            for (int s = 0; s < 6; ++s) { double en = 0; for (int i = (int) (s * fs); i < (int) ((s + 1) * fs); ++i) en += L[(size_t) i] * L[(size_t) i]; std::printf ("  %ds %.1f dB", s, 10 * std::log10 (en + 1e-30)); }
            std::printf ("\n");
        }
        return 0;
    }

    if (what == "hrtfbal")
    {
        Hrtf h; h.prepare (fs);
        const int n = h.numTaps();
        std::vector<float> l ((size_t) n), r ((size_t) n);
        float itd = 0;
        auto energy = [&] (float az, float el)
        {
            h.lookup (az, el, l.data(), r.data(), itd);
            double e = 0;
            for (int i = 0; i < n; ++i) e += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            return 0.5 * e;                      // per ear
        };
        // the sphere, area weighted
        double sum = 0, w = 0;
        for (int ei = -8; ei <= 8; ++ei)
        {
            const float el = ei * 10.0f;
            const float cw = std::cos (el * 3.14159265f / 180.0f);
            for (int ai = 0; ai < 36; ++ai) { sum += cw * energy (ai * 10.0f, el); w += cw; }
        }
        const double diffuse = sum / w;
        // the eight directions the engine's own diffuse render uses
        double d8 = 0;
        for (int j = 0; j < 8; ++j) d8 += energy (22.5f + 45.0f * j, (j & 1) ? 20.0f : -10.0f);
        d8 /= 8.0;
        std::printf ("per-ear HRTF energy, %.0f Hz, %d taps\n", fs, n);
        std::printf ("  sphere average (area weighted, 612 directions): %.4f\n", diffuse);
        std::printf ("  the engine's eight diffuse directions:          %.4f  (%+.2f dB vs the sphere)\n", d8, 10 * std::log10 (d8 / diffuse));
        for (float az : { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f })
            std::printf ("  az %5.0f: %.4f  (%+.2f dB vs the sphere)\n", az, energy (az, 0.0f), 10 * std::log10 (energy (az, 0.0f) / diffuse));
        std::printf ("\n  So a source straight ahead is %+.2f dB louder than the same power arriving\n"
                     "  diffusely. That is the direct-to-reverberant ratio a real head measures at\n"
                     "  the critical distance; the textbook 0 dB assumes an omnidirectional ear.\n",
                     10 * std::log10 (energy (0.0f, 0.0f) / diffuse));
        return 0;
    }

    if (what == "dist")
    {
        for (float d : { 0.2f, 0.3f, 0.5f, 0.7f, 1.0f, 2.0f })
        {
            Engine e; e.prepare (fs, 256);
            Params p; p.material[0] = p.material[1] = p.material[2] = 0; p.src[0].type = 0;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.lisX = 3.0f; p.lisY = 2.5f; p.lisYaw = 0; p.src[0].x = 3.0f + d; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
            p.earlyDb = -120; p.reverbDb = -120;
            std::vector<float> L, R; run (e, p, L, R, (int) (0.5 * fs), 10);
            double s = 0; for (size_t i = 0; i < L.size(); ++i) s += (double) L[i] * L[i] + (double) R[i] * R[i];
            const PathSlot& sl = e.slot (0);
            std::printf ("d %.2f: energy per ear %.4f, expected 1/d^2 * 0.58 = %.4f (%.1f dB); slot gain %.3f gL %.3f gR %.3f delay %.1f", d, 0.5 * s, 0.58 / (d * d), 10 * std::log10 (0.5 * s / (0.58 / (d * d))), sl.gain, sl.gL, sl.gR, sl.delay);
            std::printf ("  active %d src %.3f,%.3f lis %.3f,%.3f", e.numActivePaths(), e.scene().sources[0].pos.x, e.scene().sources[0].pos.y, e.scene().lis.x, e.scene().lis.y);
            std::printf ("|");
        }
        return 0;
    }

    if (what == "drr")
    {
        // direct, images and late field at the critical distance, each alone
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            const float A = ROOMS[0].surface() * MATERIAL_ALPHA[m][3];
            const double rc = std::sqrt (A / (16.0 * 3.14159265));
            double en[3];
            for (int part = 0; part < 3; ++part)
            {
                Engine e; e.prepare (fs, 256);
                Params p; p.material[0] = p.material[1] = p.material[2] = m; p.src[0].type = 0;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.lisX = 3.0f; p.lisY = 2.5f; p.lisYaw = 0; p.src[0].x = 3.0f + (float) rc; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
                p.directDb = part == 0 ? 0.0f : -120.0f; p.earlyDb = part == 1 ? 0.0f : -120.0f; p.reverbDb = part == 2 ? 0.0f : -120.0f;
                std::vector<float> L, R; run (e, p, L, R, (int) (6 * fs), 2400);   /* past the fresh-path gain ramp */
                /*  Band-limited to the 1 kHz octave, because that is where the theory
                    is exact: 16 pi / A is a per-frequency statement, and a broadband
                    measure mixes octaves whose absorption differs by several dB. */
                auto band1k = [&] (const std::vector<float>& x) {
                    std::vector<double> y (x.size());
                    for (size_t i = 0; i < x.size(); ++i) y[i] = x[i];
                    for (int pass = 0; pass < 2; ++pass) {
                        const double w = 2 * 3.14159265358979 * 1000.0 / fs, Q = 1.4142;
                        const double al = std::sin (w) / (2 * Q), cw = std::cos (w);
                        const double b0 = al, b2 = -al, a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
                        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
                        for (auto& v : y) { const double in = v; const double out = (b0 * in + b2 * x2 - a1 * y1 - a2 * y2) / a0; x2 = x1; x1 = in; y2 = y1; y1 = out; v = out; }
                    }
                    double e = 0; for (double v : y) e += v * v; return e;
                };
                en[part] = 0.5 * (band1k (L) + band1k (R));
                if (part == 2) std::printf ("   [efficiency %.3f, inGain %.3f | early %.3f  all %.3f  fraction %.3f]\n",
                                            e.field (0).efficiency, e.field (0).inGain, e.dbgEarly, e.dbgAll, e.dbgFraction);
            }
            std::printf ("%s at rc = %.2f m: direct %.4f, images %.4f, late %.4f  -> DRR %.1f dB (0 wanted), late/(dir) %.1f dB, images/dir %.1f dB\n",
                         MATERIAL_NAMES[m], rc, en[0], en[1], en[2], 10 * std::log10 (en[0] / (en[1] + en[2])), 10 * std::log10 (en[2] / en[0]), 10 * std::log10 (en[1] / en[0]));
        }
        return 0;
    }

    if (what == "rtflat")
    {
        // the network's decay with FLAT losses (the 1 kHz value everywhere) vs Eyring at 1 kHz
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            Engine e; e.dbgFlatLoss = true; e.prepare (fs, 256);
            Params p; p.material[0] = p.material[1] = p.material[2] = m;
            p.door[0] = p.door[1] = p.door[2] = 0; p.src[0].type = 0;
            p.src[0].x = 2; p.src[0].y = 2; p.lisX = 4; p.lisY = 3; p.lisYaw = 180;
            p.directDb = -120; p.earlyDb = -120;
            const RoomField& F = e.field (0);
            std::vector<float> L, R; run (e, p, L, R, (int) ((F.rt60[3] * 2.5 + 1) * fs), 10);
            std::vector<double> edc (L.size(), 0.0); double acc = 0;
            for (int i = (int) L.size() - 1; i >= 0; --i) { acc += (double) L[(size_t) i] * L[(size_t) i]; edc[(size_t) i] = acc; }
            const int from = (int) (0.05 * fs); const double top = 10 * std::log10 (edc[(size_t) from]);
            int i5 = -1, i25 = -1;
            for (int i = from; i < (int) L.size(); ++i) { const double d = 10 * std::log10 (edc[(size_t) i]) - top; if (i5 < 0 && d <= -5) i5 = i; if (d <= -25) { i25 = i; break; } }
            std::printf ("%s flat-loss network: T20x3 = %.3f s, design %.3f s (%.0f %%)\n", MATERIAL_NAMES[m], 3.0 * (i25 - i5) / fs, F.rt60[3], 100.0 * (3.0 * (i25 - i5) / fs / F.rt60[3] - 1));
        }
        return 0;
    }

    if (what == "filt")
    {
        // the loss filter as fitted vs as asked, measured with sines
        const float targets[3][7] = { { -3, -2, -1.5f, -1, -0.8f, -0.9f, -1.2f }, { -0.3f, -0.25f, -0.2f, -0.15f, -0.16f, -0.25f, -0.45f }, { -12, -10, -8, -6, -5, -4, -3 } };
        for (int t = 0; t < 3; ++t)
        {
            BandFilter f; f.setCoeffs ((float) fs); f.setBandsDb (targets[t]);
            std::printf ("target 250/1k/4k/8k = %.2f %.2f %.2f %.2f  ->", targets[t][1], targets[t][3], targets[t][5], targets[t][6]);
            for (float hz : { 250.0f, 1000.0f, 4000.0f, 8000.0f, 16000.0f })
            {
                f.reset(); double acc = 0; const int n = (int) fs;
                for (int i = 0; i < n; ++i) { const float y = f.process (std::sin (2.0f * 3.14159265f * hz * (float) i / (float) fs)); if (i > n / 2) acc += y * y; }
                std::printf ("  %.0f: %.2f", hz, 10 * std::log10 (acc / (n / 2) * 2));
            }
            std::printf ("\n");
        }
        return 0;
    }

    if (what == "cal")
    {
        // the late field alone: impulse energy against 16 pi tail / A
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            Engine e; e.prepare (fs, 256);
            Params p; p.material[0] = p.material[1] = p.material[2] = m;
            p.door[0] = p.door[1] = p.door[2] = 0; p.src[0].type = 0;
            p.src[0].x = 2; p.src[0].y = 2; p.lisX = 4; p.lisY = 3; p.lisYaw = 180;
            p.directDb = -120; p.earlyDb = -120;
            std::vector<float> L, R; run (e, p, L, R, (int) (8 * fs), (int) fs);
            double en = 0; for (size_t i = 0; i < L.size(); ++i) en += (double) L[i] * L[i] + (double) R[i] * R[i];
            en *= 0.5;   // per ear
            const RoomField& F = e.field (0);
            const float A = F.absorptionArea[3];
            const double tail = std::exp (-13.8 * F.tEarly / F.rt60[3]);
            const double want = 16 * 3.14159265 * tail / A;
            std::printf ("LARGE %s: late-field impulse energy %.4f, wanted %.4f (%.1f dB), rt %.2f tEarly %.1f ms A %.1f inGain %.3f\n", MATERIAL_NAMES[m], en, want, 10 * std::log10 (en / want), F.rt60[3], F.tEarly * 1000, A, F.inGain);
            // EDC shape at 1 kHz in 20 ms steps
            std::vector<double> edc (L.size(), 0.0); double acc = 0;
            for (int i = (int) L.size() - 1; i >= 0; --i) { acc += (double) L[(size_t) i] * L[(size_t) i]; edc[(size_t) i] = acc; }
            std::printf ("   FDN alone: inj energy %.4f, y energy %.4f -> G %.3f (formula %.3f)\n", e.dbgInjEnergy[0], e.dbgFieldEnergy[0], e.dbgFieldEnergy[0] / e.dbgInjEnergy[0], 0.0);
            std::printf ("   EDC:");
            const size_t t0 = (size_t) fs; for (int t = 0; t <= 400; t += 20) std::printf (" %d:%.0f", t, 10 * std::log10 (edc[t0 + (size_t) (t * fs / 1000)] / edc[t0] + 1e-30));
            std::printf ("\n");
        }
        return 0;
    }

    if (what == "motion")
    {
        Engine e; e.prepare (fs, 256);
        Params p; p.src[0].type = 0;
        const std::string only = argc > 2 ? argv[2] : "";
        if (only == "direct") { p.earlyDb = -120; p.reverbDb = -120; }
        if (only == "early")  { p.directDb = -120; p.reverbDb = -120; }
        if (only == "reverb") { p.directDb = -120; p.earlyDb = -120; }
        const int n = (int) (3.0 * fs);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        e.setParams (p);
        double worst = 0; int worstAt = 0;
        std::vector<int> pathsAt ((size_t) n / 128 + 1, 0);
        for (int i = 0; i < n; i += 128)
        {
            const double tt = i / fs;
            if (tt > 0.5 && tt < 2.5) { p.src[0].x = 1.0f + (float) (tt - 0.5) * 2.0f; p.src[0].y = 1.5f; e.setParams (p); }
            for (int k = 0; k < 128; ++k) { const float v = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) (i + k) / (float) fs); L[(size_t) (i + k)] = v; R[(size_t) (i + k)] = v; }
            e.process (&L[(size_t) i], &R[(size_t) i], 128);
            pathsAt[(size_t) (i / 128)] = e.numActivePaths();
            for (int k = 1; k < 128; ++k) { const double st = std::abs (L[(size_t) (i + k)] - L[(size_t) (i + k - 1)]); if (st > worst && i + k > 0.3 * fs) { worst = st; worstAt = i + k; } }
        }
        std::printf ("worst step %.4f at sample %d (t = %.3f s), block %d; paths around: ", worst, worstAt, worstAt / fs, worstAt / 128);
        for (int b = std::max (0, worstAt / 128 - 3); b <= worstAt / 128 + 1; ++b) std::printf ("%d ", pathsAt[(size_t) b]);
        std::printf ("\n");
        for (int i = worstAt - 6; i < worstAt + 6; ++i) std::printf ("%d: %+.4f\n", i, L[(size_t) i]);
        return 0;
    }
    return 0;
}
