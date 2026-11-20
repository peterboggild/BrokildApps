// DRUMEQ offline bench - plain C++17, no JUCE. Every claim the panel and the
// manual make is measured here.
//
//   cmake -S test -B <build> && cmake --build <build> --config Release

#include "../engine/de_engine.h"
#include "../engine/de_sources.h"
#include "../engine/de_find.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

#if defined(_MSC_VER)
  #include <float.h>
#endif

using namespace de;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("  FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    void noDenormals()
    {
       #if defined(_MSC_VER)
        _controlfp_s (nullptr, _DN_FLUSH, _MCW_DN);
       #endif
    }

    //  an acoustic-ish drum stroke: a few decaying modes with a pitch drop,
    //  plus a short noise strike
    void addHit (std::vector<float>& x, int at, double fs, double f0, const std::vector<double>& ratios,
                 double amp, double decay, std::mt19937& rng, double noise = 0.3)
    {
        std::normal_distribution<double> nd (0.0, 1.0);
        const int len = (int) (decay * 6.0 * fs);
        for (size_t m = 0; m < ratios.size(); ++m)
        {
            double ph = 0.0;
            const double a = amp / (1.0 + m * 0.8), tau = decay / (1.0 + m * 0.6);
            for (int i = 0; i < len && at + i < (int) x.size(); ++i)
            {
                const double t = i / fs;
                const double f = f0 * ratios[m] * (1.0 + 0.04 * std::exp (-t / 0.05));    // pitch drop
                ph += 2.0 * kPi * f / fs;
                x[(size_t) (at + i)] += (float) (a * std::exp (-t / tau) * std::sin (ph));
            }
        }
        for (int i = 0; i < (int) (0.008 * fs) && at + i < (int) x.size(); ++i)
            x[(size_t) (at + i)] += (float) (noise * amp * nd (rng) * std::exp (-i / (0.002 * fs)));
    }

    std::vector<float> render (Engine& e, const Params& p, std::vector<float> x, int block = 256)
    {
        float* chans[1];
        for (size_t s = 0; s < x.size(); s += (size_t) block)
        {
            const int n = (int) std::min ((size_t) block, x.size() - s);
            chans[0] = x.data() + s;
            e.process (p, chans, 1, n);
        }
        return x;
    }

    double rmsDb (const std::vector<float>& x, size_t a, size_t z)
    {
        double s = 0; for (size_t i = a; i < z; ++i) s += (double) x[i] * x[i];
        return 10.0 * std::log10 (std::max (1e-30, s / (double) (z - a)));
    }

    Params neutral() { return Params {}; }

    //  the worst |digital - analog| in dB over a family of settings
    double worstErr (FType t, double fs, std::initializer_list<double> gains, std::initializer_list<double> qs,
                     double f0lo, double f0hi, double fmax)
    {
        double worst = 0;
        for (double f0 = f0lo; f0 <= f0hi; f0 *= 1.07)
            for (double g : gains) for (double q : qs)
            {
                const Coefs c = design (t, f0, fs, g, q);
                for (double f = 20; f <= std::min (fmax, 0.49 * fs); f *= 1.02)
                {
                    const double a = analogMagDb (t, std::min (f0, 0.48 * fs), g, q, f);
                    if ((t == F_LP || t == F_HP) && a < -12.0) continue;      // stopbands do not matter
                    worst = std::max (worst, std::fabs (magDb (c, f, fs) - a));
                }
            }
        return worst;
    }
}

//==============================================================================
static void testFilters()
{
    std::printf ("\n[1] the bands against their analog prototypes\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const double typ = std::max ({
            worstErr (F_PEAK, fs, { -12, -6, -3, 3, 6, 12 }, { 0.7, 1.0, 1.5, 2.5, 4.0 }, 20, 14000, 20000),
            worstErr (F_HSHELF, fs, { -12, -6, -3, 3, 6, 12 }, { 0.5, 0.7, 1.0 }, 2000, 16000, 20000),
            worstErr (F_LSHELF, fs, { -12, -6, -3, 3, 6, 12 }, { 0.5, 0.7, 1.0 }, 25, 800, 20000) });
        const double ext = std::max ({
            worstErr (F_PEAK, fs, { -18, 18 }, { 0.3, 0.5, 8.0 }, 20, 16000, 20000),
            worstErr (F_HSHELF, fs, { -18, 18 }, { 0.5, 1.5 }, 2000, 16000, 20000),
            worstErr (F_LSHELF, fs, { -18, 18 }, { 0.5, 1.5 }, 25, 900, 20000) });
        const double flt = std::max (worstErr (F_HP, fs, { 0 }, { 0.5412, 0.7071, 1.3066 }, 10, 1000, 20000),
                                     worstErr (F_LP, fs, { 0 }, { 0.5412, 0.7071, 1.3066 }, 1000, 0.4 * fs, 18000));
        std::printf ("  fs %6.0f   bells/shelves (to +-12 dB, Q 0.5-4): worst %.2f dB   extremes: %.2f dB   HPF/LPF passband: %.2f dB\n",
                     fs, typ, ext, flt);
        CHECK (typ < 1.0, "typical bands at %.0f Hz: %.2f dB from the analog curve", fs, typ);
        CHECK (ext < 2.5, "extreme bands at %.0f Hz: %.2f dB", fs, ext);
        CHECK (flt < 1.0, "filters at %.0f Hz: %.2f dB", fs, flt);
    }

    //  what matching buys: a 12 kHz bell and a 10 kHz air shelf at 44.1 kHz
    {
        const double fs = 44100;
        auto err = [&] (const Coefs& c, FType t, double f0, double g, double q)
        {
            double w = 0; for (double f = 2000; f <= 20000; f *= 1.01) w = std::max (w, std::fabs (magDb (c, f, fs) - analogMagDb (t, f0, g, q, f)));
            return w;
        };
        const double m1 = err (design (F_PEAK, 12000, fs, 6, 1.0), F_PEAK, 12000, 6, 1.0);
        const double b1 = err (designBilinear (F_PEAK, 12000, fs, 6, 1.0), F_PEAK, 12000, 6, 1.0);
        const double m2 = err (design (F_HSHELF, 10000, fs, 6, 0.7), F_HSHELF, 10000, 6, 0.7);
        const double b2 = err (designBilinear (F_HSHELF, 10000, fs, 6, 0.7), F_HSHELF, 10000, 6, 0.7);
        std::printf ("  at 44.1 kHz: a +6 dB bell at 12 kHz is %.2f dB off matched, %.2f dB bilinear;"
                     " a +6 dB air shelf at 10 kHz %.2f vs %.2f\n", m1, b1, m2, b2);
        CHECK (m1 < 0.5 * b1 && m2 < 0.5 * b2, "matching should at least halve the cramping");
    }

    //  stability, everywhere the knobs can go
    int unstable = 0;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (double f0 = 10; f0 < 22000; f0 *= 1.1)
            for (double g : { -18.0, -9.0, -0.5, 0.5, 9.0, 18.0 })
                for (double q : { 0.3, 0.7, 2.0, 8.0 })
                    for (int t : { F_PEAK, F_LSHELF, F_HSHELF, F_LP, F_HP })
                    {
                        const Coefs c = design ((FType) t, f0, fs, g, q);
                        if (! stable (c) || ! std::isfinite (c.b0 + c.b1 + c.b2)) ++unstable;
                    }
    CHECK (unstable == 0, "%d unstable sections", unstable);
    std::printf ("  every section stable over 5 rates x the whole knob range\n");
}

//==============================================================================
static void testNeutralAndWiring()
{
    std::printf ("\n[2] neutral is the input, bit for bit; the engine is the curve it draws\n");
    std::mt19937 rng (7);
    std::vector<float> x (48000);
    std::normal_distribution<double> nd (0, 0.3);
    for (auto& v : x) v = (float) nd (rng);

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Engine e; e.prepare (fs, 512);
        const auto y = render (e, neutral(), x, 173);
        CHECK (std::memcmp (x.data(), y.data(), x.size() * sizeof (float)) == 0, "neutral is not bit-exact at %.0f", fs);
    }
    //  stereo too
    {
        Engine e; e.prepare (48000, 512);
        std::vector<float> l = x, r = x; std::reverse (r.begin(), r.end());
        float* ch[2] = { l.data(), r.data() };
        e.process (neutral(), ch, 2, 4096);
        CHECK (std::memcmp (l.data(), x.data(), 4096 * sizeof (float)) == 0, "neutral stereo is not bit-exact");
    }
    std::printf ("  neutral: bit-exact at 44.1/48/96 kHz, mono and stereo\n");

    //  the impulse response through the engine == the product of the designs
    {
        const double fs = 48000;
        Params p = presetParams (S_SNARE_TOP, 2);           // METAL CRACK
        p.comp = 0; p.gate = 0;
        Engine e; e.prepare (fs, 512);
        std::vector<float> imp (16384, 0.0f); imp[0] = 1.0f;
        const auto h = render (e, p, imp);
        std::vector<double> re (h.begin(), h.end()), im (h.size(), 0.0);
        fft (re, im);
        double worst = 0;
        for (double f = 30; f <= 20000; f *= 1.05)
        {
            const int b = (int) std::lround (f * 16384 / fs);
            const double meas = 20.0 * std::log10 (std::hypot (re[(size_t) b], im[(size_t) b]));
            const double ff = b * fs / 16384;
            double want = 0;
            want += magDb (design (F_HP, p.hpf, fs, 0, 0.54119610014619698), ff, fs) + magDb (design (F_HP, p.hpf, fs, 0, 1.30656296487637652), ff, fs);
            want += magDb (p.b1t > 0.5f ? design (F_PEAK, p.b1f, fs, p.b1g, p.b1q) : design (F_LSHELF, p.b1f, fs, p.b1g, p.b1q), ff, fs);
            want += magDb (design (F_PEAK, p.b2f, fs, p.b2g, p.b2q), ff, fs);
            want += magDb (design (F_PEAK, p.b3f, fs, p.b3g, p.b3q), ff, fs);
            want += magDb (p.b4t > 0.5f ? design (F_PEAK, p.b4f, fs, p.b4g, p.b4q) : design (F_HSHELF, p.b4f, fs, p.b4g, p.b4q), ff, fs);
            if (p.hpfs < 1.5f) want = 0;   // (the chosen preset is 24 dB; guard if it ever changes)
            worst = std::max (worst, std::fabs (meas - want));
        }
        std::printf ("  METAL CRACK snare: the engine's impulse response is %.3f dB from the drawn curve\n", worst);
        CHECK (worst < 0.05, "the engine does not do what the curve shows (%.3f dB)", worst);
    }
}

//==============================================================================
static void testGate()
{
    std::printf ("\n[3] the gate\n");
    const double fs = 48000;
    std::mt19937 rng (3);
    std::vector<float> x ((size_t) (3 * fs), 0.0f);
    std::normal_distribution<double> nd (0, 1);
    for (auto& v : x) v = (float) (0.003 * nd (rng));                     // -50 dB of bleed
    addHit (x, (int) (0.5 * fs), fs, 120, { 1.0, 1.59, 2.14 }, 0.5, 0.15, rng);
    addHit (x, (int) (1.8 * fs), fs, 120, { 1.0, 1.59, 2.14 }, 0.5, 0.15, rng);

    Params p = neutral();
    p.gate = 1; p.gthr = -30; p.grange = 30; p.gatk = 0.3f; p.ghold = 80; p.grel = 150; p.gkeylo = 60; p.gkeyhi = 1500;
    Engine e; e.prepare (fs, 512);
    const auto y = render (e, p, x);

    const double bleedIn = rmsDb (x, (size_t) (1.2 * fs), (size_t) (1.7 * fs));
    const double bleedOut = rmsDb (y, (size_t) (1.2 * fs), (size_t) (1.7 * fs));
    const double hitIn = rmsDb (x, (size_t) (1.8 * fs + 0.002 * fs), (size_t) (1.8 * fs + 0.05 * fs));
    const double hitOut = rmsDb (y, (size_t) (1.8 * fs + 0.002 * fs), (size_t) (1.8 * fs + 0.05 * fs));
    std::printf ("  between hits the bleed falls %.1f dB (range 30); the second hit passes %.2f dB\n", bleedIn - bleedOut, hitOut - hitIn);
    CHECK (std::fabs ((bleedIn - bleedOut) - 30.0) < 1.5, "gate range: %.1f dB", bleedIn - bleedOut);
    CHECK (hitOut - hitIn > -0.6, "the hit is cut by %.2f dB", hitOut - hitIn);

    //  key filter: a hi-hat-like bleed (all highs) does not open a tom gate keyed to 60-1500 Hz
    {
        std::vector<float> h ((size_t) fs, 0.0f);
        for (size_t i = 0; i < h.size(); ++i)
        {
            double v = nd (rng) * 0.15;  // loud noise...
            h[i] = (float) v;
        }
        //  ...high-passed to 6 kHz: a cymbal
        Biquad b1, b2; const Coefs c = design (F_HP, 6000, fs, 0, 0.7071);
        for (auto& v : h) v = (float) b2.tick (c, b1.tick (c, v));
        Engine g; g.prepare (fs, 512);
        const auto o = render (g, p, h);
        const double drop = rmsDb (h, (size_t) (0.5 * fs), h.size()) - rmsDb (o, (size_t) (0.5 * fs), h.size());
        std::printf ("  a loud cymbal (%.0f dBFS) does not open a tom gate keyed 60-1500 Hz: it is held %.1f dB down\n",
                     rmsDb (h, 0, h.size()), drop);
        CHECK (drop > 25.0, "the key filter let the cymbal open the gate (%.1f dB)", drop);
    }
}

//==============================================================================
static void testCompressor()
{
    std::printf ("\n[4] the compressor\n");
    const double fs = 48000;
    //  a square wave has a constant |x|: the static curve, exactly
    auto square = [&] (double db, double secs)
    {
        std::vector<float> x ((size_t) (secs * fs));
        const double a = std::pow (10.0, db / 20.0);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (((i / 120) & 1) ? a : -a);
        return x;
    };
    auto grAt = [&] (double inDb, double thr, double rat, double knee)
    {
        Params p = neutral(); p.comp = 1; p.cthr = (float) thr; p.crat = (float) rat; p.cknee = (float) knee;
        p.catk = 1; p.crel = 50; p.cauto = 0; p.csc = 20;
        Engine e; e.prepare (fs, 512);
        const auto x = square (inDb, 1.0);
        const auto y = render (e, p, x);
        return rmsDb (y, (size_t) (0.6 * fs), x.size()) - rmsDb (x, (size_t) (0.6 * fs), x.size());
    };
    const double g1 = grAt (-6, -20, 4, 0);
    const double g2 = grAt (-20, -20, 4, 6);
    const double g3 = grAt (-40, -20, 4, 6);
    std::printf ("  -6 dB in, -20 threshold, 4:1: %.2f dB (should be -10.50); at the threshold with a 6 dB knee %.3f (-0.563); below %.3f (0)\n",
                 g1, g2, g3);
    CHECK (std::fabs (g1 + 10.5) < 0.15, "static curve: %.2f", g1);
    CHECK (std::fabs (g2 + 0.5625) < 0.1, "knee: %.3f", g2);
    CHECK (std::fabs (g3) < 0.01, "below threshold it must not touch: %.3f", g3);

    //  attack: time to 63 % of the final reduction
    {
        Params p = neutral(); p.comp = 1; p.cthr = -30; p.crat = 10; p.cknee = 0; p.catk = 10; p.crel = 200; p.cauto = 0; p.csc = 20;
        Engine e; e.prepare (fs, 64);
        auto x = square (-60, 0.5); const auto loud = square (-6, 1.0);
        x.insert (x.end(), loud.begin(), loud.end());
        float* ch[1];
        std::vector<float> gr;
        for (size_t s = 0; s < x.size(); s += 64) { ch[0] = x.data() + s; e.process (p, ch, 1, 64); gr.push_back (e.meterGrDb()); }
        const double fin = gr.back(); int k = (int) (0.5 * fs / 64);
        while (k < (int) gr.size() && gr[(size_t) k] > 0.632 * fin) ++k;
        const double ms = (k * 64 - 0.5 * fs) / fs * 1000.0;
        std::printf ("  ATTACK 10 ms: 63 %% of the reduction after %.1f ms\n", ms);
        CHECK (ms > 6.0 && ms < 15.0, "attack: %.1f ms", ms);
    }

    //  parallel MIX: half dry and half crushed is between the two
    {
        auto lvl = [&] (float mix)
        {
            Params p = neutral(); p.comp = 1; p.cthr = -30; p.crat = 20; p.cknee = 0; p.catk = 1; p.cmix = mix; p.cauto = 0; p.csc = 20;
            Engine e; e.prepare (fs, 512);
            const auto x = square (-6, 1.0);
            const auto y = render (e, p, x);
            return rmsDb (y, (size_t) (0.6 * fs), x.size()) - rmsDb (x, (size_t) (0.6 * fs), x.size());
        };
        const double l0 = lvl (0), l1 = lvl (1), lh = lvl (0.5f);
        const double want = 20.0 * std::log10 (0.5 + 0.5 * std::pow (10.0, l1 / 20.0));
        std::printf ("  MIX: 0 %% %.2f dB, 100 %% %.2f dB, 50 %% %.2f dB (linear blend says %.2f)\n", l0, l1, lh, want);
        CHECK (std::fabs (l0) < 0.01 && std::fabs (lh - want) < 0.1, "parallel mix");
    }

    //  the key high-pass: a kick's low end does not pump a compressor keyed at 150 Hz
    {
        auto gr = [&] (float sc)
        {
            Params p = neutral(); p.comp = 1; p.cthr = -20; p.crat = 4; p.cknee = 0; p.catk = 5; p.cauto = 0; p.csc = sc;
            std::vector<float> x ((size_t) fs);
            for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.5 * std::sin (2 * kPi * 50 * i / fs));
            Engine e; e.prepare (fs, 512);
            const auto y = render (e, p, x);
            return rmsDb (y, (size_t) (0.5 * fs), x.size()) - rmsDb (x, (size_t) (0.5 * fs), x.size());
        };
        const double lo = gr (20), hi = gr (200);
        std::printf ("  a 50 Hz tone: key HPF 20 Hz takes %.1f dB, 200 Hz takes %.1f dB\n", lo, hi);
        CHECK (hi > lo + 6.0, "the key filter does nothing");
    }
}

//==============================================================================
static void testClicks()
{
    std::printf ("\n[5] every switch crossfades\n");
    const double fs = 48000;
    std::vector<float> x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.5 * std::sin (2 * kPi * 220 * i / fs));

    //  the largest second difference: a click is a kink, a sine has a fixed one
    auto kink = [] (const std::vector<float>& y, size_t a, size_t z)
    {
        double m = 0; for (size_t i = a + 2; i < z; ++i) m = std::max (m, std::fabs ((double) y[i] - 2.0 * y[i - 1] + y[i - 2]));
        return m;
    };
    struct Sw { const char* name; float Params::* m; float a, b; };
    const Sw sws[] = {
        { "POLARITY", &Params::pol, 0, 1 }, { "HPF slope", &Params::hpfs, 0, 2 }, { "LPF slope", &Params::lpfs, 0, 2 },
        { "EQ in", &Params::eq, 1, 0 }, { "BAND 1 shape", &Params::b1t, 0, 1 }, { "BAND 4 shape", &Params::b4t, 0, 1 },
        { "GATE", &Params::gate, 0, 1 }, { "COMP", &Params::comp, 0, 1 },
    };
    double worstRatio = 0;
    for (const auto& s : sws)
    {
        Params p = presetParams (S_SNARE_TOP, 2);
        p.gate = 0; p.comp = 0; p.pol = 0; p.lpf = 3000;
        p.*(s.m) = s.a;
        Engine e; e.prepare (fs, 256);
        std::vector<float> y = x;
        float* ch[1];
        for (size_t o = 0; o < y.size(); o += 256)
        {
            if (o >= (size_t) (0.5 * fs)) p.*(s.m) = s.b;
            ch[0] = y.data() + o; e.process (p, ch, 1, (int) std::min ((size_t) 256, y.size() - o));
        }
        const double before = std::max (kink (y, (size_t) (0.30 * fs), (size_t) (0.49 * fs)), kink (y, (size_t) (0.7 * fs), y.size()));
        const double during = kink (y, (size_t) (0.5 * fs) - 4, (size_t) (0.53 * fs));
        worstRatio = std::max (worstRatio, during / before);
        CHECK (during < 1.6 * before, "%s clicks: kink %.4f against %.4f", s.name, during, before);
    }
    std::printf ("  8 switches thrown on a 220 Hz tone: the worst transition kink is %.2fx the settled one\n", worstRatio);

    //  a knob swept fast does not zipper: the EQ frequency across its range in 50 ms
    {
        Params p = neutral(); p.b3g = 12; p.b3q = 3;
        Engine e; e.prepare (fs, 64);
        std::vector<float> y = x; float* ch[1];
        for (size_t o = 0; o < y.size(); o += 64)
        {
            const double t = o / fs;
            p.b3f = (float) (t < 0.4 ? 300.0 : (t < 0.45 ? 300.0 * std::pow (30.0, (t - 0.4) / 0.05) : 9000.0));
            ch[0] = y.data() + o; e.process (p, ch, 1, 64);
        }
        const double settled = kink (y, (size_t) (0.7 * fs), y.size());
        const double sweep = kink (y, (size_t) (0.4 * fs), (size_t) (0.5 * fs));
        std::printf ("  a +12 dB bell swept 300 Hz -> 9 kHz in 50 ms: kink %.4f against settled %.4f\n", sweep, settled);
        CHECK (sweep < 3.0 * std::max (settled, 0.002), "the sweep zippers");
    }
}

//==============================================================================
static void testStability()
{
    std::printf ("\n[6] random automation on drum hits, every rate\n");
    std::mt19937 rng (11);
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::vector<float> l ((size_t) (8 * fs), 0.0f), r;
        for (int k = 0; k < 40; ++k)
            addHit (l, (int) (k * 0.2 * fs), fs, 60.0 + 30.0 * (k % 7), { 1.0, 1.6, 2.2, 3.1 }, 0.9, 0.2, rng, 0.6);
        r = l; std::reverse (r.begin(), r.end());
        Engine e; e.prepare (fs, 512);
        float peak = 0; bool finite = true;
        std::uniform_real_distribution<float> u (0, 1);
        for (size_t o = 0; o < l.size(); o += 512)
        {
            Params p = presetParams ((int) (u (rng) * 11), (int) (u (rng) * 5));
            for (int i = 1; i < kNumParams; ++i)
                if (u (rng) < 0.3f)
                {
                    const auto& s = specs()[i];
                    p.*(s.member) = s.lo + u (rng) * (s.hi - s.lo);
                }
            p.out = std::min (p.out, 6.0f); p.in = std::min (p.in, 6.0f); p.cmake = std::min (p.cmake, 6.0f);
            const int n = (int) std::min ((size_t) 512, l.size() - o);
            float* ch[2] = { l.data() + o, r.data() + o };
            e.process (p, ch, 2, n);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (ch[0][i]) || ! std::isfinite (ch[1][i])) finite = false;
                peak = std::max ({ peak, std::fabs (ch[0][i]), std::fabs (ch[1][i]) });
            }
        }
        std::printf ("  %6.0f Hz: 8 s of hits, every parameter thrown about every block: peak %.2f, finite %s\n",
                     fs, peak, finite ? "yes" : "NO");
        CHECK (finite && peak < 60.0f, "unstable at %.0f", fs);
    }
}

//==============================================================================
static void testFind()
{
    std::printf ("\n[7] FIND DRUM\n");
    const double fs = 48000;
    struct Drum { const char* name; int src; double f0; std::vector<double> ratios; double decay; };
    const Drum drums[] = {
        { "kick 52 Hz", S_KICK_OUT, 52, { 1.0, 1.59, 2.14 }, 0.25 },
        { "floor tom 82 Hz", S_TOM3, 82, { 1.0, 1.59, 2.14, 2.30 }, 0.35 },
        { "rack tom 147 Hz", S_TOM1, 147, { 1.0, 1.59, 2.14, 2.30 }, 0.25 },
        { "snare 196 Hz", S_SNARE_TOP, 196, { 1.0, 1.59, 2.14, 2.65 }, 0.12 },
    };
    std::mt19937 rng (5);
    for (const auto& d : drums)
    {
        std::vector<float> x ((size_t) (4 * fs), 0.0f);
        std::normal_distribution<double> nd (0, 1);
        for (auto& v : x) v = (float) (0.002 * nd (rng));               // a little bleed
        for (int k = 0; k < 6; ++k)
            addHit (x, (int) ((0.2 + 0.6 * k) * fs), fs, d.f0, d.ratios, 0.4 + 0.1 * (k % 3), d.decay, rng, 0.8);
        const auto& s = source (d.src);
        const auto r = findFundamental (x.data(), (int) x.size(), fs, s.findLo, s.findHi);
        //  the strokes start 4 % sharp and fall back (a real drum does that too)
        const double err = 100.0 * (r.hz / d.f0 - 1.0);
        std::printf ("  %-16s -> %.1f Hz (%+.1f %%) from %d strokes, peak %.0f dB over the range\n", d.name, r.hz, err, r.hits, r.confidence);
        CHECK (r.hits == 6, "%s: %d strokes heard", d.name, r.hits);
        CHECK (std::fabs (err) < 2.5, "%s: %.1f Hz", d.name, r.hz);
    }
    //  silence finds nothing
    std::vector<float> z ((size_t) fs, 0.0f);
    CHECK (findFundamental (z.data(), (int) z.size(), fs, 40, 200).hz == 0.0, "found a drum in silence");
    CHECK (! measureLevels (z.data(), (int) z.size(), fs, 150, 8000).ok(), "heard hits in silence");

    //  ---- the levels: a snare mic with the kick spilling into it, recorded at
    //  two very different levels. The snare's own hits are told from the spill,
    //  and the measured level moves by exactly the recording's difference.
    std::printf ("  levels: a snare mic with kick spill, recorded at -40 and at -6 dB\n");
    const Params sn = presetParams (S_SNARE_TOP, 0);
    LevelResult lv[2];
    const double scale[2] = { 0.02, 1.0 };
    for (int pass = 0; pass < 2; ++pass)
    {
        std::mt19937 r2 (11);
        std::vector<float> x ((size_t) (6 * fs), 0.0f);
        for (int k = 0; k < 6; ++k)        // the snare: 2 and 4, a little uneven
            addHit (x, (int) ((0.5 + 0.9 * k) * fs), fs, 196, { 1.0, 1.59, 2.14, 2.65 }, 0.45 + 0.08 * (k % 3), 0.12, r2, 0.8);
        for (int k = 0; k < 6; ++k)        // the kick, heard 26 dB down through the snare mic
            addHit (x, (int) ((0.05 + 0.9 * k) * fs), fs, 55, { 1.0, 1.59 }, 0.05, 0.2, r2, 0.4);
        for (auto& v : x) v = (float) (v * scale[pass]);
        lv[pass] = measureLevels (x.data(), (int) x.size(), fs, sn.gkeylo, sn.gkeyhi);
        std::printf ("    x%-5.2f: %d own hits, %d spill; median peak %.1f dB, quietest hit at the gate %.1f dB, spill %.1f dB\n",
                     scale[pass], lv[pass].hits, lv[pass].spill, lv[pass].peakDb, lv[pass].keyMinDb, lv[pass].spillDb);
        CHECK (lv[pass].hits == 6, "x%.2f: %d own hits", scale[pass], lv[pass].hits);
        CHECK (lv[pass].spillDb > -199.0 && lv[pass].keyMinDb - lv[pass].spillDb > 10.0,
               "x%.2f: hits %.1f vs spill %.1f at the gate", scale[pass], lv[pass].keyMinDb, lv[pass].spillDb);
    }
    const double want = 20.0 * std::log10 (scale[1] / scale[0]);
    CHECK (std::fabs ((lv[1].peakDb - lv[0].peakDb) - want) < 0.3, "the level moved %.2f dB for %.2f", lv[1].peakDb - lv[0].peakDb, want);
    CHECK (std::fabs ((lv[1].keyMinDb - lv[0].keyMinDb) - want) < 0.3, "the gate's view moved %.2f dB for %.2f", lv[1].keyMinDb - lv[0].keyMinDb, want);
}

//==============================================================================
static void testPresets()
{
    std::printf ("\n[8] presets and kits\n");
    int presets = 0, bad = 0, flat = 0, outOfRange = 0;
    for (int s = 0; s < kNumSources; ++s)
    {
        std::string err;
        Params base; base.source = (float) s;
        if (! applyText (base, source (s).standard, &err)) { ++bad; std::printf ("  %s STANDARD: %s\n", source (s).name, err.c_str()); }
        const Params std0 = presetParams (s, 0);
        for (int i = 0; i < numPresets (s); ++i)
        {
            ++presets;
            Params p = std0;
            if (! applyText (p, preset (s, i).values, &err)) { ++bad; std::printf ("  %s / %s: %s\n", source (s).name, preset (s, i).name, err.c_str()); }
            const Params q = presetParams (s, i);
            if (i > 0 && std::memcmp (&q, &std0, sizeof (Params)) == 0) { ++flat; std::printf ("  %s / %s is STANDARD\n", source (s).name, preset (s, i).name); }
            const float fq[4] = { q.b1f, q.b2f, q.b3f, q.b4f };
            for (int b = 0; b < 4; ++b)
                if (fq[b] < source (s).band[b].lo || fq[b] > source (s).band[b].hi)
                {
                    ++outOfRange;
                    std::printf ("  %s / %s: band %d at %.0f Hz is outside %s (%.0f-%.0f)\n", source (s).name, preset (s, i).name, b + 1,
                                 fq[b], source (s).band[b].label, source (s).band[b].lo, source (s).band[b].hi);
                }
        }
    }
    std::printf ("  %d presets over %d drums: %d unreadable, %d identical to STANDARD, %d band outside its knob\n",
                 presets, (int) kNumSources, bad, flat, outOfRange);
    CHECK (bad == 0 && flat == 0 && outOfRange == 0, "the preset table");

    int kitBad = 0;
    for (int k = 0; k < numKits(); ++k)
        for (int s = 0; s < kNumSources; ++s)
            if (kit (k).pick[s] < 0 || kit (k).pick[s] >= numPresets (s)) ++kitBad;
    std::printf ("  %d factory kits, every drum's pick a real preset: %s\n", numKits(), kitBad == 0 ? "yes" : "NO");
    CHECK (kitBad == 0, "a kit picks a preset that does not exist");

    //  the kit values round-trip
    {
        Params p = presetParams (S_TOM2, 2), q; float v[kKitValues];
        toKitValues (p, v); q.source = p.source; fromKitValues (q, v);
        CHECK (std::memcmp (&p, &q, sizeof (Params)) == 0, "kit values do not round-trip");
    }

    //  every preset on its own drum: bounded, and how loud it leaves it
    const double fs = 48000;
    std::mt19937 rng (9);
    double lo = 1e9, hi = -1e9;
    const double f0s[kNumSources] = { 55, 55, 190, 190, 150, 110, 80, 0, 0, 0, 0 };
    for (int s = 0; s < kNumSources; ++s)
        for (int i = 0; i < numPresets (s); ++i)
        {
            std::vector<float> x ((size_t) (2 * fs), 0.0f);
            for (int k = 0; k < 4; ++k)
            {
                if (f0s[s] > 0) addHit (x, (int) ((0.1 + 0.45 * k) * fs), fs, f0s[s], { 1.0, 1.6, 2.2 }, 0.5, 0.2, rng);
                else            addHit (x, (int) ((0.1 + 0.45 * k) * fs), fs, 3200, { 1.0, 1.31, 1.83, 2.4, 3.3 }, 0.2, 0.3, rng, 1.0);
            }
            Engine e; e.prepare (fs, 512);
            const auto y = render (e, presetParams (s, i), x);
            float pk = 0; for (float v : y) pk = std::max (pk, std::fabs (v));
            const double d = rmsDb (y, 0, y.size()) - rmsDb (x, 0, x.size());
            lo = std::min (lo, d); hi = std::max (hi, d);
            CHECK (pk < 4.0f && std::isfinite (pk), "%s / %s peaks at %.2f", source (s).name, preset (s, i).name, pk);
        }
    std::printf ("  on a synthetic stroke every preset leaves the level between %+.1f and %+.1f dB\n", lo, hi);
}

//==============================================================================
static void testCost()
{
    std::printf ("\n[9] the cost of a whole kit\n");
    const double fs = 48000;
    const int secs = 10, block = 256;
    std::vector<Engine> eng (kNumSources);
    std::vector<std::vector<float>> bufL (kNumSources), bufR (kNumSources);
    std::mt19937 rng (1);
    for (int s = 0; s < kNumSources; ++s)
    {
        eng[(size_t) s].prepare (fs, block);
        bufL[(size_t) s].assign ((size_t) (secs * fs), 0.0f);
        for (int k = 0; k < secs * 4; ++k) addHit (bufL[(size_t) s], (int) (k * 0.25 * fs), fs, 90, { 1.0, 1.6, 2.3 }, 0.5, 0.2, rng);
        bufR[(size_t) s] = bufL[(size_t) s];
    }
    //  everything on, everywhere: the worst case
    std::vector<Params> ps;
    for (int s = 0; s < kNumSources; ++s) { Params p = presetParams (s, 2); p.gate = 1; p.comp = 1; p.hpfs = 2; p.lpfs = 2; ps.push_back (p); }
    const auto t0 = std::chrono::steady_clock::now();
    for (int o = 0; o + block <= secs * (int) fs; o += block)
        for (int s = 0; s < kNumSources; ++s)
        {
            float* ch[2] = { bufL[(size_t) s].data() + o, bufR[(size_t) s].data() + o };
            eng[(size_t) s].process (ps[(size_t) s], ch, s >= S_HAT ? 2 : 1, block);   // cymbals, room and OH in stereo
        }
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * el / secs;
    std::printf ("  eleven instances, everything on (four of them stereo): %.2f %% of one core, %.3f %% each\n", pct, pct / kNumSources);
    CHECK (pct < 15.0, "a kit costs %.2f %% of a core", pct);
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    noDenormals();
    std::printf ("DRUMEQ bench\n");
    testFilters();
    testNeutralAndWiring();
    testGate();
    testCompressor();
    testClicks();
    testStability();
    testFind();
    testPresets();
    testCost();
    std::printf ("\n%d checks - %s\n", checks, failures == 0 ? "ALL CLEAR" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
