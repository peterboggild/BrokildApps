// KICKSTART offline bench. Plain C++17, no JUCE. Every claim the design makes
// is measured here against the real engine, and a claim that only BOUNDS the
// output proves nothing about whether a control works (Martian Gain's lesson),
// so each control is checked for the thing it is FOR.
//
//   kstest            run everything, print ALL CLEAR or the failures
//   kstest --levels   print every preset's peak and loudness

#include "../engine/ks_engine.h"
#include "../engine/ks_presets.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64)
 #include <xmmintrin.h>
#endif

using namespace ks;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
constexpr double PI = 3.14159265358979323846;

struct Hit { double t; int note; float vel; };

std::vector<float> render (const Params& p, double fs, double secs, const std::vector<Hit>& hits, int block = 256)
{
    Engine e; e.prepare (fs, block);
    const int n = (int) (secs * fs);
    std::vector<float> out ((size_t) n, 0.0f);
    size_t h = 0;
    for (int i = 0; i < n; )
    {
        int m = std::min (block, n - i);
        if (h < hits.size())
        {
            const int at = (int) std::lround (hits[h].t * fs);
            if (at <= i) { e.noteOn (hits[h].note, hits[h].vel); ++h; continue; }
            m = std::min (m, at - i);
        }
        e.process (p, out.data() + i, m);
        i += m;
    }
    return out;
}

std::vector<float> one (const Params& p, double fs = 48000.0, double secs = 1.0, int note = 36, float vel = 1.0f)
{
    return render (p, fs, secs, { { 0.0, note, vel } });
}

Params base()
{
    Params p = presetParams (0);
    return p;
}

//  a clean tone for measuring: no click, no skin, pure sine
Params pure (float hz, float sweep, float decay)
{
    Params p = base();
    p.pitch = hz; p.sweep = sweep; p.decay = decay; p.curve = 0.0f;
    p.click = 0.0f; p.skin = 0.0f; p.wave = 0.0f; p.velo = 0.0f;
    return p;
}

double rmsDb (const std::vector<float>& x, int a, int b)
{
    double e = 0; a = std::max (0, a); b = std::min ((int) x.size(), b);
    for (int i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
    return 10.0 * std::log10 (e / std::max (1, b - a) + 1e-30);
}
float peakOf (const std::vector<float>& x, int a = 0, int b = -1)
{
    if (b < 0) b = (int) x.size();
    float m = 0; for (int i = a; i < b; ++i) m = std::max (m, std::abs (x[(size_t) i])); return m;
}
bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }

//  frequency by interpolated positive-going zero crossings over [a,b)
double zcFreq (const std::vector<float>& x, int a, int b, double fs)
{
    double first = -1, last = -1; int cnt = 0;
    for (int i = std::max (1, a); i < b; ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
        {
            const double fr = (double) x[(size_t) i - 1] / ((double) x[(size_t) i - 1] - x[(size_t) i]);
            const double t = (i - 1) + fr;
            if (first < 0) first = t; else ++cnt;
            last = t;
        }
    if (cnt < 1) return 0.0;
    return fs * cnt / (last - first);
}

//  complex amplitude of f over [a,b) (Hann window)
std::complex<double> goertzel (const std::vector<float>& x, int a, int b, double f, double fs)
{
    std::complex<double> s (0, 0); double wsum = 0;
    for (int i = a; i < b; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * PI * (i - a) / (b - a));
        s += w * (double) x[(size_t) i] * std::exp (std::complex<double> (0, -2 * PI * f * i / fs));
        wsum += w;
    }
    return s * (2.0 / wsum);
}
double ampDb (const std::vector<float>& x, int a, int b, double f, double fs)
{ return 20.0 * std::log10 (std::abs (goertzel (x, a, b, f, fs)) + 1e-12); }

void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1, 0);
            for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
}

//  energy NOT on harmonics of f0 (below 20 kHz) over energy on them, dB
double inharmonicDb (const std::vector<float>& x, int a, double f0, double fs)
{
    const int N = 16384;
    std::vector<std::complex<double>> b ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double t = (double) i / (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * PI * t) + 0.14128 * std::cos (4 * PI * t) - 0.01168 * std::cos (6 * PI * t);
        b[(size_t) i] = w * (double) x[(size_t) (a + i)];
    }
    fft (b);
    const double binHz = fs / N;
    double harm = 0, other = 0;
    for (int k = 2; k < N / 2; ++k)
    {
        const double f = k * binHz;
        if (f > 20000.0) break;
        const double e = std::norm (b[(size_t) k]);
        const double h = f / f0, d = std::abs (h - std::round (h)) * f0;
        if (std::round (h) >= 1 && d < 5 * binHz) harm += e; else other += e;
    }
    return 10.0 * std::log10 ((other + 1e-30) / (harm + 1e-30));
}

//  time for the windowed peak envelope to fall `db` below the hit's peak
double fallTime (const std::vector<float>& x, double fs, double db)
{
    const float pk = peakOf (x);
    const float thr = pk * (float) std::pow (10.0, -db / 20.0);
    const int w = (int) (0.01 * fs);
    int last = 0;
    for (int i = 0; i + w <= (int) x.size(); i += w / 4)
        if (peakOf (x, i, i + w) > thr) last = i + w;
    return last / fs;
}
} // namespace

int main (int argc, char** argv)
{
   #if defined(__SSE2__) || defined(_M_X64)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif

    const bool levels = argc > 1 && std::strcmp (argv[1], "--levels") == 0;
    if (levels)
    {
        for (int i = 0; i < numPresets(); ++i)
        {
            const auto x = one (presetParams (i), 48000.0, 2.0);
            std::printf ("%-20s peak %6.2f dBFS  rms(0-150ms) %6.1f dB  body -40dB at %.2f s\n",
                         preset (i).name, 20 * std::log10 (peakOf (x) + 1e-9),
                         rmsDb (x, 0, 7200), fallTime (x, 48000.0, 40.0));
        }
        return 0;
    }

    std::printf ("KICKSTART bench\n\n");

    // ---- 0. the tables ------------------------------------------------------
    {
        bool ok = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            Params p;
            for (int k = 0; k < preset (i).numValues; ++k)
                if (! setById (p, preset (i).values[k].id, preset (i).values[k].v))
                { ok = false; std::printf ("  preset %s names unknown id %s\n", preset (i).name, preset (i).values[k].id); }
            const Params q = presetParams (i);
            for (int k = 0; k < kNumParams; ++k)
            {
                const auto& s = specs()[k];
                const float v = q.*(s.member);
                if (v < s.lo - 1e-4f || v > s.hi + 1e-4f)
                { ok = false; std::printf ("  preset %s: %s = %g outside %g..%g\n", preset (i).name, s.id, v, s.lo, s.hi); }
            }
        }
        CHECK (ok, "a preset names an unknown parameter or leaves its range");
        std::printf ("  %d parameters, %d presets, every preset value known and in range\n", kNumParams, numPresets());
    }

    // ---- 1. silence and determinism ------------------------------------------
    {
        const auto z = render (presetParams (22), 48000.0, 0.5, {});
        bool silent = true; for (float v : z) silent = silent && v == 0.0f;
        CHECK (silent, "no hit, and yet not silent");
        const auto a = one (presetParams (21)), b = one (presetParams (21));
        CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0, "the same hit twice is not the same");
        //  and a voice ENDS: exact zero after the tail (no room, no comp)
        auto p = pure (52, 12, 200);
        const auto t = one (p, 48000.0, 3.0);
        bool ends = true; for (int i = 96000; i < 144000; ++i) ends = ends && t[(size_t) i] == 0.0f;
        CHECK (ends, "a 200 ms kick is still making sound at 2 s (the DC blocker's tail must flush)");
        std::printf ("  silence exact, hits deterministic, a voice ends in exact zero\n");
    }

    // ---- 2. tuning --------------------------------------------------------------
    {
        double worst = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float hz : { 30.0f, 52.0f, 110.0f, 160.0f })
            {
                const auto x = one (pure (hz, 18, 3000), fs, 0.6);
                const double f = zcFreq (x, (int) (0.3 * fs), (int) (0.55 * fs), fs);
                worst = std::max (worst, std::abs (1200.0 * std::log2 (f / hz)));
            }
        std::printf ("  settled pitch, 4 notes x 3 rates: worst %.3f cents\n", worst);
        CHECK (worst < 1.0, "the kick settles %.2f cents off PITCH", worst);

        //  the sweep starts where it says (a slow bend so the first cycles
        //  are nearly steady)
        const auto x = one (pure (52, 12, 3000), 48000.0, 0.2);
        Params p = pure (52, 12, 3000); p.bend = 300;
        const auto y = one (p, 48000.0, 0.2);
        //  start after the latency (95 samples), or the window reads pre-ring
        const double f0 = zcFreq (y, 100, 100 + (int) (0.03 * 48000), 48000.0);
        std::printf ("  SWEEP 12 st from 52 Hz starts at %.1f Hz (104 wanted, within the first 30 ms)\n", f0);
        CHECK (f0 > 97.0 && f0 < 106.0, "the sweep starts at %.1f Hz", f0);
        (void) x;

        //  key tracking: an octave up
        Params k = pure (52, 18, 3000); k.key = 1;
        const auto kx = one (k, 48000.0, 0.6, 48);
        const double kf = zcFreq (kx, 14400, 26400, 48000.0);
        std::printf ("  KEY on, C2: %.3f Hz (104 wanted)\n", kf);
        CHECK (std::abs (1200.0 * std::log2 (kf / 104.0)) < 1.0, "key tracking lands at %.2f Hz", kf);
    }

    // ---- 3. decay and curve -------------------------------------------------------
    {
        double worst = 0;
        for (float c : { 0.0f, 0.5f, 1.0f })
            for (float d : { 150.0f, 450.0f, 1500.0f })
            {
                Params p = pure (60, 0, d); p.curve = c;
                const auto x = one (p, 48000.0, d / 1000.0 + 0.5);
                const double t = fallTime (x, 48000.0, 60.0) * 1000.0;
                worst = std::max (worst, std::abs (t / d - 1.0));
            }
        std::printf ("  DECAY hits -60 dB on time: worst %.1f %% over 3 curves x 3 lengths\n", 100 * worst);
        CHECK (worst < 0.12, "DECAY is %.0f %% off", 100 * worst);

        //  CURVE is the modern hold: at half the decay, curve 1 is far louder
        Params a = pure (60, 0, 800); Params b = a; b.curve = 1.0f;
        const auto xa = one (a, 48000.0, 1.0), xb = one (b, 48000.0, 1.0);
        const double da = rmsDb (xa, 19000, 20000), db = rmsDb (xb, 19000, 20000);
        std::printf ("  at 400 ms of an 800 ms kick: CURVE 0 %.1f dB, CURVE 100 %.1f dB\n", da, db);
        CHECK (db - da > 20.0, "CURVE does not hold the body (%.1f vs %.1f)", db, da);
    }

    // ---- 4. latency: the decimator, measured by phase -------------------------------
    {
        const double fs = 48000.0, f = 150.0;
        const auto x = one (pure ((float) f, 0, 4000), fs, 0.5);
        //  the synthesised tone is sin(2 pi f n / fs) from sample 0; what comes
        //  out lags it by the decimator (and a hair of lead from the 2 Hz DC
        //  blocker, removed analytically)
        const auto g = goertzel (x, 9600, 19200, f, fs);
        const double ideal = -PI / 2.0;                      // sin = cos shifted
        double lag = std::remainder (ideal - std::arg (g), 2 * PI);
        Engine le; le.prepare (fs, 256);
        const int L = le.latencySamples();
        const double samples = lag / (2 * PI * f / fs);
        std::printf ("  latency %.2f samples (reported %d: decimator + 1.5 ms look-ahead)\n", samples, L);
        CHECK (std::abs (samples - L) < 0.6, "the latency is %.2f samples, not %d", samples, L);
    }

    // ---- 5. retrigger does not click -------------------------------------------------
    {
        Params p = pure (52, 18, 900); p.wave = 0.3f;
        const auto two  = render (p, 48000.0, 0.6, { { 0.0, 36, 1.0f }, { 0.2, 36, 1.0f } });
        const auto solo = render (p, 48000.0, 0.6, { { 0.2, 36, 1.0f } });
        const auto old  = render (p, 48000.0, 0.6, { { 0.0, 36, 1.0f } });
        auto maxStep = [] (const std::vector<float>& x, int a, int b) { float m = 0; for (int i = a + 1; i < b; ++i) m = std::max (m, std::abs (x[(size_t) i] - x[(size_t) i - 1])); return m; };
        const int at = 9600;
        const float s2 = maxStep (two, at - 50, at + 480);
        const float bound = maxStep (solo, at - 50, at + 480) + maxStep (old, at - 50, at + 480);
        std::printf ("  retrigger mid-decay: largest step %.4f against %.4f for the two hits apart\n", s2, bound);
        CHECK (s2 <= bound * 1.05f, "a retrigger clicks (%.4f > %.4f)", s2, bound);
    }

    // ---- 6. SKIN: the membrane's modes, and the tension glide ------------------------
    {
        Params a = pure (60, 0, 800); Params b = a; b.skin = 1.0f;
        const auto xa = one (a), xb = one (b);
        const double ma = ampDb (xa, 480, 4800, 60 * 1.5933 * 1.0, 48000.0);
        //  with skin, the whole head is sharp while loud: measure the mode near
        //  where it actually is
        double mb = -300;
        for (double c = 1.0; c <= 1.2; c += 0.005) mb = std::max (mb, ampDb (xb, 480, 4800, 60 * 1.5933 * c, 48000.0));
        std::printf ("  SKIN 0 -> 100: the (1,1) membrane mode %.1f -> %.1f dB\n", ma, mb);
        CHECK (mb - ma > 20.0, "SKIN does not grow the membrane modes");

        Params t = pure (60, 0, 1500); t.skin = 1.0f; t.velo = 0.0f;
        const auto xt = one (t, 48000.0, 1.2);
        const double early = zcFreq (xt, 240, 2400, 48000.0), late = zcFreq (xt, 48000, 57000, 48000.0);
        const double st = 12.0 * std::log2 (early / late);
        std::printf ("  SKIN tension glide: %.2f st sharp while loud, settling to %.2f Hz\n", st, late);
        CHECK (st > 0.8, "a hard hit on a real head should start sharp (%.2f st)", st);
        CHECK (std::abs (1200 * std::log2 (late / 60.0)) < 15.0, "the head settles at %.2f Hz", late);
    }

    // ---- 7. WAVE: harmonics grow from sine through triangle to square ------------------
    {
        double prev = -300; bool mono = true; double h3[3];
        int k = 0;
        for (float w : { 0.0f, 0.5f, 1.0f })
        {
            Params p = pure (100, 0, 3000); p.wave = w; p.curve = 1.0f;
            const auto x = one (p, 48000.0, 0.5);
            h3[k] = ampDb (x, 4800, 19200, 300, 48000.0) - ampDb (x, 4800, 19200, 100, 48000.0);
            mono = mono && h3[k] > prev; prev = h3[k]; ++k;
        }
        std::printf ("  WAVE 0/50/100: 3rd harmonic %.1f / %.1f / %.1f dB\n", h3[0], h3[1], h3[2]);
        CHECK (mono && h3[0] < -90.0 && h3[1] > -25.0, "WAVE does not add harmonics as it should");
    }

    // ---- 8. CLICK and TONE -----------------------------------------------------------
    {
        double c[3]; int k = 0;
        for (float t : { 0.0f, 0.5f, 1.0f })
        {
            Params p = pure (30, 0, 60); p.click = 1.0f; p.tone = t;
            const auto x = one (p, 48000.0, 0.1);
            std::vector<std::complex<double>> b (1024);
            for (int i = 0; i < 1024; ++i) b[(size_t) i] = x[(size_t) i];
            fft (b);
            double num = 0, den = 0;
            for (int j = 1; j < 512; ++j) { const double e = std::norm (b[(size_t) j]); num += e * j * 48000.0 / 1024; den += e; }
            c[k++] = num / den;
        }
        std::printf ("  CLICK TONE 0/50/100: spectral centroid %.0f / %.0f / %.0f Hz\n", c[0], c[1], c[2]);
        CHECK (c[0] < c[1] && c[1] < c[2] && c[2] > 4 * c[0], "TONE does not brighten the click");

        Params a = pure (52, 12, 400); Params b = a; b.click = 1.0f;
        const auto xa = one (a), xb = one (b);
        auto diffDb = [] (const std::vector<float>& x, int a, int b)
        { double e = 0; for (int i = a + 1; i < b; ++i) { const double d = x[(size_t) i] - x[(size_t) i - 1]; e += d * d; } return 10 * std::log10 (e / (b - a) + 1e-30); };
        const double hfa = diffDb (xa, 0, 480), hfb = diffDb (xb, 0, 480);
        std::printf ("  CLICK 0 -> 100: rate of change in the first 10 ms %.1f -> %.1f dB\n", hfa, hfb);
        CHECK (hfb - hfa > 12.0, "CLICK adds nothing at the attack");
    }

    // ---- 9. TRANSIENT --------------------------------------------------------------------
    {
        auto ratio = [&] (float att, float sus, double& early, double& tail)
        {
            Params p = pure (55, 12, 600); p.click = 0.5f; p.attack = att; p.sustain = sus;
            const auto x = one (p, 48000.0, 0.8);
            early = rmsDb (x, 0, 480); tail = rmsDb (x, 9600, 24000);
        };
        double e0, t0, eA, tA, eC, tC, eS, tS, eD, tD;
        ratio (0, 0, e0, t0); ratio (1, 0, eA, tA); ratio (-1, 0, eC, tC); ratio (0, 1, eS, tS); ratio (0, -1, eD, tD);
        std::printf ("  ATTACK +/-: first 10 ms %+.1f / %+.1f dB;  SUSTAIN +/-: 200-500 ms %+.1f / %+.1f dB\n",
                     eA - e0, eC - e0, tS - t0, tD - t0);
        CHECK (eA - e0 > 2.0 && eC - e0 < -2.0, "ATTACK does not move the attack");
        CHECK (tS - t0 > 2.0 && tD - t0 < -4.0, "SUSTAIN does not move the tail");
        CHECK (std::abs (tA - t0) < 1.5, "ATTACK leaks into the tail (%.1f dB)", tA - t0);

        //  and it is SMOOTH: the shaped kick is the unshaped one times a
        //  gain that moves slowly. Fit that gain every 5 ms, interpolate, and
        //  what is left is the modulation a rippling detector would add.
        Params p = pure (60, 0, 1500); p.curve = 0.3f; p.attack = 0; p.sustain = 1;
        Params q = p; q.sustain = 0;
        const auto xs = one (p, 48000.0, 0.8), xu = one (q, 48000.0, 0.8);
        const int B = 240, a0 = 4800, a1 = 36000;
        std::vector<double> gb;
        for (int b = a0; b + B <= a1; b += B)
        { double n2 = 0, d2 = 0; for (int i = b; i < b + B; ++i) { n2 += (double) xs[(size_t) i] * xu[(size_t) i]; d2 += (double) xu[(size_t) i] * xu[(size_t) i]; } gb.push_back (n2 / (d2 + 1e-30)); }
        double res = 0, sig = 0;
        for (int i = a0 + B / 2; i < a1 - B; ++i)
        {
            const double fpos = (double) (i - a0 - B / 2) / B; const int k0 = (int) fpos; const double fr = fpos - k0;
            const double g = gb[(size_t) k0] + (gb[(size_t) k0 + 1] - gb[(size_t) k0]) * fr;
            const double e = xs[(size_t) i] - g * xu[(size_t) i];
            res += e * e; sig += (double) xs[(size_t) i] * xs[(size_t) i];
        }
        const double rdb = 10 * std::log10 (res / sig + 1e-30);
        std::printf ("  SUSTAIN +100 is a smooth gain: residual after a 5 ms gain fit %.1f dB\n", rdb);
        CHECK (rdb < -50.0, "the transient shaper's gain ripples (%.1f dB)", rdb);
    }

    // ---- 10. ROOM -------------------------------------------------------------------------
    {
        Params a = pure (55, 12, 200); Params b = a; b.room = 0.7f;
        const auto xa = one (a, 48000.0, 1.0), xb = one (b, 48000.0, 1.0);
        const double ta = rmsDb (xa, 12000, 16000), tb = rmsDb (xb, 12000, 16000);
        std::printf ("  ROOM 0 -> 70: 250-330 ms %.1f -> %.1f dB\n", ta, tb);
        CHECK (tb - ta > 15.0, "ROOM adds no tail");
        CHECK (peakOf (xb) < 1.0f && finite (xb), "ROOM is not bounded");
    }

    // ---- 11. DRIVE: four engines, level-matched, each actually driving ----------------------
    {
        Params c = pure (55, 12, 500); c.curve = 0.3f;
        const auto clean = one (c);
        const double lc = rmsDb (clean, 0, 24000);
        double worst = 0;
        for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
            for (float d : { 0.4f, 1.0f })
            {
                Params p = c; p.drive = d; p.engine = (float) e;
                const auto x = one (p);
                const double l = rmsDb (x, 0, 24000);
                const double h = ampDb (x, 9600, 19200, 55 * 3, 48000.0) - ampDb (clean, 9600, 19200, 55 * 3, 48000.0);
                worst = std::max (worst, std::abs (l - lc));
                std::printf ("    %-11s drive %3.0f%%: level %+5.1f dB, 3rd harmonic %+6.1f dB over clean\n",
                             driveEngineName (e), d * 100, l - lc, h);
                CHECK (h > 10.0 || (e == ENG_HYPER), "%s at %.0f %% adds no harmonics", driveEngineName (e), d * 100);
                CHECK (finite (x) && peakOf (x) <= 1.0f, "%s is not bounded", driveEngineName (e));
            }
        std::printf ("  drive level match, worst %.1f dB across engines and settings\n", worst);
        CHECK (worst < 4.5, "a drive engine changes the level by %.1f dB", worst);

        //  HYPERDRIVE's job: harmonics a small speaker can play
        Params h = c; h.drive = 0.8f; h.engine = ENG_HYPER;
        const auto x = one (h);
        const double up = ampDb (x, 9600, 19200, 110, 48000.0) - ampDb (clean, 9600, 19200, 110, 48000.0);
        std::printf ("  HYPERDRIVE: 2nd harmonic %+.1f dB over clean\n", up);
        CHECK (up > 20.0, "HYPERDRIVE does not add the harmonics it is for");
    }

    // ---- 12. aliasing -----------------------------------------------------------------------
    {
        //  a steady held tone (so every spectral line should be a harmonic),
        //  as bright as the plug-in gets: rounded square through RAZOR WING
        Params p = pure (52, 0, 4000); p.curve = 1.0f; p.wave = 1.0f; p.drive = 1.0f; p.engine = ENG_RAZOR;
        const double typical = inharmonicDb (one (p, 48000.0, 1.0), 4800, 52, 48000.0);
        p.key = 1.0f;
        const auto hi = one (p, 48000.0, 1.0, 36 + 36);      // 416 Hz
        const double high = inharmonicDb (hi, 4800, 416, 48000.0);
        std::printf ("  aliasing, square through RAZOR WING at 100 %%: %.1f dB at 52 Hz, %.1f dB at 416 Hz\n", typical, high);
        CHECK (typical < -70.0, "aliasing at a kick's own pitch is %.1f dB", typical);
        CHECK (high < -45.0, "aliasing at 416 Hz is %.1f dB", high);
    }

    // ---- 13. GRIT, COLOUR, COMP, VELOCITY ------------------------------------------------------
    {
        Params a = pure (55, 12, 500); a.click = 0.5f;
        Params g = a; g.grit = 0.55f;
        Params gc0 = pure (80, 0, 3000); gc0.curve = 1;
        const double ia = inharmonicDb (one (gc0), 2400, 80, 48000.0);
        Params gs = gc0; gs.grit = 0.55f;
        const double ig = inharmonicDb (one (gs), 2400, 80, 48000.0);
        std::printf ("  GRIT 55 %%: off-harmonic energy %.1f dB (clean %.1f)\n", ig, ia);
        CHECK (ig > ia + 20.0, "GRIT does not degrade");

        Params col = a; col.colour = 0.3f;
        const auto xa = one (a), xc = one (col);
        const double hfa = ampDb (xa, 0, 480, 6000, 48000.0), hfc = ampDb (xc, 0, 480, 6000, 48000.0);
        std::printf ("  COLOUR 30 %%: 6 kHz in the attack %+.1f dB\n", hfc - hfa);
        CHECK (hfc - hfa < -6.0, "COLOUR does not darken");

        Params cp = a; cp.comp = 0.7f; cp.speed = 0.8f;
        const auto xp = one (cp);
        //  what a compressor does to a kick: the tail comes up to the head
        const double spanA = rmsDb (xa, 100, 1060) - rmsDb (xa, 7200, 12000);
        const double spanP = rmsDb (xp, 100, 1060) - rmsDb (xp, 7200, 12000);
        std::printf ("  COMP 70 %%: head-to-tail span %.1f -> %.1f dB\n", spanA, spanP);
        CHECK (spanA - spanP > 6.0, "COMP does not compress (%.1f -> %.1f)", spanA, spanP);

        Params v = a; v.velo = 1.0f;
        const double loud = rmsDb (one (v, 48000.0, 1.0, 36, 1.0f), 0, 14400);
        const double soft = rmsDb (one (v, 48000.0, 1.0, 36, 0.3f), 0, 14400);
        v.velo = 0.0f;
        const auto s0 = one (v, 48000.0, 1.0, 36, 0.3f), s1 = one (v, 48000.0, 1.0, 36, 1.0f);
        std::printf ("  VELOCITY 100 %%: soft hit %.1f dB under a hard one; at 0 %% identical\n", loud - soft);
        CHECK (loud - soft > 8.0, "VELOCITY does nothing");
        CHECK (std::memcmp (s0.data(), s1.data(), s0.size() * sizeof (float)) == 0, "VELOCITY 0 still responds to velocity");
    }

    // ---- 14. every preset, and 300 random kicks, bounded and audible ------------------------------
    {
        float worstPk = 0; double quietest = 0; bool fin = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            const auto x = one (presetParams (i), 48000.0, 1.0);
            worstPk = std::max (worstPk, peakOf (x));
            quietest = std::min (quietest, rmsDb (x, 0, 7200));
            fin = fin && finite (x);
        }
        uint32_t r = 12345;
        auto rnd = [&] { r = r * 1664525u + 1013904223u; return (float) (r >> 8) / 16777216.0f; };
        for (int i = 0; i < 300; ++i)
        {
            Params p;
            for (int k = 0; k < kNumParams; ++k)
            {
                const auto& s = specs()[k];
                float v = s.lo + (s.hi - s.lo) * rnd();
                if (s.kind != K_FLOAT) v = std::round (v);
                p.*(s.member) = v;
            }
            const auto x = render (p, 48000.0, 0.6, { { 0.0, 36 + (int) (rnd() * 24), rnd() }, { 0.3, 36, 1.0f } });
            worstPk = std::max (worstPk, peakOf (x));
            fin = fin && finite (x);
        }
        std::printf ("  %d presets + 300 random kicks: worst peak %.4f, quietest preset %.1f dB\n",
                     numPresets(), worstPk, quietest);
        CHECK (fin, "a non-finite sample");
        CHECK (worstPk <= 1.0f, "a kick left the ceiling (%.4f)", worstPk);
        CHECK (quietest > -30.0, "a preset is nearly silent (%.1f dB)", quietest);
    }

    // ---- 15. cost ----------------------------------------------------------------------------
    {
        Params p = presetParams (6);                    // skin, room: the heaviest body
        p.drive = 0.6f; p.comp = 0.5f; p.grit = 0.2f; p.attack = 0.5f;
        std::vector<Hit> hits;
        for (int i = 0; i < 40; ++i) hits.push_back ({ i * 0.25, 36, 1.0f });
        const auto t0 = std::chrono::steady_clock::now();
        const auto x = render (p, 48000.0, 10.0, hits);
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  cost, everything on, a hit every 250 ms: %.2f %% of one core\n", 100.0 * s / 10.0);
        CHECK (finite (x), "cost run non-finite");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
