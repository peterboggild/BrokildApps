// Magneto Transport - where the time goes (wall time, best of nine, so a busy machine reads its quietest second): the cost test's configuration with one
// part of the machine taken away at a time. Prints, asserts nothing.
//   mtcost

#include "../Source/Engine.h"

#include <cstdio>
#include <functional>
#include <random>
#include <vector>
#define NOMINMAX
#include <windows.h>

using namespace mt;

static double wallSeconds()
{
    LARGE_INTEGER f, c; QueryPerformanceFrequency (&f); QueryPerformanceCounter (&c);
    return (double) c.QuadPart / (double) f.QuadPart;
}

static double timeIt (const char* what, std::function<void (Params&, Engine&)> setup)
{
    const double fs = 48000.0, seconds = 1.0;
    const size_t n = (size_t) (seconds * fs);
    std::vector<float> x (n);
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> u (-0.2f, 0.2f);
    for (auto& v : x) v = u (rng);
    Params p; p.speed = 76.0f; p.h1On = p.h2On = p.h3On = 1; p.feedback = 0.6f; p.mix = 0.5f;
    Engine e;
    setup (p, e);
    e.setParams (p); e.prepare (fs, 256);
    std::vector<float> L (256), R (256);
    double best = 1e9;
    for (int r = 0; r < 9; ++r)
    {
        const double t0 = wallSeconds();
        for (size_t i = 0; i < n; i += 256) e.process (x.data() + i, x.data() + i, L.data(), R.data(), 256);
        best = std::min (best, wallSeconds() - t0);
    }
    const double pct = 100.0 * best / seconds;
    std::printf ("  %-48s %6.2f %%\n", what, pct);
    return pct;
}

int main()
{
    std::printf ("Magneto Transport - cost by part (48 kHz, 76 cm/s, three heads, feedback)\n");
    timeIt ("default (studio, hiss, condition 15)", [] (Params&, Engine&) {});
    timeIt ("ideal medium, condition 15 (azimuth only)", [] (Params&, Engine& e) { e.setIdealMedium (true); });
    timeIt ("ideal medium, condition 0 (shaper idle)", [] (Params& p, Engine& e) { e.setIdealMedium (true); p.condition = 0; });
    timeIt ("real medium, condition 0", [] (Params& p, Engine&) { p.condition = 0; });
    timeIt ("real medium, particles silent, condition 0", [] (Params& p, Engine&) { p.condition = 0; p.particles = 1000.0f; });
    timeIt ("shedding stock, condition 100", [] (Params& p, Engine&) { p.stock = 2; p.condition = 100; });
    timeIt ("default at 19 cm/s", [] (Params& p, Engine&) { p.speed = 19.0f; });
    return 0;
}
