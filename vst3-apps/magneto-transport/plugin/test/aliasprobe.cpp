// Magneto Transport - aliasing per quality tier (prints, asserts nothing).
// A 9 kHz tone at +12 dB re 0 VU through head 1 at 38.1 cm/s, silent tape: the levels of
// its harmonics and of the largest spur off them, per tier, at 48 and 96 kHz.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
using namespace mt;

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        for (size_t i = 0; i < n; i += len)
            for (size_t k = 0; k < len / 2; ++k)
            {
                const std::complex<double> w (std::cos (ang * k), std::sin (ang * k));
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
            }
    }
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double f = argc > 1 ? std::atof (argv[1]) : 9000.0;
    for (double fs : { 48000.0, 96000.0 })
        for (int q = 0; q < 3; ++q)
        {
            Params p; p.mix = 1; p.killDry = 1; p.feedback = 0; p.condition = 0; p.speed = 38.1f; p.particles = 1000.0f;
            p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 22.8f; p.h3Pos = 24.0f;
            Engine e; e.setIdealMedium (true); e.setQuality (q); e.setParams (p); e.prepare (fs, 256);
            const size_t NF = 65536, start = (size_t) (0.6 * fs), n = start + NF;
            std::vector<float> x (n), yl (n), yr (n);
            const double amp = 4.0 * 0.12589254117941673 * std::sqrt (2.0);
            for (size_t i = 0; i < n; ++i) x[i] = (float) (amp * std::sin (2.0 * M_PI * f * i / fs));
            for (size_t i = 0; i < n; i += 256) e.process (x.data() + i, x.data() + i, yl.data() + i, yr.data() + i, (int) std::min<size_t> (256, n - i));
            std::vector<std::complex<double>> b (NF);
            double ws = 0.0;
            for (size_t i = 0; i < NF; ++i)
            {
                const double t = 2.0 * M_PI * i / NF;
                const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
                b[i] = w * yl[start + i]; ws += w;
            }
            fft (b);
            auto db = [&] (size_t k) { double m = 0; for (size_t j = k - 4; j <= k + 4; ++j) m = std::max (m, std::abs (b[j])); return 20.0 * std::log10 (2.0 * m / ws + 1e-30); };
            const double bin = fs / NF;
            std::printf ("fs %.0f %s (internal %.0f):", fs, q == 0 ? "ECO   " : q == 1 ? "NORMAL" : "HIGH  ", e.internalRate());
            for (int h = 1; h * f < 0.5 * fs; ++h) std::printf ("  H%d %.1f", h, db ((size_t) std::lround (h * f / bin)));
            double worst = -400, wHz = 0;
            for (size_t k = 8; k < NF / 2 - 8; ++k)
            {
                const double hz = k * bin;
                if (std::abs (hz - f * std::round (hz / f)) <= 10 * bin) continue;
                const double d = 20.0 * std::log10 (2.0 * std::abs (b[k]) / ws + 1e-30);
                if (d > worst) { worst = d; wHz = hz; }
            }
            std::printf ("   largest spur %.1f dBFS at %.0f Hz\n", worst, wHz);
        }
}
