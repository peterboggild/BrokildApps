/*  MARMALADE 50 - the split circuit against the whole one

    The engine solves the amp as four blocks (Tv50Split.h). This runs the
    same input through both - the whole netlist as one Circuit, and the
    Partition of it - from the same warmed-up start, and reports how far the
    split drifts from the whole: the speaker voltage's difference as a level
    below the signal, and the rails. It is the split's only excuse, so it
    exits 1 if the difference is not small.

        mm50split [seconds, default 1]
*/
#include "Circuit.h"
#include "Partition.h"
#include "Tv50Netlist.h"
#include "Tv50Split.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace mm;

namespace
{
    constexpr double PI = 3.14159265358979323846;
    constexpr double MAINS_PEAK = 240.0 * 1.41421356237;

    template <typename C>
    void warm (C& c, std::vector<double>& out)
    {
        c.prepare (16000.0);
        double in[4] = { 0.0, MAINS_PEAK, 0.0, 0.0 };
        if (! c.solveDC (in)) { std::printf ("DC failed: %s\n", c.error().c_str()); std::exit (2); }
        for (int i = 0; i < 16000 * 5; ++i)
        {
            in[1] = MAINS_PEAK * std::sin (2.0 * PI * 50.0 * i / 16000.0);
            c.process (in, out.data());
        }
    }

    // a plucked E power chord's worth of partials, decaying, so the test is music-like
    double guitar (double t, double level)
    {
        const double f[] = { 82.41, 123.47, 164.81 };
        double s = 0.0;
        const double tt = std::fmod (t, 0.5);
        for (double fr : f)
            for (int h = 1; h <= 6; ++h)
                s += std::sin (2.0 * PI * fr * h * tt) / (h * h);
        return level * 0.3 * s * std::exp (-tt * 4.0);
    }
}

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 1.0;
    const double fs = 88200.0;

    Circuit whole;
    if (! whole.parse (TV50_AMP)) { std::printf ("%s\n", whole.error().c_str()); return 2; }
    Partition split;
    if (! split.build (TV50_AMP, TV50_CUTS, TV50_ORDER)) { std::printf ("%s\n", split.error().c_str()); return 2; }
    std::printf ("%d blocks:", split.numBlocks());
    for (int b = 0; b < split.numBlocks(); ++b)
        std::printf (" [%d ports, %d states]", (split.block (b).prepare (fs), split.block (b).numPorts()), split.block (b).numStates());
    std::printf ("  whole: ");
    whole.prepare (fs);
    std::printf ("%d ports, %d states\n", whole.numPorts(), whole.numStates());

    std::vector<double> ow ((size_t) whole.numOutputs()), os ((size_t) split.numOutputs());
    const int spkW = whole.outputIndex ("spk"), spkS = split.outputIndex ("spk");
    const char* rails[] = { "a", "b", "c", "d", "e", "f", "bias" };

    bool bad = false;
    for (double level : { 0.05, 0.3, 1.0 })
    {
        warm (whole, ow);
        warm (split, os);
        whole.changeRate (fs);
        split.changeRate (fs);

        double sumW = 0.0, sumD = 0.0, railWorst = 0.0, tW = 0.0, tS = 0.0;
        const int n = (int) (seconds * fs);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            const double in[4] = { guitar (t, level), MAINS_PEAK * std::sin (2.0 * PI * 50.0 * t), 0.0, 0.0 };
            auto t0 = std::chrono::steady_clock::now();
            whole.process (in, ow.data());
            auto t1 = std::chrono::steady_clock::now();
            split.process (in, os.data());
            auto t2 = std::chrono::steady_clock::now();
            tW += std::chrono::duration<double> (t1 - t0).count();
            tS += std::chrono::duration<double> (t2 - t1).count();
            const double d = ow[(size_t) spkW] - os[(size_t) spkS];
            sumW += ow[(size_t) spkW] * ow[(size_t) spkW];
            sumD += d * d;
            for (const char* r : rails)
                railWorst = std::max (railWorst, std::abs (ow[(size_t) whole.outputIndex (r)] - os[(size_t) split.outputIndex (r)]));
        }
        const double rel = 10.0 * std::log10 (sumD / std::max (1e-30, sumW));
        std::printf ("guitar %.2f V: speaker %.1f V rms, split differs by %.1f dB below it; rails within %.4f V;"
                     " whole %.0f%%, split %.0f%% of a core; Newton failures %d / %d\n",
                     level, std::sqrt (sumW / n), rel, railWorst, 100.0 * tW / seconds, 100.0 * tS / seconds,
                     whole.failures(), split.failures());
        if (rel > -40.0) bad = true;
    }
    return bad ? 1 : 0;
}
