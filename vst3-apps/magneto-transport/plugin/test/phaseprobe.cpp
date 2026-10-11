// Magneto Transport - the tape's phase at low frequencies against the dry signal (prints).
// A steady tone through head 1 (wet only); the output's phase against the input delayed by
// exactly distance / speed. 0 deg = the echo lands in phase with the dry; 180 = inverted.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
using namespace mt;
static const double FS = 48000.0;
static std::complex<double> tone (const std::vector<float>& y, size_t a, size_t n, double f, double shift)
{
    std::complex<double> s (0, 0);
    for (size_t i = 0; i < n; ++i) s += (double) y[a + i] * std::polar (1.0, -2.0 * M_PI * f * ((double) (a + i) - shift) / FS);
    return s;
}
int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    for (int std_ = 0; std_ < 2; ++std_)
        for (int eq = 1; eq >= 0; --eq)
        {
            std::printf ("%s, equalisers %s\n", std_ ? "NAB" : "IEC", eq ? "on" : "off");
            for (double v : { 4.75, 9.525, 19.05, 38.1, 76.0 })
            {
                std::printf ("  %5.2f cm/s:", v);
                for (double f : { 40.0, 55.0, 100.0, 1000.0 })
                {
                    Params p; p.mix = 1.0f; p.killDry = 1.0f; p.feedback = 0.0f; p.condition = 0.0f; p.mode = 0; p.speed = (float) v;
                    p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 22.8f; p.h3Pos = 24.0f; p.eqStd = (float) std_;
                    const double dl = 4.0 / v;
                    const size_t n = (size_t) (1.0 * FS), a = (size_t) ((dl + 0.6) * FS);
                    std::vector<float> x (a + n), y (a + n), yr (a + n);
                    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.03 * std::sin (2.0 * M_PI * f * i / FS));
                    Engine e; e.setIdealMedium (true); e.setEq (eq != 0);
                    e.setParams (p); e.prepare (FS, 256);
                    for (size_t i = 0; i < x.size(); i += 256) { const int m = (int) std::min<size_t> (256, x.size() - i); e.process (x.data() + i, x.data() + i, y.data() + i, yr.data() + i, m); e.service(); }
                    const auto ci = tone (x, a, n, f, 0.0), co = tone (y, a, n, f, dl * FS);
                    double ph = std::arg (co / ci) * 180.0 / M_PI;
                    std::printf ("  %4.0f Hz %+5.0f deg %+5.1f dB", f, ph, 20.0 * std::log10 (std::abs (co / ci)));
                }
                std::printf ("\n");
            }
        }
}
