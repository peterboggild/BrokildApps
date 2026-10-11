// Magneto Transport - how loud an overdub is, as played (prints).
// LOOP at the panel's defaults: phrase A (a 440 Hz tone) for one loop period in REC,
// then DUB and phrase B (660 Hz, same level) for one loop period, then silence. Each
// pass of the loop past head 1 is measured with a Goertzel at each tone's frequency,
// so A and B are told apart in the same output. argv: [feedback] [mix] [erase]
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
    return std::sqrt (re * re + im * im) / (0.25 * n);     // Hann: amplitude of a sine
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000.0;
    Params p;                                       // the panel's defaults
    p.mode = 1; p.looper = 0;
    if (argc > 1) p.feedback = (float) std::atof (argv[1]);
    if (argc > 2) p.mix = (float) std::atof (argv[2]);
    if (argc > 3) p.erase = (float) std::atof (argv[3]);
    p.h2On = p.h3On = 0;                            // one head, so a pass is one echo
    Engine e; e.setParams (p); e.prepare (fs, 256);
    const double P = p.loopLen * 100.0 / p.speed;   // loop period, s
    const double REF = 0.12589254117941673 * std::sqrt (2.0);   // 0 VU peak
    const int passes = 6;
    const double tA = 0.0, tB = P, tEnd = 2 * P, tTot = 2 * P + passes * P + 0.5;
    const size_t n = (size_t) (tTot * fs);
    std::vector<float> x (n, 0.0f), L (n), R (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = i / fs;
        if (t >= tA + 0.05 && t < tB - 0.05) x[i] = (float) (REF * std::sin (2.0 * M_PI * 440.0 * t));
        if (t >= tB + 0.05 && t < tEnd - 0.05) x[i] = (float) (REF * std::sin (2.0 * M_PI * 660.0 * t));
    }
    for (size_t i = 0; i < n; i += 256)
    {
        Params q = p;
        if (i / fs >= tB) q.looper = 1;             // DUB from the second loop period
        e.setParams (q);
        e.process (x.data() + i, x.data() + i, L.data() + i, R.data() + i, (int) std::min<size_t> (256, n - i));
        e.service();
    }
    const double d = p.h1Pos / p.speed;             // head 1's delay, s
    std::printf ("loop %.2f m at %.2f cm/s = %.2f s period; head 1 at %.1f cm = %.0f ms; feedback %.2f mix %.2f erase %.0f, bias %.1f dB\n",
                 p.loopLen, p.speed, P, p.h1Pos, 1000 * d, p.feedback, p.mix, p.erase, p.bias);
    std::printf ("input: A 440 Hz and B 660 Hz at 0 VU (%.1f dBFS peak)\n\n", 20 * std::log10 (REF));
    std::printf ("window (one loop period, shifted by head 1's delay)   A 440 Hz     B 660 Hz\n");
    auto at = [&] (double t0, const char* what) {
        const size_t a = (size_t) ((t0 + d + 0.1) * fs), m = (size_t) ((P - 0.25) * fs);
        if (a + m > n) return;
        const double la = 20 * std::log10 (goertzel (L.data() + a, m, 440.0, fs) / REF + 1e-12);
        const double lb = 20 * std::log10 (goertzel (L.data() + a, m, 660.0, fs) / REF + 1e-12);
        std::printf ("  %-52s %+7.1f dB   %+7.1f dB\n", what, la, lb);
    };
    at (tA, "A played in REC (dry + first echo)");
    at (tB, "B played in DUB (dry + echo), A coming round");
    for (int k = 0; k < passes; ++k)
    {
        char s[80]; std::snprintf (s, sizeof s, "silence, pass %d after B", k + 1);
        at (tEnd + k * P, s);
    }
}
