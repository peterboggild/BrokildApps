// Magneto Transport - dropouts, measured two ways (prints, asserts nothing):
// the medium's own event list along a stretch of tape, and a 10 kHz tone's
// envelope through the engine.
#include "Engine.h"
#include "Medium.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace mt;

int main()
{
    const StockMedium& S = stockMedium (2);
    // 1. the events along 3.9 m: count maxima of the lift and of the hole depth
    {
        DropoutCache dc;
        const uint64_t seed = 0x4D54000000ull + 2ull * 0x1000193ull;
        int lifts = 0, holes = 0, deep = 0;
        double prevDd = 0.0, prevK = 1.0; bool upD = false, upK = false;
        const double cells = 3.9e6 / 0.5;   // 3.9 m in surface cells
        for (double x = 0.0; x < cells; x += 20.0)
        {
            double dd, keep;
            dc.at (x, seed ^ 0x300u, S.dropPerM, S.dropDepthUm, S.holeFrac, dd, keep);
            if (dd < prevDd && upD) { ++lifts; if (prevDd > 1.044) ++deep; }
            if (keep > prevK && upK) { ++holes; if (prevK < 0.708) ++deep; }
            upD = dd > prevDd; upK = keep < prevK;
            prevDd = dd; prevK = keep;
        }
        std::printf ("events over 3.9 m: %d lifts, %d holes, %d deeper than -3 dB at 19 um (rate says %.1f events)\n",
                     lifts, holes, deep, S.dropPerM * 3.9);
    }
    // 2. through the engine
    {
        Params p; p.mix = 1; p.feedback = 0; p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 22.8f; p.h3Pos = 24.0f;
        p.condition = 0; p.stock = 2; p.speed = 19.05f; p.particles = 1000.0f;
        const double fs = 48000.0;
        Engine e; e.setParams (p); e.prepare (fs, 256);
        const size_t n = (size_t) (21.0 * fs);
        std::vector<float> x (n), L (n), R (n);
        for (size_t i = 0; i < n; ++i) x[i] = (float) (0.053 * std::sin (2.0 * M_PI * 10000.0 * i / fs));
        for (size_t i = 0; i < n; i += 256) { e.process (x.data() + i, x.data() + i, L.data() + i, R.data() + i, 256); e.service(); }
        const size_t hop = (size_t) (0.00025 * fs);
        std::vector<double> env;
        for (size_t a = (size_t) (0.6 * fs); a + 4 * hop <= n; a += hop)
        {
            double s = 0; for (size_t i = a; i < a + 4 * hop; ++i) s += (double) L[i] * L[i];
            env.push_back (std::sqrt (s / (4 * hop)));
        }
        std::vector<double> so = env; std::sort (so.begin(), so.end());
        std::printf ("envelope: min %.4f  p1 %.4f  median %.4f  max %.4f\n", so.front(), so[so.size() / 100], so[so.size() / 2], so.back());
        for (double th : { -1.0, -2.0, -3.0, -6.0 })
        {
            int dips = 0; bool in = false; const double lim = so[so.size() / 2] * std::pow (10.0, th / 20.0);
            for (double v : env) { const bool d = v < lim; if (d && ! in) ++dips; in = d; }
            std::printf ("  dips past %+.0f dB: %d\n", th, dips);
        }
    }
}
