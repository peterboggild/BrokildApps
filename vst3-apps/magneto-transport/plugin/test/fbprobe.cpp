// Magneto Transport - where does the feedback loop run away? (prints)
// Part 1: the one-pass response of a single head (feedback 0, wet only), small
// signal, in 1/3 octaves: the loop gain at FEEDBACK f is f * FEEDBACK_MAX * |H|,
// so the head's own onset is 1 / (FEEDBACK_MAX * max|H|).
// Part 2: the onset measured in time: a burst, then silence; is the tail growing?
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
using namespace mt;

static const double FS = 48000.0;

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        const std::complex<double> w (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> u (1, 0);
            for (size_t k = 0; k < len / 2; ++k) { auto x = a[i + k], y = a[i + k + len / 2] * u; a[i + k] = x + y; a[i + k + len / 2] = x - y; u *= w; }
        }
    }
}

static Params base (double speed)
{
    Params p; p.mix = 1.0f; p.killDry = 1.0f; p.feedback = 0.0f; p.condition = 0.0f; p.mode = 0; p.speed = (float) speed;
    p.h1On = 1; p.h2On = p.h3On = 0; p.h1Pos = 4.0f; p.h2Pos = 8.0f; p.h3Pos = 12.0f;
    return p;
}

static void render (Engine& e, const Params& p, const std::vector<float>& x, std::vector<float>& y)
{
    e.setParams (p); e.prepare (FS, 256);
    y.assign (x.size(), 0.0f);
    std::vector<float> yr (x.size());
    for (size_t i = 0; i < x.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, x.size() - i);
        e.process (x.data() + i, x.data() + i, y.data() + i, yr.data() + i, n);
        e.service();
    }
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double amp = 0.0316;   // -30 dBFS: small signal, 12 dB under 0 VU
    if (argc <= 2) {
    std::printf ("Part 1: one head's pass response (feedback 0), 1/3 octaves, dB re input; onset = 1/(%.2f*max|H|)\n", FEEDBACK_MAX);
    for (double v : { 4.75, 9.525, 19.05, 38.1, 76.2 })
    {
        Params p = base (v);
        const double dl = 4.0 / v;
        const size_t N = 1 << 17;                                   // 2.7 s analysed
        const size_t skip = (size_t) ((dl + 0.5) * FS);
        std::vector<float> x (skip + N), y;
        uint64_t s = 12345;
        for (auto& q : x) { s = s * 6364136223846793005ull + 1442695040888963407ull; q = (float) (amp * 1.7320508 * (((s >> 11) * (1.0 / 9007199254740992.0)) * 2.0 - 1.0)); }
        Engine e; e.setIdealMedium (true);
        render (e, p, x, y);
        // power spectra of the input segment and the output segment one head delay later
        const size_t lag = (size_t) std::lround (dl * FS);
        std::vector<double> Px (N / 2), Py (N / 2);
        const size_t M = 8192;
        for (size_t seg = 0; seg + M <= N - lag; seg += M / 2)
        {
            std::vector<std::complex<double>> a (M), b (M);
            for (size_t i = 0; i < M; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (M - 1));
                a[i] = w * x[skip - lag + seg + i];
                b[i] = w * y[skip + seg + i];
            }
            fft (a); fft (b);
            for (size_t k = 0; k < M / 2; ++k) { Px[k] += std::norm (a[k]); Py[k] += std::norm (b[k]); }
        }
        double worst = -1e9, fw = 0;
        std::printf ("  %5.2f cm/s:", v);
        for (double fc = 25.0; fc < 20000.0; fc *= std::pow (2.0, 1.0 / 3.0))
        {
            const size_t k0 = (size_t) (fc * std::pow (2.0, -1.0 / 6.0) * M / FS), k1 = (size_t) (fc * std::pow (2.0, 1.0 / 6.0) * M / FS);
            double sx = 0, sy = 0;
            for (size_t k = std::max<size_t> (k0, 1); k <= k1; ++k) { sx += Px[k]; sy += Py[k]; }
            const double db = 10.0 * std::log10 (sy / sx);
            if (db > worst) { worst = db; fw = fc; }
            if (argc > 1) std::printf (" %.0f:%+.1f", fc, db);
        }
        std::printf ("   max %+.2f dB at %.0f Hz -> one-head onset at FEEDBACK %.0f %%\n", worst, fw, 100.0 / (FEEDBACK_MAX * std::pow (10.0, worst / 20.0)));
    }

    }
    std::printf ("\nPart 2: onset in time (burst 0.3 s at -30 dBFS, then silence; tail rms in two late windows)\n");
    struct Case { const char* name; int heads; double speed; };
    const Case cases[] = { { "1 head, 19 cm/s", 1, 19.05 }, { "2 heads, 19 cm/s", 2, 19.05 }, { "3 heads, 19 cm/s", 3, 19.05 },
                           { "1 head, 4.75 cm/s", 1, 4.75 }, { "1 head, 76 cm/s", 1, 76.2 } };
    for (const Case& c : cases)
    {
        double lo = 0.0, hi = 1.0;
        for (int it = 0; it < 9; ++it)
        {
            const double f = 0.5 * (lo + hi);
            Params p = base (c.speed); p.feedback = (float) f;
            p.h2On = c.heads >= 2; p.h3On = c.heads >= 3;
            const double T = std::max (12.0, 40.0 * 12.0 / c.speed);
            std::vector<float> x ((size_t) (T * FS), 0.0f), y;
            uint64_t s = 99;
            for (size_t i = 0; i < (size_t) (0.3 * FS); ++i) { s = s * 6364136223846793005ull + 1442695040888963407ull; x[i] = (float) (amp * 1.7320508 * (((s >> 11) * (1.0 / 9007199254740992.0)) * 2.0 - 1.0)); }
            Engine e; e.setIdealMedium (true);
            render (e, p, x, y);
            auto rms = [&] (double a, double b) { double q = 0; size_t i0 = (size_t) (a * FS), i1 = (size_t) (b * FS); for (size_t i = i0; i < i1; ++i) q += (double) y[i] * y[i]; return std::sqrt (q / (i1 - i0)); };
            const double r1 = rms (T * 0.5, T * 0.65), r2 = rms (T * 0.85, T);
            // a decaying tail falls by g^passes; a sustained or saturated loop does not
            (r2 > 0.7 * r1 && r2 > 1.0e-5 ? hi : lo) = f;
        }
        std::printf ("  %-18s onset at FEEDBACK %.1f %%\n", c.name, 50.0 * (lo + hi));
    }
}
