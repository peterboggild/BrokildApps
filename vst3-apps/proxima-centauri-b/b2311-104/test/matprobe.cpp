//  inside one ordered conduit: how loud is the loop, and is the medium reached?
//  usage: ab104matprobe spec [note=40] [character=0.8]
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
using namespace ab104;
static const int SR = 48000, BLK = 256;

int main (int argc, char** argv)
{
    const int spec = argc > 1 ? std::atoi (argv[1]) : 99;
    const int note = argc > 2 ? std::atoi (argv[2]) : 40;
    Engine e; e.p = Params();
    if (argc > 3) e.p.character = (float) std::atof (argv[3]);
    //  argv[4]: "id=value,id=value" overrides
    if (argc > 4)
    {
        std::string all = argv[4]; size_t at = 0;
        while (at < all.size())
        {
            size_t c = all.find (',', at); if (c == std::string::npos) c = all.size();
            const std::string kv = all.substr (at, c - at); const size_t eq = kv.find ('=');
            if (eq != std::string::npos)
                for (int i = 0; i < numParams(); ++i)
                    if (kv.substr (0, eq) == paramSpec (i).id) paramSpec (i).get (e.p) = (float) std::atof (kv.c_str() + eq + 1);
            at = c + 1;
        }
    }
    e.prepare (SR, BLK); e.requestSpecimen (spec);
    std::vector<float> l (BLK), r (BLK);
    e.process (l.data(), r.data(), BLK);
    const Material& m = e.material();
    std::printf ("spec %d %s/%s blend %.2f drive %.2f fold %.2f shock %.3f closed %.2f rad %.2f skin %.2f F %.0f/%.0f %+.1f/%+.1f dB\n",
                 spec, materialName (m.family), materialName (m.family2), m.blend, m.drive, m.fold, m.shock,
                 m.closedFrac, m.radMul, m.skin, m.f1, m.f2, m.g1db, m.g2db);
    std::vector<double> outM;
    e.noteOn (note, 0.8f);
    for (int b = 0; b < (int) (2.0 * SR / BLK); ++b)
    {
        e.process (l.data(), r.data(), BLK);
        for (int k = 0; k < BLK; ++k) outM.push_back (0.5 * (l[(size_t) k] + r[(size_t) k]));
        if (b % (int) (0.25 * SR / BLK) == 0 && b > 0)
        {
            const int d = e.commandDuct (0);
            float mean, ac; int zc;
            e.ductLoopStats (d, 4800, mean, ac, zc);
            float pk = 0; for (float v : l) pk = std::max (pk, std::abs (v));
            std::printf ("  t=%.2f duct %d closed %d f=%.1f ring=%.4f drive=%.4f loopAC=%.4f (x medium %.2f) out|max| %.3f\n",
                         (double) b * BLK / SR, d, e.ductClosed (d), e.ductF (d), e.ductRing (d),
                         e.ductHeatDrive (d), ac, ac * 1.4142f * m.drive * e.p.character, pk);
        }
    }
    {
        const int d = e.commandDuct (0); const double f = e.ductF (d);
        const int N = 4096; std::vector<double> x (N);   // < lineLen (sr/11), or the wrap is measured
        for (int k = 0; k < N; ++k) x[(size_t) k] = e.ductLoopAt (d, N - 1 - k);
        auto gz = [&] (double fr) { double w = 2*PI*fr/SR, c = 2*std::cos (w), s1 = 0, s2 = 0;
            for (int k = 0; k < N; ++k) { double hn = 0.5-0.5*std::cos (2*PI*k/(N-1)); double s0 = x[(size_t)k]*hn + c*s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (std::max (0.0, s1*s1+s2*s2-c*s1*s2)); };
        const double h1 = gz (f);
        auto gzo = [&] (double fr) { double w = 2*PI*fr/SR, cc = 2*std::cos (w), s1 = 0, s2 = 0; const size_t o = outM.size() - N;
            for (int k = 0; k < N; ++k) { double hn = 0.5-0.5*std::cos (2*PI*k/(N-1)); double s0 = outM[o+(size_t)k]*hn + cc*s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (std::max (0.0, s1*s1+s2*s2-cc*s1*s2)); };
        const double o1 = gzo (f);
        std::printf ("  bright %.2f radA %.4f -> corner %.0f Hz (%.2f x f0)%c", e.ductBright (d), e.ductRadA (d),
                     -std::log (1.0 - e.ductRadA (d)) * SR / (2 * PI), -std::log (1.0 - e.ductRadA (d)) * SR / (2 * PI) / f, 10);
        std::printf ("  OUT  harmonics dB:");
        for (int k = 2; k <= 12; ++k) std::printf (" %.0f", 20*std::log10 (std::max (1e-12, gzo (f*k))/o1));
        std::printf ("%c", 10);
        std::printf ("  LOOP harmonics dB:");
        for (int k = 2; k <= 12; ++k) std::printf (" %.0f", 20*std::log10 (std::max (1e-12, gz (f*k))/h1));
        std::printf ("\n");
    }
    return 0;
}
