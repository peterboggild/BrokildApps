// SNARE TACTICS offline bench. Plain C++17, no JUCE. Every claim the design
// makes is measured here against the real engine, and a claim that only
// BOUNDS the output proves nothing about whether a control works (Martian
// Gain's lesson), so each control is checked for the thing it is FOR.
//
//   sttest            run everything, print ALL CLEAR or the failures
//   sttest --levels   print every preset's peak and loudness

#include "../engine/st_engine.h"
#include "../engine/st_presets.h"

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

using namespace st;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double FS = 48000.0;

struct Hit { double t; int note; float vel; int artic = -1; };
struct St  { std::vector<float> L, R; };

St renderSt (const Params& p, double fs, double secs, const std::vector<Hit>& hits, int block = 256, double bpm = 120.0)
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
                if (hits[h].artic >= 0) e.noteOnArtic (hits[h].artic, hits[h].vel);
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

std::vector<float> render (const Params& p, double fs, double secs, const std::vector<Hit>& hits, int block = 256)
{
    return renderSt (p, fs, secs, hits, block).L;
}

std::vector<float> one (const Params& p, double fs = FS, double secs = 1.0, int note = 38, float vel = 1.0f)
{
    return render (p, fs, secs, { { 0.0, note, vel } });
}

std::vector<float> art (const Params& p, int artic, double secs = 1.0, float vel = 1.0f)
{
    return render (p, FS, secs, { { 0.0, 38, vel, artic } });
}

//  the head alone, as clean as it gets: no wires, no stick, no clap, no room
Params head (float hz, float decay)
{
    Params p = presetParams (0);
    p.tune = hz; p.drop = 0.0f; p.decay = decay;
    p.wires = 0.0f; p.stick = 0.0f; p.clap = 0.0f; p.strike = 0.0f;
    p.velo = 0.0f; p.keys = KEYS_FIXED;
    return p;
}

//  the wires alone
Params wiresOnly (float tension, float sizzle)
{
    Params p = presetParams (0);
    p.body = 0.0f; p.stick = 0.0f; p.clap = 0.0f; p.strike = 0.0f;
    p.wires = 1.0f; p.tension = tension; p.sizzle = sizzle; p.velo = 0.0f;
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
double ampDb (const std::vector<float>& x, int a, int b, double f, double fs = FS)
{ return 20.0 * std::log10 (std::abs (goertzel (x, a, b, f, fs)) + 1e-12); }

//  the strongest partial within +-60 cents of f0, found by a fine scan and a
//  parabola through the top three points
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

//  the strongest line anywhere within +-3 semitones of f (for a mode whose
//  exact place the glide moves), dB
double bandPeakDb (const std::vector<float>& x, int a, int b, double f, int cents = 300)
{
    double m = -300;
    for (int c = -cents; c <= cents; c += 5) m = std::max (m, ampDb (x, a, b, f * std::pow (2.0, c / 1200.0)));
    return m;
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

//  energy in [f1, f2) Hz over [a, a+N), dB (Hann)
double bandDb (const std::vector<float>& x, int a, int N, double f1, double f2)
{
    int n = 1; while (n < N) n <<= 1;
    std::vector<std::complex<double>> b ((size_t) n);
    for (int i = 0; i < N && a + i < (int) x.size(); ++i)
        b[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / N)) * (double) x[(size_t) (a + i)];
    fft (b);
    double e = 0;
    for (int k = 1; k < n / 2; ++k) { const double f = k * FS / n; if (f >= f1 && f < f2) e += std::norm (b[(size_t) k]); }
    return 10 * std::log10 (e + 1e-30);
}

double centroid (const std::vector<float>& x, int a, int N)
{
    int n = 1; while (n < N) n <<= 1;
    std::vector<std::complex<double>> b ((size_t) n);
    for (int i = 0; i < N; ++i) b[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / N)) * (double) x[(size_t) (a + i)];
    fft (b);
    double num = 0, den = 0;
    for (int k = 1; k < n / 2; ++k) { const double e = std::norm (b[(size_t) k]); num += e * k * FS / n; den += e; }
    return num / (den + 1e-30);
}

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

double fallTime (const std::vector<float>& x, double fs, double db)
{
    const float pk = peakOf (x);
    const float thr = pk * (float) std::pow (10.0, -db / 20.0);
    const int w = (int) (0.005 * fs);
    int last = 0;
    for (int i = 0; i + w <= (int) x.size(); i += w / 4)
        if (peakOf (x, i, i + w) > thr) last = i + w;
    return last / fs;
}

//  first sample above `frac` of the peak
int onsetOf (const std::vector<float>& x, float frac)
{
    const float thr = peakOf (x) * frac;
    for (int i = 0; i < (int) x.size(); ++i) if (std::abs (x[(size_t) i]) > thr) return i;
    return -1;
}

//  the rectified signal's envelope modulation at f relative to its mean, dB:
//  how much the wires' noise is chopped up at the head's pitch
double buzzDb (const std::vector<float>& x, int a, int b, double f)
{
    std::vector<float> r ((size_t) x.size(), 0.0f);
    double mean = 0;
    for (int i = a; i < b; ++i) { r[(size_t) i] = std::abs (x[(size_t) i]); mean += r[(size_t) i]; }
    mean /= (b - a);
    return 20 * std::log10 (std::abs (goertzel (r, a, b, f, FS)) / (mean + 1e-30) + 1e-12);
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
            const auto x = one (presetParams (i), FS, 2.0);
            std::printf ("%-20s peak %6.2f dBFS  rms(0-150ms) %6.1f dB  -40dB at %.2f s\n",
                         preset (i).name, 20 * std::log10 (peakOf (x) + 1e-9),
                         rmsDb (x, 0, 7200), fallTime (x, FS, 40.0));
        }
        return 0;
    }

    std::printf ("SNARE TACTICS bench\n\n");

    // ---- 0. the tables ------------------------------------------------------
    {
        bool ok = true;
        std::set<std::string> names;
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
        std::set<std::string> ids;
        for (int k = 0; k < kNumParams; ++k) ok = ok && ids.insert (specs()[k].id).second;
        CHECK (ok, "a preset names an unknown parameter, leaves its range, or a name is used twice");
        std::printf ("  %d parameters, %d presets, every value known and in range, every name unique\n", kNumParams, numPresets());
    }

    // ---- 1. silence, determinism, a voice ends, mono without the echo ----------
    {
        const auto z = render (presetParams (0), FS, 0.5, {});
        bool silent = true; for (float v : z) silent = silent && v == 0.0f;
        CHECK (silent, "no hit, and yet not silent");
        const auto a = one (presetParams (1)), b = one (presetParams (1));
        CHECK (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0, "the same hit twice is not the same");
        Params p = presetParams (0); p.decay = 200; p.sizzle = 300; p.clap = 0.6f;
        const auto t = one (p, FS, 3.0);
        bool ends = true; for (int i = 96000; i < 144000; ++i) ends = ends && t[(size_t) i] == 0.0f;
        CHECK (ends, "a 300 ms snare is still making sound at 2 s");
        bool mono = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            if (presetParams (i).echo > 0.0f) continue;
            const auto s = renderSt (presetParams (i), FS, 0.8, { { 0.0, 38, 1.0f } });
            mono = mono && std::memcmp (s.L.data(), s.R.data(), s.L.size() * sizeof (float)) == 0;
        }
        CHECK (mono, "with ECHO at 0 the two channels differ");
        std::printf ("  silence exact, hits deterministic, a voice ends in exact zero, mono until the echo\n");
    }

    // ---- 2. tuning -----------------------------------------------------------------
    {
        double worst = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float hz : { 110.0f, 185.0f, 300.0f, 420.0f })
            {
                Params p = head (hz, 2000); p.skin = 0.0f;
                const auto x = one (p, fs, 0.5);
                const double f = peakFreq (x, (int) (0.1 * fs), (int) (0.45 * fs), hz, fs);
                worst = std::max (worst, std::abs (1200.0 * std::log2 (f / hz)));
            }
        std::printf ("  the electronic pair settles on TUNE, 4 notes x 3 rates: worst %.3f cents\n", worst);
        CHECK (worst < 1.0, "the head settles %.2f cents off TUNE", worst);

        Engine::benchSteady = true;
        Params m = head (185, 2000); m.skin = 1.0f; m.ring = 0.0f;
        const auto xm = one (m, FS, 0.6);
        const double fm = zcFreq (xm, 14400, 26400, FS);
        Engine::benchSteady = false;
        std::printf ("  the membrane's (0,1) mode, glide frozen: %.3f Hz for 185\n", fm);
        CHECK (std::abs (1200 * std::log2 (fm / 185.0)) < 1.0, "the membrane sits at %.2f Hz", fm);

        Params d = head (185, 2000); d.skin = 0.0f; d.drop = 12.0f; d.bend = 300.0f;
        const auto xd = one (d, FS, 0.2);
        const double fd = peakFreq (xd, 100, 100 + 1440, 185.0 * 1.93);
        std::printf ("  DROP 12 st, slow bend: starts at %.1f Hz (about %.0f wanted in the first 30 ms)\n", fd, 185.0 * 1.93);
        CHECK (fd > 185.0 * 1.85 && fd < 185.0 * 2.02, "the drop starts at %.1f Hz", fd);

        Params k = head (185, 2000); k.skin = 0.0f; k.keys = KEYS_CHROMATIC;
        const auto xk = one (k, FS, 0.5, 50);
        const double fk = peakFreq (xk, 4800, 21600, 370.0);
        std::printf ("  KEYS CHROMATIC, D2: %.3f Hz (370 wanted)\n", fk);
        CHECK (std::abs (1200.0 * std::log2 (fk / 370.0)) < 1.0, "chromatic lands at %.2f Hz", fk);

        CHECK (Engine::articFor (KEYS_GM, 38) == ART_SNARE && Engine::articFor (KEYS_GM, 40) == ART_RIM
               && Engine::articFor (KEYS_GM, 37) == ART_XSTICK && Engine::articFor (KEYS_GM, 39) == ART_CLAP
               && Engine::articFor (KEYS_FIXED, 39) == ART_SNARE && Engine::articFor (KEYS_CHROMATIC, 37) == ART_SNARE,
               "the GM kit map is wrong");
    }

    // ---- 3. DECAY and SIZZLE hit -60 dB on time ------------------------------------
    {
        double worst = 0;
        for (float d : { 100.0f, 260.0f, 800.0f })
        {
            Params p = head (200, d); p.skin = 0.0f;
            const auto x = one (p, FS, d / 1000.0 + 0.5);
            const double t = fallTime (x, FS, 60.0) * 1000.0 - 1.98;     // less the latency
            worst = std::max (worst, std::abs (t / d - 1.0));
        }
        std::printf ("  DECAY hits -60 dB on time: worst %.1f %% over three lengths\n", 100 * worst);
        CHECK (worst < 0.12, "DECAY is %.0f %% off", 100 * worst);

        double worstW = 0;
        for (float s : { 120.0f, 300.0f, 900.0f })
        {
            const auto x = one (wiresOnly (1.0f, s), FS, s / 1000.0 + 0.5);
            const double t = fallTime (x, FS, 60.0) * 1000.0 - 1.98;
            worstW = std::max (worstW, std::abs (t / s - 1.0));
        }
        std::printf ("  SIZZLE hits -60 dB on time: worst %.1f %% over three lengths\n", 100 * worstW);
        CHECK (worstW < 0.15, "SIZZLE is %.0f %% off", 100 * worstW);
    }

    // ---- 4. latency: measured by phase ---------------------------------------------------
    {
        const double f = 150.0;
        Params p = head ((float) f, 2000); p.skin = 0.0f;
        const auto x = one (p, FS, 0.5);
        const auto g = goertzel (x, 9600, 19200, f, FS);
        const double lag = std::remainder (-PI / 2.0 - std::arg (g), 2 * PI);
        Engine le; le.prepare (FS, 256);
        const int L = le.latencySamples();
        const double samples = lag / (2 * PI * f / FS);
        std::printf ("  latency %.2f samples (reported %d: decimator + 1.5 ms look-ahead)\n", samples, L);
        CHECK (std::abs (samples - L) < 0.6, "the latency is %.2f samples, not %d", samples, L);
    }

    // ---- 5. rolls: overlapping hits sum, stealing fades, nothing clicks --------------------
    {
        //  the noise in a hit is seeded by its place in the sequence, so a hit
        //  played second is not the same noise as the same hit played alone;
        //  the triangle-inequality test runs on the head, which is deterministic
        Params h = presetParams (0); h.stick = 0.0f; h.wires = 0.0f; h.clap = 0.0f;
        Params p = presetParams (0); p.stick = 0.0f;
        auto maxStep = [] (const std::vector<float>& x, int a, int b) { float m = 0; for (int i = a + 1; i < b; ++i) m = std::max (m, std::abs (x[(size_t) i] - x[(size_t) i - 1])); return m; };
        //  two hits: the sum can step no more than the two apart
        const auto two  = render (h, FS, 0.6, { { 0.0, 38, 1.0f }, { 0.2, 38, 1.0f } });
        const auto solo = render (h, FS, 0.6, { { 0.2, 38, 1.0f } });
        const auto old  = render (h, FS, 0.6, { { 0.0, 38, 1.0f } });
        const int at = 9600;
        const float s2 = maxStep (two, at - 50, at + 480);
        const float bound = maxStep (solo, at - 50, at + 480) + maxStep (old, at - 50, at + 480);
        std::printf ("  a second hit mid-decay: largest step %.4f against %.4f for the two apart\n", s2, bound);
        CHECK (s2 <= bound * 1.05f, "a second hit clicks (%.4f > %.4f)", s2, bound);

        //  a 32nd-note roll at 170 BPM: eight hits, 44 ms apart, so the fifth
        //  steals. It must stay bounded and step no more than a hit's own attack.
        std::vector<Hit> roll;
        for (int i = 0; i < 8; ++i) roll.push_back ({ i * 60.0 / 170.0 / 8.0, 38, 0.9f });
        const auto r = render (p, FS, 0.8, roll);
        const float stepRoll = maxStep (r, 0, (int) r.size());
        const float stepOne  = maxStep (one (p, FS, 0.3, 38, 0.9f), 0, 14400);
        std::printf ("  a 32nd-note roll at 170 BPM: peak %.3f, largest step %.4f against %.4f for one hit\n",
                     peakOf (r), stepRoll, stepOne);
        CHECK (finite (r) && peakOf (r) <= 1.0f, "a roll is not bounded");
        CHECK (stepRoll <= stepOne * 3.0f, "a roll steps %.4f, one hit %.4f", stepRoll, stepOne);
    }

    // ---- 6. the head: SKIN, STRIKE, RING, WAVE -------------------------------------------------
    {
        //  SKIN: the pair's 1.83 partial gives way to the membrane's (0,2) at 2.295
        //  (glide frozen, so each partial sits exactly where the table says)
        Engine::benchSteady = true;
        Params a = head (180, 800); a.skin = 0.0f; a.strike = 0.0f;
        Params b = a; b.skin = 1.0f; b.ring = 0.6f;
        const auto xa = one (a), xb = one (b);
        const double pairA = bandPeakDb (xa, 480, 4800, 180 * 1.83, 20), pairB = bandPeakDb (xb, 480, 4800, 180 * 1.83, 20);
        const double m02A = bandPeakDb (xa, 480, 4800, 180 * 2.2954, 20), m02B = bandPeakDb (xb, 480, 4800, 180 * 2.2954, 20);
        std::printf ("  SKIN 0 -> 100: the pair's 1.83 partial %.1f -> %.1f dB, the (0,2) mode %.1f -> %.1f dB\n", pairA, pairB, m02A, m02B);
        CHECK (pairA - pairB > 15.0 && m02B - m02A > 15.0, "SKIN does not turn the pair into a membrane");

        //  STRIKE: a centre hit cannot excite the (1,1) mode; an edge hit does
        Params c = head (180, 800); c.skin = 1.0f; c.ring = 0.6f; c.strike = 0.0f;
        Params e = c; e.strike = 0.7f;
        const double m11c = bandPeakDb (one (c), 480, 4800, 180 * 1.5933, 20), m11e = bandPeakDb (one (e), 480, 4800, 180 * 1.5933, 20);
        std::printf ("  STRIKE centre -> edge: the (1,1) mode %.1f -> %.1f dB\n", m11c, m11e);
        CHECK (m11e - m11c > 20.0, "STRIKE does not bring in the off-centre modes");

        //  RING: an undamped head keeps its overtones
        Params r0 = e; r0.ring = 0.0f;
        Params r1 = e; r1.ring = 1.0f;
        const double o0 = bandPeakDb (one (r0), 7200, 12000, 180 * 1.5933, 20), o1 = bandPeakDb (one (r1), 7200, 12000, 180 * 1.5933, 20);
        Engine::benchSteady = false;
        std::printf ("  RING 0 -> 100: the (1,1) mode at 150-250 ms %.1f -> %.1f dB\n", o0, o1);
        CHECK (o1 - o0 > 20.0, "RING does not keep the overtone ringing");

        //  WAVE: the pair's harmonics grow sine -> triangle -> square
        double prev = -300; bool mono = true; double h3[3]; int k = 0;
        for (float w : { 0.0f, 0.5f, 1.0f })
        {
            Params p = head (150, 2000); p.skin = 0.0f; p.wave = w;
            const auto x = one (p, FS, 0.5);
            h3[k] = ampDb (x, 4800, 19200, 450) - ampDb (x, 4800, 19200, 150);
            mono = mono && h3[k] > prev; prev = h3[k]; ++k;
        }
        std::printf ("  WAVE 0/50/100: 3rd harmonic %.1f / %.1f / %.1f dB\n", h3[0], h3[1], h3[2]);
        CHECK (mono && h3[0] < -80.0 && h3[1] > -25.0, "WAVE does not add harmonics as it should");

        //  BODY: 0 is no head at all
        Params nb = presetParams (0); nb.body = 0.0f; nb.wires = 0.0f; nb.stick = 0.0f;
        CHECK (peakOf (one (nb)) == 0.0f, "BODY 0 still sounds");
    }

    // ---- 7. the wires ---------------------------------------------------------------------------
    {
        Params p = presetParams (0); p.stick = 0.0f;
        Params q = p; q.wires = 0.0f;
        const double w1 = bandDb (one (p), 480, 4800, 4000, 10000), w0 = bandDb (one (q), 480, 4800, 4000, 10000);
        std::printf ("  WIRES 0 -> 60: 4-10 kHz %+.1f dB\n", w1 - w0);
        CHECK (w1 - w0 > 20.0, "WIRES adds no snare");

        const auto loose = one (wiresOnly (0.0f, 400), FS, 1.2), tight = one (wiresOnly (1.0f, 400), FS, 1.2);
        const double cl = centroid (loose, 480, 4800), ct = centroid (tight, 480, 4800);
        std::printf ("  TENSION loose -> tight: centroid %.0f -> %.0f Hz\n", cl, ct);
        CHECK (ct > cl * 1.5, "TENSION does not tighten the wires");

        //  loose wires slap the head once a cycle: their noise is chopped at its pitch
        const double bl = buzzDb (loose, 960, 9600, 185), bt = buzzDb (tight, 960, 9600, 185);
        std::printf ("  the buzz at the head's pitch: loose %.1f dB, tight %.1f dB of the wires' mean level\n", bl, bt);
        CHECK (bl - bt > 12.0, "loose wires do not buzz at the head's pitch");

        //  and they lag the stick, more when loose
        const int ol = onsetOf (loose, 0.1f), ot = onsetOf (tight, 0.1f);
        std::printf ("  the wires lag the stick: loose %.2f ms, tight %.2f ms (after the latency)\n",
                     (ol - 95) / 48.0, (ot - 95) / 48.0);
        CHECK (ol - ot > 40, "loose wires do not lag the tight ones");

        //  loose wires keep singing along with a long-ringing head
        Params sl = wiresOnly (0.0f, 150); sl.decay = 1500;
        Params stt = wiresOnly (1.0f, 150); stt.decay = 1500;
        const double sy0 = rmsDb (one (sl, FS, 1.0), 14400, 19200), sy1 = rmsDb (one (stt, FS, 1.0), 14400, 19200);
        std::printf ("  300-400 ms, SIZZLE 150 ms, a 1.5 s head: loose wires %.1f dB, tight %.1f dB\n", sy0, sy1);
        CHECK (sy0 - sy1 > 30.0, "loose wires do not ring with the head");

        Params a0 = wiresOnly (0.6f, 300); a0.air = 0.0f;
        Params a1 = a0; a1.air = 1.0f;
        //  the wires are levelled by their bandwidth, so AIR is a TILT: the top
        //  against the body of the noise
        const auto xa0 = one (a0), xa1 = one (a1);
        const double h0 = bandDb (xa0, 480, 4800, 9000, 16000) - bandDb (xa0, 480, 4800, 1500, 3000);
        const double h1 = bandDb (xa1, 480, 4800, 9000, 16000) - bandDb (xa1, 480, 4800, 1500, 3000);
        std::printf ("  AIR 0 -> 100: 9-16 kHz against 1.5-3 kHz %+.1f dB\n", h1 - h0);
        CHECK (h1 - h0 > 15.0, "AIR does not open the top");

        //  a ghost note is mostly wires: the balance tips toward them as velocity falls
        Params g = presetParams (0); g.velo = 1.0f; g.stick = 0.0f;
        auto ratio = [&] (float vel)
        {
            const auto x = one (g, FS, 0.5, 38, vel);
            return bandDb (x, 480, 4800, 4000, 12000) - bandDb (x, 480, 4800, 100, 500);
        };
        const double rg = ratio (0.25f), rh = ratio (1.0f);
        std::printf ("  wires over head: ghost note %+.1f dB, full hit %+.1f dB\n", rg, rh);
        CHECK (rg - rh > 3.0, "a ghost note is not wirier than a full hit");
    }

    // ---- 8. STICK, rim shot, cross-stick ------------------------------------------------------
    {
        Params a = presetParams (0); a.stick = 0.0f; a.wires = 0.0f; a.strike = 0.0f;
        Params b = a; b.stick = 1.0f;
        const double sa = bandDb (one (a), 95, 96, 3000, 16000), sb = bandDb (one (b), 95, 96, 3000, 16000);
        std::printf ("  STICK 0 -> 100: 3-16 kHz in the first 2 ms %+.1f dB\n", sb - sa);
        CHECK (sb - sa > 15.0, "STICK adds no click");

        const auto centre = art (a, ART_SNARE), rim = art (a, ART_RIM);
        const double shC = bandPeakDb (centre, 100, 1600, 520 * std::pow (185 / 185.0, 0.3));
        const double shR = bandPeakDb (rim, 100, 1600, 520);
        std::printf ("  a rim shot: the shell's 520 Hz mode %.1f -> %.1f dB, peak %+.1f dB over a centre hit\n",
                     shC, shR, 20 * std::log10 (peakOf (rim) / peakOf (centre)));
        CHECK (shR - shC > 15.0 && peakOf (rim) > peakOf (centre), "a rim shot is not a rim shot");

        Params s = presetParams (0);
        const auto snare = art (s, ART_SNARE), xs = art (s, ART_XSTICK);
        const double ws = bandDb (snare, 960, 4800, 5000, 12000), wx = bandDb (xs, 960, 4800, 5000, 12000);
        const double tailS = rmsDb (snare, 4800, 9600), tailX = rmsDb (xs, 4800, 9600);
        std::printf ("  a cross-stick: wires %+.1f dB, 100-200 ms %+.1f dB against the snare\n", wx - ws, tailX - tailS);
        CHECK (wx - ws < -15.0 && tailX - tailS < -15.0, "a cross-stick is still a snare");
    }

    // ---- 9. CLAP ----------------------------------------------------------------------------------
    {
        auto bursts = [&] (float spread)
        {
            Params p = presetParams (0); p.spread = spread;
            const auto x = art (p, ART_CLAP, 0.3);
            //  a burst is where the 1 ms rms envelope has risen 4x over its value
            //  1.5 ms before, somewhere near the top of the clap, 3 ms after the last
            std::vector<double> env;
            for (int i = 95; i < 95 + (int) (0.05 * FS); i += 12)
                env.push_back (std::sqrt (std::pow (10.0, rmsDb (x, i - 24, i + 24) / 10.0)));
            const double top = *std::max_element (env.begin(), env.end());
            int n = 0, lastAt = -1000;
            for (int k = 0; k < (int) env.size(); ++k)
            {
                const double before = k >= 6 ? env[(size_t) k - 6] : 0.0;
                if (env[(size_t) k] > 4.0 * before && env[(size_t) k] > 0.1 * top && k - lastAt > 12) { ++n; lastAt = k; }
            }
            return n;
        };
        const int b0 = bursts (0.0f), b1 = bursts (0.6f);
        std::printf ("  a clap: SPREAD 0 %d burst, SPREAD 60 %d bursts in the first 50 ms\n", b0, b1);
        CHECK (b0 == 1 && b1 == 4, "the clap's bursts are wrong (%d, %d)", b0, b1);

        Params p = presetParams (0);
        const auto c = art (p, ART_CLAP), s = art (p, ART_SNARE);
        const double hc = ampDb (c, 480, 4800, 185), hs = ampDb (s, 480, 4800, 185);
        std::printf ("  a clap has no head: %.1f dB at TUNE against %.1f for the snare\n", hc, hs);
        CHECK (hs - hc > 20.0, "the clap articulation carries the head");

        Params cl = presetParams (0); cl.stick = 0.0f;
        Params cc = cl; cc.clap = 1.0f;
        const double c0 = bandDb (one (cl), 480, 2400, 900, 1600), c1 = bandDb (one (cc), 480, 2400, 900, 1600);
        std::printf ("  CLAP 0 -> 100 on a snare: 0.9-1.6 kHz %+.1f dB\n", c1 - c0);
        CHECK (c1 - c0 > 6.0, "CLAP layers nothing");
    }

    // ---- 10. TRANSIENT -------------------------------------------------------------------------
    {
        auto ratio = [&] (float att, float sus, double& early, double& tail)
        {
            Params p = presetParams (0); p.attack = att; p.sustain = sus; p.decay = 400; p.sizzle = 400;
            const auto x = one (p, FS, 0.8);
            early = rmsDb (x, 95, 95 + 240); tail = rmsDb (x, 7200, 14400);
        };
        double e0, t0, eA, tA, eC, tC, eS, tS, eD, tD;
        ratio (0, 0, e0, t0); ratio (1, 0, eA, tA); ratio (-1, 0, eC, tC); ratio (0, 1, eS, tS); ratio (0, -1, eD, tD);
        std::printf ("  ATTACK +/-: first 5 ms %+.1f / %+.1f dB;  SUSTAIN +/-: 150-300 ms %+.1f / %+.1f dB\n",
                     eA - e0, eC - e0, tS - t0, tD - t0);
        CHECK (eA - e0 > 2.0 && eC - e0 < -2.0, "ATTACK does not move the attack");
        CHECK (tS - t0 > 2.0 && tD - t0 < -4.0, "SUSTAIN does not move the tail");
        CHECK (std::abs (tA - t0) < 1.5, "ATTACK leaks into the tail (%.1f dB)", tA - t0);
    }

    // ---- 11. ROOM and GATE ------------------------------------------------------------------------
    {
        Params a = presetParams (0); a.decay = 150; a.sizzle = 150;
        Params b = a; b.room = 0.7f;
        const auto xa = one (a, FS, 1.5), xb = one (b, FS, 1.5);
        const double ta = rmsDb (xa, 14400, 19200), tb = rmsDb (xb, 14400, 19200);
        std::printf ("  ROOM 0 -> 70: 300-400 ms %.1f -> %.1f dB\n", ta, tb);
        CHECK (tb - ta > 25.0, "ROOM adds no tail");
        CHECK (peakOf (xb) <= 1.0f && finite (xb), "ROOM is not bounded");

        //  the gate: the room is untouched until the gate time, then gone
        Params g = b; g.gate = 150;
        const auto xg = one (g, FS, 1.5);
        const int lat = 95;
        const double before = rmsDb (xg, lat, lat + (int) (0.13 * FS)) - rmsDb (xb, lat, lat + (int) (0.13 * FS));
        const double after  = rmsDb (xg, lat + (int) (0.175 * FS), lat + (int) (0.5 * FS));
        const double openAt = rmsDb (xb, lat + (int) (0.175 * FS), lat + (int) (0.5 * FS));
        std::printf ("  GATE 150 ms: before it %+.2f dB against the open room; 175-500 ms %.1f dB (open %.1f)\n",
                     before, after, openAt);
        CHECK (std::abs (before) < 0.5, "the gate touches the hit before it closes (%.2f dB)", before);
        CHECK (after < -100.0 && openAt > -60.0, "the gate does not cut the room");

        //  and it opens again on the next hit
        const auto x2 = render (g, FS, 1.5, { { 0.0, 38, 1.0f }, { 0.6, 38, 1.0f } });
        const double second = rmsDb (x2, lat + (int) (0.6 * FS), lat + (int) (0.7 * FS));
        std::printf ("  the gate reopens for the next hit: %.1f dB\n", second);
        CHECK (second > -40.0, "the gate stays shut");
    }

    // ---- 12. ECHO --------------------------------------------------------------------------------
    {
        Params p = presetParams (0); p.echo = 0.6f; p.time = 3.0f;     // a dotted eighth
        p.decay = 100; p.sizzle = 100;
        const auto s = renderSt (p, FS, 2.0, { { 0.0, 38, 1.0f } }, 256, 120.0);
        //  at 120 BPM a dotted eighth is 375 ms: first repeat LEFT, second RIGHT
        auto energyAt = [&] (const std::vector<float>& x, double t) { return rmsDb (x, 95 + (int) (t * FS) - 480, 95 + (int) (t * FS) + 2400); };
        const double l1 = energyAt (s.L, 0.375), r1 = energyAt (s.R, 0.375), l2 = energyAt (s.L, 0.75), r2 = energyAt (s.R, 0.75);
        std::printf ("  ECHO 1/8D at 120 BPM: at 375 ms L %.1f R %.1f dB; at 750 ms L %.1f R %.1f dB\n", l1, r1, l2, r2);
        CHECK (l1 - r1 > 30.0 && r2 - l2 > 30.0, "the echo does not ping-pong");

        //  the repeat lands where the tempo says, found by cross-correlation
        auto lagOf = [&] (const std::vector<float>& a, const std::vector<float>& b, int lo, int hi)
        {
            double best = -1; int bl = 0;
            for (int L = lo; L <= hi; ++L)
            {
                double c = 0; for (int i = 95; i < 95 + 4800; ++i) c += (double) a[(size_t) i] * b[(size_t) (i + L)];
                if (c > best) { best = c; bl = L; }
            }
            return bl;
        };
        const int lg = lagOf (s.L, s.L, (int) (0.33 * FS), (int) (0.42 * FS));
        const auto s90 = renderSt (p, FS, 2.0, { { 0.0, 38, 1.0f } }, 256, 90.0);
        const int lg90 = lagOf (s90.L, s90.L, (int) (0.45 * FS), (int) (0.55 * FS));
        std::printf ("  first repeat at %.2f ms (375 wanted); at 90 BPM %.2f ms (500 wanted)\n", lg / 48.0, lg90 / 48.0);
        CHECK (std::abs (lg / 48.0 - 375.0) < 1.5 && std::abs (lg90 / 48.0 - 500.0) < 1.5, "the echo time is not the tempo's");

        //  at full ECHO, repeats die away rather than build up
        Params f = p; f.echo = 1.0f;
        const auto sf = renderSt (f, FS, 20.0, { { 0.0, 38, 1.0f } });
        const double early = rmsDb (sf.L, 48000, 4 * 48000), late = rmsDb (sf.L, 17 * 48000, 20 * 48000);
        std::printf ("  ECHO 100 %%: 1-4 s %.1f dB, 17-20 s %.1f dB\n", early, late);
        CHECK (late < early - 30.0 && finite (sf.L) && peakOf (sf.L) <= 1.0f, "full echo runs away");
    }

    // ---- 13. DRIVE ---------------------------------------------------------------------------------
    {
        Params c = head (200, 400); c.skin = 0.3f;
        const auto clean = one (c);
        const double lc = rmsDb (clean, 0, 14400);
        double worst = 0;
        for (int e = 0; e < NUM_DRIVE_ENGINES; ++e)
            for (float d : { 0.4f, 1.0f })
            {
                Params p = c; p.drive = d; p.engine = (float) e;
                const auto x = one (p);
                const double l = rmsDb (x, 0, 14400);
                const double h = ampDb (x, 1200, 4800, 600) - ampDb (clean, 1200, 4800, 600);
                worst = std::max (worst, std::abs (l - lc));
                std::printf ("    %-11s drive %3.0f%%: level %+5.1f dB, 3rd harmonic %+6.1f dB over clean\n",
                             driveEngineName (e), d * 100, l - lc, h);
                CHECK (h > 10.0 || e == ENG_HYPER, "%s at %.0f %% adds no harmonics", driveEngineName (e), d * 100);
                CHECK (finite (x) && peakOf (x) <= 1.0f, "%s is not bounded", driveEngineName (e));
            }
        std::printf ("  drive level match, worst %.1f dB across engines and settings\n", worst);
        CHECK (worst < 4.5, "a drive engine changes the level by %.1f dB", worst);

        //  aliasing: a steady membrane tone through RAZOR WING at 100 %
        Engine::benchSteady = true;
        Params a = head (200, 2000); a.skin = 1.0f; a.ring = 0.0f; a.drive = 1.0f; a.engine = ENG_RAZOR;
        const double al = inharmonicDb (one (a, FS, 1.0), (int) (0.35 * FS), 200, FS);
        Params a4 = a; a4.tune = 420;
        const double al4 = inharmonicDb (one (a4, FS, 1.0), (int) (0.35 * FS), 420, FS);
        Engine::benchSteady = false;
        std::printf ("  aliasing, RAZOR WING at 100 %%: %.1f dB at 200 Hz, %.1f dB at 420 Hz\n", al, al4);
        CHECK (al < -70.0 && al4 < -60.0, "aliasing %.1f / %.1f dB", al, al4);
    }

    // ---- 14. GRIT, COLOUR, COMP, VELOCITY ------------------------------------------------------------
    {
        Engine::benchSteady = true;
        Params g0 = head (200, 2000); g0.skin = 1.0f; g0.ring = 0.0f;
        const double ia = inharmonicDb (one (g0), (int) (0.35 * FS), 200, FS);
        Params gs = g0; gs.grit = 0.55f;
        const double ig = inharmonicDb (one (gs), (int) (0.35 * FS), 200, FS);
        Engine::benchSteady = false;
        std::printf ("  GRIT 55 %%: off-harmonic energy %.1f dB (clean %.1f)\n", ig, ia);
        CHECK (ig > ia + 20.0, "GRIT does not degrade");

        Params a = presetParams (0);
        Params col = a; col.colour = 0.3f;
        const auto xa = one (a), xc = one (col);
        const double hfa = bandDb (xa, 95, 960, 6000, 12000), hfc = bandDb (xc, 95, 960, 6000, 12000);
        std::printf ("  COLOUR 30 %%: 6-12 kHz %+.1f dB\n", hfc - hfa);
        CHECK (hfc - hfa < -6.0, "COLOUR does not darken");

        Params cp = a; cp.comp = 0.7f; cp.speed = 0.8f; cp.decay = 400; cp.sizzle = 400;
        Params cn = cp; cn.comp = 0.0f;
        const auto xn = one (cn), xp = one (cp);
        const double spanA = rmsDb (xn, 95, 1055) - rmsDb (xn, 4800, 9600);
        const double spanP = rmsDb (xp, 95, 1055) - rmsDb (xp, 4800, 9600);
        std::printf ("  COMP 70 %%: head-to-tail span %.1f -> %.1f dB\n", spanA, spanP);
        CHECK (spanA - spanP > 6.0, "COMP does not compress (%.1f -> %.1f)", spanA, spanP);

        Params v = a; v.velo = 1.0f;
        const double loud = rmsDb (one (v, FS, 1.0, 38, 1.0f), 0, 14400);
        const double soft = rmsDb (one (v, FS, 1.0, 38, 0.3f), 0, 14400);
        v.velo = 0.0f;
        const auto s0 = one (v, FS, 1.0, 38, 0.3f), s1 = one (v, FS, 1.0, 38, 1.0f);
        std::printf ("  VELOCITY 100 %%: soft hit %.1f dB under a hard one; at 0 %% identical\n", loud - soft);
        CHECK (loud - soft > 8.0, "VELOCITY does nothing");
        CHECK (std::memcmp (s0.data(), s1.data(), s0.size() * sizeof (float)) == 0, "VELOCITY 0 still responds to velocity");
    }

    // ---- 15. every preset, and 300 random snares, bounded and audible -------------------------------
    {
        float worstPk = 0; double quietest = 0; bool fin = true;
        for (int i = 0; i < numPresets(); ++i)
        {
            const auto s = renderSt (presetParams (i), FS, 1.0, { { 0.0, 38, 1.0f } });
            worstPk = std::max ({ worstPk, peakOf (s.L), peakOf (s.R) });
            quietest = std::min (quietest, rmsDb (s.L, 0, 7200));
            fin = fin && finite (s.L) && finite (s.R);
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
            const auto s = renderSt (p, FS, 0.6, { { 0.0, 36 + (int) (rnd() * 24), rnd() }, { 0.3, 38, 1.0f },
                                                   { 0.31, 40, 0.7f }, { 0.32, 39, 0.9f } });
            worstPk = std::max ({ worstPk, peakOf (s.L), peakOf (s.R) });
            fin = fin && finite (s.L) && finite (s.R);
        }
        std::printf ("  %d presets + 300 random snares: worst peak %.4f, quietest preset %.1f dB\n",
                     numPresets(), worstPk, quietest);
        CHECK (fin, "a non-finite sample");
        CHECK (worstPk <= 1.0f, "a snare left the ceiling (%.4f)", worstPk);
        CHECK (quietest > -30.0, "a preset is nearly silent (%.1f dB)", quietest);
    }

    // ---- 16. cost ----------------------------------------------------------------------------------
    {
        Params p = presetParams (presetByName ("Big Eighties"));
        p.drive = 0.6f; p.grit = 0.2f; p.attack = 0.5f; p.clap = 0.6f; p.echo = 0.5f; p.skin = 1.0f; p.ring = 1.0f;
        std::vector<Hit> hits;
        for (int i = 0; i < 80; ++i) hits.push_back ({ i * 0.125, 38, 1.0f });
        const auto t0 = std::chrono::steady_clock::now();
        const auto s = renderSt (p, FS, 10.0, hits);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  cost, everything on, a hit every 125 ms: %.2f %% of one core\n", 100.0 * sec / 10.0);
        CHECK (finite (s.L), "cost run non-finite");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
