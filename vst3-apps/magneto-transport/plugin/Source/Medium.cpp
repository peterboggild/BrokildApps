#include "Medium.h"
#include "Playback.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#if defined (_M_X64) || defined (__SSE2__)
 #include <emmintrin.h>
 #define MT_SSE 1
#endif

namespace mt
{
namespace
{
constexpr double PI = 3.14159265358979323846;

// integral of e^(-c y) from a to b
inline double expInt (double c, double a, double b)
{
    if (c < 1.0e-9) return b - a;
    return (std::exp (-c * a) - std::exp (-c * b)) / c;
}

//------------------------------------------------------------------------------ shaper tables
struct ShaperTables
{
    static constexpr int K = HeadShaper::K, NK = 2 * K + 1;
    static constexpr int NTAU = 57, NBOX = 49, NPH = 64;
    static constexpr double T0 = 0.02, TMAX = 24.0, B0 = 0.02, BMAX = 16.0;
    double rT = 1.0, rB = 1.0;
    float spc[NTAU][NBOX][NK] {};
    float fd[NPH + 1][8] {};

    static double tauAt (int i, double r) { return i == 0 ? 0.0 : T0 * std::pow (r, i - 1); }
    static double boxAt (int i, double r) { return i == 0 ? 0.0 : B0 * std::pow (r, i - 1); }

    ShaperTables()
    {
        rT = std::pow (TMAX / T0, 1.0 / (NTAU - 2));
        rB = std::pow (BMAX / B0, 1.0 / (NBOX - 2));
        // The taps are the exact response's inverse transform, exp(-w tau) sinc(w B / 2)
        // over the whole band (binning the continuous kernel instead adds a sample's box:
        // -0.6 dB at 20 kHz at 96 kHz). The Cauchy kernel's tails beyond +-K hold its cusp
        // at DC (the bass part of a dropout's loss); their mass goes back in as a raised
        // cosine over the kernel, a lobe below about 3 kHz at 96 kHz, so the top is exact
        // and DC is exactly unity.
        constexpr int NQ = 768;
        double cw[NQ], wq[NQ];
        for (int q = 0; q < NQ; ++q) { cw[q] = PI * (q + 0.5) / NQ; wq[q] = PI / NQ; }
        double fill[NK], fsum = 0.0;
        for (int j = -K; j <= K; ++j) { fill[j + K] = 0.5 * (1.0 + std::cos (PI * j / (double) (K + 1))); fsum += fill[j + K]; }
        for (double& x : fill) x /= fsum;
        for (int it = 0; it < NTAU; ++it)
            for (int ib = 0; ib < NBOX; ++ib)
            {
                const double tau = tauAt (it, rT), B = boxAt (ib, rB);
                double v[NK], s = 0.0;
                if (tau <= 0.0 && B <= 0.0) { for (int j = 0; j < NK; ++j) v[j] = 0.0; v[K] = 1.0; s = 1.0; }
                else
                {
                    double H[NQ];
                    for (int q = 0; q < NQ; ++q)
                    {
                        const double x = 0.5 * cw[q] * B;
                        H[q] = std::exp (-cw[q] * tau) * ((x < 1.0e-12) ? 1.0 : std::sin (x) / x);
                    }
                    for (int j = 0; j <= K; ++j)
                    {
                        double a = 0.0;
                        for (int q = 0; q < NQ; ++q) a += H[q] * std::cos (cw[q] * j) * wq[q];
                        a /= PI;
                        v[K + j] = v[K - j] = a;
                    }
                    for (int j = 0; j < NK; ++j) s += v[j];
                    for (int j = 0; j < NK; ++j) v[j] += (1.0 - s) * fill[j];
                    s = 1.0;
                }
                for (int j = 0; j < NK; ++j) spc[it][ib][j] = (float) (v[j] / s);
            }
        // fractional delay: Kaiser-windowed sinc, 8 taps (i = -3..4), beta 6
        auto I0 = [] (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 40; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double beta = 6.0, i0b = I0 (beta);
        for (int p = 0; p <= NPH; ++p)
        {
            const double f = (double) p / NPH;
            double v[8], s = 0.0;
            for (int i = -3; i <= 4; ++i)
            {
                const double x = i - f;
                const double sinc = std::abs (x) < 1.0e-12 ? 1.0 : std::sin (PI * x) / (PI * x);
                const double r = x / 4.0;
                const double win = std::abs (r) < 1.0 ? I0 (beta * std::sqrt (1.0 - r * r)) / i0b : 0.0;
                v[i + 3] = sinc * win; s += v[i + 3];
            }
            for (int i = 0; i < 8; ++i) fd[p][i] = (float) (v[i] / s);
        }
    }
    static const ShaperTables& get() { static const ShaperTables t; return t; }
};
} // namespace

//------------------------------------------------------------------------------ noise colour
double noiseColourTarget (int l, double k, double dsp)
{
    const double top = PB_SPACING_UM + SLAB_UM * l, bot = top + SLAB_UM;
    const double cont = expInt (2.0 * k, top, bot);                 // every depth on its own
    const double slab = expInt (k, top + dsp, bot + dsp);           // the slab's column, coherent
    return std::sqrt (cont * SLAB_UM / std::max (slab * slab, 1.0e-300));
}

void noiseColour (double dsp, double (&g)[4][3])
{
    for (int l = 0; l < 4; ++l)
    {
        const double c = 0.5 * (double) (1 << l);
        const double top = PB_SPACING_UM + SLAB_UM * l;
        // fit where this slab can still be heard: below its own cell Nyquist, where
        // its spacing loss is under 40 dB, and below 3 /um (20 kHz at 4.75 cm/s is 2.6)
        const double kmax = std::min ({ 0.9 * PI / c, 4.6 / top, 3.0 });
        double a11 = 0, a12 = 0, a22 = 0, b1 = 0, b2 = 0;
        const int n = 64;
        for (int i = 0; i < n; ++i)
        {
            const double k = kmax * (i + 0.5) / n;
            const double x1 = 2.0 * (std::cos (k * c) - 1.0), x2 = 2.0 * (std::cos (2.0 * k * c) - 1.0);
            const double y = noiseColourTarget (l, k, dsp) - 1.0;
            a11 += x1 * x1; a12 += x1 * x2; a22 += x2 * x2; b1 += x1 * y; b2 += x2 * y;
        }
        const double det = a11 * a22 - a12 * a12;
        const double g1 = (std::abs (det) > 1e-30) ? (b1 * a22 - b2 * a12) / det : 0.0;
        const double g2 = (std::abs (det) > 1e-30) ? (a11 * b2 - a12 * b1) / det : 0.0;
        g[l][0] = 1.0 - 2.0 * g1 - 2.0 * g2; g[l][1] = g1; g[l][2] = g2;
    }
}

//------------------------------------------------------------------------------ dropouts
void DropoutCache::at (double x, uint64_t seed, double perM, double meanUm, double hole, double& dd, double& keep)
{
    dd = 0.0; keep = 1.0;
    if (perM <= 0.0) return;
    const int64_t s = (int64_t) std::floor (x / SEG);
    if (s != seg || std::abs (perM - rate) > 1.0e-3 * rate || meanUm != depthMean || hole != holeFrac)
    {
        seg = s; rate = perM; depthMean = meanUm; holeFrac = hole; n = 0;
        const double p = std::min (1.0, perM * 0.01 / 2.0);      // two candidates per centimetre
        for (int64_t q = s - 1; q <= s + 1; ++q)
            for (int e = 0; e < 2; ++e)
            {
                if (hashUni (key3 (q, seed, 2 * e)) >= p) continue;
                Ev& v = ev[n++];
                v.x0 = (q + hashUni (key3 (q, seed, 2 * e + 1))) * SEG;
                v.len = 200.0 * std::pow (10.0, hashUni (key3 (q, seed, 2 * e + 16)));   // 0.1 .. 1 mm
                v.depth = std::min (8.0, -meanUm * std::log (std::max (1.0e-9, hashUni (key3 (q, seed, 2 * e + 32)))));
                v.hole = (hashUni (key3 (q, seed, 2 * e + 48)) < hole) ? std::min (0.95, v.depth / 2.5) : 0.0;
            }
    }
    for (int i = 0; i < n; ++i)
    {
        const Ev& v = ev[i];
        const double d = x - v.x0;
        if (std::abs (d) >= 0.5 * v.len) continue;
        const double prof = 0.5 * (1.0 + std::cos (2.0 * PI * d / v.len));
        if (v.hole > 0.0) keep *= 1.0 - v.hole * prof;
        else dd += v.depth * prof;
    }
}

//------------------------------------------------------------------------------ the shaper
void HeadShaper::prepareTables() { (void) ShaperTables::get(); }

void HeadShaper::reset()
{
    std::memset (ringA, 0, sizeof (ringA));
    std::memset (ringB, 0, sizeof (ringB));
    w = 0;
    for (int ch = 0; ch < 2; ++ch) { identA[ch] = identB[ch] = true; gn[ch] = 1.0; offB[ch] = D0; }
}

void HeadShaper::set (int ch, double tau, double box, double shift, double gain)
{
    gn[ch] = gain;
    const ShaperTables& T = ShaperTables::get();
    identA[ch] = tau < 1.0e-4 && box < 1.0e-4;
    if (! identA[ch])
    {
        auto index = [] (double v, double v0, double r, int nmax, int& i, double& t) {
            double f;
            if (v <= 0.0) f = 0.0;
            else if (v < v0) f = v / v0;
            else f = 1.0 + std::log (v / v0) / std::log (r);
            f = std::clamp (f, 0.0, (double) (nmax - 1));
            i = std::min ((int) f, nmax - 2); t = f - i;
        };
        int it, ib; double tt, tb;
        index (tau, ShaperTables::T0, T.rT, ShaperTables::NTAU, it, tt);
        index (box, ShaperTables::B0, T.rB, ShaperTables::NBOX, ib, tb);
        const float* a = T.spc[it][ib];      const float* b = T.spc[it][ib + 1];
        const float* cc = T.spc[it + 1][ib]; const float* d = T.spc[it + 1][ib + 1];
        const float w00 = (float) ((1.0 - tt) * (1.0 - tb)), w01 = (float) ((1.0 - tt) * tb);
        const float w10 = (float) (tt * (1.0 - tb)), w11 = (float) (tt * tb);
        float* o = tapA[ch];
        for (int j = 0; j < ShaperTables::NK; ++j) o[j] = w00 * a[j] + w01 * b[j] + w10 * cc[j] + w11 * d[j];
        int lo = 0, hi = 2 * K;
        while (lo < hi && o[lo] == 0.0f) ++lo;
        while (hi > lo && o[hi] == 0.0f) --hi;
        a0[ch] = lo; a1[ch] = hi;
    }
    identB[ch] = std::abs (shift) < 1.0e-6;
    if (! identB[ch])
    {
        const double s = std::clamp (shift, -(double) DMAX, (double) DMAX - 1.0e-9);
        const int ip = (int) std::floor (s);
        const double fp = (s - ip) * ShaperTables::NPH;
        const int p0 = std::min ((int) fp, ShaperTables::NPH - 1);
        const double tp = fp - p0;
        // tap i weighs A(n - (D0 + ip + i - 3)); the window starts D0 + ip + 4 back
        for (int i = 0; i < 8; ++i) rtapB[ch][7 - i] = (float) ((1.0 - tp) * T.fd[p0][i] + tp * T.fd[p0 + 1][i]);
        offB[ch] = D0 + ip + 4;
    }
}

namespace
{
    inline double dotf (const float* t, const float* x, int n)
    {
        double s = 0.0;
        int q = 0;
       #ifdef MT_SSE
        __m128 acc = _mm_setzero_ps();
        for (; q + 4 <= n; q += 4) acc = _mm_add_ps (acc, _mm_mul_ps (_mm_loadu_ps (t + q), _mm_loadu_ps (x + q)));
        alignas (16) float a4[4]; _mm_store_ps (a4, acc);
        s = (double) a4[0] + a4[1] + a4[2] + a4[3];
       #endif
        for (; q < n; ++q) s += (double) t[q] * x[q];
        return s;
    }
}

void HeadShaper::push (double inL, double inR, double& outL, double& outR)
{
    const double in[2] = { inL, inR };
    double y[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        float* ra = ringA[ch];
        ra[w] = ra[w + RING] = (float) in[ch];
        // stage A: x(n - 2K + q), q over the kernel's nonzero span
        const double av = identA[ch] ? ra[w + RING - K]
                                     : dotf (tapA[ch] + a0[ch], ra + (w + RING - 2 * K + a0[ch]), a1[ch] - a0[ch] + 1);
        float* rb = ringB[ch];
        rb[w] = rb[w + RING] = (float) av;
        y[ch] = identB[ch] ? rb[w + RING - D0] : dotf (rtapB[ch], rb + (w + RING - offB[ch]), 8);
    }
    outL = y[0] * gn[0]; outR = y[1] * gn[1];
    w = (w + 1) & (RING - 1);
}

} // namespace mt
