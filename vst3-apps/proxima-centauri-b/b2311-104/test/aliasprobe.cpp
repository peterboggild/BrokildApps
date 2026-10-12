//  is the energy between the harmonics ALIASING? Aliasing falls when the rate
//  doubles; a sub-octave, a mode or noise does not.
//  usage: ab104aliasprobe [specimen=5] [steepen=1]
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace ab104;

static double gz (const std::vector<float>& x, size_t a, size_t b, double f, double sr)
{
    const double w = 2 * PI * f / sr, c = 2 * std::cos (w); double s1 = 0, s2 = 0; const size_t n = b - a;
    for (size_t i = 0; i < n; ++i)
    { const double h = 0.5 - 0.5 * std::cos (2 * PI * (double) i / (double) (n - 1)); const double s0 = x[a + i] * h + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2));
}

int main (int argc, char** argv)
{
    const int spec = argc > 1 ? std::atoi (argv[1]) : 5;
    const float st = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    const float satv = argc > 3 ? (float) std::atof (argv[3]) : 0.35f;
    const float lvl = argc > 4 ? (float) std::atof (argv[4]) : 0.62f;
    const float chf = argc > 5 ? (float) std::atof (argv[5]) : 0.45f;
    for (double sr : { 48000.0, 96000.0 })
    {
        Engine e; e.p = Params(); e.p.stiffness = 0; e.p.turbulence = 0; e.p.discipline = 1; e.p.bleed = 0; e.p.traffic = 0; e.p.steepen = st; e.p.sat = satv; e.p.level = lvl; e.p.chuff = chf;
        e.prepare (sr, 256); e.requestSpecimen (spec);
        std::vector<float> l (256), r (256), m;
        e.process (l.data(), r.data(), 256);
        e.noteOn (33, 1.0f);
        for (int b = 0; b < (int) (4.0 * sr / 256); ++b) { e.process (l.data(), r.data(), 256); m.insert (m.end(), l.begin(), l.end()); }
        const size_t a = (size_t) (2.0 * sr), bb = (size_t) (3.9 * sr);
        const double f0 = 55.0, h1 = gz (m, a, bb, f0, sr);
        const Material& mt = e.material(); const double ch = e.p.character;
        const double mr[2] = { 2.18 + ch * (mt.modeR[0] - 2.18), 3.63 + ch * (mt.modeR[1] - 3.63) };
        double worst = 0; int wk = 0;
        for (int k = 3; k <= 40; ++k)
        {
            const double fq = f0 * (k + 0.5); bool on = false;
            for (double rr : mr) if (std::abs (fq / (f0 * rr) - 1.0) < 0.12) on = true;
            if (on) continue;
            const double v = gz (m, a, bb, fq, sr); if (v > worst) { worst = v; wk = k; }
        }
        std::printf ("[chuffs %d] ", e.chuffFired.load());
        std::printf ("%s %.0f Hz: floor %.1f dB at %.1f f0;  sub-octave %.1f dB;  h2 %.1f h5 %.1f h10 %.1f h20 %.1f dB\n",
                     materialName (mt.family), sr, 20 * std::log10 (worst / h1 + 1e-15), wk + 0.5,
                     20 * std::log10 (gz (m, a, bb, f0 * 0.5, sr) / h1 + 1e-15),
                     20 * std::log10 (gz (m, a, bb, f0 * 2, sr) / h1 + 1e-15), 20 * std::log10 (gz (m, a, bb, f0 * 5, sr) / h1 + 1e-15),
                     20 * std::log10 (gz (m, a, bb, f0 * 10, sr) / h1 + 1e-15), 20 * std::log10 (gz (m, a, bb, f0 * 20, sr) / h1 + 1e-15));
    }
    return 0;
}
