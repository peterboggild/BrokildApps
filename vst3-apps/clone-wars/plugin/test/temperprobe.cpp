// Does GROWL (temper 0) actually render differently from SCREAM (temper 1)?
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static void render (cw::Engine& e, std::vector<float>& L, std::vector<float>& R,
                    int fs, double secs)
{
    const size_t n = (size_t) (secs * fs);
    L.assign (n, 0.f); R.assign (n, 0.f);
    size_t done = 0;
    while (done < n) {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + done, R.data() + done, m);
        done += (size_t) m;
    }
}

static void run (int temper, std::vector<float>& L, std::vector<float>& R, int fs, float cut)
{
    cw::Engine e;
    e.prepare (fs, 512);
    e.setGlobal (cw::gDrone, 1);
    e.setGlobal (cw::gTemperA, (float) temper);
    e.setGlobal (cw::gTemperB, (float) temper);
    for (int v = 0; v < cw::kVoices; ++v) {
        e.setVoice (v, cw::vfRes, 0.95f);
        e.setVoice (v, cw::vfCut, cut);
    }
    render (e, L, R, fs, 4.0);
}

int main()
{
    const int fs = 48000;
    std::vector<float> L0, R0, L1, R1, L2, R2;
    for (float cut : { 0.20f, 0.35f, 0.50f, 0.75f }) {
    run (0, L0, R0, fs, cut);   // growl
    run (1, L1, R1, fs, cut);   // scream
    run (2, L2, R2, fs, cut);   // ladder

    auto cmp = [&] (const char* tag, const std::vector<float>& a, const std::vector<float>& b) {
        double maxd = 0, sum = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            const double d = std::fabs ((double) a[i] - b[i]);
            maxd = std::max (maxd, d); sum += d * d;
        }
        printf ("  %-18s max|diff| %.3e   rms diff %.3e   %s\n", tag, maxd,
                std::sqrt (sum / a.size()), maxd == 0.0 ? "*** BIT-IDENTICAL ***" : "differs");
    };
    printf ("4 s drone, res 0.95, cut 0.75, 16 voices\n");
    cmp ("growl vs scream", L0, L1);
    cmp ("growl vs ladder", L0, L2);
    cmp ("scream vs ladder", L1, L2);
    }
    return 0;
}
