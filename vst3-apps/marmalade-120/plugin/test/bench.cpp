/*  MARMALADE 120 - bench

    Plain C++17, no JUCE, no audio device: the engine is driven exactly as the
    plugin drives it and every check reads a number off real rendered audio.

        cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Release
        cmake --build test/build && ./test/build/mmtest

    spicecheck (same folder) is the other half: it checks the circuit solver
    against ngspice. This checks that the amplifier built on it behaves like
    the amplifier it is meant to be.
*/
#include "Engine.h"
#include "Or120Netlist.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace mm;

namespace
{
    int passed = 0, failed = 0;

    void check (bool ok, const char* what, const std::string& detail)
    {
        (ok ? passed : failed)++;
        std::printf ("  [%s] %-58s %s\n", ok ? " ok " : "FAIL", what, detail.c_str());
    }

    std::string fmt (const char* f, double a, double b = 0, double c = 0)
    {
        char buf[160];
        std::snprintf (buf, sizeof buf, f, a, b, c);
        return buf;
    }

    constexpr double SR = 48000.0;
    constexpr int BLOCK = 256;

    /*  Renders a mono signal through a freshly prepared engine and returns
        the left output. Settling time is rendered and thrown away first. */
    std::vector<float> render (Engine& e, const Params& p, const std::vector<float>& in,
                               double sr = SR, double settle = 0.25)
    {
        e.setParams (p);
        e.prepare (sr, BLOCK);
        std::vector<float> L (BLOCK), R (BLOCK);

        for (int done = 0; done < (int) (settle * sr); done += BLOCK)
        {
            std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
            e.process (L.data(), R.data(), BLOCK);
        }

        std::vector<float> out (in.size());
        for (size_t s = 0; s < in.size(); s += BLOCK)
        {
            const int n = (int) std::min ((size_t) BLOCK, in.size() - s);
            for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = in[s + (size_t) i];
            e.process (L.data(), R.data(), n);
            for (int i = 0; i < n; ++i) out[s + (size_t) i] = L[(size_t) i];
        }
        return out;
    }

    std::vector<float> sine (double f, double amp, double sec, double sr = SR)
    {
        std::vector<float> v ((size_t) (sec * sr));
        for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin (2.0 * M_PI * f * (double) i / sr));
        return v;
    }

    // amplitude of one frequency over the second half of a buffer, Hann-windowed
    double toneAmp (const std::vector<float>& v, double f, double sr = SR)
    {
        const size_t from = v.size() / 2, n = v.size() - from;
        double re = 0, im = 0, wsum = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) (n - 1));
            const double ph = 2.0 * M_PI * f * (double) (from + i) / sr;
            re += w * v[from + i] * std::cos (ph);
            im += w * v[from + i] * std::sin (ph);
            wsum += w;
        }
        return 2.0 * std::sqrt (re * re + im * im) / wsum;
    }

    double rms (const std::vector<float>& v)
    {
        const size_t from = v.size() / 2;
        double s = 0;
        for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
        return std::sqrt (s / (double) (v.size() - from));
    }

    double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

    // spectral centroid of the second half, in Hz, by a plain DFT on a decimated grid
    double centroid (const std::vector<float>& v, double sr = SR)
    {
        double num = 0, den = 0;
        for (double f = 50.0; f < 12000.0; f *= 1.06)
        {
            const double a = toneAmp (v, f, sr);
            num += f * a * a; den += a * a;
        }
        return num / std::max (den, 1e-30);
    }

    /*  Everything in a hard-clipped tone that is not a harmonic of it: the
        aliasing that folded back down, as dB under the fundamental. */
    double aliasDb (const std::vector<float>& v, double f0, double sr = SR)
    {
        const double fund = toneAmp (v, f0, sr);
        double worst = 0.0;
        for (double f = 100.0; f < 18000.0; f += 37.0)
        {
            const double h = f / f0;
            if (std::abs (h - std::round (h)) * f0 < 120.0) continue;   // near a harmonic
            worst = std::max (worst, toneAmp (v, f, sr));
        }
        return db (worst / fund);
    }
}

//==============================================================================
int main()
{
    std::printf ("MARMALADE 120 bench\n\n");

    Engine e;
    check (e.ok(), "both netlists parse and drive every part", e.ok() ? "" : e.error());
    if (! e.ok()) return 1;

    //--------------------------------------------------------------------------
    std::printf ("\noperating point (idle, after warm-up)\n");
    {
        Params p;
        render (e, p, std::vector<float> (4800, 0.0f));
        Circuit pre, pw;
        pre.parse (OR120_PREAMP); pw.parse (OR120_POWER);
        pre.prepare (96000.0); pw.prepare (96000.0);
        const double z[1] = { 0.0 };
        pw.solveDC (z);
        const double b4 = pw.nodeVoltage (pw.nodeIndex ("b4"));
        const double pin[2] = { 0.0, b4 };
        pre.solveDC (pin);
        auto V  = [] (Circuit& c, const char* n) { return c.nodeVoltage (c.nodeIndex (n)); };

        const double ia = (V (pw, "ota") - V (pw, "pa")) / 45.0 / 2.0;   // per valve
        check (V (pw, "b1") > 480 && V (pw, "b1") < 530, "B+ at the output transformer", fmt ("%.1f V", V (pw, "b1")));
        check (b4 > 310 && b4 < 350,                    "preamp rail (documented about 330 V)", fmt ("%.1f V", b4));
        check (V (pw, "b3") > 350 && V (pw, "b3") < 390, "inverter rail (documented about 370 V)", fmt ("%.1f V", V (pw, "b3")));
        check (V (pre, "p1") > 180 && V (pre, "p1") < 250, "V1a plate", fmt ("%.1f V", V (pre, "p1")));
        check (V (pw, "p3") > 120 && V (pw, "p3") < 180, "driver plate = cathodyne grid (DC coupled)", fmt ("%.1f V", V (pw, "p3")));
        const double vpk = V (pw, "pc") - V (pw, "kc");
        check (vpk > 40 && vpk < 110, "cathodyne plate-cathode, room to swing", fmt ("%.1f V", vpk));
        check (ia > 0.025 && ia < 0.050, "EL34 idle current, per valve", fmt ("%.1f mA", ia * 1e3));
        const double diss = ia * V (pw, "pa");
        check (diss < 25.0, "EL34 idle dissipation under its 25 W", fmt ("%.1f W", diss));
    }

    //--------------------------------------------------------------------------
    std::printf ("\npower and sag\n");
    {
        /*  Rated power: the most the amp gives before 10% THD, the figure the
            vintage heads of the time were rated at - with a high-gain driver,
            a cathodyne and little global feedback, this amp is not a hi-fi and
            is a few percent into distortion well before that. GAIN low so the
            preamp is clean, MASTER full, the drive raised to 10%. */
        Params p; p.gain = 0.12f; p.master = 1.0f; p.cab = false; p.output = 2.0f / 3.0f;
        double watts5 = 0.0, at = 0.0;
        for (double a = 0.02; a < 2.0; a *= 1.15)
        {
            const auto out = render (e, p, sine (1000.0, a, 0.3), SR, 0.1);
            const double f = toneAmp (out, 1000.0);
            double h = 0.0;
            for (int k = 2; k <= 9; ++k) { const double hk = toneAmp (out, 1000.0 * k); h += hk * hk; }
            const double thd = std::sqrt (h) / f;
            const double vpk = f * 80.0;                              // back to volts at the tap
            if (thd > 0.10) break;
            watts5 = vpk * vpk / 2.0 / 8.0; at = a;
        }
        check (watts5 > 90.0 && watts5 < 170.0, "rated power, 10% THD, into the 8 ohm tap",
               fmt ("%.0f W (at %.2f V in)", watts5, at));

        Params f = p; f.gain = 0.9f;
        render (e, f, sine (1000.0, 0.5, 0.6));
        check (e.sag() > 10.0, "flat out, B+ sags", fmt ("%.1f V below idle (%.0f W into the load)", e.sag(), e.watts()));

        Params q = p; q.master = 0.15f; q.gain = 0.1f;
        render (e, q, sine (1000.0, 0.1, 0.4));
        check (e.sag() < 2.0, "and not at bedroom level", fmt ("%.2f V", e.sag()));
    }

    //--------------------------------------------------------------------------
    std::printf ("\nthe controls\n");
    {
        const auto in = sine (220.0, 0.3, 0.5);
        Params p; p.cab = false;
        double last = -1e9; bool mono = true; std::string seq;
        for (float g : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            p.gain = g;
            const double l = db (rms (render (e, p, in)));
            mono = mono && l > last - 1.0;          // a fully clipped stage may give back a little
            last = l;
            seq += fmt ("%.1f ", l);
        }
        check (mono, "GAIN only ever adds level (until it compresses)", seq + "dBFS");

        // distortion rises with GAIN
        auto thdDb = [] (const std::vector<float>& v, double f0)
        {
            double h = 0.0;
            for (int k = 2; k <= 9; ++k) { const double a = toneAmp (v, f0 * k); h += a * a; }
            return db (std::sqrt (h) / toneAmp (v, f0));
        };
        p.gain = 0.15f; const auto clean = render (e, p, sine (220.0, 0.05, 0.5));
        p.gain = 1.0f;  const auto dirty = render (e, p, in);
        const double thdC = thdDb (clean, 220.0), thdD = thdDb (dirty, 220.0);
        check (thdD > thdC + 20.0, "GAIN up: clean to driven", fmt ("THD %.1f -> %.1f dB", thdC, thdD));

        // the cathodyne and the lopsided preamp clip unevenly: even harmonics
        const double h2 = db (toneAmp (dirty, 440.0) / toneAmp (dirty, 220.0));
        check (h2 > -40.0, "asymmetric clipping leaves a real second harmonic", fmt ("%.1f dBc", h2));

        const auto noise = [] ()
        {
            std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.05f);
            std::vector<float> v ((size_t) (0.6 * SR));
            for (auto& s : v) s = nd (rng);
            return v;
        } ();
        Params t; t.cab = false; t.gain = 0.2f;
        auto c = [&] (Params q) { return centroid (render (e, q, noise)); };
        t.treble = 0.0f; const double tr0 = c (t); t.treble = 1.0f; const double tr1 = c (t); t.treble = 0.55f;
        check (tr1 > tr0 * 1.3, "TREBLE brightens", fmt ("centroid %.0f -> %.0f Hz", tr0, tr1));
        t.bass = 0.0f; const double b0 = c (t); t.bass = 1.0f; const double b1 = c (t); t.bass = 0.5f;
        check (b1 < b0 * 0.9, "BASS darkens", fmt ("centroid %.0f -> %.0f Hz", b0, b1));
        t.fac = 0; const double f0 = c (t); t.fac = 5; const double f5 = c (t); t.fac = 2;
        check (f5 > f0 * 1.15, "VOICE 1 -> 6 thins the low end", fmt ("centroid %.0f -> %.0f Hz", f0, f5));
        t.hfDrive = 0.0f; const double h0 = c (t); t.hfDrive = 1.0f; const double h1 = c (t); t.hfDrive = 0.25f;
        check (h1 > h0 * 1.1, "BITE pushes the top into the third stage", fmt ("centroid %.0f -> %.0f Hz", h0, h1));

        Params m; m.cab = false; m.gain = 0.3f;
        m.master = 0.2f; const double m0 = db (rms (render (e, m, in)));
        m.master = 0.8f; const double m1 = db (rms (render (e, m, in)));
        check (m1 > m0 + 10.0, "MASTER sets the level", fmt ("%.1f -> %.1f dBFS", m0, m1));
    }

    //--------------------------------------------------------------------------
    std::printf ("\nCAB\n");
    {
        Params p; p.gain = 0.1f; p.master = 0.3f;
        const auto in = sine (1000.0, 0.05, 0.5);
        p.cab = false; const double off = toneAmp (render (e, p, in), 1000.0);
        p.cab = true;  const double on  = toneAmp (render (e, p, in), 1000.0);
        check (std::abs (db (on / off)) < 1.5, "CAB on and off match at 1 kHz", fmt ("%+.2f dB", db (on / off)));

        const auto in8 = sine (8000.0, 0.05, 0.5);
        const double hi = toneAmp (render (e, p, in8), 8000.0);
        check (db (hi / on) < -15.0, "a 12\" cone has nothing much past 5 kHz", fmt ("8 kHz at %.1f dB re 1 kHz", db (hi / on)));
    }

    //--------------------------------------------------------------------------
    std::printf ("\nquiet and stable\n");
    {
        Params p; p.gain = 1.0f; p.master = 1.0f; p.hfDrive = 1.0f; p.treble = 1.0f;
        const auto out = render (e, p, std::vector<float> ((size_t) SR, 0.0f), SR, 0.5);
        check (db (rms (out)) < -110.0, "silence in, silence out (no hum is modelled)", fmt ("%.1f dBFS", db (rms (out))));

        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        int bad = 0, worst = 0, fails = 0;
        const auto burst = [] ()
        {
            std::vector<float> v ((size_t) (0.25 * SR));
            for (size_t i = 0; i < v.size(); ++i)
                v[i] = (float) (0.9 * std::sin (2.0 * M_PI * 82.4 * i / SR) * std::exp (-(double) i / (0.08 * SR))
                              + 0.6 * std::sin (2.0 * M_PI * 1318.5 * i / SR) * std::exp (-(double) i / (0.03 * SR)));
            return v;
        } ();
        for (int k = 0; k < 40; ++k)
        {
            Params q;
            q.gain = u (rng); q.bass = u (rng); q.treble = u (rng); q.hfDrive = u (rng);
            q.master = u (rng); q.fac = (int) (u (rng) * 5.99f); q.cab = u (rng) > 0.5f;
            const auto out = render (e, q, burst, SR, 0.05);
            for (float s : out) if (! std::isfinite (s) || std::abs (s) > 4.0f) { ++bad; break; }
            bad += e.resets();
            worst = std::max ({ worst, e.preamp().maxIterations(), e.power().maxIterations() });
            fails += e.preamp().failures() + e.power().failures();
        }
        check (bad == 0, "40 random settings, hard-picked low E and high E: all finite", fmt ("%.0f bad", bad));
        check (fails == 0, "Newton converged on every sample of them", fmt ("%.0f failures, worst %.0f iterations", fails, worst));
    }

    //--------------------------------------------------------------------------
    std::printf ("\naliasing and cost\n");
    {
        /*  The worst a guitar asks: A6, high E at the 17th fret, GAIN full,
            through the cabinet. Aliases fold back on to a 160 Hz grid for
            this note at 48 kHz, so they are easy to tell from harmonics. */
        Params p; p.gain = 1.0f; p.master = 0.3f; p.cab = true;
        const double f0 = 1760.0;
        p.quality = 0; const double a2 = aliasDb (render (e, p, sine (f0, 0.5, 0.5)), f0);
        p.quality = 1; const double a4 = aliasDb (render (e, p, sine (f0, 0.5, 0.5)), f0);
        check (a2 < -40.0, "NORMAL (2x at 48 kHz): aliasing of a clipped A6", fmt ("%.1f dB re fundamental", a2));
        check (a4 < a2 - 6.0, "HIGH (4x) is cleaner by more than 6 dB", fmt ("%.1f dB", a4));

        for (int q = 0; q < 2; ++q)
        {
            Params c; c.gain = 0.7f; c.master = 0.6f; c.quality = q;
            e.setParams (c); e.prepare (SR, BLOCK);
            std::vector<float> L (BLOCK), R (BLOCK);
            const int blocks = (int) (4.0 * SR / BLOCK);
            const auto t0 = std::chrono::steady_clock::now();
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < BLOCK; ++i)
                    L[(size_t) i] = R[(size_t) i] = (float) (0.4 * std::sin (2.0 * M_PI * 110.0 * (b * BLOCK + i) / SR));
                e.process (L.data(), R.data(), BLOCK);
            }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-64s %.1f%% of one core\n", q == 0 ? "cost, NORMAL, 48 kHz:" : "cost, HIGH, 48 kHz:", 100.0 * sec / 4.0);
        }
        check (e.latencySamples() > 0, "latency reported to the host", fmt ("%.0f samples", e.latencySamples()));
    }

    std::printf ("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
