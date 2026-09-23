/*  Which presets are shrill?
 *
 *      cmake --build <dir> --config Release --target ttyharsh
 *
 *  "Listen to them" has to become a measurement, and the measurement has to be
 *  of the thing that hurts. What makes a sound unpleasant to walk past is not
 *  its overall level but how much energy it puts where the ear is most
 *  sensitive: the 2-5 kHz band, where the ear canal resonates and where the
 *  equal-loudness contours dip hardest. A quiet patch with a lot there is more
 *  painful than a loud patch with none.
 *
 *  So the number that ranks them is the ABSOLUTE level in that band, not the
 *  fraction of the total -- a dark patch can be 90 % mid-band and still be
 *  perfectly comfortable. The fraction is printed too, because it says whether
 *  to pull the level or to shift the register.
 *
 *  Metallic resonators keep ringing long after a strike, so the window is the
 *  settled part of a held chord rather than the attack.
 */
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace tty;

struct Take { std::vector<float> L, R; double sr; int n() const { return (int) L.size(); } };

static Take render (Engine& e, double seconds, int block = 256)
{
    Take t; t.sr = e.sr; const int n = (int) (seconds * e.sr);
    t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m, nullptr, nullptr);
    }
    return t;
}
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }
static double rmsOf (const Take& t, int from, int to)
{
    double s = 0; int c = 0;
    for (int i = from; i < to && i < t.n(); ++i) { s += (double) t.L[(size_t) i] * t.L[(size_t) i]; ++c; }
    return c ? std::sqrt (s / c) : 0.0;
}
/*  energy between two corners, by a 2-pole high-pass into a 2-pole low-pass */
static double bandRms (const Take& t, double flo, double fhi, int from, int to)
{
    const double ah = std::exp (-2.0 * 3.14159265358979 * flo / t.sr);
    const double al = std::exp (-2.0 * 3.14159265358979 * fhi / t.sr);
    double h1 = 0, h2 = 0, l1 = 0, l2 = 0, acc = 0; int c = 0;
    for (int i = 0; i < to && i < t.n(); ++i)
    {
        double x = t.L[(size_t) i];
        h1 = x * (1 - ah) + h1 * ah;  x = x - h1;          // high-pass
        h2 = x * (1 - ah) + h2 * ah;  x = x - h2;
        l1 = x * (1 - al) + l1 * al;  x = l1;              // low-pass
        l2 = x * (1 - al) + l2 * al;  x = l2;
        if (i >= from) { acc += x * x; ++c; }
    }
    return c ? std::sqrt (acc / c) : 0.0;
}
static Engine* load (int idx, double sr = 48000.0)
{
    Engine* e = new Engine();
    applyPreset (idx, e->p, e->macro, e->scene, e->sceneSet, e->life.slots, e->life.mseg);
    e->prepare (sr, 256);
    return e;
}
static void playChord (Engine& e)
{
    const int root = (int) e.p[P_drone_root];
    e.noteOn (root, 0.8f, 0); e.noteOn (root + 7, 0.75f, 0); e.noteOn (root + 12, 0.7f, 0);
}

struct Row { int i; const char* name; const char* cat; double full, harsh, frac, harshNoSt; bool stOn; };

int main (int argc, char** argv)
{
    const double SECS = 10.0;
    const bool one = argc > 1;

    std::vector<Row> rows;
    for (int i = 0; i < numPresets(); ++i)
    {
        if (one && std::strcmp (preset (i).name, argv[1]) != 0) continue;
        Engine* e = load (i);
        playChord (*e);
        Take t = render (*e, SECS);
        const int from = (int) (4.0 * t.sr), to = t.n();       // the settled part
        const double full  = rmsOf (t, from, to);
        const double harsh = bandRms (t, 2000, 5000, from, to);
        const bool stOn = e->p.sw (P_st_on);
        delete e;

        /*  the same patch with STRUCTURE silenced: if the shrillness is the
            metal, this is where it goes. */
        Engine* f = load (i);
        f->p[P_st_on] = 0;
        playChord (*f);
        Take u = render (*f, SECS);
        const double harshNoSt = bandRms (u, 2000, 5000, from, to);
        delete f;

        rows.push_back ({ i, preset (i).name, preset (i).cat, full, harsh,
                          full > 0 ? harsh / full : 0, harshNoSt, stOn });
    }

    std::sort (rows.begin(), rows.end(), [] (const Row& a, const Row& b) { return a.harsh > b.harsh; });

    std::printf ("\n  2-5 kHz is where it hurts. Ranked by how much is there.\n");
    std::printf ("  %-3s %-32s %-20s %9s %7s %6s %10s\n",
                 "#", "preset", "family", "2-5kHz", "share", "STRUCT", "without it");
    std::printf ("  %-3s %-32s %-20s %9s %7s %6s %10s\n", "---",
                 "--------------------------------", "--------------------",
                 "---------", "-------", "------", "----------");
    for (const Row& r : rows)
        std::printf ("  %-3d %-32s %-20s %8.1f dB %6.0f %% %6s %9.1f dB\n",
                     r.i, r.name, r.cat, db (r.harsh), r.frac * 100.0,
                     r.stOn ? "on" : "-", db (r.harshNoSt));
    std::printf ("\n");
    return 0;
}
