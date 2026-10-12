//  tuning across the catalogue, with the servo's own state beside each note
//  usage: ab104tuneprobe [nSpecimens=24] [character=0.8]
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace ab104;
static const int SR = 48000, BLK = 256;

static double gz (const std::vector<float>& x, size_t a, size_t b, double f)
{
    const double w = 2 * PI * f / SR, c = 2 * std::cos (w); double s1 = 0, s2 = 0; const size_t n = b - a;
    for (size_t i = 0; i < n; ++i)
    { const double h = 0.5 - 0.5 * std::cos (2 * PI * (double) i / (double) (n - 1)); const double s0 = x[a + i] * h + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2));
}

int main (int argc, char** argv)
{
    const int nS = argc > 1 ? std::atoi (argv[1]) : 24;
    const float ch = argc > 2 ? (float) std::atof (argv[2]) : 0.8f;
    const int notes[] = { 26, 31, 36, 41, 45 };
    double worst = 0; int bad = 0, all = 0;
    for (int s = 0; s < nS; ++s)
    {
        const int spec = (s * 37 + 5) % specimenCount();
        std::printf ("%03d %-8s", spec, materialName (materialFamilyOf (spec)));
        for (int n : notes)
        {
            Engine e; e.p = Params(); e.p.stiffness = 0; e.p.turbulence = 0; e.p.discipline = 1; e.p.character = ch;
            e.prepare (SR, BLK); e.requestSpecimen (spec);
            std::vector<float> l (BLK), r (BLK), m;
            e.process (l.data(), r.data(), BLK);
            e.noteOn (n, 0.85f);
            for (int b = 0; b < (int) (2.6 * SR / BLK); ++b)
            { e.process (l.data(), r.data(), BLK); m.insert (m.end(), l.begin(), l.end()); }
            const double ft = 440.0 * std::pow (2.0, (n - 69) / 12.0);
            const size_t a = (size_t) (1.4 * SR), b = (size_t) (2.5 * SR);
            double best = -1, bc = 0;
            for (double c = -80; c <= 80; c += 0.5)
            { const double v = gz (m, a, b, ft * std::pow (2.0, c / 1200.0)); if (v > best) { best = v; bc = c; } }
            const int d = e.commandDuct (0);
            std::printf (" %+6.1f(%c%.2f)", bc, e.ductClosed (d) ? 'c' : 'o', e.ductChaos (d));
            worst = std::max (worst, std::abs (bc)); ++all; if (std::abs (bc) > 10) ++bad;
        }
        std::printf ("\n");
    }
    std::printf ("worst %.1f cents, %d of %d notes beyond 10\n", worst, bad, all);
    return 0;
}
