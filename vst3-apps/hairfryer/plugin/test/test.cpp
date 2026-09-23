/*  Offline bench for the Hairfryer engine.

    The claim this plugin makes is unusually checkable: a metal scream IS
    subharmonics plus pulse-locked noise wearing the singer's formants. So
    the bench sings into the plugin — a synthetic sung vowel, glottal pulse
    train through a two-formant filter — and MEASURES, by Goertzel:

      1.  the subharmonics: with the growl on, f0/2 and f0/3 energy that the
          input does not have;
      2.  the formants survive: vowel resonances still in place with the
          basket at work — this is what separates the design from a fuzz
          pedal, so it is a test, not a hope;
      3.  the pitch survives: f0 still the strongest of {f0/2, f0, f0*1.5};
      4.  the chaos knob walks the cascade: period patterns 1 -> 2 -> 3;
      5.  transparency: everything off = delay + nothing, measured;
      6.  the strip does strip things: gate gates, de-esser ducks esses and
          not vowels, compressor compresses, limiter holds the ceiling;
      7.  every recipe, every extreme, 200 random settings, 4 sample rates:
          finite, bounded, no growth, silence in = silence out.

    Lesson carried over from The Mars Wars: bounded is not the same as
    working. Every claim above is an effect measurement, not a survival
    measurement.
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

    double peakAbs (const std::vector<float>& x)
    {
        double p = 0;
        for (float v : x) p = std::max (p, (double) std::abs (v));
        return p;
    }

    double db (double v) { return 20.0 * std::log10 (std::max (1.0e-12, v)); }

    void check (bool ok, const char* what)
    {
        std::printf ("  %-64s %s\n", what, ok ? "ok" : "FAIL");
        if (! ok) ++failures;
    }

    //==========================================================================
    /*  A synthetic singer. A Rosenberg-ish glottal pulse train at f0 with a
        touch of shimmer, through two formant resonators — an "ah" (F1 700,
        F2 1100) — plus a little breath. Deterministic. */
    struct Singer
    {
        double sr, f0;
        double ph = 0.0;
        hf::Biquad fm1, fm2;
        hf::Rng rng;
        float breath = 0.004f;

        Singer (double sampleRate, double hz, float f1 = 700.0f, float f2 = 1100.0f)
            : sr (sampleRate), f0 (hz)
        {
            fm1.bandpass (f1, 6.0f, sr);
            fm2.bandpass (f2, 8.0f, sr);
            rng.seed (0xbeefu);
        }

        float next()
        {
            ph += f0 / sr;
            if (ph >= 1.0) ph -= 1.0;
            // glottal-ish: sharp closure, gentle opening
            const double t = ph;
            double g = 0.0;
            if (t < 0.6) g = 0.5 * (1.0 - std::cos (PI * t / 0.6));
            else         g = std::cos (0.5 * PI * (t - 0.6) / 0.4);
            const float src = (float) (g * 2.0 - 0.8) + rng.bi() * breath;
            return (fm1 (src) * 1.2f + fm2 (src) * 0.8f + src * 0.08f) * 0.9f;
        }
    };

    // Run the engine over a generated signal, return left channel after skip.
    template <typename Gen>
    std::vector<float> run (hf::Engine& e, double sr, double seconds, double skip, Gen gen)
    {
        const int block = 128;
        const int total = (int) (sr * seconds);
        const int drop  = (int) (sr * skip);
        std::vector<float> L ((size_t) block), R ((size_t) block), out;
        out.reserve ((size_t) std::max (0, total - drop));
        for (int done = 0; done < total; done += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = gen (done + i);
                L[(size_t) i] = v;
                R[(size_t) i] = v;
            }
            e.process (L.data(), R.data(), block);
            for (int i = 0; i < block; ++i)
                if (done + i >= drop) out.push_back (L[(size_t) i]);
        }
        return out;
    }

    // A vowel sung through a given parameter set.
    std::vector<float> sing (const hf::Params& p, double sr, double f0,
                             double seconds = 2.2, double skip = 0.7)
    {
        hf::Engine e;
        e.prepare (sr, 128);
        e.p = p;
        Singer s (sr, f0);
        return run (e, sr, seconds, skip, [&] (int) { return s.next(); });
    }

    hf::Params neutral()
    {
        hf::Params p;                    // defaults
        p.engOn = 0.0f; p.blend = 0.0f;
        p.gateOn = 0.0f; p.deEssOn = 0.0f; p.eqOn = 0.0f;
        p.compOn = 0.0f; p.limOn = 0.0f;
        p.drive = 0.0f; p.tube = 0.0f; p.sparkle = 0.0f;
        return p;
    }

    hf::Params growlPatch (float chaos, float grip = 0.8f)
    {
        hf::Params p = neutral();
        p.engOn = 1.0f; p.blend = 1.0f; p.match = 1.0f;
        p.a.level = 0.8f; p.a.chaos = chaos; p.a.grip = grip;
        p.a.rasp = 0.0f; p.a.fold = 0.0f; p.a.split = 0.0f;
        p.a.jitter = 0.0f; p.a.throat = 0.5f;
        p.b.level = 0.0f;
        return p;
    }
}

//==============================================================================
int main()
{
    const double SR = 48000.0;
    const double F0 = 165.0;             // an E3-ish full-voice note

    std::printf ("HAIRFRYER offline bench\n=======================\n");

    // ---- 1 · transparency ---------------------------------------------------
    {
        std::printf ("\n1 - everything off is a wire\n");
        hf::Params p = neutral();

        /*  Deterministic singer (no breath — the latency shift decorrelates
            noise between the two windows) and only EXACT harmonics of f0:
            a frequency between comb lines measures leakage, not signal. */
        Singer sIn (SR, F0);
        sIn.breath = 0.0f;
        std::vector<float> in;
        const int skipN = (int) (SR * 0.7);
        for (int i = 0; i < (int) (SR * 2.2); ++i)
        {
            const float v = sIn.next();
            if (i >= skipN) in.push_back (v);
        }

        hf::Engine e;
        e.prepare (SR, 128);
        e.p = p;
        Singer sOut (SR, F0);
        sOut.breath = 0.0f;
        auto out = run (e, SR, 2.2, 0.7, [&] (int) { return sOut.next(); });

        double worst = 0.0;
        for (int h : { 1, 2, 3, 4, 6, 12 })
        {
            const double f = F0 * h;
            const double a = mag (in, f, SR), b = mag (out, f, SR);
            worst = std::max (worst, std::abs (db (b) - db (a)));
        }
        std::printf ("    worst harmonic deviation %.2f dB\n", worst);
        check (worst < 1.0, "bypassed strip is spectrally transparent (< 1 dB)");
    }

    // ---- 2 · the subharmonics are real --------------------------------------
    {
        std::printf ("\n2 - the growl makes subharmonics the input does not have\n");
        Singer sIn (SR, F0);
        std::vector<float> in;
        for (int i = 0; i < (int) (SR * 2.2); ++i) in.push_back (sIn.next());
        const double inSub2 = db (mag (in, F0 / 2, SR)) - db (mag (in, F0, SR));

        // chaos on the two-cycle: expect f0/2
        auto o2 = sing (growlPatch (0.30f), SR, F0);
        const double sub2 = db (mag (o2, F0 / 2, SR)) - db (mag (o2, F0, SR));

        // chaos on the period-three plateau: expect f0/3
        auto o3 = sing (growlPatch (0.85f), SR, F0);
        const double sub3 = db (mag (o3, F0 / 3, SR)) - db (mag (o3, F0, SR));

        std::printf ("    input   f0/2 vs f0 : %6.1f dB\n", inSub2);
        std::printf ("    chaos.30 f0/2 vs f0: %6.1f dB\n", sub2);
        std::printf ("    chaos.85 f0/3 vs f0: %6.1f dB\n", sub3);
        check (sub2 > inSub2 + 12.0, "period-2 growl raises f0/2 by > 12 dB");
        check (sub3 > db (mag (in, F0 / 3, SR)) - db (mag (in, F0, SR)) + 10.0,
               "period-3 guttural raises f0/3 by > 10 dB");
    }

    // ---- 3 · the vowel survives ---------------------------------------------
    {
        std::printf ("\n3 - the growl keeps the vowel; a fuzz pedal loses it\n");

        /*  THE design point of the plugin, measured head to head. Formant
            contrast = the harmonic inside F1 (h4, 660 Hz) against a
            harmonic far above the formants (h12, 1980 Hz). Distorting the
            SOURCE keeps that contrast, because the wreckage is re-dressed
            in the singer's vocal tract; distorting the WHOLE VOICE fills
            1980 Hz with intermodulation and flattens it. */
        Singer sIn (SR, F0);
        std::vector<float> in;
        for (int i = 0; i < (int) (SR * 2.2); ++i) in.push_back (sIn.next());
        const double cIn = db (mag (in, F0 * 4, SR)) - db (mag (in, F0 * 12, SR));

        auto grown = sing (growlPatch (0.85f, 0.7f), SR, F0);
        const double cGrowl = db (mag (grown, F0 * 4, SR)) - db (mag (grown, F0 * 12, SR));

        hf::Params fuzz = neutral();
        fuzz.drive = 0.9f; fuzz.tube = 0.6f;
        auto fuzzed = sing (fuzz, SR, F0);
        const double cFuzz = db (mag (fuzzed, F0 * 4, SR)) - db (mag (fuzzed, F0 * 12, SR));

        std::printf ("    formant contrast: input %.1f dB, growl %.1f dB, fuzz %.1f dB\n",
                     cIn, cGrowl, cFuzz);
        check (cGrowl > cFuzz + 6.0, "the growl keeps > 6 dB more vowel than the fuzz");
        check (cGrowl > cIn - 12.0, "the growl keeps most of the input's contrast");
        check (cGrowl > 10.0, "and the contrast is real, not relative");
    }

    // ---- 4 · the pitch survives ---------------------------------------------
    {
        std::printf ("\n4 - the melody is still there\n");
        auto grown = sing (growlPatch (0.85f, 0.7f), SR, F0);
        const double at = db (mag (grown, F0, SR));
        const double up = db (mag (grown, F0 * 1.26, SR));    // a major third off
        const double dn = db (mag (grown, F0 * 0.79, SR));
        std::printf ("    f0 %5.1f dB   +M3 %5.1f dB   -M3 %5.1f dB\n", at, up, dn);
        check (at > up + 6.0 && at > dn + 6.0, "f0 beats its off-pitch neighbours by > 6 dB");
    }

    // ---- 5 · the chaos knob walks the cascade -------------------------------
    {
        std::printf ("\n5 - one knob, the whole bifurcation cascade\n");
        struct Case { float knob; int want; const char* what; };
        const Case cases[] = {
            { 0.02f, 1, "chaos 0.02 -> steady (period 1)" },
            { 0.30f, 2, "chaos 0.30 -> period 2" },
            { 0.85f, 3, "chaos 0.85 -> period 3 (the plateau)" },
        };
        for (const auto& c : cases)
        {
            hf::Engine e;
            e.prepare (SR, 128);
            e.p = growlPatch (c.knob);
            Singer s (SR, F0);
            run (e, SR, 2.0, 1.0, [&] (int) { return s.next(); });
            std::printf ("    %-38s measured order %d\n", c.what, e.orderA);
            check (e.orderA == c.want, c.what);
        }
    }

    // ---- 6 · rasp is pulse-locked noise, not hiss ---------------------------
    {
        std::printf ("\n6 - rasp rides the voice\n");
        hf::Params p = growlPatch (0.3f, 0.0f);
        p.a.rasp = 0.8f;
        auto noisy = sing (p, SR, F0);
        const double loud = rms (noisy);

        // same patch, silence in: the noise must not free-run
        hf::Engine e;
        e.prepare (SR, 128);
        e.p = p;
        auto quiet = run (e, SR, 1.5, 0.5, [] (int) { return 0.0f; });
        std::printf ("    output with voice %.3f rms, with silence %.6f rms\n",
                     loud, rms (quiet));
        check (rms (quiet) < 1.0e-4, "silence in -> silence out, even with RASP up");
        check (loud > 0.01, "with a voice the rack is audible");
    }

    // ---- 7 · the false folds lock a subharmonic -----------------------------
    {
        std::printf ("\n7 - false folds: SPLIT locks to the chosen fraction\n");
        hf::Params p = growlPatch (0.02f, 0.0f);    // steady source, folds do the work
        p.a.split = 0.8f; p.a.ratio = 0.5f;         // aim at f0/2
        auto o = sing (p, SR, F0);
        const double lock = db (mag (o, F0 / 2, SR)) - db (mag (o, F0, SR));
        std::printf ("    f0/2 vs f0 with folds at f0/2: %6.1f dB\n", lock);
        check (lock > -6.0, "false folds put substantial energy at f0/2");
    }

    // ---- 8 · throat moves formants, not pitch -------------------------------
    {
        std::printf ("\n8 - THROAT slides the vowel and leaves the note alone\n");
        hf::Params up = growlPatch (0.02f, 0.0f);
        up.a.throat = 0.9f;
        auto small = sing (up, SR, F0);
        hf::Params dn = growlPatch (0.02f, 0.0f);
        dn.a.throat = 0.1f;
        auto big = sing (dn, SR, F0);

        // F1 at 700: throat up should move that energy above 700, down below.
        const double upHi = db (mag (small, 1000.0, SR)) - db (mag (small, 500.0, SR));
        const double dnHi = db (mag (big,   1000.0, SR)) - db (mag (big,   500.0, SR));
        std::printf ("    1000/500 balance: throat up %+.1f dB, down %+.1f dB\n", upHi, dnHi);
        check (upHi > dnHi + 6.0, "formant energy moves with the THROAT knob");

        const double pu = db (mag (small, F0, SR)) - db (mag (small, F0 * 1.26, SR));
        const double pd = db (mag (big, F0, SR)) - db (mag (big, F0 * 1.26, SR));
        check (pu > 6.0 && pd > 6.0, "the note stays put at both extremes");
    }

    // ---- 9 · the strip does strip things ------------------------------------
    {
        std::printf ("\n9 - the strip itself\n");

        // gate: -60 dB noise must be cut, a full voice must not
        {
            hf::Params p = neutral();
            p.gateOn = 1.0f; p.gateThr = 0.45f;
            hf::Engine e;
            e.prepare (SR, 128);
            e.p = p;
            hf::Rng rg;
            auto hiss = run (e, SR, 1.5, 0.5, [&] (int) { return rg.bi() * 0.001f; });
            auto voice = sing (p, SR, F0);
            std::printf ("    gate: hiss %.6f rms, voice %.3f rms\n", rms (hiss), rms (voice));
            check (rms (hiss) < 2.0e-4, "gate closes on the noise floor");
            check (rms (voice) > 0.02, "gate opens for the singer");
        }

        // de-esser: a 6 kHz ess ducks, the vowel does not
        {
            hf::Params p = neutral();
            p.deEssOn = 1.0f; p.deEssAmt = 0.8f; p.deEssFrq = 0.5f;
            hf::Engine e1;
            e1.prepare (SR, 128);
            e1.p = p;
            double ph = 0;
            auto essOut = run (e1, SR, 1.5, 0.5, [&] (int)
            {
                ph += 6000.0 / SR;
                return 0.4f * (float) std::sin (2.0 * PI * ph);
            });
            hf::Params off = p; off.deEssOn = 0.0f;
            hf::Engine e0;
            e0.prepare (SR, 128);
            e0.p = off;
            double ph0 = 0;
            auto essRef = run (e0, SR, 1.5, 0.5, [&] (int)
            {
                ph0 += 6000.0 / SR;
                return 0.4f * (float) std::sin (2.0 * PI * ph0);
            });
            const double duck = db (rms (essOut)) - db (rms (essRef));
            auto vOn  = sing (p, SR, F0);
            auto vOff = sing (off, SR, F0);
            const double vowelDuck = db (rms (vOn)) - db (rms (vOff));
            std::printf ("    de-ess: ess ducked %.1f dB, vowel ducked %.2f dB\n", duck, vowelDuck);
            check (duck < -6.0, "a bare ess is ducked > 6 dB");
            check (vowelDuck > -1.5, "the vowel is left alone (< 1.5 dB)");
        }

        // compressor: 12 dB more in must come out much less than 12 dB more
        {
            hf::Params p = neutral();
            p.compOn = 1.0f; p.cThr = 0.3f; p.cRatio = 0.6f; p.cAuto = 0.0f;
            hf::Engine eq_, el_;
            eq_.prepare (SR, 128); eq_.p = p;
            el_.prepare (SR, 128); el_.p = p;
            Singer s1 (SR, F0), s2 (SR, F0);
            auto qo = run (eq_, SR, 1.6, 0.6, [&] (int) { return s1.next() * 0.2f; });
            auto lo = run (el_, SR, 1.6, 0.6, [&] (int) { return s2.next() * 0.8f; });
            const double gain = db (rms (lo)) - db (rms (qo));
            std::printf ("    comp: 12 dB more in -> %.1f dB more out\n", gain);
            check (gain < 8.0, "compressor compresses (12 in -> < 8 out)");
        }

        // limiter: hot input, ceiling honoured
        {
            hf::Params p = neutral();
            p.limOn = 1.0f; p.limCeil = 0.5f;    // about -6 dB
            hf::Engine e;
            e.prepare (SR, 128);
            e.p = p;
            Singer s (SR, F0);
            auto o = run (e, SR, 1.6, 0.6, [&] (int) { return s.next() * 3.0f; });
            const double ceil = std::pow (10.0, (-12.0 + 0.5 * 11.7) / 20.0);
            std::printf ("    limiter: peak %.3f against ceiling %.3f\n", peakAbs (o), ceil);
            check (peakAbs (o) < ceil * 1.15, "limiter holds its ceiling (+15%% grace)");
        }
    }

    // ---- 10 · level match means BLEND is honest ------------------------------
    {
        std::printf ("\n10 - the growl does not win by being louder\n");
        auto dry = sing (neutral(), SR, F0);
        auto wet = sing (growlPatch (0.85f, 0.7f), SR, F0);
        const double d = db (rms (wet)) - db (rms (dry));
        std::printf ("    growled output vs clean: %+.1f dB\n", d);
        check (std::abs (d) < 3.5, "matched to within 3.5 dB");
    }

    // ---- 11 · every recipe, and the kitchen-sink sweeps ----------------------
    {
        std::printf ("\n11 - recipes, extremes, random, rates\n");

        for (int i = 0; i < hf::NUM_RECIPES; ++i)
        {
            hf::Params p;
            hf::applyRecipe (i, p);
            auto o = sing (p, SR, F0, 1.6, 0.5);
            const double pk = peakAbs (o), r = rms (o);
            const bool ok = pk < 1.05 && r > 1.0e-4 && std::isfinite ((float) r);
            std::printf ("    %-12s peak %.3f rms %.3f  %s\n",
                         hf::recipeName (i), pk, r, ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }

        // every parameter at both extremes, everything else at default
        int badExtremes = 0;
        for (int i = 0; i < hf::numParams(); ++i)
            for (float v : { 0.0f, 1.0f })
            {
                hf::Params p;
                const auto& sp = hf::paramSpec (i);
                sp.get (p) = sp.kind == hf::KP_OS ? v * 2.0f : v;
                auto o = sing (p, SR, F0, 1.2, 0.5);
                const double pk = peakAbs (o);
                if (! (pk < 1.05) || ! std::isfinite ((float) rms (o)))
                {
                    std::printf ("    EXTREME BAD: %s = %.0f  peak %.3f\n", sp.id, v, pk);
                    ++badExtremes;
                }
            }
        std::printf ("    %d parameters x 2 extremes\n", hf::numParams());
        check (badExtremes == 0, "every extreme finite and under +0.4 dBFS");

        // random rooms
        hf::Rng rr;
        rr.seed (0x5eedu);
        int badRandom = 0;
        double worstPk = 0;
        for (int t = 0; t < 200; ++t)
        {
            hf::Params p;
            for (int i = 0; i < hf::numParams(); ++i)
            {
                const auto& sp = hf::paramSpec (i);
                sp.get (p) = sp.kind == hf::KP_OS ? (float) (rr.u32() % 3) : rr.uni();
            }
            p.inGain = std::min (p.inGain, 0.75f);      // +12 dB in is enough cruelty
            auto o = sing (p, SR, 120.0 + 250.0 * rr.uni(), 0.9, 0.4);
            const double pk = peakAbs (o);
            worstPk = std::max (worstPk, pk);
            if (! (pk < 1.05) || ! std::isfinite ((float) rms (o))) ++badRandom;
        }
        std::printf ("    200 random settings, worst peak %.3f\n", worstPk);
        check (badRandom == 0, "200 random settings all finite and bounded");

        // rates
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            hf::Params p;
            hf::applyRecipe (4, p);
            auto o = sing (p, rate, F0, 1.2, 0.5);
            const bool ok = peakAbs (o) < 1.05 && std::isfinite ((float) rms (o)) && rms (o) > 1.0e-4;
            std::printf ("    %6.0f Hz  peak %.3f rms %.3f  %s\n",
                         rate, peakAbs (o), rms (o), ok ? "ok" : "BAD");
            if (! ok) ++failures;
        }

        // no growth: a long hold at the nastiest recipe
        {
            hf::Params p;
            hf::applyRecipe (6, p);
            hf::Engine e;
            e.prepare (SR, 128);
            e.p = p;
            Singer s (SR, F0);
            auto head = run (e, SR, 5.0, 0.5, [&] (int) { return s.next(); });
            std::vector<float> early (head.begin(), head.begin() + (int) SR);
            std::vector<float> late  (head.end() - (int) SR, head.end());
            const double g = db (rms (late)) - db (rms (early));
            std::printf ("    60 s equivalent hold: late vs early %+.2f dB\n", g);
            check (std::abs (g) < 3.0, "no growth over a long hold");
        }
    }

    std::printf ("\n%s  (%d failure%s)\n", failures ? "FAILED" : "ALL CLEAR",
                 failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
