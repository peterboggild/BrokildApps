
/*  levelprobe - does LADDER sit quieter than GROWL and SCREAM?

    Peter, by ear, 2026-08-26: "LADDER filter seems to lower the volume".
    Plausible on physics (a 4-pole eats more top end than a 2-pole at the same
    CUT, and a real ladder loses passband level as resonance rises) - but Clone
    Wars had the identical complaint and there it was REAL, so it is settled by
    measurement, not by ear-matching and not by reasoning.

    This asserts nothing. It prints numbers (the boquality pattern).

    Everything that could move the cutoff behind the probe's back is pinned:
    EG1 > CUTOFF, KEY TRACK and LFO > CUTOFF all to zero. Brain Scan's tuning
    probe was wrecked by exactly this - KEY TRACK defaults to 50 %% and pulled a
    1 kHz cutoff to 917 Hz, which read as the filter model being 150 cents flat.
    FILTER DRIVE is swept separately because it is a level control in its own
    right and the seeds use its default of 0.2.                               */
#include "Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <tuple>
using namespace bk;

struct Render { std::vector<float> L, R; };

static Render render (Engine& e, double sr, int block, double seconds, int note, float vel)
{
    Render r; const int n = (int) (sr * seconds);
    r.L.assign ((size_t) n, 0.0f); r.R.assign ((size_t) n, 0.0f);
    e.noteOn (note, vel);
    for (int pos = 0; pos < n; pos += block)
    {
        const int nb = std::min (block, n - pos);
        e.process (r.L.data() + pos, r.R.data() + pos, nb);
    }
    return r;
}

static double rmsWindow (const Render& r, double sr, double t0, double t1)
{
    const int a = (int) (t0 * sr), b = std::min ((int) (t1 * sr), (int) r.L.size());
    double acc = 0; int n = 0;
    for (int i = a; i < b; ++i) { acc += (double) r.L[(size_t) i] * r.L[(size_t) i]
                                       + (double) r.R[(size_t) i] * r.R[(size_t) i]; ++n; }
    return n ? std::sqrt (acc / (2.0 * n)) : 0.0;
}

static double dB (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

int main()
{
    const double sr = 48000.0; const int block = 256;
    const char* names[3] = { "GROWL", "SCREAM", "LADDER" };

    for (int pass = 0; pass < 2; ++pass)
    {
        const float fdrive = pass == 0 ? 0.0f : 0.2f;
        std::printf ("\n=== FILTER DRIVE %.2f %s ===\n", (double) fdrive,
                     pass == 0 ? "(isolated: only the model differs)" : "(the shipped default, as the seeds use it)");
        std::printf ("  CUT  PEAK |   GROWL   SCREAM   LADDER |  LAD-GRO  LAD-SCR\n");

        double sumLG = 0, sumLS = 0; int nCase = 0;
        double worstLG = 0; float worstCut = 0, worstPeak = 0;

        for (int ci = 0; ci < 6; ++ci)
            for (int pi = 0; pi < 4; ++pi)
            {
                const float cut  = 0.25f + 0.13f * (float) ci;     // 0.25 .. 0.90
                const float peak = 0.00f + 0.30f * (float) pi;     // 0.0 .. 0.9
                double lev[3];
                for (int m = 0; m < 3; ++m)
                {
                    Params p;                       // defaults
                    p.fmodel = m;
                    p.lpf = cut; p.lpeak = peak; p.fdrive = fdrive;
                    p.fenv = 0.0f;                  // EG1 > CUTOFF: bipolar, defaults 0.65
                    p.fkey = 0.0f;                  // KEY TRACK: defaults 0.5 - the Brain Scan trap
                    p.flfo = 0.0f;                  // LFO > CUTOFF
                    p.hpf = 0.0f; p.hpeak = 0.0f;   // the series HPF out of the way
                    p.e2s = 1.0f;                   // amp sustain full: a steady note, not a decay
                    p.e2a = 0.0f;
                    p.drv = 0.0f;                   // no post drive
                    Engine e; e.p = p; e.prepare (sr, block);
                    auto r = render (e, sr, block, 1.2, 45, 0.8f);   // 110 Hz, settled window below
                    lev[m] = rmsWindow (r, sr, 0.5, 1.1);
                }
                const double dLG = dB (lev[2]) - dB (lev[0]);
                const double dLS = dB (lev[2]) - dB (lev[1]);
                sumLG += dLG; sumLS += dLS; ++nCase;
                if (dLG < worstLG) { worstLG = dLG; worstCut = cut; worstPeak = peak; }
                std::printf (" %.2f  %.2f | %7.1f  %7.1f  %7.1f | %+7.1f  %+7.1f\n",
                             (double) cut, (double) peak, dB (lev[0]), dB (lev[1]), dB (lev[2]), dLG, dLS);
            }
        std::printf ("  mean LADDER - GROWL  %+.2f dB\n", sumLG / nCase);
        std::printf ("  mean LADDER - SCREAM %+.2f dB\n", sumLS / nCase);
        std::printf ("  worst LADDER deficit %+.2f dB at CUT %.2f PEAK %.2f\n",
                     worstLG, (double) worstCut, (double) worstPeak);
    }
    (void) names;
    return 0;
}
