/*  Offline bench for Blade Ruiner.

    Three synthesisers, one of which never stops and one of which runs its
    own clock, is a lot of state to leave running unattended. Everything here
    is a measurement rather than an assertion about the source: render real
    audio, and look at it.                                                   */

#include "Engine.h"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace br;

namespace
{
    int failures = 0;

    struct Stat { float peak = 0.0f; double rms = 0.0; bool finite = true; };

    Stat measure (const std::vector<float>& l, const std::vector<float>& r, size_t from = 0)
    {
        Stat s;
        double acc = 0.0;
        size_t n = 0;
        for (size_t i = from; i < l.size(); ++i, ++n)
        {
            const float a = l[i], b = r[i];
            if (! std::isfinite (a) || ! std::isfinite (b)) s.finite = false;
            s.peak = std::max (s.peak, std::max (std::abs (a), std::abs (b)));
            acc += (double) a * a + (double) b * b;
        }
        s.rms = n ? std::sqrt (acc / (2.0 * (double) n)) : 0.0;
        return s;
    }

    /*  Render seconds of audio in realistic block sizes, so any state that
        only misbehaves across a block boundary gets the chance. */
    Stat render (Engine& e, double sr, float seconds,
                 std::vector<float>* keepL = nullptr, std::vector<float>* keepR = nullptr,
                 int noteOnAt = -1, int noteOffAt = -1, int note = 60)
    {
        const int total = (int) (sr * seconds);
        std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
        int done = 0, blk = 0;
        static const int SIZES[5] = { 64, 128, 256, 480, 512 };
        while (done < total)
        {
            const int n = std::min (SIZES[blk++ % 5], total - done);
            if (noteOnAt  >= 0 && done <= (int) (sr * noteOnAt  / 1000.0) && done + n > (int) (sr * noteOnAt  / 1000.0))
                e.noteOn (note, 0.9f);
            if (noteOffAt >= 0 && done <= (int) (sr * noteOffAt / 1000.0) && done + n > (int) (sr * noteOffAt / 1000.0))
                e.noteOff (note);
            e.process (L.data() + done, R.data() + done, n);
            done += n;
        }
        auto s = measure (L, R);
        if (keepL) *keepL = L;
        if (keepR) *keepR = R;
        return s;
    }

    void check (const std::string& what, const Stat& s, float peakMax = 1.0f, bool wantSound = true)
    {
        const bool ok = s.finite && s.peak <= peakMax + 1.0e-4f && (! wantSound || s.rms > 1.0e-4);
        std::printf ("%-46s peak %.3f  rms %.4f  %s\n", what.c_str(), s.peak, s.rms,
                     ok ? "ok" : (! s.finite ? "NOT FINITE" : (s.peak > peakMax ? "TOO LOUD" : "SILENT")));
        if (! ok) ++failures;
    }

    /*  Goertzel: how much energy sits at one frequency. Used to prove that a
        drone is actually where it says it is. */
    double goertzel (const std::vector<float>& x, double sr, double hz, size_t from, size_t count)
    {
        const double w = 2.0 * 3.14159265358979323846 * hz / sr;
        const double c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = from; i < from + count && i < x.size(); ++i)
        {
            const double s0 = (double) x[i] + c * s1 - s2;
            s2 = s1; s1 = s0;
        }
        return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) count;
    }

    void soloLayer (Engine& e, int which)
    {
        e.p.laOn = which == 0 ? 1.0f : 0.0f;
        e.p.dkOn = which == 1 ? 1.0f : 0.0f;
        e.p.rpOn = which == 2 ? 1.0f : 0.0f;
    }
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::string (argv[1]) == "--moods")
    {
        for (int i = 0; i < NUM_MOODS; ++i)
            std::printf ("%3d %s %s\n", i, moodIsWritten (i) ? "*" : " ", moodLine (i).c_str());
        return 0;
    }

    const double SR = 48000.0;
    std::printf ("\nBLADE RUINER  --  offline bench\n");
    std::printf ("================================================================\n\n");

    // ---------------------------------------------------------------- 1
    std::printf ("-- defaults ----------------------------------------------------\n");
    {
        Engine e;
        e.prepare (SR, 512);
        check ("defaults, 12 s", render (e, SR, 12.0f));
    }

    // ---------------------------------------------------------------- 2
    std::printf ("\n-- each layer alone --------------------------------------------\n");
    {
        const char* names[3] = { "LOS ANGELES alone", "DECKARD alone (one note)", "REPLICANT alone" };
        for (int i = 0; i < 3; ++i)
        {
            Engine e;
            e.prepare (SR, 512);
            soloLayer (e, i);
            e.p.bpm = 112.0;
            const int on  = i == 1 ? 200  : -1;
            const int off = i == 1 ? 4000 : -1;
            check (names[i], render (e, SR, 8.0f, nullptr, nullptr, on, off, 55));
        }
    }

    // ---------------------------------------------------------------- 3
    std::printf ("\n-- Deckard: silent with no keys, and stops when released -------\n");
    {
        Engine e;
        e.prepare (SR, 512);
        soloLayer (e, 1);
        check ("no notes held", render (e, SR, 3.0f), 1.0f, false);

        std::vector<float> L, R;
        e.reset();
        e.p.dkRel = 0.15f;                      // a short release, so it really ends
        render (e, SR, 10.0f, &L, &R, 100, 1500, 60);
        const auto head = measure (L, R, (size_t) (SR * 0.6));
        std::vector<float> tailL (L.begin() + (size_t) (SR * 8.0), L.end());
        std::vector<float> tailR (R.begin() + (size_t) (SR * 8.0), R.end());
        const auto tail = measure (tailL, tailR);
        std::printf ("%-46s held rms %.4f  tail rms %.6f  %s\n", "note decays to nothing",
                     head.rms, tail.rms, (head.rms > 1.0e-3 && tail.rms < 1.0e-4) ? "ok" : "STILL RINGING");
        if (! (head.rms > 1.0e-3 && tail.rms < 1.0e-4)) ++failures;
    }

    // ---------------------------------------------------------------- 4
    std::printf ("\n-- the drone is where it says it is ----------------------------\n");
    {
        // root A1 = 55 Hz, octave voicing, no drift, no sprawl: it should be exact
        Engine e;
        e.prepare (SR, 512);
        soloLayer (e, 0);
        e.p.laSprawl = 0.0f; e.p.laDrift = 0.0f; e.p.laRain = 0.0f;
        e.p.laKipple = 0.0f; e.p.laNeon = 0.0f; e.p.laSmog = 0.75f;
        e.p.laChord = 0.0f; e.p.laRoot = 0.0f;

        std::vector<float> L, R;
        render (e, SR, 6.0f, &L, &R);
        const size_t from = (size_t) (SR * 4.0), count = (size_t) (SR * 1.0);
        const double at55 = goertzel (L, SR, 55.0,  from, count);
        const double at58 = goertzel (L, SR, 58.27, from, count);   // a semitone up
        std::printf ("%-46s 55 Hz %.5f  vs semitone up %.5f  %s\n", "A1 drone sits on 55 Hz",
                     at55, at58, at55 > at58 * 4.0 ? "ok" : "OFF PITCH");
        if (! (at55 > at58 * 4.0)) ++failures;

        // and the global tune really moves it
        Engine e2;
        e2.prepare (SR, 512);
        e2.p = e.p;
        e2.p.tune = 1.0f;                                       // +100 cents
        std::vector<float> L2, R2;
        render (e2, SR, 6.0f, &L2, &R2);
        const double t55 = goertzel (L2, SR, 55.0,  from, count);
        const double t58 = goertzel (L2, SR, 58.27, from, count);
        std::printf ("%-46s 55 Hz %.5f  vs 58.27 Hz %.5f  %s\n", "+100 cents moves it a semitone",
                     t55, t58, t58 > t55 * 4.0 ? "ok" : "TUNE DID NOT TAKE");
        if (! (t58 > t55 * 4.0)) ++failures;
    }

    // ---------------------------------------------------------------- 5
    std::printf ("\n-- all 64 Nexus seeds ------------------------------------------\n");
    {
        int bad = 0;
        float worst = 0.0f;
        std::vector<std::string> fingerprints;
        for (int seed = 0; seed < 64; ++seed)
        {
            Engine e;
            e.prepare (SR, 512);
            soloLayer (e, 2);
            e.p.rpNexus = (float) seed;
            e.p.bpm = 128.0;
            e.p.rpRate = 5.0f;                                   // 1/16, so a bar goes by quickly
            const auto s = render (e, SR, 3.0f);
            worst = std::max (worst, s.peak);
            if (! s.finite || s.peak > 1.0f + 1.0e-4f || s.rms < 1.0e-5) ++bad;
        }
        std::printf ("%-46s worst peak %.3f  bad %d/64  %s\n", "every seed bounded and audible",
                     worst, bad, bad == 0 ? "ok" : "FAILED");
        if (bad) ++failures;
    }

    // ---------------------------------------------------------------- 6
    std::printf ("\n-- the seeds are actually different ----------------------------\n");
    {
        // fingerprint the pattern by the RMS envelope of the first bar
        std::vector<std::string> fp;
        for (int seed = 0; seed < 64; ++seed)
        {
            Engine e;
            e.prepare (SR, 512);
            soloLayer (e, 2);
            e.p.rpNexus = (float) seed;
            e.p.bpm = 120.0;
            e.p.rpRate = 5.0f;
            e.p.rpSpace = 0.0f;
            e.p.rpDecay = 0.1f;
            std::vector<float> L, R;
            render (e, SR, 2.0f, &L, &R);
            std::string s;
            for (int k = 0; k < 32; ++k)
            {
                const size_t a = (size_t) (SR * 2.0 * k / 32.0);
                const size_t b = (size_t) (SR * 2.0 * (k + 1) / 32.0);
                double acc = 0.0;
                for (size_t i = a; i < b && i < L.size(); ++i) acc += (double) L[i] * L[i];
                s += (char) ('a' + std::min (25, (int) (std::sqrt (acc / (double) (b - a)) * 260.0)));
            }
            fp.push_back (s);
        }
        int dupes = 0;
        for (size_t i = 0; i < fp.size(); ++i)
            for (size_t j = i + 1; j < fp.size(); ++j)
                if (fp[i] == fp[j]) ++dupes;
        std::printf ("%-46s duplicate pairs %d/2016  %s\n", "64 seeds, 64 machines",
                     dupes, dupes == 0 ? "ok" : "SEEDS COLLIDE");
        if (dupes) ++failures;
    }

    // ---------------------------------------------------------------- 7
    std::printf ("\n-- every control at both ends ----------------------------------\n");
    {
        float* knobs[] = {
            nullptr
        };
        (void) knobs;

        Engine e;
        e.prepare (SR, 512);
        int bad = 0;
        float worst = 0.0f;

        // walk a pointer over the whole Params block, one float at a time
        Params base;
        base.rpOn = 1.0f;
        const size_t nFloats = (offsetof (Params, bpm)) / sizeof (float);
        for (size_t i = 0; i < nFloats; ++i)
            for (int end = 0; end < 2; ++end)
            {
                Engine x;
                x.prepare (SR, 256);
                x.p = base;
                float* f = reinterpret_cast<float*> (&x.p);
                // keep the list-valued ones legal: they are raw values, not 0..1
                f[i] = end ? 1.0f : 0.0f;
                x.p.laOn = 1.0f; x.p.dkOn = 1.0f; x.p.rpOn = 1.0f;
                x.p.bpm = 140.0;
                const auto s = render (x, SR, 1.5f, nullptr, nullptr, 100, 900, 57);
                worst = std::max (worst, s.peak);
                if (! s.finite || s.peak > 1.02f) { ++bad;
                    std::printf ("   float #%zu at %s: peak %.3f finite=%d\n",
                                 i, end ? "1" : "0", s.peak, (int) s.finite); }
            }
        std::printf ("%-46s worst peak %.3f  bad %d  %s\n", "single control extremes",
                     worst, bad, bad == 0 ? "ok" : "FAILED");
        if (bad) ++failures;
    }

    // ---------------------------------------------------------------- 8
    std::printf ("\n-- 300 random machines -----------------------------------------\n");
    {
        std::mt19937 rng (20260822u);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        int bad = 0;
        float worst = 0.0f;
        for (int t = 0; t < 300; ++t)
        {
            Engine e;
            e.prepare (SR, 256);
            float* f = reinterpret_cast<float*> (&e.p);
            const size_t nFloats = (offsetof (Params, bpm)) / sizeof (float);
            for (size_t i = 0; i < nFloats; ++i) f[i] = u (rng);
            // the list-valued controls carry raw values
            e.p.laChord = (float) (rng() % NUM_CHORDS);
            e.p.laRoot  = (float) ((int) (rng() % 25) - 12);
            e.p.dkWave  = (float) (rng() % NUM_WAVES);
            e.p.dkOct   = (float) ((int) (rng() % 5) - 2);
            e.p.rpRate  = (float) (rng() % NUM_RATES);
            e.p.rpNexus = (float) (rng() % 64);
            e.p.rpSteps = (float) (4 + rng() % 13);
            e.p.rpOct   = (float) ((int) (rng() % 5) - 2);
            e.p.laOn = 1.0f; e.p.dkOn = 1.0f; e.p.rpOn = 1.0f;
            e.p.limiter = 1.0f;
            e.p.bpm = 60.0 + (double) (rng() % 140);
            const auto s = render (e, SR, 2.0f, nullptr, nullptr, 60, 1200, 45 + (int) (rng() % 30));
            worst = std::max (worst, s.peak);
            if (! s.finite || s.peak > 1.02f) ++bad;
        }
        std::printf ("%-46s worst peak %.3f  bad %d/300  %s\n", "random settings, all layers live",
                     worst, bad, bad == 0 ? "ok" : "FAILED");
        if (bad) ++failures;
    }

    // ---------------------------------------------------------------- 9
    std::printf ("\n-- sample rates ------------------------------------------------\n");
    {
        const double rates[4] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double sr : rates)
        {
            Engine e;
            e.prepare (sr, 512);
            e.p.rpOn = 1.0f;
            e.p.bpm = 120.0;
            std::vector<float> L, R;
            const auto s = render (e, sr, 4.0f, &L, &R, 200, 2500, 55);
            const double at55 = goertzel (L, sr, 55.0, (size_t) (sr * 3.0), (size_t) (sr * 0.8));
            std::printf ("%-30s %6.0f Hz   peak %.3f  55 Hz %.5f  %s\n", "all three layers",
                         sr, s.peak, at55,
                         (s.finite && s.peak <= 1.001f && at55 > 1.0e-4) ? "ok" : "FAILED");
            if (! (s.finite && s.peak <= 1.001f && at55 > 1.0e-4)) ++failures;
        }
    }

    // ---------------------------------------------------------------- 10
    std::printf ("\n-- nothing creeps up over a long run ---------------------------\n");
    {
        Engine e;
        e.prepare (SR, 512);
        e.p.laOn = 1.0f; e.p.dkOn = 1.0f; e.p.rpOn = 1.0f;
        e.p.laKipple = 0.9f; e.p.laNeon = 0.9f; e.p.laDecay = 0.98f;
        e.p.rpMenace = 0.9f; e.p.rpGlitch = 0.8f;
        e.p.bpm = 96.0;
        std::vector<float> L, R;
        render (e, SR, 60.0f, &L, &R, 500, 30000, 48);

        std::vector<float> aL (L.begin() + (size_t) (SR * 5),  L.begin() + (size_t) (SR * 10));
        std::vector<float> aR (R.begin() + (size_t) (SR * 5),  R.begin() + (size_t) (SR * 10));
        std::vector<float> bL (L.begin() + (size_t) (SR * 54), L.begin() + (size_t) (SR * 59));
        std::vector<float> bR (R.begin() + (size_t) (SR * 54), R.begin() + (size_t) (SR * 59));
        const auto a = measure (aL, aR), b = measure (bL, bR);
        const double ratio = b.rms / std::max (1.0e-9, a.rms);
        std::printf ("%-46s 5-10 s %.4f  54-59 s %.4f  x%.2f  %s\n", "shimmer + kipple, 60 s",
                     a.rms, b.rms, ratio, (ratio < 2.5 && b.finite) ? "ok" : "RUNAWAY");
        if (! (ratio < 2.5 && b.finite)) ++failures;
    }

    // ---------------------------------------------------------------- 11
    std::printf ("\n-- the last layer cannot be switched off -----------------------\n");
    {
        // the engine itself does not enforce it - the processor does - but a
        // machine with nothing running must at least be silent, not broken
        Engine e;
        e.prepare (SR, 512);
        e.p.laOn = e.p.dkOn = e.p.rpOn = 0.0f;
        const auto s = render (e, SR, 2.0f);
        std::printf ("%-46s peak %.5f  %s\n", "all layers off is silence",
                     s.peak, (s.finite && s.peak < 1.0e-4f) ? "ok" : "NOT SILENT");
        if (! (s.finite && s.peak < 1.0e-4f)) ++failures;
    }

    // ---------------------------------------------------------------- 12
    std::printf ("\n-- the mood organ ----------------------------------------------\n");
    {
        /*  A seed is only worth writing down if it means the same thing
            tomorrow. Everything here is about that. */
        int notDeterministic = 0, wrongSize = 0, outOfRange = 0;
        std::vector<std::string> lines;
        std::vector<std::string> patches;

        for (int seed = 0; seed < NUM_MOODS; ++seed)
        {
            const auto a1 = moodPatch (seed);
            const auto a2 = moodPatch (seed);
            if (a1.size() != a2.size()) { ++notDeterministic; continue; }
            for (size_t i = 0; i < a1.size(); ++i)
                if (std::string (a1[i].id) != a2[i].id || a1[i].v != a2[i].v) { ++notDeterministic; break; }

            if (a1.size() != moodPatch (0).size()) ++wrongSize;

            std::string fp;
            for (const auto& mv : a1)
            {
                if (! std::isfinite (mv.v)) ++outOfRange;
                const std::string id (mv.id);
                // the list-valued ones carry raw values; everything else is 0..1
                const bool listy = id == "laroot" || id == "lachord" || id == "dkwave"
                                || id == "dkoct"  || id == "rprate"  || id == "rpnexus"
                                || id == "rpsteps"|| id == "rpoct";
                if (! listy && (mv.v < -0.001f || mv.v > 1.001f)) ++outOfRange;
                if (id == "laroot"  && (mv.v < -12 || mv.v > 12)) ++outOfRange;
                if (id == "lachord" && (mv.v < 0 || mv.v >= NUM_CHORDS)) ++outOfRange;
                if (id == "dkwave"  && (mv.v < 0 || mv.v >= NUM_WAVES)) ++outOfRange;
                if (id == "rprate"  && (mv.v < 0 || mv.v >= NUM_RATES)) ++outOfRange;
                if (id == "rpnexus" && (mv.v < 0 || mv.v > 63)) ++outOfRange;
                if (id == "rpsteps" && (mv.v < 4 || mv.v > 16)) ++outOfRange;
                if ((id == "dkoct" || id == "rpoct") && (mv.v < -2 || mv.v > 2)) ++outOfRange;
                fp += id + "=" + std::to_string ((int) std::lround (mv.v * 1000.0f)) + ";";
            }
            patches.push_back (fp);

            const auto l1 = moodLine (seed), l2 = moodLine (seed);
            if (l1 != l2) ++notDeterministic;
            lines.push_back (l1);
        }

        std::printf ("%-46s %d/%d  %s\n", "same seed, same patch and same line",
                     NUM_MOODS - notDeterministic, NUM_MOODS,
                     notDeterministic == 0 ? "ok" : "NOT DETERMINISTIC");
        if (notDeterministic) ++failures;

        std::printf ("%-46s %d bad  %s\n", "every value inside its own range",
                     outOfRange, (outOfRange == 0 && wrongSize == 0) ? "ok" : "OUT OF RANGE");
        if (outOfRange || wrongSize) ++failures;

        auto distinct = [] (std::vector<std::string> v) {
            std::sort (v.begin(), v.end());
            v.erase (std::unique (v.begin(), v.end()), v.end());
            return (int) v.size();
        };
        const int dl = distinct (lines), dp = distinct (patches);
        std::printf ("%-46s %d/%d distinct  %s\n", "a thousand different lines of text",
                     dl, NUM_MOODS, dl >= 950 ? "ok" : "TOO MANY REPEATS");
        if (dl < 950) ++failures;
        std::printf ("%-46s %d/%d distinct  %s\n", "a thousand different patches",
                     dp, NUM_MOODS, dp == NUM_MOODS ? "ok" : "PATCHES COLLIDE");
        if (dp != NUM_MOODS) ++failures;

        // the five that are Dick's must survive the generator
        const bool dick = moodLine (481) == "awareness of the manifold possibilities open to me in the future"
                       && moodLine (382) == "six hours of self-accusatory depression"
                       && moodLine (888) == "the desire to watch television, no matter what is on it"
                       && moodIsWritten (3) && moodIsWritten (594) && ! moodIsWritten (0);
        std::printf ("%-46s %s\n", "the written ones are not overwritten",
                     dick ? "ok" : "FAILED");
        if (! dick) ++failures;

        /*  A mood that needs a keyboard before it says anything reads as a
            broken plugin rather than as a mood. Every one of the thousand has
            to speak on its own. */
        int mute = 0;
        for (int seed = 0; seed < NUM_MOODS; ++seed)
        {
            bool laOn = false, laKeyed = false, rpOn = false, rpKeyed = false;
            for (const auto& mv : moodPatch (seed))
            {
                const std::string id (mv.id);
                if (id == "laon")   laOn    = mv.v > 0.5f;
                if (id == "lagate") laKeyed = mv.v > 0.5f;
                if (id == "rpon")   rpOn    = mv.v > 0.5f;
                if (id == "rpgate") rpKeyed = mv.v > 0.5f;
            }
            if (! ((laOn && ! laKeyed) || (rpOn && ! rpKeyed))) ++mute;
        }
        std::printf ("%-46s %d silent  %s\n", "every mood makes a sound on its own",
                     mute, mute == 0 ? "ok" : "SOME MOODS SAY NOTHING");
        if (mute) ++failures;

        std::printf ("   481 -> %s\n", moodLine (481).c_str());
        for (int seed : { 0, 17, 250, 613, 999 })
            std::printf ("   %3d -> %s\n", seed, moodLine (seed).c_str());
    }

    // ---- the SPECTRA world-mod bus (BWFX) --------------------------------
    std::printf ("\n[world-mod bus]\n");
    {
        // neutral bus == bit-identical to never calling setWorldMod
        Engine a, b;
        a.prepare (SR, 512); b.prepare (SR, 512);
        b.setWorldMod (0, 0, 0, 0, 0, 1);
        std::vector<float> La, Lb;
        render (a, SR, 2.0f, &La, nullptr);
        render (b, SR, 2.0f, &Lb, nullptr);
        const bool same = std::memcmp (La.data(), Lb.data(), La.size() * sizeof (float)) == 0;
        std::printf ("%-46s %s\n", "neutral bus bit-identical", same ? "ok" : "DIFFERS");
        if (! same) ++failures;
    }
    {
        // DECKARD sag keys to the gate: in tune held, flat as it dies
        Engine e;
        e.prepare (SR, 512);
        soloLayer (e, 1);
        e.p.dkRel = 0.55f; e.p.dkDetune = 0.0f; e.p.dkVib = 0.0f;
        e.setWorldMod (0, 0, 0, 0, 1.0f, 1);
        std::vector<float> L;
        render (e, SR, 4.0f, &L, nullptr, 0, 1500, 69);
        const double heldA  = goertzel (L, SR, 440.0, (size_t) (SR * 0.7), (size_t) (SR * 0.6));
        const double heldB  = goertzel (L, SR, 415.3, (size_t) (SR * 0.7), (size_t) (SR * 0.6));
        const double lateA  = goertzel (L, SR, 440.0, (size_t) (SR * 2.4), (size_t) (SR * 0.8));
        const double lateB  = goertzel (L, SR, 415.3, (size_t) (SR * 2.4), (size_t) (SR * 0.8));
        const bool ok = heldA > heldB * 3.0 && lateB > lateA;
        std::printf ("%-46s held 440:%.2e vs 415:%.2e, tail 440:%.2e vs 415:%.2e %s\n",
                     "deckard sag: in tune held, -100c in the tail", heldA, heldB, lateA, lateB, ok ? "ok" : "WRONG");
        if (! ok) ++failures;
    }
    {
        // LA: detune fans over the stack (render differs), filterMul darkens
        Engine a, b;
        a.prepare (SR, 512); b.prepare (SR, 512);
        soloLayer (a, 0); soloLayer (b, 0);
        b.setWorldMod (30.0f, 0, 0, 0, 0, 1);
        std::vector<float> La, Lb;
        render (a, SR, 2.0f, &La, nullptr);
        render (b, SR, 2.0f, &Lb, nullptr);
        double diff = 0;
        for (size_t i = (size_t) SR; i < La.size(); ++i) diff += std::abs ((double) La[i] - Lb[i]);
        diff /= (double) SR;
        std::printf ("%-46s mean|delta| %.5f %s\n", "LA stack detune does something", diff, diff > 1e-4 ? "ok" : "INERT");
        if (diff <= 1e-4) ++failures;

        // filterMul: probe the BARE drone — kipple grains and rain enter
        // AFTER the smog filter and would floor the HF measurement
        auto bareLA = [&] (Engine& e)
        {
            e.prepare (SR, 512);
            soloLayer (e, 0);
            e.p.laKipple = 0.0f; e.p.laRain = 0.0f; e.p.laSub = 0.0f;
            e.p.laDrift = 0.0f;  e.p.laNeon = 0.0f; e.p.laSmog = 0.3f;
        };
        Engine c, d;
        bareLA (c); bareLA (d);
        d.setWorldMod (0, 0, 0, 0, 0, 0.25f);
        std::vector<float> Lc, Ld;
        render (c, SR, 2.0f, &Lc, nullptr);
        render (d, SR, 2.0f, &Ld, nullptr);
        double hfC = 0, hfD = 0;
        for (double hz : { 1800.0, 2600.0, 3400.0 })
        {
            hfC += goertzel (Lc, SR, hz, (size_t) SR, (size_t) SR);
            hfD += goertzel (Ld, SR, hz, (size_t) SR, (size_t) SR);
        }
        std::printf ("%-46s %.2e -> %.2e %s\n", "LA filterMul darkens", hfC, hfD, hfD < hfC * 0.7 ? "ok" : "WRONG");
        if (! (hfD < hfC * 0.7)) ++failures;
    }
    {
        // whole-machine tremolo breathes, and the extreme bus stays bounded
        Engine e;
        e.prepare (SR, 512);
        e.p.laOn = 1.0f; e.p.dkOn = 1.0f; e.p.rpOn = 1.0f;
        e.setWorldMod (60.0f, 1.0f, 1.0f, 5.0f, 1.0f, 4.0f);
        std::vector<float> L;
        const auto s = render (e, SR, 3.0f, &L, nullptr, 0, 2000, 57);
        double mn = 1e9, mx = 0;
        for (size_t w = (size_t) SR; w + 4800 < L.size(); w += 4800)
        {
            double acc = 0;
            for (size_t i = w; i < w + 4800; ++i) acc += (double) L[i] * L[i];
            const double v = std::sqrt (acc / 4800.0);
            mn = std::min (mn, v); mx = std::max (mx, v);
        }
        const bool ok = s.finite && s.peak < 1.4f && mn < mx * 0.7;
        std::printf ("%-46s rms %.4f..%.4f peak %.2f %s\n", "extreme bus: trem breathes, bounded", mn, mx, s.peak, ok ? "ok" : "WRONG");
        if (! ok) ++failures;
    }

    std::printf ("\n================================================================\n");
    std::printf (failures == 0 ? "ALL CLEAR  (0 failures)\n\n" : "%d FAILURES\n\n", failures);
    return failures == 0 ? 0 : 1;
}
