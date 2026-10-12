// Magneto Transport - DUB's in-place re-record, pass by pass (prints). BUGLIST 9.
// A 0.5 m loop at 19.05 cm/s, the tape ideal (no noise) so levels read cleanly. In REC:
// a click (a 3 kHz Hann burst, 4 ms) and three tone bursts (100 Hz, 1 kHz, 8 kHz), each in
// its own stretch of the loop. Then DUB with ERASE 0 and silence in. Per pass: each tone's
// level through head 1 (Goertzel over its stretch) and where the click arrives (envelope
// centroid against the pass's start). A unity round trip keeps every level; a placed
// re-record keeps the click still.  argv: [trim] [lagSamples] [passes] [ideal 1/0]
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
using namespace mt;

static double goertzel (const float* x, size_t n, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (n - 1));
        const double s0 = x[i] * win + c * s1 - s2; s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return std::sqrt (re * re + im * im) / (0.25 * n);
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double trim = argc > 1 ? std::atof (argv[1]) : SYNC_TRIM;
    const double lag  = argc > 2 ? std::atof (argv[2]) : SYNC_LAG;
    const int passes  = argc > 3 ? std::atoi (argv[3]) : 6;
    const bool ideal  = argc > 4 ? std::atoi (argv[4]) != 0 : true;
    const double fs = 48000.0, L = 0.5, v = argc > 6 ? std::atof (argv[6]) : 19.05, P = L * 100.0 / v;
    const int qual = argc > 7 ? std::atoi (argv[7]) : 1;
    Params p; p.mode = 1; p.looper = 0; p.loopLen = (float) L; p.speed = (float) v; p.erase = 0.0f;
    p.mix = 1.0f; p.feedback = 0.0f; p.condition = 0.0f; p.h2On = p.h3On = 0; p.h1Pos = 4.0f;
    if (argc > 8) p.stock = (float) std::atof (argv[8]);
    if (argc > 9) p.bias = (float) std::atof (argv[9]);
    if (argc > 10) p.feedback = (float) std::atof (argv[10]);
    Engine e; e.setIdealMedium (ideal); e.setSyncCal (trim, lag); e.setQuality (qual);
    e.setParams (p); e.prepare (fs, 256);
    const double REF = 0.12589254117941673 * std::sqrt (2.0);
    // the REC pass: starts after 1 s of settling; stretches within [0, P) of that pass
    const double t0 = 1.0;
    struct Burst { double f, a, b; } tones[3] = { { 100.0, 0.20, 0.80 }, { 1000.0, 1.00, 1.60 }, { 8000.0, 1.80, 2.40 } };
    if (argc > 5) tones[0].f = std::atof (argv[5]);   // the low tone: probe the head bump
    // argv[11] = 1: the low tone fills the whole loop and the others are silent, so FEEDBACK's
    // echoes (a head-distance along the loop) stay inside the window measured
    const bool cont = argc > 11 && std::atoi (argv[11]) != 0;
    if (cont) { tones[0].a = 0.02; tones[0].b = 2.60; tones[1].a = tones[1].b = 0.0; tones[2].a = tones[2].b = 0.0; }
    const double kP = P / 2.625;              // the stretches scale with the loop period
    for (auto& b : tones) { b.a *= kP; b.b *= kP; }
    const double clickAt = 0.08 * kP, clickLen = 0.004;
    const size_t n = (size_t) ((t0 + P * (passes + 2) + 0.5) * fs);
    std::vector<float> x (n, 0.0f), Lo (n), Ro (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / fs - t0;
        if (t < 0 || t >= P) continue;
        for (const auto& b : tones)
            if (t >= b.a && t < b.b)
            {
                const double r = std::min ({ 1.0, (t - b.a) / 0.01, (b.b - t) / 0.01 });
                x[i] += (float) (REF * r * std::sin (2.0 * M_PI * b.f * t));
            }
        if (t >= clickAt && t < clickAt + clickLen)
        {
            const double u = (t - clickAt) / clickLen;
            x[i] += (float) (2.0 * REF * (0.5 - 0.5 * std::cos (2.0 * M_PI * u)) * std::sin (2.0 * M_PI * 3000.0 * t));
        }
    }
    const double tDub = t0 + P;            // DUB from the end of the REC pass
    for (size_t i = 0; i < n; i += 256)
    {
        Params q = p;
        if (i / fs >= tDub) q.looper = 1;
        e.setParams (q);
        e.process (x.data() + i, x.data() + i, Lo.data() + i, Ro.data() + i, (int) std::min<size_t> (256, n - i));
        e.service();
    }
    const double d = p.h1Pos / v;          // head 1's delay
    std::printf ("computed: loop %.3f dB at 1 kHz, treble %+.3f dB re 1 kHz\n", e.tele.syncLoopDb.load(), e.tele.syncHfDb.load());
    std::printf ("computed: bass %+.3f dB re 1 kHz\n", e.tele.syncLfDb.load());
    std::printf ("trim %.4f lag %.2f samples, %s medium; loop %.3f s; head 1 %.0f ms\n", trim, lag, ideal ? "ideal" : "real", P, 1000 * d);
    std::printf ("pass   100 Hz     1 kHz      8 kHz     click at (us, vs pass 0)\n");
    double c0 = 0.0, lv0[3] {};
    for (int k = 0; k <= passes; ++k)
    {
        const double base = t0 + k * P + d;
        double lv[3];
        for (int j = 0; j < 3; ++j)
        {
            if (tones[j].b - tones[j].a < 0.2) { lv[j] = -200.0; continue; }
            const size_t a = (size_t) ((base + tones[j].a + 0.05) * fs), m = (size_t) ((tones[j].b - tones[j].a - 0.1) * fs);
            lv[j] = 20.0 * std::log10 (goertzel (Lo.data() + a, m, tones[j].f, fs) / REF + 1e-12);
        }
        // the click's envelope centroid (squared, around where it should be)
        const size_t a = (size_t) ((base + clickAt - 0.01) * fs), m = (size_t) (0.03 * fs);
        double sw = 0, st = 0;
        for (size_t i = 0; i < m; ++i) { const double y = Lo[a + i]; sw += y * y; st += y * y * (double) i; }
        // against where the window SHOULD start: a window start is a whole sample, the pass is not
        const double cUs = ((double) a + st / std::max (sw, 1e-30) - (base + clickAt - 0.01) * fs) / fs * 1.0e6;
        if (k == 0) { c0 = cUs; for (int j = 0; j < 3; ++j) lv0[j] = lv[j]; }
        std::printf ("%3d  %+7.2f dB %+7.2f dB %+7.2f dB   %+8.2f us   (per pass: %+6.3f %+6.3f %+6.3f dB)\n", k, lv[0], lv[1], lv[2], cUs - c0,
                     k ? (lv[0] - lv0[0]) / k : 0.0, k ? (lv[1] - lv0[1]) / k : 0.0, k ? (lv[2] - lv0[2]) / k : 0.0);
    }
}
