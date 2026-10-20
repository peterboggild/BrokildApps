/*  MARMALADE 50 - set the two windings the sheets print as "???"

    The factory drawings give the rails' test-point voltages but not the
    transformer's: B (TP7) 390 V and -BIAS (TP5) -50 V, with the bias preset
    fully anti-CW. This finds the HT half-winding ratio (Tht1/Tht2) and the
    pink bias winding's ratio (Tbs) that land the warmed-up amp on those two,
    averaged over the ripple - the two together, by a proportional fixed-point
    iteration (B is close to proportional to the HT winding and the bias to
    its winding) - and prints everything else the sheets print,
    which the circuit then has to hit on its own: C, D, E, F.

    Then, as a technician would, it turns the bias preset (P1, Rp1) from
    there to idle each EL34 at a target current - averaged over the ripple,
    across the output transformer's half-primary resistance - and prints the
    rails the amp then runs at.

        mm50bias [mA per valve, default 35] [B volts, default 390] [bias volts, default -50]
*/
#include "Circuit.h"
#include "Tv50Netlist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace mm;

namespace
{
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237;

    struct Result { double ia, ib, bias, a, b, c, d, e, f; };

    double ht = 1.18, bs = 0.9, p1 = 47e3;

    Result idle()
    {
        Circuit c;
        c.parse (TV50_AMP);
        c.setValue (c.elementIndex ("tht1"), ht);
        c.setValue (c.elementIndex ("tht2"), ht);
        c.setValue (c.elementIndex ("tbs"), bs);
        c.setValue (c.elementIndex ("rp1"), p1);
        const double fs = 16000.0;
        c.prepare (fs);
        double in[4] = { 0.0, MAINS_PEAK, 0.0, 0.0 };
        c.solveDC (in);
        std::vector<double> o ((size_t) c.numOutputs());
        const int ota = c.nodeIndex ("ota"), pa = c.nodeIndex ("pa");
        const int otb = c.nodeIndex ("otb"), pb = c.nodeIndex ("pb");
        const double rwa = c.getValue (c.elementIndex ("rwa")), rwb = c.getValue (c.elementIndex ("rwb"));
        auto V = [&] (const char* n) { return c.nodeVoltage (c.nodeIndex (n)); };
        Result r {};
        // 14 s: the bias filter (220k into 10u) takes seconds to settle
        const int n = (int) (14.0 * fs), avg = (int) (0.2 * fs);
        for (int i = 0; i < n; ++i)
        {
            in[1] = MAINS_PEAK * std::sin (2.0 * M_PI * 50.0 * i / fs);
            c.process (in, o.data());
            if (i >= n - avg)
            {
                r.ia += (c.nodeVoltage (ota) - c.nodeVoltage (pa)) / rwa / avg;
                r.ib += (c.nodeVoltage (otb) - c.nodeVoltage (pb)) / rwb / avg;
                r.bias += V ("nb") / avg;
                r.a += V ("ap") / avg; r.b += V ("bp") / avg; r.c += V ("cp") / avg;
                r.d += V ("dp") / avg; r.e += V ("ep") / avg; r.f += V ("fp") / avg;
            }
        }
        if (c.failures() > 0) std::printf ("  (Newton failed %d times at ht %.5f bs %.5f)\n", c.failures(), ht, bs);
        return r;
    }
}

int main (int argc, char** argv)
{
    const double iTarget    = (argc > 1 ? std::atof (argv[1]) : 35.0) * 1e-3;
    const double bTarget    = argc > 2 ? std::atof (argv[2]) : 390.0;
    const double biasTarget = argc > 3 ? std::atof (argv[3]) : -50.0;

    for (int round = 0; round < 8; ++round)
    {
        const Result r = idle();
        std::printf ("round %d: Tht %.5f -> B %.2f V, Tbs %.5f -> bias %.2f V\n", round, ht, r.b, bs, r.bias);
        if (std::abs (r.b - bTarget) < 0.05 && std::abs (r.bias - biasTarget) < 0.02) break;
        ht *= bTarget / r.b;
        bs *= biasTarget / r.bias;
    }

    const Result s = idle();
    std::printf ("at the sheet's setting (P1 fully anti-CW):\n");
    std::printf ("rail     sim      sheet\n");
    std::printf ("A     %6.1f      ???\n", s.a);
    std::printf ("B     %6.1f    390 (TP7)\n", s.b);
    std::printf ("C     %6.1f    317 (TP8)\n", s.c);
    std::printf ("D     %6.1f    270 (TP9)\n", s.d);
    std::printf ("E     %6.1f    245 (TP10)\n", s.e);
    std::printf ("F     %6.1f    218 (TP11)\n", s.f);
    std::printf ("bias  %6.1f    -50 (TP5)   EL34 %.2f / %.2f mA\n\n", s.bias, s.ia * 1e3, s.ib * 1e3);

    // the preset: less of it, less negative, more current
    double lo = 0.0, hi = 47e3;
    for (int k = 0; k < 14; ++k)
    {
        p1 = 0.5 * (lo + hi);
        const Result t = idle();
        (0.5 * (t.ia + t.ib) > iTarget ? lo : hi) = p1;
    }
    p1 = 0.5 * (lo + hi);

    const Result r = idle();
    std::printf ("biased: P1 %.0f ohms of 47k\n", p1);
    std::printf ("Tht1/Tht2 %.5f (%.1f V rms each half), Tbs %.5f (%.1f V rms)\n", ht, ht * 240.0, bs, bs * 240.0);
    std::printf ("EL34 idle: V1 %.2f mA, V2 %.2f mA, bias %.2f V\n", r.ia * 1e3, r.ib * 1e3, r.bias);
    std::printf ("rails: A %.1f  B %.1f  C %.1f  D %.1f  E %.1f  F %.1f V\n", r.a, r.b, r.c, r.d, r.e, r.f);
    return 0;
}
