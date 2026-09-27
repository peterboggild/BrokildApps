// "The moment I choose LADDER the whole thing goes quiet" - where does the level go?
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static double runRms (int temper, float cut, float res, int fs)
{
    cw::Engine e;
    e.prepare (fs, 512);
    e.setGlobal (cw::gDrone, 1);
    e.setGlobal (cw::gTemperA, (float) temper);
    e.setGlobal (cw::gTemperB, (float) temper);
    e.setGlobal (cw::gTolerance, 0.f);
    for (int v = 0; v < cw::kVoices; ++v) {
        e.setVoice (v, cw::vfCut, cut);
        e.setVoice (v, cw::vfRes, res);
    }
    const size_t n = (size_t) (3.0 * fs);
    std::vector<float> L (n, 0.f), R (n, 0.f);
    size_t done = 0;
    while (done < n) {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + done, R.data() + done, m);
        done += (size_t) m;
    }
    double s = 0; const size_t from = n / 2;
    for (size_t i = from; i < n; ++i) s += (double) L[i] * L[i];
    return std::sqrt (s / (double) (n - from));
}

int main()
{
    const int fs = 48000;
    const float cuts[] = { 0.30f, 0.50f, 0.65f, 0.85f, 1.00f };
    const float rezz[] = { 0.00f, 0.20f, 0.50f, 0.80f, 0.95f };
    const char* names[] = { "GROWL ", "SCREAM", "LADDER" };

    printf ("RMS in dB relative to GROWL at the same cut/res (default drone, TOLERANCE 0)\n\n");
    for (float cut : cuts)
    {
        printf ("  cut %.2f\n", cut);
        printf ("        res:     0.00     0.20     0.50     0.80     0.95\n");
        double ref[5];
        for (int r = 0; r < 5; ++r) ref[r] = runRms (0, cut, rezz[r], fs);
        for (int t = 0; t < 3; ++t)
        {
            printf ("    %s ", names[t]);
            for (int r = 0; r < 5; ++r)
            {
                const double v = runRms (t, cut, rezz[r], fs);
                if (t == 0) printf ("  %7.4f", v);
                else        printf ("  %+6.1f dB", 20.0 * std::log10 (std::max (1e-9, v / std::max (1e-9, ref[r]))));
            }
            printf ("\n");
        }
        printf ("\n");
    }
    return 0;
}
