//  why does a heat-flooded conduit not sing? print its internals.
#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace ab104;
static const int SR = 48000, BLK = 256;
static float ambFor (double k){ return (float)(std::log(k/T_AMB_LO)/std::log(T_AMB_HI/T_AMB_LO)); }

int main()
{
    Params p; p.ambient = ambFor (700.0);   // hot site
    Engine e; e.p = p; e.prepare (SR, BLK);
    std::vector<float> l (BLK), r (BLK);
    e.process (l.data(), r.data(), BLK);
    int best = 0; float bs = -1;
    for (int i = 0; i < e.ductCount(); ++i) if (e.ductSigma(i) > bs){ bs=e.ductSigma(i); best=i; }
    std::printf ("best duct %d  sigma %.3f  passive %.1f Hz\n", best, e.ductSigma(best), e.ductPassive(best));

    e.pour (best, 6.0f * 700.0f);   // flood it
    std::vector<float> all;
    for (int b = 0; b < (int)(2.5*SR/BLK); ++b)
    {
        e.process (l.data(), r.data(), BLK);
        all.insert (all.end(), l.begin(), l.end());
        if (b % (int)(0.25*SR/BLK) == 0)
        {
            float mean, ac; int zc;
            e.ductLoopStats (best, 4800, mean, ac, zc);       // the last 100 ms of the loop
            std::printf ("  t=%.2fs  T=%.0f  f=%.1f  ring=%.4f  drive=%.4f  radCur=%.3f  loop: mean=%+.4f ac=%.4f zc=%d (~%.0f Hz)  |max|=%.5f\n",
                         (double)b*BLK/SR, e.ductT(best), e.ductF(best), e.ductRing(best),
                         e.ductHeatDrive(best), e.ductRadCur(best), mean, ac, zc, zc * 5.0,
                         [&]{ float m=0; for(float v:l) m=std::max(m,std::abs(v)); return m; }());
        }
    }
    double sum=0; size_t n=0;
    for (size_t i=(size_t)(1.0*SR); i<all.size(); ++i){ sum+=all[i]*all[i]; ++n; }
    std::printf ("rms 1-2.5s = %.6f\n", std::sqrt(sum/std::max((size_t)1,n)));
    return 0;
}
