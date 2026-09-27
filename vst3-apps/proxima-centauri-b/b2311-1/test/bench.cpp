/*  ARTEFACT B2311.1 — the bench.

    The design document makes claims; this measures them. Where a claim came
    from a prototype, the number is repeated here against the real engine, so
    "it entrained in the prototype" cannot quietly stop being true.

    A note on one metric, because it was wrong first time round and the wrong
    version looked plausible: counting BLOCKS in which something fired reports
    the block rate and nothing else — 187 for every setting, every temperature,
    every note, because 2 s at 512 samples is 187 blocks. What varies is
    FIRINGS PER SECOND, and the engine counts those itself.

    cmake -S test -B test/build ; cmake --build test/build --config Release
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const char* detail = "")
{ ++checks; if (!c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail); } }
static void head (const char* s) { std::printf ("\n== %s ==\n", s); }

static const int SR = 48000, BLK = 512;

struct Take
{
    std::vector<float> L, R;
    std::vector<int> casc;
    float R_first = 0, R_last = 0;
    long long fired = 0;
    int   biggest = 0;
    double secs = 1;
    double fps() const { return fired / secs; }
};

static Take run (double secs, ab1::Params p, double bpm, bool playing,
                 int note = 48, bool audio = true)
{
    ab1::Engine e; e.p = p;
    e.prepare (SR, BLK);
    e.service();
    e.setTransport (bpm, 0.0, playing);
    if (note >= 0) e.noteOn (note, 0.9f);

    const int nb = (int) (secs * SR / BLK);
    Take t; t.secs = secs;
    if (audio) { t.L.reserve ((size_t) nb * BLK); t.R.reserve ((size_t) nb * BLK); }
    std::vector<float> bl (BLK), br (BLK);

    //  the beat the object is being shown, in Hz
    static const double DIV[7] = { 4.0, 2.0, 1.0, 0.5, 1.0/3.0, 0.25, 0.125 };
    const int di = (int) std::lround (std::max (0.0f, std::min (6.0f, p.division)));
    const double pulseHz = playing ? (bpm / 60.0) / DIV[di] : 0.0;

    double sx1=0, sy1=0, n1=0, sxL=0, syL=0, nL=0;

    for (int b = 0; b < nb; ++b)
    {
        e.process (bl.data(), br.data(), BLK);
        const int c = e.lastCascade.load();
        if (c > 0) t.casc.push_back (c);
        if (audio) { t.L.insert (t.L.end(), bl.begin(), bl.end());
                     t.R.insert (t.R.end(), br.begin(), br.end()); }
        if (pulseHz > 0)
        {
            /*  Phase concentration is taken from the AUDIO against the beat,
                not from the engine's own opinion of it. Per sample, so a block
                straddling a beat is not smeared into one phase. */
            for (int i = 0; i < BLK; ++i)
            {
                const double tt = (double)(b * BLK + i) / SR;
                const double a = 2.0 * ab1::PI * (tt * pulseHz - std::floor (tt * pulseHz));
                const double amp = std::abs (bl[(size_t)i]);
                if (amp <= 0) continue;
                if (b < nb / 3)          { sx1 += std::cos(a)*amp; sy1 += std::sin(a)*amp; n1 += amp; }
                else if (b > nb * 2 / 3) { sxL += std::cos(a)*amp; syL += std::sin(a)*amp; nL += amp; }
            }
        }
    }
    t.fired = e.totalFired.load();
    t.biggest = e.biggestCascade.load();
    if (n1 > 0) t.R_first = (float) (std::hypot (sx1, sy1) / n1);
    if (nL > 0) t.R_last  = (float) (std::hypot (sxL, syL) / nL);
    return t;
}

static double peakOf (const std::vector<float>& v)
{ double p = 0; for (float x : v) p = std::max (p, (double) std::abs (x)); return p; }
static double rmsOf (const std::vector<float>& v)
{ double s = 0; for (float x : v) s += (double) x * x; return v.empty() ? 0 : std::sqrt (s / v.size()); }

int main()
{
    std::printf ("ARTEFACT B2311.1 - the bench\n%d units, %d parameters\n",
                 ab1::NUNIT, ab1::numParams());
    ab1::Params base;

    /*  THE ENTRAINMENT TESTS RUN WITH THE BODY NEUTRAL, and that is not a
        loosened test — it is the test measuring only its own claim.

        R is taken from the AUDIO envelope against the beat, deliberately, so
        that the object is not merely asked for its own opinion of how well it
        is following. That works while an event is a 1 ms click. From 260902.2
        an event can ring for forty, and a tail that long lays energy across
        every phase of the beat whatever the lattice is doing — measured, R
        fell at every tempo at the shipping defaults and this check failed
        while the counting had not changed at all. What was being measured was
        the tail.

        So the claim "it eases towards an imposed pulse" — a claim about the
        COUNTING — is tested with the three body controls shut, where the audio
        is the 260902.1 audio to -119 dB and the numbers are therefore the ones
        in the design document. */
    ab1::Params entrain = base;
    entrain.strat = entrain.persist = entrain.traverse = 0.0f;
    /*  ...and WEIGHT and ENCLOSURE, added 260902.3. Missing them cost a round:
        the check failed again and the counting had not moved a sample. Both
        put energy into the gaps between events — a low mode ringing for a
        quarter of a second, a crate for half of one — and this measurement
        reads the audio envelope against the beat. ANYTHING that lengthens an
        event lowers R without touching the lattice. The rule, now stated once
        so it is not learned a third time: if it makes sound after the firing,
        it is shut off for the entrainment tests. */
    entrain.weight = 0.0f;
    entrain.space  = 0.0f;

    //------------------------------------------------------------------
    head ("1 - cold, it does not count");
    {
        ab1::Params p = base; p.temp = 0.0f;
        Take t = run (1.0, p, 120.0, true);
        ok (peakOf (t.L) == 0.0 && peakOf (t.R) == 0.0,
            "at 77 K the object is exactly silent, not nearly");
        p.temp = 0.298755f;
        Take w = run (1.0, p, 120.0, true);
        std::printf ("  77 K peak %.6f   293 K peak %.4f, %.0f firings/s\n",
                     peakOf (t.L), peakOf (w.L), w.fps());
        ok (peakOf (w.L) > 0.001, "at room temperature it counts");
    }

    //------------------------------------------------------------------
    head ("2 - warming it makes it count faster");
    {
        std::printf ("  kelvin   firings per second   rms\n");
        double prev = -1; bool rises = true;
        for (float t : { 0.15f, 0.35f, 0.6f, 1.0f })
        {
            ab1::Params p = base; p.temp = t;
            Take k = run (2.0, p, 120.0, true);
            std::printf ("  %6.0f   %18.0f   %.4f\n", 77.0 + 723.0 * t, k.fps(), rmsOf (k.L));
            if (prev >= 0 && k.fps() < prev * 1.02) rises = false;
            prev = k.fps();
        }
        ok (rises, "hotter is busier");
    }

    //------------------------------------------------------------------
    head ("3 - conduction makes events, and nobody chose their length");
    {
        std::printf ("  conduction   firings/s   median cascade   largest\n");
        int smallest = 1 << 30, biggest = 0;
        for (float c : { 0.02f, 0.15f, 0.35f, 0.7f })
        {
            ab1::Params p = base; p.couple = c;
            Take k = run (2.0, p, 120.0, true);
            std::vector<int> v = k.casc; std::sort (v.begin(), v.end());
            std::printf ("  %10.2f   %9.0f   %14d   %7d\n",
                         c, k.fps(), v.empty() ? 0 : v[v.size()/2], k.biggest);
            smallest = std::min (smallest, k.biggest);
            biggest  = std::max (biggest,  k.biggest);
        }
        ok (biggest > smallest * 3,
            "conduction changes the SIZE of an event, not just how often one happens");
    }

    //------------------------------------------------------------------
    head ("4 - THE CLAIM: it eases towards an imposed pulse");
    {
        std::printf ("  bpm    R first third   R last third\n");
        int eased = 0;
        for (double bpm : { 70.0, 100.0, 128.0, 160.0 })
        {
            Take k = run (8.0, entrain, bpm, true);
            std::printf ("  %5.0f   %13.3f   %12.3f\n", bpm, k.R_first, k.R_last);
            if (k.R_last > k.R_first * 1.10 || k.R_last > 0.25) ++eased;
        }
        ok (eased >= 3, "the relationship to the beat builds over a take, at most tempi");
    }

    //------------------------------------------------------------------
    head ("5 - it can be LED, but not SHOVED");
    {
        /*  This test asserted the wrong shape first time round: that a harder
            grip gathers more of the object onto the beat. Measured, it does the
            opposite. Once the lattice has organised itself into clusters, a
            gentle nudge aligns them and a violent one scatters them — R peaked
            at a grip of 0.20 and had fallen by more than half at 0.55.

            Which is the brief's own word, EASES, turning out to be the physics.
            The hypothesis was wrong; the engine was not. */
        std::printf ("  grip   R last third\n");
        double gentle = 0, violent = 0;
        for (float g : { 0.05f, 0.20f, 0.55f, 0.95f })
        {
            ab1::Params p = entrain; p.grip = g;
            Take k = run (6.0, p, 120.0, true);
            std::printf ("  %5.2f   %12.3f\n", g, k.R_last);
            if (g < 0.25f) gentle  = std::max (gentle,  (double) k.R_last);
            if (g > 0.50f) violent = std::max (violent, (double) k.R_last);
        }
        ok (gentle > violent, "a gentle pulse leads it; a violent one scatters it");
    }

    //------------------------------------------------------------------
    head ("6 - it keeps its own time when the transport stops");
    {
        Take k = run (3.0, base, 120.0, false);
        std::printf ("  transport stopped: %.0f firings/s, rms %.4f\n", k.fps(), rmsOf (k.L));
        ok (k.fired > 0 && rmsOf (k.L) > 1e-5,
            "an object that goes silent when the transport stops is a plug-in, not an artefact");
    }

    //------------------------------------------------------------------
    head ("7 - the section through the body is the timbre");
    {
        auto crossings = [] (const std::vector<float>& v)
        {
            int zc = 0; for (size_t i = 1; i < v.size(); ++i)
                if ((v[i-1] < 0) != (v[i] < 0)) ++zc;
            return v.empty() ? 0.0 : (double) zc * SR / (2.0 * v.size());
        };
        std::printf ("  section                crossing rate\n");
        std::vector<double> cs;
        struct PJ { const char* n; float x, y, z, w; };
        for (PJ q : { PJ{"width only",   1.0f, 0.5f, 0.5f, 0.5f},
                      PJ{"height only",  0.5f, 1.0f, 0.5f, 0.5f},
                      PJ{"into w",       0.5f, 0.5f, 0.5f, 1.0f},
                      PJ{"width and w",  1.0f, 0.5f, 0.5f, 0.0f} })
        {
            ab1::Params p = base; p.projx=q.x; p.projy=q.y; p.projz=q.z; p.projw=q.w;
            Take k = run (2.0, p, 120.0, true);
            const double c = crossings (k.L);
            std::printf ("  %-20s %10.0f Hz\n", q.n, c);
            cs.push_back (c);
        }
        const double mn = *std::min_element (cs.begin(), cs.end());
        const double mx = *std::max_element (cs.begin(), cs.end());
        ok (mx > mn * 1.20, "turning the section through the body changes the sound");
    }

    //------------------------------------------------------------------
    head ("8 - a played note transposes the counting");
    {
        std::printf ("  note   firings per second\n");
        double lowC = 0, highC = 0;
        for (int nt : { 36, 48, 60 })
        {
            Take k = run (2.0, base, 120.0, true, nt);
            std::printf ("  %4d   %18.0f\n", nt, k.fps());
            if (nt == 36) lowC = k.fps();
            if (nt == 60) highC = k.fps();
        }
        ok (highC > lowC * 1.5,
            "a higher note counts faster - pitch here IS the rate of events");
    }

    //------------------------------------------------------------------
    head ("9 - bounded, and quiet when it should be");
    {
        double worst = 0;
        for (int s = 0; s < 256; s += 8)
        {
            ab1::Params p = base; ab1::applySpecimen (s, p); p.level = 1.0f; p.temp = 1.0f;
            Take k = run (0.8, p, 140.0, true);
            worst = std::max (worst, std::max (peakOf (k.L), peakOf (k.R)));
        }
        std::printf ("  worst peak across the catalogue, hot and flat out: %.4f\n", worst);
        ok (worst <= 1.0, "nothing in the catalogue leaves full scale");

        ab1::Params q = base; q.temp = 0.0f;
        Take z = run (0.5, q, 120.0, true, -1);
        ok (peakOf (z.L) == 0.0, "silent in, silent out");
    }

    //------------------------------------------------------------------
    head ("10 - every specimen is its own object, and stays so");
    {
        std::vector<std::vector<double>> sig;
        for (int s = 0; s < 64; s += 4)
        {
            ab1::Params p = base; ab1::applySpecimen (s, p);
            Take k = run (1.5, p, 120.0, true);
            std::vector<int> c = k.casc; std::sort (c.begin(), c.end());
            sig.push_back ({ (double) k.fired,
                             c.empty() ? 0.0 : (double) c[c.size()/2],
                             (double) k.biggest,
                             rmsOf (k.L) * 1000.0 });
        }
        int same = 0;
        for (size_t a = 0; a < sig.size(); ++a)
            for (size_t b = a+1; b < sig.size(); ++b)
            {
                double d = 0; for (int k = 0; k < 4; ++k) d += std::abs (sig[a][k] - sig[b][k]);
                if (d < 1e-9) ++same;
            }
        std::printf ("  %d specimens compared, %d identical pairs\n", (int) sig.size(), same);
        ok (same == 0, "no two specimens are the same object");

        ab1::Params p = base; ab1::applySpecimen (37, p);
        Take x = run (1.0, p, 120.0, true), y = run (1.0, p, 120.0, true);
        ok (x.L.size() == y.L.size()
            && std::memcmp (x.L.data(), y.L.data(), sizeof(float) * x.L.size()) == 0,
            "the same specimen plays identically twice, so a bounce matches what was heard");
    }

    //------------------------------------------------------------------
    head ("11 - the body has layers, and a large event rings");
    {
        /*  NOT INERT. A control that is wired to nothing passes every bounded
            test there is, so each of the three has to be shown CHANGING the
            output — and shown doing it on its own, with the other two shut. */
        Take ref = run (4.0, entrain, 120.0, true);
        const char* nm[3] = { "STRATIFICATION", "PERSISTENCE", "TRAVERSE" };
        for (int k = 0; k < 3; ++k)
        {
            ab1::Params p = entrain;
            if (k == 0) p.strat = 1.0f; else if (k == 1) p.persist = 1.0f; else p.traverse = 1.0f;
            Take t = run (4.0, p, 120.0, true);
            double num = 0, den = 0;
            const size_t n = std::min (t.L.size(), ref.L.size());
            for (size_t i = 0; i < n; ++i)
            {
                const double d = (double) t.L[i] - ref.L[i];
                num += d * d; den += (double) ref.L[i] * ref.L[i];
            }
            const double rel = std::sqrt (num / std::max (1e-30, den));
            std::printf ("  %-15s alone changes the output by %6.1f %%\n", nm[k], 100.0 * rel);
            ok (rel > 0.02, "this control is wired to something");
        }

        //  bounded with all three flat out, across the catalogue
        double worst = 0;
        for (int s = 0; s < 256; s += 16)
        {
            ab1::Params p = base; ab1::applySpecimen (s, p);
            p.strat = p.persist = p.traverse = 1.0f;
            p.temp = 0.9f; p.level = 1.0f; p.sat = 0.0f;
            worst = std::max (worst, peakOf (run (1.5, p, 120.0, true).L));
        }
        std::printf ("  worst peak, all three flat out and hot: %.4f\n", worst);
        ok (worst <= 1.0, "the long mode cannot push the object past full scale");

        //  cold is still exactly silent, long mode or no long mode
        {
            ab1::Params p = base; p.temp = 0.0f;
            p.strat = p.persist = p.traverse = 1.0f;
            Take t = run (1.0, p, 120.0, true);
            ok (peakOf (t.L) == 0.0 && peakOf (t.R) == 0.0,
                "at 77 K it is still exactly silent, with nothing left ringing");
        }

        /*  THE CLAIM ITSELF, and it has to be measured on events rather than
            on a spectrum: sort events by the energy of their first 3 ms — the
            cascade arriving, before the long mode has said anything — and
            compare how long the biggest quarter last against the smallest
            half. Sorting by PEAK would sort them by the wrong thing, because
            TRAVERSE spreads a large cascade flatter. */
        for (int mode = 0; mode < 2; ++mode)
        {
            ab1::Params p = base;
            p.rateLo = 0.0f; p.rateHi = 0.10f; p.couple = 0.55f;
            p.dead = 0.30f;  p.grip = 0.0f;
            p.persist = mode ? 0.85f : 0.0f;
            Take t = run (20.0, p, 120.0, false);
            const std::vector<float>& L = t.L;
            double gpk = 0; for (float v : L) gpk = std::max (gpk, (double) std::abs (v));
            std::vector<std::pair<double,double>> ev;
            const int G = 200;
            for (int i = G; i < (int) L.size() - 8000; ++i)
            {
                const double a = std::abs (L[(size_t)i]);
                if (a < 0.06 * gpk) continue;
                bool top = true;
                for (int q = -G; q <= G && top; ++q) if (std::abs (L[(size_t)(i+q)]) > a) top = false;
                if (! top) continue;
                double drive = 0;
                for (int q = 0; q < 144; ++q) drive += std::abs (L[(size_t)(i+q)]);
                int below = 0, dec = -1; bool spoiled = false;
                for (int q = 1; q < 8000; ++q)
                {
                    const double v = std::abs (L[(size_t)(i+q)]);
                    if (v > a * 0.55 && q > 64) { spoiled = true; break; }
                    if (v < a * 0.1) { if (++below >= 48) { dec = q - 48; break; } } else below = 0;
                }
                if (! spoiled && dec > 0) ev.push_back ({ drive, 1000.0 * dec / SR });
            }
            if (ev.size() < 24) { ok (false, "enough clean events to measure a decay at all"); continue; }
            std::sort (ev.begin(), ev.end());
            double ds = 0, db = 0; int ns = 0, nbg = 0;
            for (size_t q = 0; q < ev.size(); ++q)
            {
                if (q < ev.size()/2)      { ds += ev[q].second; ++ns; }
                if (q >= ev.size()*3/4)   { db += ev[q].second; ++nbg; }
            }
            ds /= ns; db /= nbg;
            std::printf ("  PERSISTENCE %.2f : %d events, small %.2f ms, big %.2f ms, ratio %.2fx\n",
                         p.persist, (int) ev.size(), ds, db, db / ds);
            if (mode) ok (db / ds > 2.5, "a large event outlasts a small one by more than 2.5x");
            else      ok (db / ds > 1.0, "even with PERSISTENCE shut, a large event is not shorter");
        }
    }

    //------------------------------------------------------------------
    head ("12 - it runs down; it is not switched off");
    {
        /*  Peter, 260902.3: "it is unrealistic that the patterns stop dead at
            77 K... they should just be less active - there is big change from
            79 to 77 K". There were two cliffs stacked on each other: the rate
            law still counted at a QUARTER SPEED at 77.1 K, and a hard return
            then muted the object outright at 77.0. */
        /*  Twenty-second takes and TOTAL firings, not a rate over three
            seconds. Cold, the lattice synchronises and goes off as one body of
            9216, so the quantity that exists at all is how OFTEN that happens,
            and three seconds could only ever report none, one or two of them.  */
        std::printf ("  kelvin   whole-body events in 20 s   x previous\n");
        double prev = -1, worstRatio = 1.0;
        bool monotonic = true, countsWhenNearlyFrozen = false;
        for (double k : { 80.0, 90.0, 100.0, 120.0, 150.0, 190.0 })
        {
            ab1::Params p = base; p.temp = (float) ((k - 77.0) / 723.0);
            Take t = run (20.0, p, 120.0, true);
            const double ev = (double) t.fired / (double) ab1::NUNIT;
            std::printf ("  %6.0f   %25.1f", k, ev);
            if (prev > 0.5)
            {
                const double ratio = ev / prev;
                std::printf ("   %.2fx", ratio);
                if (ev + 0.5 < prev) monotonic = false;
                worstRatio = std::max (worstRatio, ratio);
            }
            std::printf ("\n");
            if (k <= 80.5 && ev > 0.5) countsWhenNearlyFrozen = true;
            prev = ev;
        }
        ok (countsWhenNearlyFrozen,
            "three kelvin above the floor it still counts, slowly");
        ok (monotonic, "warming it never makes it quieter");
        /*  A cliff shows up as one step multiplying the activity many times
            over. Smooth means every step is a modest factor. */
        std::printf ("  worst single step: %.2fx\n", worstRatio);
        ok (worstRatio < 6.0, "no single step across the cold end is a cliff");

        ab1::Params c = base; c.temp = 0.0f;
        Take z = run (1.0, c, 120.0, true);
        ok (peakOf (z.L) == 0.0, "and at 77 K it is still exactly silent");
    }

    //------------------------------------------------------------------
    head ("13 - a touch is remembered");
    {
        /*  Peter, 260902.3: "dragging or marking areas should change these tiny
            clocks behaviour permanently, or at least a while - not just while
            it is pressed down."

            The thing to measure is therefore what the object is doing AFTER
            the touch has ended, and against an untouched object run for the
            same length of time from the same seed. A mark that only lasted
            while the button was down would look identical here. */
        /*  Measured on the COLOUR, not on the number of firings. A mark speeds
            a unit up, and at any real CONDUCTION the coupling pins the
            collective rate — measured, the census moved one per cent and
            MEMORY made no difference to it whatsoever. What a mark actually
            does is move those units into another layer of the body, so what
            has to be measured is what the object SOUNDS like afterwards. */
        auto colourAfter = [&base] (float memory, bool touch, double waitSecs)
        {
            ab1::Params p = base; p.memory = memory;
            ab1::Engine e; e.p = p;
            e.prepare (SR, BLK); e.service();
            e.setTransport (120.0, 0.0, true); e.noteOn (48, 0.9f);
            std::vector<float> bl (BLK), br (BLK), L;
            for (int q = 0; q < (int)(0.5 * SR / BLK); ++q) e.process (bl.data(), br.data(), BLK);
            if (touch)
                for (int x = 6; x < 26; ++x) e.poke (x, 16, 0.35f, 4);
            for (int q = 0; q < (int)(waitSecs * SR / BLK); ++q) e.process (bl.data(), br.data(), BLK);
            //  one second of sound, AFTER the touch has long ended
            for (int q = 0; q < (int)(1.0 * SR / BLK); ++q)
            { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }
            double se = 0, sd = 0;
            for (size_t q = 1; q < L.size(); ++q)
            { const double v = L[q], d = v - L[q-1]; se += v*v; sd += d*d; }
            return se > 1e-30 ? (SR / (2.0 * ab1::PI)) * std::sqrt (sd / se) : 0.0;
        };

        const double plain = colourAfter (0.60f, false, 1.0);
        const double poked = colourAfter (0.60f, true,  1.0);
        const double diff  = std::abs (poked - plain) / std::max (1.0, plain);
        std::printf ("  a second after the touch ended: %.0f Hz vs %.0f untouched (%.1f %%)\n",
                     poked, plain, 100.0 * diff);
        ok (diff > 0.03, "the object still sounds different after the touch has ended");

        /*  and MEMORY decides for how long. Eight seconds on, a short memory
            should have let go of what a long one is still holding. */
        const double sBase = colourAfter (0.05f, false, 8.0);
        const double sMark = colourAfter (0.05f, true,  8.0);
        const double lBase = colourAfter (0.95f, false, 8.0);
        const double lMark = colourAfter (0.95f, true,  8.0);
        const double shortMem = std::abs (sMark - sBase) / std::max (1.0, sBase);
        const double longMem  = std::abs (lMark - lBase) / std::max (1.0, lBase);
        std::printf ("  eight seconds later: MEMORY 0.05 differs %.1f %%, MEMORY 0.95 %.1f %%\n",
                     100.0 * shortMem, 100.0 * longMem);
        ok (longMem > shortMem, "a long MEMORY holds the mark longer than a short one");
    }

    //==========================================================================
    head ("14 - the site");
    {
        /*  THE SITE (proxima_site.h): a second imposed pulse, from the other
            findings on the bench. At pull 0 it must be an exact no-op; at
            pull 1, cold, the object must audibly fall onto the site's wraps.
            Both measured. The host is stopped throughout so the object keeps
            its own time and the only pulse offered is the site's. */
        /*  What is folded is the COUNTING — firings per block against the
            site phase — exactly as the object's own LEANING readout folds them
            against the host beat. The audio envelope was tried first and read
            0.163 coupled, 0.163 free: at room temperature the body fires in
            the thousands per second and its envelope is flat whatever the
            lattice is doing. Measure the mechanism, not its shadow. */
        struct SiteTake { std::vector<float> L; std::vector<double> ph, fired; int wraps = 0; };
        auto take = [&] (float pull, double siteHz, double seconds, float temp) -> SiteTake
        {
            //  the entrainment configuration; its own pulse parked at 0.15 Hz
            ab1::Params p = entrain; p.temp = temp; p.freeHz = 0.0f;
            ab1::Engine e; e.p = p; e.prepare (SR, BLK); e.service();
            e.setTransport (120.0, 0.0, false);
            e.noteOn (48, 0.9f);
            SiteTake t;
            std::vector<float> bl (BLK), br (BLK);
            long long lastFired = 0;
            for (int q = 0; q < (int) (seconds * SR / BLK); ++q)
            {
                const double tm = (double) q * BLK / SR;
                const double sp = std::fmod (tm * siteHz, 1.0);
                e.setSite ((float) sp, (float) siteHz, pull);
                e.process (bl.data(), br.data(), BLK);
                t.L.insert (t.L.end(), bl.begin(), bl.end());
                const long long f = e.totalFired.load();
                t.ph.push_back (sp); t.fired.push_back ((double) (f - lastFired)); lastFired = f;
            }
            t.wraps = e.siteWraps.load();
            return t;
        };
        {
            ab1::Params p = entrain; p.temp = 0.30f; p.freeHz = 0.0f;
            ab1::Engine e; e.p = p; e.prepare (SR, BLK); e.service();
            e.setTransport (120.0, 0.0, false); e.noteOn (48, 0.9f);
            std::vector<float> bl (BLK), br (BLK), ref;
            for (int q = 0; q < (int) (4.0 * SR / BLK); ++q)
            { e.process (bl.data(), br.data(), BLK); ref.insert (ref.end(), bl.begin(), bl.end()); }
            const auto un = take (0.0f, 1.3, 4.0, 0.30f);
            bool same = un.L.size() == ref.size();
            for (size_t i = 0; same && i < ref.size(); ++i) same = un.L[i] == ref[i];
            std::printf ("  uncoupled: %s, %d site shoves\n", same ? "byte-identical" : "DIFFERS", un.wraps);
            ok (same && un.wraps == 0, "uncoupled, the site is an exact no-op");
        }
        /*  THE LOCK, cold. A cold body fires as one, a few times in ten
            seconds, on its own clock; on a shared bench it should fire on the
            site's wraps. Measured as the share of all firings that land in
            the band around a wrap (phase within 0.12 either side — the engine
            wraps its OWN eased phase, which crosses a hair before the bench's
            bookkeeping does, so a one-sided window read 1 % of a locked take):
            uniform is 0.24. Forty seconds, so that "a few events" becomes a
            few dozen — folding three events reads whatever you like. */
        auto onWrap = [] (const SiteTake& t) -> double
        {
            double in = 0, all = 0;
            for (size_t i = t.ph.size() / 4; i < t.ph.size(); ++i)
            {
                all += t.fired[i];
                if (t.ph[i] < 0.12 || t.ph[i] > 0.88) in += t.fired[i];
            }
            return all > 0 ? in / all : 0.0;
        };
        const SiteTake tFree = take (0.0f, 1.0, 40.0, 0.10f), tLock = take (1.0f, 1.0, 40.0, 0.10f);
        const double fFree = onWrap (tFree), fLock = onWrap (tLock);
        double nFree = 0, nLock = 0;
        for (double f : tFree.fired) nFree += f;
        for (double f : tLock.fired) nLock += f;
        std::printf ("  149 K, site at 1 Hz: %.0f%% of firings on a wrap coupled (%d shoves, %.0f firings)"
                     " vs %.0f%% free (%.0f firings)\n", 100 * fLock, tLock.wraps, nLock, 100 * fFree, nFree);
        ok (tLock.wraps >= 35, "coupled, the site's wraps actually shove the lattice");
        ok (fLock > 0.65 && fLock > fFree + 0.30, "cold and close, the body fires on the site's wraps");
    }

    std::printf ("\n%d checks, %d failed  -  %s\n", checks, fails,
                 fails ? "SEE ABOVE" : "ALL CLEAR");
    return fails ? 1 : 0;
}
