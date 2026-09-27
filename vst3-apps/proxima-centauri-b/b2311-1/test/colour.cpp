/*  ARTEFACT B2311.1 — where does the colour and the length of an event come
    from, and how much can either of them vary?

    Peter, 2026-09-02: more variety in lengths and spectral weight, without
    pasting oscillators on. Before proposing anything, measure what the object
    can actually do now.

        ab1colour
*/
#include "../Source/Engine.h"
#include "legacy/EngineLegacy.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <ctime>
#include <xmmintrin.h>
#include <pmmintrin.h>

static const int SR = 48000, BLK = 256;

//  the engine's own mapping, copied so the probe measures the shipped law
static double xmapd (double v, double lo, double hi)
{ double c = v < 0 ? 0 : (v > 1 ? 1 : v); return lo * std::pow (hi / lo, c); }

/*  RMS frequency: (fs/2pi) * RMS(dx/dt) / RMS(x). Exact for the second moment
    of the power spectrum, O(n), no FFT, and it is the number that answers
    "where does this sit spectrally". */
static double rmsFreq (const std::vector<float>& x, int a, int b)
{
    double se = 0, sd = 0;
    for (int i = a + 1; i < b; ++i)
    {
        const double v = x[(size_t)i], d = v - x[(size_t)i-1];
        se += v * v; sd += d * d;
    }
    if (se < 1e-30) return 0.0;
    return (SR / (2.0 * ab1::PI)) * std::sqrt (sd / se);
}

static double rms (const std::vector<float>& x, int a, int b)
{
    double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t)i] * x[(size_t)i];
    return std::sqrt (s / std::max (1, b - a));
}

/*  THE KERNEL. Every firing anywhere in the lattice is a single sample spike
    put through this and nothing else, so this pair of one-poles is the entire
    body of the instrument. */
struct Kernel { double t20ms, t60ms, fcHz, peak; };

static Kernel measureKernel (float shape, float damp)
{
    const double a1 = xmapd (shape, 0.60, 0.03);
    const double a2 = xmapd (damp,  0.45, 0.015);
    std::vector<float> h ((size_t) SR / 2, 0.0f);          // half a second is plenty
    double z1 = 0, z2 = 0;
    for (size_t i = 0; i < h.size(); ++i)
    {
        const double in = (i == 0) ? 1.0 : 0.0;
        z1 += a1 * (in - z1);
        z2 += a2 * (z1 - z2);
        h[i] = (float) (z1 - z2);
    }
    double pk = 0; int pkAt = 0;
    for (size_t i = 0; i < h.size(); ++i) if (std::abs (h[i]) > pk) { pk = std::abs (h[i]); pkAt = (int) i; }
    int i20 = pkAt, i60 = pkAt;
    for (size_t i = (size_t) pkAt; i < h.size(); ++i) { if (std::abs (h[i]) > pk * 0.1)   i20 = (int) i; }
    for (size_t i = (size_t) pkAt; i < h.size(); ++i) { if (std::abs (h[i]) > pk * 0.001) i60 = (int) i; }
    Kernel k;
    k.t20ms = 1000.0 * (i20 - pkAt) / SR;
    k.t60ms = 1000.0 * (i60 - pkAt) / SR;
    k.fcHz  = rmsFreq (h, 0, (int) h.size());
    k.peak  = pk;
    return k;
}

//==============================================================================
struct Take { double rmsCV, fcMean, fcCV, fcLo, fcHi, firePerS, biggest, peak; };

static Take renderTake (const ab1::Params& p, double secs, bool playing)
{
    ab1::Engine e; e.p = p;
    e.prepare (SR, BLK); e.service();
    e.setTransport (120.0, 0.0, playing);
    e.noteOn (48, 0.9f);

    const int nb = (int) (secs * SR / BLK);
    std::vector<float> L; L.reserve ((size_t) nb * BLK);
    std::vector<float> bl (BLK), br (BLK);
    for (int b = 0; b < nb; ++b)
    {
        e.process (bl.data(), br.data(), BLK);
        L.insert (L.end(), bl.begin(), bl.end());
    }

    //  windows of 2048 samples (43 ms) across the take, skipping the first second
    const int W = 2048, start = SR;
    std::vector<double> rr, ff;
    for (int a = start; a + W < (int) L.size(); a += W)
    {
        const double r = rms (L, a, a + W);
        if (r < 1e-7) continue;                 // a silent window has no colour
        rr.push_back (r);
        ff.push_back (rmsFreq (L, a, a + W));
    }
    Take t {};
    auto stats = [] (const std::vector<double>& v, double& mean, double& cv, double& lo, double& hi)
    {
        mean = 0; lo = 1e18; hi = -1e18;
        for (double x : v) { mean += x; lo = std::min (lo, x); hi = std::max (hi, x); }
        mean /= std::max<size_t> (1, v.size());
        double s = 0; for (double x : v) s += (x - mean) * (x - mean);
        s = std::sqrt (s / std::max<size_t> (1, v.size()));
        cv = mean > 1e-12 ? s / mean : 0.0;
    };
    double m, c, lo, hi;
    stats (rr, m, c, lo, hi); t.rmsCV = c;
    stats (ff, m, c, lo, hi); t.fcMean = m; t.fcCV = c; t.fcLo = lo; t.fcHi = hi;
    t.firePerS = (double) e.totalFired.load() / secs;
    t.biggest  = (double) e.biggestCascade.load();
    double pk = 0; for (float v : L) pk = std::max (pk, (double) std::abs (v));
    t.peak = pk;
    return t;
}

//==============================================================================
int main()
{
    std::printf ("ARTEFACT B2311.1 — the body, measured\n\n");

    ab1::Params def;

    std::printf ("1. THE KERNEL — WHAT ONE FIRING GOT AT 260902.1\n");
    std::printf ("   Two one-poles, difference taken. ONE instance, shared by all %d\n", ab1::NUNIT);
    std::printf ("   units — which is the whole diagnosis. From 260902.2 this pair is the\n");
    std::printf ("   CENTRE of a bank of %d, and section 4c is its gamut.\n\n", ab1::NBAND);
    Kernel k0 = measureKernel (def.shape, def.damp);
    std::printf ("   at the defaults (AFTERSOUND %.2f, ABSORPTION %.2f):\n", def.shape, def.damp);
    std::printf ("     -20 dB in %6.3f ms      -60 dB in %6.3f ms      centre %7.0f Hz\n\n", k0.t20ms, k0.t60ms, k0.fcHz);

    std::printf ("   the whole gamut those two knobs can reach:\n");
    std::printf ("     %-10s %-10s %10s %10s %10s\n", "AFTERSND", "ABSORP", "-20dB ms", "-60dB ms", "centre Hz");
    double t20lo = 1e18, t20hi = -1e18, fclo = 1e18, fchi = -1e18;
    for (int i = 0; i <= 4; ++i)
        for (int j = 0; j <= 4; ++j)
        {
            const float s = i / 4.0f, d = j / 4.0f;
            Kernel k = measureKernel (s, d);
            t20lo = std::min (t20lo, k.t20ms); t20hi = std::max (t20hi, k.t20ms);
            fclo  = std::min (fclo,  k.fcHz);  fchi  = std::max (fchi,  k.fcHz);
            if ((i == 0 || i == 2 || i == 4) && (j == 0 || j == 2 || j == 4))
                std::printf ("     %-10.2f %-10.2f %10.3f %10.3f %10.0f\n", s, d, k.t20ms, k.t60ms, k.fcHz);
        }
    std::printf ("\n   RANGE: length %.3f .. %.3f ms   centre %.0f .. %.0f Hz\n", t20lo, t20hi, fclo, fchi);
    std::printf ("   Nothing this object made had a body longer than ten milliseconds,\n");
    std::printf ("   and it was ONE setting per patch — so within a take every event was\n");
    std::printf ("   this length and this colour, whatever the lattice did.\n\n");

    std::printf ("2. A TAKE AT THE DEFAULTS, 20 s, transport rolling (260902.2 as shipped)\n");
    std::printf ("   43 ms windows: does the density move? does the colour move?\n\n");
    Take t = renderTake (def, 20.0, true);
    std::printf ("     firings/s %8.0f      biggest cascade %5.0f      peak %.3f\n", t.firePerS, t.biggest, t.peak);
    std::printf ("     loudness  CV %6.1f %%   (window to window)\n", 100.0 * t.rmsCV);
    std::printf ("     colour    CV %6.1f %%   mean %.0f Hz, range %.0f .. %.0f Hz\n\n",
                 100.0 * t.fcCV, t.fcMean, t.fcLo, t.fcHi);

    std::printf ("3. ACROSS THE CATALOGUE — eight specimens, 12 s each (260902.2)\n");
    std::printf ("   %-6s %10s %10s %10s %10s %10s\n", "spec", "fire/s", "biggest", "colour Hz", "colour CV", "loud CV");
    double cmin = 1e18, cmax = -1e18, cvsum = 0; int nspec = 0;
    for (int s = 0; s < 256; s += 32)
    {
        ab1::Params p; ab1::applySpecimen (s, p);
        Take ts = renderTake (p, 12.0, true);
        std::printf ("   %-6d %10.0f %10.0f %10.0f %9.1f %% %9.1f %%\n",
                     s, ts.firePerS, ts.biggest, ts.fcMean, 100.0 * ts.fcCV, 100.0 * ts.rmsCV);
        cmin = std::min (cmin, ts.fcMean); cmax = std::max (cmax, ts.fcMean);
        cvsum += ts.fcCV; ++nspec;
    }
    std::printf ("\n   between specimens the colour spans %.0f .. %.0f Hz\n", cmin, cmax);
    std::printf ("   but WITHIN a specimen it wanders only %.1f %% on average.\n", 100.0 * cvsum / nspec);

    //==========================================================================
    std::printf ("\n4. THE THREE NEW CONTROLS\n");

    ab1::Params legacy = def;
    legacy.strat = legacy.persist = legacy.traverse = 0.0f;

    std::printf ("\n   a) all three at zero — is it still the instrument that shipped?\n");
    std::printf ("      Against the REAL 260902.1 engine, `git show HEAD:` compiled in beside\n");
    std::printf ("      this one. Comparing the new engine with itself would pass whatever\n");
    std::printf ("      the bank did, and prove only that it is deterministic.\n");
    {
        ab1::Engine e1;
        ab1legacy::Engine e0;
        e1.p = legacy;
        //  the legacy Params has no strat/persist/traverse; everything else matches
        e0.p.level = legacy.level;
        e0.p.rateLo = legacy.rateLo; e0.p.rateHi = legacy.rateHi;
        e0.p.couple = legacy.couple; e0.p.dead = legacy.dead; e0.p.leak = legacy.leak;
        e0.p.grip = legacy.grip; e0.p.reach = legacy.reach;
        e0.p.division = legacy.division; e0.p.freeHz = legacy.freeHz;
        e0.p.projx = legacy.projx; e0.p.projy = legacy.projy;
        e0.p.projz = legacy.projz; e0.p.projw = legacy.projw;
        e0.p.spanx = legacy.spanx; e0.p.tilt = legacy.tilt;
        e0.p.temp = legacy.temp; e0.p.shape = legacy.shape; e0.p.damp = legacy.damp;
        e0.p.space = legacy.space; e0.p.sat = legacy.sat;

        e1.prepare (SR, BLK); e1.service(); e1.setTransport (120, 0, true); e1.noteOn (48, 0.9f);
        e0.prepare (SR, BLK); e0.service(); e0.setTransport (120, 0, true); e0.noteOn (48, 0.9f);
        double worst = 0, pk = 0, sd = 0, sn = 0;
        std::vector<float> l1 (BLK), r1 (BLK), l0 (BLK), r0 (BLK);
        std::vector<float> keepOld, keepNew;
        for (int b = 0; b < 8 * SR / BLK; ++b)
        {
            e1.process (l1.data(), r1.data(), BLK);
            e0.process (l0.data(), r0.data(), BLK);
            keepOld.insert (keepOld.end(), l0.begin(), l0.end());
            keepNew.insert (keepNew.end(), l1.begin(), l1.end());
            for (int i = 0; i < BLK; ++i)
            {
                const double d = (double) l1[(size_t)i] - l0[(size_t)i];
                worst = std::max (worst, std::abs (d));
                sd += d * d; sn += (double) l0[(size_t)i] * l0[(size_t)i];
                pk = std::max (pk, (double) std::abs (l0[(size_t)i]));
            }
        }
        std::printf ("      8 s: worst sample difference %.3g on a peak of %.3f\n", worst, pk);
        std::printf ("      residual %.1f dB below the signal\n", 10.0 * std::log10 ((sd + 1e-30) / (sn + 1e-30)));
        /*  WHERE the difference lives decides whether it is a changed
            instrument or a ceiling doing its job. Split the samples by how
            loud the old engine was at that instant. */
        std::printf ("      %-22s %12s %12s\n", "old level band", "share of", "residual dB");
        const double edges[4] = { 0.10, 0.35, 0.70, 1.01 };
        double lastE = 0.0;
        for (int q = 0; q < 4; ++q)
        {
            double d2 = 0, s2 = 0, cnt = 0;
            for (size_t i = 0; i < keepOld.size(); ++i)
            {
                const double a = std::abs (keepOld[i]) / pk;
                if (a < lastE || a >= edges[q]) continue;
                const double d = (double) keepNew[i] - keepOld[i];
                d2 += d * d; s2 += (double) keepOld[i] * keepOld[i]; ++cnt;
            }
            std::printf ("      %5.2f .. %-14.2f %10.1f %% %11.1f\n", lastE, edges[q],
                         100.0 * cnt / std::max<size_t> (1, keepOld.size()),
                         10.0 * std::log10 ((d2 + 1e-30) / (s2 + 1e-30)));
            lastE = edges[q];
        }
    }

    std::printf ("\n   b) event LENGTH, measured on the events themselves\n");
    std::printf ("      A slow, sparse setting so events stand apart far enough to have a\n");
    std::printf ("      measurable decay at all — at the busy defaults the next cascade\n");
    std::printf ("      arrives inside the last one's tail and no decay is observable.\n");
    std::printf ("      Any event whose tail is interrupted is DISCARDED, not measured.\n\n");
    {
        std::printf ("      %-12s %8s %10s %12s %12s %10s\n",
                     "PERSISTENCE", "events", "gap ms", "small ev ms", "big ev ms", "ratio");
        for (int mode = 0; mode < 3; ++mode)
        {
            ab1::Params q = def;
            q.rateLo = 0.0f; q.rateHi = 0.10f;      // 0.25 .. ~1 Hz: a sparse object
            q.couple = 0.55f; q.dead = 0.30f;
            q.grip = 0.0f;                          // nothing imposed, so nothing regular
            q.persist = mode == 0 ? 0.0f : (mode == 1 ? 0.45f : 0.85f);

            ab1::Engine e; e.p = q;
            e.prepare (SR, BLK); e.service();
            e.setTransport (120.0, 0.0, false);
            e.noteOn (48, 0.9f);
            std::vector<float> L; std::vector<float> bl (BLK), br (BLK);
            for (int b = 0; b < 40 * SR / BLK; ++b)
            { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }

            double gpk = 0; for (float v : L) gpk = std::max (gpk, (double) std::abs (v));
            if (gpk < 1e-6) { std::printf ("      %-12.2f  (silent)\n", q.persist); continue; }

            //  local maxima of |L|, well clear of the floor
            const int GUARD = 200;
            std::vector<std::pair<double,double>> ev;   // peak, decay ms
            std::vector<int> peakAt;
            for (int i = GUARD; i < (int) L.size() - 8000; ++i)
            {
                const double a = std::abs (L[(size_t)i]);
                if (a < 0.06 * gpk) continue;
                bool top = true;
                for (int k = -GUARD; k <= GUARD && top; ++k)
                    if (std::abs (L[(size_t)(i + k)]) > a) top = false;
                if (top) peakAt.push_back (i);
            }
            double gaps = 0;
            for (size_t k = 1; k < peakAt.size(); ++k) gaps += peakAt[k] - peakAt[k-1];
            const double meanGap = peakAt.size() > 1 ? 1000.0 * gaps / (peakAt.size()-1) / SR : 0.0;

            for (int i : peakAt)
            {
                const double pk2 = std::abs (L[(size_t)i]);
                /*  Sort events by their DRIVE — the energy in the first 3 ms,
                    which is the cascade arriving — not by their peak. TRAVERSE
                    spreads a large cascade over more of the step, so a big
                    event can have a LOWER peak than a small tight one, and
                    splitting on the peak sorts the events by the wrong thing.
                    3 ms also ends before the long mode has said anything, so
                    the cause is measured clear of the effect. */
                double drive = 0;
                for (int k = 0; k < 144 && i + k < (int) L.size(); ++k)
                    drive += std::abs (L[(size_t)(i + k)]);
                int below = 0, dec = -1; bool spoiled = false;
                for (int k = 1; k < 8000 && i + k < (int) L.size(); ++k)
                {
                    const double a = std::abs (L[(size_t)(i + k)]);
                    if (a > pk2 * 0.55 && k > 64) { spoiled = true; break; }   // another event
                    if (a < pk2 * 0.1) { if (++below >= 48) { dec = k - 48; break; } }
                    else below = 0;
                }
                if (! spoiled && dec > 0) ev.push_back ({ drive, 1000.0 * dec / SR });
            }
            if (ev.size() < 8) { std::printf ("      %-12.2f %8d  too few clean events to measure\n",
                                              q.persist, (int) ev.size()); continue; }
            std::sort (ev.begin(), ev.end());
            double dq = 0, dl = 0; int nq = 0, nl = 0;
            for (size_t k = 0; k < ev.size(); ++k)
            {
                if (k < ev.size() / 2)      { dq += ev[k].second; ++nq; }
                if (k >= ev.size() * 3 / 4) { dl += ev[k].second; ++nl; }
            }
            dq /= std::max (1, nq); dl /= std::max (1, nl);
            std::printf ("      %-12.2f %8d %10.1f %12.2f %12.2f %9.2fx   send<=%.2f\n",
                         q.persist, (int) ev.size(), meanGap, dq, dl, dl / std::max (1e-9, dq),
                         e.maxRingSend.load());
        }
        std::printf ("\n      small = lower half by drive, big = top quarter.\n");
        std::printf ("      PERSISTENCE 0 is the spread the bank alone already gives.\n");
    }

    std::printf ("\n   c) STRATIFICATION — the gamut of the bank, band by band\n");
    {
        for (int si = 0; si <= 2; ++si)
        {
            const float st = si * 0.5f;
            std::printf ("      STRAT %.2f : ", st);
            double tlo = 1e18, thi = -1e18, flo = 1e18, fhi = -1e18;
            for (int b = 0; b < ab1::NBAND; ++b)
            {
                const double s = ((b + 0.5) / ab1::NBAND) * 2.0 - 1.0;
                const double mul = std::pow (2.0, s * (double) st * 1.9);
                const double a1 = std::min (0.95, std::max (1e-5, xmapd (def.shape, 0.60, 0.03) * mul));
                const double a2 = std::min (0.95, std::max (1e-5, xmapd (def.damp,  0.45, 0.015) * mul));
                //  reuse the kernel measurement by inverting the mapping
                std::vector<float> h ((size_t) SR / 2, 0.0f);
                double z1 = 0, z2 = 0;
                for (size_t i = 0; i < h.size(); ++i)
                { const double in = (i == 0) ? 1.0 : 0.0; z1 += a1*(in-z1); z2 += a2*(z1-z2); h[i] = (float)(z1-z2); }
                double pk = 0; for (float v : h) pk = std::max (pk, (double) std::abs (v));
                int i60 = 0; for (size_t i = 0; i < h.size(); ++i) if (std::abs (h[i]) > pk*0.001) i60 = (int) i;
                const double t60 = 1000.0 * i60 / SR, fc = rmsFreq (h, 0, (int) h.size());
                tlo = std::min (tlo, t60); thi = std::max (thi, t60);
                flo = std::min (flo, fc);  fhi = std::max (fhi, fc);
            }
            std::printf ("length %6.3f .. %6.3f ms   colour %6.0f .. %6.0f Hz\n", tlo, thi, flo, fhi);
        }
        std::printf ("      -> at 0 the eight bands are one body. That is the old instrument.\n");
    }

    std::printf ("\n   d) the shipping defaults against 260902.1\n");
    {
        Take o = renderTake (legacy, 20.0, true);
        Take t = renderTake (def, 20.0, true);
        std::printf ("      260902.1 (all three at 0) : colour %5.0f Hz, colour CV %5.1f %%, loud CV %6.1f %%\n",
                     o.fcMean, 100.0 * o.fcCV, 100.0 * o.rmsCV);
        std::printf ("      260902.2 defaults         : colour %5.0f Hz, colour CV %5.1f %%, loud CV %6.1f %%\n",
                     t.fcMean, 100.0 * t.fcCV, 100.0 * t.rmsCV);
    }

    std::printf ("\n   f) what the bank costs\n");
    {
        /*  Denormals flushed, because the plug-in flushes them
            (juce::ScopedNoDenormals in processBlock) and a bank of one-poles
            decaying towards zero is exactly the code that spends its life in
            the denormal range. Timing it without this measures a machine the
            instrument never runs on. */
        _mm_setcsr (_mm_getcsr() | 0x8040);          // FTZ | DAZ
        auto timeIt = [] (bool old, const ab1::Params& q)
        {
            const int nb = 40 * SR / BLK;
            std::vector<float> l (BLK), r (BLK);
            const clock_t t0 = clock();
            if (old)
            {
                ab1legacy::Engine e;
                e.p.level = q.level; e.p.rateLo = q.rateLo; e.p.rateHi = q.rateHi;
                e.p.couple = q.couple; e.p.dead = q.dead; e.p.leak = q.leak;
                e.p.temp = q.temp; e.p.shape = q.shape; e.p.damp = q.damp;
                e.prepare (SR, BLK); e.service(); e.setTransport (120, 0, true); e.noteOn (48, 0.9f);
                for (int b = 0; b < nb; ++b) e.process (l.data(), r.data(), BLK);
            }
            else
            {
                ab1::Engine e; e.p = q;
                e.prepare (SR, BLK); e.service(); e.setTransport (120, 0, true); e.noteOn (48, 0.9f);
                for (int b = 0; b < nb; ++b) e.process (l.data(), r.data(), BLK);
            }
            const double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
            return 40.0 / std::max (1e-9, secs);
        };
        const double xo = timeIt (true, legacy);
        const double xn = timeIt (false, def);
        std::printf ("      260902.1        %7.0f x realtime  (%.2f %% of one core)\n", xo, 100.0 / xo);
        std::printf ("      260902.2 as shipped %3.0f x realtime  (%.2f %% of one core)\n", xn, 100.0 / xn);
    }

    std::printf ("\n   e) the catalogue, before and after — colour movement WITHIN each take\n");
    {
        double cvOld = 0, cvNew = 0; int nn = 0;
        std::printf ("      %-6s %12s %12s\n", "spec", "260902.1", "260902.2");
        for (int s = 0; s < 256; s += 32)
        {
            ab1::Params a; ab1::applySpecimen (s, a);
            ab1::Params b = a; b.strat = b.persist = b.traverse = 0.0f;
            Take ta = renderTake (b, 12.0, true);
            Take tb = renderTake (a, 12.0, true);
            std::printf ("      %-6d %10.1f %% %10.1f %%\n", s, 100.0*ta.fcCV, 100.0*tb.fcCV);
            cvOld += ta.fcCV; cvNew += tb.fcCV; ++nn;
        }
        std::printf ("      mean %10.1f %% %10.1f %%\n", 100.0*cvOld/nn, 100.0*cvNew/nn);
    }

    //==========================================================================
    /*  5. THE TWO THINGS PETER STILL HEARD MISSING — and both were measured in
        ABSOLUTE terms this time, not as ratios. 260902.2 improved the length
        ratio between big and small events by 2.4x while the longest tail in the
        whole instrument was still 48 ms, and a ratio cannot tell you that. */
    std::printf ("\n5. WEIGHT AND ENCLOSURE (260902.3)\n");

    //  how much of the object's energy is below 250 Hz, by a 2-pole lowpass
    auto lowShare = [] (const std::vector<float>& v)
    {
        double z1 = 0, z2 = 0, lo = 0, all = 0;
        const double a = 1.0 - std::exp (-2.0 * ab1::PI * 250.0 / SR);
        for (float s : v)
        {
            z1 += a * (s - z1); z2 += a * (z1 - z2);
            lo += z2 * z2; all += (double) s * s;
        }
        return all > 1e-30 ? lo / all : 0.0;
    };
    /*  How long the object takes to fall 40 dB once the counting stops. The
        lattice is frozen by dropping to 77 K, where process() returns without
        touching the buffer — so the tail has to be measured by rendering the
        body's own decay through a SECOND engine fed nothing. Simpler and
        honest: measure the gap statistics instead — the level halfway between
        events, against the peak. A longer tail fills the gaps. */
    auto gapFill = [] (const std::vector<float>& v)
    {
        std::vector<double> w;
        const int W = 256;
        for (int a = SR; a + W < (int) v.size(); a += W)
        {
            double s = 0; for (int i = a; i < a + W; ++i) s += std::abs (v[(size_t)i]);
            w.push_back (s / W);
        }
        if (w.size() < 20) return 0.0;
        std::vector<double> srt = w; std::sort (srt.begin(), srt.end());
        const double quiet = srt[srt.size() / 10];        // the 10th percentile window
        const double loud  = srt[srt.size() * 95 / 100];
        return loud > 1e-12 ? quiet / loud : 0.0;
    };

    struct Row { const char* name; ab1::Params p; };
    ab1::Params pOff = def;  pOff.weight = 0.0f; pOff.space = 0.0f;
    ab1::Params pW   = def;  pW.weight   = 0.85f; pW.space = 0.0f;
    ab1::Params pE   = def;  pE.weight   = 0.0f;  pE.space = 0.70f;
    ab1::Params pB   = def;  pB.weight   = 0.85f; pB.space = 0.70f;
    Row rows[4] = { { "neither", pOff }, { "WEIGHT .85", pW },
                    { "ENCLOSURE .70", pE }, { "both", pB } };

    std::printf ("   at the shipping defaults, 12 s each:\n");
    std::printf ("   %-16s %14s %14s %12s\n", "", "energy <250 Hz", "gap fill", "colour Hz");
    for (Row& r : rows)
    {
        ab1::Engine e; e.p = r.p;
        e.prepare (SR, BLK); e.service(); e.setTransport (120, 0, true); e.noteOn (48, 0.9f);
        std::vector<float> L; std::vector<float> bl (BLK), br (BLK);
        for (int b = 0; b < 12 * SR / BLK; ++b)
        { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }
        std::printf ("   %-16s %13.1f %% %13.1f %% %12.0f\n", r.name,
                     100.0 * lowShare (L), 100.0 * gapFill (L),
                     rmsFreq (L, SR, (int) L.size()));
    }

    std::printf ("\n   the crate's own tail, measured on its impulse response:\n");
    for (float sp : { 0.10f, 0.30f, 0.50f, 0.70f, 0.90f })
    {
        ab1::Params q = def; q.space = sp; q.weight = 0.0f;
        q.temp = 0.0f;                       // 77 K: the lattice does not count
        ab1::Engine e; e.p = q;
        e.prepare (SR, BLK); e.service();
        /*  Cold, process() returns without writing, so the crate is fed by
            hand: warm it for one block with a single strike, then freeze. */
        e.p.temp = 0.298755f;
        std::vector<float> bl (BLK), br (BLK), L;
        e.setTransport (120, 0, true); e.noteOn (48, 1.0f);
        for (int b = 0; b < 8; ++b)
        { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }
        e.p.temp = 0.0f;                     // stop the counting; only the body is left
        const size_t mark = L.size();
        for (int b = 0; b < 4 * SR / BLK; ++b)
        { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }
        double pk = 0; for (size_t i = 0; i < mark; ++i) pk = std::max (pk, (double) std::abs (L[i]));
        double t40 = 0;
        for (size_t i = mark; i < L.size(); ++i)
            if (std::abs (L[i]) > pk * 0.01) t40 = 1000.0 * (double)(i - mark) / SR;
        std::printf ("      ENCLOSURE %.2f : %8.1f ms to -40 dB after the counting stops\n", sp, t40);
    }

    std::printf ("\n   half the catalogue is drawn sparse now — odd numbers:\n");
    std::printf ("   %-6s %10s %12s %12s %10s\n", "spec", "fire/s", "<250 Hz", "gap fill", "colour Hz");
    for (int s : { 8, 9, 40, 41, 136, 137, 200, 201 })
    {
        ab1::Params q; ab1::applySpecimen (s, q);
        ab1::Engine e; e.p = q;
        e.prepare (SR, BLK); e.service(); e.setTransport (120, 0, true); e.noteOn (48, 0.9f);
        std::vector<float> L; std::vector<float> bl (BLK), br (BLK);
        for (int b = 0; b < 12 * SR / BLK; ++b)
        { e.process (bl.data(), br.data(), BLK); L.insert (L.end(), bl.begin(), bl.end()); }
        std::printf ("   %-6d %10.0f %11.1f %% %11.1f %% %10.0f%s\n", s,
                     (double) e.totalFired.load() / 12.0,
                     100.0 * lowShare (L), 100.0 * gapFill (L),
                     rmsFreq (L, SR, (int) L.size()), (s & 1) ? "   <- sparse" : "");
    }

    return 0;
}
