// Magneto Transport - does a bass kick duck at low tape speed, and why? (prints)
// A synthetic kick (a 55 -> 45 Hz sine, 180 ms decay, with a 3 ms click) through head 1,
// wet only. For each speed: the kick's energy out against in, soft (-30 dBFS peak) and
// loud (-6 dBFS peak), so a level-dependent loss shows as loud - soft; then the same with
// the equalisers off, and with the record and replay losses off, to name the stage.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace mt;

static const double FS = 48000.0;

static std::vector<float> kick (double peak, double t0, double len)
{
    std::vector<float> x ((size_t) (len * FS), 0.0f);
    double ph = 0.0;
    for (size_t i = (size_t) (t0 * FS); i < x.size(); ++i)
    {
        const double t = (double) i / FS - t0;
        const double f = 45.0 + 10.0 * std::exp (-t / 0.04);
        ph += 2.0 * M_PI * f / FS;
        const double env = std::exp (-t / 0.18);
        const double click = std::exp (-t / 0.003) * std::sin (2.0 * M_PI * 1800.0 * t);
        x[i] = (float) (peak * env * (0.85 * std::sin (ph) + 0.15 * click));
    }
    return x;
}

static double energyDb (const std::vector<float>& y, double a, double b)
{
    double e = 0.0;
    for (size_t i = (size_t) (a * FS); i < std::min (y.size(), (size_t) (b * FS)); ++i) e += (double) y[i] * y[i];
    return 10.0 * std::log10 (e + 1e-30);
}

static double runDb (double speed, double peak, bool eq, bool losses, int eqStd, double& peakOut)
{
    Params p; p.mix = 1.0f; p.killDry = 1.0f; p.feedback = 0.0f; p.condition = 0.0f; p.mode = 0; p.speed = (float) speed;
    p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 22.8f; p.h3Pos = 24.0f; p.eqStd = (float) eqStd;
    const double dl = 4.0 / speed, t0 = 0.2, len = t0 + dl + 1.2;
    auto x = kick (peak, t0, len);
    Engine e; e.setIdealMedium (true); e.setEq (eq); e.setPlaybackLoss (losses);
    e.setParams (p); e.prepare (FS, 256);
    std::vector<float> y (x.size()), yr (x.size());
    for (size_t i = 0; i < x.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, x.size() - i);
        e.process (x.data() + i, x.data() + i, y.data() + i, yr.data() + i, n);
        e.service();
    }
    peakOut = 0.0;
    for (size_t i = (size_t) ((t0 + dl - 0.05) * FS); i < y.size(); ++i) peakOut = std::max (peakOut, (double) std::abs (y[i]));
    return energyDb (y, t0 + dl - 0.05, t0 + dl + 1.0) - energyDb (x, t0 - 0.05, t0 + 1.0);
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    std::printf ("kick energy out - in (dB), head 1 at 4 cm, wet only; loud - soft = what the level costs\n");
    for (int std_ = 0; std_ < 2; ++std_)
    {
        std::printf ("%s equalisation\n", std_ ? "NAB" : "IEC");
        for (double v : { 4.75, 9.525, 19.05, 38.1, 76.0 })
        {
            double pS, pL, pE, pN;
            const double soft = runDb (v, 0.0316, true, true, std_, pS);
            const double loud = runDb (v, 0.501, true, true, std_, pL);
            double pH; const double hot = runDb (v, 1.0, true, true, std_, pH);
            std::printf ("  %5.2f cm/s: a 0 dBFS kick costs %+6.2f dB against a quiet one (out peak %.3f)\n", v, hot - soft, pH);
            const double loudNoEq = runDb (v, 0.501, false, true, std_, pE) - runDb (v, 0.0316, false, true, std_, pN);
            std::printf ("  %5.2f cm/s: soft %+6.2f  loud %+6.2f  -> level costs %+6.2f dB (out peak %.3f)   with the equalisers off it costs %+6.2f dB\n",
                         v, soft, loud, loud - soft, pL, loudNoEq);
        }
    }
}
