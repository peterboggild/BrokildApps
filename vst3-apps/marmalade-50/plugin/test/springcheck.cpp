/*  MARMALADE 50 - the spring model, measured

    Checks that the springs behave like springs and not like a room or a
    slapback: the decay, the echo period, the descending chirp (low
    frequencies arriving after high ones within an echo), the coils' band,
    silence in -> silence out, stability under a loud long drive, the same
    decay at three sample rates, and the cost. Exits 1 on a failed check.

        mm50spring
*/
#include "SpringTank.h"

#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace mm;

namespace
{
    int fails = 0, checks = 0;
    void check (bool ok, const char* what)
    {
        ++checks; if (! ok) ++fails;
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    }

    std::vector<float> impulse (double sr, double seconds)
    {
        SpringTank t; t.prepare (sr, 0);
        std::vector<float> x ((size_t) (seconds * sr), 0.0f);
        x[0] = 1.0f;
        for (size_t i = 0; i < x.size(); i += 32) t.process (x.data() + i, (int) std::min<size_t> (32, x.size() - i));
        return x;
    }

    // a one-octave band (two biquads), zero phase is not needed here
    std::vector<double> band (const std::vector<float>& x, double sr, double fc)
    {
        std::vector<double> y (x.begin(), x.end());
        for (int pass = 0; pass < 2; ++pass)
        {
            const double w = 2 * M_PI * fc / sr, q = 1.41, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
            const double b0 = al / a0, b2 = -al / a0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
            double z1 = 0, z2 = 0;
            for (auto& v : y) { const double o = b0 * v + z1; z1 = -a1 * o + z2; z2 = b2 * v - a2 * o; v = o; }
        }
        return y;
    }

    // RT60 from Schroeder backward integration, -5 to -25 dB (T20)
    double rt60 (const std::vector<double>& y, double sr)
    {
        std::vector<double> e (y.size());
        double acc = 0;
        for (size_t i = y.size(); i-- > 0;) { acc += y[i] * y[i]; e[i] = acc; }
        const double e0 = e[0];
        double t5 = -1, t25 = -1;
        for (size_t i = 0; i < e.size(); ++i)
        {
            const double db = 10 * std::log10 (e[i] / e0 + 1e-30);
            if (t5 < 0 && db <= -5) t5 = i / sr;
            if (t25 < 0 && db <= -25) { t25 = i / sr; break; }
        }
        return (t5 < 0 || t25 < 0) ? -1 : 3.0 * (t25 - t5);
    }

    // energy-weighted arrival time of a band within a window
    double centroid (const std::vector<double>& y, double sr, double t0, double t1)
    {
        double num = 0, den = 0;
        for (size_t i = (size_t) (t0 * sr); i < (size_t) (t1 * sr) && i < y.size(); ++i) { num += y[i] * y[i] * i; den += y[i] * y[i]; }
        return den > 0 ? num / den / sr : -1;
    }
}

int main()
{
    std::printf ("MARMALADE 50 spring model\n\n");
    const double sr = 48000.0;
    const auto h = impulse (sr, 8.0);

    const auto b1k = band (h, sr, 1000.0), b300 = band (h, sr, 300.0), b3k = band (h, sr, 3000.0);
    const double r1k = rt60 (b1k, sr), r300 = rt60 (b300, sr), r3k = rt60 (b3k, sr);
    std::printf ("  [KNOWN] RT60 (T20): 300 Hz %.2f s, 1 kHz %.2f s, 3 kHz %.2f s\n", r300, r1k, r3k);
    check (r1k > 2.4 && r1k < 3.6, "mid-band decay within 20 % of the 3.0 s it was set to");
    check (r3k < r1k, "the top dies first (the loop's damping)");

    // the first echo: where it lands, and that it is a DESCENDING chirp
    double pk = 0; size_t at = 0;
    for (size_t i = 0; i < (size_t) (0.2 * sr); ++i) if (std::abs (b1k[i]) > pk) { pk = std::abs (b1k[i]); at = i; }
    std::printf ("  [KNOWN] first 1 kHz arrival %.1f ms\n", 1000.0 * at / sr);
    check (at / sr > 0.030 && at / sr < 0.070, "the first echo arrives in the spring's 30-70 ms, not at once");
    const double cHi = centroid (b3k, sr, 0.0, 0.090), cLo = centroid (b300, sr, 0.0, 0.090);
    std::printf ("  [KNOWN] first-echo centroid: 3 kHz %.1f ms, 300 Hz %.1f ms\n", 1000 * cHi, 1000 * cLo);
    check (cLo - cHi > 0.008, "low frequencies arrive more than 8 ms after high ones: a descending chirp (the drip)");

    // the coils' band
    auto lvl = [&] (double f) { const auto b = band (h, sr, f); double s = 0; for (double v : b) s += v * v; return 10 * std::log10 (s + 1e-30); };
    const double l40 = lvl (40), l1k = lvl (1000), l10k = lvl (10000);
    std::printf ("  [KNOWN] band energy re 1 kHz: 40 Hz %.1f dB, 10 kHz %.1f dB\n", l40 - l1k, l10k - l1k);
    check (l40 - l1k < -12 && l10k - l1k < -20, "a tank passes neither deep bass nor the top octave");

    // silence, stability
    {
        SpringTank t; t.prepare (sr, 32);
        std::vector<float> z (48000, 0.0f);
        t.process (z.data(), (int) z.size());
        double m = 0; for (float v : z) m = std::max (m, (double) std::abs (v));
        check (m == 0.0, "silence in, exact silence out");
    }
    {
        SpringTank t; t.prepare (sr, 32);
        uint32_t r = 1; double m = 0, late = 0;
        std::vector<float> x (32);
        for (int b = 0; b < (int) (60 * sr / 32); ++b)
        {
            const bool on = b < (int) (40 * sr / 32);
            for (auto& v : x) { r = r * 1664525u + 1013904223u; v = on ? (float) (((r >> 8) / 16777216.0 - 0.5) * 4.0) : 0.0f; }
            t.process (x.data(), 32);
            for (float v : x) { m = std::max (m, (double) std::abs (v)); if (! on && b > (int) (59 * sr / 32)) late = std::max (late, (double) std::abs (v)); }
        }
        std::printf ("  [KNOWN] 40 s of loud noise: peak %.3f, 19 s after it stops %.2e\n", m, late);
        check (std::isfinite (m) && m < 1.0 && late < m * 1e-3, "40 s of loud drive stays bounded and dies away afterwards");
    }

    // rate independence
    for (double r : { 44100.0, 96000.0 })
    {
        const auto hr = impulse (r, 8.0);
        const double t = rt60 (band (hr, r, 1000.0), r);
        std::printf ("  [KNOWN] at %.1f kHz: RT60 1 kHz %.2f s\n", r / 1000, t);
        check (std::abs (t - r1k) / r1k < 0.15, "the decay does not move with the sample rate");
    }

    // the engine's advance: the return a chunk early must land on the same echo
    {
        SpringTank a; a.prepare (sr, 0);
        SpringTank b; b.prepare (sr, 32);
        std::vector<float> xa (24000, 0.0f), xb (24000, 0.0f); xa[0] = xb[0] = 1.0f;
        a.process (xa.data(), (int) xa.size()); b.process (xb.data(), (int) xb.size());
        // past the sizzle (which is not delayed), the advanced response must be the
        // same response 32 samples earlier, LATER ECHOES INCLUDED - so the echo
        // period is the spring's and not shortened by the advance
        double num = 0, ea = 0, eb = 0;
        for (size_t i = 1500; i + 32 < xa.size(); ++i) { num += (double) xa[i + 32] * xb[i]; ea += (double) xa[i + 32] * xa[i + 32]; eb += (double) xb[i] * xb[i]; }
        const double rho = num / std::sqrt (ea * eb + 1e-300);
        std::printf ("  [KNOWN] advanced vs plain, shifted 32: correlation %.6f over 0.5 s\n", rho);
        check (rho > 0.999, "a 32-sample advance moves the whole response 32 samples earlier, echo period unchanged");
    }

    // cost
    {
        SpringTank t; t.prepare (sr, 32);
        std::vector<float> x (32, 0.1f);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (10 * sr / 32); ++b) t.process (x.data(), 32);
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  [KNOWN] cost %.2f %% of one core at 48 kHz\n", 100.0 * s / 10.0);
        check (s / 10.0 < 0.05, "under 5 % of one core");
    }

    std::printf ("\n%d of %d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
