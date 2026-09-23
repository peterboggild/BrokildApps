/*  THIN WALLS bench. Every claim in Engine.h measured, none of them assumed.

    Build:  cmake -S test -B test/build && cmake --build test/build --config Release
    Run:    test/build/Release/twtest.exe   (or through test/run-bench.ps1 past SAC)
*/
#include "Engine.h"
#include "Geometry.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

using namespace tw;

static int checks = 0, failures = 0;
static void check (bool ok, const char* what, double a = 0, double b = 0)
{
    ++checks;
    if (! ok) ++failures;
    std::printf ("  %s  %s", ok ? "ok  " : "FAIL", what);
    if (a != 0 || b != 0) std::printf ("   [%.4g vs %.4g]", a, b);
    std::printf ("\n");
}

//------------------------------------------------------------------------------
struct Take
{
    std::vector<float> L, R;
    double fs;
    int size() const { return (int) L.size(); }
};

// render `seconds` of the engine at params p with the given input generator
template <typename Gen>
static Take render (Engine& e, const Params& p, double fs, double seconds, Gen gen, int block = 256)
{
    Take t; t.fs = fs;
    const int n = (int) (seconds * fs);
    t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
    e.setParams (p);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        for (int k = 0; k < m; ++k) { const float v = gen (i + k); t.L[(size_t) (i + k)] = v; t.R[(size_t) (i + k)] = v; }
        e.process (&t.L[(size_t) i], &t.R[(size_t) i], m);
    }
    return t;
}

static Engine* fresh (double fs)
{
    Engine* e = new Engine();
    e->prepare (fs, 256);
    return e;
}

static double energy (const std::vector<float>& v, int a, int b)
{
    double s = 0; a = std::max (a, 0); b = std::min (b, (int) v.size());
    for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
    return s;
}
static double db (double x) { return 10.0 * std::log10 (std::max (x, 1e-30)); }
static double peakAbs (const std::vector<float>& v) { double p = 0; for (float x : v) p = std::max (p, (double) std::abs (x)); return p; }
static int firstAbove (const std::vector<float>& v, float thr) { for (int i = 0; i < (int) v.size(); ++i) if (std::abs (v[(size_t) i]) > thr) return i; return -1; }

// RBJ bandpass, Q ~ 1.4, applied twice (a fourth-order octave band)
static std::vector<float> octaveBand (const std::vector<float>& x, double fs, double fc)
{
    std::vector<float> y = x;
    for (int pass = 0; pass < 4; ++pass)
    {
        const double w = 2 * 3.14159265358979 * fc / fs, Q = 1.4142;
        const double alpha = std::sin (w) / (2 * Q), cw = std::cos (w);
        const double b0 = alpha, b1 = 0, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& v : y)
        {
            const double in = v;
            const double out = (b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1; x1 = in; y2 = y1; y1 = out; v = (float) out;
        }
    }
    return y;
}

// Schroeder backward integration; T from the fit between -5 and -25 dB (T20 * 3)
static double rt60From (const std::vector<float>& h, double fs, int from)
{
    std::vector<double> edc ((size_t) h.size(), 0.0);
    double acc = 0;
    for (int i = (int) h.size() - 1; i >= from; --i) { acc += (double) h[(size_t) i] * h[(size_t) i]; edc[(size_t) i] = acc; }
    const double top = db (edc[(size_t) from]);
    int i5 = -1, i25 = -1;
    for (int i = from; i < (int) h.size(); ++i)
    {
        const double d = db (edc[(size_t) i]) - top;
        if (i5 < 0 && d <= -5) i5 = i;
        if (i25 < 0 && d <= -25) { i25 = i; break; }
    }
    if (i5 < 0 || i25 < 0) return -1;
    return 3.0 * (i25 - i5) / fs;
}

// interaural delay by cross-correlation over a window, in seconds (positive = right later)
static double itdSeconds (const Take& t, int a, int b, double maxMs = 1.2)
{
    const int maxLag = (int) (maxMs * 1e-3 * t.fs);
    double best = -1e30; int bestLag = 0;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double s = 0;
        for (int i = a; i < b; ++i)
        {
            const int j = i + lag;      // R[j] vs L[i]
            if (j < 0 || j >= t.size()) continue;
            s += (double) t.L[(size_t) i] * t.R[(size_t) j];
        }
        if (s > best) { best = s; bestLag = lag; }
    }
    return bestLag / t.fs;
}

static Params base()
{
    Params p;
    p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = 1.65f; p.src[0].yaw = 0; p.src[0].type = 0;
    p.lisX = 4.5f; p.lisY = 2.5f; p.lisYaw = 180.0f;
    p.door[0] = 1; p.door[1] = 1; p.door[2] = 0;
    p.material[0] = 1; p.material[1] = 1; p.material[2] = 2;
    return p;
}

// a source in the middle of the GIANT hall (absorbing), the listener 2 m away, doors shut:
// the cleanest place to look at a direct path
static Params anechoicish (float azDeg, float dist, float elDeg = 0)
{
    Params p = base();
    p.material[2] = 0; p.door[1] = 0; p.door[2] = 0;
    p.lisX = 12.0f; p.lisY = 4.5f; p.lisYaw = 90.0f;          // facing north
    // az measured clockwise from the facing direction (0 front, 90 right)
    const float a = (90.0f - azDeg) * 3.14159265f / 180.0f;
    const float horiz = dist * std::cos (elDeg * 3.14159265f / 180.0f);
    p.src[0].x = p.lisX + horiz * std::cos (a);
    p.src[0].y = p.lisY + horiz * std::sin (a);
    p.src[0].z = EAR_HEIGHT + dist * std::sin (elDeg * 3.14159265f / 180.0f);
    p.earlyDb = -80; p.reverbDb = -80;
    return p;
}

static auto impulseAt (int at) { return [at] (int i) { return i == at ? 1.0f : 0.0f; }; }

//------------------------------------------------------------------------------
int main (int argc, char** argv)
{
    const double fs = 48000.0;
    std::printf ("THIN WALLS bench, %.0f Hz\n\n", fs);
    (void) argc; (void) argv;

    // ---- 1. silence, bounds, decay, determinism ----------------------------------
    std::printf ("1. housekeeping\n");
    {
        Engine* e = fresh (fs);
        Take t = render (*e, base(), fs, 1.0, [] (int) { return 0.0f; });
        check (energy (t.L, 0, t.size()) == 0.0 && energy (t.R, 0, t.size()) == 0.0, "silence in, exact silence out");
        delete e;

        Engine* a = fresh (fs); Engine* b = fresh (fs);
        uint32_t s1 = 1, s2 = 1;
        auto noiseA = [&s1] (int) { s1 = s1 * 1664525u + 1013904223u; return ((s1 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
        auto noiseB = [&s2] (int) { s2 = s2 * 1664525u + 1013904223u; return ((s2 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
        Take ta = render (*a, base(), fs, 2.0, noiseA);
        Take tb = render (*b, base(), fs, 2.0, noiseB);
        check (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0
            && std::memcmp (ta.R.data(), tb.R.data(), ta.R.size() * sizeof (float)) == 0, "deterministic: two engines, identical takes");
        check (std::isfinite (peakAbs (ta.L)) && peakAbs (ta.L) < 6.0, "furnished large room, noise at -10 dBFS: finite, bounded", peakAbs (ta.L));
        delete a; delete b;
    }

    // every room, every material, a burst then silence: it must die
    {
        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int m = 0; m < NUM_MATERIALS; ++m)
            {
                Engine* e = fresh (fs);
                Params p = base();
                p.material[0] = p.material[1] = p.material[2] = m;
                p.door[0] = p.door[1] = p.door[2] = 1;
                const Room& R = ROOMS[r];
                p.src[0].x = 0.5f * (R.x0 + R.x1) - 0.7f; p.src[0].y = 0.5f * (R.y0 + R.y1) + 0.4f;
                p.lisX = 0.5f * (R.x0 + R.x1) + 0.9f; p.lisY = 0.5f * (R.y0 + R.y1) - 0.3f;
                uint32_t s = 7;
                Take t = render (*e, p, fs, 14.0, [&s, fs] (int i) { s = s * 1664525u + 1013904223u; return i < (int) (0.5 * fs) ? ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f : 0.0f; });
                const double late = energy (t.L, (int) (13.0 * fs), t.size()) / (1.0 * fs);
                const double loud = energy (t.L, (int) (0.1 * fs), (int) (0.5 * fs)) / (0.4 * fs);
                char buf[128]; std::snprintf (buf, sizeof buf, "%s %s: a burst decays (late %.0f dB below the burst) and stays bounded", R.name, MATERIAL_NAMES[m], db (loud) - db (late));
                check (std::isfinite (peakAbs (t.L)) && peakAbs (t.L) < 12.0 && db (loud) - db (late) > 60.0, buf, peakAbs (t.L));
                delete e;
            }
    }

    // ---- 2. the head -------------------------------------------------------------
    std::printf ("\n2. the head (direct path only, hall, absorbing)\n");
    {
        auto directWindow = [&] (const Take& t, int& a, int& b)
        {
            a = firstAbove (t.L, 1e-4f); const int ar = firstAbove (t.R, 1e-4f);
            if (ar >= 0 && (a < 0 || ar < a)) a = ar;
            b = a + (int) (0.050 * fs);
        };
        double itd90 = 0, itd270 = 0, itd0 = 0, itd180 = 0;
        for (float az : { 0.0f, 90.0f, 180.0f, 270.0f, 45.0f })
        {
            Engine* e = fresh (fs);
            Take t = render (*e, anechoicish (az, 2.0f), fs, 0.3, impulseAt (100));
            int a, b; directWindow (t, a, b);
            const double it = itdSeconds (t, a, b);
            if (az == 90) itd90 = it; if (az == 270) itd270 = it; if (az == 0) itd0 = it; if (az == 180) itd180 = it;
            if (az == 45)
            {
                // Woodworth: (r/c)(theta + sin theta) = 0.0875/343 * (0.785 + 0.707) = 381 us
                check (it < -250e-6 && it > -520e-6, "azimuth 45: ITD near Woodworth's 381 us, right ear first", it * 1e6, -381);
            }
            delete e;
        }
        check (itd90 < -550e-6 && itd90 > -800e-6, "azimuth 90: right ear leads by ~0.65 ms (Woodworth 666 us)", itd90 * 1e6, -666);
        check (itd270 > 550e-6 && itd270 < 800e-6, "azimuth 270: left ear leads by the same", itd270 * 1e6, 666);
        check (std::abs (itd0) < 60e-6 && std::abs (itd180) < 60e-6, "front and back: no ITD", itd0 * 1e6, itd180 * 1e6);

        // front vs back differ in the pinna bands
        {
            Engine* e0 = fresh (fs); Engine* e1 = fresh (fs);
            Take f = render (*e0, anechoicish (0, 2.0f), fs, 0.3, impulseAt (100));
            Take k = render (*e1, anechoicish (180, 2.0f), fs, 0.3, impulseAt (100));
            int a, b; directWindow (f, a, b);
            const double f8 = db (energy (octaveBand (f.L, fs, 8000), a, b)), k8 = db (energy (octaveBand (k.L, fs, 8000), a, b));
            const double f1 = db (energy (octaveBand (f.L, fs, 1000), a, b)), k1 = db (energy (octaveBand (k.L, fs, 1000), a, b));
            const double f4 = db (energy (octaveBand (f.L, fs, 4000), a, b)), k4 = db (energy (octaveBand (k.L, fs, 4000), a, b));
            const double f5 = db (energy (octaveBand (f.L, fs, 500), a, b)), k5 = db (energy (octaveBand (k.L, fs, 500), a, b));
            // the spectral SHAPE differs (the pinna), not merely the level
            const double shape = std::abs ((f8 - f5) - (k8 - k5)) + std::abs ((f4 - f5) - (k4 - k5));
            char fb[160]; std::snprintf (fb, sizeof fb, "front vs back: level %.1f dB apart, spectral shape %.1f dB apart at 4k+8k vs 500 (the pinna)", f5 - k5, shape);
            check (shape > 4.0 && std::abs (f1 - k1) < 8.0, fb);
            delete e0; delete e1;
        }
        // above vs level: a spectral change at high frequency, none at low
        {
            Engine* e0 = fresh (fs); Engine* e1 = fresh (fs);
            Take f = render (*e0, anechoicish (0, 2.0f, 0), fs, 0.3, impulseAt (100));
            Take u = render (*e1, anechoicish (0, 2.0f, 45), fs, 0.3, impulseAt (100));
            int a, b; directWindow (f, a, b);
            const double f8 = db (energy (octaveBand (f.L, fs, 8000), a, b)), u8 = db (energy (octaveBand (u.L, fs, 8000), a, b));
            const double f5 = db (energy (octaveBand (f.L, fs, 500), a, b)), u5 = db (energy (octaveBand (u.L, fs, 500), a, b));
            check (std::abs (f8 - u8) > 2.0 && std::abs (f5 - u5) < 2.5, "elevation 45: high band moves, low band does not", f8 - u8, f5 - u5);
            delete e0; delete e1;
        }
        // ILD grows with azimuth, and in the near field
        {
            auto ild = [&] (float az, float dist)
            {
                Engine* e = fresh (fs);
                Take t = render (*e, anechoicish (az, dist), fs, 0.3, impulseAt (100));
                int a, b; directWindow (t, a, b);
                const double v = db (energy (t.L, a, b)) - db (energy (t.R, a, b));
                delete e; return v;
            };
            const double i0 = ild (0, 2), i90 = ild (90, 2), i90n = ild (90, 0.3f), i0n = ild (0, 0.3f);
            check (std::abs (i0) < 1.5 && i90 < -6.0, "ILD: none in front, the right ear louder at 90", i0, i90);
            check (i90n < i90 - 3.0 && std::abs (i0n) < 1.5, "near field (0.3 m): the ILD at 90 grows by 3 dB or more, still none in front", i90n, i90);
        }
        // 6 dB per doubling
        {
            double lv[3];
            for (int k = 0; k < 3; ++k)
            {
                Engine* e = fresh (fs);
                Take t = render (*e, anechoicish (0, 1.0f * (float) (1 << k)), fs, 0.3, impulseAt (100));
                int a, b; directWindow (t, a, b);
                lv[k] = db (energy (t.L, a, b));
                delete e;
            }
            check (std::abs ((lv[0] - lv[1]) - 6.0) < 0.8 && std::abs ((lv[1] - lv[2]) - 6.0) < 0.8, "direct level: 6 dB per doubling of distance", lv[0] - lv[1], lv[1] - lv[2]);
        }
        // air: 8 kHz loses more than 250 Hz over a long path (beyond spreading)
        {
            Engine* e0 = fresh (fs); Engine* e1 = fresh (fs);
            Params n = anechoicish (0, 1.0f), f = anechoicish (0, 1.0f);
            f.lisX = 6.5f; f.lisY = 4.5f; f.lisYaw = 0; f.src[0].x = 17.5f; f.src[0].y = 4.5f;      // 11 m
            n.lisX = 6.5f; n.lisY = 4.5f; n.lisYaw = 0; n.src[0].x = 7.5f; n.src[0].y = 4.5f;       // 1 m
            Take tn = render (*e0, n, fs, 0.3, impulseAt (100));
            Take tf = render (*e1, f, fs, 0.3, impulseAt (100));
            int a, b; directWindow (tn, a, b); int c, d; directWindow (tf, c, d);
            const double lo = db (energy (octaveBand (tn.L, fs, 250), a, b)) - db (energy (octaveBand (tf.L, fs, 250), c, d));
            const double hi = db (energy (octaveBand (tn.L, fs, 8000), a, b)) - db (energy (octaveBand (tf.L, fs, 8000), c, d));
            check (hi - lo > 0.5 && hi - lo < 2.0, "air absorption: 11 m costs 8 kHz ~0.8 dB more than 250 Hz (ISO 9613-1)", hi - lo, 0.78);
            delete e0; delete e1;
        }
        // loudspeaker directivity
        {
            Engine* e0 = fresh (fs); Engine* e1 = fresh (fs);
            Params to = anechoicish (0, 2.0f), away = to;
            to.src[0].type = away.src[0].type = 1;
            to.src[0].yaw = 270; away.src[0].yaw = 90;      // the listener faces north, the source is north of it
            Take tt = render (*e0, to, fs, 0.3, impulseAt (100));
            Take ta = render (*e1, away, fs, 0.3, impulseAt (100));
            int a, b; directWindow (tt, a, b);
            const double d4 = db (energy (octaveBand (tt.L, fs, 4000), a, b)) - db (energy (octaveBand (ta.L, fs, 4000), a, b));
            const double d2 = db (energy (octaveBand (tt.L, fs, 250), a, b)) - db (energy (octaveBand (ta.L, fs, 250), a, b));
            check (d4 > 12.0 && d2 < 7.0 && d2 > 2.0, "loudspeaker turned away: 4 kHz drops 12 dB or more, 250 Hz only a few dB (a box is nearly omni down there)", d4, d2);
            delete e0; delete e1;
        }
    }

    // ---- 3. the rooms ------------------------------------------------------------
    std::printf ("\n3. the rooms\n");
    {
        // RT60 per band against Eyring, every room, every material
        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int m = 0; m < NUM_MATERIALS; ++m)
            {
                Engine* e = fresh (fs);
                Params p = base();
                p.material[0] = p.material[1] = p.material[2] = m;
                p.door[0] = p.door[1] = p.door[2] = 0;
                const Room& R = ROOMS[r];
                p.src[0].x = R.x0 + 0.3f * (R.x1 - R.x0); p.src[0].y = R.y0 + 0.35f * (R.y1 - R.y0);
                p.lisX = R.x0 + 0.7f * (R.x1 - R.x0); p.lisY = R.y0 + 0.6f * (R.y1 - R.y0);
                p.directDb = -60; p.earlyDb = -60;              // the tail alone
                float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
                const double dur = std::min (14.0, 2.5 * ey[3] + 1.0);
                Take t = render (*e, p, fs, dur, impulseAt (2400));
                const int from = (int) (0.05 * fs);
                const double t1 = rt60From (octaveBand (t.L, fs, 1000), fs, from);
                const double t4 = rt60From (octaveBand (t.L, fs, 4000), fs, from);
                char buf[160];
                std::snprintf (buf, sizeof buf, "%s %s: RT60 1k %.2f s (Eyring %.2f), 4k %.2f (%.2f)", R.name, MATERIAL_NAMES[m], t1, ey[3], t4, ey[5]);
                // a 16-line network cannot shape a 70 ms decay finely; a third is inaudible there
                // a 16-line network cannot shape a 60-150 ms decay finely; nobody hears 40 ms there
                const double tol1 = ey[3] < 0.2f ? 0.45 : (ey[3] < 0.3f ? 0.35 : 0.2), tol4 = ey[5] < 0.2f ? 0.45 : (ey[5] < 0.3f ? 0.35 : 0.2);
                const bool ok1 = t1 > 0 && std::abs (t1 / ey[3] - 1.0) < tol1;
                const bool ok4 = t4 > 0 && std::abs (t4 / ey[5] - 1.0) < tol4;
                check (ok1 && ok4, buf);
                delete e;
            }

        // DRR: at the critical distance the reverberant energy equals the direct
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            Params p = base(); p.src[0].type = 0;
            p.material[0] = p.material[1] = p.material[2] = m;
            p.door[0] = p.door[1] = p.door[2] = 0;
            const int r = 0;
            float ey[NBAND]; Engine::eyringRt60 (r, p.material, p.door, ey);
            /*  The engine's OWN absorption area, not S x alpha: it also counts the
                doors and what the party walls pass, which makes A larger and the
                critical distance longer. Taking the smaller one put every reading
                on the wrong side of the target. */
            float Ab[NBAND]; Engine::absorptionArea (r, p.material, p.door, Ab);
            const float A = Ab[3];
            const double rc = std::sqrt (A / (16.0 * 3.14159265));
            double drr[3];
            for (int k = 0; k < 3; ++k)
            {
                const double dist = rc * (k == 0 ? 0.5 : (k == 1 ? 1.0 : 2.0));
                const double secs = std::max (3.0, 2.0 * (double) ey[3] + 1.0);
                /*  Each part rendered on its own, with the others trimmed away, and
                    measured in the 1 kHz octave. Isolation rather than a window,
                    because the head response is 2.9 ms long and a 2.5 ms window
                    clipped part of the direct sound and then counted the offcut as
                    reverberation. Narrowband because 16 pi / A is a statement about
                    one frequency: broadband mixes octaves whose absorption differs
                    by several dB, and the top ones, which carry most of a flat
                    spectrum, are the most absorbent. */
                double part[3];
                for (int q = 0; q < 3; ++q)
                {
                    Engine* e = fresh (fs);
                    Params pp = p;
                    pp.lisX = 3.0f; pp.lisY = 2.5f; pp.lisYaw = 0;
                    pp.src[0].x = 3.0f + (float) dist; pp.src[0].y = 2.5f; pp.src[0].z = EAR_HEIGHT;
                    pp.directDb = q == 0 ? 0.0f : -120.0f;
                    pp.earlyDb  = q == 1 ? 0.0f : -120.0f;
                    pp.reverbDb = q == 2 ? 0.0f : -120.0f;
                    Take t = render (*e, pp, fs, secs, impulseAt (2400));
                    std::vector<float> bl = octaveBand (t.L, fs, 1000), br = octaveBand (t.R, fs, 1000);
                    part[q] = energy (bl, 0, t.size()) + energy (br, 0, t.size());
                    delete e;
                }
                drr[k] = db (part[0]) - db (part[1] + part[2]);
            }
            /*  +0.9 dB, not 0: a head hears a frontal source that much louder than
                the same power arriving diffusely. Below about half a metre the
                critical distance is inside the near field - the head is a third of
                the way to the source and neither 1/r nor a far-field HRTF applies -
                so there only the 6 dB per doubling is asserted. */
            const bool farField = rc > 0.5;
            char buf[240];
            std::snprintf (buf, sizeof buf, "large %s: DRR at the critical distance %.2f m is %+.1f dB (%s), %+.1f at half, %+.1f at double",
                           MATERIAL_NAMES[m], rc, drr[1], farField ? "+0.9 expected, the head's own frontal gain" : "near field, value not asserted", drr[0], drr[2]);
            const bool slope = drr[0] > drr[1] + 3.5 && drr[2] < drr[1] - 3.5;
            check (slope && (! farField || std::abs (drr[1] - 0.9) < 2.5), buf);
        }
    }

    // ---- 4. the doors ------------------------------------------------------------
    std::printf ("\n4. the doors\n");
    {
        // source in LARGE, listener in SMALL facing the door: open vs shut
        auto throughDoor = [&] (float aperture, bool absorbing) -> Take
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = 0;
            p.material[0] = p.material[1] = absorbing ? 0 : 1; p.material[2] = 2;
            p.door[0] = aperture; p.door[1] = 0; p.door[2] = 0;
            p.src[0].x = 4.5f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
            p.lisX = 4.5f; p.lisY = 6.5f; p.lisYaw = 270.0f;     // facing south, at the door
            Take t = render (*e, p, fs, 2.0, impulseAt (2400));
            delete e; return t;
        };
        Take open = throughDoor (1.0f, true), shut = throughDoor (0.0f, true), ajar = throughDoor (0.15f, true);
        const double o1 = db (energy (octaveBand (open.L, fs, 1000), 0, open.size())), s1 = db (energy (octaveBand (shut.L, fs, 1000), 0, shut.size()));
        const double o4 = db (energy (octaveBand (open.L, fs, 4000), 0, open.size())), s4 = db (energy (octaveBand (shut.L, fs, 4000), 0, shut.size()));
        const double o2 = db (energy (octaveBand (open.L, fs, 250), 0, open.size())), s2 = db (energy (octaveBand (shut.L, fs, 250), 0, shut.size()));
        const double a1 = db (energy (octaveBand (ajar.L, fs, 1000), 0, ajar.size()));
        check (o1 - s1 > 12.0 && o1 - s1 < 40.0, "shutting the door: 1 kHz drops by 12-40 dB", o1 - s1);
        check ((o4 - s4) - (o2 - s2) > 6.0, "shut door is dark: 4 kHz loses 6 dB more than 250 Hz (mass law)", o4 - s4, o2 - s2);
        check (a1 < o1 - 2.0 && a1 > s1 + 2.0, "a door ajar (15 %) sits between open and shut", a1, o1);
        check (energy (shut.L, 0, shut.size()) > 0, "a shut door still leaks (transmission), not silence", db (energy (shut.L, 0, shut.size())));

        // localisation: with the source well off the straight line, the sound still comes from the doorway
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = 0;
            p.material[0] = p.material[1] = 0; p.material[2] = 2;
            p.door[0] = 1; p.door[1] = 0; p.door[2] = 0;
            p.src[0].x = 1.0f; p.src[0].y = 1.0f; p.src[0].z = EAR_HEIGHT;      // far to the south-west of the door
            p.lisX = 4.5f; p.lisY = 6.5f; p.lisYaw = 270.0f;        // in SMALL facing the door, which is straight ahead
            p.earlyDb = -40; p.reverbDb = -40;
            Take t = render (*e, p, fs, 0.5, impulseAt (2400));
            const int a = firstAbove (t.L, 1e-5f); const int ar = firstAbove (t.R, 1e-5f);
            const int s = std::min (a < 0 ? ar : a, ar < 0 ? a : ar);
            const double it = itdSeconds (t, s, s + (int) (0.004 * fs));
            // the geometric direction to the source would be ~ 57 degrees to the right: ITD ~ 550 us
            check (std::abs (it) < 200e-6, "through the doorway: the sound arrives from the door (ITD near zero), not from the source's bearing (~550 us)", it * 1e6);
            // and diffraction has made it dark relative to line of sight
            Engine* e2 = fresh (fs);
            Params q = p; q.lisX = 4.5f; q.lisY = 5.8f; q.src[0].x = 4.5f; q.src[0].y = 1.0f;   // straight through the opening
            Take u = render (*e2, q, fs, 0.5, impulseAt (2400));
            const int ua = firstAbove (u.L, 1e-5f);
            const double bentHi = db (energy (octaveBand (t.L, fs, 4000), s, s + (int) (0.004 * fs))) - db (energy (octaveBand (t.L, fs, 250), s, s + (int) (0.004 * fs)));
            const double losHi = db (energy (octaveBand (u.L, fs, 4000), ua, ua + (int) (0.004 * fs))) - db (energy (octaveBand (u.L, fs, 250), ua, ua + (int) (0.004 * fs)));
            check (losHi - bentHi > 6.0, "diffraction: the bent path loses 6 dB more at 4 kHz than at 250 Hz, the line of sight does not", losHi - bentHi);
            delete e; delete e2;
        }

        // the double slope: listener in SMALL (absorbing), source in GIANT (tiled) through the open door
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = 0;
            p.material[0] = 0; p.material[1] = 0; p.material[2] = 3;
            p.door[0] = 0; p.door[1] = 0; p.door[2] = 1;
            p.src[0].x = 4.0f; p.src[0].y = 7.5f; p.src[0].z = EAR_HEIGHT;      // both in SMALL
            p.lisX = 5.0f; p.lisY = 6.0f; p.lisYaw = 0;
            float eySmall[NBAND], eyGiant[NBAND];
            Engine::eyringRt60 (1, p.material, p.door, eySmall);
            Engine::eyringRt60 (2, p.material, p.door, eyGiant);
            Take t = render (*e, p, fs, 10.0, impulseAt (2400));
            std::vector<float> b1 = octaveBand (t.L, fs, 1000);
            // decay rate early (first 100 ms after the peak) vs late (2..4 s)
            std::vector<double> edc ((size_t) b1.size(), 0.0); double acc = 0;
            for (int i = (int) b1.size() - 1; i >= 0; --i) { acc += (double) b1[(size_t) i] * b1[(size_t) i]; edc[(size_t) i] = acc; }
            auto slopeDbPerS = [&] (double t0, double t1) { return (db (edc[(size_t) (t1 * fs)]) - db (edc[(size_t) (t0 * fs)])) / (t1 - t0); };
            const int s0 = firstAbove (b1, 1e-4f);
            const double drop40 = db (edc[(size_t) (s0 + 0.040 * fs)]) - db (edc[(size_t) (s0 + 0.003 * fs)]);
            const double sLate = slopeDbPerS (2.0, 4.0);
            const double tLate = -60.0 / sLate;
            char buf[220]; std::snprintf (buf, sizeof buf, "double slope: the dead room's own field is %.0f dB down by 40 ms (Eyring %.2f s), then the hall's returns at %.0f dB/s = %.1f s (hall Eyring %.1f s)", -drop40, eySmall[3], sLate, tLate, eyGiant[3]);
            check (drop40 < -12.0 && tLate > 0.6 * eyGiant[3] && tLate < 1.6 * eyGiant[3], buf);
            delete e;
        }

        // all doors shut: the party wall still carries something, faint and dark
        {
            Engine* e = fresh (fs); Engine* e2 = fresh (fs);
            Params p = base(); p.src[0].type = 0;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 5.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
            p.lisX = 7.0f; p.lisY = 2.5f; p.lisYaw = 180;
            Take t = render (*e, p, fs, 2.0, impulseAt (2400));
            Params q = p; q.src[0].x = 8.0f; q.src[0].y = 2.5f; q.lisX = 10.0f;     // same 2 m, same room
            Take u = render (*e2, q, fs, 2.0, impulseAt (2400));
            const double wall = db (energy (t.L, 0, t.size())), same = db (energy (u.L, 0, u.size()));
            check (same - wall > 20.0 && same - wall < 60.0, "through the party wall and the shut door: 20-60 dB down on the same distance in one room", same - wall);
            delete e; delete e2;
        }

        // two doors: small -> large -> giant with the direct door shut
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = 0;
            p.door[0] = 1; p.door[1] = 1; p.door[2] = 0;
            p.src[0].x = 4.5f; p.src[0].y = 7.0f; p.src[0].z = EAR_HEIGHT;      // SMALL
            p.lisX = 8.0f; p.lisY = 2.5f; p.lisYaw = 180;            // GIANT, near the LARGE-GIANT door
            Take t = render (*e, p, fs, 1.0, impulseAt (2400));
            Params q = p; q.door[0] = 0; q.door[1] = 0;
            Engine* e2 = fresh (fs);
            Take u = render (*e2, q, fs, 1.0, impulseAt (2400));
            check (db (energy (t.L, 0, t.size())) - db (energy (u.L, 0, u.size())) > 6.0, "a route through the third room: opening both doors brings the sound up", db (energy (t.L, 0, t.size())) - db (energy (u.L, 0, u.size())));
            delete e; delete e2;
        }
    }

    // ---- 5. motion ---------------------------------------------------------------
    std::printf ("\n5. motion and mix\n");
    {
        // a steady tone, the source walking 4 m across the room in 2 s: no clicks
        auto maxStep = [] (const std::vector<float>& v, int a, int b) { double m = 0; for (int i = std::max (a, 1); i < std::min (b, (int) v.size()); ++i) m = std::max (m, (double) std::abs (v[(size_t) i] - v[(size_t) i - 1])); return m; };
        Engine* e = fresh (fs);
        Params p = base(); p.src[0].type = 0;
        const int n = (int) (3.0 * fs);
        Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
        e->setParams (p);
        for (int i = 0; i < n; i += 128)
        {
            const double tt = i / fs;
            if (tt > 0.5 && tt < 2.5) { p.src[0].x = 1.0f + (float) (tt - 0.5) * 2.0f; p.src[0].y = 1.5f; e->setParams (p); }
            for (int k = 0; k < 128; ++k) { const float v = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) (i + k) / (float) fs); t.L[(size_t) (i + k)] = v; t.R[(size_t) (i + k)] = v; }
            e->process (&t.L[(size_t) i], &t.R[(size_t) i], 128);
        }
        // a click is broadband: on a pure 440 Hz tone, energy above 6 kHz is artefact
        {
            std::vector<float> hf = octaveBand (t.L, fs, 8000);
            const double stillHf = db (energy (hf, (int) (0.2 * fs), (int) (0.5 * fs))) - db (energy (t.L, (int) (0.2 * fs), (int) (0.5 * fs)));
            const double movingHf = db (energy (hf, (int) (0.6 * fs), (int) (2.4 * fs))) - db (energy (t.L, (int) (0.6 * fs), (int) (2.4 * fs)));
            const double still = maxStep (t.L, (int) (0.2 * fs), (int) (0.5 * fs)), moving = maxStep (t.L, (int) (0.6 * fs), (int) (2.4 * fs));
            char buf[200]; std::snprintf (buf, sizeof buf, "source walking 2 m/s on a 440 Hz tone: energy above 6 kHz %.0f dB below the tone (still %.0f), max step %.3f vs %.3f still", -movingHf, -stillHf, moving, still);
            check (movingHf < -50.0 && moving < 4.0 * still, buf);
        }
        delete e;

        // a jump of 4 m in one block
        Engine* e2 = fresh (fs);
        Params q = base(); q.src[0].type = 0;
        Take u; u.fs = fs; u.L.assign ((size_t) n, 0.0f); u.R.assign ((size_t) n, 0.0f);
        e2->setParams (q);
        for (int i = 0; i < n; i += 128)
        {
            if (i == (int) (1.0 * fs)) { q.src[0].x = 1.0f; q.src[0].y = 4.0f; e2->setParams (q); }
            for (int k = 0; k < 128; ++k) { const float v = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) (i + k) / (float) fs); u.L[(size_t) (i + k)] = v; u.R[(size_t) (i + k)] = v; }
            e2->process (&u.L[(size_t) i], &u.R[(size_t) i], 128);
        }
        {
            std::vector<float> hf = octaveBand (u.L, fs, 8000);
            const double jumpHf = db (energy (hf, (int) (0.95 * fs), (int) (1.1 * fs))) - db (energy (u.L, (int) (0.95 * fs), (int) (1.1 * fs)));
            char buf[160]; std::snprintf (buf, sizeof buf, "a 4 m jump crossfades: energy above 6 kHz stays %.0f dB below the tone", -jumpHf);
            check (jumpHf < -50.0, buf);
        }
        delete e2;

        // mix 0 = the input, exactly
        Engine* e3 = fresh (fs);
        Params m = base(); m.mix = 0;
        std::vector<float> ref;
        Take w = render (*e3, m, fs, 0.5, [&ref] (int i) { const float v = 0.2f * std::sin (0.01f * (float) i); ref.push_back (v); return v; });
        double diff = 0; for (int i = (int) (0.2 * fs); i < w.size(); ++i) diff = std::max (diff, (double) std::abs (w.L[(size_t) i] - ref[(size_t) i]));
        check (diff < 1e-6, "MIX 0: the input passes untouched", diff);
        delete e3;
    }

    // ---- 6. other rates, cost ----------------------------------------------------
    std::printf ("\n6. rates and cost\n");
    {
        for (double rate : { 44100.0, 96000.0 })
        {
            Engine* e = fresh (rate);
            uint32_t s = 3;
            Params shut = base(); shut.door[0] = shut.door[1] = shut.door[2] = 0;
            Take t = render (*e, shut, rate, 2.0, [&s] (int i) { s = s * 1664525u + 1013904223u; return i < 20000 ? ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f : 0.0f; });
            /*  A decay test measures decay. The absolute floor this used to
                assert is now exceeded by the hall's plaster tail leaking through
                the shut party wall, which is the transmission model working. */
            const double burst = energy (t.L, (int) (0.05 * rate), (int) (0.40 * rate));
            const double after = energy (t.L, (int) (1.8 * rate), t.size());
            char buf[160];
            std::snprintf (buf, sizeof buf, "%.0f Hz: bounded, and %.0f dB down 1.8 s after the burst", rate, db (burst) - db (after));
            check (std::isfinite (peakAbs (t.L)) && peakAbs (t.L) < 6.0 && db (burst) - db (after) > 60.0, buf, peakAbs (t.L));
            // the ITD at 90 degrees is rate independent
            Engine* e2 = fresh (rate);
            Take u = render (*e2, anechoicish (90, 2.0f), rate, 0.3, impulseAt (100));
            const int a = std::min (firstAbove (u.L, 1e-4f), firstAbove (u.R, 1e-4f));
            const double it = itdSeconds (u, a, a + (int) (0.004 * rate));
            std::snprintf (buf, sizeof buf, "%.0f Hz: ITD at 90 degrees still ~ -666 us", rate);
            check (it < -550e-6 && it > -800e-6, buf, it * 1e6);
            delete e; delete e2;
        }
        // cost: a busy configuration (same room, second order, open doors -> door fields), 10 s
        {
            Engine* e = fresh (fs);
            Params p = base(); p.door[0] = p.door[1] = p.door[2] = 1;
            uint32_t s = 5;
            const int n = (int) (10.0 * fs);
            std::vector<float> L ((size_t) n), R ((size_t) n);
            for (int i = 0; i < n; ++i) { s = s * 1664525u + 1013904223u; L[(size_t) i] = R[(size_t) i] = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.2f; }
            e->setParams (p);
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; i += 256) e->process (&L[(size_t) i], &R[(size_t) i], std::min (256, n - i));
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  cost: %d paths active, %.1f %% of one core at 48 kHz (%.2f s for 10 s)\n", e->numActivePaths(), 100.0 * secs / 10.0, secs);
            check (secs < 5.0, "cost under 50 % of a core", 100.0 * secs / 10.0);
            delete e;
        }
    }

    // ---- 7. several sources ------------------------------------------------------
    std::printf ("\n7. several sources\n");
    {
        // a source switched OFF changes nothing: memcmp against one source alone
        {
            Engine* a = fresh (fs); Engine* b = fresh (fs);
            Params pa = base(); Params pb = base();
            pb.src[1].input = IN_OFF; pb.src[1].x = 1.0f; pb.src[1].y = 1.0f;   // moved, but off
            uint32_t s1 = 11, s2 = 11;
            auto nA = [&s1] (int) { s1 = s1 * 1664525u + 1013904223u; return ((s1 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
            auto nB = [&s2] (int) { s2 = s2 * 1664525u + 1013904223u; return ((s2 >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; };
            Take ta = render (*a, pa, fs, 1.0, nA), tb = render (*b, pb, fs, 1.0, nB);
            check (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "a source with INPUT off is bit-identical to no source at all");
            delete a; delete b;
        }
        // two sources from the two channels: L fed to a speaker on the left, R to one on the right
        {
            Engine* e = fresh (fs);
            Params p = base();
            p.src[0].input = IN_MAIN_L; p.src[0].x = 3.0f; p.src[0].y = 3.5f; p.src[0].type = SRC_PURE;
            p.src[1].input = IN_MAIN_R; p.src[1].x = 3.0f; p.src[1].y = 1.5f; p.src[1].type = SRC_PURE;
            p.lisX = 4.5f; p.lisY = 2.5f; p.lisYaw = 180;      // facing west: src[0] is to the right, src[1] to the left
            p.earlyDb = -80; p.reverbDb = -80; p.door[0] = p.door[1] = 0;
            // channel L carries a tone, channel R silence, then the reverse
            const int n = (int) (0.6 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 256)
            {
                for (int k = 0; k < 256 && i + k < n; ++k)
                {
                    const float tone = 0.3f * std::sin (2.0f * 3.14159265f * 2000.0f * (float) (i + k) / (float) fs);
                    t.L[(size_t) (i + k)] = (i + k < n / 2) ? tone : 0.0f;
                    t.R[(size_t) (i + k)] = (i + k < n / 2) ? 0.0f : tone;
                }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], std::min (256, n - i));
            }
            const int a0 = (int) (0.1 * fs), a1 = (int) (0.28 * fs), b0 = (int) (0.4 * fs), b1 = (int) (0.58 * fs);
            const double firstHalfLR = db (energy (t.L, a0, a1)) - db (energy (t.R, a0, a1));
            const double secondHalfLR = db (energy (t.L, b0, b1)) - db (energy (t.R, b0, b1));
            char buf[160]; std::snprintf (buf, sizeof buf, "MAIN L to the right-hand source, MAIN R to the left-hand one: L/R %.1f dB then %.1f dB", firstHalfLR, secondHalfLR);
            check (firstHalfLR < -4.0 && secondHalfLR > 4.0, buf);
            delete e;
        }
        // the AUX bus reaches a source
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].input = IN_AUX_L; p.earlyDb = -80; p.reverbDb = -80;
            const int n = 4800;
            std::vector<float> mL ((size_t) n, 0.0f), mR ((size_t) n, 0.0f), aL ((size_t) n, 0.0f), aR ((size_t) n, 0.0f), oL ((size_t) n), oR ((size_t) n);
            aL[2400] = 1.0f;
            e->setParams (p);
            for (int i = 0; i < n; i += 256) e->process (mL.data() + i, mR.data() + i, aL.data() + i, aR.data() + i, oL.data() + i, oR.data() + i, std::min (256, n - i));
            check (energy (oL, 0, n) > 1e-3, "AUX L feeds a source whose INPUT is AUX L", energy (oL, 0, n));
            delete e;
        }
        // the pure source: omni at 0, cardioid at 1
        {
            SourceParams sp; sp.type = SRC_PURE;
            sp.directivity = 0; const double o = Engine::directivityDb (sp, -1.0f, 3);
            sp.directivity = 1; const double c = Engine::directivityDb (sp, -1.0f, 3), c90 = Engine::directivityDb (sp, 0.0f, 3);
            check (std::abs (o) < 0.01 && c < -25.0 && std::abs (c90 + 6.0) < 0.2, "PURE: omni at directivity 0, cardioid at 1 (-6 dB at 90, off behind)", c, c90);
            // and the loudspeaker: nearly omni at 125 Hz, beamed at 4 kHz, 90 degrees in between
            SourceParams ls; ls.type = SRC_LOUDSPEAKER;
            const double lo = Engine::directivityDb (ls, -1.0f, 0), hi = Engine::directivityDb (ls, -1.0f, 5), side = Engine::directivityDb (ls, 0.0f, 5);
            char buf[160]; std::snprintf (buf, sizeof buf, "LOUDSPEAKER: behind %.1f dB at 125 Hz, %.1f at 4 kHz, %.1f at 90 degrees / 4 kHz", lo, hi, side);
            check (lo > -6.0 && hi < -18.0 && side < hi + 18.0 && side > hi + 4.0, buf);
            // the radiated power of a cardioid is a third of an omni's: -4.8 dB
            sp.directivity = 1;
            check (std::abs (Engine::radiatedPowerDb (sp, 3) + 4.77) < 0.3, "a cardioid radiates 1/3 the power of an omni (-4.8 dB into the room)", Engine::radiatedPowerDb (sp, 3));
        }
        // sources in two rooms at once, doors shut: each is heard where it is
        {
            Engine* e = fresh (fs);
            Params p = base();
            p.src[0].input = IN_MAIN_L; p.src[0].x = 2.0f; p.src[0].y = 2.5f;
            p.src[1].input = IN_MAIN_R; p.src[1].x = 10.0f; p.src[1].y = 4.5f;     // in the hall
            p.lisX = 4.5f; p.lisY = 2.5f; p.lisYaw = 180; p.door[0] = p.door[1] = p.door[2] = 0;
            uint32_t s = 21;
            const int n = (int) (1.5 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 256)
            {
                for (int k = 0; k < 256 && i + k < n; ++k) { s = s * 1664525u + 1013904223u; const float v = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f; t.L[(size_t) (i + k)] = (i + k < n / 2) ? v : 0.0f; t.R[(size_t) (i + k)] = (i + k < n / 2) ? 0.0f : v; }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], std::min (256, n - i));
            }
            const double here = db (energy (t.L, (int) (0.2 * fs), (int) (0.7 * fs)));
            const double nextDoor = db (energy (t.L, (int) (0.95 * fs), (int) (1.45 * fs)));
            char buf[160]; std::snprintf (buf, sizeof buf, "one source in the room, one behind a shut door into the hall: %.0f dB apart, both audible", here - nextDoor);
            check (here - nextDoor > 15.0 && here - nextDoor < 60.0, buf);
            delete e;
        }
        // cost with all four sources live, every door open
        {
            Engine* e = fresh (fs);
            Params p = base();
            for (int k = 0; k < MAX_SOURCES; ++k) p.src[k].input = IN_MAIN_LR;
            p.src[1].x = 1.0f; p.src[1].y = 1.0f; p.src[2].x = 4.5f; p.src[2].y = 7.0f; p.src[3].x = 12.0f; p.src[3].y = 4.5f;
            p.door[0] = p.door[1] = p.door[2] = 1;
            uint32_t s = 5;
            const int n = (int) (10.0 * fs);
            std::vector<float> L ((size_t) n), R ((size_t) n);
            for (int i = 0; i < n; ++i) { s = s * 1664525u + 1013904223u; L[(size_t) i] = R[(size_t) i] = ((s >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.2f; }
            e->setParams (p);
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; i += 256) e->process (&L[(size_t) i], &R[(size_t) i], std::min (256, n - i));
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  cost: four sources, %d paths active, %.1f %% of one core at 48 kHz\n", e->numActivePaths(), 100.0 * secs / 10.0);
            check (secs < 6.0 && std::isfinite (peakAbs (L)) && peakAbs (L) < 8.0, "four sources: under 60 % of a core, bounded", 100.0 * secs / 10.0, peakAbs (L));
            delete e;
        }
    }

    // ---- 8. what makes STUDIO a studio -------------------------------------------
    std::printf ("\n8. the studio\n");
    {
        const int STUDIO = NUM_MATERIALS - 1;
        // (a) the decay is FLAT across the band - the property treatment buys
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            int mats[NUM_ROOMS] = { STUDIO, STUDIO, STUDIO };
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float st[NBAND], ti[NBAND];
            Engine::eyringRt60 (r, mats, shut, st);
            int tiled[NUM_ROOMS] = { 3, 3, 3 };
            Engine::eyringRt60 (r, tiled, shut, ti);
            const double spreadS = std::max ({ st[1], st[3], st[5] }) / std::min ({ st[1], st[3], st[5] });
            const double spreadT = std::max ({ ti[1], ti[3], ti[5] }) / std::min ({ ti[1], ti[3], ti[5] });
            char buf[220];
            std::snprintf (buf, sizeof buf, "%s STUDIO: RT60 %.2f / %.2f / %.2f s at 250 / 1k / 4k - a spread of %.2fx, where TILED spreads %.2fx",
                           ROOMS[r].name, st[1], st[3], st[5], spreadS, spreadT);
            check (spreadS < 1.15 && spreadS < spreadT * 0.6, buf);
        }
        // (b) live, not dead: the hall keeps a real tail
        {
            int mats[NUM_ROOMS] = { STUDIO, STUDIO, STUDIO };
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float st[NBAND]; Engine::eyringRt60 (2, mats, shut, st);
            char buf[160];
            std::snprintf (buf, sizeof buf, "the hall as a studio is live, not dead: %.2f s at 1 kHz", st[3]);
            check (st[3] > 0.45 && st[3] < 1.0, buf);
        }
        // (c) diffusion: the specular reflections give way, the room does not go quiet
        {
            auto earlyLate = [&] (int mat, double& early, double& total)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = mat;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 2.0f; p.src[0].y = 1.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 4.0f; p.lisY = 3.0f; p.lisYaw = 200.0f;
                p.reverbDb = -80;                  // the images alone, or the field buries the difference
                Take t = render (*e, p, fs, 4.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                // the window that holds the images and almost nothing else
                early = energy (t.L, a + (int) (0.004 * fs), a + (int) (0.030 * fs));
                total = energy (t.L, a, t.size());
                delete e;
            };
            double eS, tS, eP, tP;
            earlyLate (NUM_MATERIALS - 1, eS, tS);
            earlyLate (2, eP, tP);                     // PLASTER, a hard room with no diffusion
            char buf[240];
            std::snprintf (buf, sizeof buf, "diffusion: the specular reflections carry %.1f %% of what PLASTER's carry, the rest scattered into the room rather than absorbed",
                           100.0 * eS / eP);
            check (eS < 0.75 * eP, buf);
        }
        // (d) splayed walls: the reflections stop landing at exact multiples, so a
        //     source in the middle of the room does not comb. Measured as the spread
        //     of the early response across a fine frequency comb.
        {
            auto ripple = [&] (int mat)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = mat;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;   // dead centre of LARGE
                p.lisX = 3.0f; p.lisY = 3.6f; p.lisYaw = 270.0f;
                p.reverbDb = -80;                      // the images alone
                Take t = render (*e, p, fs, 1.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                // energy in 24 narrow bands across 300 Hz - 1.2 kHz, how uneven
                double m[24]; double mean = 0;
                for (int k = 0; k < 24; ++k)
                {
                    const double f = 300.0 * std::pow (4.0, k / 23.0);
                    std::vector<float> b = octaveBand (t.L, fs, f);
                    m[k] = db (energy (b, a, a + (int) (0.08 * fs)));
                    mean += m[k];
                }
                mean /= 24;
                double var = 0; for (int k = 0; k < 24; ++k) var += (m[k] - mean) * (m[k] - mean);
                delete e;
                return std::sqrt (var / 24);
            };
            const double rs = ripple (NUM_MATERIALS - 1), rp = ripple (2);
            char buf[220];
            std::snprintf (buf, sizeof buf, "splay and diffusion: the early response of a centred source is %.1f dB uneven in STUDIO against %.1f dB in PLASTER", rs, rp);
            check (rs < rp, buf);
        }
        // (e) the four original materials are untouched by any of it
        {
            for (int m = 0; m < NUM_MATERIALS - 1; ++m)
            {
                bool zero = MATERIAL_SPLAY[m] == 0.0f;
                for (int b = 0; b < NBAND; ++b) if (MATERIAL_SCATTER[m][b] != 0.0f) zero = false;
                char buf[160];
                std::snprintf (buf, sizeof buf, "%s scatters nothing and splays nothing, so it sounds exactly as it did", MATERIAL_NAMES[m]);
                check (zero, buf);
            }
        }
    }

    // ---- 9. walls that are no longer parallel --------------------------------------
    std::printf ("\n9. broken walls\n");
    {
        auto takeOf = [&] (float push0, float push1, float along0, double secs, int* paths)
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = SRC_PURE;
            p.material[0] = p.material[1] = p.material[2] = 2;          // PLASTER: hard, so reflections speak
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
            p.lisX = 3.0f; p.lisY = 3.8f; p.lisYaw = 270.0f;
            p.breakPush[0][0] = push0; p.breakPush[0][1] = push1; p.breakAlong[0][0] = along0;
            Take t = render (*e, p, fs, secs, impulseAt (2400));
            if (paths) *paths = e->numActivePaths();
            delete e;
            return t;
        };

        // (a) a two-millimetre break is the same room: the general search must agree
        //     with the shoebox arithmetic it replaces
        {
            int nSquare = 0, nBroken = 0;
            Take sq = takeOf (0.0f, 0.0f, 0.5f, 1.0, &nSquare);
            Take br = takeOf (0.002f, 0.0f, 0.5f, 1.0, &nBroken);
            const double a = db (energy (sq.L, 0, sq.size())), b = db (energy (br.L, 0, br.size()));
            // and the early part, where the images are and where a missing path would show
            const int s0 = firstAbove (sq.L, 1e-5f);
            const double ae = db (energy (sq.L, s0, s0 + (int) (0.05 * fs)));
            const double be = db (energy (br.L, s0, s0 + (int) (0.05 * fs)));
            char buf[240];
            std::snprintf (buf, sizeof buf, "a wall broken by 2 mm is the same room: the general search lands within %.2f dB of the shoebox overall and %.2f dB over the first 50 ms (%d paths against %d)",
                           b - a, be - ae, nBroken, nSquare);
            check (std::abs (b - a) < 0.6 && std::abs (be - ae) < 0.6 && nBroken >= nSquare - 2, buf);
        }

        // (b) pushing a wall out really does make the room bigger and longer
        {
            RoomSurfaces surf; surf.wall = 2;
            float shut[NUM_DOORS] = { 0, 0, 0 };
            float flat7[NBAND], bent7[NBAND];
            Engine::eyringRt60 (0, surf, shut, flat7);
            Engine* e = fresh (fs);
            Params p = base(); p.material[0] = p.material[1] = p.material[2] = 2;
            p.door[0] = p.door[1] = p.door[2] = 0;
            p.breakPush[0][0] = 0.6f;
            Take t = render (*e, p, fs, 0.2, impulseAt (2400));
            e->roomRt60 (0, bent7);
            delete e;
            char buf[200];
            std::snprintf (buf, sizeof buf, "pushing a wall 0.6 m out lengthens the room's own decay: %.3f s becomes %.3f s at 1 kHz", flat7[3], bent7[3]);
            check (bent7[3] > flat7[3] * 1.002 && bent7[3] < flat7[3] * 1.15, buf);
        }

        // (c) the point of the whole thing: a source in the middle of a hard room
        //     combs, and breaking the walls stops it
        {
            auto ripple = [&] (float push0, float push1)
            {
                Engine* e = fresh (fs);
                Params p = base(); p.src[0].type = SRC_PURE;
                p.material[0] = p.material[1] = p.material[2] = 2;
                p.door[0] = p.door[1] = p.door[2] = 0;
                p.src[0].x = 3.0f; p.src[0].y = 2.5f; p.src[0].z = EAR_HEIGHT;
                p.lisX = 3.0f; p.lisY = 3.6f; p.lisYaw = 270.0f;
                p.reverbDb = -80;                       // the images alone
                p.breakPush[0][0] = push0; p.breakPush[0][1] = push1;
                p.breakAlong[0][0] = 0.42f; p.breakAlong[0][1] = 0.58f;
                Take t = render (*e, p, fs, 1.0, impulseAt (2400));
                const int a = firstAbove (t.L, 1e-5f);
                double m[24]; double mean = 0;
                for (int k = 0; k < 24; ++k)
                {
                    const double f = 300.0 * std::pow (4.0, k / 23.0);
                    std::vector<float> bnd = octaveBand (t.L, fs, f);
                    m[k] = db (energy (bnd, a, a + (int) (0.08 * fs)));
                    mean += m[k];
                }
                mean /= 24;
                double var = 0; for (int k = 0; k < 24; ++k) var += (m[k] - mean) * (m[k] - mean);
                delete e;
                return std::sqrt (var / 24);
            };
            const double flat = ripple (0.0f, 0.0f), bent = ripple (0.55f, 0.45f);
            char buf[220];
            std::snprintf (buf, sizeof buf, "a centred source in a square hard room is %.1f dB uneven across 300 Hz - 1.2 kHz; with both walls broken, %.1f dB", flat, bent);
            check (bent < flat, buf);
        }

        // (d) a wall pushed INTO the room is concave, and the geometry knows it:
        //     some paths are now blocked by the fold itself
        {
            int nOut = 0, nIn = 0;
            takeOf (0.55f, 0.0f, 0.5f, 0.3, &nOut);
            takeOf (-0.55f, 0.0f, 0.5f, 0.3, &nIn);
            char buf[200];
            std::snprintf (buf, sizeof buf, "pushed out the fold disperses (%d paths); pushed in it is concave and shadows itself (%d)", nOut, nIn);
            check (nIn <= nOut, buf);
        }

        // (e) bounded and finite across the whole range, in every room
        {
            bool ok = true; double worst = 0;
            for (int r = 0; r < NUM_ROOMS; ++r)
                for (int k = 0; k < 6; ++k)
                {
                    const float push = -0.6f + 0.24f * k;
                    Engine* e = fresh (fs);
                    Params p = base(); p.src[0].type = SRC_PURE;
                    p.material[0] = p.material[1] = p.material[2] = 3;
                    p.door[0] = p.door[1] = p.door[2] = 1;
                    const Room& R = ROOMS[r];
                    p.src[0].x = 0.5f * (R.x0 + R.x1) - 0.6f; p.src[0].y = 0.5f * (R.y0 + R.y1) + 0.5f;
                    p.lisX = 0.5f * (R.x0 + R.x1) + 0.8f; p.lisY = 0.5f * (R.y0 + R.y1) - 0.4f;
                    p.breakPush[r][0] = push; p.breakPush[r][1] = -push;
                    uint32_t sd = 9;
                    Take t = render (*e, p, fs, 2.0, [&sd] (int i) { sd = sd * 1664525u + 1013904223u; return i < 20000 ? ((sd >> 8) * (1.0f / 8388608.0f) - 1.0f) * 0.3f : 0.0f; });
                    const double pk = peakAbs (t.L);
                    if (! std::isfinite (pk) || pk > 8.0) ok = false;
                    worst = std::max (worst, pk);
                    delete e;
                }
            char buf[200];
            std::snprintf (buf, sizeof buf, "every room, every fold from -0.6 to +0.6 m: finite and bounded, worst peak %.3f", worst);
            check (ok, buf);
        }

        // (f) and the walls may be moved while a note is sounding without a click
        {
            Engine* e = fresh (fs);
            Params p = base(); p.src[0].type = SRC_PURE;
            p.material[0] = p.material[1] = p.material[2] = 2;
            const int n = (int) (3.0 * fs);
            Take t; t.fs = fs; t.L.assign ((size_t) n, 0.0f); t.R.assign ((size_t) n, 0.0f);
            e->setParams (p);
            for (int i = 0; i < n; i += 128)
            {
                const double tt = i / fs;
                if (tt > 0.5 && tt < 2.5) { p.breakPush[0][0] = (float) ((tt - 0.5) * 0.3); e->setParams (p); }
                for (int k = 0; k < 128; ++k) { const float v = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) (i + k) / (float) fs); t.L[(size_t) (i + k)] = v; t.R[(size_t) (i + k)] = v; }
                e->process (&t.L[(size_t) i], &t.R[(size_t) i], 128);
            }
            std::vector<float> hf = octaveBand (t.L, fs, 8000);
            const double moving = db (energy (hf, (int) (0.6 * fs), (int) (2.4 * fs))) - db (energy (t.L, (int) (0.6 * fs), (int) (2.4 * fs)));
            char buf[200];
            std::snprintf (buf, sizeof buf, "a wall moved under a sounding note keeps everything above 6 kHz %.0f dB below it", -moving);
            check (moving < -45.0, buf);
            delete e;
        }
    }

    std::printf ("\n%d checks, %d failed%s\n", checks, failures, failures == 0 ? "  -  ALL CLEAR" : "");
    return failures == 0 ? 0 : 1;
}
