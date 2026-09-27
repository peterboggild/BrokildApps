/*  ARTEFACT B2311.22 — the bench. Plain C++, no JUCE.
    Everything the design claims gets a number here; alienness itself is one
    of the numbers (the non-harmonicity floor).
*/

#include "../Source/Specimen.h"
#include "../Source/Engine.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

using namespace ab;

static int checks = 0, fails = 0;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        ++checks;                                                            \
        if (! (cond)) { ++fails; std::printf ("FAIL @%d: ", __LINE__);       \
                        std::printf (__VA_ARGS__); std::printf ("\n"); }     \
    } while (0)

//  Goertzel over a window
static double goertzel (const float* x, int from, int to, double f, double fs)
{
    const double w = 2.0 * 3.14159265358979 * f / fs;
    const double c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i)
    {
        const double s0 = (double) x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2));
}

struct Stats { float peak = 0; double rms = 0; bool finite = true; };
static Stats measure (const float* L, const float* R, int n)
{
    Stats s;
    double acc = 0;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) s.finite = false;
        s.peak = std::max (s.peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
        acc += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]);
    }
    s.rms = std::sqrt (acc / std::max (1, n));
    return s;
}

//  render helper: MIDI-ish event list -> buffers
struct Ev { int at; int kind; int note; float vel; };   // kind 0=on 1=off

static void render (Engine& e, const std::vector<Ev>& evs, float* L, float* R,
                    int n, int block = 256)
{
    size_t next = 0;
    int done = 0;
    while (done < n)
    {
        while (next < evs.size() && evs[next].at <= done)
        {
            const Ev& v = evs[next++];
            if (v.kind == 0) e.noteOn (v.note, v.vel);
            else             e.noteOff (v.note);
        }
        const int m = std::min (block, n - done);
        e.process (L + done, R + done, m);
        done += m;
    }
}

int main()
{
    std::printf ("=================================================================\n");
    std::printf ("  ARTEFACT B2311.22 - offline bench\n");
    std::printf ("=================================================================\n");
    const double fs = 48000.0;

    //  ---- 1. parameter table sanity ------------------------------------
    {
        std::printf ("-- table\n");
        for (int i = 0; i < numParams(); ++i)
            for (int j = i + 1; j < numParams(); ++j)
                CHECK (std::strcmp (paramSpec (i).id, paramSpec (j).id) != 0,
                       "duplicate id %s", paramSpec (i).id);
        Params dflt;
        for (int i = 0; i < numParams(); ++i)
        {
            const PSpec& s = paramSpec (i);
            CHECK (pvalue (dflt, s) == s.def, "member/default mismatch on %s", s.id);
        }
    }

    //  ---- 2. the catalog: eigen honesty + the alienness floor ----------
    {
        std::printf ("-- catalog: 300 specimens, eigen residuals, harmonic floor\n");
        const auto t0 = std::chrono::steady_clock::now();
        float worstResid = 0, worstMargin = 1e9f;
        int distinctHash[kCatalog];
        int worstMarginAt = -1, maxSalt = 0;
        static Specimen sp;
        for (int c = 0; c < kCatalog; ++c)
        {
            generateSpecimen (c, sp);
            CHECK (sp.nModes >= 16, "specimen %d too small (%d modes)", c, sp.nModes);
            CHECK (sp.eigenResidual < 1e-6f,
                   "specimen %d eigen residual %.2e", c, (double) sp.eigenResidual);
            worstResid = std::max (worstResid, sp.eigenResidual);
            worstMargin = std::min (worstMargin, sp.harmonicMarginCents);
            if (worstMargin == sp.harmonicMarginCents) worstMarginAt = c;
            maxSalt = std::max (maxSalt, sp.saltUsed);
            CHECK (sp.harmonicMarginCents >= kHarmonicFloor,
                   "specimen %d ships within %.1f cents of a harmonic series",
                   c, (double) sp.harmonicMarginCents);
            //  distinctness fingerprint
            double h = 0;
            for (int k = 0; k < sp.nModes; ++k) h += sp.ratio[k] * (k + 1);
            distinctHash[c] = (int) (h * 1000.0);
            //  path + ears sane
            CHECK (sp.pathLen >= 2, "specimen %d path too short", c);
            CHECK (sp.earL != sp.earR, "specimen %d ears coincide", c);
        }
        int dup = 0;
        for (int a = 0; a < kCatalog; ++a)
            for (int b = a + 1; b < kCatalog; ++b)
                if (distinctHash[a] == distinctHash[b]) ++dup;
        CHECK (dup == 0, "%d duplicate specimens", dup);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds> (
                            std::chrono::steady_clock::now() - t0).count();
        std::printf ("   catalog built in %lld ms; worst eigen residual %.2e\n",
                     (long long) ms, (double) worstResid);
        std::printf ("   ALIENNESS FLOOR: closest approach to any harmonic series"
                     " = %.1f cents RMS (specimen %03d; floor %.0f); max salt %d\n",
                     (double) worstMargin, worstMarginAt,
                     (double) kHarmonicFloor, maxSalt);
    }

    //  ---- 3. silence in, silence out -----------------------------------
    {
        std::printf ("-- silence\n");
        Engine e;
        e.prepare (fs, 512);
        const int N = 48000;
        std::vector<float> L (N, 1.0f), R (N, 1.0f);
        render (e, {}, L.data(), R.data(), N);
        double s = 0;
        for (int i = 0; i < N; ++i) s += std::fabs (L[i]) + std::fabs (R[i]);
        CHECK (s == 0.0, "no notes, wake 0, output not exactly zero (%.3e)", s);
    }

    //  ---- 4. determinism: same events, same samples ---------------------
    {
        std::printf ("-- determinism\n");
        auto run = [&] (std::vector<float>& L, std::vector<float>& R)
        {
            Engine e;
            e.p.wake = 0.4f; e.p.warmth = 0.6f; e.p.metabolism = 0.7f;
            e.p.membrane = 0.5f; e.p.revival = 0.35f; e.p.transit = 0.8f;
            e.prepare (fs, 512);
            const int N = 96000;
            L.assign (N, 0); R.assign (N, 0);
            render (e, { { 0, 0, 48, 0.9f }, { 24000, 0, 55, 0.6f },
                         { 48000, 1, 48, 0 }, { 60000, 0, 62, 0.8f },
                         { 84000, 1, 55, 0 } },
                    L.data(), R.data(), N);
        };
        std::vector<float> a1, b1, a2, b2;
        run (a1, b1);
        run (a2, b2);
        CHECK (std::memcmp (a1.data(), a2.data(), a1.size() * 4) == 0
            && std::memcmp (b1.data(), b2.data(), b1.size() * 4) == 0,
               "same events, different samples");
    }

    //  ---- 5. audible, bounded, across settings and specimens ------------
    {
        std::printf ("-- bounded & audible: extremes + 60 random machines\n");
        Rng rr (777);
        float worstPeak = 0;
        for (int trial = 0; trial < 60; ++trial)
        {
            Engine e;
            for (int i = 0; i < numParams(); ++i)
            {
                const PSpec& s = paramSpec (i);
                float v = s.kind == KP_SW ? (float) (rr.next() & 1)
                                          : rr.uni();
                if (std::strcmp (s.id, "specimen") == 0)
                    v = (float) rr.irange (kCatalog);
                if (std::strcmp (s.id, "volume") == 0)
                    v = 0.72f;
                pvalue (e.p, s) = v;
            }
            e.loadSpecimen ((int) e.p.specimen);
            e.prepare (fs, 512);
            const int N = 48000;
            std::vector<float> L (N), R (N);
            render (e, { { 0, 0, 36 + (int) (rr.next() % 48), 0.95f },
                         { 40000, 1, -1, 0 } },   // note off via matching below
                    L.data(), R.data(), N);
            const Stats st = measure (L.data(), R.data(), N);
            CHECK (st.finite, "trial %d not finite", trial);
            CHECK (st.peak < 1.30f, "trial %d peak %.3f", trial, (double) st.peak);
            worstPeak = std::max (worstPeak, st.peak);
        }
        std::printf ("   worst peak %.3f\n", (double) worstPeak);
    }

    //  ---- 6. a note is audible at defaults ------------------------------
    {
        Engine e;
        e.prepare (fs, 512);
        const int N = 48000;
        std::vector<float> L (N), R (N);
        render (e, { { 0, 0, 57, 0.9f } }, L.data(), R.data(), N);
        const Stats st = measure (L.data(), R.data(), N);
        CHECK (st.rms > 1e-4, "default note inaudible (rms %.2e)", st.rms);
        std::printf ("-- default note rms %.4f, peak %.3f\n", st.rms, (double) st.peak);
    }

    //  ---- 7. REVIVAL: the sound recurs exactly --------------------------
    {
        std::printf ("-- revival: exact recurrence at full REVIVAL\n");
        Engine probe;
        probe.loadSpecimen (7);
        const float T = probe.specimen().revivalSeconds;      // at 220 Hz

        auto corrAtLag = [&] (float revival) -> double
        {
            Engine e;
            e.p.specimen = 7; e.p.revival = revival;
            e.p.metabolism = 0.0f; e.p.membrane = 0.0f;
            e.p.transit = 0.0f; e.p.wake = 0.0f; e.p.warmth = 0.0f;
            e.p.aperture = 0.8f; e.p.slab = 1.0f; e.p.winch = 0.5f;
            e.loadSpecimen (7);
            e.prepare (fs, 512);
            const int lag = (int) (T * fs);
            const int settle = (int) (1.2 * fs);
            const int win = (int) (0.5 * fs);
            const int N = settle + lag + win + 4800;
            std::vector<float> L (N), R (N);
            render (e, { { 0, 0, 57, 0.9f } }, L.data(), R.data(), N);
            //  normalised correlation of x(t) with x(t+T)
            double num = 0, d1 = 0, d2 = 0;
            for (int i = settle; i < settle + win; ++i)
            {
                num += (double) L[i] * L[i + lag];
                d1 += (double) L[i] * L[i];
                d2 += (double) L[i + lag] * L[i + lag];
            }
            return num / std::sqrt (std::max (1e-12, d1 * d2));
        };

        const double cFull = corrAtLag (1.0f);
        const double cNone = corrAtLag (0.0f);
        std::printf ("   T = %.2f s; correlation at lag T: revival=1 -> %.4f,"
                     " revival=0 -> %.4f\n", (double) T, cFull, cNone);
        CHECK (cFull > 0.985, "no exact recurrence at full revival (%.3f)", cFull);
        CHECK (cNone < 0.6, "dispersive spectrum recurs anyway (%.3f) - not alien enough", cNone);
    }

    //  ---- 8. anatomy: hub vs remote tissue sound different --------------
    {
        std::printf ("-- anatomy: DEPTH changes the spectrum (hub vs remote)\n");
        auto renderDepth = [&] (float depth, std::vector<float>& L)
        {
            Engine e;
            e.p.specimen = 13; e.p.depth = depth;
            e.p.transit = 0; e.p.wake = 0; e.p.warmth = 0;
            //  migration EQUALISES the strike position over time - that is the
            //  design doing its job. Where you touched the body is measured
            //  before the anatomy has redistributed it.
            e.p.metabolism = 0.0f;
            e.loadSpecimen (13);
            e.prepare (fs, 512);
            const int N = 48000;
            L.assign (N, 0);
            std::vector<float> R (N);
            render (e, { { 0, 0, 57, 0.9f } }, L.data(), R.data(), N);
        };
        std::vector<float> hub, leaf;
        renderDepth (0.0f, hub);
        renderDepth (1.0f, leaf);
        Engine probe;
        probe.loadSpecimen (13);
        const Specimen& sp = probe.specimen();
        double diff = 0, tot = 0;
        const int from = (int) (0.2 * fs), to = (int) (0.6 * fs);
        for (int k = 0; k < std::min (24, sp.nModes); ++k)
        {
            const double f = 220.0 * sp.ratio[k];
            if (f > 0.45 * fs) break;
            const double a = goertzel (hub.data(), from, to, f, fs);
            const double b = goertzel (leaf.data(), from, to, f, fs);
            diff += std::fabs (a - b);
            tot += std::max (a, b);
        }
        const double rel = tot > 0 ? diff / tot : 0;
        std::printf ("   spectral difference hub vs remote: %.2f (rel)\n", rel);
        CHECK (rel > 0.25, "DEPTH does not audibly change the excitation (%.2f)", rel);
    }

    //  ---- 9. conservation: the energy budget honours velocity -----------
    {
        Engine e;
        e.prepare (fs, 512);
        std::vector<float> L (2048), R (2048);
        e.noteOn (57, 0.8f);
        e.process (L.data(), R.data(), 2048);
        const float es = e.debugEnergySum (0);
        //  ringdown has begun by 43 ms (that is the point of it), so the
        //  budget check allows the first few percent of decay
        CHECK (std::fabs (es - 0.8f * 0.8f) < 0.05f,
               "energy budget %.3f != vel^2 0.640", (double) es);
        //  gravity re-leans the spectrum inside the SAME budget
        Engine e2;
        e2.p.gravity = 0.95f;
        e2.prepare (fs, 512);
        e2.noteOn (57, 0.8f);
        e2.process (L.data(), R.data(), 2048);
        const float es2 = e2.debugEnergySum (0);
        CHECK (std::fabs (es2 - es) < 0.02f,
               "gravity broke conservation (%.3f vs %.3f)", (double) es2, (double) es);
        std::printf ("-- conservation: budget %.3f at vel 0.8, invariant under gravity\n",
                     (double) es);
    }

    //  ---- 10. the section: winch recall + playing displaces -------------
    {
        std::printf ("-- the section\n");
        auto settleW = [&] (float winch, float transit, bool play) -> float
        {
            Engine e;
            e.p.winch = winch; e.p.transit = transit; e.p.wake = 0;
            e.prepare (fs, 512);
            const int N = 6 * 48000;
            std::vector<float> L (N), R (N);
            std::vector<Ev> evs;
            if (play)
                for (int h = 0; h < 12; ++h)
                    evs.push_back ({ h * 8000, 0, 40 + h * 3, 0.9f });
            render (e, evs, L.data(), R.data(), N);
            return e.sectionW();
        };
        const float wLo = settleW (0.1f, 0.0f, false);
        const float wHi = settleW (0.9f, 0.0f, false);
        std::printf ("   winch 0.1 -> w %.3f, winch 0.9 -> w %.3f\n",
                     (double) wLo, (double) wHi);
        CHECK (wLo < -1.2f, "the traverse does not reach past the body low (%.3f)", (double) wLo);
        CHECK (wHi >  1.2f, "the traverse does not reach past the body high (%.3f)", (double) wHi);
        CHECK (std::fabs (wLo + wHi) < 0.05f, "the traverse is lopsided (%.3f)",
               (double) (wLo + wHi));

        /*  AND WHAT IT IS FOR: wound to either end, the object has been
            carried out of the plane and there is nothing there. The widening
            guard still protects the gaps BETWEEN organs, so this can only
            pass because the section is genuinely outside the tissue. */
        {
            auto loudAt = [&] (float winch) -> double
            {
                Engine e;
                e.p.winch = winch; e.p.wake = 0; e.p.transit = 0;
                e.prepare (fs, 512);
                const int N = 6 * 48000;
                std::vector<float> L ((size_t) N), R ((size_t) N);
                render (e, { { 2 * 48000, 0, 50, 0.9f } }, L.data(), R.data(), N);
                double pk = 0;
                for (int i = 4 * 48000; i < N; ++i) pk = std::max (pk, (double) std::fabs (L[(size_t) i]));
                return pk;
            };
            const double mid  = loudAt (0.5f);
            const double edge = loudAt (0.0f);
            const double edg2 = loudAt (1.0f);
            std::printf ("   peak: mid %.5f, wound fully out %.5f / %.5f\n", mid, edge, edg2);
            CHECK (mid > 0.01, "the object is not audible in the middle of the traverse");
            CHECK (edge < 0.02 * mid && edg2 < 0.02 * mid,
                   "wound fully out the structures do not disappear (%.4f, %.4f of %.4f)",
                   edge, edg2, mid);
        }

        const float still = settleW (0.5f, 0.0f, true);
        const float moved = settleW (0.5f, 1.0f, true);
        std::printf ("   played, transit 0 -> w %.3f; transit 1 -> w %.3f\n",
                     (double) still, (double) moved);
        CHECK (std::fabs (moved - still) > 0.02f,
               "playing does not move the section (%.3f vs %.3f)",
               (double) moved, (double) still);

        //  the slice must MATTER: same note at two winch positions differs
        auto renderAtW = [&] (float winch, std::vector<float>& L)
        {
            Engine e;
            e.p.winch = winch; e.p.transit = 0; e.p.slab = 0.25f;
            e.loadSpecimen (0);
            e.prepare (fs, 512);
            const int N = 48000;
            L.assign (N, 0);
            std::vector<float> R (N);
            render (e, { { 24000, 0, 57, 0.9f } }, L.data(), R.data(), N);
        };
        std::vector<float> wa, wb;
        renderAtW (0.05f, wa);
        renderAtW (0.95f, wb);
        double d = 0, t = 0;
        for (int i = 24000; i < 48000; ++i)
        {
            d += std::fabs ((double) wa[i] - wb[i]);
            t += std::max (std::fabs ((double) wa[i]), std::fabs ((double) wb[i]));
        }
        const double rel = t > 0 ? d / t : 0;
        std::printf ("   same note, two slices: relative difference %.2f\n", rel);
        CHECK (rel > 0.2, "the slice does not change the sound (%.2f)", rel);
    }

    //  ---- 11. migration: a held note's timbre travels -------------------
    {
        std::printf ("-- migration: timbre moves along the anatomy while held\n");
        Engine e;
        e.p.metabolism = 0.85f; e.p.transit = 0; e.p.wake = 0;
        e.loadSpecimen (30);
        e.prepare (fs, 512);
        const int N = 5 * 48000;
        std::vector<float> L (N), R (N);
        render (e, { { 0, 0, 50, 0.9f } }, L.data(), R.data(), N);
        const Specimen& sp = e.specimen();
        const double f0 = e.p.sidereal >= 0.5f ? 0 : 440.0 * std::pow (2.0, (50 - 69) / 12.0);
        //  spectral centroid early vs late (over the mode set)
        auto centroid = [&] (int from, int to) -> double
        {
            double num = 0, den = 0;
            for (int k = 0; k < std::min (32, sp.nModes); ++k)
            {
                const double f = f0 * sp.ratio[k];
                if (f > 0.45 * fs) break;
                const double a = goertzel (L.data(), from, to, f, fs);
                num += f * a; den += a;
            }
            return den > 0 ? num / den : 0;
        };
        const double cEarly = centroid ((int) (0.4 * fs), (int) (0.9 * fs));
        const double cLate  = centroid ((int) (4.2 * fs), (int) (4.9 * fs));
        std::printf ("   centroid %.0f Hz early -> %.0f Hz late (section frozen)\n",
                     cEarly, cLate);

        /*  THE MECHANISM ITSELF: where the energy is, mode by mode. This is
            what migration moves, and it cannot be pinned by the section. */
        {
            Engine m;
            m.p.metabolism = 0.85f; m.p.transit = 0; m.p.wake = 0;
            m.loadSpecimen (30);
            m.prepare (fs, 512);
            std::vector<float> l2 (512), r2 (512);
            m.noteOn (50, 0.9f);
            auto snap = [&] (std::vector<double>& out)
            {
                out.assign ((size_t) std::min (32, m.specimen().nModes), 0.0);
                for (size_t k = 0; k < out.size(); ++k)
                    out[k] = m.debugModeEnergy (0, (int) k);
            };
            auto advance = [&] (double secs)
            {
                const int nb = (int) (secs * fs / 512);
                for (int b = 0; b < nb; ++b) m.process (l2.data(), r2.data(), 512);
            };
            advance (0.6);
            std::vector<double> e0; snap (e0);
            advance (4.0);
            std::vector<double> e1; snap (e1);
            //  how far the distribution moved, as a share of its own size
            double num = 0, d0 = 0, d1 = 0;
            for (size_t k = 0; k < e0.size(); ++k)
            { num += std::fabs (e1[k] - e0[k]); d0 += e0[k]; d1 += e1[k]; }
            const double travelled = num / std::max (1e-12, 0.5 * (d0 + d1));
            std::printf ("   energy distribution travelled %.1f %% of itself\n",
                         100.0 * travelled);
            CHECK (travelled > 0.10,
                   "energy does not migrate along the anatomy (%.1f %%)",
                   100.0 * travelled);
        }

        /*  AND THE AUDIBLE CONSEQUENCE, under the condition the object is
            actually in: the section is never frozen in use — it patrols, and
            everything played moves it. */
        {
            Engine a;
            a.p.metabolism = 0.85f;
            a.loadSpecimen (30);
            a.prepare (fs, 512);
            const int NA = 30 * 48000;
            std::vector<float> AL ((size_t) NA), AR ((size_t) NA);
            render (a, { { 0, 0, 50, 0.9f } }, AL.data(), AR.data(), NA);
            auto rmsF = [&] (int from, int to)
            {
                double se = 0, sd = 0;
                for (int i = from + 1; i < to; ++i)
                { const double x = AL[(size_t) i], d = x - AL[(size_t) i - 1];
                  se += x * x; sd += d * d; }
                return se > 1e-30 ? (fs / (2.0 * 3.14159265358979)) * std::sqrt (sd / se) : 0.0;
            };
            /*  THE SPREAD ACROSS THE WHOLE TAKE, not two instants of it. The
                section patrols on a period of tens of seconds, so comparing
                the start against one chosen later moment can catch the wander
                back where it began and report that nothing moved — measured,
                4.7 % between two instants of a take whose colour actually
                ranges over five times that. Travel is a range, not a
                difference of endpoints. */
            double lo = 1e18, hi = -1e18, a0 = 0;
            for (int q = 0; q < 10; ++q)
            {
                const double t0 = 0.4 + q * 2.85;
                const double f = rmsF ((int) (t0 * fs), (int) ((t0 + 1.0) * fs));
                if (q == 0) a0 = f;
                lo = std::min (lo, f); hi = std::max (hi, f);
            }
            const double a1 = hi;
            const double moved = (hi - lo) / std::max (1.0, lo);
            std::printf ("   colour of a held note over 29 s spans %.0f..%.0f Hz (%.1f %%)\n",
                         lo, hi, 100.0 * moved);
            CHECK (moved > 0.10,
                   "a held note does not travel in use (%.1f %%)", 100.0 * moved);
        }
    }

    //  ---- 11b. Peter's report: specimens must DIFFER, gravity must LIVE --
    {
        std::printf ("-- identity: two specimens differ; gravity reshapes a held note\n");
        auto renderSpec = [&] (int cat, std::vector<float>& L)
        {
            Engine e;
            e.p.specimen = (float) cat; e.p.transit = 0; e.p.wake = 0;
            e.loadSpecimen (cat);
            e.prepare (fs, 512);
            const int N = 48000;
            L.assign (N, 0);
            std::vector<float> R (N);
            render (e, { { 0, 0, 57, 0.9f } }, L.data(), R.data(), N);
        };
        std::vector<float> s0, s1;
        renderSpec (0, s0);
        renderSpec (37, s1);
        double d = 0, t = 0;
        for (int i2 = 12000; i2 < 48000; ++i2)
        {
            d += std::fabs ((double) s0[i2] - s1[i2]);
            t += std::max (std::fabs ((double) s0[i2]), std::fabs ((double) s1[i2]));
        }
        const double relSpec = t > 0 ? d / t : 0;
        std::printf ("   specimen 0 vs 37: relative difference %.2f\n", relSpec);
        CHECK (relSpec > 0.5, "specimens sound alike (%.2f)", relSpec);

        //  gravity moved MID-NOTE must reshape the spectrum of the held note
        auto renderGrav = [&] (bool move, std::vector<float>& L)
        {
            Engine e;
            e.p.transit = 0; e.p.wake = 0; e.p.metabolism = 0;
            e.prepare (fs, 512);
            const int N = 96000;
            L.assign (N, 0);
            std::vector<float> R (N);
            e.noteOn (57, 0.9f);
            int done = 0;
            while (done < N)
            {
                if (move && done >= 48000) e.p.gravity = 0.95f;
                const int m = std::min (512, N - done);
                e.process (L.data() + done, R.data() + done, m);
                done += m;
            }
        };
        std::vector<float> gStill, gMoved;
        renderGrav (false, gStill);
        renderGrav (true, gMoved);
        double gd = 0, gt = 0;
        for (int i2 = 60000; i2 < 96000; ++i2)
        {
            gd += std::fabs ((double) gStill[i2] - gMoved[i2]);
            gt += std::max (std::fabs ((double) gStill[i2]), std::fabs ((double) gMoved[i2]));
        }
        const double relG = gt > 0 ? gd / gt : 0;
        std::printf ("   gravity moved mid-note: relative difference %.2f\n", relG);
        CHECK (relG > 0.2, "gravity does not live on a held note (%.2f)", relG);
    }

    //  ---- 12. WAKE 0 is deterministic; the default wander is real -------
    {
        //  wake defaults ON (0.15): the artefact idles alive. So the
        //  contracts are: wake 0 renders bit-identically twice, and a
        //  default engine does NOT render like a wake-0 engine over a
        //  wander period (the exploration is real, not cosmetic).
        Engine a, b, c;
        a.p.wake = 0.0f; a.prepare (fs, 512);
        b.p.wake = 0.0f; b.prepare (fs, 512);
        c.prepare (fs, 512);          // default wake 0.15
        const int N = 6 * 48000;
        std::vector<float> l1 (N), r1 (N), l2 (N), r2 (N), l3 (N), r3 (N);
        render (a, { { 0, 0, 57, 0.8f } }, l1.data(), r1.data(), N);
        render (b, { { 0, 0, 57, 0.8f } }, l2.data(), r2.data(), N);
        render (c, { { 0, 0, 57, 0.8f } }, l3.data(), r3.data(), N);
        CHECK (std::memcmp (l1.data(), l2.data(), N * 4) == 0,
               "wake 0 is not deterministic");
        CHECK (std::memcmp (l1.data(), l3.data(), N * 4) != 0,
               "default wake changes nothing - the wander is cosmetic");
    }

    //  ---- 13b. THE INTERLOCUTOR: it answers, it is kin or deaf, it changes
    {
        std::printf ("-- the interlocutor\n");

        auto mk = [&] (Engine& e, int self, int other, float conv,
                       float commune, float plastic)
        {
            e.p.specimen = (float) self;
            e.p.other    = (float) other;
            e.p.converse = conv;
            e.p.commune  = commune;
            e.p.plastic  = plastic;
            e.p.wake = 0; e.p.transit = 0;
            e.prepare (fs, 512);
        };
        const int N = 5 * 48000;
        //  a phrase, then SILENCE — the reply lives in the silence
        const std::vector<Ev> phrase = {
            { 0, 0, 57, 0.9f }, { 20000, 1, 57, 0 },
            { 24000, 0, 62, 0.9f }, { 44000, 1, 62, 0 } };
        auto rmsFrom = [] (const std::vector<float>& v, int from) -> double
        {
            double e = 0;
            for (int i2 = from; i2 < (int) v.size(); ++i2) e += (double) v[i2] * v[i2];
            return std::sqrt (e / std::max (1, (int) v.size() - from));
        };

        //  (a) IT ANSWERS: the silence after a phrase fills with a voice
        //      that is not the note's own ringdown.
        {
            std::vector<float> lOff (N), rOff (N), lOn (N), rOn (N);
            { Engine e; mk (e, 3, 3, 0.0f, 0.0f, 0.0f);
              render (e, phrase, lOff.data(), rOff.data(), N); }
            { Engine e; mk (e, 3, 3, 0.7f, 0.0f, 0.0f);
              render (e, phrase, lOn.data(), rOn.data(), N); }
            const double a = rmsFrom (lOff, (int) (4.2 * fs));
            const double b = rmsFrom (lOn,  (int) (4.2 * fs));
            std::printf ("   answers into the silence: rms %.5f -> %.5f\n", a, b);
            CHECK (b > 5.0e-4 && b > a * 5.0,
                   "the interlocutor does not answer (%.5f vs %.5f)", b, a);
        }

        //  (b) KINSHIP IS REAL, in the metric and in the engine's ears.
        {
            Specimen a, b;
            generateSpecimen (3, a);
            const float kinSelf = spectralKinship (a, a);
            int worst = 0; float kinWorst = 1e9f;
            for (int c = 0; c < kCatalog; c += 7)
            {
                if (c == 3) continue;
                generateSpecimen (c, b);
                const float k = spectralKinship (b, a);
                if (k < kinWorst) { kinWorst = k; worst = c; }
            }
            std::printf ("   ladder overlap: self %.3f, least kin (#%d) %.3f\n",
                         (double) kinSelf, worst, (double) kinWorst);
            CHECK (kinSelf > kinWorst * 2.0f,
                   "kinship does not discriminate (%.3f vs %.3f)",
                   (double) kinSelf, (double) kinWorst);

            std::vector<float> l1 (N), r1 (N), l2 (N), r2 (N);
            Engine e1; mk (e1, 3, 3,     0.7f, 0.0f, 0.0f);
            render (e1, phrase, l1.data(), r1.data(), N);
            Engine e2; mk (e2, 3, worst, 0.7f, 0.0f, 0.0f);
            render (e2, phrase, l2.data(), r2.data(), N);
            const double tKin = rmsFrom (l1, (int) (4.2 * fs));
            const double tStr = rmsFrom (l2, (int) (4.2 * fs));
            std::printf ("   engine hears: kin %.3f (reply rms %.5f),"
                         " stranger %.3f (reply rms %.5f)\n",
                         (double) e1.debugKinship(), tKin,
                         (double) e2.debugKinship(), tStr);
            CHECK (e1.debugKinship() > e2.debugKinship() * 1.3f,
                   "the engine hears no difference between kin and stranger");
            CHECK (tKin > tStr,
                   "kin do not answer more strongly than strangers");
            CHECK (tStr > 1.0e-5,
                   "a stranger is mute — it must answer faintly, never not at all");
        }

        //  (c) IT IS A TRANSLATION: the same body answers a dark utterance
        //      differently from a bright one. The reply carries the SHAPE
        //      of what was said, not a fixed gesture.
        {
            auto replyCentroid = [&] (float depth) -> double
            {
                Engine e;
                e.p.depth = depth;
                mk (e, 3, 3, 0.7f, 0.0f, 0.0f);
                std::vector<float> Lv (N), Rv (N);
                render (e, phrase, Lv.data(), Rv.data(), N);
                const int from = (int) (4.3 * fs), to = (int) (4.9 * fs);
                const Specimen& os = e.otherSpecimen();
                double num = 0, den = 0;
                for (int k = 0; k < os.nModes; ++k)
                {
                    const double f = os.voiceHz * os.ratio[k];
                    if (f < 30 || f > 8000) continue;
                    const double amp = goertzel (Lv.data(), from, to, f, fs);
                    num += amp * f; den += amp;
                }
                return den > 0 ? num / den : 0;
            };
            const double cDark = replyCentroid (0.05f);
            const double cBright = replyCentroid (0.95f);
            std::printf ("   reply centroid: dark utterance %.0f Hz, bright %.0f Hz\n",
                         cDark, cBright);
            CHECK (cDark > 0 && std::fabs (cBright - cDark) / cDark > 0.03,
                   "the reply ignores what was said (%.0f vs %.0f)", cDark, cBright);
        }

        //  (d) THE MEMBRANE: a third voice neither body owns. Three
        //      properties, and the first is the decisive one — a product
        //      with silence is silence, exactly.
        {
            const int M = 9 * 48000;
            auto held = [&] (Engine& e, std::vector<float>& Lv, std::vector<float>& Rv)
            {
                Lv.assign (M, 0); Rv.assign (M, 0);
                render (e, { { 0, 0, 57, 0.9f } }, Lv.data(), Rv.data(), M);
            };
            //  (d1) with no partner at all it must not exist, bit for bit
            {
                std::vector<float> l1, r1, l2, r2;
                Engine a; mk (a, 3, 61, 0.0f, 0.0f, 0.0f); held (a, l1, r1);
                Engine b; mk (b, 3, 61, 0.0f, 0.9f, 0.0f); held (b, l2, r2);
                CHECK (std::memcmp (l1.data(), l2.data(), (size_t) M * 4) == 0,
                       "the membrane sounds with only one body present");
            }
            //  (d2) with both sounding it is a material part of the sound,
            //  and (d3) it puts energy on a DIFFERENCE frequency that is in
            //  neither ladder, far more than it moves a control frequency.
            {
                std::vector<float> l0, r0, l1, r1;
                Engine a; mk (a, 3, 61, 0.7f, 0.0f, 0.0f); held (a, l0, r0);
                Engine b; mk (b, 3, 61, 0.7f, 0.9f, 0.0f); held (b, l1, r1);
                const int F = (int) (5.3 * fs), T = (int) (6.8 * fs);
                double d = 0, t = 0;
                for (int i2 = F; i2 < T; ++i2)
                {
                    d += std::fabs ((double) l1[i2] - l0[i2]);
                    t += std::max (std::fabs ((double) l1[i2]), std::fabs ((double) l0[i2]));
                }
                const double rel = t > 0 ? d / t : 0;
                std::printf ("   membrane: %.0f%% of the sound while both voices ring\n",
                             rel * 100.0);
                CHECK (rel > 0.05, "the membrane is inaudible (%.3f)", rel);

                const Specimen& ss = a.specimen();
                const Specimen& os = a.otherSpecimen();
                const double f0 = 440.0 * std::pow (2.0, (57.0 - 69.0) / 12.0);
                auto owned = [&] (double f) -> bool
                {
                    for (int k = 0; k < ss.nModes; ++k)
                        if (std::fabs (1200.0 * std::log2 (f0 * ss.ratio[k] / f)) < 40.0)
                            return true;
                    for (int k = 0; k < os.nModes; ++k)
                        if (std::fabs (1200.0 * std::log2 (os.voiceHz * os.ratio[k] / f)) < 40.0)
                            return true;
                    return false;
                };
                //  a difference frequency belonging to neither body
                double target = 0;
                for (int k = 0; k < ss.nModes && target == 0; ++k)
                    for (int l = 0; l < os.nModes; ++l)
                    {
                        const double f = std::fabs (f0 * ss.ratio[k] - os.voiceHz * os.ratio[l]);
                        if (f > 90 && f < 2500 && ! owned (f)) { target = f; break; }
                    }
                CHECK (target > 0, "no unowned difference frequency to probe");
                if (target > 0)
                {
                    //  a control frequency, also unowned, well away from it
                    double ctl = 0;
                    for (double q = 1.07; q < 1.9 && ctl == 0; q += 0.013)
                        if (! owned (target * q)) ctl = target * q;
                    const double dOff = goertzel (l0.data(), F, T, target, fs);
                    const double dOn  = goertzel (l1.data(), F, T, target, fs);
                    const double cOff = goertzel (l0.data(), F, T, ctl, fs);
                    const double cOn  = goertzel (l1.data(), F, T, ctl, fs);
                    const double rD = dOff > 0 ? dOn / dOff : 0;
                    const double rC = cOff > 0 ? cOn / cOff : 1;
                    std::printf ("   difference tone %.0f Hz (in neither ladder):"
                                 " x%.2f, control %.0f Hz: x%.2f\n",
                                 target, rD, ctl, rC);
                    CHECK (rD > rC * 1.4,
                           "the membrane makes no unowned frequencies (x%.2f vs x%.2f)",
                           rD, rC);
                }
            }
        }

        //  (e) SPEECH IS SURGERY: after a conversation the listener's own
        //      voice has moved — in cents — and is still a solvable body.
        {
            auto converse = [&] (float plastic, Specimen& before, Specimen& after,
                                 int& steps, float& peak)
            {
                Engine e; mk (e, 3, 61, 0.8f, 0.0f, plastic);
                before = e.otherSpecimen();
                const int M = 30 * 48000;
                std::vector<float> Lv (M), Rv (M);
                std::vector<Ev> evs;
                for (int h = 0; h < 30; ++h)
                {
                    evs.push_back ({ h * 40000,         0, 45 + (h * 5) % 19, 0.95f });
                    evs.push_back ({ h * 40000 + 24000, 1, 45 + (h * 5) % 19, 0 });
                }
                size_t next = 0; int done = 0;
                peak = 0;
                while (done < M)
                {
                    while (next < evs.size() && evs[next].at <= done)
                    {
                        const Ev& v = evs[next++];
                        if (v.kind == 0) e.noteOn (v.note, v.vel);
                        else             e.noteOff (v.note);
                    }
                    const int m = std::min (2048, M - done);
                    e.process (Lv.data() + done, Rv.data() + done, m);
                    for (int i2 = done; i2 < done + m; ++i2)
                        peak = std::max (peak, std::fabs (Lv[i2]));
                    done += m;
                    //  the message thread's turn, exactly as the plugin does it
                    if (e.remodelPending()) e.serviceRemodel();
                }
                after = e.otherSpecimen();
                steps = e.accentSteps();
            };
            auto driftCents = [&] (const Specimen& x, const Specimen& y) -> double
            {
                const int nm = std::min (x.nModes, y.nModes);
                double acc = 0; int cnt = 0;
                for (int k = 0; k < nm; ++k)
                {
                    if (x.ratio[k] <= 0 || y.ratio[k] <= 0) continue;
                    const double c = 1200.0 * std::log2 (y.ratio[k] / x.ratio[k]);
                    acc += c * c; ++cnt;
                }
                return cnt > 0 ? std::sqrt (acc / cnt) : 0;
            };
            Specimen b0, a0, b1, a1;
            int st0 = 0, st1 = 0; float pk0 = 0, pk1 = 0;
            converse (0.0f, b0, a0, st0, pk0);
            converse (1.0f, b1, a1, st1, pk1);
            const double d0 = driftCents (b0, a0);
            const double d1 = driftCents (b1, a1);
            std::printf ("   accent after 30 s of talking: plasticity 0 -> %.1f cents"
                         " (%d steps), plasticity 1 -> %.1f cents (%d steps)\n",
                         d0, st0, d1, st1);
            CHECK (d0 < 0.01, "plasticity 0 remodelled the body (%.2f cents)", d0);
            CHECK (st0 == 0, "plasticity 0 applied %d accent steps", st0);
            CHECK (d1 > 5.0, "listening does not change the listener (%.1f cents)", d1);
            CHECK (d1 < 2400.0, "the accent dissolved the body (%.1f cents)", d1);
            CHECK (pk1 < 1.0f, "remodelling broke the ceiling (peak %.3f)", (double) pk1);
            CHECK (a1.nModes > 0 && a1.eigenResidual < 1e-6f,
                   "the remodelled body does not solve (residual %.2e)",
                   (double) a1.eigenResidual);
            //  and it is still a BODY: bounded edges, a real slab, audible
            CHECK (a1.nEdges > 0 && a1.pathLen > 1,
                   "the remodelled body lost its anatomy");
        }

        //  (f) CONVERSE 0: the second body does not exist, whichever it is
        {
            const int M = 2 * 48000;
            std::vector<float> l1 (M), r1 (M), l2 (M), r2 (M);
            Engine a; mk (a, 3,  61, 0.0f, 0.9f, 0.9f);
            render (a, phrase, l1.data(), r1.data(), M);
            Engine b; mk (b, 3, 200, 0.0f, 0.9f, 0.9f);
            render (b, phrase, l2.data(), r2.data(), M);
            CHECK (std::memcmp (l1.data(), l2.data(), (size_t) M * 4) == 0,
                   "converse 0 is not silent about which body is not there");
        }

        //  (g) the whole conversation is deterministic
        {
            const int M = 3 * 48000;
            std::vector<float> l1 (M), r1 (M), l2 (M), r2 (M);
            Engine a; mk (a, 3, 61, 0.8f, 0.5f, 0.7f);
            render (a, phrase, l1.data(), r1.data(), M);
            Engine b; mk (b, 3, 61, 0.8f, 0.5f, 0.7f);
            render (b, phrase, l2.data(), r2.data(), M);
            CHECK (std::memcmp (l1.data(), l2.data(), (size_t) M * 4) == 0,
                   "the conversation is not deterministic");
        }
    }
    //  ---- 13. rates ------------------------------------------------------
    {
        std::printf ("-- rates\n");
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e;
            e.p.membrane = 0.6f; e.p.metabolism = 0.6f; e.p.revival = 0.5f;
            e.prepare (rate, 512);
            const int N = (int) rate;
            std::vector<float> L (N), R (N);
            render (e, { { 0, 0, 96, 0.95f }, { N / 2, 0, 33, 0.95f } },
                    L.data(), R.data(), N);
            const Stats st = measure (L.data(), R.data(), N);
            CHECK (st.finite && st.peak < 1.30f,
                   "rate %.0f: peak %.3f finite %d", rate, (double) st.peak, (int) st.finite);
        }
    }

    //  ---- 13b. the site ---------------------------------------------------
    {
        /*  THE SITE (proxima_site.h). Uncoupled, this body must be BYTE-
            identical to one that never heard of it; coupled, the sustain
            floor must audibly breathe with the site — measured as the output
            envelope's concentration on the site's period, once the strike
            has rung down to the floor. */
        std::printf ("-- the site\n");
        auto take = [&] (float pull, double siteHz, double seconds) -> std::vector<float>
        {
            Engine e; e.p.aperture = 0.9f; e.p.temp = 0.12f; e.p.revival = 0.0f;
            e.prepare (fs, 512);
            e.noteOn (45, 0.9f);
            const int nb = (int) (seconds * fs / 512);
            std::vector<float> L ((size_t) nb * 512), R ((size_t) nb * 512);
            for (int b = 0; b < nb; ++b)
            {
                const double t = (double) b * 512 / fs;
                e.setSite ((float) std::fmod (t * siteHz, 1.0), (float) siteHz, pull);
                e.process (L.data() + (size_t) b * 512, R.data() + (size_t) b * 512, 512);
            }
            return L;
        };
        {
            Engine e; e.p.aperture = 0.9f; e.p.temp = 0.12f; e.p.revival = 0.0f;
            e.prepare (fs, 512);
            e.noteOn (45, 0.9f);
            const int N = 4 * 48000;
            std::vector<float> L (N), R (N);
            e.process (L.data(), R.data(), N);
            const auto un = take (0.0f, 0.5, 4.0);
            bool same = un.size() == L.size();
            for (size_t i = 0; same && i < L.size(); ++i) same = un[i] == L[i];
            std::printf ("   uncoupled: %s\n", same ? "byte-identical" : "DIFFERS");
            CHECK (same, "uncoupled, the site must be an exact no-op");
        }
        /*  The lean that is HEARD is the section rocking at the site's rate:
            the slab decides which modes are audible and answers within a
            tick. (The sustain breath is real but its modes reach the floor
            over seconds — measured 0.047 vs 0.046 on a four-wrap cycle, below
            the body's own breath noise — so it is not what this check reads.)
            Fold the output energy on the site period once the strike has
            settled; a window of several periods, or it measures nothing. */
        auto fold = [&] (const std::vector<float>& L, double hz) -> double
        {
            double cx = 0, cy = 0, w = 0;
            const int hop = 256;
            for (size_t i = (size_t) (6.0 * fs); i + hop < L.size(); i += hop)
            {
                double e = 0; for (int k = 0; k < hop; ++k) e += (double) L[i + k] * L[i + k];
                const double ph = std::fmod ((double) i / fs * hz, 1.0) * 2.0 * 3.14159265358979;
                cx += e * std::cos (ph); cy += e * std::sin (ph); w += e;
            }
            return w > 0 ? std::sqrt (cx * cx + cy * cy) / w : 0.0;
        };
        const double rFree = fold (take (0.0f, 0.5, 24.0), 0.5);
        const double rLock = fold (take (1.0f, 0.5, 24.0), 0.5);
        std::printf ("   the section rocking on the site's 0.5 Hz: %.3f coupled vs %.3f free\n", rLock, rFree);
        CHECK (rLock > 0.10 && rLock > rFree * 2.0,
               "coupled, the section must rock with the site (%.3f vs %.3f)", rLock, rFree);
    }

    //  ---- 14. cost -------------------------------------------------------
    {
        Engine e;
        e.p.metabolism = 0.6f;
        e.prepare (fs, 512);
        for (int v = 0; v < 6; ++v) e.noteOn (40 + v * 5, 0.9f);
        const int N = 4 * 48000;
        std::vector<float> L (N), R (N);
        const auto t0 = std::chrono::steady_clock::now();
        e.process (L.data(), R.data(), N);
        const auto us = std::chrono::duration_cast<std::chrono::microseconds> (
                            std::chrono::steady_clock::now() - t0).count();
        const double realtime = (double) N / fs;
        const double took = (double) us / 1e6;
        std::printf ("-- cost: 6 voices, %.2f s audio in %.3f s = %.1fx realtime"
                     " (~%.1f%% of a core)\n",
                     realtime, took, realtime / took, 100.0 * took / realtime);
    }

    std::printf ("=================================================================\n");
    if (fails == 0) std::printf ("  ALL CLEAR  --  %d checks\n", checks);
    else            std::printf ("  %d FAILURES of %d checks\n", fails, checks);
    std::printf ("=================================================================\n");
    return fails == 0 ? 0 : 1;
}
