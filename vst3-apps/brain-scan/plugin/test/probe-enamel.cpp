//  a probe: render a factory patch and say where the samples go
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
using namespace bs;
int main (int argc, char** argv)
{
    const int which = argc > 1 ? std::atoi (argv[1]) : 10;
    Line six[NLINES]; Params p; factory (which).build (six, p);
    std::printf ("patch %s: specimen %.3f grain %.2f contrast %.2f fold %.2f unison %.2f head2 %.2f\n",
                 factory (which).name, p.specimen, p.grain, p.contrast, p.fold, p.unison, p.head2);
    Engine e; e.p = p; e.prepare (48000.0, 256); e.setLines (six); e.service();
    std::printf ("loaded specimen %d\n", e.specimenLoaded());
    std::vector<float> l (256), r (256);
    e.noteOn (48, 0.9f);
    int nan = 0, big = 0; double peak = 0;
    for (int b = 0; b < 200; ++b)
    {
        e.process (l.data(), r.data(), 256);
        double bs = 0, bm = 0;
        for (float v : l) { if (! std::isfinite (v)) ++nan; if (std::abs (v) > 0.99f) ++big; bs += (double) v * v; bm = std::max (bm, (double) std::abs (v)); peak = std::max (peak, (double) std::abs (v)); }
        if (b % 20 == 0 || (b < 6))
            std::printf ("  block %3d: rms %.4f max %.4f  active %d cut %.0f lod %.2f scan %.3f\n",
                         b, std::sqrt (bs / 256.0), bm, e.activeVoices(), e.voiceCutoff (0), e.voiceLod (0), e.voiceScan (0));
    }
    std::printf ("peak %.4f, nan %d, > 0.99: %d\n", peak, nan, big);
    return 0;
}
