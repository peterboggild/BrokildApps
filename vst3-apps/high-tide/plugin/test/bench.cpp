/*  HIGH TIDE — the bench.

    The design document makes claims; this measures them against the real
    engine. Two house rules: a window that cannot fail proves nothing (every
    threshold below was chosen to be failable), and BOUNDED proves nothing
    about WORKING — where a mechanism is claimed, the check renders with and
    without it and measures the difference.

    cmake -S test -B test/build ; cmake --build test/build --config Release
*/
#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
#include <functional>
#include <chrono>
#if defined(_MSC_VER)
#include <immintrin.h>
#endif

using namespace ht;

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const char* detail = "")
{ ++checks; if (!c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail); } }
static void head (const char* s) { std::printf ("\n== %s ==\n", s); std::fflush (stdout); }

static const int SR = 48000, BLK = 256;

//==============================================================================
struct Ev { double t; int kind; int a; float v; };   // 0 on, 1 off, 2 bend, 3 drop-on(z=v), 4 drop-off

struct Take
{
    std::vector<float> L, R;
    std::vector<double> z;        // voice 0's z per block
    double rms (double t0, double t1) const
    {
        size_t a = (size_t) (t0 * SR), b = std::min (L.size(), (size_t) (t1 * SR));
        if (b <= a) return 0;
        double acc = 0;
        for (size_t i = a; i < b; ++i) acc += (double) L[i] * L[i] + (double) R[i] * R[i];
        return std::sqrt (acc / (2.0 * (b - a)));
    }
    float peak() const
    {
        float p = 0;
        for (float v : L) p = std::max (p, std::abs (v));
        for (float v : R) p = std::max (p, std::abs (v));
        return p;
    }
    bool finite() const
    {
        for (float v : L) if (! std::isfinite (v)) return false;
        for (float v : R) if (! std::isfinite (v)) return false;
        return true;
    }
    bool exactlyZero() const
    {
        for (float v : L) if (v != 0.0f) return false;
        for (float v : R) if (v != 0.0f) return false;
        return true;
    }
};

static void setP (Engine& e, const char* id, float v) { paramSpec (paramIndex (id)).get (e.p) = v; }

static Take run (Engine& e, double secs, const std::vector<Ev>& evs, int sr = SR,
                 const std::function<void (Engine&, int)>* perBlock = nullptr)
{
    Take t;
    const int nb = (int) (secs * sr / BLK);
    t.L.reserve ((size_t) nb * BLK); t.R.reserve ((size_t) nb * BLK); t.z.reserve ((size_t) nb);
    std::vector<float> bl (BLK), br (BLK);
    size_t ei = 0;
    std::vector<Ev> sorted = evs;
    std::sort (sorted.begin(), sorted.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
    for (int b = 0; b < nb; ++b)
    {
        const double now = (double) b * BLK / sr;
        while (ei < sorted.size() && sorted[ei].t <= now)
        {
            const Ev& v = sorted[ei++];
            if      (v.kind == 0) e.noteOn (v.a, v.v);
            else if (v.kind == 1) e.noteOff (v.a);
            else if (v.kind == 2) e.setBend (v.v);
            else if (v.kind == 3) e.drop (v.v, 0.6f, v.a, true);
            else if (v.kind == 4) e.drop (0, 0, v.a, false);
        }
        if (perBlock) (*perBlock) (e, b);
        e.process (bl.data(), br.data(), BLK);
        t.L.insert (t.L.end(), bl.begin(), bl.end());
        t.R.insert (t.R.end(), br.begin(), br.end());
        t.z.push_back (e.voiceZ (0));
    }
    return t;
}

static Engine* fresh (Engine& e, int sr = SR)
{
    e.p = Params();
    e.prepare (sr, BLK);
    e.terrain().makeReference();
    e.setLanes (Lanes());
    setP (e, "ampA", 0.0f); setP (e, "ampD", 0.5f); setP (e, "ampS", 1.0f); setP (e, "ampR", 0.3f);
    setP (e, "friction", 0.0f);
    return &e;
}

//==============================================================================
static double goertzel (const std::vector<float>& x, size_t a, size_t b, double f, int sr)
{
    b = std::min (b, x.size());
    if (b <= a + 16) return 0;
    const double w = 2.0 * PI * f / sr;
    const double c = 2.0 * std::cos (w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double h = 0.5 * (1.0 - std::cos (2.0 * PI * (i - a) / (double) (b - a - 1)));
        s0 = (double) x[i] * h + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) (b - a);
}

static double pitchNear (const std::vector<float>& x, size_t a, size_t b, double fGuess, int sr, double spanCents = 300)
{
    double bestF = fGuess, bestM = -1;
    for (double c = -spanCents; c <= spanCents; c += 4)
    {
        const double f = fGuess * std::pow (2.0, c / 1200.0);
        const double m = goertzel (x, a, b, f, sr);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    double lo = bestF * std::pow (2.0, -4.0 / 1200.0), hi = bestF * std::pow (2.0, 4.0 / 1200.0);
    for (int it = 0; it < 30; ++it)
    {
        const double m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
        if (goertzel (x, a, b, m1, sr) < goertzel (x, a, b, m2, sr)) lo = m1; else hi = m2;
    }
    return 0.5 * (lo + hi);
}
static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }
static double dB (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }
/*  A bowl that is not isochronous plays at ITS OWN pitch, not the note's —
    that is the design, so every spectral probe on such a bowl must first find
    the actual fundamental. Measuring harmonics of the note instead reads pure
    leakage (the first run of this bench did exactly that). */
static double actualF (const std::vector<float>& x, size_t a, size_t b, double fGuess, int sr)
{ return pitchNear (x, a, b, fGuess, sr, 1400); }

//  radix-2 FFT for the aliasing measurement
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1, 0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/*  Non-harmonic energy against harmonic energy over a 65536-sample window:
    what is in the spectrum that is not on a multiple of f. Blackman-Harris
    keeps the leakage floor below -90 dB, so what remains is aliasing (and
    the little the DC blocker and decimator leave). Returns dB. */
static double aliasFloor (const std::vector<float>& x, size_t a, double f, int sr)
{
    const size_t N = 65536;
    if (x.size() < a + N) return 0;
    std::vector<std::complex<double>> buf (N);
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / (double) (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * PI * t) + 0.14128 * std::cos (4 * PI * t) - 0.01168 * std::cos (6 * PI * t);
        buf[i] = std::complex<double> ((double) x[a + i] * w, 0.0);
    }
    fft (buf);
    const double binHz = (double) sr / (double) N;
    double harm = 0, other = 0;
    for (size_t k = 8; k < N / 2; ++k)
    {
        const double fk = k * binHz;
        const double m = std::round (fk / f);
        //  a harmonic's window grows with its order: the measured f carries a
        //  few ppm of error and the 20th harmonic wears twenty times that
        const bool onHarm = m >= 1 && std::abs (fk - m * f) <= 4.5 * binHz + 0.0015 * m * f;
        const double e = std::norm (buf[k]);
        if (onHarm) harm += e; else other += e;
    }
    return 10.0 * std::log10 (std::max (1e-30, other) / std::max (1e-30, harm));
}

//==============================================================================
//  terrain builders for the bench (the factory's shapes are in Factory.cpp;
//  these are the ones the checks need with exact control)
static void fillAll (Terrain& t, const std::function<float (float)>& shape)
{
    for (int j = 0; j < NZ; ++j)
        for (int i = 0; i < NX; ++i)
            t.at (i, j) = shape (XMIN + (XMAX - XMIN) * (float) i / (float) (NX - 1));
    t.recomputeRelief();
}
static float shearedBowl (float x, float s)
{
    float lo = 0, hi = 3.0f;
    for (int it = 0; it < 40; ++it)
    {
        const float h = 0.5f * (lo + hi);
        const float r = std::sqrt (2.0f * h);
        const float xl = -r + s * h, xr = r + s * h;
        if (x >= xl && x <= xr) hi = h; else lo = h;
    }
    return hi;
}
//  the factory's box: a rounded knee over the brush floor (see Factory.cpp)
static float boxBowl (float x, float w = 0.55f, float k = 6.0f)
{
    const float ws = 0.04f;
    const float a  = (std::abs (x) - w) / ws;
    const float s  = ws * (a > 20.0f ? a : std::log1p (std::exp (a)));
    return k * s * s;
}
//  and the hard knee the brush refuses — kept to show why
static float hardBox (float x, float w = 0.55f, float k = 40.0f)
{
    const float a = std::abs (x) - w; return a <= 0 ? 0.0f : k * a * a;
}
static float pitBowl (float x, float d = 0.16f, float pw = 0.08f)
{
    return 0.5f * x * x + d * (1.0f - std::exp (-(x * x) / (pw * pw)));
}
static float doubleWell (float x, float w = 0.55f, float h = 0.09f)
{
    const float a = h / (w * w * w * w);
    const float q = (x * x - w * w);
    float u = a * q * q;
    const float ax = std::abs (x);
    if (ax > 1.0f) u += 3.0f * (ax - 1.0f) * (ax - 1.0f);
    return u;
}
//  two valleys and a ridge of height H between them
static void twoValleys (Terrain& t, float H)
{
    for (int j = 0; j < NZ; ++j)
    {
        const float z = (float) j / (float) (NZ - 1);
        const float d = (z - 0.5f) / 0.06f;
        const float bump = H * std::exp (-d * d * 2.0f);
        for (int i = 0; i < NX; ++i)
        {
            const float x = XMIN + (XMAX - XMIN) * (float) i / (float) (NX - 1);
            t.at (i, j) = 0.5f * x * x + bump;
        }
    }
    t.recomputeRelief();
}

//==============================================================================
int main (int argc, char** argv)
{
   #if defined(_MSC_VER)
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
   #endif
    (void) argc; (void) argv;
    std::printf ("HIGH TIDE bench  (%d params, %d factory patches)\n", numParams(), numFactory());

    //--------------------------------------------------------------------------
    head ("1  nothing to erase: the reference bowl is a sine");
    {
        Engine e; fresh (e);
        setP (e, "tone", 1.0f);
        auto t = run (e, 2.0, { { 0.0, 0, 45, 1.0f } });     // A2 = 110 Hz
        const double f0 = 110.0;
        const double fund = goertzel (t.L, SR / 2, SR / 2 + 32768, f0, SR);
        double worst = -200;
        for (int h = 2; h <= 8; ++h) worst = std::max (worst, dB (goertzel (t.L, SR / 2, SR / 2 + 32768, f0 * h, SR) / fund));
        const double fm = pitchNear (t.L, SR / 2, SR / 2 + 32768, f0, SR);
        std::printf ("  worst harmonic %.1f dB   pitch %.3f Hz (%+.2f c)   peak %.3f\n", worst, fm, cents (fm, f0), t.peak());
        ok (t.finite(), "finite");
        ok (worst < -60.0, "reference bowl THD < -60 dB");
        ok (std::abs (cents (fm, f0)) < 1.0, "reference bowl in tune within 1 cent");
        ok (t.peak() > 0.05, "audible");
    }

    //--------------------------------------------------------------------------
    head ("2  the isochrone: a sheared bowl holds pitch across 40 dB of strike");
    {
        double pLo = 0, pHi = 0, cLo = 0, cHi = 0;
        for (int which = 0; which < 2; ++which)
        {
            Engine e; fresh (e);
            fillAll (e.terrain(), [] (float x) { return shearedBowl (x, 0.45f); });
            setP (e, "strike", which ? 1.0f : 0.1f); setP (e, "velSens", 0.0f);
            auto t = run (e, 2.0, { { 0.0, 0, 45, 1.0f } });
            const double per = e.ballPeriod (0, 0);
            const double fm = pitchNear (t.L, SR / 2, SR / 2 + 32768, 110.0, SR);
            if (which) { pHi = per; cHi = cents (fm, 110.0); } else { pLo = per; cLo = cents (fm, 110.0); }
            std::printf ("  strike %.1f: energy %.4f  period/2pi %.5f  audio %+.2f c  2nd harm %.1f dB\n",
                         which ? 1.0 : 0.1, e.ballEnergy (0, 0), per / (2 * PI), cents (fm, 110.0),
                         dB (goertzel (t.L, SR / 2, SR / 2 + 32768, 220.0, SR) / goertzel (t.L, SR / 2, SR / 2 + 32768, 110.0, SR)));
        }
        ok (std::abs (1200.0 * std::log2 (pHi / pLo)) < 2.0, "sheared bowl period within 2 cents across 40 dB", "");
        ok (std::abs (cLo) < 2.0 && std::abs (cHi) < 2.0, "sheared bowl audio pitch within 2 cents of the note");
        //  and the same bowl breaking the rule bends
        Engine e; fresh (e);
        fillAll (e.terrain(), [] (float x) { return boxBowl (x); });
        setP (e, "strike", 0.1f); setP (e, "velSens", 0.0f);
        run (e, 1.0, { { 0.0, 0, 45, 1.0f } });
        const double pa = e.ballPeriod (0, 0);
        Engine e2; fresh (e2);
        fillAll (e2.terrain(), [] (float x) { return boxBowl (x); });
        setP (e2, "strike", 1.0f); setP (e2, "velSens", 0.0f);
        run (e2, 1.0, { { 0.0, 0, 45, 1.0f } });
        const double pb = e2.ballPeriod (0, 0);
        std::printf ("  box bowl: period ratio hard/soft %.3f (%+.0f c) — tension, as carved\n", pb / pa, 1200 * std::log2 (pb / pa));
        ok (std::abs (1200 * std::log2 (pb / pa)) > 100.0, "a box bowl bends with the strike (tension is carved, not a knob)");
        //  the width test agrees
        std::printf ("  widthDeparture: parabola %.4f  sheared %.4f  box %.3f\n",
                     Terrain().widthDeparture (10), e.terrain().widthDeparture (10) * 0 + [&] { Terrain t; fillAll (t, [] (float x) { return shearedBowl (x, 0.45f); }); return t.widthDeparture (10); }(),
                     e.terrain().widthDeparture (10));
        { Terrain t; fillAll (t, [] (float x) { return shearedBowl (x, 0.45f); });
          ok (t.widthDeparture (10) < 0.03, "width test: sheared bowl passes"); }
        ok (e.terrain().widthDeparture (10) > 0.2, "width test: box bowl fails");
        //  SERVO brings the box back
        Engine e3; fresh (e3);
        fillAll (e3.terrain(), [] (float x) { return boxBowl (x); });
        setP (e3, "strike", 1.0f); setP (e3, "velSens", 0.0f); setP (e3, "servo", 0.7f);
        auto t3 = run (e3, 2.0, { { 0.0, 0, 45, 1.0f } });
        const double f3 = pitchNear (t3.L, SR, SR + 32768, 110.0, SR, 900);
        std::printf ("  box + SERVO 0.7: audio %+.1f c (clock %.3f)\n", cents (f3, 110.0), e3.ballClock (0, 0));
        ok (std::abs (cents (f3, 110.0)) < 8.0, "SERVO corrects the box to within 8 cents");
    }

    //--------------------------------------------------------------------------
    head ("3  energy: a frictionless ball keeps its energy for a minute");
    {
        Engine e; fresh (e);
        fillAll (e.terrain(), [] (float x) { return pitBowl (x); });
        setP (e, "friction", 0.0f);
        run (e, 0.2, { { 0.0, 0, 45, 1.0f } });
        const double e0 = e.ballEnergy (0, 0);
        run (e, 60.0, {});
        const double e1 = e.ballEnergy (0, 0);
        std::printf ("  pit bowl: energy %.6f -> %.6f (%.4f %%)\n", e0, e1, 100.0 * (e1 - e0) / e0);
        ok (std::abs (e1 - e0) / e0 < 0.005, "pit bowl: energy drift < 0.5 % over 60 s");
        ok (e.activeVoices() == 1, "the note is still sounding");
        Engine q; fresh (q); setP (q, "friction", 0.0f);
        run (q, 0.2, { { 0.0, 0, 45, 1.0f } });
        const double q0 = q.ballEnergy (0, 0);
        run (q, 60.0, {});
        const double q1 = q.ballEnergy (0, 0);
        std::printf ("  parabola: energy %.6f -> %.6f (%.5f %%)\n", q0, q1, 100.0 * (q1 - q0) / q0);
        ok (std::abs (q1 - q0) / q0 < 0.0001, "parabola: energy drift < 0.01 % over 60 s (symplectic)");
    }

    //--------------------------------------------------------------------------
    head ("4  the dictionary, item by item");
    {
        //  harmonics of the bowl's OWN fundamental (see actualF)
        auto spectrum = [] (Engine& e, const char* tap, double* h) -> double {
            setP (e, "tapPos", 0); setP (e, "tapVel", 0); setP (e, "tapFrc", 0);
            setP (e, tap, 1.0f); setP (e, "tone", 1.0f); setP (e, "strike", 0.8f); setP (e, "velSens", 0);
            auto t = run (e, 2.0, { { 0.0, 0, 45, 1.0f } });
            const double f0 = actualF (t.L, SR, SR + 32768, 110.0, SR);
            for (int k = 1; k <= 6; ++k) h[k] = goertzel (t.L, SR, SR + 32768, f0 * k, SR);
            return f0;
        };
        double h[8];
        //  box: velocity is a square, position a triangle
        { Engine e; fresh (e); fillAll (e.terrain(), [] (float x) { return boxBowl (x, 0.55f, 40.0f); });
          const double f0 = spectrum (e, "tapVel", h);
          const double r3 = dB (h[3] / h[1]), r5 = dB (h[5] / h[1]), r2 = dB (h[2] / h[1]);
          std::printf ("  box/velocity (own pitch %.1f Hz): 3rd %.1f dB  5th %.1f dB  2nd %.1f dB (square: -9.5, -14.0, none)\n", f0, r3, r5, r2);
          ok (r3 > -12.5 && r3 < -6.5, "box velocity tap is square-like (3rd near -9.5 dB)");
          ok (r2 < -30.0, "box is symmetric: no even harmonics");
          Engine e2; fresh (e2); fillAll (e2.terrain(), [] (float x) { return boxBowl (x, 0.55f, 40.0f); });
          spectrum (e2, "tapPos", h);
          std::printf ("  box/position: 3rd %.1f dB (triangle: -19.1)\n", dB (h[3] / h[1]));
          ok (dB (h[3] / h[1]) > -23.0 && dB (h[3] / h[1]) < -15.0, "box position tap is triangle-like");
        }
        //  pit: a pulse in the velocity — brighter than the parabola
        { Engine a; fresh (a); spectrum (a, "tapVel", h);
          const double para3 = dB (h[3] / h[1]);
          Engine b; fresh (b); fillAll (b.terrain(), [] (float x) { return pitBowl (x, 0.25f, 0.06f); });
          const double fp = spectrum (b, "tapVel", h);
          const double pit3 = dB (h[3] / h[1]), pit5 = dB (h[5] / h[1]);
          std::printf ("  pit/velocity (own pitch %.1f Hz): 3rd %.1f dB 5th %.1f dB   (parabola 3rd %.1f)\n", fp, pit3, pit5, para3);
          ok (pit3 > -40.0, "a pit in the floor puts harmonics into the velocity tap");
          ok (pit3 > para3 + 20.0, "and far more than the parabola has"); }
        //  asymmetry: even harmonics; mirror: gone
        { Engine a; fresh (a); fillAll (a.terrain(), [] (float x) { return shearedBowl (x, 0.5f); });
          spectrum (a, "tapPos", h);
          const double ev = dB (h[2] / h[1]);
          Engine b; fresh (b); spectrum (b, "tapPos", h);
          const double ev0 = dB (h[2] / h[1]);
          std::printf ("  sheared/position 2nd %.1f dB   mirrored 2nd %.1f dB\n", ev, ev0);
          ok (ev > -25.0, "asymmetry gives even harmonics");
          ok (ev0 < -60.0, "a mirrored bowl has none"); }
        //  double well: near the barrier the cycle slows (the saddle dwell); above it, it spans both pits
        { double pLo, pSep;
          { Engine e; fresh (e); fillAll (e.terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
            setP (e, "strike", 0.25f); setP (e, "velSens", 0); run (e, 1.0, { { 0.0, 0, 45, 1.0f } }); pLo = e.ballPeriod (0, 0);
            std::printf ("  double well, soft: energy %.4f (barrier 0.09)  period/2pi %.3f\n", e.ballEnergy (0, 0), pLo / (2 * PI)); }
          { Engine e; fresh (e); fillAll (e.terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
            setP (e, "strike", 0.447f); setP (e, "velSens", 0); run (e, 1.0, { { 0.0, 0, 45, 1.0f } }); pSep = e.ballPeriod (0, 0);
            std::printf ("  double well, just over the barrier: energy %.4f  period/2pi %.3f  (ratio %.2f — the saddle dwell)\n", e.ballEnergy (0, 0), pSep / (2 * PI), pSep / pLo); }
          ok (pSep / pLo > 1.4, "just above the barrier the cycle slows through the saddle (period ratio > 1.4)");
          Engine e; fresh (e); fillAll (e.terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
          setP (e, "strike", 0.8f); setP (e, "velSens", 0);
          std::vector<float> L (BLK), R (BLK); e.noteOn (45, 1.0f);
          bool left = false, right = false;
          for (int b = 0; b < SR / BLK; ++b) { e.process (L.data(), R.data(), BLK); const double x = e.ballX (0, 0); left = left || x < -0.3; right = right || x > 0.3; }
          ok (left && right, "well above the barrier the ball visits both pits");
          Engine s; fresh (s); fillAll (s.terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
          setP (s, "strike", 0.25f); setP (s, "velSens", 0); s.noteOn (45, 1.0f);
          left = right = false;
          for (int b = 0; b < SR / BLK; ++b) { s.process (L.data(), R.data(), BLK); const double x = s.ballX (0, 0); left = left || x < -0.3; right = right || x > 0.3; }
          ok (left != right, "a soft strike stays in one pit"); }
    }

    //--------------------------------------------------------------------------
    head ("5  ROCK: period doubling, chaos as chaos, and zero as nothing");
    {
        //  a rocked double well at the note: friction kills the bowl's own motion,
        //  what remains is the forced response — and its sub-harmonics
        double firstDouble = -1, none = -1;
        for (int k = 0; k <= 10; ++k)
        {
            const float depth = 0.1f * k;
            Engine e; fresh (e);
            fillAll (e.terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
            setP (e, "rock", depth); setP (e, "rockRatio", 0.0f); setP (e, "tapVel", 1.0f); setP (e, "tapPos", 0.0f);
            setP (e, "strike", 0.3f); setP (e, "velSens", 0); setP (e, "friction", 0.5f); setP (e, "tone", 1.0f);
            auto t = run (e, 4.0, { { 0.0, 0, 45, 1.0f } });
            const double a1 = goertzel (t.L, 3 * SR, 3 * SR + 32768, 110.0, SR);
            const double a2 = goertzel (t.L, 3 * SR, 3 * SR + 32768, 55.0, SR);
            const double a4 = goertzel (t.L, 3 * SR, 3 * SR + 32768, 27.5, SR);
            const double nh = aliasFloor (t.L, 2 * SR, 55.0, SR);   // energy off the f/2 grid = broadband
            std::printf ("  rock %.1f: f %6.1f dBFS  f/2 %6.1f dB  f/4 %6.1f dB  off-grid %6.1f dB  rms %.3f\n", depth, dB (a1), dB (a2 / a1), dB (a4 / a1), nh, t.rms (3, 4));
            if (k == 0) none = dB (a2 / a1);
            if (firstDouble < 0 && dB (a1) > -40.0 && dB (a2 / a1) > -20.0) firstDouble = depth;
            ok (t.finite() && t.peak() <= 1.0f, "rocked well bounded");
        }
        ok (firstDouble > 0.05, "a sub-octave appears at some ROCK depth");
        std::printf ("  first sub-octave at ROCK %.1f\n", firstDouble);

        //  chaos: two strikes a hair apart diverge; without rock they do not
        auto diverge = [] (float depth) {
            Engine a; fresh (a); Engine b; fresh (b);
            for (Engine* e : { &a, &b })
            {
                fillAll (e->terrain(), [] (float x) { return doubleWell (x, 0.55f, 0.09f); });
                setP (*e, "rock", depth); setP (*e, "tapVel", 1.0f); setP (*e, "tapPos", 0.0f); setP (*e, "friction", 0.25f);
                setP (*e, "strike", 0.3f); setP (*e, "velSens", 1.0f);
            }
            auto ta = run (a, 4.0, { { 0.0, 0, 45, 0.7f } });
            auto tb = run (b, 4.0, { { 0.0, 0, 45, 0.7f + 1e-6f } });
            double d = 0, ref = 0;
            for (size_t i = 3 * SR; i < ta.L.size(); ++i) { d = std::max (d, (double) std::abs (ta.L[i] - tb.L[i])); ref = std::max (ref, (double) std::abs (ta.L[i])); }
            return d / std::max (1e-9, ref);
        };
        const double d0 = diverge (0.0f), d1 = diverge (0.85f);
        std::printf ("  divergence (1e-6 apart, after 3 s): rock 0 %.2e   rock 0.85 %.3f\n", d0, d1);
        ok (d0 < 1e-3, "no rock: a hair apart stays a hair apart");
        ok (d1 > 0.3, "deep rock: a hair apart diverges to order one (chaos, not busyness)");

        //  ROCK 0 is bit-identical whatever the other rock settings are
        Engine a; fresh (a); Engine b; fresh (b);
        for (Engine* e : { &a, &b }) fillAll (e->terrain(), [] (float x) { return doubleWell (x); });
        setP (b, "rockRatio", 1.0f); setP (b, "rockHz", 0.9f); setP (b, "rockMode", 1.0f);
        auto ta = run (a, 1.0, { { 0.0, 0, 45, 0.7f } });
        auto tb = run (b, 1.0, { { 0.0, 0, 45, 0.7f } });
        ok (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "ROCK 0 is memcmp-identical whatever the ratio and mode");

        //  BREATH at 2/1 pumps a bowl that would otherwise decay (the bow)
        { Engine e; fresh (e); setP (e, "friction", 0.4f); setP (e, "rockMode", 1.0f); setP (e, "rock", 0.5f); setP (e, "rockRatio", 1.0f / 6.0f);
          auto t = run (e, 4.0, { { 0.0, 0, 45, 0.6f } });
          Engine e2; fresh (e2); setP (e2, "friction", 0.4f);
          auto t2 = run (e2, 4.0, { { 0.0, 0, 45, 0.6f } });
          std::printf ("  BREATH 2/1 held 3-4 s: rms %.4f   without %.5f\n", t.rms (3, 4), t2.rms (3, 4));
          ok (t.rms (3, 4) > 20.0 * t2.rms (3, 4), "BREATH at 2/1 sustains a note that friction would have killed"); }
    }

    //--------------------------------------------------------------------------
    head ("6  relief and tide: the ridge, the water line, the stranded ball");
    {
        //  a pin jump at 0.3 s from valley 0.2 to valley 0.8 across a ridge of height H
        auto crossing = [] (float H, float tide, float hold) {
            Engine e; fresh (e);
            twoValleys (e.terrain(), H);
            Lanes l; l.pin.on = { { 0.0f, 0.2f, 0 }, { 0.30f, 0.2f, 0 }, { 0.31f, 0.8f, 0 } };
            e.setLanes (l);
            setP (e, "tide", tide); setP (e, "hold", hold);
            auto t = run (e, 3.0, { { 0.0, 0, 45, 0.7f } });
            for (size_t i = 0; i < t.z.size(); ++i) if (t.z[i] > 0.5) return (double) i * BLK / SR - 0.31;
            return 99.0;
        };
        const double c1 = crossing (0.05f, 0.0f, 0.6f), c2 = crossing (0.25f, 0.0f, 0.6f), c3 = crossing (0.6f, 0.0f, 0.6f);
        std::printf ("  low tide, HOLD 0.6: crossing after %.3f / %.3f / %.3f s for ridge 0.05 / 0.25 / 0.6\n", c1, c2, c3);
        ok (c1 < c2 && c2 < c3, "a higher ridge takes longer to cross");
        const double h1 = crossing (0.05f, 1.0f, 0.6f), h2 = crossing (0.25f, 1.0f, 0.6f), h3 = crossing (0.6f, 1.0f, 0.6f);
        std::printf ("  high tide: %.3f / %.3f / %.3f s\n", h1, h2, h3);
        ok (std::abs (h1 - h3) < 0.02 && std::abs (h2 - h3) < 0.02, "at TIDE 1 the ridge is not there");
        const double s = crossing (0.6f, 0.0f, 0.15f);
        std::printf ("  low tide, HOLD 0.15, ridge 0.6: %s\n", s > 90 ? "stranded (never crosses)" : "crossed");
        ok (s > 90, "a weak tether at low tide leaves the ball stranded");
        const double s2 = crossing (0.6f, 0.0f, 1.0f);
        std::printf ("  low tide, HOLD 1.0, ridge 0.6: crossing after %.3f s\n", s2);
        ok (s2 < 1.0, "a strong tether drags it over anyway");
    }

    //--------------------------------------------------------------------------
    head ("7  pins: the same strike leaves the same wake; hold reaches; loops loop; release anchors");
    {
        Lanes l; l.pin.on = { { 0.0f, 0.2f, 2 }, { 0.5f, 0.7f, 2 } }; l.pin.off = { { 0.4f, 0.1f, 2 } };
        Engine a; fresh (a); a.setLanes (l); setP (a, "tide", 1.0f); setP (a, "hold", 1.0f); setP (a, "ampR", 0.8f);
        Engine b; fresh (b); b.setLanes (l); setP (b, "tide", 1.0f); setP (b, "hold", 1.0f); setP (b, "ampR", 0.8f);
        auto ta = run (a, 3.0, { { 0.0, 0, 45, 0.7f }, { 2.0, 1, 45, 0 } });
        auto tb = run (b, 3.0, { { 0.0, 0, 45, 0.7f }, { 2.0, 1, 45, 0 } });
        ok (std::memcmp (ta.z.data(), tb.z.data(), ta.z.size() * sizeof (double)) == 0, "two identical notes: identical wake (memcmp)");
        ok (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "and identical audio");
        const double zAt = [&] (double t) { return ta.z[(size_t) (t * SR / BLK)]; } (0.75);
        std::printf ("  HOLD 1: z at 0.75 s = %.3f (pin 0.7 at 0.5 s)\n", zAt);
        ok (std::abs (zAt - 0.7) < 0.03, "HOLD 1 reaches a pin within a quarter second");
        const double zRel = ta.z[(size_t) (2.9 * SR / BLK)];
        std::printf ("  after release: z = %.3f (off pin 0.1 at 0.4 s)\n", zRel);
        ok (std::abs (zRel - 0.1) < 0.03, "the off pin is reached after release");
        const double zStart = ta.z[0];
        ok (std::abs (zStart - 0.2) < 0.02, "a fresh ball starts in the valley it is pointed at");

        //  loop
        Lanes lp; lp.pin.on = { { 0.0f, 0.2f, 2 }, { 0.5f, 0.2f, 0 }, { 0.75f, 0.8f, 0 } }; lp.pin.hasLoop = true; lp.pin.loopA = 0.5f; lp.pin.loopB = 1.0f;
        Engine c; fresh (c); c.setLanes (lp); setP (c, "tide", 1.0f); setP (c, "hold", 1.0f);
        auto tc = run (c, 4.0, { { 0.0, 0, 45, 0.7f } });
        double worst = 0;
        for (double t = 2.0; t < 3.4; t += 0.02)
            worst = std::max (worst, std::abs (tc.z[(size_t) (t * SR / BLK)] - tc.z[(size_t) ((t + 0.5) * SR / BLK)]));
        std::printf ("  loop 0.5 s: worst |z(t) - z(t+0.5)| = %.4f\n", worst);
        ok (worst < 0.02, "a looped pin lane repeats with the loop's period");
        double zmin = 1, zmax = 0;
        for (double t = 2.0; t < 3.5; t += 0.01) { const double z = tc.z[(size_t) (t * SR / BLK)]; zmin = std::min (zmin, z); zmax = std::max (zmax, z); }
        ok (zmax - zmin > 0.4, "and it actually moves");

        //  a fresh voice at the same velocity, but a different terrain edit: different wake (the wake reads the terrain)
        Engine d; fresh (d); d.setLanes (l); setP (d, "tide", 0.0f); setP (d, "hold", 0.5f); twoValleys (d.terrain(), 0.4f);
        auto td = run (d, 3.0, { { 0.0, 0, 45, 0.7f }, { 2.0, 1, 45, 0 } });
        ok (std::memcmp (ta.z.data(), td.z.data(), ta.z.size() * sizeof (double)) != 0, "a ridge changes the wake");
    }

    //--------------------------------------------------------------------------
    head ("8  aliasing: the non-harmonic floor on the parabola, the box and the pit, at 2x and 4x");
    {
        for (int q = 0; q < 2; ++q)
        {
            const char* names[] = { "parabola", "box", "pit", "box k40" };
            for (int s = 0; s < 4; ++s)
            {
                Engine e; fresh (e);
                if (s == 1) fillAll (e.terrain(), [] (float x) { return boxBowl (x, 0.55f, 6.0f); });
                if (s == 2) fillAll (e.terrain(), [] (float x) { return pitBowl (x, 0.3f, 0.05f); });
                if (s == 3) fillAll (e.terrain(), [] (float x) { return boxBowl (x, 0.55f, 40.0f); });
                setP (e, "quality", (float) q); setP (e, "tapVel", 1.0f); setP (e, "tapPos", 0.0f);
                setP (e, "tone", 1.0f); setP (e, "strike", 1.0f); setP (e, "velSens", 0);
                auto t = run (e, 2.5, { { 0.0, 0, 83, 1.0f } });     // B5 = 987.77 Hz
                const double f = actualF (t.L, SR / 2, SR / 2 + 32768, 987.77, SR);
                const double fl = aliasFloor (t.L, SR / 2, f, SR);
                std::printf ("  %dx %-9s own pitch %7.1f Hz  non-harmonic floor %.1f dB%s\n", q ? 4 : 2, names[s], f, fl,
                             s == 3 ? "   (walls 40x the reference bowl at B5: reported, not promised)" : "");
                if (s == 0) ok (fl < -70.0, q ? "4x parabola clean" : "2x parabola clean");
                if (s == 1) ok (fl < (q ? -50.0 : -40.0), q ? "4x box alias floor < -50 dB" : "2x box alias floor < -40 dB");
                if (s == 2) ok (fl < (q ? -50.0 : -40.0), q ? "4x pit alias floor < -50 dB" : "2x pit alias floor < -40 dB");
            }
        }
        //  the brush floor: terrain sharper than the sculptor is allowed makes a mess, and the check knows it
        Engine e; fresh (e);
        for (int j = 0; j < NZ; ++j) for (int i = 0; i < NX; ++i)
        { const float x = XMIN + (XMAX - XMIN) * (float) i / (float) (NX - 1); e.terrain().at (i, j) = 0.5f * x * x + 0.02f * (float) ((i * 7919) % 13) / 13.0f; }
        e.terrain().recomputeRelief();
        setP (e, "tapVel", 1.0f); setP (e, "tapPos", 0.0f); setP (e, "tone", 1.0f); setP (e, "strike", 1.0f); setP (e, "velSens", 0);
        auto t = run (e, 2.5, { { 0.0, 0, 83, 1.0f } });
        std::printf ("  cell-scale roughness (below the brush floor): floor %.1f dB — which is why the brush refuses it\n",
                     aliasFloor (t.L, SR / 2, actualF (t.L, SR / 2, SR / 2 + 32768, 987.77, SR), SR));
        Engine hb; fresh (hb); fillAll (hb.terrain(), [] (float x) { return hardBox (x); });
        setP (hb, "tapVel", 1.0f); setP (hb, "tapPos", 0.0f); setP (hb, "tone", 1.0f); setP (hb, "strike", 1.0f); setP (hb, "velSens", 0);
        auto th = run (hb, 2.5, { { 0.0, 0, 83, 1.0f } });
        std::printf ("  hard-kneed box (a stiffness JUMP, below the brush floor): floor %.1f dB — the knee must be rounded over the floor\n",
                     aliasFloor (th.L, SR / 2, actualF (th.L, SR / 2, SR / 2 + 32768, 987.77, SR), SR));
    }

    //--------------------------------------------------------------------------
    head ("9  unison: equal energy is zero detune; an isochronous bowl is zero detune at ANY spread");
    {
        Engine a; fresh (a); setP (a, "unison", 1.0f); setP (a, "spread", 0.0f);
        run (a, 1.0, { { 0.0, 0, 45, 0.8f } });
        double dmax = 0;
        for (int b = 1; b < 4; ++b) dmax = std::max (dmax, std::abs (1200.0 * std::log2 (a.ballPeriod (0, b) / a.ballPeriod (0, 0))));
        std::printf ("  parabola, spread 0: worst detune %.4f c\n", dmax);
        ok (dmax < 0.5, "equal-energy unison: zero detune");
        Engine b; fresh (b); setP (b, "unison", 1.0f); setP (b, "spread", 1.0f);
        run (b, 1.0, { { 0.0, 0, 45, 0.8f } });
        dmax = 0;
        for (int k = 1; k < 4; ++k) dmax = std::max (dmax, std::abs (1200.0 * std::log2 (b.ballPeriod (0, k) / b.ballPeriod (0, 0))));
        std::printf ("  parabola, spread 1: worst detune %.4f c (energies differ; the balls' mutual push is the rest)\n", dmax);
        ok (dmax < 30.0, "isochronous bowl: spread scatters energy, and little pitch");
        Engine c; fresh (c); fillAll (c.terrain(), [] (float x) { return boxBowl (x); }); setP (c, "unison", 1.0f); setP (c, "spread", 1.0f);
        run (c, 1.0, { { 0.0, 0, 45, 0.8f } });
        dmax = 0;
        for (int k = 1; k < 4; ++k) dmax = std::max (dmax, std::abs (1200.0 * std::log2 (c.ballPeriod (0, k) / c.ballPeriod (0, 0))));
        std::printf ("  box, spread 1: worst detune %.1f c (a box is not isochronous: spread IS detune here)\n", dmax);
        ok (dmax > 10.0, "non-isochronous bowl: spread becomes detune");
    }

    //--------------------------------------------------------------------------
    head ("9b  DETUNE: the balls' clocks, fanned; zero is bit-identical");
    {
        Engine a; fresh (a); setP (a, "unison", 1.0f); setP (a, "spread", 0.0f); setP (a, "detune", 0.0f);
        Engine b; fresh (b); setP (b, "unison", 1.0f); setP (b, "spread", 0.0f);
        auto ta = run (a, 1.0, { { 0.0, 0, 45, 0.8f } });
        auto tb = run (b, 1.0, { { 0.0, 0, 45, 0.8f } });
        ok (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "DETUNE 0 is memcmp-identical to no detune at all");
        /*  ballPeriod is measured in BALL TIME, and a detuned ball's clock
            scales that time too — so its period reads 2*pi whatever the
            detune, and the first version of this check measured exactly
            nothing. Detune is heard as BEATING; measure the beating. */
        auto beat = [] (float det) {
            Engine e; fresh (e); setP (e, "unison", 1.0f); setP (e, "spread", 0.0f); setP (e, "detune", det);
            auto t = run (e, 3.0, { { 0.0, 0, 45, 0.8f } });
            double lo = 1e9, hi = 0;
            for (double w = 0.5; w < 2.9; w += 0.1) { const double r = t.rms (w, w + 0.1); lo = std::min (lo, r); hi = std::max (hi, r); }
            return hi / std::max (1e-9, lo);
        };
        const double b0 = beat (0.0f), b4 = beat (0.4f);
        /*  With four detuned balls the peak of the sum is a blur between the
            partials — a fair measurement of nothing. The claim is about the
            PLAYED ball, whose fan is exactly zero, so play exactly it. */
        Engine c; fresh (c); setP (c, "unison", 0.0f); setP (c, "detune", 1.0f);
        auto tc = run (c, 2.0, { { 0.0, 0, 45, 0.8f } });
        const double f0 = pitchNear (tc.L, SR / 2, SR / 2 + 32768, 110.0, SR, 120);
        std::printf ("  envelope swing over 2.4 s: detune 0 = %.2fx, detune 40 %% = %.2fx;"
                     "  one ball at DETUNE 100 %% is %+.3f c off the note\n", b0, b4, cents (f0, 110.0));
        ok (b0 < 1.15, "no detune: the unison does not beat");
        ok (b4 > 1.6, "DETUNE 40 %: the unison beats");
        ok (std::abs (cents (f0, 110.0)) < 1.0, "the played ball is never detuned, at any DETUNE (its fan is exactly zero)");
    }

    //--------------------------------------------------------------------------
    head ("10  the factory: every patch bounded, silent when silent, deterministic, and its own thing");
    {
        std::vector<std::vector<float>> takes;
        for (int i = 0; i < numFactory(); ++i)
        {
            Engine e; fresh (e); e.p = Params();
            Lanes l; factory (i).build (e.terrain(), l, e.p); e.setLanes (l);
            auto s = run (e, 1.0, {});
            ok (s.exactlyZero(), "factory silent with no note", factory (i).name);
            auto t = run (e, 5.0, { { 0.0, 0, 45, 0.8f }, { 1.5, 0, 52, 0.7f }, { 3.0, 1, 45, 0 }, { 3.5, 1, 52, 0 } });
            Engine e2; fresh (e2); e2.p = Params();
            Lanes l2; factory (i).build (e2.terrain(), l2, e2.p); e2.setLanes (l2);
            auto t2 = run (e2, 5.0, { { 0.0, 0, 45, 0.8f }, { 1.5, 0, 52, 0.7f }, { 3.0, 1, 45, 0 }, { 3.5, 1, 52, 0 } });
            std::printf ("  %-13s peak %.3f  rms %.4f  tail(4.5-5) %.5f\n", factory (i).name, t.peak(), t.rms (0.2, 3.0), t.rms (4.5, 5.0));
            ok (t.finite() && t.peak() < 0.95f, "factory bounded and clear of the ceiling", factory (i).name);
            ok (t.rms (0.2, 3.0) > 0.01, "factory audible", factory (i).name);
            ok (std::memcmp (t.L.data(), t2.L.data(), t.L.size() * sizeof (float)) == 0, "factory deterministic", factory (i).name);
            takes.push_back (t.L);
        }
        int alike = 0;
        for (size_t a = 0; a < takes.size(); ++a)
            for (size_t b = a + 1; b < takes.size(); ++b)
            {
                double d = 0, r = 0;
                for (size_t k = SR / 4; k < (size_t) SR * 2; ++k) { d += (double) (takes[a][k] - takes[b][k]) * (takes[a][k] - takes[b][k]); r += (double) takes[a][k] * takes[a][k]; }
                if (d < 0.05 * r) { ++alike; std::printf ("  ALIKE: %s ~ %s (%.3f)\n", factory ((int) a).name, factory ((int) b).name, d / r); }
            }
        ok (alike == 0, "no two factory patches alike");
    }

    //--------------------------------------------------------------------------
    head ("10b  THE STARTERS ARE SAFE: in tune at any strike, and level with each other");
    {
        /*  The promise a starter makes is that it can be played without
            learning the instrument first. Stated as a number: A2 within a few
            cents whether the key is brushed or hit, on every one of them.
            Tune-locked bowls hold it by construction; the box and pit
            starters hold it because SERVO measures the period and corrects. */
        double worstC = 0; const char* worstName = "";
        double rmsLo = 1e9, rmsHi = 0;
        for (int i = 0; i < numFactory(); ++i)
        {
            if (std::strcmp (factory (i).group, "STARTERS") != 0) continue;
            double c[2] = { 0, 0 }, r[2] = { 0, 0 }; float det = 0;
            for (int hard = 0; hard < 2; ++hard)
            {
                Engine e; fresh (e); e.p = Params();
                Lanes l; factory (i).build (e.terrain(), l, e.p); e.setLanes (l);
                /*  The patch's own unison detune is set aside for the tuning
                    measurement: it deliberately spreads the partials, so the
                    peak of the sum is not the note. Detune has its own check
                    in section 9b; what is claimed here is the NOTE. */
                det = e.p.detune; e.p.detune = 0.0f;
                auto t = run (e, 2.2, { { 0.0, 0, 45, hard ? 1.0f : 0.25f }, { 1.8, 1, 45, 0 } });
                const size_t a = (size_t) (0.6 * SR), b = a + 32768;
                c[hard] = cents (pitchNear (t.L, a, b, 110.0, SR, 700), 110.0);
                r[hard] = t.rms (0.3, 1.4);
                ok (t.finite() && t.peak() < 0.95f, "starter bounded", factory (i).name);
            }
            const double w = std::max (std::abs (c[0]), std::abs (c[1]));
            if (w > worstC) { worstC = w; worstName = factory (i).name; }
            rmsLo = std::min (rmsLo, r[1]); rmsHi = std::max (rmsHi, r[1]);
            std::printf ("  %-15s soft %+6.2f c   hard %+6.2f c   drift %5.2f c   rms %.3f   detune %2.0f c\n",
                         factory (i).name, c[0], c[1], c[1] - c[0], r[1], det * 50.0f);
            ok (std::abs (c[0]) < 6.0 && std::abs (c[1]) < 6.0, "starter in tune at both strikes", factory (i).name);
            ok (std::abs (c[1] - c[0]) < 5.0, "starter does not bend with the strike", factory (i).name);
            ok (r[1] > 0.03, "starter audible", factory (i).name);
        }
        std::printf ("  worst of all: %.2f cents (%s);  loudest / quietest rms ratio %.2f\n",
                     worstC, worstName, rmsHi / std::max (1e-9, rmsLo));
        ok (worstC < 6.0, "every starter is in tune within 6 cents");
        ok (rmsHi / std::max (1e-9, rmsLo) < 3.2, "the starters are level with each other within 10 dB");
        ok (numStarters() >= 16, "there are at least sixteen starters");
    }

    //--------------------------------------------------------------------------
    head ("11  the world-mod bus: neutral is bit-identical, live is not");
    {
        Engine a; fresh (a); Engine b; fresh (b); Engine c; fresh (c);
        b.setWorldMod (0, 0, 0, 0, 1, 0);
        c.setWorldMod (12, 0, 0, 0, 1, 0);
        setP (a, "unison", 0.67f); setP (b, "unison", 0.67f); setP (c, "unison", 0.67f);
        auto ta = run (a, 1.0, { { 0.0, 0, 45, 0.8f } });
        auto tb = run (b, 1.0, { { 0.0, 0, 45, 0.8f } });
        auto tc = run (c, 1.0, { { 0.0, 0, 45, 0.8f } });
        ok (std::memcmp (ta.L.data(), tb.L.data(), ta.L.size() * sizeof (float)) == 0, "neutral bus: memcmp-identical");
        ok (std::memcmp (ta.L.data(), tc.L.data(), ta.L.size() * sizeof (float)) != 0, "12 cents of bus detune changes the sound");
    }

    //--------------------------------------------------------------------------
    head ("12  polyphony, mono, legato, sustain, bend, rates");
    {
        Engine e; fresh (e);
        auto t = run (e, 2.0, { { 0.0, 0, 45, 0.8f }, { 0.1, 0, 49, 0.8f }, { 0.2, 0, 52, 0.8f }, { 1.0, 1, 45, 0 }, { 1.0, 1, 49, 0 }, { 1.0, 1, 52, 0 } });
        ok (t.finite() && t.peak() <= 1.0f, "a chord is bounded");
        ok (t.rms (1.9, 2.0) < t.rms (0.5, 0.9) * 0.5, "released chord decays");
        Engine m; fresh (m); setP (m, "voiceMode", 1.0f); setP (m, "glide", 0.3f);
        auto tm = run (m, 2.0, { { 0.0, 0, 45, 0.8f }, { 0.5, 0, 57, 0.8f }, { 1.0, 1, 57, 0 }, { 1.5, 1, 45, 0 } });
        ok (tm.finite() && m.activeVoices() <= 1, "mono keeps one voice");
        const double fm = pitchNear (tm.L, (size_t) (1.2 * SR), (size_t) (1.2 * SR) + 8192, 110.0, SR, 1300);
        std::printf ("  mono: pitch at 0.3 s %.1f, 0.7 s %.1f, 1.2 s %.1f Hz (110 / 220 / 110 wanted)\n",
                     pitchNear (tm.L, (size_t) (0.3 * SR), (size_t) (0.3 * SR) + 8192, 110.0, SR, 1300),
                     pitchNear (tm.L, (size_t) (0.7 * SR), (size_t) (0.7 * SR) + 8192, 220.0, SR, 1300), fm);
        ok (std::abs (cents (fm, 110.0)) < 30.0, "mono returns to the held note");
        //  legato: an overlapping note does not restrike (no energy jump)
        Engine lg; fresh (lg); setP (lg, "voiceMode", 1.0f); setP (lg, "glide", 0.2f);
        lg.noteOn (45, 0.8f);
        { std::vector<float> L (BLK), R (BLK); for (int b = 0; b < SR / 4 / BLK; ++b) lg.process (L.data(), R.data(), BLK);
          const double e0 = lg.ballEnergy (0, 0); lg.noteOn (52, 0.8f);
          for (int b = 0; b < 8; ++b) lg.process (L.data(), R.data(), BLK);
          const double e1 = lg.ballEnergy (0, 0);
          std::printf ("  legato overlap: energy %.4f -> %.4f\n", e0, e1);
          ok (std::abs (e1 - e0) < 0.02, "legato overlap does not restrike"); }
        Engine s; fresh (s);
        s.setSustain (true);
        auto ts = run (s, 2.0, { { 0.0, 0, 45, 0.8f }, { 0.3, 1, 45, 0 } });
        ok (ts.rms (1.5, 2.0) > 0.5 * ts.rms (0.5, 1.0), "sustain pedal holds the note");
        Engine bd; fresh (bd);
        auto tb = run (bd, 1.5, { { 0.0, 0, 45, 0.8f }, { 0.5, 2, 0, 2.0f } });
        const double fb = pitchNear (tb.L, SR, SR + 16384, 110.0 * std::pow (2.0, 2.0 / 12.0), SR);
        std::printf ("  bend +2 st: %.2f Hz (%+.1f c of 123.47)\n", fb, cents (fb, 110.0 * std::pow (2.0, 2.0 / 12.0)));
        ok (std::abs (cents (fb, 110.0 * std::pow (2.0, 2.0 / 12.0))) < 3.0, "pitch bend lands");
        for (int sr : { 44100, 96000 })
        {
            Engine r; fresh (r, sr);
            std::vector<float> L (BLK), R (BLK);
            r.noteOn (45, 0.8f);
            double pk = 0; bool fin = true;
            for (int b = 0; b < sr / BLK; ++b) { r.process (L.data(), R.data(), BLK); for (float v : L) { pk = std::max (pk, (double) std::abs (v)); fin = fin && std::isfinite (v); } }
            ok (fin && pk <= 1.0 && pk > 0.05, "other sample rate OK", sr == 44100 ? "44100" : "96000");
        }
    }

    //--------------------------------------------------------------------------
    head ("13  cost");
    {
        Engine e; fresh (e); setP (e, "unison", 1.0f); setP (e, "friction", 0.0f); setP (e, "rock", 0.3f);
        std::vector<float> L (BLK), R (BLK);
        for (int n = 0; n < 12; ++n) e.noteOn (36 + n * 3, 0.8f);
        const auto t0 = std::chrono::steady_clock::now();
        const int nb = 5 * SR / BLK;
        for (int b = 0; b < nb; ++b) e.process (L.data(), R.data(), BLK);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  12 voices x 4 balls, 4x (the default): %.1f %% of one core\n", 100.0 * secs / 5.0);
        ok (secs / 5.0 < 0.40, "48 balls at 4x (the most the instrument can run) under 40 % of a core");
        Engine e2; fresh (e2); setP (e2, "unison", 1.0f); setP (e2, "quality", 0.0f); setP (e2, "friction", 0.0f); setP (e2, "rock", 0.3f);
        for (int n = 0; n < 12; ++n) e2.noteOn (36 + n * 3, 0.8f);
        const auto t1 = std::chrono::steady_clock::now();
        for (int b = 0; b < nb; ++b) e2.process (L.data(), R.data(), BLK);
        const double s2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
        std::printf ("  12 voices x 4 balls, 2x: %.1f %% of one core\n", 100.0 * s2 / 5.0);
        ok (s2 / 5.0 < 0.24, "48 balls at 2x under 24 % of a core");
        Engine e1; fresh (e1); setP (e1, "friction", 0.0f);
        for (int n = 0; n < 4; ++n) e1.noteOn (40 + n * 5, 0.8f);
        const auto t2 = std::chrono::steady_clock::now();
        for (int b = 0; b < nb; ++b) e1.process (L.data(), R.data(), BLK);
        const double s1 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t2).count();
        std::printf ("  4 voices x 1 ball, 4x (an ordinary chord): %.1f %% of one core\n", 100.0 * s1 / 5.0);
        ok (s1 / 5.0 < 0.05, "an ordinary four-note chord under 5 % of a core");
    }


    /*  A PANIC LETS GO OF THE PEDAL.  Dropping the gates without releasing
        the sustain pedal silences the note that is ringing and leaves the
        next one held for ever by a pedal nobody is pressing -- and the
        panel's PANIC button routes here too, so there is no way out.
        Measured against a control engine that never touched the pedal, so
        this holds whatever the default patch does. */
    {
        double t[2] = { 0, 0 };
        for (int pass = 0; pass < 2; ++pass)
        {
            Engine e; fresh (e);
            if (pass == 0) { e.setSustain (true); e.allNotesOff(); }
            std::vector<float> L (BLK), R (BLK);
            e.noteOn (57, 0.8f);
            for (int k = 0; k < 40; ++k) e.process (L.data(), R.data(), BLK);
            e.noteOff (57);
            for (int k = 0; k < 400; ++k) e.process (L.data(), R.data(), BLK);
            for (int i = 0; i < BLK; ++i) t[pass] = std::max (t[pass], (double) std::abs (L[(size_t) i]));
        }
        ok (t[0] <= t[1] + 1.0e-4, "a panic lets go of the pedal: a note played afterwards still stops");
    }

    std::printf ("\n%d checks, %d failed — %s\n", checks, fails, fails ? "SEE ABOVE" : "ALL CLEAR");
    return fails ? 1 : 0;
}
