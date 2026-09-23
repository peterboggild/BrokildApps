/*  Offline stress test for the Escape Room engine.

    The room has three feedback paths that can all be opened at once — the maw
    eating the door's output, the chasm's own loop, and a filter with Q up to
    58 — so the only question worth asking of it is whether it can be made to
    run away. This drives it at every extreme and at a few thousand random
    settings and complains about anything non-finite, anything over full
    scale, and anything that grows without stopping.
*/
#include "Engine.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    er::Rng R { 0xC0FFEEu };
    float uni() { return R.uni(); }

    struct Result { float peak = 0, rms = 0, tailGrowth = 0; bool finite = true; };

    Result run (er::Engine& e, double seconds, bool playNotes)
    {
        const int block = 256;
        const int total = (int) (48000.0 * seconds);
        std::vector<float> L ((size_t) block), Rr ((size_t) block);
        Result out;
        double sum = 0; long n = 0;
        double firstHalf = 0, secondHalf = 0; long nf = 0, ns = 0;

        for (int done = 0; done < total; done += block)
        {
            if (playNotes)
            {
                if (done == 0)             { e.noteOn (43, 0.9f); e.noteOn (50, 0.7f); }
                if (done > total / 3 && done <= total / 3 + block) { e.noteOff (43); e.noteOn (57, 1.0f); }
                if (done > total * 2 / 3 && done <= total * 2 / 3 + block) { e.allNotesOff(); }
            }
            e.process (L.data(), Rr.data(), block);
            for (int i = 0; i < block; ++i)
            {
                const float a = std::abs (L[(size_t) i]), b = std::abs (Rr[(size_t) i]);
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (Rr[(size_t) i])) out.finite = false;
                const float m = a > b ? a : b;
                if (m > out.peak) out.peak = m;
                sum += (double) m * m; ++n;
                if (done < total / 2) { firstHalf += (double) m * m; ++nf; }
                else                  { secondHalf += (double) m * m; ++ns; }
            }
        }
        out.rms = (float) std::sqrt (sum / (double) (n ? n : 1));
        const double f = std::sqrt (firstHalf / (double) (nf ? nf : 1));
        const double s = std::sqrt (secondHalf / (double) (ns ? ns : 1));
        out.tailGrowth = (float) (s / (f > 1e-7 ? f : 1e-7));
        return out;
    }

    int failures = 0;
    void check (const char* what, const Result& r, float peakMax = 1.0001f, float growthMax = 8.0f)
    {
        const bool ok = r.finite && r.peak <= peakMax && r.tailGrowth <= growthMax;
        std::printf ("%-34s peak %6.3f  rms %6.3f  growth %6.2f  %s\n",
                     what, r.peak, r.rms, r.tailGrowth,
                     ok ? "ok" : (! r.finite ? "NOT FINITE" : (r.peak > peakMax ? "OVER" : "RUNAWAY")));
        if (! ok) ++failures;
    }
}

int main()
{
    er::Engine e;
    e.prepare (48000.0, 256);

    // ---- 1 · defaults --------------------------------------------------
    check ("defaults", run (e, 4.0, false));
    e.panic();
    check ("defaults + notes", run (e, 4.0, true));

    // ---- 2 · every cell open, everything at maximum ---------------------
    {
        e.panic();
        er::Params& p = e.p;
        for (int c = 0; c < er::NUM_CELLS; ++c) { p.cell[(size_t) c] = { 1.0f, 1.0f, 1.0f, 1.0f }; }
        p.cut = 1; p.res = 1; p.key = 1; p.glide = 1; p.atk = 0; p.rel = 1;
        p.floorLvl = 1; p.envd = 1; p.twin = 1; p.twinAmt = 1; p.mode = 1;
        p.bind = 1; p.drift = 1; p.rate = 1; p.sigil = 41;
        p.shAmt = 1; p.shSize = 1; p.chTime = 1; p.chFeed = 1; p.chTone = 1;
        p.vaSize = 1; p.vaMix = 1; p.vaShim = 1; p.drive = 1; p.gain = 1;
        check ("everything at maximum", run (e, 8.0, true));
    }

    // ---- 3 · the three feedback paths, one at a time --------------------
    auto reset = [&e]()
    {
        e.panic();
        e.p = er::Params{};
    };
    {
        reset();
        e.p.cell[4] = { 1.0f, 1.0f, 0.5f, 1.0f };     // the maw, fully fed
        e.p.res = 1.0f; e.p.floorLvl = 1.0f; e.p.gain = 1.0f;
        check ("maw feedback wide open", run (e, 10.0, false));
    }
    {
        reset();
        e.p.chFeed = 1.0f; e.p.chTime = 0.05f; e.p.gain = 1.0f;  // short loop, max return
        check ("chasm at runaway feedback", run (e, 10.0, false));
    }
    {
        reset();
        e.p.res = 1.0f; e.p.cut = 0.1f; e.p.floorLvl = 1.0f;     // filter self-ringing
        e.p.mode = 1; e.p.gain = 1.0f;
        check ("door singing", run (e, 10.0, true));
    }
    {
        reset();
        e.p.vaSize = 1.0f; e.p.vaMix = 1.0f; e.p.vaShim = 1.0f; e.p.gain = 1.0f;
        check ("vapour + shimmer", run (e, 10.0, false));
    }

    // ---- 4 · silence in, silence out -----------------------------------
    {
        reset();
        for (int c = 0; c < er::NUM_CELLS; ++c) e.p.cell[(size_t) c].on = 0.0f;
        e.p.floorLvl = 0.0f;
        const auto r = run (e, 2.0, false);
        std::printf ("%-34s peak %6.4f  %s\n", "all cells sealed", r.peak,
                     r.peak < 0.02f ? "ok" : "LEAKING");
        if (! (r.peak < 0.02f)) ++failures;
    }

    // ---- 5 · every sigil, briefly --------------------------------------
    {
        float worst = 0; bool allFinite = true;
        for (int s = 0; s < 64; ++s)
        {
            reset();
            e.p.sigil = (float) s;
            e.p.bind = 1.0f; e.p.drift = 0.6f; e.p.rate = 0.8f;
            for (int c = 0; c < er::NUM_CELLS; ++c) e.p.cell[(size_t) c].on = 1.0f;
            const auto r = run (e, 1.2, true);
            if (r.peak > worst) worst = r.peak;
            if (! r.finite) { allFinite = false; std::printf ("  sigil %d NOT FINITE\n", s); }
        }
        std::printf ("%-34s peak %6.3f  %s\n", "all 64 sigils, bind at max", worst,
                     (allFinite && worst <= 1.0001f) ? "ok" : "BAD");
        if (! (allFinite && worst <= 1.0001f)) ++failures;
    }

    // ---- 6 · a few hundred random rooms --------------------------------
    {
        float worst = 0; int bad = 0;
        for (int t = 0; t < 300; ++t)
        {
            reset();
            er::Params& p = e.p;
            for (int c = 0; c < er::NUM_CELLS; ++c)
                p.cell[(size_t) c] = { uni() < 0.7f ? 1.0f : 0.0f, uni(), uni(), uni() };
            p.cut = uni(); p.res = uni(); p.key = uni(); p.glide = uni();
            p.atk = uni(); p.rel = uni(); p.floorLvl = uni(); p.envd = uni();
            p.twin = uni(); p.twinAmt = uni(); p.mode = (int) (uni() * 3.99f);
            p.bind = uni(); p.drift = uni(); p.rate = uni(); p.sigil = std::floor (uni() * 63.99f);
            p.shAmt = uni(); p.shSize = uni(); p.chTime = uni(); p.chFeed = uni(); p.chTone = uni();
            p.vaSize = uni(); p.vaMix = uni(); p.vaShim = uni(); p.drive = uni(); p.gain = uni();
            const auto r = run (e, 1.5, uni() < 0.6f);
            if (r.peak > worst) worst = r.peak;
            if (! r.finite || r.peak > 1.0001f) { ++bad; if (bad < 4) std::printf ("  random %d bad: peak %f finite %d\n", t, r.peak, (int) r.finite); }
        }
        std::printf ("%-34s peak %6.3f  bad %d/300  %s\n", "300 random rooms", worst, bad,
                     bad == 0 ? "ok" : "BAD");
        failures += bad ? 1 : 0;
    }

    // ---- 7 · sample rates ----------------------------------------------
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        er::Engine e2;
        e2.prepare (sr, 128);
        for (int c = 0; c < er::NUM_CELLS; ++c) e2.p.cell[(size_t) c] = { 1.0f, 0.6f, 0.5f, 0.6f };
        e2.p.bind = 0.7f; e2.p.res = 0.9f; e2.p.vaMix = 0.4f; e2.p.chFeed = 0.4f;
        std::vector<float> L (128), Rr (128);
        bool fin = true; float peak = 0;
        e2.noteOn (48, 0.9f);
        for (int i = 0; i < (int) (sr * 3.0 / 128.0); ++i)
        {
            e2.process (L.data(), Rr.data(), 128);
            for (int k = 0; k < 128; ++k)
            {
                if (! std::isfinite (L[(size_t) k])) fin = false;
                peak = std::max (peak, std::abs (L[(size_t) k]));
            }
        }
        std::printf ("%-34s peak %6.3f  %s\n",
                     ("sample rate " + std::to_string ((int) sr)).c_str(), peak,
                     (fin && peak <= 1.0001f) ? "ok" : "BAD");
        if (! (fin && peak <= 1.0001f)) ++failures;
    }

    // ---- 8 · the sigil really is deterministic --------------------------
    {
        er::Slot a[er::NUM_SLOTS], b[er::NUM_SLOTS];
        er::buildWiring (41, a);
        er::buildWiring (7, b);
        er::buildWiring (41, b);                       // build 41 again after a detour
        bool same = true;
        for (int i = 0; i < er::NUM_SLOTS; ++i)
            if (a[i].src != b[i].src || a[i].dst != b[i].dst
                || a[i].curve != b[i].curve || a[i].depth != b[i].depth) same = false;

        // and that the 64 rooms are actually different from one another
        int identical = 0;
        for (int i = 0; i < 64; ++i)
            for (int j = i + 1; j < 64; ++j)
            {
                er::Slot x[er::NUM_SLOTS], y[er::NUM_SLOTS];
                er::buildWiring (i, x); er::buildWiring (j, y);
                bool eq = true;
                for (int k = 0; k < er::NUM_SLOTS; ++k)
                    if (x[k].src != y[k].src || x[k].dst != y[k].dst) { eq = false; break; }
                if (eq) ++identical;
            }
        std::printf ("%-34s repeatable %d  duplicate pairs %d  %s\n", "the lock", (int) same,
                     identical, (same && identical == 0) ? "ok" : "BAD");
        if (! (same && identical == 0)) ++failures;
    }

    // ---- 9 · a played note really does tune the resonance ---------------
    {
        auto goertzel = [] (const std::vector<float>& x, double f, double srate)
        {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos (w);
            double s1 = 0, s2 = 0;
            for (float v : x) { const double s0 = (double) v + c * s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (std::abs (s1 * s1 + s2 * s2 - c * s1 * s2));
        };

        er::Engine e3;
        e3.prepare (48000.0, 256);
        e3.p = er::Params{};
        for (int c = 0; c < er::NUM_CELLS; ++c) e3.p.cell[(size_t) c].on = 0.0f;
        e3.p.cell[2] = { 1.0f, 0.7f, 0.5f, 0.2f };      // wideband noise, nothing else
        e3.p.res = 0.95f; e3.p.key = 1.0f; e3.p.mode = 1; e3.p.floorLvl = 0.0f;
        e3.p.bind = 0.0f; e3.p.drift = 0.0f; e3.p.vaMix = 0.0f; e3.p.envd = 0.5f;
        e3.p.atk = 0.0f; e3.p.glide = 0.0f; e3.p.drive = 0.0f; e3.p.twinAmt = 0.0f;

        const int note = 57;                             // A3
        const float want = 440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f);
        e3.noteOn (note, 1.0f);
        std::vector<float> L (256), Rr (256), acc;
        for (int i = 0; i < 400; ++i)
        {
            e3.process (L.data(), Rr.data(), 256);
            if (i > 60) acc.insert (acc.end(), L.begin(), L.end());
        }
        const double at   = goertzel (acc, want, 48000.0);
        const double up   = goertzel (acc, want * 1.5, 48000.0);
        const double down = goertzel (acc, want / 1.5, 48000.0);
        const bool ok = at > up * 3.0 && at > down * 3.0;
        std::printf ("%-34s %.0f Hz beats its neighbours %.1fx / %.1fx  %s\n",
                     "note 57 tunes the door", want,
                     at / (up > 1e-9 ? up : 1e-9), at / (down > 1e-9 ? down : 1e-9),
                     ok ? "ok" : "BAD");
        if (! ok) ++failures;
    }

    // ---- the SPECTRA world-mod bus (BWFX) --------------------------------
    std::printf ("\n[world-mod bus]\n");
    auto goertzelBus = [] (const std::vector<float>& x, double f, double srate)
    {
        const double w = 2.0 * 3.14159265358979 * f / srate;
        const double c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (float v : x) { const double s0 = (double) v + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (std::abs (s1 * s1 + s2 * s2 - c * s1 * s2));
    };
    {
        // neutral bus == bit-identical to never calling setWorldMod
        er::Engine a, b;
        a.prepare (48000.0, 256); b.prepare (48000.0, 256);
        a.setSigil (7); b.setSigil (7);
        b.setWorldMod (0, 0, 0, 0, 0, 1);
        std::vector<float> La, Ra, Lb, Rb, accA, accB;
        La.resize (256); Ra.resize (256); Lb.resize (256); Rb.resize (256);
        for (int i = 0; i < 200; ++i)
        {
            a.process (La.data(), Ra.data(), 256);
            b.process (Lb.data(), Rb.data(), 256);
            accA.insert (accA.end(), La.begin(), La.end());
            accB.insert (accB.end(), Lb.begin(), Lb.end());
        }
        const bool same = std::memcmp (accA.data(), accB.data(), accA.size() * sizeof (float)) == 0;
        std::printf ("%-34s %s\n", "neutral bus bit-identical", same ? "ok" : "DIFFERS");
        if (! same) ++failures;
    }
    {
        // sag keys to the door voice's gate: the played resonance sinks a
        // semitone as the voice releases (the tuned-door probe, saggy twin)
        er::Engine e3;
        e3.prepare (48000.0, 256);
        e3.p = er::Params{};
        for (int c = 0; c < er::NUM_CELLS; ++c) e3.p.cell[(size_t) c].on = 0.0f;
        e3.p.cell[2] = { 1.0f, 0.7f, 0.5f, 0.2f };
        e3.p.res = 0.95f; e3.p.key = 1.0f; e3.p.mode = 1; e3.p.floorLvl = 0.0f;
        e3.p.bind = 0.0f; e3.p.drift = 0.0f; e3.p.vaMix = 0.0f; e3.p.envd = 0.5f;
        e3.p.atk = 0.0f; e3.p.glide = 0.0f; e3.p.drive = 0.0f; e3.p.twinAmt = 0.0f;
        e3.p.rel = 0.8f;
        e3.setWorldMod (0, 0, 0, 0, 1.0f, 1);
        const float want = 440.0f * std::pow (2.0f, (57.0f - 69.0f) / 12.0f);   // 220
        const float flat = want * std::pow (2.0f, -100.0f / 1200.0f);           // -100 c
        e3.noteOn (57, 1.0f);
        std::vector<float> L (256), Rr (256), held, tail;
        for (int i = 0; i < 400; ++i)
        {
            e3.process (L.data(), Rr.data(), 256);
            if (i > 60 && i < 180) held.insert (held.end(), L.begin(), L.end());
        }
        e3.noteOff (57);
        for (int i = 0; i < 400; ++i)
        {
            e3.process (L.data(), Rr.data(), 256);
            if (i > 120) tail.insert (tail.end(), L.begin(), L.end());
        }
        const double heldAt = goertzelBus (held, want, 48000.0);
        const double heldFl = goertzelBus (held, flat, 48000.0);
        const double tailAt = goertzelBus (tail, want, 48000.0);
        const double tailFl = goertzelBus (tail, flat, 48000.0);
        const bool ok = heldAt > heldFl * 2.0 && tailFl > tailAt;
        std::printf ("%-34s held %.2e/%.2e tail %.2e/%.2e  %s\n", "door sag: in tune held, -100c tail",
                     heldAt, heldFl, tailAt, tailFl, ok ? "ok" : "WRONG");
        if (! ok) ++failures;
    }
    {
        // filterMul moves the free door; extreme bus stays bounded
        er::Engine a, b;
        a.prepare (48000.0, 256); b.prepare (48000.0, 256);
        a.setSigil (3); b.setSigil (3);
        b.setWorldMod (0, 0, 0, 0, 0, 0.25f);
        std::vector<float> La (256), Ra (256), Lb (256), Rb (256);
        double diff = 0; long n = 0;
        for (int i = 0; i < 200; ++i)
        {
            a.process (La.data(), Ra.data(), 256);
            b.process (Lb.data(), Rb.data(), 256);
            if (i > 40) for (int k = 0; k < 256; ++k) { diff += std::abs ((double) La[(size_t) k] - Lb[(size_t) k]); ++n; }
        }
        diff /= (double) n;
        std::printf ("%-34s mean|delta| %.5f  %s\n", "filterMul moves the door", diff, diff > 1e-5 ? "ok" : "INERT");
        if (diff <= 1e-5) ++failures;

        er::Engine x;
        x.prepare (48000.0, 256);
        x.setSigil (42);
        x.setWorldMod (60.0f, 1.0f, 1.0f, 5.0f, 1.0f, 4.0f);
        check ("extreme bus", run (x, 6.0, true), 1.0001f, 10.0f);
    }

    std::printf ("\n%s  (%d failure%s)\n", failures ? "FAILED" : "ALL CLEAR",
                 failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
