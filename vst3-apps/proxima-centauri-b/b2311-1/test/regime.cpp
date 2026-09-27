/*  ARTEFACT B2311.1 — where does it stop being a texture and start being a pulse?

    At the first defaults the object fires about eleven thousand times a second.
    That is a TONE, which the thesis says it should be at that rate — but the
    brief asks for rhythmic pulses, and a pulse needs silence around it. So what
    is wanted is the regime where the lattice CLUMPS: most units held below
    threshold, released together, then nothing for a while.

    That regime has a name in this mechanism. Mirollo and Strogatz: strong
    coupling drives pulse-coupled units to fire together. With a spread of
    natural rates they do not all join one group, they fall into CLUSTERS — and
    clusters firing at different periods is exactly "several layers following
    different timings".

    Measured here, per configuration:

      firings/s      how busy it is
      silent %       of lattice steps, how many had nothing at all in them
                     -- this is the number that separates rhythm from texture
      biggest        largest single cascade
      R              how far it leans towards an imposed 8 Hz beat
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

static const int SR = 48000, BLK = 64;

struct M { double fps, silent, R; int biggest; double rms; };

static M probe (ab1::Params p, double secs, double bpm)
{
    ab1::Engine e; e.p = p;
    e.prepare (SR, BLK); e.service();
    e.setTransport (bpm, 0.0, true);
    e.noteOn (48, 0.9f);

    static const double DIV[7] = { 4.0, 2.0, 1.0, 0.5, 1.0/3.0, 0.25, 0.125 };
    const int di = (int) std::lround (std::max (0.0f, std::min (6.0f, p.division)));
    const double pulseHz = (bpm / 60.0) / DIV[di];

    const int nb = (int) (secs * SR / BLK);
    std::vector<float> bl (BLK), br (BLK);
    double sx = 0, sy = 0, n = 0, ss = 0;

    for (int b = 0; b < nb; ++b)
    {
        e.process (bl.data(), br.data(), BLK);
        for (int i = 0; i < BLK; ++i)
        {
            const double tt = (double)(b * BLK + i) / SR;
            const double a = 2.0 * ab1::PI * (tt * pulseHz - std::floor (tt * pulseHz));
            const double amp = std::abs (bl[(size_t)i]);
            ss += amp * amp;
            if (b > nb / 3 && amp > 0) { sx += std::cos(a)*amp; sy += std::sin(a)*amp; n += amp; }
        }
    }
    M m;
    m.fps = (double) e.totalFired.load() / secs;
    const double st = (double) e.stepsRun.load();
    m.silent = st > 0 ? 100.0 * e.silentSteps.load() / st : 0;
    m.biggest = e.biggestCascade.load();
    m.R = n > 0 ? std::hypot (sx, sy) / n : 0;
    m.rms = std::sqrt (ss / (nb * BLK));
    return m;
}

int main()
{
    ab1::Params base;

    std::printf ("ARTEFACT B2311.1 - texture or pulse?\n\n");
    std::printf ("1. CONDUCTION AND DEAD TIME: what makes it clump?\n");
    std::printf ("   couple  dead   firings/s   silent%%   biggest      R\n");
    for (float cp : { 0.30f, 0.60f, 0.80f, 0.95f })
      for (float dd : { 0.20f, 0.55f, 0.90f })
      {
        ab1::Params p = base; p.couple = cp; p.dead = dd;
        M m = probe (p, 3.0, 128.0);
        std::printf ("   %6.2f  %.2f  %10.0f  %8.1f  %7d  %5.3f\n",
                     cp, dd, m.fps, m.silent, m.biggest, m.R);
      }

    std::printf ("\n2. THE SPREAD OF RATES: one clock, or several?\n");
    std::printf ("   slowest fastest   firings/s   silent%%   biggest      R\n");
    for (float lo : { 0.05f, 0.20f })
      for (float hi : { 0.25f, 0.50f, 0.90f })
      {
        if (hi <= lo) continue;
        ab1::Params p = base; p.rateLo = lo; p.rateHi = hi; p.couple = 0.80f; p.dead = 0.55f;
        M m = probe (p, 3.0, 128.0);
        std::printf ("   %7.2f %7.2f  %10.0f  %8.1f  %7d  %5.3f\n",
                     lo, hi, m.fps, m.silent, m.biggest, m.R);
      }

    std::printf ("\n3. GRIP, AGAIN: why does leaning fall off when shoved harder?\n");
    std::printf ("   grip   firings/s   silent%%   biggest      R\n");
    for (float g : { 0.05f, 0.20f, 0.35f, 0.55f, 0.75f, 0.95f })
    {
        ab1::Params p = base; p.couple = 0.80f; p.dead = 0.55f; p.grip = g;
        M m = probe (p, 5.0, 128.0);
        std::printf ("   %5.2f  %10.0f  %8.1f  %7d  %5.3f\n", g, m.fps, m.silent, m.biggest, m.R);
    }

    std::printf ("\n4. A CANDIDATE SET OF DEFAULTS\n");
    {
        ab1::Params p = base;
        p.couple = 0.80f; p.dead = 0.55f; p.rateLo = 0.05f; p.rateHi = 0.30f; p.grip = 0.35f;
        M m = probe (p, 6.0, 128.0);
        std::printf ("   firings/s %.0f   silent %.1f%%   biggest %d   R %.3f   rms %.4f\n",
                     m.fps, m.silent, m.biggest, m.R, m.rms);
        for (double bpm : { 90.0, 128.0, 170.0 })
        {
            M q = probe (p, 6.0, bpm);
            std::printf ("   at %3.0f bpm: R %.3f, silent %.1f%%\n", bpm, q.R, q.silent);
        }
    }
    return 0;
}
