/*  MARMALADE 120 - set the bias, the way a technician does

    Turns the set-bias pot (Rbp1 + Rbp2 in Or120Netlist.h, 47.2k between them)
    until the warmed-up amp idles at the target current per EL34, measured
    across the output transformer's winding resistance and averaged over the
    ripple, by bisection. Prints the two halves to put in the netlist, with
    the rails it lands, so a change elsewhere (a value read off a schematic)
    can be re-biased instead of left to run hot.

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
    constexpr double TOTAL = 47.2e3;
    constexpr double HT_PEAK = 375.0 * 1.41421356237, BIAS_PEAK = 50.0 * 1.41421356237;

    struct Result { double ia, bias, b1, b3, b4; };

    Result idle (double lower)
    {
        Circuit c;
        c.parse (OR120_AMP);
        c.setValue (c.elementIndex ("rbp2"), lower);
        c.setValue (c.elementIndex ("rbp1"), TOTAL - lower);
        const double fs = 16000.0;
        c.prepare (fs);
        double in[3] = { 0.0, HT_PEAK, -BIAS_PEAK };
        c.solveDC (in);
        std::vector<double> o ((size_t) c.numOutputs());
        const int ota = c.nodeIndex ("ota"), pa = c.nodeIndex ("pa");
        const double rw = c.getValue (c.elementIndex ("rwa"));
        Result r { 0, 0, 0, 0, 0 };
        const int n = (int) (6.0 * fs), avg = (int) (0.2 * fs);
        for (int i = 0; i < n; ++i)
        {
            const double m = std::sin (2.0 * M_PI * 50.0 * i / fs);
            in[1] = HT_PEAK * m; in[2] = BIAS_PEAK * m;
            c.process (in, o.data());
            if (i >= n - avg)
            {
                r.ia   += (c.nodeVoltage (ota) - c.nodeVoltage (pa)) / rw / 2.0 / avg;   // two valves a side
                r.bias += c.nodeVoltage (c.nodeIndex ("bias")) / avg;
                r.b1   += c.nodeVoltage (c.nodeIndex ("b1")) / avg;
                r.b3   += c.nodeVoltage (c.nodeIndex ("b3")) / avg;
                r.b4   += c.nodeVoltage (c.nodeIndex ("b4")) / avg;
            }
        }
        return r;
    }
}

int main (int argc, char** argv)
{
    const double target = (argc > 1 ? std::atof (argv[1]) : 35.0) * 1e-3;
    double lo = 10e3, hi = TOTAL - 100.0;       // more of the divider below the wiper: more negative, less current
    for (int k = 0; k < 16; ++k)
    {
        const double mid = 0.5 * (lo + hi);
        (idle (mid).ia > target ? lo : hi) = mid;
    }
    const double lower = 0.5 * (lo + hi);
    const Result r = idle (lower);
    std::printf ("Rbp1 %.0f  Rbp2 %.0f : %.2f mA a valve at %.2f V bias; B+ %.1f, rails %.1f / %.1f V\n",
                 TOTAL - lower, lower, r.ia * 1e3, r.bias, r.b1, r.b3, r.b4);
    return 0;
}
