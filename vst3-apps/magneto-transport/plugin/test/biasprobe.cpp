// Magneto Transport - how much of an old recording survives one overdub pass, by frequency
// (prints). A loop at 19.05 cm/s, erase head OFF, record on with silence going in: the
// old tone is read back through the playback head (EQ in) one loop later.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
using namespace mt;
static const double FS = 48000.0;
static double amp (const std::vector<float>& y, double a, double b, double f)
{
    std::complex<double> s (0, 0); double w = 0;
    const size_t i0 = (size_t) (a * FS), i1 = (size_t) (b * FS);
    for (size_t i = i0; i < i1; ++i) { const double h = 0.5 - 0.5 * std::cos (2.0 * M_PI * (i - i0) / (i1 - i0)); s += h * (double) y[i] * std::polar (1.0, -2.0 * M_PI * f * i / FS); w += h; }
    return 2.0 * std::abs (s) / w;
}
int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double v = 19.05, L = 0.5, P = L * 100.0 / v, h = 4.0, dl = h / v, tSw = 3.0;
    for (double biasDb : { 0.0, -3.0, -6.0 })
    {
        std::printf ("BIAS %+.0f dB:", biasDb);
        for (double f : { 60.0, 200.0, 1000.0, 3000.0, 8000.0 })
        {
            Params p; p.mix = 1; p.killDry = 1; p.feedback = 0; p.condition = 0; p.mode = 1; p.loopLen = (float) L; p.speed = (float) v;
            p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = (float) h; p.h2Pos = 22.8f; p.h3Pos = 24.0f; p.bias = (float) biasDb;
            const size_t n = (size_t) ((tSw + dl + P + 0.3) * FS);
            std::vector<float> x (n, 0.0f), y (n), yr (n);
            const double a0 = 0.12589254117941673 * std::sqrt (2.0);   // 0 VU
            for (size_t i = 0; i < (size_t) (tSw * FS); ++i) x[i] = (float) (a0 * std::sin (2.0 * M_PI * f * i / FS));
            Engine e; e.setIdealMedium (true);
            e.setParams (p); e.prepare (FS, 256);
            for (size_t i = 0; i < n; i += 256)
            {
                if ((double) i / FS >= tSw) { Params q = p; q.erase = 0.0f; e.setParams (q); }
                const int m = (int) std::min<size_t> (256, n - i);
                e.process (x.data() + i, x.data() + i, y.data() + i, yr.data() + i, m); e.service();
            }
            const double before = amp (y, dl + 0.5, tSw - 0.05, f), after = amp (y, tSw + dl + 0.1, tSw + dl + P - 0.1, f);
            std::printf ("  %5.0f Hz %+6.1f dB", f, 20.0 * std::log10 (after / before));
        }
        std::printf ("\n");
    }
}
