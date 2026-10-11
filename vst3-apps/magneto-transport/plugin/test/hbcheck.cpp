// The half-band pair and its 4x cascade alone: magnitude response (prints).
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>
using namespace mt;
int main()
{
    for (double f : { 1000.0, 5000.0, 9000.0, 15000.0, 18000.0, 20000.0 })
    {
        const double fs = 48000.0; const int n = 48000;
        HalfBand u, d, u2, d2; u.design(); d.design(); u2.design(); d2.design(); u.clear(); d.clear(); u2.clear(); d2.clear();
        HalfBand U, D; U.design(); D.design(); U.clear(); D.clear();
        double s2 = 0, s4 = 0, sx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) std::sin (2.0 * M_PI * f * i / fs);
            float a0, a1; U.up (x, a0, a1); const float y2 = D.down (a0, a1);
            float b0, b1, q[4]; u.up (x, b0, b1); u2.up (b0, q[0], q[1]); u2.up (b1, q[2], q[3]);
            const float m0 = d2.down (q[0], q[1]);
            const float m1 = d2.down (q[2], q[3]);
            const float y4 = d.down (m0, m1);
            if (i > 2000) { s2 += y2 * y2; s4 += y4 * y4; sx += x * x; }
        }
        std::printf ("%6.0f Hz: 2x pair %+.3f dB, 4x cascade %+.3f dB\n", f, 10 * std::log10 (s2 / sx), 10 * std::log10 (s4 / sx));
    }
}
