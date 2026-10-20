/*  MARMALADE 120 - set the bias winding

    The 1972 sheet has no bias adjustment: the negative supply is a
    centre-tapped winding, two diodes and a fixed RC filter, so the idle
    current is whatever that winding's voltage makes it. The voltage is not
    on the sheet. This finds the winding ratio (Tba/Tbb in Or120Netlist.h)
    that idles the warmed-up amp at the target current per EL34 - measured
    across the output transformer's winding resistance and averaged over the
    ripple, by bisection - and prints it with the rails it lands.

        mmbias [mA per valve, default 35]
*/
#include "Circuit.h"
#include "Or120Netlist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace mm;

namespace
{
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237;

    struct Result { double ia, bias, a, b, c, d; };

    Result idle (double ratio)
    {
        Circuit c;
        c.parse (OR120_AMP);
        c.setValue (c.elementIndex ("tba"), ratio);
        c.setValue (c.elementIndex ("tbb"), ratio);
        const double fs = 16000.0;
        c.prepare (fs);
        double in[2] = { 0.0, MAINS_PEAK };
        c.solveDC (in);
        std::vector<double> o ((size_t) c.numOutputs());
        const int ota = c.nodeIndex ("ota"), pa = c.nodeIndex ("pa");
        const double rw = c.getValue (c.elementIndex ("rwa"));
        auto V = [&] (const char* n) { return c.nodeVoltage (c.nodeIndex (n)); };
        Result r { 0, 0, 0, 0, 0, 0 };
        const int n = (int) (6.0 * fs), avg = (int) (0.2 * fs);
        for (int i = 0; i < n; ++i)
        {
            in[1] = MAINS_PEAK * std::sin (2.0 * M_PI * 50.0 * i / fs);
            c.process (in, o.data());
            if (i >= n - avg)
            {
                r.ia   += (c.nodeVoltage (ota) - c.nodeVoltage (pa)) / rw / 2.0 / avg;   // two valves a side
                r.bias += V ("nb") / avg;
                r.a += V ("ap") / avg; r.b += V ("bp") / avg; r.c += V ("cp") / avg; r.d += V ("dp") / avg;
            }
        }
        return r;
    }
}

int main (int argc, char** argv)
{
    const double target = (argc > 1 ? std::atof (argv[1]) : 35.0) * 1e-3;
    double lo = 0.05, hi = 0.6;          // more winding: more negative bias, less current
    for (int k = 0; k < 18; ++k)
    {
        const double mid = 0.5 * (lo + hi);
        (idle (mid).ia > target ? lo : hi) = mid;
    }
    const double ratio = 0.5 * (lo + hi);
    const Result r = idle (ratio);
    std::printf ("Tba/Tbb ratio %.5f (%.1f V rms each half): %.2f mA a valve at %.2f V bias\n"
                 "rails: A+ %.1f  B+ %.1f  C+ %.1f  D+ %.1f V\n",
                 ratio, ratio * 240.0, r.ia * 1e3, r.bias, r.a, r.b, r.c, r.d);
    return 0;
}
