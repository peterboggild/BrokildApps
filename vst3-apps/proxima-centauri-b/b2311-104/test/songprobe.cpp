//  the bench's "heat-flooded conduit at 700 K sings on", step by step,
//  at a given CHARACTER.   usage: ab104songprobe [character=0.8] [specimen=0]
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace ab104;
static const int SR = 48000, BLK = 256;
static float ambFor (double k) { return (float) (std::log (k / T_AMB_LO) / std::log (T_AMB_HI / T_AMB_LO)); }

int main (int argc, char** argv)
{
    Engine e; e.p = Params(); e.p.bleed = 0; e.p.traffic = 0;
    e.p.character = argc > 1 ? (float) std::atof (argv[1]) : 0.8f;
    e.p.ambient = ambFor (700.0);
    e.prepare (SR, BLK);
    if (argc > 2) e.requestSpecimen (std::atoi (argv[2]));
    std::vector<float> l (BLK), r (BLK);
    e.process (l.data(), r.data(), BLK);
    int best = 0; float bs = -1;
    for (int i = 0; i < e.ductCount(); ++i) if (e.ductSigma (i) > bs) { bs = e.ductSigma (i); best = i; }
    e.pour (best, 6.0f * 700.0f);
    double acc = 0; int nAcc = 0;
    for (int b = 0; b < (int) (2.0 * SR / BLK); ++b)
    {
        e.process (l.data(), r.data(), BLK);
        if (b > (int) (1.0 * SR / BLK)) for (float v : l) { acc += (double) v * v; ++nAcc; }
        if (b % (int) (0.2 * SR / BLK) == 0)
        {
            float mean, ac; int zc;
            e.ductLoopStats (best, 4000, mean, ac, zc);
            std::printf ("  t=%.2f T=%.0f f=%.1f closed %d ring=%.4f drive=%.4f loop mean %+.3f ac %.4f zc %d radTgt %.3f\n",
                         (double) b * BLK / SR, e.ductT (best), e.ductF (best), e.ductClosed (best), e.ductRing (best),
                         e.ductHeatDrive (best), mean, ac, zc, e.ductRadTgt (best));
        }
    }
    std::printf ("rms 1-2 s %.5f\n", std::sqrt (acc / std::max (1, nAcc)));
    return 0;
}
