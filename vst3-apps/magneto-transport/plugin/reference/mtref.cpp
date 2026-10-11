// Magneto Transport - OFFLINE REFERENCE MODEL (stage 2). Does not ship.
//
// A Preisach ferric coating (docs/physics/03-hysteresis.md) recorded by a
// Karlqvist head with explicit 150 kHz AC bias (04-record-process.md). Exact in
// the sense that matters: the Preisach state depends only on the field's turning
// points, and every turning point is found to ~1e-8 - there is no time step.
//
// Everything here is measured; the numbers go into 05-reference-results.md and
// are what stage 3's real-time model is fitted to and proved against.
//
//   mtref [out-dir] [--fast]

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#ifdef _OPENMP
 #include <omp.h>
#endif

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double MU0 = 4.0e-7 * PI;

//------------------------------------------------------------------------------ the stock
struct Stock
{
    const char* name;
    double HcKAm, MrKAm;       // data sheet
    double deltaUm;            // coating thickness
    double molRms;             // data sheet MOL, nWb/m rms (3 % third harmonic, 1 kHz)
    double sigC;               // lognormal width of the particle coercivity (assumed, see 03-hysteresis.md)
    double fwhmU;              // full width of the interaction field, Hc (assumed)
    double nu;                 // tail shape of the interaction field: FITTED to the MOL
    double S;                  // bulk squareness, orientation: only sets Ms for the flux scale
    double Ms() const { return MrKAm * 1.0e3 / S; }                       // A/m
    double fluxSat() const { return MU0 * Ms() * deltaUm * 1.0e-6 * 1.0e9; } // nWb/m at M = Ms, full thickness (x S = at Mr)
};

//------------------------------------------------------------------------------ the medium: Everett function
struct Medium
{
    static constexpr double HS = 6.0;      // beyond this (in Hc) no hysteron lives; fields clamp
    double sigC = 0.12;
    double nu = 10.0, gam = 0.1;   // Student-t interaction field: degrees of freedom, scale
    void setU (double nuIn, double fwhm)
    {
        nu = nuIn;
        gam = fwhm / (2.0 * std::sqrt (nu * (std::pow (2.0, 2.0 / (nu + 1.0)) - 1.0)));
    }
    double fwhmU() const { return 2.0 * gam * std::sqrt (nu * (std::pow (2.0, 2.0 / (nu + 1.0)) - 1.0)); }
    double pU0() const { return std::exp (std::lgamma (0.5 * (nu + 1.0)) - std::lgamma (0.5 * nu)) / (std::sqrt (nu * PI) * gam); }

    double pcW (double w) const { return std::exp (-0.5 * w * w / (sigC * sigC)) / (sigC * std::sqrt (2.0 * PI)); }

    // regularised incomplete beta, continued fraction (Numerical Recipes)
    static double betacf (double a, double b, double x)
    {
        const double qab = a + b, qap = a + 1.0, qam = a - 1.0;
        double c = 1.0, d = 1.0 - qab * x / qap;
        if (std::abs (d) < 1e-300) d = 1e-300;
        d = 1.0 / d; double h = d;
        for (int m = 1; m <= 300; ++m)
        {
            const int m2 = 2 * m;
            double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
            d = 1.0 + aa * d; if (std::abs (d) < 1e-300) d = 1e-300; c = 1.0 + aa / c; if (std::abs (c) < 1e-300) c = 1e-300;
            d = 1.0 / d; h *= d * c;
            aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
            d = 1.0 + aa * d; if (std::abs (d) < 1e-300) d = 1e-300; c = 1.0 + aa / c; if (std::abs (c) < 1e-300) c = 1e-300;
            d = 1.0 / d; const double del = d * c; h *= del;
            if (std::abs (del - 1.0) < 1e-15) break;
        }
        return h;
    }
    static double ibeta (double a, double b, double x)
    {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        const double bt = std::exp (std::lgamma (a + b) - std::lgamma (a) - std::lgamma (b) + a * std::log (x) + b * std::log (1.0 - x));
        if (x < (a + 1.0) / (a + b + 2.0)) return bt * betacf (a, b, x) / a;
        return 1.0 - bt * betacf (b, a, 1.0 - x) / b;
    }
    // CDF of the interaction field
    double PhiU (double x) const
    {
        const double t = x / gam;
        const double ib = ibeta (0.5 * nu, 0.5, nu / (nu + t * t));
        return t > 0.0 ? 1.0 - 0.5 * ib : 0.5 * ib;
    }

    // F(a,b) = int_0^{(a-b)/2} p_c(c) [Phi_u(a-c) - Phi_u(b+c)] dc, c = e^w so p_c dc is a Gaussian in w.
    double Fdirect (double a, double b) const
    {
        if (a <= b) return 0.0;
        const double whi = std::min (6.0 * sigC, std::log (0.5 * (a - b))), wlo = -6.0 * sigC;
        if (whi <= wlo) return 0.0;
        static const double xg[4] = { -0.861136311594053, -0.339981043584856, 0.339981043584856, 0.861136311594053 };
        static const double wg[4] = { 0.347854845137454, 0.652145154862546, 0.652145154862546, 0.347854845137454 };
        const int P = 32;
        const double h = (whi - wlo) / P;
        double s = 0.0;
        for (int p = 0; p < P; ++p)
        {
            const double c0 = wlo + (p + 0.5) * h;
            for (int k = 0; k < 4; ++k)
            {
                const double w = c0 + 0.5 * h * xg[k], c = std::exp (w);
                s += wg[k] * 0.5 * h * pcW (w) * (PhiU (a - c) - PhiU (b + c));
            }
        }
        return s;
    }

    // tabulated on a 0.01 grid over [-HS, HS]^2, bicubic between nodes
    int N = 0; double dh = 0.01; std::vector<double> T;
    void build()
    {
        N = (int) std::lround (2.0 * HS / dh) + 1;
        T.assign ((size_t) N * N, 0.0);
       #pragma omp parallel for schedule(dynamic, 8)
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                const double a = -HS + i * dh, b = -HS + j * dh;
                T[(size_t) i * N + j] = (a > b) ? Fdirect (a, b) : 0.0;
            }
    }
    inline double at (int i, int j) const
    {
        i = std::clamp (i, 0, N - 1); j = std::clamp (j, 0, N - 1);
        return T[(size_t) i * N + j];
    }
    static inline double cr (double p0, double p1, double p2, double p3, double t)
    {
        return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
    }
    inline double F (double a, double b) const
    {
        if (a <= b) return 0.0;
        a = std::clamp (a, -HS, HS); b = std::clamp (b, -HS, HS);
        const double fi = (a + HS) / dh, fj = (b + HS) / dh;
        const int i = (int) std::floor (fi), j = (int) std::floor (fj);
        const double ti = fi - i, tj = fj - j;
        double col[4];
        for (int m = -1; m <= 2; ++m)
            col[m + 1] = cr (at (i - 1, j + m), at (i, j + m), at (i + 1, j + m), at (i + 2, j + m), ti);
        return cr (col[0], col[1], col[2], col[3], tj);
    }
};

//------------------------------------------------------------------------------ the state of one layer of one cell
struct Stair
{
    std::vector<double> v;       // alternating extrema: M1, m1, M2, m2, ... (maxima falling, minima rising)
    double cur = -Medium::HS;    // current field; even size = rising from the last minimum
    void reset() { v.clear(); cur = -Medium::HS; }
    /*  The AC-demagnetised state: what an erase head (or a bulk eraser) leaves,
        and what the real-time tape calls M = 0. A default staircase is NEGATIVE
        SATURATION - the first reference run measured everything on tape in that
        state, which is not what a recorder ever records onto. */
    void demag (double amp0 = Medium::HS, int steps = 400)
    {
        reset();
        for (int i = 0; i < steps; ++i) { const double A = amp0 * (1.0 - (double) i / steps); apply (i % 2 ? -A : A); }
        apply (0.0);
    }
    void apply (double h)
    {
        h = std::clamp (h, -Medium::HS, Medium::HS);
        const bool rising = (v.size() % 2 == 0);
        if (rising)
        {
            if (h >= cur) { while (v.size() >= 2 && h >= v[v.size() - 2]) { v.pop_back(); v.pop_back(); } }
            else { v.push_back (cur); while (v.size() >= 2 && h <= v[v.size() - 2]) { v.pop_back(); v.pop_back(); } }
        }
        else
        {
            if (h <= cur) { while (v.size() >= 2 && h <= v[v.size() - 2]) { v.pop_back(); v.pop_back(); } }
            else { v.push_back (cur); while (v.size() >= 2 && h >= v[v.size() - 2]) { v.pop_back(); v.pop_back(); } }
        }
        cur = h;
    }
    double M (const Medium& md) const
    {
        double m = -1.0, pm = -Medium::HS;
        const size_t n = v.size() / 2;
        for (size_t k = 0; k < n; ++k)
        {
            const double Mk = v[2 * k], mk = v[2 * k + 1];
            m += 2.0 * (md.F (Mk, pm) - md.F (Mk, mk));
            pm = mk;
        }
        if (v.size() % 2) { const double Mk = v.back(); m += 2.0 * (md.F (Mk, pm) - md.F (Mk, cur)); }
        else               m += 2.0 * md.F (cur, pm);
        return m;
    }
};

//------------------------------------------------------------------------------ heads
struct HeadGeom { double g, d; };   // um
inline double karlqvist (double g, double x, double y)
{
    return (std::atan ((0.5 * g + x) / y) + std::atan ((0.5 * g - x) / y)) / PI;
}

//------------------------------------------------------------------------------ the tape under test
struct Tape
{
    double pitch = 0.5;          // um per cell
    int ncell = 0, nl = 8;
    double delta = 14.0;         // coating, um
    std::vector<double> y;       // layer centres from the tape SURFACE
    std::vector<Stair> st;       // [cell * nl + layer]
    void init (int cells, int layers, double coating)
    {
        ncell = cells; nl = layers; delta = coating;
        y.resize ((size_t) nl);
        for (int l = 0; l < nl; ++l) y[(size_t) l] = (l + 0.5) * delta / nl;
        // fresh tape is demagnetised (one prepared state, copied)
        Stair d; d.demag();
        st.assign ((size_t) ncell * nl, d);
    }
    Stair& at (int c, int l) { return st[(size_t) c * nl + l]; }
    const Stair& at (int c, int l) const { return st[(size_t) c * nl + l]; }
};

//------------------------------------------------------------------------------ one pass under a head
struct Pass
{
    HeadGeom head;
    double hb = 0.0, fb = 150.0e3;       // bias deep-gap amplitude (Hc), Hz
    double hs = 0.0;                     // signal deep-gap field at s = 1 (Hc)
    std::function<double (double)> sig;  // s(t), t in seconds
    double v = 38.1;                     // cm/s
};

// where the field at the deepest layer has fallen to 0.25 Hc
static double zoneHalfWidth (const Pass& P, double yDeep)
{
    const double amp = P.hb + std::abs (P.hs);
    if (amp <= 0.0) return P.head.g;
    double lo = 0.5 * P.head.g, hi = 1.0e6;
    for (int it = 0; it < 100; ++it)
    {
        const double m = 0.5 * (lo + hi);
        if (karlqvist (P.head.g, m, yDeep) * amp > 0.25) lo = m; else hi = m;
    }
    return hi + P.head.g;
}

/*  One layer of one cell through the head: every bias half-cycle's maximum and
    minimum of the field at the cell, refined to ~1e-8, fed to the staircase in
    time order. The cell is at x = v t - u; y is its depth below the head. */
static void advect (Stair& S, double y, double u, const Pass& P, double X, double vUm)
{
    const double Th = 0.5 / P.fb;
    const bool hasSig = (bool) P.sig && P.hs != 0.0;
    const double t0 = (u - X) / vUm, t1 = (u + X) / vUm;
    auto H = [&] (double t)
    {
        const double x = vUm * t - u;
        const double drive = P.hb * std::sin (2.0 * PI * P.fb * t) + (hasSig ? P.hs * P.sig (t) : 0.0);
        return karlqvist (P.head.g, x, y) * drive;
    };
    auto refine = [&] (const double* tt, const double* hh, int b, bool wantMax)
    {
        if (b == 0 || b == 8) return hh[b];
        double x0 = tt[b - 1], x1 = tt[b], x2 = tt[b + 1], f0 = hh[b - 1], f1 = hh[b], f2 = hh[b + 1];
        double best = f1, tbest = x1;
        for (int it = 0; it < 3; ++it)
        {
            const double den = (f0 - 2.0 * f1 + f2);
            if (std::abs (den) < 1.0e-300) break;
            const double tv = x1 - 0.5 * (x2 - x0) * (f2 - f0) / (2.0 * den);
            const double tcl = std::clamp (tv, x0, x2);
            const double fv = H (tcl);
            if (wantMax ? fv > best : fv < best) { best = fv; tbest = tcl; }
            const double dd = 0.25 * (x2 - x0);
            x0 = tbest - dd; x2 = tbest + dd; x1 = tbest;
            f0 = H (x0); f2 = H (x2); f1 = best;
        }
        return best;
    };
    const long long n0 = (long long) std::floor (t0 / Th), n1 = (long long) std::ceil (t1 / Th);
    for (long long n = n0; n < n1; ++n)
    {
        const double ta = std::max (n * Th, t0), tb = std::min ((n + 1) * Th, t1);
        if (tb <= ta) continue;
        double tt[9], hh[9];
        int bmax = 0, bmin = 0;
        for (int k = 0; k <= 8; ++k)
        {
            tt[k] = ta + (tb - ta) * k / 8.0;
            hh[k] = H (tt[k]);
            if (hh[k] > hh[bmax]) bmax = k;
            if (hh[k] < hh[bmin]) bmin = k;
        }
        const double vmax = refine (tt, hh, bmax, true), vmin = refine (tt, hh, bmin, false);
        if (bmax < bmin) { S.apply (vmax); S.apply (vmin); }
        else             { S.apply (vmin); S.apply (vmax); }
    }
}

static void runPass (Tape& tape, const Pass& P)
{
    const double vUm = P.v * 1.0e4;                     // um/s
    const double yDeep = P.head.d + tape.y.back();
    const double X = zoneHalfWidth (P, yDeep);
   #pragma omp parallel for schedule(dynamic, 64)
    for (int c = 0; c < tape.ncell; ++c)
        for (int l = 0; l < tape.nl; ++l)
            advect (tape.at (c, l), P.head.d + tape.y[(size_t) l], c * tape.pitch, P, X, vUm);
}

//------------------------------------------------------------------------------ readout
struct Readout
{
    const Medium& md; const Tape& tape; const Stock& stock;
    std::vector<double> M;      // [cell * nl + l]
    Readout (const Medium& m, const Tape& t, const Stock& s) : md (m), tape (t), stock (s)
    {
        M.resize (tape.st.size());
       #pragma omp parallel for schedule(static)
        for (int c = 0; c < tape.ncell; ++c)
            for (int l = 0; l < tape.nl; ++l)
                M[(size_t) c * tape.nl + l] = tape.at (c, l).M (md);
    }
    // depth-averaged flux per cell, nWb/m
    double flux (int c) const
    {
        double s = 0.0; for (int l = 0; l < tape.nl; ++l) s += M[(size_t) c * tape.nl + l];
        return stock.fluxSat() * stock.S * s / tape.nl;
    }
    // complex amplitude (nWb/m, peak) of frequency f in cells [c0, c0+n) of layer l (-1 = depth average), Hann
    std::complex<double> amp (double f, double v, int c0, int n, int l) const
    {
        std::complex<double> acc (0, 0); double wsum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * PI * i / (n - 1.0));
            const double t = (c0 + i) * tape.pitch / (v * 1.0e4);
            double x;
            if (l < 0) x = flux (c0 + i);
            else x = stock.fluxSat() * stock.S * M[(size_t) (c0 + i) * tape.nl + l];
            acc += w * x * std::polar (1.0, -2.0 * PI * f * t);
            wsum += w;
        }
        return 2.0 * acc / wsum;
    }
    // amplitude seen by a playback head (spacing dp, gap gp), nWb/m peak
    double playback (double f, double v, int c0, int n, double dp, double gp) const
    {
        const double k = 2.0 * PI * f / (v * 1.0e4);          // 1/um
        const double dy = tape.delta / tape.nl;
        std::complex<double> s (0, 0);
        for (int l = 0; l < tape.nl; ++l)
        {
            const double yl = dp + tape.y[(size_t) l];
            const double thick = (k * dy > 1e-9) ? std::sinh (0.5 * k * dy) / (0.5 * k * dy) : 1.0;
            s += amp (f, v, c0, n, l) * std::exp (-k * yl) * thick;
        }
        const double gap = (k * gp > 1e-9) ? std::sin (0.5 * k * gp) / (0.5 * k * gp) : 1.0;
        return std::abs (s) * gap / tape.nl;
    }
};

inline double dB (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-30)); }

//------------------------------------------------------------------------------ helpers for the measurements
struct Lab
{
    Stock stock; Medium md;
    HeadGeom rec { 10.0, 0.3 }, play { 2.5, 0.3 }, erase { 60.0, 0.3 };
    double v = 38.1, fb = 150.0e3;
    double pitch = 0.5; int NL = 8;
    double lengthUm = 4500.0;
    FILE* csv = nullptr;
    std::string outDir;

    int cells() const { return (int) std::lround (lengthUm / pitch); }

    // window of an integer number of periods starting at 10 % in
    void window (double f, int& c0, int& n) const
    {
        const double lam = v * 1.0e4 / f / pitch;              // cells per period
        const int total = cells();
        c0 = total / 10;
        const int periods = (int) std::floor ((total - 2 * c0) / lam);
        n = (int) std::lround (periods * lam);
    }

    Tape fresh() const { Tape t; t.pitch = pitch; t.init (cells(), NL, stock.deltaUm); return t; }

    static std::function<double (double)> sine (double f) { return [f] (double t) { return std::sin (2.0 * PI * f * t); }; }

    void record (Tape& t, double hb, double hs, double f) const
    {
        Pass P; P.head = rec; P.hb = hb; P.fb = fb; P.hs = hs; P.sig = sine (f); P.v = v;
        runPass (t, P);
    }
    void erasePass (Tape& t, double he) const
    {
        Pass P; P.head = erase; P.hb = he; P.fb = fb; P.hs = 0.0; P.v = v;
        runPass (t, P);
    }
    // rms flux (nWb/m) at f after a fresh record, depth-averaged and through the playback head
    void measure (double hb, double hs, double f, double& rmsAvg, double& rmsPlay, double* thd3 = nullptr, double* thd = nullptr) const
    {
        Tape t = fresh();
        record (t, hb, hs, f);
        Readout R (md, t, stock);
        int c0, n; window (f, c0, n);
        const double a1 = std::abs (R.amp (f, v, c0, n, -1));
        rmsAvg = a1 / std::sqrt (2.0);
        rmsPlay = R.playback (f, v, c0, n, play.d, play.g) / std::sqrt (2.0);
        if (thd3 || thd)
        {
            double s = 0.0, a3 = 0.0;
            for (int h = 2; h <= 10; ++h)
            {
                const double ah = std::abs (R.amp (h * f, v, c0, n, -1));
                s += ah * ah; if (h == 3) a3 = ah;
            }
            if (thd3) *thd3 = a3 / a1;
            if (thd)  *thd = std::sqrt (s) / a1;
        }
    }
};

static double now_s()
{
    using namespace std::chrono;
    return duration<double> (steady_clock::now().time_since_epoch()).count();
}

//------------------------------------------------------------------------------ the medium's own numbers
static double squarenessP (const Medium& m) { return 1.0 - 2.0 * m.Fdirect (Medium::HS, 0.0); }
static double sfdOf (const Medium& m, double& Hr)
{
    // DCD remanence from negative saturation: Mr(h) = -1 + 2[F(h,-HS) - F(h,0)]; width of its derivative
    const int n = 1500; const double dh = 3.0 / n;
    std::vector<double> d (n);
    double peak = 0.0; int ip = 0;
    for (int i = 1; i < n - 1; ++i)
    {
        const double h = i * dh;
        const double mr1 = -1.0 + 2.0 * (m.Fdirect (h + dh, -Medium::HS) - m.Fdirect (h + dh, 0.0));
        const double mr0 = -1.0 + 2.0 * (m.Fdirect (h - dh, -Medium::HS) - m.Fdirect (h - dh, 0.0));
        d[(size_t) i] = (mr1 - mr0) / (2.0 * dh);
        if (d[(size_t) i] > peak) { peak = d[(size_t) i]; ip = i; }
    }
    Hr = ip * dh;
    int lo = ip, hi = ip;
    while (lo > 1 && d[(size_t) lo] > 0.5 * peak) --lo;
    while (hi < n - 2 && d[(size_t) hi] > 0.5 * peak) ++hi;
    return (hi - lo) * dh / Hr;
}
// third harmonic of the IDEAL anhysteretic curve M = 2 Phi_u(s) - 1 driven to peak level mPeak (of Ms)
static double idealHd3 (const Medium& m, double mPeak)
{
    double lo = 0.0, hi = 20.0;
    for (int it = 0; it < 60; ++it) { const double sv = 0.5 * (lo + hi); (2.0 * m.PhiU (sv) - 1.0 < mPeak ? lo : hi) = sv; }
    const double A = 0.5 * (lo + hi);
    const int N = 1024; double c1 = 0, c3 = 0;
    for (int i = 0; i < N; ++i)
    {
        const double th = 2.0 * PI * (i + 0.5) / N, M = 2.0 * m.PhiU (A * std::sin (th)) - 1.0;
        c1 += M * std::sin (th); c3 += M * std::sin (3.0 * th);
    }
    return std::abs (c3 / c1);
}

//------------------------------------------------------------------------------ the alignment, as the sheet defines it
// 10 kHz playback output vs bias at a fixed small signal: its peak, found by a
// coarse scan then golden-section (the curve is unimodal).
static double peak10Bias (const Lab& L, double& best10)
{
    auto p10 = [&] (double hb) { double a, p; L.measure (hb, 0.08, 10000.0, a, p); return p; };
    const double grid[] = { 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0, 8.0 };
    const int NG = 12;
    int bi = 0; double bv = -1.0;
    for (int i = 0; i < NG; ++i) { const double v = p10 (grid[i]); if (v > bv) { bv = v; bi = i; } }
    double a = grid[std::max (0, bi - 1)], b = grid[std::min (NG - 1, bi + 1)];
    const double gr = 0.5 * (std::sqrt (5.0) - 1.0);
    double c = b - gr * (b - a), d = a + gr * (b - a), fc = p10 (c), fd = p10 (d);
    for (int it = 0; it < 14; ++it)
    {
        if (fc > fd) { b = d; d = c; fd = fc; c = b - gr * (b - a); fc = p10 (c); }
        else         { a = c; c = d; fc = fd; d = a + gr * (b - a); fd = p10 (d); }
    }
    best10 = std::max (fc, fd);
    return 0.5 * (a + b);
}
// operating bias: 10 kHz 3 dB past its peak (overbias)
static double operatingBias (const Lab& L, double bestB, double best10)
{
    double lo = bestB, hi = 40.0;
    for (int it = 0; it < 16; ++it)
    {
        const double hb = 0.5 * (lo + hi);
        double a10, p10; L.measure (hb, 0.08, 10000.0, a10, p10);
        (dB (p10 / best10) > -3.0 ? lo : hi) = hb;
    }
    return 0.5 * (lo + hi);
}
// the deep-gap signal field that records 250 nWb/m rms at 1 kHz
static double calibrateHs (const Lab& L, double hbOp)
{
    double lo = 0.001, hi = 5.0;
    for (int it = 0; it < 22; ++it)
    {
        const double hs = std::sqrt (lo * hi);
        double a, p; L.measure (hbOp, hs, 1000.0, a, p);
        (a < 250.0 ? lo : hi) = hs;
    }
    return std::sqrt (lo * hi);
}
// MOL: the level (dB re 250 nWb/m) at which the third harmonic reaches 3 %
static double molDb (const Lab& L, double hbOp, double hsRef)
{
    double lo = 0.0, hi = 22.0;
    for (int it = 0; it < 16; ++it)
    {
        const double lv = 0.5 * (lo + hi);
        double a, p, hd3; L.measure (hbOp, hsRef * std::pow (10.0, lv / 20.0), 1000.0, a, p, &hd3);
        (hd3 < 0.03 ? lo : hi) = lv;
    }
    double a, p; L.measure (hbOp, hsRef * std::pow (10.0, 0.5 * (lo + hi) / 20.0), 1000.0, a, p);
    return dB (a / 250.0);
}

//------------------------------------------------------------------------------ the stage 3 record model, measured
/*  Per real-time layer l (four of them, at halving resolution), per peak bias
    amplitude A the layer sees, per deep-gap signal field s held constant over
    the pass, per previous remanence M_old (prepared as an anhysteretic state):
    the new remanence G. And per (l, A): the small-signal recording zone, from a
    narrow pulse - its centroid downstream of the gap and its rms width - which
    is what gives the HF recording loss and the bias-dependent delay shift. */
static void writeTables (const Lab& Lin, double hbOp38, double hsRef38, const std::string& path)
{
    constexpr int NLR = 4, NA = 28, NS = 41, NM = 11, NV = 5, NSUB = 4;
    // Speeds a factor of 2 apart, so the real-time model interpolates in log speed
    // with a fixed step. The machine is ALIGNED at 19.05 cm/s, its nominal speed.
    const double speeds[NV] = { 4.7625, 9.525, 19.05, 38.1, 76.2 };
    constexpr int IALIGN = 2;
    const double yR[NLR] = { 0.125 * 14.0, 0.375 * 14.0, 0.625 * 14.0, 0.875 * 14.0 };
    // Each real-time layer stands for a SLAB of the coating, its tables averaged
    // over NSUB sub-depths (16 depths in all, finer than the converged 8). Point
    // samples at the slab centres were 1.4 dB off near the bias threshold.
    const double slab = 14.0 / NLR;
    auto ySub = [&] (int l, int k) { return Lin.rec.d + l * slab + (k + 0.5) * slab / NSUB; };

    double Ag[NA], Sg[NS], Mg[NM];
    for (int i = 0; i < NA; ++i) Ag[i] = 0.1 * std::pow (80.0, i / (NA - 1.0));            // 0.1 .. 8 Hc
    for (int i = 0; i < NS; ++i) { const double t = -1.0 + 2.0 * i / (NS - 1.0); Sg[i] = 2.5 * std::sinh (3.0 * t) / std::sinh (3.0); }
    for (int i = 0; i < NM; ++i) Mg[i] = -0.95 + 1.9 * i / (NM - 1.0);
    Sg[NS / 2] = 0.0; Mg[NM / 2] = 0.0;

    // The tape stores REMANENCE: index G by what the prepared state holds.
    double Mrem[NM], sOld[NM];
    std::vector<Stair> pre (NM);
    for (int im = 0; im < NM; ++im)
    {
        double lo = -20.0, hi = 20.0;
        for (int it = 0; it < 60; ++it) { const double sv = 0.5 * (lo + hi); (2.0 * Lin.md.PhiU (sv) - 1.0 < Mg[im] ? lo : hi) = sv; }
        sOld[im] = 0.5 * (lo + hi);
        Stair S;
        for (int i = 0; i < 2000; ++i) { const double A = 5.0 * (1.0 - i / 2000.0); S.apply (sOld[im] + (i % 2 ? -A : A)); }
        S.apply (sOld[im]); S.apply (0.0);
        pre[(size_t) im] = S;
        Mrem[im] = S.M (Lin.md);
    }
    Stair demagS; demagS.demag();

    // ---- the alignment, at 19.05 cm/s, by the sheet's definition
    Lab La = Lin; La.lengthUm = Lin.lengthUm * speeds[IALIGN] / Lin.v; La.v = speeds[IALIGN];
    double b10; const double bb = peak10Bias (La, b10);
    const double hbOp = operatingBias (La, bb, b10);
    const double hsRef = calibrateHs (La, hbOp);
    std::printf ("   alignment at %.2f cm/s: 10 kHz peak at hb %.2f, operating bias %.4f Hc, 0 VU = %.5f Hc deep-gap\n",
                 speeds[IALIGN], bb, hbOp, hsRef);

    const size_t GN = (size_t) NV * NLR * NA * NM * NS;
    std::vector<float> G (GN);
    std::vector<double> zX ((size_t) NV * NLR * NA), zW (zX.size()), zWr (zX.size()), zG (zX.size());
    auto gi = [&] (int v, int l, int a, int m, int s) { return ((((size_t) v * NLR + l) * NA + a) * NM + m) * NS + s; };
    auto zi = [&] (int v, int l, int a) { return ((size_t) v * NLR + l) * NA + a; };

    for (int iv = 0; iv < NV; ++iv)
    {
        const double t0 = now_s();
        const double vUm = speeds[iv] * 1.0e4;
        const double lamB = vUm / Lin.fb;
        for (int l = 0; l < NLR; ++l)
        {
            const double fmax = karlqvist (Lin.rec.g, 0.0, Lin.rec.d + yR[l]);
           #pragma omp parallel for schedule(dynamic, 1)
            for (int ia = 0; ia < NA; ++ia)
            {
                Pass P; P.head = Lin.rec; P.fb = Lin.fb; P.v = speeds[iv]; P.hb = Ag[ia] / fmax;
                // G: averaged over 8 bias phases and NSUB sub-depths
                constexpr int NPH = 8;
                for (int im = 0; im < NM; ++im)
                    for (int is = 0; is < NS; ++is)
                    {
                        Pass Q = P; Q.hs = Sg[is]; Q.sig = [] (double) { return 1.0; };
                        double acc = 0.0;
                        for (int k = 0; k < NSUB; ++k)
                        {
                            const double yk = ySub (l, k);
                            const double X = zoneHalfWidth (Q, yk);
                            for (int ph = 0; ph < NPH; ++ph)
                            {
                                Stair S = pre[(size_t) im];
                                advect (S, yk, lamB * ph / NPH, Q, X, vUm);
                                acc += S.M (Lin.md);
                            }
                        }
                        G[gi (iv, l, ia, im, is)] = (float) (acc / (NPH * NSUB));
                    }
                // the zone: a 1.2 um Gaussian pulse at small level, minus the same
                // cells with no pulse (cancels the bias-phase ripple), averaged over
                // the slab's sub-depths, pulse width deconvolved
                {
                    const double w = 1.2;
                    Pass Q = P; Q.hs = 0.02;
                    Q.sig = [=] (double t) { const double x = vUm * t; return std::exp (-0.5 * x * x / (w * w)); };
                    Pass Q0 = P; Q0.hs = 0.0;
                    const int n = 1600; const double pitch = 0.25;
                    std::vector<double> m (n, 0.0);
                    for (int k = 0; k < NSUB; ++k)
                    {
                        const double yk = ySub (l, k);
                        const double X = zoneHalfWidth (Q, yk);
                        for (int c = 0; c < n; ++c)
                        {
                            Stair S = demagS, S0 = demagS;
                            advect (S, yk, (c - n / 2) * pitch, Q, X, vUm);
                            advect (S0, yk, (c - n / 2) * pitch, Q0, X, vUm);
                            m[(size_t) c] += (S.M (Lin.md) - S0.M (Lin.md)) / NSUB;
                        }
                    }
                    double s0 = 0, s1 = 0, s2 = 0;
                    for (int c = 0; c < n; ++c) { const double u = (c - n / 2) * pitch; s0 += m[(size_t) c]; s1 += m[(size_t) c] * u; s2 += m[(size_t) c] * u * u; }
                    const double cen = (std::abs (s0) > 1e-12) ? s1 / s0 : 0.0;
                    const double var = (std::abs (s0) > 1e-12) ? s2 / s0 - cen * cen - w * w : 0.0;
                    zG[zi (iv, l, ia)] = s0 * pitch / (Q.hs * w * std::sqrt (2.0 * PI));
                    zX[zi (iv, l, ia)] = -cen;
                    zWr[zi (iv, l, ia)] = var > 0 ? std::sqrt (var) : 0.0;
                    // The engine models the zone as a Gaussian. The real kernel has a
                    // heavier tail than a Gaussian, so a Gaussian of the same RMS loses
                    // too much top end (0.5 dB at 15 kHz at 19 cm/s). Fit the width to
                    // the kernel's own transfer magnitude over the wavelengths that
                    // matter (6..40 um), with the probe pulse divided out:
                    // ln|K(k)/K(0)| = -k^2 w^2 / 2 in the least-squares sense.
                    double num = 0.0, den = 0.0;
                    if (std::abs (s0) > 1e-12)
                        for (double lam : { 40.0, 25.0, 16.0, 12.0, 9.0, 6.0 })
                        {
                            const double k = 2.0 * PI / lam;
                            double re = 0.0, im = 0.0;
                            for (int c = 0; c < n; ++c)
                            {
                                const double u = (c - n / 2) * pitch - cen;
                                re += m[(size_t) c] * std::cos (k * u); im += m[(size_t) c] * std::sin (k * u);
                            }
                            const double ratio = std::sqrt (re * re + im * im) / std::abs (s0) / std::exp (-0.5 * k * k * w * w);
                            if (ratio < 0.05 || ratio > 1.0) continue;
                            const double L = std::log (ratio);
                            num += k * k * L; den += k * k * k * k;
                        }
                    const double w2 = den > 0.0 ? -2.0 * num / den : 0.0;
                    zW[zi (iv, l, ia)] = w2 > 0.0 ? std::sqrt (w2) : 0.0;
                }
            }
        }
        std::printf ("   tables at %.2f cm/s (%.0f s)\n", speeds[iv], now_s() - t0);
    }

    // ---- per-layer small-signal response at the alignment speed, 0.7 / 1 / 1.4 x bias
    constexpr int NF = 6, NB = 3;
    const double fList[NF] = { 1000, 2000, 5000, 10000, 15000, 20000 };
    const double bMul[NB] = { 0.7, 1.0, 1.4 };
    double resp[NB][NLR][NF] {};
    {
        Lab L4 = La; L4.NL = NLR * NSUB;
        for (int b = 0; b < NB; ++b)
        {
            double ref1[NLR] {};
            for (int fi = 0; fi < NF; ++fi)
            {
                Tape tp = L4.fresh();
                L4.record (tp, hbOp * bMul[b], hsRef * 0.1, fList[fi]);
                Readout R (L4.md, tp, L4.stock);
                int c0, n; L4.window (fList[fi], c0, n);
                for (int l = 0; l < NLR; ++l)
                {
                    std::complex<double> acc (0, 0);
                    for (int k = 0; k < NSUB; ++k) acc += R.amp (fList[fi], L4.v, c0, n, l * NSUB + k);
                    const double a = std::abs (acc) / NSUB;
                    if (fi == 0) ref1[l] = a;
                    resp[b][l][fi] = dB (a / std::max (ref1[l], 1e-30));
                }
            }
        }
    }

    // ---- write
    FILE* o = std::fopen (path.c_str(), "w");
    if (! o) { std::printf ("   cannot write %s\n", path.c_str()); return; }
    std::fprintf (o, "// GENERATED by mtref --tables (the stage 2 reference model). Do not edit.\n");
    std::fprintf (o, "// Stock: %s. See docs/physics/03..06.\n#pragma once\nnamespace mt { namespace stock456 {\n", Lin.stock.name);
    std::fprintf (o, "constexpr double HC_KAM = %.4f, MR_KAM = %.4f, S_BULK = %.4f, DELTA_UM = %.3f;\n", Lin.stock.HcKAm, Lin.stock.MrKAm, Lin.stock.S, Lin.stock.deltaUm);
    std::fprintf (o, "constexpr double FLUX_MR_NWBM = %.3f;   // flux at Mr over the coating: G = 1 is this\n", Lin.stock.fluxSat() * Lin.stock.S);
    std::fprintf (o, "constexpr double SIG_C = %.5f, NU_U = %.5f, GAM_U = %.6f;\n", Lin.md.sigC, Lin.md.nu, Lin.md.gam);
    std::fprintf (o, "constexpr double REC_GAP_UM = %.3f, REC_SPACING_UM = %.3f, BIAS_HZ = %.1f;\n", Lin.rec.g, Lin.rec.d, Lin.fb);
    std::fprintf (o, "// the machine's alignment, at its nominal speed (deep-gap fields in Hc)\n");
    std::fprintf (o, "constexpr double ALIGN_SPEED_CMS = %.4f, HB_OPERATING = %.5f, HS_0VU = %.6f;\n", speeds[IALIGN], hbOp, hsRef);
    std::fprintf (o, "// the reference measurements' alignment at 38.1 cm/s (05-reference-results.md)\n");
    std::fprintf (o, "constexpr double HB_OP_REF38 = %.5f, HS_0VU_REF38 = %.6f;\n", hbOp38, hsRef38);
    std::fprintf (o, "constexpr int NV = %d, NL = %d, NA = %d, NS = %d, NM = %d;\n", NV, NLR, NA, NS, NM);
    std::fprintf (o, "constexpr double SPEED_CMS[NV] = {"); for (int i = 0; i < NV; ++i) std::fprintf (o, " %.4f,", speeds[i]); std::fprintf (o, " };   // a factor of 2 apart\n");
    std::fprintf (o, "// each layer is a SLAB of %.2f um, its tables averaged over %d sub-depths; centres:\n", slab, NSUB);
    std::fprintf (o, "constexpr double LAYER_Y_UM[NL] = {"); for (int l = 0; l < NLR; ++l) std::fprintf (o, " %.4f,", yR[l]); std::fprintf (o, " };\n");
    std::fprintf (o, "constexpr double LAYER_FMAX[NL] = {"); for (int l = 0; l < NLR; ++l) std::fprintf (o, " %.6f,", karlqvist (Lin.rec.g, 0.0, Lin.rec.d + yR[l])); std::fprintf (o, " };  // peak bias amplitude = hb * this\n");
    std::fprintf (o, "constexpr double A_GRID[NA] = {"); for (int i = 0; i < NA; ++i) std::fprintf (o, " %.6f,", Ag[i]); std::fprintf (o, " };\n");
    std::fprintf (o, "constexpr double S_GRID[NS] = {"); for (int i = 0; i < NS; ++i) std::fprintf (o, " %.6f,", Sg[i]); std::fprintf (o, " };\n");
    std::fprintf (o, "constexpr double M_GRID[NM] = {"); for (int i = 0; i < NM; ++i) std::fprintf (o, " %.6f,", Mg[i]); std::fprintf (o, " };  // in-field level of the prepared state\n");
    std::fprintf (o, "constexpr double M_REM[NM] = {"); for (int i = 0; i < NM; ++i) std::fprintf (o, " %.6f,", Mrem[i]); std::fprintf (o, " };   // the REMANENCE it holds: index G by this\n");
    std::fprintf (o, "// zone centroid downstream of the gap (um), Gaussian width fitted to the kernel transfer over 6..40 um wavelengths (um), small-signal gain dM/ds, per [speed][layer][A]\n");
    for (int which = 0; which < 3; ++which)
    {
        const char* nm = which == 0 ? "ZONE_X_UM" : which == 1 ? "ZONE_W_UM" : "ZONE_GAIN";
        const std::vector<double>& src = which == 0 ? zX : which == 1 ? zW : zG;
        std::fprintf (o, "constexpr double %s[NV][NL][NA] = {\n", nm);
        for (int iv = 0; iv < NV; ++iv)
        {
            std::fprintf (o, " {\n");
            for (int l = 0; l < NLR; ++l)
            {
                std::fprintf (o, "  {");
                for (int i = 0; i < NA; ++i) std::fprintf (o, " %.5f,", src[zi (iv, l, i)]);
                std::fprintf (o, " },\n");
            }
            std::fprintf (o, " },\n");
        }
        std::fprintf (o, "};\n");
    }
    std::fprintf (o, "// small-signal recording loss at the alignment speed per [bias 0.7x,1x,1.4x][layer][f], dB re the layer's 1 kHz\n");
    std::fprintf (o, "constexpr int NF = %d, NB = %d;\nconstexpr double RESP_F_HZ[NF] = { 1000, 2000, 5000, 10000, 15000, 20000 };\nconstexpr double RESP_BIAS_MUL[NB] = { 0.7, 1.0, 1.4 };\n", NF, NB);
    std::fprintf (o, "constexpr double RESP_DB[NB][NL][NF] = {\n");
    for (int b = 0; b < NB; ++b)
    {
        std::fprintf (o, " {");
        for (int l = 0; l < NLR; ++l) { std::fprintf (o, " {"); for (int fi = 0; fi < NF; ++fi) std::fprintf (o, " %.4f,", resp[b][l][fi]); std::fprintf (o, " },"); }
        std::fprintf (o, " },\n");
    }
    std::fprintf (o, "};\n");
    std::fprintf (o, "// new remanence (M/Ms, G = 1 is Mr) per [speed][layer][A][M_old][s]\n");
    std::fprintf (o, "constexpr float G[NV][NL][NA][NM][NS] = {\n");
    for (int iv = 0; iv < NV; ++iv)
    {
        std::fprintf (o, "{\n");
        for (int l = 0; l < NLR; ++l)
        {
            std::fprintf (o, " {\n");
            for (int ia = 0; ia < NA; ++ia)
            {
                std::fprintf (o, "  {\n");
                for (int im = 0; im < NM; ++im)
                {
                    std::fprintf (o, "   {");
                    for (int is = 0; is < NS; ++is) std::fprintf (o, "%.7ff,", G[gi (iv, l, ia, im, is)]);
                    std::fprintf (o, "},\n");
                }
                std::fprintf (o, "  },\n");
            }
            std::fprintf (o, " },\n");
        }
        std::fprintf (o, "},\n");
    }
    std::fprintf (o, "};\n} }\n");
    std::fclose (o);

    // ---- what the tables say, at the alignment speed
    std::printf ("   zone per layer at %.2f cm/s, operating bias (A, centroid, fitted width (rms), gain vs table slope):\n", speeds[IALIGN]);
    double sum = 0.0;
    for (int l = 0; l < NLR; ++l)
    {
        const double A = hbOp * karlqvist (Lin.rec.g, 0.0, Lin.rec.d + yR[l]);
        int ia = 0; while (ia < NA - 1 && Ag[ia + 1] < A) ++ia;
        const double ta = (A - Ag[ia]) / (Ag[ia + 1] - Ag[ia]);
        const double ds = Sg[NS / 2 + 1] - Sg[NS / 2 - 1];
        const double slope = (G[gi (IALIGN, l, ia, NM / 2, NS / 2 + 1)] - G[gi (IALIGN, l, ia, NM / 2, NS / 2 - 1)]) / ds;
        std::printf ("     layer %d: A %.2f  centroid %.2f um  width %.2f um (rms %.2f)  gain %.3f (table slope %.3f)\n",
                     l, A, zX[zi (IALIGN, l, ia)], zW[zi (IALIGN, l, ia)], zWr[zi (IALIGN, l, ia)], zG[zi (IALIGN, l, ia)], slope);
        int is = 0; while (is < NS - 1 && Sg[is + 1] < hsRef) ++is;
        const double ts = (hsRef - Sg[is]) / (Sg[is + 1] - Sg[is]);
        auto g = [&] (int a, int sidx) { return (double) G[gi (IALIGN, l, a, NM / 2, sidx)]; };
        sum += (1 - ta) * ((1 - ts) * g (ia, is) + ts * g (ia, is + 1)) + ta * ((1 - ts) * g (ia + 1, is) + ts * g (ia + 1, is + 1));
    }
    std::printf ("   table at 0 VU, DC, %.2f cm/s: %.1f nWb/m rms (target 250.0)\n", speeds[IALIGN],
                 sum / NLR * Lin.stock.fluxSat() * Lin.stock.S / std::sqrt (2.0));
    std::printf ("   written %s\n", path.c_str());
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    std::string out = argc > 1 && argv[1][0] != '-' ? argv[1] : ".";
    bool fast = false;
    int layersOpt = 0; bool playbackOpt = false; std::string tablesPath, pbPath; double nuOpt = 0.0, speedOpt = 0.0, hbOpt = 0.0, hsOpt = 0.0;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--fast")) fast = true;
        if (! std::strncmp (argv[i], "--layers=", 9)) layersOpt = std::atoi (argv[i] + 9);
        if (! std::strncmp (argv[i], "--tables=", 9)) tablesPath = argv[i] + 9;
        if (! std::strcmp (argv[i], "--playback")) playbackOpt = true;
        if (! std::strncmp (argv[i], "--pbtables=", 11)) { pbPath = argv[i] + 11; playbackOpt = true; }
        if (! std::strncmp (argv[i], "--nu=", 5)) nuOpt = std::atof (argv[i] + 5);
        if (! std::strncmp (argv[i], "--speed=", 8)) speedOpt = std::atof (argv[i] + 8);
        if (! std::strncmp (argv[i], "--hbop=", 7)) hbOpt = std::atof (argv[i] + 7);
        if (! std::strncmp (argv[i], "--hs0vu=", 8)) hsOpt = std::atof (argv[i] + 8);
    }

    Lab L;
    L.outDir = out;
    L.stock = Stock { "1970s high-output studio (456 class)", 26.3, 116.2, 14.0, 760.0, 0.12, 0.40, 0.0, 0.80 };
    if (fast) { L.lengthUm = 2500.0; L.NL = 6; }
    if (layersOpt > 0) L.NL = layersOpt;
    // Another speed under a FIXED alignment (the real-time model keeps the 38.1
    // cm/s one at every speed): the tape needs more length for the same number
    // of periods at a higher speed, so scale it.
    if (speedOpt > 0.0) { L.lengthUm *= speedOpt / L.v; L.v = speedOpt; }

    std::printf ("Magneto Transport reference model (stage 2)\n");
   #ifdef _OPENMP
    std::printf ("OpenMP threads: %d\n", omp_get_max_threads());
   #endif

    //-------------------------------------------------------------- 1. the medium
    std::printf ("\n1. THE MEDIUM\n");
    double t = now_s();
    std::printf ("   assumed: particle coercivity lognormal sigma_c = %.2f; interaction field FWHM = %.2f Hc; orientation squareness %.2f\n",
                 L.stock.sigC, L.stock.fwhmU, L.stock.S);
    L.md.sigC = L.stock.sigC;
    // The tail shape of the interaction field is the one free shape parameter, and
    // MOL is the one data-sheet number that measures the approach to saturation:
    // fit nu on the ideal anhysteretic curve so that HD3 = 3 % at the sheet's MOL
    // (peak flux / full remanence), then measure the REAL process against it.
    const double mPeakMol = L.stock.molRms * std::sqrt (2.0) / (L.stock.fluxSat() * L.stock.S);
    {
        double lo = std::log (1.05), hi = std::log (400.0);
        for (int it = 0; it < 40; ++it)
        {
            const double nu = std::exp (0.5 * (lo + hi));
            L.md.setU (nu, L.stock.fwhmU);
            (idealHd3 (L.md, mPeakMol) > 0.03 ? lo : hi) = std::log (nu);   // heavier tails distort more
        }
        L.md.setU (std::exp (0.5 * (lo + hi)), L.stock.fwhmU);
        L.stock.nu = L.md.nu;
    }
    std::printf ("   fitted interaction tail: Student-t nu = %.3f (scale %.4f Hc) for HD3 = 3 %% at M/Mr = %.3f (the sheet's MOL)\n",
                 L.md.nu, L.md.gam, mPeakMol);
    std::printf ("   ideal anhysteretic HD3 at 185 nWb/m: %.3f %% (sheet 0.1); anhysteretic susceptibility chi*Hc/Mr = %.2f\n",
                 100 * idealHd3 (L.md, 185.0 * std::sqrt (2.0) / (L.stock.fluxSat() * L.stock.S)), 2.0 * L.md.pU0() / L.stock.S);
    double Hr; const double sfd = sfdOf (L.md, Hr);
    std::printf ("   the model's own loop: Preisach squareness %.4f, DCD-derivative SFD %.3f (Hr = %.3f Hc)\n", squarenessP (L.md), sfd, Hr);
    { double lo = -3.0, hi = 0.0; for (int i = 0; i < 60; ++i) { const double h = 0.5 * (lo + hi); (1.0 - 2.0 * L.md.Fdirect (Medium::HS, h) > 0 ? hi : lo) = h; }
      std::printf ("   bulk Hc = %.5f (must be 1 by construction)\n", -0.5 * (lo + hi)); }
    std::printf ("   Ms = %.1f kA/m, Mr = %.1f kA/m; flux at Mr over %.0f um = %.0f nWb/m (the AC-recordable maximum)\n",
                 L.stock.Ms() * 1e-3, L.stock.MrKAm, L.stock.deltaUm, L.stock.fluxSat() * L.stock.S);
    t = now_s();
    L.md.build();
    std::printf ("   Everett table %dx%d built in %.1f s\n", L.md.N, L.md.N, now_s() - t);
    {
        // The ideal curve is only a starting point: the real record process
        // (finite bias decay, the moving zone, the depth of the coating) moves
        // MOL by most of a dB. Refine nu on the FULL chain - align bias the way
        // the sheet does, set 0 VU, find 3 % HD3 - until MOL is the sheet's.
        const double target = dB (L.stock.molRms / 250.0);
        double lo = std::log (std::max (1.2, 0.4 * L.md.nu)), hi = std::log (8.0 * L.md.nu);
        if (nuOpt > 0.0) { lo = hi = std::log (nuOpt); }      // --nu=: a fixed coating, e.g. to compare layer counts
        std::printf ("   refining nu on the full record process for MOL = %+.2f dB re 250:\n", target);
        t = now_s();
        for (int it = 0; it < (nuOpt > 0.0 ? 1 : 11); ++it)
        {
            const double nu = std::exp (0.5 * (lo + hi));
            L.md.setU (nu, L.stock.fwhmU); L.md.build();
            double b10; const double bb = peak10Bias (L, b10);
            const double op = operatingBias (L, bb, b10);
            const double hs = calibrateHs (L, op);
            const double mol = molDb (L, op, hs);
            std::printf ("     nu %.3f: operating bias %.2f Hc, MOL %+.3f dB\n", nu, op, mol);
            (mol > target ? hi : lo) = std::log (nu);     // heavier tails (smaller nu) lower the MOL
        }
        L.md.setU (std::exp (0.5 * (lo + hi)), L.stock.fwhmU); L.md.build();
        L.stock.nu = L.md.nu;
        std::printf ("   nu = %.4f (scale %.4f Hc), fitted on the full process in %.0f s\n", L.md.nu, L.md.gam, now_s() - t);
        std::printf ("   ideal anhysteretic HD3 at 185 nWb/m with it: %.3f %%; susceptibility chi*Hc/Mr = %.2f\n",
                     100 * idealHd3 (L.md, 185.0 * std::sqrt (2.0) / (L.stock.fluxSat() * L.stock.S)), 2.0 * L.md.pU0() / L.stock.S);
    }
    {
        double worst = 0.0;
        for (int i = 0; i < 200; ++i)
        {
            const double a = -5.5 + 11.0 * ((i * 7919) % 1000) / 1000.0, b = a - 3.0 * ((i * 104729) % 1000) / 1000.0;
            worst = std::max (worst, std::abs (L.md.F (a, b) - L.md.Fdirect (a, b)));
        }
        std::printf ("   bicubic table vs direct quadrature, worst |dF| = %.2e\n", worst);
    }
    {
        Stair sx; sx.apply (3.0); sx.apply (-0.4);
        const double m0 = sx.M (L.md);
        for (int i = 0; i < 10000; ++i) { sx.apply (1.3); sx.apply (-0.4); }
        std::printf ("   minor loop closure: M after 1 cycle %.12f, after 10 000 cycles %.12f, staircase %zu entries\n",
                     m0, sx.M (L.md), sx.v.size());
    }
    {
        std::printf ("   anhysteretic curve, decaying AC (5 Hc -> 0, 4000 half-cycles) vs 2 Phi_u(s) - 1:\n     ");
        for (double sv : { 0.05, 0.1, 0.2, 0.4, 0.8, 1.6 })
        {
            Stair sx; const int Nn = 4000;
            for (int i = 0; i < Nn; ++i) { const double A = 5.0 * (1.0 - (double) i / Nn); sx.apply (sv + (i % 2 ? -A : A)); }
            sx.apply (sv);
            std::printf ("s=%.2f: %.4f/%.4f  ", sv, sx.M (L.md), 2.0 * L.md.PhiU (sv) - 1.0);
        }
        std::printf ("\n");
    }

    //-------------------------------------------------------------- 2. bias curve
    std::printf ("\n2. BIAS CURVE at %.1f cm/s, signal deep-gap field 0.08 Hc; output in dB re 250 nWb/m rms\n", L.v);
    std::printf ("   %6s %10s %10s %10s %10s\n", "hb/Hc", "1k avg", "1k play", "10k avg", "10k play");
    const double biasList[] = { 0.5, 0.75, 1, 1.25, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10, 12, 16, 20 };
    std::vector<double> b10;
    double bestB = 0, best10 = -1e9;
    t = now_s();
    for (double hb : biasList)
    {
        double a1, p1, a10, p10;
        L.measure (hb, 0.08, 1000.0, a1, p1);
        L.measure (hb, 0.08, 10000.0, a10, p10);
        std::printf ("   %6.1f %10.2f %10.2f %10.2f %10.2f\n", hb, dB (a1 / 250), dB (p1 / 250), dB (a10 / 250), dB (p10 / 250));
        b10.push_back (p10);
        if (p10 > best10) { best10 = p10; bestB = hb; }
    }
    std::printf ("   (%.1f s)\n", now_s() - t);
    // operating bias: 10 kHz 3 dB past its peak, the peak found properly
    bestB = peak10Bias (L, best10);
    const double hbOp = hbOpt > 0.0 ? hbOpt : operatingBias (L, bestB, best10);
    std::printf ("   10 kHz peaks at hb = %.2f; OPERATING BIAS (10 kHz -3 dB past peak) hb = %.2f Hc\n", bestB, hbOp);

    //-------------------------------------------------------------- 3. level sweep
    std::printf ("\n3. LEVEL SWEEP at 1 kHz, operating bias\n");
    // calibrate hs for 250 nWb/m rms
    const double hsRef = hsOpt > 0.0 ? hsOpt : calibrateHs (L, hbOp);
    std::printf ("   0 VU (250 nWb/m rms) = signal deep-gap field %.4f Hc = %.3f of the bias\n", hsRef, hsRef / hbOp);
    std::printf ("   %8s %10s %10s %10s %10s\n", "dB re ref", "flux rms", "re ref dB", "HD3 %", "THD %");
    FILE* f = std::fopen ((out + "/level-sweep.csv").c_str(), "w");
    if (f) std::fprintf (f, "level_dB,flux_nWbm,hd3,thd\n");
    double mol = 0, hd3ref = 0, hd3at185 = 0;
    for (double lv = -20; lv <= 16.01; lv += 2)
    {
        double a, p, hd3, thd;
        L.measure (hbOp, hsRef * std::pow (10.0, lv / 20.0), 1000.0, a, p, &hd3, &thd);
        std::printf ("   %8.0f %10.1f %10.2f %10.3f %10.3f\n", lv, a, dB (a / 250), 100 * hd3, 100 * thd);
        if (f) std::fprintf (f, "%.1f,%.3f,%.6f,%.6f\n", lv, a, hd3, thd);
        if (lv == 0) hd3ref = hd3;
    }
    if (f) std::fclose (f);
    {
        // MOL: 3 % third harmonic, bisect on level
        double lo = 0.0, hi = 20.0;
        for (int it = 0; it < 14; ++it)
        {
            const double lv = 0.5 * (lo + hi);
            double a, p, hd3; L.measure (hbOp, hsRef * std::pow (10.0, lv / 20.0), 1000.0, a, p, &hd3);
            (hd3 < 0.03 ? lo : hi) = lv;
        }
        double a, p; L.measure (hbOp, hsRef * std::pow (10.0, 0.5 * (lo + hi) / 20.0), 1000.0, a, p);
        mol = a;
        double hd3; L.measure (hbOp, hsRef * (185.0 / 250.0), 1000.0, a, p, &hd3); hd3at185 = hd3;
    }
    std::printf ("   MOL (3 %% HD3): %.0f nWb/m rms = %+.2f dB re 250 = %+.2f dB re 185   (sheet: +12.3 dB re 185 = 760 nWb/m)\n",
                 mol, dB (mol / 250), dB (mol / 185));
    std::printf ("   HD3 at 250 nWb/m: %.3f %%; at 185 nWb/m: %.3f %%   (sheet: 0.1 %% at 185)\n", 100 * hd3ref, 100 * hd3at185);

    //-------------------------------------------------------------- 9. playback (stage 4 targets)
    // The engine stores FOUR slabs, each one magnetisation value per cell; the
    // playback head weights depth by e^{-k y}. Here: the exact playback over 16
    // depths, the same recording composed through 4 uniform slabs (the slab
    // integral (e^{-k top} - e^{-k bot}) / (k D) per slab - what the engine can
    // do), and the depth-averaged (recording) response, each re its own 1 kHz,
    // at a small signal (-20 dB re 0 VU) and a FIXED bias and field.
    if (playbackOpt)
    {
        std::printf ("\n9. PLAYBACK RESPONSE (16 depths exact vs 4 slabs, -20 dB, hb %.4f, hs %.5f)\n", hbOp, hsRef);
        Lab P = L; P.NL = 16;
        const double fl[] = { 500, 1000, 2000, 5000, 10000, 15000, 20000 };
        const double speeds[] = { 4.7625, 9.525, 19.05, 38.1, 76.2 };
        constexpr int PNV = 5, PNF = 7;
        double ref16[PNV][PNF] {}, ref4[PNV][PNF] {}, refRec[PNV][PNF] {}, loss1k[PNV] {}, dsp[PNV] {}, abs1k[PNV] {};
        int iv = -1;
        for (double sp : speeds)
        {
            ++iv;
            P.v = sp; P.lengthUm = L.lengthUm * std::max (1.0, sp / L.v);
            double refAvg = 0, abs16 = 0, abs4 = 0;
            std::printf ("   %6.2f cm/s  f: 16-depth / 4-slab / depth-avg (dB re 1 nWb/m peak)\n", sp);
            for (double f : fl)
            {
                Tape tp = P.fresh();
                P.record (tp, hbOp, hsRef * 0.1, f);
                Readout R (P.md, tp, P.stock);
                int c0, n; P.window (f, c0, n);
                const double k = 2.0 * PI * f / (sp * 1.0e4);
                const double gap = std::sin (0.5 * k * P.play.g) / (0.5 * k * P.play.g);
                const double D = P.stock.deltaUm / 4.0;
                std::complex<double> s4 (0, 0);
                for (int s = 0; s < 4; ++s)
                {
                    std::complex<double> A (0, 0);
                    for (int q = 0; q < 4; ++q) A += R.amp (f, sp, c0, n, 4 * s + q);
                    A /= 4.0;
                    const double top = P.play.d + s * D, bot = top + D;
                    s4 += A * (std::exp (-k * top) - std::exp (-k * bot)) / (k * D);
                }
                const double p16 = std::abs (R.playback (f, sp, c0, n, P.play.d, P.play.g));
                const double p4 = std::abs (s4 * gap) / 4.0;
                const double avg = std::abs (R.amp (f, sp, c0, n, -1));
                int fi = 0; while (fl[fi] != f) ++fi;
                ref16[iv][fi] = dB (p16); ref4[iv][fi] = dB (p4); refRec[iv][fi] = dB (avg);
                if (f == 1000.0) { abs16 = p16; abs4 = p4; refAvg = avg; }
                std::printf ("      %6.0f: %+7.2f %+7.2f %+7.2f\n", f, dB (p16), dB (p4), dB (avg));
            }
            std::printf ("      1 kHz absolute (nWb/m peak): 16-depth %.3f, 4-slab %.3f (%.3f dB apart)\n",
                         abs16, abs4, dB (abs4 / abs16));
            loss1k[iv] = dB (abs16 / refAvg);
            abs1k[iv] = abs16;
            // Four uniform slabs read slightly bright at the top: fit the effective
            // extra head spacing that removes it. A spacing is a common factor
            // e^{-k dd} on every slab, so re the 1 kHz it predicts -8.686 dd (k - k1),
            // fitted over 2..15 kHz where the response is within 30 dB of 1 kHz.
            const double k1 = 2.0 * PI * 1000.0 / (sp * 1.0e4);
            double su = 0.0, suu = 0.0;
            for (int fi = 2; fi <= 5; ++fi)
            {
                const double r16 = ref16[iv][fi] - ref16[iv][1], r4 = ref4[iv][fi] - ref4[iv][1];
                if (r16 < -30.0) continue;
                const double u = 2.0 * PI * fl[fi] / (sp * 1.0e4) - k1;
                su += (r16 - r4) * u; suu += u * u;
            }
            dsp[iv] = suu > 0.0 ? -su / (8.686 * suu) : 0.0;
            double worst = 0.0;
            for (int fi = 2; fi <= 5; ++fi)
            {
                const double r16 = ref16[iv][fi] - ref16[iv][1], r4 = ref4[iv][fi] - ref4[iv][1];
                if (r16 < -30.0) continue;
                const double u = 2.0 * PI * fl[fi] / (sp * 1.0e4) - k1;
                worst = std::max (worst, std::abs (r16 - (r4 - 8.686 * dsp[iv] * u)));
            }
            std::printf ("      effective extra spacing for 4 slabs: %+.4f um (residual %.3f dB, 2..15 kHz); 1 kHz playback loss %.3f dB\n",
                         dsp[iv], worst, loss1k[iv]);
        }
        if (! pbPath.empty())
        {
            FILE* o = std::fopen (pbPath.c_str(), "w");
            std::fprintf (o, "#pragma once\n// GENERATED by reference/mtref --playback --pbtables= : do not edit.\n");
            std::fprintf (o, "// Playback targets at hb %.4f Hc, hs %.5f Hc (-20 dB re 0 VU at the 19.05 cm/s alignment).\n", hbOp, hsRef);
            std::fprintf (o, "namespace mt { namespace pb456 {\n");
            std::fprintf (o, "constexpr int NV = %d, NF = %d;\n", PNV, PNF);
            std::fprintf (o, "constexpr double SPEED_CMS[NV] = {"); for (double s : speeds) std::fprintf (o, " %.4f,", s); std::fprintf (o, " };\n");
            std::fprintf (o, "constexpr double F_HZ[NF] = {"); for (double f : fl) std::fprintf (o, " %.1f,", f); std::fprintf (o, " };\n");
            std::fprintf (o, "// effective extra head spacing (um) that makes 4 uniform slabs read like 16 depths\n");
            std::fprintf (o, "constexpr double D_SPACING_UM[NV] = {"); for (int v = 0; v < PNV; ++v) std::fprintf (o, " %.5f,", dsp[v]); std::fprintf (o, " };\n");
            std::fprintf (o, "// 16-depth playback response, dB re its own 1 kHz\n");
            std::fprintf (o, "constexpr double REF_DB[NV][NF] = {\n");
            for (int v = 0; v < PNV; ++v) { std::fprintf (o, " {"); for (int fi = 0; fi < PNF; ++fi) std::fprintf (o, " %.4f,", ref16[v][fi] - ref16[v][1]); std::fprintf (o, " },\n"); }
            std::fprintf (o, "};\n// the same recordings, depth-averaged flux (no playback head), dB re its own 1 kHz\n");
            std::fprintf (o, "constexpr double REC_DB[NV][NF] = {\n");
            for (int v = 0; v < PNV; ++v) { std::fprintf (o, " {"); for (int fi = 0; fi < PNF; ++fi) std::fprintf (o, " %.4f,", refRec[v][fi] - refRec[v][1]); std::fprintf (o, " },\n"); }
            std::fprintf (o, "};\n// playback loss at 1 kHz: 16-depth playback vs depth-averaged flux, dB\n");
            std::fprintf (o, "constexpr double LOSS_1K_DB[NV] = {"); for (int v = 0; v < PNV; ++v) std::fprintf (o, " %.4f,", loss1k[v]); std::fprintf (o, " };\n");
            std::fprintf (o, "constexpr double ABS_1K_NWB_PEAK[NV] = {"); for (int v = 0; v < PNV; ++v) std::fprintf (o, " %.4f,", abs1k[v]); std::fprintf (o, " };\n");
            std::fprintf (o, "}} // namespace mt::pb456\n");
            std::fclose (o);
            std::printf ("   written %s\n", pbPath.c_str());
        }
        return 0;
    }

    //-------------------------------------------------------------- 4. wavelength response vs bias
    std::printf ("\n4. RESPONSE vs FREQUENCY at %.1f cm/s, -20 dB re ref, three bias settings; dB re the 1 kHz output of each\n", L.v);
    std::printf ("   %8s | %8s %8s %8s | %8s %8s %8s\n", "f Hz", "0.7x avg", "1.0x avg", "1.4x avg", "0.7x pb", "1.0x pb", "1.4x pb");
    f = std::fopen ((out + "/response.csv").c_str(), "w");
    if (f) std::fprintf (f, "f,avg07,avg10,avg14,pb07,pb10,pb14\n");
    const double freqs[] = { 1000, 2000, 5000, 10000, 15000, 20000 };
    const double biasMul[] = { 0.7, 1.0, 1.4 };
    double ref1[3][2] {};
    for (double fr : freqs)
    {
        double avg[3], pb[3];
        for (int bI = 0; bI < 3; ++bI)
        {
            double a, p; L.measure (hbOp * biasMul[bI], hsRef * 0.1, fr, a, p);
            if (fr == 1000) { ref1[bI][0] = a; ref1[bI][1] = p; }
            avg[bI] = dB (a / ref1[bI][0]); pb[bI] = dB (p / ref1[bI][1]);
        }
        std::printf ("   %8.0f | %8.2f %8.2f %8.2f | %8.2f %8.2f %8.2f\n", fr, avg[0], avg[1], avg[2], pb[0], pb[1], pb[2]);
        if (f) std::fprintf (f, "%.0f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", fr, avg[0], avg[1], avg[2], pb[0], pb[1], pb[2]);
    }
    if (f) std::fclose (f);

    //-------------------------------------------------------------- 5. erasure
    std::printf ("\n5. ERASURE of a 0 VU 1 kHz recording (residual in dB re the original)\n");
    {
        int c0, n; L.window (1000.0, c0, n);
        auto residual = [&] (Tape& tp) { Readout R (L.md, tp, L.stock); return std::abs (R.amp (1000.0, L.v, c0, n, -1)) / std::sqrt (2.0); };
        Tape base = L.fresh(); L.record (base, hbOp, hsRef, 1000.0);
        const double orig = residual (base);
        std::printf ("   original: %.1f nWb/m rms\n", orig);
        { Tape tp = base; Pass P; P.head = L.rec; P.hb = hbOp; P.fb = L.fb; P.v = L.v; runPass (tp, P);
          std::printf ("   bias-only pass over it (record head, no signal): %+.1f dB\n", dB (residual (tp) / orig)); }
        { Tape tp = base; L.record (tp, hbOp, hsRef, 3000.0);
          std::printf ("   recorded over with 3 kHz at 0 VU: 1 kHz residual %+.1f dB\n", dB (residual (tp) / orig)); }
        { Tape tp = base; L.record (tp, hbOp, hsRef * 0.1, 3000.0);
          std::printf ("   recorded over with 3 kHz at -20 dB: 1 kHz residual %+.1f dB\n", dB (residual (tp) / orig)); }
        { Tape tp = base; L.record (tp, hbOp * 0.5, hsRef, 3000.0);
          std::printf ("   recorded over with 3 kHz at 0 VU, HALF bias: 1 kHz residual %+.1f dB\n", dB (residual (tp) / orig)); }
        std::printf ("   erase head (60 um gap) at he = ");
        for (double he : { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 4.0, 40.0 })
        {
            Tape tp = base; L.erasePass (tp, he);
            std::printf ("%.0f: %+.1f dB  ", he, dB (residual (tp) / orig));
        }
        std::printf ("\n");
    }

    //-------------------------------------------------------------- 6. one number per cell?
    std::printf ("\n6. DOES THE OVERWRITE DEPEND ON HISTORY BEYOND THE FLUX?\n");
    std::printf ("   pre-states with the SAME 1 kHz flux made differently, each overwritten with 3 kHz at 0 VU, operating bias\n");
    {
        int c0, n; L.window (1000.0, c0, n);
        auto flux1k = [&] (Tape& tp) { Readout R (L.md, tp, L.stock); return std::abs (R.amp (1000.0, L.v, c0, n, -1)) / std::sqrt (2.0); };
        auto flux3k = [&] (Tape& tp) { int c3, n3; L.window (3000.0, c3, n3); Readout R (L.md, tp, L.stock); return std::abs (R.amp (3000.0, L.v, c3, n3, -1)) / std::sqrt (2.0); };
        struct Route { const char* name; double biasMul; };
        const Route routes[] = { { "operating bias", 1.0 }, { "0.6x bias (shallow)", 0.6 }, { "1.6x bias (deep)", 1.6 } };
        double resid[3], newTone[3];
        for (int r = 0; r < 3; ++r)
        {
            // calibrate the level on this route for 250 nWb/m
            double lo = 0.001, hi = 8.0;
            for (int it = 0; it < 18; ++it)
            {
                const double hs = std::sqrt (lo * hi);
                Tape tp = L.fresh(); L.record (tp, hbOp * routes[r].biasMul, hs, 1000.0);
                (flux1k (tp) < 250.0 ? lo : hi) = hs;
            }
            Tape tp = L.fresh(); L.record (tp, hbOp * routes[r].biasMul, std::sqrt (lo * hi), 1000.0);
            const double before = flux1k (tp);
            L.record (tp, hbOp, hsRef, 3000.0);
            resid[r] = dB (flux1k (tp) / before); newTone[r] = flux3k (tp);
            std::printf ("   %-22s pre 1 kHz %.1f nWb/m -> after overwrite: 1 kHz residual %+.2f dB, new 3 kHz tone %.1f nWb/m\n",
                         routes[r].name, before, resid[r], newTone[r]);
        }
        std::printf ("   spread of the residual across histories: %.2f dB; of the new tone: %.2f dB\n",
                     *std::max_element (resid, resid + 3) - *std::min_element (resid, resid + 3),
                     dB (*std::max_element (newTone, newTone + 3) / *std::min_element (newTone, newTone + 3)));
    }

    //-------------------------------------------------------------- 6b. convergence in layers
    std::printf ("\n6b. LAYER CONVERGENCE at 0 VU, 1 kHz, operating bias (flux, HD3) for %d and %d layers\n", L.NL, 2 * L.NL);
    {
        double a1, p1, h1, a2, p2, h2;
        L.measure (hbOp, hsRef, 1000.0, a1, p1, &h1);
        const int nl0 = L.NL; L.NL = 2 * nl0;
        L.measure (hbOp, hsRef, 1000.0, a2, p2, &h2);
        L.NL = nl0;
        std::printf ("   %d layers: %.2f nWb/m, HD3 %.3f %%;  %d layers: %.2f nWb/m, HD3 %.3f %%  (%.2f dB, %.1f %% apart)\n",
                     nl0, a1, 100 * h1, 2 * nl0, a2, 100 * h2, dB (a2 / a1), 100 * (h2 / h1 - 1));
    }

    //-------------------------------------------------------------- 8. the stage 3 tables
    if (! tablesPath.empty())
    {
        std::printf ("\n8. STAGE 3 RECORD TABLES\n");
        t = now_s();
        writeTables (L, hbOp, hsRef, tablesPath);
        std::printf ("   (%.1f s)\n", now_s() - t);
    }

    //-------------------------------------------------------------- 7. cost
    std::printf ("\n7. COST\n");
    {
        t = now_s();
        Tape tp = L.fresh(); L.record (tp, hbOp, hsRef, 1000.0);
        const double sec = now_s() - t;
        const double tapeS = L.lengthUm * 1e-4 / L.v;
        std::printf ("   record pass: %d cells x %d layers in %.2f s = %.0f x real time at %.1f cm/s (%d threads)\n",
                     L.cells(), L.NL, sec, sec / tapeS, L.v,
                   #ifdef _OPENMP
                     omp_get_max_threads()
                   #else
                     1
                   #endif
                     );
    }
    std::printf ("\ndone\n");
    return 0;
}
