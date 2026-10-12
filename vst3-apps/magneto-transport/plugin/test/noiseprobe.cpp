// Magneto Transport - does DUB + FEEDBACK build up? (prints). BUGLIST 11.
// LOOP, panel defaults, real medium: one loop period of white noise recorded in REC, then
// DUB with silence in for many passes. White noise holds every frequency, so if the loop
// gain is above unity anywhere (the in-phase teeth of the feedback comb, the head bump)
// that frequency takes over and the per-pass level stops falling and rises. Prints the
// output level of each pass past head 1, in narrow log bands, and the worst band's slope
// over the last passes.  argv: feedback [speed] [erase] [passes] [stock] [bias]
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <complex>
using namespace mt;

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000.0;
    Params p;
    p.mode = 1; p.looper = 0; p.h2On = p.h3On = 0;
    p.feedback = argc > 1 ? (float) std::atof (argv[1]) : 0.5f;
    if (argc > 2) p.speed = (float) std::atof (argv[2]);
    p.erase = argc > 3 ? (float) std::atof (argv[3]) : 0.0f;
    const int passes = argc > 4 ? std::atoi (argv[4]) : 10;
    if (argc > 5) p.stock = (float) std::atof (argv[5]);
    if (argc > 6) p.bias = (float) std::atof (argv[6]);
    p.mix = 1.0f;
    Engine e; if (argc > 7) e.setIdealMedium (std::atoi (argv[7]) != 0); e.setParams (p); e.prepare (fs, 256);
    const double P = p.loopLen * 100.0 / p.speed, d = p.h1Pos / p.speed;
    const double t0 = 0.5;
    const size_t n = (size_t) ((t0 + P * (passes + 2) + 0.5) * fs);
    std::vector<float> x (n, 0.0f), L (n), R (n);
    unsigned s = 12345u;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / fs - t0;
        if (t < 0.02 || t >= P - 0.02) continue;
        s = s * 1664525u + 1013904223u;
        x[i] = (float) (0.06 * (((s >> 8) & 0xFFFF) / 32768.0 - 1.0));
    }
    for (size_t i = 0; i < n; i += 256)
    {
        Params q = p; if (i / fs >= t0 + P) q.looper = 1;
        e.setParams (q);
        e.process (x.data() + i, x.data() + i, L.data() + i, R.data() + i, (int) std::min<size_t> (256, n - i));
        e.service();
    }
    // bands: 24 per octave from 40 Hz to 16 kHz, energy per pass from one FFT per pass
    const size_t N = 1u << 17;
    std::vector<double> edges; for (double f = 40.0; f < 16000.0; f *= std::pow (2.0, 1.0 / 24.0)) edges.push_back (f);
    const size_t nb = edges.size() - 1;
    std::vector<std::vector<double>> E (passes + 1, std::vector<double> (nb, 0.0));
    std::vector<double> tot (passes + 1, 0.0);
    for (int k = 0; k <= passes; ++k)
    {
        const size_t a = (size_t) ((t0 + (k + 1) * P + d + 0.05) * fs), m = std::min (N, (size_t) ((P - 0.1) * fs));
        if (a + m > n) break;
        std::vector<std::complex<double>> b (N, 0.0);
        for (size_t i = 0; i < m; ++i) { const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (m - 1)); b[i] = L[a + i] * w; }
        fft (b);
        for (size_t i = 1; i < N / 2; ++i)
        {
            const double f = i * fs / N, en = std::norm (b[i]);
            tot[k] += en;
            const size_t j = (size_t) (std::upper_bound (edges.begin(), edges.end(), f) - edges.begin());
            if (j >= 1 && j <= nb) E[k][j - 1] += en;
        }
    }
    std::printf ("re-record keep %.2f dB, loop gain at 1 kHz %.2f dB\n", e.tele.reRecDb.load(), e.tele.syncLoopDb.load());
    std::printf ("feedback %.2f speed %.2f erase %.0f stock %.0f bias %.1f; loop %.3f s\n", p.feedback, p.speed, p.erase, p.stock, p.bias, P);
    for (int k = 0; k <= passes; ++k)
    {
        const size_t a = (size_t) ((t0 + (k + 1) * P + d + 0.05) * fs), m = (size_t) ((P - 0.1) * fs);
        double ss = 0.0, pk = 0.0;
        if (a + m <= n) for (size_t i = 0; i < m; ++i) { ss += (double) L[a + i] * L[a + i]; pk = std::max (pk, (double) std::abs (L[a + i])); }
        std::printf ("pass %2d total %+7.2f dB   rms %+7.2f dBFS  peak %+7.2f dBFS\n", k, 10 * std::log10 (tot[k] / tot[0] + 1e-30),
                     10 * std::log10 (ss / std::max<size_t> (m, 1) + 1e-30), 20 * std::log10 (pk + 1e-30));
    }
    // per band: slope over the last half of the passes (dB per pass); the worst band
    const int k0 = passes / 2;
    double worst = -1e9, fW = 0.0;
    for (size_t j = 0; j < nb; ++j)
    {
        if (E[0][j] <= 0 || E[passes][j] <= 0 || E[k0][j] <= 0) continue;
        if (10 * std::log10 (E[k0][j] / E[0][j]) < -60.0) continue;     // gone into the floor
        const double sl = 10 * std::log10 (E[passes][j] / E[k0][j]) / (passes - k0);
        if (sl > worst) { worst = sl; fW = std::sqrt (edges[j] * edges[j + 1]); }
    }
    std::printf ("worst band slope over passes %d..%d: %+.3f dB per pass at %.0f Hz\n", k0, passes, worst, fW);
}
