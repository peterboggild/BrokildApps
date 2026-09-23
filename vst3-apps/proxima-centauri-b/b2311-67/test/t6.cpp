/*  What actually carries the SUSTAINED tone, and how much of the
    four-dimensional structure reaches it.

    Peter's report: everything shows up in the transient, but the tone that
    remains while a key is held "lives a quieter life". Measure it. */

#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

static const int SR = 48000;

static double goertzel (const float* x, int n, double f)
{
    const double w = 2.0 * ab::PI * f / SR, coeff = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s0 = x[i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (std::max (0.0, s1*s1 + s2*s2 - coeff*s1*s2)) / n;
}

static std::vector<double> print (const std::vector<float>& L, int a0, int a1, int nb = 20)
{
    std::vector<double> fp ((size_t) nb);
    for (int b = 0; b < nb; ++b)
    {
        const double f = 60.0 * std::pow (2.0, b * 0.40);
        fp[(size_t) b] = std::log (1e-9 + goertzel (L.data() + a0, a1 - a0, f));
    }
    return fp;
}
static double dist (const std::vector<double>& a, const std::vector<double>& b)
{
    double d = 0; for (size_t i = 0; i < a.size(); ++i) { const double q = a[i] - b[i]; d += q*q; }
    return std::sqrt (d);
}

struct Run { double transRms, susRms; std::vector<double> trans, sus; };

static Run render (int habit, int note, double travel, float blend, float drive, float drift, float sustain)
{
    ax::Engine e; e.prepare ((double) SR, 512);
    ax::applyHabit (habit, e.p);
    if (blend >= 0) e.p.blend = blend;
    e.p.drive = drive; e.p.drift = drift; e.p.sustain = sustain;
    e.p.travel = (float) travel;
    e.service();
    std::vector<float> L ((size_t) SR * 3), R ((size_t) SR * 3);
    e.noteOn (note, 0.95f);
    for (int off = 0; off < SR * 3; off += 512)
    { e.service(); e.process (L.data() + off, R.data() + off, std::min (512, SR * 3 - off)); }

    Run r;
    const int t0 = 400, t1 = (int) (SR * 0.28);              // the strike
    const int s0 = (int) (SR * 1.6), s1 = (int) (SR * 2.9);  // what is left
    double a = 0, b = 0;
    for (int i = t0; i < t1; ++i) a += (double) L[(size_t)i] * L[(size_t)i];
    for (int i = s0; i < s1; ++i) b += (double) L[(size_t)i] * L[(size_t)i];
    r.transRms = std::sqrt (a / (t1 - t0));
    r.susRms   = std::sqrt (b / (s1 - s0));
    r.trans = print (L, t0, t1);
    r.sus   = print (L, s0, s1);
    return r;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const int H = 58, N = 52;

    std::printf ("=== 1 - what carries the sustain? (habit %d, note %d) ===\n", H, N);
    {
        Run both = render (H, N, 0.5, -1,   0.0f, 0.0f, 0.35f);
        Run fil  = render (H, N, 0.5, 0.0f, 0.0f, 0.0f, 0.35f);
        Run sta  = render (H, N, 0.5, 1.0f, 0.0f, 0.0f, 0.35f);
        std::printf ("  transient rms   both %.5f   filament-only %.5f   star-only %.5f\n",
                     both.transRms, fil.transRms, sta.transRms);
        std::printf ("  SUSTAIN   rms   both %.5f   filament-only %.5f   star-only %.5f\n",
                     both.susRms, fil.susRms, sta.susRms);
        std::printf ("  -> the filament keeps %.2f %% of its own transient into the sustain\n",
                     100.0 * fil.susRms / std::max (1e-9, fil.transRms));
        std::printf ("  -> the star     keeps %.2f %% of its own transient into the sustain\n",
                     100.0 * sta.susRms / std::max (1e-9, sta.transRms));
    }

    std::printf ("\n=== 2 - how much does TRAVELLING change each part? ===\n");
    {
        Run a = render (H, N, 0.50, -1, 0.0f, 0.0f, 0.35f);
        Run b = render (H, N, 0.66, -1, 0.0f, 0.0f, 0.35f);
        Run c = render (H, N, 0.85, -1, 0.0f, 0.0f, 0.35f);
        std::printf ("  transient spectrum moves   tau 0->3.8 : %.3f    tau 0->8.4 : %.3f\n",
                     dist (a.trans, b.trans), dist (a.trans, c.trans));
        std::printf ("  SUSTAIN   spectrum moves   tau 0->3.8 : %.3f    tau 0->8.4 : %.3f\n",
                     dist (a.sus, b.sus), dist (a.sus, c.sus));
    }

    std::printf ("\n=== 3 - how much does the SPECIMEN change each part? ===\n");
    {
        Run a = render (11, N, 0.5, -1, 0.0f, 0.0f, 0.35f);
        Run b = render (58, N, 0.5, -1, 0.0f, 0.0f, 0.35f);
        Run c = render (203, N, 0.5, -1, 0.0f, 0.0f, 0.35f);
        std::printf ("  transient   11 vs 58 : %.3f    11 vs 203 : %.3f\n",
                     dist (a.trans, b.trans), dist (a.trans, c.trans));
        std::printf ("  SUSTAIN     11 vs 58 : %.3f    11 vs 203 : %.3f\n",
                     dist (a.sus, b.sus), dist (a.sus, c.sus));
    }

    std::printf ("\n=== 4 - the two controls that could feed the body into the sustain ===\n");
    {
        Run base = render (H, N, 0.5, -1, 0.00f, 0.0f, 0.35f);
        Run drv  = render (H, N, 0.5, -1, 0.45f, 0.0f, 0.35f);
        Run pre  = render (H, N, 0.5, -1, 0.00f, 0.5f, 0.35f);
        std::printf ("  sustain rms   default %.5f   SUSTAINED FORCE %.5f   PRECESSION %.5f\n",
                     base.susRms, drv.susRms, pre.susRms);

        Run b0 = render (H, N, 0.50, -1, 0.45f, 0.0f, 0.35f);
        Run b1 = render (H, N, 0.85, -1, 0.45f, 0.0f, 0.35f);
        std::printf ("  with SUSTAINED FORCE the SUSTAIN moves under travel 0->8.4 : %.3f\n",
                     dist (b0.sus, b1.sus));
        Run c0 = render (H, N, 0.50, -1, 0.00f, 0.5f, 0.35f);
        Run c1 = render (H, N, 0.85, -1, 0.00f, 0.5f, 0.35f);
        std::printf ("  with PRECESSION      the SUSTAIN moves under travel 0->8.4 : %.3f\n",
                     dist (c0.sus, c1.sus));
    }
    return 0;
}
