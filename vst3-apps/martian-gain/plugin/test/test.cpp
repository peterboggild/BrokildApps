/*  Offline bench for the Mars Wars engine.

    Three things here are claims rather than opinions, so they get measured:
      1. the crossover tree sums flat, for every band count,
      2. the oversampling is transparent in the audio band,
      3. turning DRIVE up does not turn the volume up — the whole point of
         the automatic gain match, checked for all sixteen shapers.
    The rest is the usual: nothing non-finite, nothing over full scale,
    nothing that grows on its own.
*/
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int failures = 0;
    constexpr double PI = 3.14159265358979323846;

    struct Pink
    {
        float a = 0, b = 0, c = 0, g = 0.22f;
        mw::Rng r;
        float next()
        {
            const float w = r.bi();
            a = 0.99765f * a + w * 0.0990460f;
            b = 0.96300f * b + w * 0.2965164f;
            c = 0.57000f * c + w * 1.0526913f;
            return (a + b + c + w * 0.1848f) * g;
        }
    };

    // Amplitude of frequency f in x, by Goertzel.
    double mag (const std::vector<float>& x, double f, double sr)
    {
        const double w = 2.0 * PI * f / sr;
        const double co = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (float v : x) { const double s0 = v + co * s1 - s2; s2 = s1; s1 = s0; }
        return 2.0 * std::sqrt (std::abs (s1 * s1 + s2 * s2 - co * s1 * s2)) / (double) x.size();
    }

    double rms (const std::vector<float>& x)
    {
        double s = 0;
        for (float v : x) s += (double) v * v;
        return std::sqrt (s / (double) (x.empty() ? 1 : x.size()));
    }

    // Run the engine on a generated signal; return the output, dropping the
    // first `skip` seconds so followers and filters have settled.
    template <typename Gen>
    std::vector<float> run (mw::Engine& e, double sr, double seconds, double skip, Gen gen)
    {
        const int block = 128;
        const int total = (int) (sr * seconds);
        const int drop  = (int) (sr * skip);
        std::vector<float> L ((size_t) block), R ((size_t) block), out;
        out.reserve ((size_t) std::max (0, total - drop));
        for (int done = 0; done < total; done += block)
        {
            for (int i = 0; i < block; ++i) { const float v = gen (done + i); L[(size_t) i] = v; R[(size_t) i] = v; }
            e.process (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i)
                if (done + i >= drop) out.push_back (L[(size_t) i]);
        }
        return out;
    }

    void clean (mw::Engine& e, int bands)
    {
        e.p = mw::Params{};
        e.p.bands = bands;
        e.p.os = 0;
        e.p.autoGain = 0.0f;
        e.p.masterLim = 0.0f;
        for (auto& b : e.p.band) { b.mix = 0.0f; b.drive = 0.0f; b.level = 0.5f; b.on = 1.0f; b.ceil = 1.0f; }
        e.reset();
    }
}

int main()
{
    const double SR = 48000.0;

    // ---- 1 · the crossover tree sums flat ---------------------------------
    {
        const double freqs[] = { 30, 60, 120, 250, 500, 1000, 2000, 4000, 8000, 14000 };
        for (int nb = 1; nb <= mw::MAX_BANDS; ++nb)
        {
            double worst = 0; double atF = 0;
            for (double f : freqs)
            {
                mw::Engine e; e.prepare (SR, 128); clean (e, nb);
                auto out = run (e, SR, 0.55, 0.25, [&] (int n) { return 0.25f * (float) std::sin (2.0 * PI * f * n / SR); });
                const double g = mag (out, f, SR) / 0.25;
                const double dB = 20.0 * std::log10 (std::max (1e-9, g));
                if (std::abs (dB) > std::abs (worst)) { worst = dB; atF = f; }
            }
            const bool ok = std::abs (worst) < 0.6;
            std::printf ("%-34s %d bands: worst %+5.2f dB at %5.0f Hz  %s\n",
                         "crossover sums flat", nb, worst, atF, ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }
    }

    // ---- 2 · oversampling is transparent in the audio band ----------------
    {
        const double freqs[] = { 100, 1000, 5000, 10000, 15000 };
        for (int os = 1; os <= 2; ++os)
        {
            double worst = 0, atF = 0;
            for (double f : freqs)
            {
                mw::Engine e; e.prepare (SR, 128); clean (e, 3);
                e.p.os = os; e.reset();
                auto out = run (e, SR, 0.55, 0.25, [&] (int n) { return 0.25f * (float) std::sin (2.0 * PI * f * n / SR); });
                const double dB = 20.0 * std::log10 (std::max (1e-9, mag (out, f, SR) / 0.25));
                if (std::abs (dB) > std::abs (worst)) { worst = dB; atF = f; }
            }
            const bool ok = std::abs (worst) < 0.8;
            std::printf ("%-34s %dx: worst %+5.2f dB at %5.0f Hz  %s\n",
                         "oversampling transparent", os == 1 ? 2 : 4, worst, atF, ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }
    }

    // ---- 3 · DRIVE does not change the level ------------------------------
    {
        std::printf ("\n%-14s %8s %8s %8s %8s %8s   %s\n",
                     "algorithm", "d=0.0", "d=0.25", "d=0.5", "d=0.75", "d=1.0", "spread");
        double worstSpread = 0; int worstAlgo = 0;
        for (int a = 0; a < mw::NUM_ALGOS; ++a)
        {
            double lvl[5];
            for (int s = 0; s < 5; ++s)
            {
                mw::Engine e; e.prepare (SR, 128);
                e.p = mw::Params{};
                e.p.bands = 1;
                e.p.os = 1;
                e.p.autoGain = 1.0f;
                e.p.masterLim = 0.0f;
                for (auto& b : e.p.band)
                {
                    b.algo = a; b.mix = 1.0f; b.on = 1.0f; b.level = 0.5f;
                    b.bias = 0.5f; b.chr = 0.5f; b.tone = 0.5f; b.ceil = 1.0f;
                    b.drive = (float) s * 0.25f;
                }
                e.reset();
                Pink pk; pk.g = 0.07f;   // -20 dBFS: headroom, so nothing clips
                auto out = run (e, SR, 3.0, 1.6, [&] (int) { return pk.next(); });
                lvl[s] = 20.0 * std::log10 (std::max (1e-9, rms (out)));
            }
            double lo = lvl[0], hi = lvl[0];
            for (int s = 1; s < 5; ++s) { lo = std::min (lo, lvl[s]); hi = std::max (hi, lvl[s]); }
            const double spread = hi - lo;
            if (spread > worstSpread) { worstSpread = spread; worstAlgo = a; }
            std::printf ("%-14s %8.2f %8.2f %8.2f %8.2f %8.2f   %5.2f dB %s\n",
                         mw::algoName (a), lvl[0], lvl[1], lvl[2], lvl[3], lvl[4],
                         spread, spread < 3.0 ? "ok" : "LOUD");
            if (spread >= 3.0) ++failures;
        }
        std::printf ("%-34s worst %.2f dB (%s)\n\n", "gain match across DRIVE",
                     worstSpread, mw::algoName (worstAlgo));
    }

    // ---- 4 · nothing non-finite, nothing over full scale ------------------
    {
        mw::Rng r; r.s = 0xfeedface;
        float worst = 0; int bad = 0;
        for (int t = 0; t < 240; ++t)
        {
            mw::Engine e; e.prepare (SR, 128);
            e.p = mw::Params{};
            e.p.bands = 1 + (int) (r.uni() * 4.99f);
            e.p.os = (int) (r.uni() * 2.99f);
            e.p.autoGain = r.uni() < 0.8f ? 1.0f : 0.0f;
            e.p.masterLim = r.uni() < 0.8f ? 1.0f : 0.0f;
            e.p.inGain = 0.3f + r.uni() * 0.5f;
            e.p.outGain = 0.3f + r.uni() * 0.5f;
            e.p.dryWet = r.uni();
            for (int k = 0; k < mw::MAX_BANDS - 1; ++k) e.p.xover[(size_t) k] = r.uni();
            for (auto& b : e.p.band)
            {
                b.algo = (int) (r.uni() * (mw::NUM_ALGOS - 0.01f));
                b.on = r.uni() < 0.85f ? 1.0f : 0.0f;
                b.solo = r.uni() < 0.1f ? 1.0f : 0.0f;
                b.drive = r.uni(); b.bias = r.uni(); b.chr = r.uni();
                b.tone = r.uni(); b.mix = r.uni(); b.level = r.uni(); b.ceil = 0.3f + r.uni() * 0.7f;
            }
            e.reset();
            Pink pk;
            auto out = run (e, SR, 1.2, 0.0, [&] (int) { return pk.next() * 3.0f; });
            bool fin = true;
            for (float v : out) { if (! std::isfinite (v)) fin = false; worst = std::max (worst, std::abs (v)); }
            if (! fin || worst > 1.0001f) { ++bad; if (bad < 4) std::printf ("  random %d: finite %d peak %.3f\n", t, (int) fin, worst); }
        }
        std::printf ("%-34s peak %.3f  bad %d/240  %s\n", "240 random settings", worst, bad, bad ? "BAD" : "ok");
        failures += bad ? 1 : 0;
    }

    // ---- 5 · sample rates and oversampling ratios -------------------------
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (int os = 0; os <= 2; ++os)
        {
            mw::Engine e; e.prepare (sr, 128);
            e.p = mw::Params{};
            e.p.bands = 4; e.p.os = os;
            for (auto& b : e.p.band) { b.drive = 0.8f; b.mix = 1.0f; b.algo = 4; b.level = 0.5f; }
            e.reset();
            Pink pk;
            auto out = run (e, sr, 1.0, 0.0, [&] (int) { return pk.next(); });
            bool fin = true; float peak = 0;
            for (float v : out) { if (! std::isfinite (v)) fin = false; peak = std::max (peak, std::abs (v)); }
            const bool ok = fin && peak <= 1.0001f;
            std::printf ("%-24s %6.0f Hz  %dx  peak %.3f  %s\n", "rate / oversampling", sr,
                         os == 0 ? 1 : (os == 1 ? 2 : 4), peak, ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }

    // ---- 6 · switching bands mid-stream does not click --------------------
    // A hard-clipped broadband signal has big steps of its own, so the test
    // is against a control run that changes nothing, not against a constant.
    {
        auto runSwitch = [&] (bool doSwitch, int startBands)
        {
            mw::Engine e; e.prepare (SR, 128);
            e.p = mw::Params{};
            e.p.bands = startBands; e.p.os = 1;
            for (auto& b : e.p.band) { b.drive = 0.6f; b.mix = 1.0f; b.algo = 1; b.level = 0.5f; }
            e.reset();
            std::vector<float> L (128), R (128);
            Pink pk; pk.g = 0.10f;
            float biggest = 0, prev = 0; int atBlk = -1;
            for (int blk = 0; blk < 400; ++blk)
            {
                if (doSwitch)
                {
                    if (blk == 120) e.p.bands = 5;
                    if (blk == 200) e.p.os = 2;
                    if (blk == 280) e.p.band[0].on = 0.0f;
                    if (blk == 340) e.p.band[2].solo = 1.0f;
                }
                for (int i = 0; i < 128; ++i) { const float v = pk.next(); L[(size_t) i] = v; R[(size_t) i] = v; }
                e.process (L.data(), R.data(), 128);
                if (blk > 40)
                    for (int i = 0; i < 128; ++i)
                    { const float st = std::abs (L[(size_t) i] - prev);
                      if (st > biggest) { biggest = st; atBlk = blk; }
                      prev = L[(size_t) i]; }
                else prev = L[127];
            }
            return biggest;
        };
        const float c2 = runSwitch (false, 2);
        const float c5 = runSwitch (false, 5);
        const float control = std::max (c2, c5);
        const float switched = runSwitch (true, 2);
        const bool ok = switched <= control * 1.25f + 0.02f;
        std::printf ("%-34s steady 2/5 bands %.3f/%.3f  while switching %.3f  %s\n",
                     "band changes add no step", c2, c5, switched, ok ? "ok" : "CLICKS");
        if (! ok) ++failures;
    }

    // ---- 7 · silence in, silence out --------------------------------------
    {
        mw::Engine e; e.prepare (SR, 128);
        e.p = mw::Params{};
        e.p.bands = 5; e.p.os = 1;
        for (auto& b : e.p.band) { b.drive = 1.0f; b.mix = 1.0f; b.algo = 15; }
        e.reset();
        auto out = run (e, SR, 1.0, 0.2, [] (int) { return 0.0f; });
        float peak = 0; for (float v : out) peak = std::max (peak, std::abs (v));
        const bool ok = peak < 0.01f;
        std::printf ("%-34s peak %.5f  %s\n", "silence stays silent", peak, ok ? "ok" : "NOISY");
        if (! ok) ++failures;

        /*  One second is nowhere near long enough to catch this. The automatic
            gain match is a slow follower, and with nothing coming in it has
            nothing to match — so it can spend tens of seconds winding its gain
            up and amplifying whatever numerical dust is left in the filters.
            A muted track that hisses is a real fault, and the short test above
            walks straight past it. */
        mw::Engine e2; e2.prepare (SR, 128);
        e2.p = mw::Params{};
        e2.p.bands = 5; e2.p.os = 1;
        e2.reset();
        auto longOut = run (e2, SR, 45.0, 40.0, [] (int) { return 0.0f; });
        float lp = 0; for (float v : longOut) lp = std::max (lp, std::abs (v));
        const double ldb = 20.0 * std::log10 (std::max (1e-9, (double) lp));
        const bool lok = lp < 0.0005f;                        // below about -66 dBFS
        std::printf ("%-34s peak %.6f  (%.1f dBFS)  %s\n", "silence stays silent for 45 s",
                     lp, ldb, lok ? "ok" : "WINDS UP");
        if (! lok) ++failures;

        /*  And silence with the BIAS knob moved off centre. Bias shifts the
            shaper's operating point, which is the whole point of it — but it
            also means a band with no input is not being fed zero, it is being
            fed the offset, and a shaper fed a constant emits something. On a
            muted track that is audible hiss, or for RING MOD an audible tone.
            Found on the running plugin, so every algorithm gets checked. */
        std::printf ("\n");
        int gen = 0;
        for (int algo = 0; algo < mw::NUM_ALGOS; ++algo)
        {
            mw::Engine e3; e3.prepare (SR, 128);
            e3.p = mw::Params{};
            e3.p.bands = 1; e3.p.os = 1;
            e3.p.band[0].algo = algo;
            e3.p.band[0].bias  = 0.456f;              // what the live plugin had
            e3.p.band[0].drive = 0.33f;
            e3.p.band[0].mix   = 0.82f;
            e3.reset();
            auto o = run (e3, SR, 6.0, 3.0, [] (int) { return 0.0f; });
            const double rr = rms (o);
            const double db = 20.0 * std::log10 (std::max (1e-9, rr));
            const bool quiet = rr < 1.0e-4;           // below about -80 dBFS
            if (! quiet)
            {
                ++gen;
                std::printf ("   %-12s emits %.5f rms (%.1f dBFS) with NO input\n",
                             mw::algoName (algo), rr, db);
            }
        }
        std::printf ("%-34s %d of %d generate  %s\n", "no input, biased: still silent?",
                     gen, mw::NUM_ALGOS, gen == 0 ? "ok" : "THEY GENERATE");
        if (gen) ++failures;
    }

    // ---- 8 · the patch bay cannot run away --------------------------------
    //  Audio cables read the previous sample and every band is clamped at its
    //  own ceiling, so even a deliberate ring of them has to stay bounded.
    {
        mw::Rng r; r.s = 0x51ce1234;
        float worst = 0; int bad = 0;

        // first, the nastiest case anyone could patch by hand: a full ring
        {
            mw::Engine e; e.prepare (SR, 128);
            e.p = mw::Params{};
            e.p.bands = 5; e.p.os = 1; e.p.masterLim = 1.0f;
            for (auto& b : e.p.band) { b.on = 1.0f; b.drive = 0.9f; b.mix = 1.0f; b.algo = 4; b.ceil = 1.0f; }
            for (int i = 0; i < 5; ++i)
            {
                e.p.cable[(size_t) i].src = mw::S_AUD1 + i;
                e.p.cable[(size_t) i].dst = mw::D_IN1 + ((i + 1) % 5);
                e.p.cable[(size_t) i].amt = 1.0f;
            }
            e.reset();
            Pink pk;
            auto out = run (e, SR, 3.0, 0.0, [&] (int n) { return n < 20000 ? pk.next() : 0.0f; });
            bool fin = true; float peak = 0, tail = 0;
            for (size_t i = 0; i < out.size(); ++i)
            {
                if (! std::isfinite (out[i])) fin = false;
                peak = std::max (peak, std::abs (out[i]));
                if (i > out.size() * 3 / 4) tail = std::max (tail, std::abs (out[i]));
            }
            const bool ok = fin && peak <= 1.0001f;
            std::printf ("%-34s peak %.3f  tail %.3f  %s\n",
                         "five audio cables in a ring", peak, tail, ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }

        // then a few hundred random patchings
        for (int t = 0; t < 200; ++t)
        {
            mw::Engine e; e.prepare (SR, 128);
            e.p = mw::Params{};
            e.p.bands = 1 + (int) (r.uni() * 4.99f);
            e.p.os = (int) (r.uni() * 2.99f);
            for (auto& b : e.p.band)
            {
                b.algo = (int) (r.uni() * (mw::NUM_ALGOS - 0.01f));
                b.on = 1.0f; b.drive = r.uni(); b.bias = r.uni(); b.chr = r.uni();
                b.tone = r.uni(); b.mix = r.uni(); b.level = r.uni(); b.ceil = 0.4f + r.uni() * 0.6f;
            }
            for (auto& c : e.p.cable)
            {
                c.src = (int) (r.uni() * (mw::NUM_SOURCES - 0.01f));
                c.dst = (int) (r.uni() * (mw::NUM_DESTS - 0.01f));
                c.amt = r.bi();
            }
            e.reset();
            Pink pk;
            auto out = run (e, SR, 1.2, 0.0, [&] (int) { return pk.next() * 2.0f; });
            bool fin = true;
            for (float v : out) { if (! std::isfinite (v)) fin = false; worst = std::max (worst, std::abs (v)); }
            if (! fin || worst > 1.0001f) { ++bad; if (bad < 4) std::printf ("  patch %d: finite %d peak %.3f\n", t, (int) fin, worst); }
        }
        std::printf ("%-34s peak %.3f  bad %d/200  %s\n", "200 random patchings", worst, bad, bad ? "BAD" : "ok");
        failures += bad ? 1 : 0;
    }

    // ---- 9 · the cables actually do something -----------------------------
    //  Everything above proves a patched machine stays bounded. None of it
    //  proves a cable is connected to anything: a cable that does nothing
    //  would pass all of it. So render the same input twice, once with the
    //  cable and once without, and measure how far apart the two outputs are.
    {
        std::printf ("\n");

        //  How different are two renders, as a fraction of the signal?
        auto apart = [] (const std::vector<float>& x, const std::vector<float>& y)
        {
            const size_t n = std::min (x.size(), y.size());
            double num = 0, den = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double d = (double) x[i] - (double) y[i];
                num += d * d;
                den += (double) x[i] * x[i];
            }
            return den > 0 ? std::sqrt (num / den) : 0.0;
        };

        //  A machine with something happening in every band, and the auto
        //  gain off so a cable's effect is not quietly compensated away.
        auto setup = [&] (mw::Engine& e)
        {
            e.prepare (SR, 128);
            e.p = mw::Params{};
            e.p.bands = 3;
            e.p.os = 1;
            e.p.autoGain = 0.0f;
            e.p.masterLim = 0.0f;
            for (int b = 0; b < mw::MAX_BANDS; ++b)
            {
                auto& bd = e.p.band[(size_t) b];
                bd.on = 1.0f; bd.algo = 7;          // SINE FOLD: character really bites
                bd.drive = 0.45f; bd.chr = 0.5f; bd.bias = 0.5f;
                bd.tone = 0.5f; bd.mix = 1.0f; bd.level = 0.5f; bd.ceil = 0.9f;
            }
            e.reset();
        };

        auto render = [&] (int src, int dst, float amt)
        {
            mw::Engine e; setup (e);
            e.p.cable[0].src = src;
            e.p.cable[0].dst = dst;
            e.p.cable[0].amt = amt;
            Pink pk;                                 // same seed every time
            return run (e, SR, 1.4, 0.35, [&] (int) { return pk.next() * 1.6f; });
        };

        const auto none = render (mw::S_NONE, mw::D_NONE, 0.0f);

        struct Case { const char* what; int src; int dst; };
        const Case cases[] = {
            { "ENV IN  -> band 1 DRIVE",     mw::S_INENV,  mw::D_DRIVE1 },
            { "ENV IN  -> band 1 LEVEL",     mw::S_INENV,  mw::D_LEVEL1 },
            { "ENV IN  -> band 1 CHARACTER", mw::S_INENV,  mw::D_CHAR1  },
            { "ENV IN  -> band 1 CEILING",   mw::S_INENV,  mw::D_CEIL1  },
            { "ENV IN  -> crossover 1",      mw::S_INENV,  mw::D_XOVER1 },
            { "ENV 1   -> band 3 DRIVE",     mw::S_ENV1,   mw::D_DRIVE3 },
            { "ENV OUT -> band 2 CHARACTER", mw::S_OUTENV, mw::D_CHAR2  },
            { "AUDIO 1 -> band 2 AUDIO IN",  mw::S_AUD1,   mw::D_IN2    },
            { "AUDIO 3 -> band 1 AUDIO IN",  mw::S_AUD3,   mw::D_IN1    }
        };

        int dead = 0;
        for (const auto& c : cases)
        {
            const double d = apart (none, render (c.src, c.dst, 1.0f)) * 100.0;
            const bool ok = d > 1.0;               // at least one per cent different
            std::printf ("%-34s changes the output by %6.2f %%  %s\n", c.what, d,
                         ok ? "ok" : "DOES NOTHING");
            if (! ok) ++dead;
        }
        failures += dead ? 1 : 0;

        //  ...and the amount control has to mean something too.
        const double atZero = apart (none, render (mw::S_INENV, mw::D_DRIVE1, 0.0f)) * 100.0;
        const double atPlus = apart (none, render (mw::S_INENV, mw::D_DRIVE1, 1.0f)) * 100.0;
        const double atMinus = apart (none, render (mw::S_INENV, mw::D_DRIVE1, -1.0f)) * 100.0;
        const double plusVsMinus = apart (render (mw::S_INENV, mw::D_DRIVE1, 1.0f),
                                          render (mw::S_INENV, mw::D_DRIVE1, -1.0f)) * 100.0;
        const bool amtOk = atZero < 0.01 && atPlus > 1.0 && atMinus > 1.0 && plusVsMinus > 1.0;
        std::printf ("%-34s 0%% %.3f  +100%% %.2f  -100%% %.2f  +/- apart %.2f  %s\n",
                     "AMOUNT means something", atZero, atPlus, atMinus, plusVsMinus,
                     amtOk ? "ok" : "BAD");
        failures += amtOk ? 0 : 1;

        //  An unpatched cable must be inert however it is set.
        const double unpatched = apart (none, render (mw::S_NONE, mw::D_DRIVE1, 1.0f)) * 100.0;
        std::printf ("%-34s %.4f %%  %s\n", "a cable with no source is inert",
                     unpatched, unpatched < 0.01 ? "ok" : "BAD");
        failures += unpatched < 0.01 ? 0 : 1;

        //  A control cable must follow its source: a loud band and a silent
        //  one cannot drive a destination to the same place.
        {
            mw::Engine e1; setup (e1);
            e1.p.band[4].on = 0.0f;                 // band 5 is dark, so its envelope is nothing
            e1.p.cable[0] = { mw::S_ENV5, mw::D_DRIVE1, 1.0f };
            Pink p1; auto a1 = run (e1, SR, 1.4, 0.35, [&] (int) { return p1.next() * 1.6f; });

            mw::Engine e2; setup (e2);
            e2.p.band[4].on = 0.0f;
            e2.p.cable[0] = { mw::S_ENV1, mw::D_DRIVE1, 1.0f };
            Pink p2; auto a2 = run (e2, SR, 1.4, 0.35, [&] (int) { return p2.next() * 1.6f; });

            const double d = apart (a1, a2) * 100.0;
            std::printf ("%-34s %.2f %%  %s\n", "the SOURCE matters, not just the wire",
                         d, d > 1.0 ? "ok" : "SOURCE IGNORED");
            failures += d > 1.0 ? 0 : 1;
        }
    }

    std::printf ("\n%s  (%d failure%s)\n", failures ? "FAILED" : "ALL CLEAR",
                 failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
