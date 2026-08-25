// "When ANY army was on LADDER, everything went quiet - even the GROWL army."
// Two tests: (1) a muted army must not affect the output at all, whatever its
// temper; (2) the four temper pairings, measured on the real mixed bus.
#include "cw_core.h"
#include <cstdio>
#include <cmath>
#include <vector>

static void run (int tA, int tB, bool muteB, std::vector<float>& L, int fs)
{
    cw::Engine e;
    e.prepare (fs, 512);
    e.setGlobal (cw::gDrone, 1);
    e.setGlobal (cw::gTemperA, (float) tA);
    e.setGlobal (cw::gTemperB, (float) tB);
    e.setGlobal (cw::gTolerance, 0.f);
    for (int v = 0; v < cw::kVoices; ++v) {
        e.setVoice (v, cw::vfRes, 0.85f);            // where the old ladder died
        if (muteB && v >= cw::kArmySize) e.setVoice (v, cw::vfMute, 1.f);
    }
    const size_t n = (size_t) (4.0 * fs);
    std::vector<float> R (n, 0.f);
    L.assign (n, 0.f);
    size_t done = 0;
    while (done < n) {
        const int m = (int) std::min<size_t> (512, n - done);
        e.process (L.data() + done, R.data() + done, m); done += (size_t) m;
    }
}
static double rms (const std::vector<float>& x)
{
    double s = 0; const size_t from = x.size() / 2;
    for (size_t i = from; i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (x.size() - from));
}

int main()
{
    const int fs = 48000;
    const char* nm[] = { "GROWL", "SCREAM", "LADDER" };

    printf ("(1) army B MUTED - its temper must change nothing at all\n");
    std::vector<float> ref, alt;
    run (0, 0, true, ref, fs);
    bool clean = true;
    for (int tB = 1; tB < 3; ++tB) {
        run (0, tB, true, alt, fs);
        double maxd = 0;
        for (size_t i = 0; i < ref.size(); ++i) maxd = std::max (maxd, std::fabs ((double) ref[i] - alt[i]));
        printf ("    A=GROWL, B=%-6s muted : max|diff| %.3e  %s\n", nm[tB], maxd,
                maxd == 0.0 ? "identical - no coupling" : "*** LEAKS ***");
        if (maxd != 0.0) clean = false;
    }

    printf ("\n(2) both armies audible, res 0.85 - level of the mixed bus\n");
    std::vector<float> b;
    run (0, 0, false, b, fs);
    const double base = rms (b);
    for (int tA = 0; tA < 3; ++tA)
        for (int tB = 0; tB < 3; ++tB)
        {
            run (tA, tB, false, b, fs);
            printf ("    A=%-6s B=%-6s : rms %.4f  %+5.1f dB vs GROWL/GROWL\n",
                    nm[tA], nm[tB], rms (b), 20.0 * std::log10 (rms (b) / base));
        }
    printf ("\n%s\n", clean ? "no cross-army coupling" : "CROSS-ARMY COUPLING PRESENT");
    return 0;
}
