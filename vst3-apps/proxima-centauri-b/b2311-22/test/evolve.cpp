/*  ARTEFACT B2311.22 — how much does it actually EVOLVE, and does playing it
    leave it changed?

    Peter, 2026-09-02: "focus a lot more on how the sounds evolve over time -
    and let that evolution be determined by when and what notes are played...
    they should definitely also change as well as the sound they produce...
    a greater change when pushing it through its 2D PROJECTION (IT IS REALLY
    4D) ... would be striking".

    Four questions, measured before anything is designed:

      1. A HELD note — how far does its colour actually travel, and over what
         time? (The design claims 535 -> 442 Hz; over how long?)
      2. THE SECTION — playing displaces it. By how much, and for how long
         before the winch recall drags it back?
      3. THE WINCH — sweeping it across its whole range: how different does
         the object actually sound at one end against the other?
      4. MEMORY OF BEING PLAYED — take two objects, play twenty notes into one
         and nothing into the other, wait, then play the SAME note into both.
         Do they answer differently? If not, nothing about playing persists.

        abevolve
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

static const int SR = 48000, BLK = 128;

//  RMS frequency: exact second moment of the power spectrum, O(n), no FFT.
static double rmsFreq (const std::vector<float>& v, size_t a, size_t b)
{
    double se = 0, sd = 0;
    for (size_t i = a + 1; i < b && i < v.size(); ++i)
    { const double x = v[i], d = x - v[i-1]; se += x*x; sd += d*d; }
    return se > 1e-30 ? (SR / (2.0 * 3.14159265358979)) * std::sqrt (sd / se) : 0.0;
}
static double rmsOf (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; size_t n = 0;
    for (size_t i = a; i < b && i < v.size(); ++i) { s += (double) v[i]*v[i]; ++n; }
    return n ? std::sqrt (s / n) : 0.0;
}

struct Rig
{
    ab::Engine e;
    std::vector<float> L, R, bl, br;
    Rig (const ab::Params& p) : bl (BLK), br (BLK)
    { e.p = p; e.prepare (SR, BLK); }
    void run (double secs)
    {
        const int nb = (int) (secs * SR / BLK);
        for (int b = 0; b < nb; ++b)
        {
            e.process (bl.data(), br.data(), BLK);
            L.insert (L.end(), bl.begin(), bl.end());
            R.insert (R.end(), br.begin(), br.end());
        }
    }
    size_t at (double secs) const { return (size_t) (secs * SR); }
};

int main()
{
    std::printf ("ARTEFACT B2311.22 — how much does it evolve?  (build under test)\n\n");
    ab::Params def;

    //  ---------------------------------------------------------------- 1 --
    std::printf ("1. A HELD NOTE: where does its colour go, and when?\n");
    std::printf ("   %8s %12s %12s %12s\n", "window", "colour Hz", "vs 0-1 s", "level");
    {
        Rig r (def);
        r.run (0.2);
        r.e.noteOn (52, 0.9f);
        r.run (30.0);
        const double win[6][2] = { {0.3,1.3}, {2,3}, {5,6}, {10,11}, {20,21}, {28,29} };
        double first = 0;
        for (int i = 0; i < 6; ++i)
        {
            const double f = rmsFreq (r.L, r.at (win[i][0]), r.at (win[i][1]));
            const double a = rmsOf   (r.L, r.at (win[i][0]), r.at (win[i][1]));
            if (i == 0) first = f;
            std::printf ("   %5.0f-%2.0fs %12.0f %11.1f %% %12.5f\n",
                         win[i][0], win[i][1], f, 100.0 * (f - first) / std::max (1.0, first), a);
        }
    }

    //  ---------------------------------------------------------------- 2 --
    std::printf ("\n2. THE SECTION: playing displaces it — how far, and for how long?\n");
    {
        Rig r (def);
        r.run (0.5);
        const float before = r.e.sectionW();
        r.e.noteOn (52, 1.0f);
        r.run (0.25);
        const float atNote = r.e.sectionW();
        r.e.noteOff (52);
        double halfBack = -1;
        for (int i = 1; i <= 60; ++i)
        {
            r.run (0.25);
            const float now = r.e.sectionW();
            if (halfBack < 0 && std::abs (now - before) < 0.5f * std::abs (atNote - before))
                halfBack = 0.25 * i;
        }
        std::printf ("   w0 before %.4f, at the note %.4f (moved %.4f)\n",
                     before, atNote, atNote - before);
        std::printf ("   half of that displacement was gone after %.2f s\n", halfBack);
        std::printf ("   settled at %.4f after 15 s\n", r.e.sectionW());
    }

    //  ---------------------------------------------------------------- 3 --
    std::printf ("\n3. THE WINCH: how different is the object at one end against the other?\n");
    std::printf ("   %8s %12s %12s %12s\n", "winch", "colour Hz", "level", "vs winch 0");
    {
        std::vector<float> ref;
        for (int i = 0; i <= 4; ++i)
        {
            ab::Params p = def; p.winch = i / 4.0f; p.wake = 0.0f;
            Rig r (p);
            r.run (2.0);                       // let the winch settle
            r.e.noteOn (52, 0.9f);
            r.run (4.0);
            std::vector<float> seg (r.L.begin() + (long) r.at (2.5), r.L.begin() + (long) r.at (6.0));
            const double f = rmsFreq (seg, 0, seg.size());
            const double a = rmsOf (seg, 0, seg.size());
            double rel = 0;
            if (i == 0) ref = seg;
            else
            {
                double num = 0, den = 0;
                for (size_t q = 0; q < seg.size() && q < ref.size(); ++q)
                { const double d = seg[q] - ref[q]; num += d*d; den += (double) ref[q]*ref[q]; }
                rel = 100.0 * std::sqrt (num / std::max (1e-30, den));
            }
            std::printf ("   %8.2f %12.0f %12.5f %11.0f %%\n", i / 4.0f, f, a, rel);
        }
    }

    //  ---------------------------------------------------------------- 4 --
    std::printf ("\n4. IS IT CHANGED BY HAVING BEEN PLAYED?\n");
    std::printf ("   Twenty notes into one object, silence into the other, then the\n");
    std::printf ("   SAME note into both. If nothing persists they answer identically.\n\n");
    {
        auto probeAfter = [&] (bool playFirst)
        {
            Rig r (def);
            r.run (0.3);
            if (playFirst)
            {
                const int notes[8] = { 45, 52, 57, 60, 64, 48, 55, 62 };
                for (int i = 0; i < 20; ++i)
                {
                    r.e.noteOn (notes[i % 8], 0.85f);
                    r.run (0.35);
                    r.e.noteOff (notes[i % 8]);
                    r.run (0.15);
                }
            }
            else r.run (10.0);
            r.run (3.0);                    // the same settling for both
            const size_t mark = r.L.size();
            r.e.noteOn (59, 0.9f);
            r.run (5.0);
            std::vector<float> seg (r.L.begin() + (long) mark, r.L.end());
            return seg;
        };
        std::vector<float> fresh = probeAfter (false);
        std::vector<float> used  = probeAfter (true);
        const size_t n = std::min (fresh.size(), used.size());
        double num = 0, den = 0;
        for (size_t q = 0; q < n; ++q)
        { const double d = used[q] - fresh[q]; num += d*d; den += (double) fresh[q]*fresh[q]; }
        std::printf ("   colour of the answer: fresh %.0f Hz, played-in %.0f Hz\n",
                     rmsFreq (fresh, 0, n), rmsFreq (used, 0, n));
        std::printf ("   level of the answer:  fresh %.5f, played-in %.5f\n",
                     rmsOf (fresh, 0, n), rmsOf (used, 0, n));
        std::printf ("   waveform difference:  %.1f %% of the fresh object's own level\n",
                     100.0 * std::sqrt (num / std::max (1e-30, den)));
    }

    return 0;
}
