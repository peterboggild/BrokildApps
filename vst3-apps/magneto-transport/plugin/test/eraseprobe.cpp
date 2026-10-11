// Magneto Transport - where an overwrite's residual lives, slab by slab (prints).
// The record tables' retention at s = 0 per layer, and a 0 VU 1 kHz recording's
// remanence per layer before and after one bias-only pass on a loop.
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace mt;

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000.0, v = 19.05, L = 0.5, P = L * 100.0 / v, tSw = 3.0;
    Params p; p.mix = 1; p.feedback = 0; p.condition = 0; p.mode = 1; p.loopLen = (float) L; p.speed = (float) v;
    p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 22.8f; p.h3Pos = 24.0f;
    Engine e; e.setIdealMedium (true); e.setEq (false); e.setPlaybackLoss (false);
    e.setParams (p); e.prepare (fs, 256);
    // the tables' retention at s = 0, per layer, for small old remanence
    for (int l = 0; l < NLAYER; ++l)
    {
        const LayerRecord& r = e.layerRecord (l);
        const int c = 5;
        const double gp = r.g[c + 1][r.NSMAX > 0 ? 20 : 0], gm = r.g[c - 1][20];
        std::printf ("layer %d: bias amplitude %.3f Hc, G(+0.19, s=0) = %+.5f, G(-0.19, s=0) = %+.5f, retention %.4f (%.1f dB)\n",
                     l, r.amp, gp, gm, 0.5 * (gp - gm) / 0.19, 20.0 * std::log10 (std::abs (0.5 * (gp - gm) / 0.19) + 1e-30));
    }
    const double REF_AMP = 0.12589254117941673 * std::sqrt (2.0);
    const size_t n = (size_t) ((tSw + 1.0) * fs);   // the stretch at 0.5-2.5 cm has had exactly one bias-only pass
    std::vector<float> x (n, 0.0f), Lo (n), Ro (n);
    for (size_t i = 0; i < (size_t) (tSw * fs); ++i) x[i] = (float) (REF_AMP * std::sin (2.0 * M_PI * 1000.0 * i / fs));
    double before[NLAYER] {}, after[NLAYER] {};
    auto layerRms = [&] (int l) {
        // the remanence of layer l over 2 cm of loop just past the record head (path 0.5 .. 2.5 cm)
        double s = 0; int k = 0;
        for (double cm = 0.5; cm < 2.5; cm += 0.0002) { const double m = e.layerM (0, l, cm); s += m * m; ++k; }
        return std::sqrt (s / k);
    };
    for (size_t i = 0; i < n; i += 256)
    {
        const double t = i / fs;
        if (t >= tSw) { Params q = p; q.erase = 0.0f; e.setParams (q); }
        e.process (x.data() + i, x.data() + i, Lo.data() + i, Ro.data() + i, (int) std::min<size_t> (256, n - i));
        e.service();
        if (before[0] == 0.0 && t >= tSw - 0.1) for (int l = 0; l < NLAYER; ++l) before[l] = layerRms (l);
    }
    for (int l = 0; l < NLAYER; ++l) after[l] = layerRms (l);
    double sb = 0, sa = 0;
    for (int l = 0; l < NLAYER; ++l)
    {
        std::printf ("layer %d: recorded %.5f rms, after one bias-only pass %.6f (%.1f dB)\n", l, before[l], after[l], 20.0 * std::log10 (after[l] / before[l]));
        sb += before[l]; sa += after[l];
    }
    std::printf ("depth mean: %.1f dB\n", 20.0 * std::log10 (sa / sb));
}
