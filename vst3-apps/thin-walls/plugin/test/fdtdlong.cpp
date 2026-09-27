// twfdtdlong: the wave simulation over a long take. A kick every 2 s (a
// decaying 55 Hz burst) into the default apartment, and per second the
// loudest 100 ms block mean and rms at the ears - a slow instability shows as
// a number that keeps growing.
//   twfdtdlong [seconds] [scene]   scene 0 default, 1 doors shut, 2 all hard, 3 one kick then silence
#include "Engine.h"
#include "Fdtd.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
using namespace tw;

int main (int argc, char** argv)
{
    const double fs = 44100.0;
    const double seconds = argc > 1 ? std::atof (argv[1]) : 60.0;
    const int scene = argc > 2 ? std::atoi (argv[2]) : 0;
    const int n = (int) (seconds * fs), nb = (n + 255) / 256;
    Params p;
    if (scene == 1) { p.door[0] = p.door[1] = p.door[2] = 0; }
    if (scene == 2) { for (int r = 0; r < NUM_ROOMS; ++r) { p.material[r] = 1; p.floorMat[r] = 1; p.ceilMat[r] = 1; } }
    std::vector<Params> blocks ((size_t) nb, p);
    std::vector<float> mono[MAX_SOURCES];
    mono[0].assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double t = std::fmod (i / fs, 2.0);
        if (scene == 3 && i > 2 * fs) { mono[0][(size_t) i] = 0; continue; }
        mono[0][(size_t) i] = (float) (0.8 * std::sin (2 * 3.14159265358979 * 55.0 * t) * std::exp (-t / 0.15));
    }
    std::vector<float> L, R;
    FdtdStats st;
    fdtdLowBand (blocks, 256, mono, n, fs, L, R, &st);
    std::printf ("cells %lld  rate %.1f  steps %lld  rebuilds %d  %.1f s\n", st.cells, st.rate, st.steps, st.rebuilds, st.seconds);
    const int blk = (int) (0.1 * fs);
    for (int s = 0; s * fs < n; ++s)
    {
        double mxdc = 0, e = 0; float pk = 0;
        for (int b = 0; b < 10; ++b)
        {
            const int i0 = (int) (s * fs) + b * blk; if (i0 + blk > n) break;
            double dc = 0; for (int k = 0; k < blk; ++k) { const float v = L[(size_t) (i0 + k)]; dc += v; e += (double) v * v; pk = std::max (pk, std::abs (v)); }
            mxdc = std::max (mxdc, std::abs (dc / blk));
        }
        std::printf ("%4d dc %.5f rms %.5f peak %.4f\n", s, mxdc, std::sqrt (e / fs), pk);
    }
    return 0;
}
