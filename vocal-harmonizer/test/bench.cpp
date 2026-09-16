// LEGION offline bench. Plain C++17, no JUCE, no audio device — the same
// house rule every Brokild engine is held to: every claim the design doc
// makes is something this file measures against the real engine.
//
// What it proves, in the order the design doc claims it:
//   * the transform round-trips, and the window is COLA at 75 % overlap;
//   * all voices off = the harmony bus is EXACTLY zero and the dry bus is
//     bit-identical to the input, delayed by the reported latency;
//   * a voice at unity (0 st, 0 formant) reproduces the input to better
//     than -40 dB — the transparency floor everything else is measured
//     against;
//   * PITCH moves f0 and leaves the formants where they were;
//   * FORMANT moves the formants and leaves f0 where it was;
//   * the two are INDEPENDENT: neither knob measurably disturbs the other;
//   * FOLLOW 1 collapses to plain resampling, on purpose;
//   * screams (noisy, inharmonic, clipped) stay bounded and finite;
//   * output is deterministic, and the latency is what it says it is.
//
// Prints ALL CLEAR, or lists what failed.

#include "../engine/legion_harmonizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
 #include <xmmintrin.h>
#endif

using namespace legion;

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{

constexpr double kFs = 48000.0;

// ---------------------------------------------------------------------------
// signals

uint32_t rngState = 0x1234567u;
float rnd01() { rngState = 1664525u * rngState + 1013904223u; return (float) (rngState / 4294967296.0); }
float rndPm() { return rnd01() * 2.0f - 1.0f; }

struct Reson
{
    double a1 = 0, a2 = 0, y1 = 0, y2 = 0;
    void set (double f, double bw, double fs)
    {
        const double r = std::exp (-M_PI * bw / fs);
        a1 = 2.0 * r * std::cos (2.0 * M_PI * f / fs);
        a2 = -r * r;
        y1 = y2 = 0;
    }
    double tick (double x)
    {
        const double y = x + a1 * y1 + a2 * y2;
        y2 = y1; y1 = y;
        return y;
    }
};

// A vowel: a glottal impulse train through three formants. Not a toy — it is
// the signal every claim about formants has to be measured on, because it is
// the only one whose formants are known exactly.
void makeVowel (std::vector<float>& out, int n, double fs, double f0,
                double F1, double F2, double F3, float amp = 0.5f)
{
    out.assign ((size_t) n, 0.0f);
    Reson r1, r2, r3;
    r1.set (F1, 90.0, fs); r2.set (F2, 110.0, fs); r3.set (F3, 170.0, fs);
    double phase = 0.0;
    double peak = 0.0;
    std::vector<double> tmp ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        phase += f0 / fs;
        double x = 0.0;
        if (phase >= 1.0) { phase -= 1.0; x = 1.0; }
        const double y = r3.tick (r2.tick (r1.tick (x)));
        tmp[(size_t) i] = y;
        peak = std::max (peak, std::abs (y));
    }
    const double g = peak > 0 ? (double) amp / peak : 0.0;
    for (int i = 0; i < n; ++i) out[(size_t) i] = (float) (tmp[(size_t) i] * g);
}

// A scream: a vowel gone rough — jittered period, subharmonic, broadband
// noise, and hard clipping. Nothing here is periodic enough for a pitch
// tracker to like, which is exactly why it is in the bench.
void makeScream (std::vector<float>& out, int n, double fs)
{
    std::vector<float> v;
    makeVowel (v, n, fs, 210.0, 800.0, 1400.0, 2900.0, 0.9f);
    out.assign ((size_t) n, 0.0f);
    double sub = 0.0;
    for (int i = 0; i < n; ++i)
    {
        sub += 2.0 * M_PI * 105.0 / fs;
        float x = v[(size_t) i] + 0.35f * (float) std::sin (sub) + 0.25f * rndPm();
        x = std::tanh (4.0f * x);
        out[(size_t) i] = x;
    }
}

// ---------------------------------------------------------------------------
// measurement

struct Stats { float peak = 0; float rms = 0; bool finite = true; };

Stats measure (const float* x, int n)
{
    Stats s;
    double acc = 0;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (x[i])) s.finite = false;
        s.peak = std::max (s.peak, std::abs (x[i]));
        acc += (double) x[i] * x[i];
    }
    s.rms = (float) std::sqrt (acc / std::max (1, n));
    return s;
}

// f0 by normalised autocorrelation — independent of anything the engine
// does. The octave rule is the McLeod one: take the LOWEST lag whose peak
// reaches 90 % of the best, so a signal that correlates just as well an
// octave down is not reported an octave down.
double measureF0 (const float* x, int n, double fs, double lo = 50.0, double hi = 1200.0)
{
    const int minLag = (int) (fs / hi), maxLag = std::min (n / 2, (int) (fs / lo));
    if (maxLag <= minLag + 2) return 0.0;
    double e0 = 0.0;
    for (int i = 0; i < n / 2; ++i) e0 += (double) x[i] * x[i];
    if (e0 < 1e-12) return 0.0;

    std::vector<double> r ((size_t) (maxLag + 1), 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double num = 0.0, den = 0.0;
        for (int i = 0; i < n / 2; ++i)
        {
            num += (double) x[i] * x[i + lag];
            den += (double) x[i + lag] * x[i + lag];
        }
        r[(size_t) lag] = num / std::sqrt (e0 * den + 1e-20);
    }
    double best = 0.0;
    for (int lag = minLag; lag <= maxLag; ++lag) best = std::max (best, r[(size_t) lag]);
    if (best <= 0.0) return 0.0;

    int chosen = 0;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (r[(size_t) lag] >= 0.90 * best
            && r[(size_t) lag] > r[(size_t) (lag - 1)] && r[(size_t) lag] >= r[(size_t) (lag + 1)])
        { chosen = lag; break; }
    if (chosen == 0) return 0.0;

    const double ym = r[(size_t) (chosen - 1)], y0 = r[(size_t) chosen], yp = r[(size_t) (chosen + 1)];
    const double den = ym - 2 * y0 + yp;
    const double d = std::abs (den) > 1e-12 ? 0.5 * (ym - yp) / den : 0.0;
    return fs / ((double) chosen + std::max (-0.5, std::min (0.5, d)));
}

// The averaged log spectrum — the raw material for every formant claim,
// measured independently of anything the engine does.
struct Spectrum
{
    std::vector<float> db;      // one entry per bin, 0..nfft/2
    double binHz = 0.0;
    int half = 0;
};

Spectrum measureSpectrum (const float* x, int n, double fs)
{
    const int nfft = 8192;
    Spectrum e;
    e.half = nfft / 2;
    e.binHz = fs / nfft;
    e.db.assign ((size_t) (e.half + 1), -200.0f);
    if (n < nfft) return e;

    Fft fft; fft.prepare (nfft);

    //  average the log spectrum over several windows: one frame of a rough
    //  voice is not a measurement, it is an anecdote
    std::vector<double> acc ((size_t) (e.half + 1), 0.0);
    std::vector<float> buf ((size_t) (2 * nfft));
    int frames = 0;
    for (int start = 0; start + nfft <= n; start += nfft / 2)
    {
        for (int i = 0; i < nfft; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / nfft);
            buf[(size_t) (2 * i)] = (float) (x[start + i] * w);
            buf[(size_t) (2 * i + 1)] = 0.0f;
        }
        fft.forward (buf.data(), nfft);
        for (int k = 0; k <= e.half; ++k)
        {
            const double re = buf[(size_t) (2 * k)], im = buf[(size_t) (2 * k + 1)];
            acc[(size_t) k] += std::log (std::sqrt (re * re + im * im) + 1e-12);
        }
        ++frames;
    }
    if (frames == 0) return e;
    for (int k = 0; k <= e.half; ++k) e.db[(size_t) k] = (float) (8.685889638 * acc[(size_t) k] / frames);
    return e;
}

/*  HOW FAR DID THE FORMANTS MOVE?

    Not "where are the three peaks" — peak picking on a real envelope is a
    coin toss, and a formant that merges with its neighbour has no peak at
    all. Not a cepstral lifter either: on a 240 Hz voice the lifter has to
    be so short that two different voices smooth into the same two gentle
    humps, and the meter starts reading -9 st where the spectra differ by
    almost nothing (measured, while building this file — the meter was wrong,
    not the engine).

    What is left is the only thing actually known about the tract: its value
    AT EACH HARMONIC, which is where the voice sampled it. So: read the level
    at every harmonic of a pitch the caller already knows, lay those samples
    on a LOG-frequency grid where a formant shift is a pure translation, and
    find the translation that lines the two up. One number, in semitones, and
    it is exactly the claim the design doc makes. */
constexpr int kGridPts = 320;

struct Grid { std::vector<double> v; double lo = 0, hi = 0; };

double gridStepOctaves (const Grid& g) { return std::log2 (g.hi / g.lo) / (kGridPts - 1); }

Grid harmonicGrid (const Spectrum& s, double f0, double lo, double hi)
{
    //  level at each harmonic: the local max within +-2 bins, so window
    //  leakage and bin rounding cannot read a peak as a valley
    std::vector<std::pair<double,double>> pts;
    for (int h = 1; h * f0 < 0.9 * s.half * s.binHz; ++h)
    {
        const double f = h * f0;
        const int b = (int) (f / s.binHz + 0.5);
        if (b < 2 || b > s.half - 2) continue;
        float m = -200.0f;
        for (int j = b - 2; j <= b + 2; ++j) m = std::max (m, s.db[(size_t) j]);
        pts.push_back ({ std::log2 (f), (double) m });
    }

    Grid g; g.lo = lo; g.hi = hi;
    g.v.assign ((size_t) kGridPts, 0.0);
    if (pts.empty()) return g;

    const double step = std::log2 (hi / lo) / (kGridPts - 1);
    size_t j = 0;
    for (int i = 0; i < kGridPts; ++i)
    {
        const double lf = std::log2 (lo) + i * step;
        while (j + 2 < pts.size() && pts[j + 1].first < lf) ++j;
        if (lf <= pts.front().first)      g.v[(size_t) i] = pts.front().second;
        else if (lf >= pts.back().first)  g.v[(size_t) i] = pts.back().second;
        else
        {
            const double t = (lf - pts[j].first) / (pts[j + 1].first - pts[j].first);
            g.v[(size_t) i] = pts[j].second + t * (pts[j + 1].second - pts[j].second);
        }
    }
    return g;
}

// Positive = b's formants sit ABOVE a's, in semitones.
double gridShift (const Grid& a, const Grid& b)
{
    const double step = gridStepOctaves (a);
    const int D = (int) (1.3 / step);
    double best = -2.0; int bestD = 0;
    std::vector<double> score ((size_t) (2 * D + 1), -2.0);
    for (int d = -D; d <= D; ++d)
    {
        const int i0 = std::max (0, -d), i1 = std::min (kGridPts, kGridPts - d);
        if (i1 - i0 < kGridPts / 2) continue;
        double ma = 0, mb = 0;
        for (int i = i0; i < i1; ++i) { ma += a.v[(size_t) i]; mb += b.v[(size_t) (i + d)]; }
        ma /= (i1 - i0); mb /= (i1 - i0);
        double num = 0, da = 0, db = 0;
        for (int i = i0; i < i1; ++i)
        {
            const double u = a.v[(size_t) i] - ma, v = b.v[(size_t) (i + d)] - mb;
            num += u * v; da += u * u; db += v * v;
        }
        const double r = num / std::sqrt (da * db + 1e-30);
        score[(size_t) (d + D)] = r;
        if (r > best) { best = r; bestD = d; }
    }
    double refine = 0.0;
    if (bestD > -D && bestD < D)
    {
        const double ym = score[(size_t) (bestD + D - 1)], y0 = score[(size_t) (bestD + D)],
                     yp = score[(size_t) (bestD + D + 1)];
        const double den = ym - 2 * y0 + yp;
        if (ym > -1.9 && yp > -1.9 && std::abs (den) > 1e-12)
            refine = std::max (-0.5, std::min (0.5, 0.5 * (ym - yp) / den));
    }
    return 12.0 * (bestD + refine) * step;
}

double formantMove (const float* a, int na, double f0a,
                    const float* b, int nb, double f0b)
{
    //  the grid starts above BOTH fundamentals: below the first harmonic
    //  there is no measurement, only extrapolation
    const double lo = std::max (250.0, 1.05 * std::max (f0a, f0b));
    const double hi = 5000.0;
    return gridShift (harmonicGrid (measureSpectrum (a, na, kFs), f0a, lo, hi),
                      harmonicGrid (measureSpectrum (b, nb, kFs), f0b, lo, hi));
}

// ---------------------------------------------------------------------------
// running the engine

struct Run
{
    std::vector<float> harmL, harmR, dryL, dryR;
};

Run render (Harmonizer& h, const Params& p, const std::vector<float>& in, int block = 256)
{
    const int n = (int) in.size();
    Run r;
    r.harmL.assign ((size_t) n, 0.0f); r.harmR.assign ((size_t) n, 0.0f);
    r.dryL .assign ((size_t) n, 0.0f); r.dryR .assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        h.process (p, in.data() + i, in.data() + i,  m,
                   r.harmL.data() + i, r.harmR.data() + i,
                   r.dryL.data() + i,  r.dryR.data() + i);
    }
    return r;
}

Params oneVoice (float semis, float formant, float follow = 0.0f)
{
    Params p;
    p.humanize = 0.0f;
    p.v[0].on = true;
    p.v[0].semitones = semis;
    p.v[0].formant = formant;
    p.v[0].follow = follow;
    p.v[0].gainDb = 0.0f;
    p.v[0].pan = 0.0f;
    p.v[0].delayMs = 0.0f;
    return p;
}

double ratioSemis (double s) { return std::pow (2.0, s / 12.0); }

} // namespace

// ===========================================================================
int main()
{
   #if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
    _mm_setcsr (_mm_getcsr() | 0x8040);    // FTZ + DAZ: denormals are not music
   #endif

    std::printf ("LEGION bench — fs %.0f\n\n", kFs);

    // -- 1. the transform ---------------------------------------------------
    {
        Fft fft; fft.prepare (4096);
        for (int n : { 256, 1024, 2048, 4096 })
        {
            std::vector<float> a ((size_t) (2 * n)), b;
            for (int i = 0; i < 2 * n; ++i) a[(size_t) i] = rndPm();
            b = a;
            fft.forward (b.data(), n);
            fft.inverse (b.data(), n);
            float worst = 0.0f;
            for (int i = 0; i < 2 * n; ++i) worst = std::max (worst, std::abs (a[(size_t) i] - b[(size_t) i]));
            CHECK (worst < 2.0e-5f, "FFT %d round trip off by %.2e", n, worst);
        }

        //  a pure tone must land on one bin and nowhere else
        const int n = 2048;
        std::vector<float> s ((size_t) (2 * n), 0.0f);
        for (int i = 0; i < n; ++i) s[(size_t) (2 * i)] = (float) std::cos (2.0 * M_PI * 64.0 * i / n);
        fft.forward (s.data(), n);
        const float m64 = std::hypot (s[128], s[129]);
        float other = 0.0f;
        for (int k = 0; k <= n / 2; ++k)
            if (std::abs (k - 64) > 1) other = std::max (other, std::hypot (s[(size_t) (2 * k)], s[(size_t) (2 * k + 1)]));
        CHECK (m64 > 1000.0f * other, "tone leaks across bins (%.3g vs %.3g)", m64, other);
    }

    // -- 2. the window is COLA at the hop the engine uses -------------------
    {
        const int N = 2048, hop = N / 8;
        std::vector<double> sum ((size_t) (4 * N), 0.0);
        for (int f = 0; f < 24; ++f)
            for (int i = 0; i < N; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / N);
                sum[(size_t) (f * hop + i)] += w * w * (1.0 / 3.0);
            }
        double lo = 1e9, hi = -1e9;
        for (int i = N; i < 2 * N; ++i) { lo = std::min (lo, sum[(size_t) i]); hi = std::max (hi, sum[(size_t) i]); }
        CHECK (std::abs (lo - 1.0) < 1e-9 && std::abs (hi - 1.0) < 1e-9,
               "Hann^2 overlap-add is not unity (%.9f .. %.9f)", lo, hi);
    }

    Harmonizer h;
    h.prepare (kFs, 256);
    const int lat = h.latencySamples();
    std::printf ("  NATURAL window %d, latency %d samples (%.1f ms)\n\n",
                 h.latencySamples() - kTapPad, lat, 1000.0 * lat / kFs);

    const int nLen = (int) (kFs * 2.0);
    const int skip = (int) (kFs * 0.4);     // startup: rings filling, gains gliding

    std::vector<float> vowel;
    makeVowel (vowel, nLen, kFs, 120.0, 700.0, 1220.0, 2600.0);

    // -- 3. THE CONTRACT: nothing on = nothing added ------------------------
    {
        h.reset();
        Params p;                                   // every voice off
        auto r = render (h, p, vowel);
        float worstH = 0.0f, worstD = 0.0f;
        for (int i = lat; i < nLen; ++i)
        {
            worstH = std::max (worstH, std::abs (r.harmL[(size_t) i]) + std::abs (r.harmR[(size_t) i]));
            worstD = std::max (worstD, std::abs (r.dryL[(size_t) i] - vowel[(size_t) (i - lat)]));
        }
        CHECK (worstH == 0.0f, "harmony bus is not silent with every voice off (%.3g)", worstH);
        CHECK (worstD == 0.0f, "dry bus is not bit-identical to the input (%.3g)", worstD);
    }

    // -- 4. silence in, silence out -----------------------------------------
    {
        h.reset();
        std::vector<float> zero ((size_t) nLen, 0.0f);
        auto r = render (h, oneVoice (7.0f, 0.0f), zero);
        auto s = measure (r.harmL.data(), nLen);
        CHECK (s.finite && s.peak == 0.0f, "silence in did not give silence out (%.3g)", s.peak);
    }

    // -- 5. UNITY TRANSPARENCY: the floor everything else is measured against
    {
        h.reset();
        auto r = render (h, oneVoice (0.0f, 0.0f), vowel);
        double num = 0.0, den = 0.0;
        for (int i = skip; i < nLen; ++i)
        {
            //  pan centre is equal power, so the voice arrives at -3 dB
            const double y = (double) r.harmL[(size_t) i] * M_SQRT2;
            const double x = (double) r.dryL[(size_t) i];
            num += (y - x) * (y - x);
            den += x * x;
        }
        const double errDb = 10.0 * std::log10 ((num + 1e-30) / (den + 1e-30));
        std::printf ("  unity error            %7.1f dB\n", errDb);
        CHECK (errDb < -40.0, "unity is not transparent (%.1f dB)", errDb);
    }

    // -- 6. reference measurement of the untouched vowel --------------------
    const double f0In = measureF0 (vowel.data() + skip, 16384, kFs);
    CHECK (std::abs (f0In - 120.0) < 1.0, "the test vowel is not at 120 Hz (%.1f)", f0In);
    std::printf ("  input  f0 %6.1f Hz  (formants 700 / 1220 / 2600 by construction)\n\n", f0In);

    //  the measurement itself, checked against a signal whose formants are
    //  known to have moved by exactly one known amount
    {
        std::vector<float> up;
        makeVowel (up, nLen, kFs, 120.0, 700.0 * 1.5, 1220.0 * 1.5, 2600.0 * 1.5);
        const double got = formantMove (vowel.data() + skip, nLen - skip, 120.0,
                                        up.data() + skip, nLen - skip, 120.0);
        const double want = 12.0 * std::log2 (1.5);
        CHECK (std::abs (got - want) < 0.5,
               "the formant-shift METER is wrong: read %.2f st on a known %.2f", got, want);
    }

    // -- 7. PITCH moves the pitch and leaves the body alone -----------------
    struct PitchCase { float semis; const char* label; };
    for (PitchCase c : { PitchCase { 7.0f, "+7 st" }, PitchCase { 12.0f, "+12 st" },
                         PitchCase { 4.0f, "+4 st" }, PitchCase { -5.0f, "-5 st" },
                         PitchCase { -12.0f, "-12 st" } })
    {
        h.reset();
        auto r = render (h, oneVoice (c.semis, 0.0f), vowel);
        const double want = 120.0 * ratioSemis (c.semis);
        const double got  = measureF0 (r.harmL.data() + skip, 16384, kFs);
        const double moved = formantMove (vowel.data() + skip, nLen - skip, 120.0,
                                          r.harmL.data() + skip, nLen - skip, want);
        auto st = measure (r.harmL.data() + skip, nLen - skip);

        std::printf ("  %-6s  f0 %6.1f Hz (want %6.1f)   formants moved %+5.2f st\n",
                     c.label, got, want, moved);

        CHECK (st.finite, "%s produced a non-finite sample", c.label);
        CHECK (std::abs (got - want) / want < 0.01, "%s landed on %.1f Hz, wanted %.1f", c.label, got, want);
        CHECK (std::abs (moved) < 0.8,
               "%s dragged the formants %+.2f st with it — they must NOT move",
               c.label, moved);
    }
    std::printf ("\n");

    // -- 8. FORMANT moves the body and leaves the pitch alone ---------------
    for (float fSemis : { 6.0f, -6.0f, 3.0f, -10.0f, 12.0f })
    {
        h.reset();
        auto r = render (h, oneVoice (0.0f, fSemis), vowel);
        const double got = measureF0 (r.harmL.data() + skip, 16384, kFs);
        const double moved = formantMove (vowel.data() + skip, nLen - skip, 120.0,
                                          r.harmL.data() + skip, nLen - skip, 120.0);

        std::printf ("  formant %+5.1f st   f0 %6.1f Hz   formants moved %+5.2f st\n",
                     fSemis, got, moved);

        CHECK (std::abs (got - 120.0) / 120.0 < 0.01, "formant %+.1f st moved f0 to %.1f Hz", fSemis, got);
        CHECK (std::abs (moved - fSemis) < 1.0,
               "formant %+.1f st moved the formants %+.2f st", fSemis, moved);
    }
    std::printf ("\n");

    // -- 9. both at once, and FOLLOW = plain resampling ---------------------
    {
        h.reset();
        auto r = render (h, oneVoice (12.0f, -4.0f), vowel);
        const double got = measureF0 (r.harmL.data() + skip, 16384, kFs);
        const double moved = formantMove (vowel.data() + skip, nLen - skip, 120.0,
                                          r.harmL.data() + skip, nLen - skip, 240.0);
        std::printf ("  +12 st / formant -4   f0 %6.1f Hz   formants moved %+5.2f st\n", got, moved);
        CHECK (std::abs (got - 240.0) / 240.0 < 0.01, "+12 st with a formant offset landed on %.1f Hz", got);
        CHECK (std::abs (moved + 4.0) < 1.0, "+12 st / -4 formant moved the formants %+.2f st", moved);
    }
    {
        h.reset();
        auto r = render (h, oneVoice (12.0f, 0.0f, 1.0f), vowel);   // FOLLOW 1
        const double got = measureF0 (r.harmL.data() + skip, 16384, kFs);
        const double moved = formantMove (vowel.data() + skip, nLen - skip, 120.0,
                                          r.harmL.data() + skip, nLen - skip, 240.0);
        std::printf ("  +12 st / follow 1     f0 %6.1f Hz   formants moved %+5.2f st (want +12)\n", got, moved);
        CHECK (std::abs (got - 240.0) / 240.0 < 0.01, "FOLLOW 1 landed on %.1f Hz", got);
        CHECK (std::abs (moved - 12.0) < 1.2, "FOLLOW 1 is not plain resampling (%+.2f st)", moved);
    }
    std::printf ("\n");

    // -- 10. level sanity ----------------------------------------------------
    {
        const auto in = measure (vowel.data() + skip, nLen - skip);
        for (float s : { -12.0f, -5.0f, 0.0f, 7.0f, 12.0f })
        {
            h.reset();
            auto r = render (h, oneVoice (s, 0.0f), vowel);
            auto st = measure (r.harmL.data() + skip, nLen - skip);
            const double d = 20.0 * std::log10 ((st.rms * M_SQRT2 + 1e-12) / (in.rms + 1e-12));
            CHECK (std::abs (d) < 6.0, "%+.0f st changed the level by %.1f dB", s, d);
        }
    }

    // -- 11. screams: bounded, finite, and still shifted ---------------------
    {
        std::vector<float> scream;
        makeScream (scream, nLen, kFs);
        for (float s : { -12.0f, -7.0f, 5.0f, 12.0f })
            for (float f : { -5.0f, 0.0f, 5.0f })
            {
                h.reset();
                auto r = render (h, oneVoice (s, f), scream);
                auto st = measure (r.harmL.data() + skip, nLen - skip);
                CHECK (st.finite, "scream %+.0f st / %+.0f formant produced a non-finite sample", s, f);
                CHECK (st.peak < 6.0f, "scream %+.0f st / %+.0f formant reached %.2f", s, f, st.peak);
            }
    }

    // -- 12. transients survive, and the latency is what it says -------------
    {
        h.reset();
        std::vector<float> click ((size_t) nLen, 0.0f);
        for (int i = 0; i < 40; ++i) click[(size_t) (4800 + i)] = (i < 4 ? 0.8f : 0.0f);
        auto r = render (h, oneVoice (0.0f, 0.0f), click);
        int argmax = 0;
        for (int i = 0; i < nLen; ++i)
            if (std::abs (r.harmL[(size_t) i]) > std::abs (r.harmL[(size_t) argmax])) argmax = i;
        CHECK (std::abs (argmax - (4800 + lat)) <= 3,
               "the click came out at %d, latency says %d", argmax, 4800 + lat);

        //  energy outside +-2 ms of the click is smear; a phase vocoder that
        //  did not reset on the transient would put most of it there
        double inWin = 0.0, outWin = 0.0;
        const int w = (int) (0.002 * kFs);
        for (int i = 0; i < nLen; ++i)
        {
            const double e = (double) r.harmL[(size_t) i] * r.harmL[(size_t) i];
            if (std::abs (i - (4800 + lat)) <= w) inWin += e; else outWin += e;
        }
        const double smearDb = 10.0 * std::log10 ((outWin + 1e-30) / (inWin + 1e-30));
        std::printf ("  transient smear        %7.1f dB outside +-2 ms\n", smearDb);
        CHECK (smearDb < -6.0, "the click smeared (%.1f dB outside the window)", smearDb);
    }

    // -- 12b. TONE PURITY: what a shifted sine turns into -------------------
    /*  The sharpest artefact test there is. A sine has one line in it; after
        a shift it must still have one line, at the new frequency. Anything
        else the engine invented — a lobe placed at the wrong bin beating
        against its own phase accumulator, a stretched lobe re-synthesising
        as an amplitude-modulated partial, a phase reset firing where there
        is no transient — shows up here as sidebands, and nowhere else as
        clearly. */
    {
        std::printf ("  shifted-tone purity:\n");
        for (double f : { 220.0, 440.0, 1000.0 })
            for (float semis : { -7.0f, 3.0f, 7.0f })
            {
                std::vector<float> tone ((size_t) nLen);
                for (int i = 0; i < nLen; ++i)
                    tone[(size_t) i] = 0.5f * (float) std::sin (2.0 * M_PI * f * i / kFs);

                h.reset();
                auto r = render (h, oneVoice (semis, 0.0f), tone);
                const double want = f * ratioSemis (semis);

                //  energy at the wanted line (and its immediate neighbours)
                //  against everything else in the band
                Spectrum sp = measureSpectrum (r.harmL.data() + skip, nLen - skip, kFs);
                double sig = -200.0, spur = -200.0;
                const int kw = (int) (want / sp.binHz + 0.5);
                for (int k = 2; k <= sp.half; ++k)
                {
                    if (std::abs (k - kw) <= 4) sig = std::max (sig, (double) sp.db[(size_t) k]);
                    else                        spur = std::max (spur, (double) sp.db[(size_t) k]);
                }
                const double snr = sig - spur;
                std::printf ("    %6.0f Hz %+4.0f st -> %7.1f Hz   worst spur %6.1f dB\n",
                             f, semis, want, -snr);
                CHECK (snr > 30.0, "%.0f Hz shifted %+.0f st has a spur only %.1f dB down",
                       f, semis, snr);
            }
        std::printf ("\n");
    }

    // -- 13. determinism -----------------------------------------------------
    {
        h.reset(); auto a = render (h, oneVoice (7.0f, 2.0f), vowel, 256);
        h.reset(); auto b = render (h, oneVoice (7.0f, 2.0f), vowel, 64);
        float worst = 0.0f;
        for (int i = 0; i < nLen; ++i) worst = std::max (worst, std::abs (a.harmL[(size_t) i] - b.harmL[(size_t) i]));
        CHECK (worst == 0.0f, "the engine is block-size dependent (%.3g)", worst);
    }

    // -- 14. every window size ------------------------------------------------
    /*  A window can only separate harmonics that are further apart than its
        own main lobe: Hann is 4 bins wide, so the lowest f0 a window can
        shift cleanly is 4*fs/N. That is 187 Hz for TIGHT, 94 Hz for NATURAL
        and 47 Hz for SMOOTH at 48 k, and it is not a bug to be fixed — it is
        why the switch exists. All three are measured here on ONE voice that
        every window can resolve, so what is being tested is the window and
        nothing else, and they must agree with each other. */
    {
        std::vector<float> v240;
        makeVowel (v240, nLen, kFs, 240.0, 700.0, 1220.0, 2600.0);
        const double want = 240.0 * ratioSemis (7.0);
        double ref = 0.0;

        for (int d = 0; d < kDetails; ++d)
        {
            h.setDetail (d);
            h.reset();
            auto r = render (h, oneVoice (7.0f, 0.0f), v240);
            auto st = measure (r.harmL.data() + skip, nLen - skip);
            const double got = measureF0 (r.harmL.data() + skip, 16384, kFs);
            const double moved = formantMove (v240.data() + skip, nLen - skip, 240.0,
                                              r.harmL.data() + skip, nLen - skip, want);
            std::printf ("  detail %d (window %5d, floor %3.0f Hz)  f0 %6.1f -> %6.1f Hz"
                         "  formants moved %+5.2f st\n",
                         d, h.windowFor (d), 4.0 * kFs / h.windowFor (d), 240.0, got, moved);
            CHECK (st.finite, "detail %d produced a non-finite sample", d);
            CHECK (std::abs (got - want) / want < 0.01, "detail %d landed on %.1f Hz, wanted %.1f",
                   d, got, want);
            if (d == 1) ref = moved;
            else CHECK (std::abs (moved - ref) < 0.7,
                        "detail %d reads %+.2f st of formant move where NATURAL reads %+.2f — "
                        "the window size must not change the answer", d, moved, ref);
        }

        //  and the floor is where it says it is: TIGHT on a 120 Hz voice is
        //  below its own limit and must NOT be trusted
        h.setDetail (0);
        h.reset();
        auto r = render (h, oneVoice (7.0f, 0.0f), vowel);
        const double got = measureF0 (r.harmL.data() + skip, 16384, kFs);
        CHECK (measure (r.harmL.data() + skip, nLen - skip).finite,
               "TIGHT below its floor produced a non-finite sample");
        CHECK (std::abs (got - 120.0 * ratioSemis (7.0)) / (120.0 * ratioSemis (7.0)) > 0.05,
               "TIGHT now resolves a 120 Hz voice (%.1f Hz) — the documented floor moved", got);
        h.setDetail (1);
        std::printf ("\n");
    }

    // -- 14b. THE ENVELOPE LIMIT, measured rather than claimed ----------------
    /*  The tract can only be estimated where the voice put a harmonic, and a
        high voice puts them far apart. At 90 Hz there is a harmonic every
        90 Hz and the envelope is nearly exact; at 330 Hz the estimator is
        interpolating across a third of an octave and a narrow F1 gets
        rounded off, so a shifted copy carries a slightly flatter tract. The
        PITCH stays exact throughout — this is a colour error, not a tuning
        one — and the numbers below are what the design doc quotes. Every
        pitch shifter that separates source from filter has this limit; the
        point of measuring it is that a regression cannot hide in it. */
    {
        std::printf ("  envelope limit vs input pitch (+7 st, formant 0):\n");
        for (double f0 : { 90.0, 120.0, 165.0, 240.0, 330.0 })
        {
            std::vector<float> v;
            makeVowel (v, nLen, kFs, f0, 700.0, 1220.0, 2600.0);
            h.reset();
            auto r = render (h, oneVoice (7.0f, 0.0f), v);
            const double want = f0 * ratioSemis (7.0);
            const double got  = measureF0 (r.harmL.data() + skip, 16384, kFs);
            const double moved = formantMove (v.data() + skip, nLen - skip, f0,
                                              r.harmL.data() + skip, nLen - skip, want);
            std::printf ("    f0 %5.0f Hz -> %6.1f Hz   formants moved %+5.2f st\n", f0, got, moved);
            CHECK (std::abs (got - want) / want < 0.01,
                   "a %.0f Hz voice landed on %.1f Hz, wanted %.1f", f0, got, want);
            CHECK (std::abs (moved) < (f0 < 200.0 ? 0.8 : 3.0),
                   "a %.0f Hz voice dragged the formants %+.2f st", f0, moved);
        }
        std::printf ("\n");
    }

    // -- 15. four voices, humanised: a choir, bounded -------------------------
    {
        h.reset();
        Params p;
        p.humanize = 0.6f;
        const float semis[kVoices] = { -12.0f, 3.0f, 7.0f, 12.0f };
        const float pans [kVoices] = { -0.7f, -0.25f, 0.25f, 0.7f };
        for (int v = 0; v < kVoices; ++v)
        {
            p.v[v].on = true;
            p.v[v].semitones = semis[v];
            p.v[v].formant = (v % 2) ? 1.5f : -1.5f;
            p.v[v].gainDb = -6.0f;
            p.v[v].pan = pans[v];
            p.v[v].delayMs = (float) v * 9.0f;
        }
        auto r = render (h, p, vowel);
        auto sl = measure (r.harmL.data() + skip, nLen - skip);
        auto sr = measure (r.harmR.data() + skip, nLen - skip);
        CHECK (sl.finite && sr.finite, "the choir produced a non-finite sample");
        CHECK (sl.peak < 4.0f && sr.peak < 4.0f, "the choir reached %.2f / %.2f", sl.peak, sr.peak);
        CHECK (sl.rms > 0.01f && sr.rms > 0.01f, "the choir is inaudible (%.4f / %.4f)", sl.rms, sr.rms);
        //  humanise must actually decorrelate the two sides
        double num = 0.0, dl = 0.0, dr = 0.0;
        for (int i = skip; i < nLen; ++i)
        {
            num += (double) r.harmL[(size_t) i] * r.harmR[(size_t) i];
            dl  += (double) r.harmL[(size_t) i] * r.harmL[(size_t) i];
            dr  += (double) r.harmR[(size_t) i] * r.harmR[(size_t) i];
        }
        const double corr = num / std::sqrt (dl * dr + 1e-30);
        std::printf ("  choir L/R correlation  %7.2f\n", corr);
        CHECK (corr < 0.9, "the choir is mono (correlation %.2f)", corr);
    }

    // -- 16. cost --------------------------------------------------------------
    {
        h.reset();
        Params p; p.humanize = 0.5f;
        for (int v = 0; v < kVoices; ++v)
        {
            p.v[v].on = true;
            p.v[v].semitones = (float) (v * 4 - 6);
            p.v[v].gainDb = -6.0f;
        }
        const int n = (int) (kFs * 4.0);
        std::vector<float> in ((size_t) n);
        makeVowel (in, n, kFs, 130.0, 650.0, 1100.0, 2500.0);
        const auto t0 = std::chrono::steady_clock::now();
        auto r = render (h, p, in, 256);
        const auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        std::printf ("  four voices            %7.1f x real time (%.1f %% of one core)\n",
                     4.0 / secs, 100.0 * secs / 4.0);
        CHECK (measure (r.harmL.data(), n).finite, "the cost run produced a non-finite sample");
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" — ALL CLEAR\n");
    else               std::printf (" — %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
