// Does the CUT knob actually move the filter?
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static void render (cw::Engine& e, std::vector<float>& L, int fs, double secs)
{
    const size_t n = (size_t) (secs * fs);
    std::vector<float> R (n, 0.f);
    L.assign (n, 0.f);
    size_t done = 0;
    while (done < n) {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + done, R.data() + done, m);
        done += (size_t) m;
    }
}

static void run (float cut, std::vector<float>& L, int fs, int temper)
{
    cw::Engine e;
    e.prepare (fs, 512);
    e.setGlobal (cw::gDrone, 1);
    e.setGlobal (cw::gTemperA, (float) temper);
    e.setGlobal (cw::gTemperB, (float) temper);
    e.setGlobal (cw::gTolerance, 0.f);          // no per-clone scatter
    for (int v = 0; v < cw::kVoices; ++v) e.setVoice (v, cw::vfCut, cut);
    render (e, L, fs, 3.0);
}

// Goertzel magnitude at f over the tail of the buffer
static double mag (const std::vector<float>& x, size_t from, double f, int fs)
{
    const double w = 2.0 * M_PI * f / fs;
    const double c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < x.size(); ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (x.size() - from);
}

int main()
{
    const int fs = 48000;
    printf ("default drone, TOLERANCE 0, 16 voices, steady state (last 1.5 s)\n");
    printf ("  cut   predicted fc   centroid    >2kHz share\n");
    std::vector<std::vector<float>> keep;
    for (int i = 0; i <= 10; ++i)
    {
        const float cut = i / 10.0f;
        std::vector<float> L;
        run (cut, L, fs, 0);
        const size_t from = L.size() / 2;
        double num = 0, den = 0, hi = 0;
        for (int b = 0; b < 48; ++b) {
            const double f = 40.0 * std::pow (16000.0 / 40.0, b / 47.0);
            const double m = mag (L, from, f, fs);
            num += m * f; den += m;
            if (f > 2000.0) hi += m;
        }
        printf ("  %.1f   %8.0f Hz   %7.0f Hz   %6.1f %%\n",
                cut, 30.0 * std::pow (533.0, cut), den > 0 ? num / den : 0.0,
                den > 0 ? 100.0 * hi / den : 0.0);
        keep.push_back (L);
    }
    printf ("\nbit-identity between neighbouring cut settings:\n");
    for (size_t i = 1; i < keep.size(); ++i) {
        double maxd = 0;
        for (size_t k = 0; k < keep[i].size(); ++k)
            maxd = std::max (maxd, std::fabs ((double) keep[i][k] - keep[i-1][k]));
        printf ("  cut %.1f vs %.1f : max|diff| %.3e  %s\n", i / 10.0, (i - 1) / 10.0,
                maxd, maxd == 0.0 ? "*** IDENTICAL - knob does nothing here ***" : "");
    }
    return 0;
}
