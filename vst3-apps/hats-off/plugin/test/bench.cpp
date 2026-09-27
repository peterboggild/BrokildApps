// HATS OFF offline bench. Plain C++17, no JUCE. Every control is checked for
// the thing it is FOR, not merely for staying bounded (Martian Gain's lesson).
//
//   hotest            run everything, print ALL CLEAR or the failures
//   hotest --levels   print every preset's peak and loudness

#include "../engine/ho_engine.h"
#include "../engine/ho_presets.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64)
 #include <xmmintrin.h>
#endif

using namespace ho;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double FS = 48000.0;
constexpr int LAT = 103;

struct Hit { double t; int note; float vel; int artic = -1; float strike = -1.0f; };
struct St  { std::vector<float> L, R; };

St renderSt (const Params& p, double secs, const std::vector<Hit>& hits, double fs = FS, int block = 256, double bpm = 120.0)
{
    Engine e; e.prepare (fs, block); e.setTempo (bpm);
    const int n = (int) (secs * fs);
    St o; o.L.assign ((size_t) n, 0.0f); o.R.assign ((size_t) n, 0.0f);
    size_t h = 0;
    for (int i = 0; i < n; )
    {
        int m = std::min (block, n - i);
        if (h < hits.size())
        {
            const int at = (int) std::lround (hits[h].t * fs);
            if (at <= i)
            {
                if (hits[h].artic >= 0) e.noteOnArtic (hits[h].artic, hits[h].vel, hits[h].strike);
                else                    e.noteOn (hits[h].note, hits[h].vel);
                ++h; continue;
            }
            m = std::min (m, at - i);
        }
        e.process (p, o.L.data() + i, o.R.data() + i, m);
        i += m;
    }
    return o;
}
std::vector<float> one (const Params& p, double secs = 1.0, int note = 51, float vel = 1.0f, double fs = FS)
{ return renderSt (p, secs, { { 0.0, note, vel } }, fs).L; }
std::vector<float> art (const Params& p, int artic, double secs = 1.0, float vel = 1.0f)
{ return renderSt (p, secs, { { 0.0, 42, vel, artic } }).L; }

//  a free cymbal, nothing but the plate
Params plate()
{
    Params p = presetParams (0);
    p.open = 1.0f; p.stick = 0.0f; p.sizzle = 0.0f; p.noise = 0.0f; p.width = 0.0f;
    p.cut = 20.0f; p.air = 1.0f; p.velo = 0.0f; p.keys = KEYS_FIXED; p.bloom = 0.0f; p.choke = 0.0f;
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

std::complex<double> goertzel (const std::vector<float>& x, int a, int b, double f, double fs = FS)
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
double ampDb (const std::vector<float>& x, int a, int b, double f, double fs = FS)
{ return 20.0 * std::log10 (std::abs (goertzel (x, a, b, f, fs)) + 1e-12); }

double peakFreq (const std::vector<float>& x, int a, int b, double f0, double fs = FS)
{
    double best = -1e9; int bi = 0; std::vector<double> m;
    for (int c = -60; c <= 60; ++c)
    {
        const double v = std::abs (goertzel (x, a, b, f0 * std::pow (2.0, c / 1200.0), fs));
        m.push_back (v);
        if (v > best) { best = v; bi = c + 60; }
    }
    double off = 0.0;
    if (bi > 0 && bi < (int) m.size() - 1)
    {
        const double y0 = m[(size_t) bi - 1], y1 = m[(size_t) bi], y2 = m[(size_t) bi + 1];
        const double den = y0 - 2 * y1 + y2;
        if (den != 0) off = 0.5 * (y0 - y2) / den;
    }
    return f0 * std::pow (2.0, (bi - 60 + off) / 1200.0);
}

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

//  power spectrum of [a, a+N), Hann, N a power of two
std::vector<double> spectrum (const std::vector<float>& x, int a, int N)
{
    std::vector<std::complex<double>> b ((size_t) N);
    for (int i = 0; i < N; ++i)
        b[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / N)) * (a + i < (int) x.size() ? (double) x[(size_t) (a + i)] : 0.0);
    fft (b);
    std::vector<double> p ((size_t) N / 2);
    for (int k = 0; k < N / 2; ++k) p[(size_t) k] = std::norm (b[(size_t) k]);
    return p;
}
double bandDb (const std::vector<float>& x, int a, int N, double f1, double f2)
{
    const auto p = spectrum (x, a, N);
    double e = 0; for (int k = 1; k < N / 2; ++k) { const double f = k * FS / N; if (f >= f1 && f < f2) e += p[(size_t) k]; }
    return 10 * std::log10 (e + 1e-30);
}
double centroid (const std::vector<float>& x, int a, int N)
{
    const auto p = spectrum (x, a, N);
    double num = 0, den = 0;
    for (int k = 1; k < N / 2; ++k) { num += p[(size_t) k] * k * FS / N; den += p[(size_t) k]; }
    return num / (den + 1e-30);
}
//  spectral flatness over [f1, f2): 1 is white noise, near 0 is a few lines
double flatness (const std::vector<float>& x, int a, int N, double f1, double f2)
{
    const auto p = spectrum (x, a, N);
    double lg = 0, ar = 0; int n = 0;
    for (int k = 1; k < N / 2; ++k) { const double f = k * FS / N; if (f < f1 || f >= f2) continue; lg += std::log (p[(size_t) k] + 1e-30); ar += p[(size_t) k]; ++n; }
    return std::exp (lg / n) / (ar / n + 1e-30);
}
//  spectral lines in [f1, f2): local maxima standing 12 dB over their neighbourhood
int lineCount (const std::vector<float>& x, int a, int N, double f1, double f2)
{
    const auto p = spectrum (x, a, N);
    int c = 0;
    for (int k = 8; k < N / 2 - 8; ++k)
    {
        const double f = k * FS / N; if (f < f1 || f >= f2) continue;
        bool mx = true; double nb = 0;
        for (int j = -8; j <= 8; ++j) { if (j == 0) continue; if (p[(size_t) (k + j)] >= p[(size_t) k]) mx = false; nb += p[(size_t) (k + j)]; }
        if (mx && p[(size_t) k] > 16.0 * nb / 16.0) ++c;
    }
    return c;
}
double inharmonicDb (const std::vector<float>& x, int a, double f0)
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
    const double binHz = FS / N;
    double harm = 0, other = 0;
    for (int k = 2; k < N / 2; ++k)
    {
        const double f = k * binHz; if (f > 20000.0) break;
        const double e = std::norm (b[(size_t) k]);
        const double h = f / f0, d = std::abs (h - std::round (h)) * f0;
        if (std::round (h) >= 1 && d < 5 * binHz) harm += e; else other += e;
    }
    return 10.0 * std::log10 ((other + 1e-30) / (harm + 1e-30));
}
double fallTime (const std::vector<float>& x, double db)
{
    const float pk = peakOf (x);
    const float thr = pk * (float) std::pow (10.0, -db / 20.0);
    const int w = (int) (0.005 * FS);
    int last = 0;
    for (int i = 0; i + w <= (int) x.size(); i += w / 4)
        if (peakOf (x, i, i + w) > thr) last = i + w;
    return last / FS;
}
//  the share of 2-12 kHz energy lying on the 808's lattice: the odd harmonics of
//  its six squares (at PITCH 0 and DENSITY 50 %, the original ratios)
double circuitShare (const std::vector<float>& x, int a)
{
    const int N = 16384;
    const auto p = spectrum (x, a, N);
    double on = 0, all = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * FS / N; if (f < 2000 || f >= 12000) continue;
        all += p[(size_t) k];
        bool hit = false;
        for (float f0 : k808Hz) { const double h = f / f0; const double n = std::round (h); if (((int) n & 1) && std::abs (h - n) * f0 < 4.0) hit = true; }
        if (hit) on += p[(size_t) k];
    }
    return on / (all + 1e-30);
}
double correlation (const std::vector<float>& a, const std::vector<float>& b, int i0, int i1)
{
    double ab = 0, aa = 0, bb = 0;
    for (int i = i0; i < i1; ++i) { ab += (double) a[(size_t) i] * b[(size_t) i]; aa += (double) a[(size_t) i] * a[(size_t) i]; bb += (double) b[(size_t) i] * b[(size_t) i]; }
    return ab / std::sqrt (aa * bb + 1e-30);
}
} // namespace

int main (int argc, char** argv)
{
   #if defined(__SSE2__) || defined(_M_X64)
    _mm_setcsr (_mm_getcsr() | 0x8040);
   #endif

    if (argc > 1 && std::strcmp (argv[1], "--levels") == 0)
    {
        for (int i = 0; i < numPresets(); ++i)
        {
            const auto x = one (presetParams (i), 2.0);
            std::printf ("%-20s peak %6.2f dBFS  rms(0-80ms) %6.1f dB  -40dB at %.2f s\n",
                         preset (i).name, 20 * std::log10 (peakOf (x) + 1e-9),
                         rmsDb (x, 0, 3840), fallTime (x, 40.0));
        }
        return 0;
    }

    std::setvbuf (stdout, nullptr, _IONBF, 0);
    std::printf ("HATS OFF bench\n\n");

    // ---- 0. the tables ------------------------------------------------------
    {
        bool ok = true;
        std::set<std::string> names, ids;
        for (int i = 0; i < numPresets(); ++i)
        {
            ok = ok && names.insert (preset (i).name).second;
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
        for (int k = 0; k < kNumParams; ++k) ok = ok && ids.insert (specs()[k].id).second;
        CHECK (ok, "a preset names an unknown parameter, leaves its range, or a name is used twice");
        std::printf ("  %d parameters, %d presets, every value known and in range, every name unique\n", kNumParams, numPresets());
    }

    // ---- 1. silence, determinism, variation, an end, mono ------------------------
    {
        const auto z = renderSt (presetParams (0), 0.5, {}).L;
        bool silent = true; for (float v : z) silent = silent && v == 0.0f;
        CHECK (silent, "no hit, and yet not silent");
        const auto a = one (presetParams (1)), b = one (presetParams (1));
        CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0, "the same take twice is not the same");
        //  but two hits in one take are not identical: a real cymbal never is
        Params p = plate(); p.decay = 300;
        const auto two = renderSt (p, 1.2, { { 0.0, 51, 1.0f }, { 0.6, 51, 1.0f } }).L;
        double d = 0, e = 0;
        for (int i = 0; i < 9600; ++i) { const double u = two[(size_t) (LAT + i)], w = two[(size_t) (LAT + 28800 + i)]; d += (u - w) * (u - w); e += u * u; }
        std::printf ("  two identical hits differ by %.1f dB (each mode's amplitude moves a little): no machine gun\n", 10 * std::log10 (d / e));
        CHECK (d / e > 0.02, "every hit is identical");
        const auto t = one (p, 3.0);
        bool ends = true; for (int i = 96000; i < 144000; ++i) ends = ends && t[(size_t) i] == 0.0f;
        CHECK (ends, "a 300 ms cymbal is still sounding at 2 s");
        bool mono = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            Params q = presetParams (i); q.width = 0.0f; q.echo = 0.0f;
            const auto s = renderSt (q, 0.6, { { 0.0, 51, 1.0f } });
            mono = mono && std::memcmp (s.L.data(), s.R.data(), s.L.size() * sizeof (float)) == 0;
        }
        CHECK (mono, "with WIDTH and ECHO at 0 the two channels differ");
        std::printf ("  silence exact, takes deterministic, a voice ends in exact zero, mono at WIDTH 0\n");
    }

    // ---- 2. tuning -----------------------------------------------------------------
    {
        Engine::benchSteady = true;
        double worst = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float sz : { 8.0f, 14.0f, 22.0f })
            {
                Params p = plate(); p.size = sz; p.density = 0.0f; p.decay = 8000;
                const float want = Engine::lowestModeHz (p);
                const auto x = one (p, 0.8, 51, 1.0f, fs);
                const double f = peakFreq (x, (int) (0.1 * fs), (int) (0.7 * fs), want, fs);
                worst = std::max (worst, std::abs (1200.0 * std::log2 (f / want)));
            }
        std::printf ("  the plate's lowest mode, 3 sizes x 3 rates: worst %.3f cents from 2900 / SIZE\n", worst);
        CHECK (worst < 1.0, "the lowest mode is %.2f cents off", worst);

        double wc = 0;
        for (float st : { -5.0f, 0.0f, 7.0f })
        {
            Params c = plate(); c.bronze = 0.0f; c.density = 0.5f; c.decay = 8000; c.pitch = st;
            const double want = 800.0 * std::pow (2.0, st / 12.0);
            const auto x = one (c, 0.6);
            const double f = peakFreq (x, 4800, 28800, want);
            wc = std::max (wc, std::abs (1200.0 * std::log2 (f / want)));
        }
        std::printf ("  the circuit's 800 Hz square across PITCH -5/0/+7: worst %.3f cents\n", wc);
        CHECK (wc < 1.0, "the 808 squares are %.2f cents off", wc);

        Params k = plate(); k.density = 0.0f; k.decay = 8000; k.keys = KEYS_CHROMATIC;
        const float f0 = Engine::lowestModeHz (k);
        const auto xk = one (k, 0.8, 54);
        const double fk = peakFreq (xk, 4800, 33600, 2.0 * f0);
        std::printf ("  KEYS CHROMATIC, F#2: %.2f Hz (%.2f wanted)\n", fk, 2.0 * f0);
        CHECK (std::abs (1200.0 * std::log2 (fk / (2.0 * f0))) < 1.0, "chromatic lands at %.2f Hz", fk);
        Engine::benchSteady = false;

        CHECK (Engine::articFor (KEYS_GM, 42) == ART_CLOSED && Engine::articFor (KEYS_GM, 44) == ART_PEDAL
               && Engine::articFor (KEYS_GM, 46) == ART_OPEN && Engine::articFor (KEYS_GM, 53) == ART_BELL
               && Engine::articFor (KEYS_GM, 49) == ART_EDGE && Engine::articFor (KEYS_GM, 51) == ART_DIALLED
               && Engine::articFor (KEYS_FIXED, 42) == ART_DIALLED, "the GM kit map is wrong");
    }

    // ---- 3. DECAY, and OPEN ---------------------------------------------------------
    {
        double worst = 0;
        for (float d : { 300.0f, 1500.0f, 5000.0f })
        {
            Params p = plate(); p.decay = d; p.density = 0.6f; p.strike = 1.0f;
            //  DECAY is the -60 dB time of a mode at 1 kHz (higher modes shorter,
            //  lower longer). Put the plate's LOWEST mode exactly there - SIZE 8,
            //  PITCH up 17.57 semitones - and follow that one mode's envelope with
            //  a narrow window; its nearest neighbour is over an octave away.
            p.size = 8.0f; p.pitch = (float) (12.0 * std::log2 (1000.0 / (2900.0 / 8.0))); p.density = 0.0f;
            Engine::benchSteady = true;
            const auto x = one (p, d / 1000.0 * 1.2 + 0.3);
            Engine::benchSteady = false;
            const int W = 2048;
            std::vector<double> ts, ls;
            double l0 = 0;
            for (int i = LAT + 2400; i + W < (int) x.size(); i += W / 2)
            {
                const double l = ampDb (x, i, i + W, 1000.0);
                if (ts.empty()) l0 = l;
                if (l < l0 - 45.0) break;
                ts.push_back ((i + W / 2 - LAT) / FS); ls.push_back (l);
            }
            //  least-squares slope in dB per second
            double mt = 0, ml = 0; for (size_t k = 0; k < ts.size(); ++k) { mt += ts[k]; ml += ls[k]; }
            mt /= (double) ts.size(); ml /= (double) ts.size();
            double num = 0, den = 0; for (size_t k = 0; k < ts.size(); ++k) { num += (ts[k] - mt) * (ls[k] - ml); den += (ts[k] - mt) * (ts[k] - mt); }
            const double t60 = -60.0 / (num / den) * 1000.0;
            worst = std::max (worst, std::abs (t60 / d - 1.0));
        }
        std::printf ("  DECAY: a mode at 1 kHz falls 60 dB on time, worst %.1f %% over three lengths\n", 100 * worst);
        CHECK (worst < 0.05, "DECAY is %.0f %% off", 100 * worst);

        Params o = presetParams (0); o.sizzle = 0.0f; o.stick = 0.0f;
        Params c = o; c.open = 0.0f; o.open = 1.0f;
        const double to = fallTime (one (o, 4.0), 40.0), tc = fallTime (one (c, 4.0), 40.0);
        std::printf ("  OPEN 100 -> 0: -40 dB after %.0f ms -> %.0f ms\n", to * 1000, tc * 1000);
        CHECK (to / tc > 15.0, "OPEN does not close the hat");
    }

    // ---- 4. latency --------------------------------------------------------------------
    {
        Engine::benchToneHz = 1000.0f;
        const auto x = one (plate(), 0.5);
        Engine::benchToneHz = 0.0f;
        const auto g = goertzel (x, 9600, 19200, 1000.0);
        //  the tone is sin(2 pi f n / fo) from the voice's first OS sample
        const double lag = std::remainder (-PI / 2.0 - std::arg (g), 2 * PI);
        Engine le; le.prepare (FS, 256);
        const double samples = lag / (2 * PI * 1000.0 / FS);
        //  1 kHz repeats every 48 samples: resolve the whole cycles from the onset
        int onset = 0; for (int i = 0; i < (int) x.size(); ++i) if (std::abs (x[(size_t) i]) > 0.05f) { onset = i; break; }
        const double full = samples + 48.0 * std::round ((le.latencySamples() - samples) / 48.0);
        std::printf ("  latency %.2f samples (reported %d; onset seen at %d)\n", full, le.latencySamples(), onset);
        CHECK (std::abs (full - le.latencySamples()) < 0.6 && std::abs (onset - le.latencySamples()) < 30, "the latency is %.2f, not %d", full, le.latencySamples());
    }

    // ---- 5. choke and voices ---------------------------------------------------------------
    {
        Params p = presetParams (presetByName ("Studio Hi-Hat 14"));
        const auto on  = renderSt (p, 1.2, { { 0.0, 46, 1.0f }, { 0.3, 42, 0.8f } }).L;
        Params q = p; q.choke = 0.0f;
        const auto off = renderSt (q, 1.2, { { 0.0, 46, 1.0f }, { 0.3, 42, 0.8f } }).L;
        const double a = rmsDb (on, LAT + 19200, LAT + 28800), b = rmsDb (off, LAT + 19200, LAT + 28800);
        std::printf ("  an open hat, then a closed one: 400-600 ms %.1f dB with CHOKE, %.1f without\n", a, b);
        CHECK (b - a > 30.0, "CHOKE does not close the open hat");

        auto maxStep = [] (const std::vector<float>& x, int i0, int i1) { float m = 0; for (int i = i0 + 1; i < i1; ++i) m = std::max (m, std::abs (x[(size_t) i] - x[(size_t) i - 1])); return m; };
        std::vector<Hit> roll;
        for (int i = 0; i < 16; ++i) roll.push_back ({ i * 0.0625, 51, 0.8f });
        Params r = presetParams (presetByName ("Crash 18"));
        const auto x = renderSt (r, 2.0, roll).L;
        const auto s = one (r, 0.5, 51, 0.8f);
        std::printf ("  sixteen crashes at 1/64 of a second: peak %.3f, largest step %.3f against %.3f for one\n",
                     peakOf (x), maxStep (x, 0, (int) x.size()), maxStep (s, 0, (int) s.size()));
        CHECK (finite (x) && peakOf (x) <= 1.0f, "a cymbal roll is not bounded");
        CHECK (maxStep (x, 0, (int) x.size()) < 3.0f * maxStep (s, 0, (int) s.size()), "stealing voices clicks");
    }

    // ---- 6. the metal: BRONZE, DENSITY, BLOOM, TRASH, NOISE --------------------------------
    {
        Params c = plate(); c.bronze = 0.0f; c.density = 0.5f; c.decay = 3000;
        Params b = c; b.bronze = 1.0f;
        const double s0 = circuitShare (one (c), 4800), s1 = circuitShare (one (b), 4800);
        std::printf ("  BRONZE 0 -> 100: share of 2-12 kHz on the 808's harmonic lattice %.0f %% -> %.0f %%\n", 100 * s0, 100 * s1);
        CHECK (s0 > 0.8 && s1 < 0.3, "BRONZE does not leave the circuit");

        Params d0 = plate(); d0.density = 0.0f; d0.decay = 4000;
        Params d1 = d0; d1.density = 1.0f;
        //  a few lines is a bell; a filled spectrum is a wash: how many sixth-octave
        //  bands between 1 and 12 kHz carry energy within 30 dB of the loudest
        auto coverage = [&] (const std::vector<float>& x)
        {
            const auto sp = spectrum (x, 4800, 16384);
            std::vector<double> band;
            for (double f = 1000.0; f < 12000.0; f *= std::pow (2.0, 1.0 / 6.0))
            {
                double e = 0; for (int k = 1; k < 8192; ++k) { const double fk = k * FS / 16384; if (fk >= f && fk < f * std::pow (2.0, 1.0 / 6.0)) e += sp[(size_t) k]; }
                band.push_back (e);
            }
            const double top = *std::max_element (band.begin(), band.end());
            int c = 0; for (double e : band) if (e > top * 1.0e-3) ++c;
            return std::make_pair (c, (int) band.size());
        };
        const auto c0 = coverage (one (d0)), c1 = coverage (one (d1));
        std::printf ("  DENSITY 0 -> 100: %d -> %d modes; sixth-octave bands 1-12 kHz filled %d -> %d of %d\n",
                     Engine::modeCount (d0), Engine::modeCount (d1), c0.first, c1.first, c1.second);
        CHECK (c1.first * 10 >= c1.second * 9 && c1.first > c0.first + 4, "DENSITY does not fill the spectrum");

        Params b0 = plate(); b0.strike = 1.0f; b0.decay = 3000; b0.density = 0.9f;
        Params b1 = b0; b1.bloom = 1.0f;
        auto rise = [&] (const Params& p)
        {
            const auto x = one (p, 0.5);
            return centroid (x, LAT + 2880, 1024) / centroid (x, LAT, 512);
        };
        const double r0 = rise (b0), r1 = rise (b1);
        std::printf ("  BLOOM 0 -> 100: brightness at 60 ms over the first 10 ms, x%.2f -> x%.2f\n", r0, r1);
        CHECK (r1 > 1.2 && r1 > r0 * 1.25, "BLOOM does not make the cymbal brighter after the hit");

        //  measured where the lines are sparse: the WASH is noise already, and
        //  a flatness measure cannot see roughness added to something flat
        Params t0 = plate(); t0.decay = 3000; t0.density = 0.1f;
        Params t1 = t0; t1.trash = 1.0f;
        const double f0 = flatness (one (t0), 4800, 8192, 1000, 10000), f1 = flatness (one (t1), 4800, 8192, 1000, 10000);
        std::printf ("  TRASH 0 -> 100: spectral flatness %.3f -> %.3f\n", f0, f1);
        CHECK (f1 > 2.0 * f0, "TRASH adds no roughness");

        Params n0p = plate(); n0p.decay = 2000; n0p.density = 0.2f;
        Params n1p = n0p; n1p.noise = 1.0f;
        const double g0 = flatness (one (n0p), 2400, 8192, 1000, 15000), g1 = flatness (one (n1p), 2400, 8192, 1000, 15000);
        std::printf ("  NOISE 0 -> 100: spectral flatness %.3f -> %.3f\n", g0, g1);
        CHECK (g1 > 3.0 * g0, "NOISE adds no noise");

        Params s8 = plate(); s8.size = 8; s8.decay = 2000;
        Params s24 = s8; s24.size = 24;
        const double c8 = centroid (one (s8), 2400, 4096), c24 = centroid (one (s24), 2400, 4096);
        std::printf ("  SIZE 8 -> 24 inches: centroid %.0f -> %.0f Hz\n", c8, c24);
        CHECK (c24 < c8 * 0.85, "a bigger cymbal is not lower");
    }

    // ---- 7. the hat: SIZZLE, CHICK --------------------------------------------------------
    {
        Params h = presetParams (0); h.open = 0.5f; h.sizzle = 0.0f; h.stick = 0.0f; h.decay = 2000;
        Params s = h; s.sizzle = 1.0f;
        const double a0 = bandDb (one (h), 1200, 4096, 5000, 16000), a1 = bandDb (one (s), 1200, 4096, 5000, 16000);
        Params hc = h; hc.open = 0.0f; Params sc = hc; sc.sizzle = 1.0f;
        const double c0 = bandDb (one (hc), 240, 1024, 5000, 16000), c1 = bandDb (one (sc), 240, 1024, 5000, 16000);
        std::printf ("  SIZZLE 0 -> 100: half-open +%.1f dB of 5-16 kHz; closed +%.1f dB (the plates cannot rattle)\n", a1 - a0, c1 - c0);
        CHECK (a1 - a0 > 3.0 && c1 - c0 < 1.0, "SIZZLE is not the half-open rattle");

        Params ch = presetParams (0);
        Params cz = ch; cz.chick = 0.0f; ch.chick = 1.0f;
        const double k1 = bandDb (art (ch, ART_PEDAL, 0.2), LAT, 1024, 1500, 4000), k0 = bandDb (art (cz, ART_PEDAL, 0.2), LAT, 1024, 1500, 4000);
        std::printf ("  CHICK 0 -> 100, the pedal note: 1.5-4 kHz in the first 20 ms %+.1f dB\n", k1 - k0);
        CHECK (k1 - k0 > 6.0, "CHICK adds nothing to the pedal");
    }

    // ---- 8. the hit: STRIKE, STICK ------------------------------------------------------------
    {
        Params b = plate(); b.decay = 4000; b.density = 0.8f;
        const auto bell = art (b, ART_BELL, 8.0), edge = art (b, ART_EDGE, 8.0);
        const double cb = centroid (bell, LAT, 4096), ce = centroid (edge, LAT, 4096);
        //  and the bell rings on: the dome partials outlast the bow
        const double lb = fallTime (bell, 40.0), le = fallTime (edge, 40.0);
        std::printf ("  STRIKE bell -> edge: centroid %.0f -> %.0f Hz; -40 dB after %.2f s on the bell, %.2f s on the edge\n", cb, ce, lb, le);
        CHECK (ce > 1.4 * cb && lb > 1.15 * le, "the bell and the edge sound alike");

        Params s0 = plate(); s0.stick = 0.0f;
        Params s1 = s0; s1.stick = 1.0f;
        const double st0 = bandDb (one (s0), LAT - 48, 128, 3000, 16000), st1 = bandDb (one (s1), LAT - 48, 128, 3000, 16000);
        std::printf ("  STICK 0 -> 100: 3-16 kHz in the first 2 ms %+.1f dB\n", st1 - st0);
        CHECK (st1 - st0 > 6.0, "STICK adds no tick");
    }

    // ---- 9. EQ, WIDTH ----------------------------------------------------------------------------
    {
        Params a = plate(); a.decay = 2000; a.size = 20;
        Params c = a; c.cut = 2000;
        const double lo0 = bandDb (one (a), 2400, 4096, 100, 600), lo1 = bandDb (one (c), 2400, 4096, 100, 600);
        std::printf ("  CUT 20 -> 2000 Hz: 100-600 Hz %+.1f dB\n", lo1 - lo0);
        CHECK (lo1 - lo0 < -20.0, "CUT does not thin");
        Params r = a; r.air = 0.0f;
        const double hi0 = bandDb (one (a), 2400, 4096, 10000, 18000), hi1 = bandDb (one (r), 2400, 4096, 10000, 18000);
        std::printf ("  AIR 100 -> 0: 10-18 kHz %+.1f dB\n", hi1 - hi0);
        CHECK (hi1 - hi0 < -15.0, "AIR does not darken");

        Params w = a; w.width = 1.0f;
        const auto s = renderSt (w, 0.8, { { 0.0, 51, 1.0f } });
        const double cc = correlation (s.L, s.R, 2400, 24000);
        const double ll = rmsDb (s.L, 2400, 24000), rr = rmsDb (s.R, 2400, 24000);
        std::printf ("  WIDTH 100: left/right correlation %.2f, balance %+.1f dB\n", cc, ll - rr);
        CHECK (cc < 0.6 && std::abs (ll - rr) < 2.0, "WIDTH does not spread the cymbal evenly");
    }

    // ---- 10. TRANSIENT ------------------------------------------------------------------------------
    {
        auto meas = [&] (float at, float su, double& early, double& tail)
        {
            Params p = plate(); p.stick = 0.5f; p.decay = 1500; p.attack = at; p.sustain = su;
            const auto x = one (p, 0.8);
            early = rmsDb (x, LAT, LAT + 240); tail = rmsDb (x, LAT + 9600, LAT + 19200);
        };
        double e0, t0, eA, tA, eC, tC, eS, tS, eD, tD;
        meas (0, 0, e0, t0); meas (1, 0, eA, tA); meas (-1, 0, eC, tC); meas (0, 1, eS, tS); meas (0, -1, eD, tD);
        std::printf ("  ATTACK +/-: first 5 ms %+.1f / %+.1f dB;  SUSTAIN +/-: 200-400 ms %+.1f / %+.1f dB\n",
                     eA - e0, eC - e0, tS - t0, tD - t0);
        CHECK (eA - e0 > 2.0 && eC - e0 < -2.0, "ATTACK does not move the attack");
        CHECK (tS - t0 > 2.0 && tD - t0 < -4.0, "SUSTAIN does not move the wash");
    }

    // ---- 11. ROOM, ECHO -------------------------------------------------------------------------------
    {
        Params a = plate(); a.decay = 150;
        Params b = a; b.room = 0.7f;
        const double ta = rmsDb (one (a, 1.5), 14400, 19200), tb = rmsDb (one (b, 1.5), 14400, 19200);
        std::printf ("  ROOM 0 -> 70: 300-400 ms %.1f -> %.1f dB\n", ta, tb);
        CHECK (tb - ta > 25.0, "ROOM adds no tail");

        Params p = plate(); p.decay = 100; p.echo = 0.6f; p.time = 3.0f;
        const auto s = renderSt (p, 2.0, { { 0.0, 51, 1.0f } });
        auto energyAt = [&] (const std::vector<float>& x, double t) { return rmsDb (x, LAT + (int) (t * FS) - 480, LAT + (int) (t * FS) + 2400); };
        const double l1 = energyAt (s.L, 0.375), r1 = energyAt (s.R, 0.375), l2 = energyAt (s.L, 0.75), r2 = energyAt (s.R, 0.75);
        std::printf ("  ECHO 1/8D at 120 BPM: at 375 ms L %.1f R %.1f dB; at 750 ms L %.1f R %.1f dB\n", l1, r1, l2, r2);
        CHECK (l1 - r1 > 30.0 && r2 - l2 > 30.0, "the echo does not ping-pong");
        Params f = p; f.echo = 1.0f;
        const auto sf = renderSt (f, 20.0, { { 0.0, 51, 1.0f } });
        const double early = rmsDb (sf.L, 48000, 4 * 48000), late = rmsDb (sf.L, 17 * 48000, 20 * 48000);
        std::printf ("  ECHO 100 %%: 1-4 s %.1f dB, 17-20 s %.1f dB\n", early, late);
        CHECK (late < early - 30.0 && finite (sf.L) && peakOf (sf.L) <= 1.0f, "full echo runs away");
    }

    // ---- 12. DRIVE, aliasing, GRIT ---------------------------------------------------------------------
    {
        Engine::benchToneHz = 2000.0f;
        Params c = plate(); c.cut = 20.0f;
        const auto clean = one (c, 0.6);
        const double lc = rmsDb (clean, 4800, 24000);
        double worst = 0;
        for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
            for (float d : { 0.4f, 1.0f })
            {
                Params q = c; q.drive = d; q.engine = (float) e;
                const auto x = one (q, 0.6);
                const double l = rmsDb (x, 4800, 24000);
                const double h = ampDb (x, 4800, 24000, 6000) - ampDb (clean, 4800, 24000, 6000);
                worst = std::max (worst, std::abs (l - lc));
                std::printf ("    %-11s drive %3.0f%%: level %+5.1f dB, 3rd harmonic %+6.1f dB over clean\n", driveEngineName (e), d * 100, l - lc, h);
                CHECK (h > 10.0 || e == ENG_HYPER, "%s adds no harmonics", driveEngineName (e));
            }
        std::printf ("  drive level match, worst %.1f dB\n", worst);
        CHECK (worst < 4.5, "a drive engine changes the level by %.1f dB", worst);
        Params r = c; r.drive = 1.0f; r.engine = ENG_RAZOR;
        const double al = inharmonicDb (one (r, 1.0), 16800, 2000.0);
        //  a cymbal lives high: at 4.7 kHz a FULL hard clip is a square whose 41st
        //  harmonic sits on the 4x rate and folds back (recorded, not hidden); the
        //  gate is a working setting
        Engine::benchToneHz = 4700.0f;
        const double alFull = inharmonicDb (one (r, 1.0), 16800, 4700.0);
        Params r4 = r; r4.drive = 0.4f;
        const double al2 = inharmonicDb (one (r4, 1.0), 16800, 4700.0);
        std::printf ("  aliasing, RAZOR WING: %.1f dB at 2 kHz full; at 4.7 kHz %.1f dB at 40 %%, %.1f dB full\n", al, al2, alFull);
        CHECK (al < -80.0 && al2 < -55.0, "aliasing %.1f / %.1f dB", al, al2);
        Engine::benchToneHz = 1000.0f;
        const double ia = inharmonicDb (one (c, 1.0), 16800, 1000.0);
        Params gp = c; gp.grit = 0.55f;
        const double ig = inharmonicDb (one (gp, 1.0), 16800, 1000.0);
        Engine::benchToneHz = 0.0f;
        std::printf ("  GRIT 55 %%: off-harmonic energy %.1f dB (clean %.1f)\n", ig, ia);
        CHECK (ig > ia + 20.0, "GRIT does not degrade");
    }

    // ---- 13. COLOUR, COMP, VELOCITY -----------------------------------------------------------------------
    {
        Params a = plate(); a.decay = 1500;
        Params col = a; col.colour = 0.3f;
        const double h0 = bandDb (one (a), 960, 2048, 6000, 16000), h1 = bandDb (one (col), 960, 2048, 6000, 16000);
        std::printf ("  COLOUR 30 %%: 6-16 kHz %+.1f dB\n", h1 - h0);
        CHECK (h1 - h0 < -6.0, "COLOUR does not darken");

        Params cn = plate(); cn.stick = 0.6f; cn.decay = 800;
        Params cp = cn; cp.comp = 0.7f; cp.speed = 0.8f;
        const auto xn = one (cn), xp = one (cp);
        const double sA = rmsDb (xn, LAT, LAT + 960) - rmsDb (xn, LAT + 9600, LAT + 19200);
        const double sP = rmsDb (xp, LAT, LAT + 960) - rmsDb (xp, LAT + 9600, LAT + 19200);
        std::printf ("  COMP 70 %%: head-to-wash span %.1f -> %.1f dB\n", sA, sP);
        CHECK (sA - sP > 6.0, "COMP does not compress");

        Params v = plate(); v.velo = 1.0f; v.decay = 1500;
        const auto hard = one (v, 1.0, 51, 1.0f), soft = one (v, 1.0, 51, 0.3f);
        const double loud = rmsDb (hard, 0, 14400) - rmsDb (soft, 0, 14400);
        const double br = centroid (hard, 960, 4096) - centroid (soft, 960, 4096);
        v.velo = 0.0f;
        const auto s0 = one (v, 1.0, 51, 0.3f), s1 = one (v, 1.0, 51, 1.0f);
        std::printf ("  VELOCITY 100 %%: a hard hit %.1f dB louder and %.0f Hz brighter; at 0 %% identical\n", loud, br);
        CHECK (loud > 8.0 && br > 300.0, "VELOCITY does not move loudness and brightness");
        CHECK (std::memcmp (s0.data(), s1.data(), s0.size() * sizeof (float)) == 0, "VELO 0 still responds");
    }

    // ---- 14. every preset, and 300 random cymbals -----------------------------------------------------------
    {
        float worstPk = 0; double quietest = 0; bool fin = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            const auto s = renderSt (presetParams (i), 1.0, { { 0.0, 51, 1.0f } });
            worstPk = std::max ({ worstPk, peakOf (s.L), peakOf (s.R) });
            quietest = std::min (quietest, rmsDb (s.L, 0, 3840));
            fin = fin && finite (s.L) && finite (s.R);
        }
        uint32_t r = 4242;
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
            const auto s = renderSt (p, 0.6, { { 0.0, 40 + (int) (rnd() * 20), rnd() }, { 0.2, 46, 1.0f },
                                              { 0.25, 42, 0.7f }, { 0.3, 49, 0.9f }, { 0.31, 44, 0.8f } });
            worstPk = std::max ({ worstPk, peakOf (s.L), peakOf (s.R) });
            fin = fin && finite (s.L) && finite (s.R);
        }
        std::printf ("  %d presets + 300 random cymbals: worst peak %.4f, quietest preset %.1f dB\n", numPresets(), worstPk, quietest);
        CHECK (fin, "a non-finite sample");
        CHECK (worstPk <= 1.0f, "a cymbal left the ceiling (%.4f)", worstPk);
        CHECK (quietest > -35.0, "a preset is nearly silent (%.1f dB)", quietest);
    }

    // ---- 15. cost ---------------------------------------------------------------------------------------
    {
        Params p = presetParams (presetByName ("Dark Ride 22"));
        p.room = 0.4f; p.drive = 0.3f; p.echo = 0.3f; p.width = 0.8f; p.bloom = 0.6f;
        std::vector<Hit> hits;
        for (int i = 0; i < 80; ++i) hits.push_back ({ i * 0.125, 51, 0.9f });
        const auto t0 = std::chrono::steady_clock::now();
        const auto s = renderSt (p, 10.0, hits);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  cost, a 22-inch ride on eighths with six ringing, everything on: %.1f %% of one core\n", 100.0 * sec / 10.0);
        CHECK (finite (s.L), "cost run non-finite");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
