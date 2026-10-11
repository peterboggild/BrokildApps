#include "Equaliser.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mt
{
namespace
{
constexpr double PI = 3.14159265358979323846;

// first-order high shelf, unity at DC, A = 10^(g/20) above the corner fc
// (the corner is the geometric mean of the zero and the pole)
double shelfDb (double f, double fc, double gDb)
{
    const double A = std::pow (10.0, gDb / 20.0);
    const double w = 2.0 * PI * f, wc = 2.0 * PI * fc;
    const double wz = wc / std::sqrt (A), wp = wc * std::sqrt (A);
    return 10.0 * std::log10 ((w * w + wz * wz) / (w * w + wp * wp)) + gDb;
}

// solve A x = b in place (n x n, Gaussian elimination with partial pivoting)
bool solve (std::vector<double>& A, std::vector<double>& b, int n)
{
    for (int c = 0; c < n; ++c)
    {
        int p = c;
        for (int r = c + 1; r < n; ++r) if (std::abs (A[(size_t) (r * n + c)]) > std::abs (A[(size_t) (p * n + c)])) p = r;
        if (std::abs (A[(size_t) (p * n + c)]) < 1.0e-14) return false;
        if (p != c) { for (int k = 0; k < n; ++k) std::swap (A[(size_t) (c * n + k)], A[(size_t) (p * n + k)]); std::swap (b[(size_t) c], b[(size_t) p]); }
        for (int r = c + 1; r < n; ++r)
        {
            const double m = A[(size_t) (r * n + c)] / A[(size_t) (c * n + c)];
            for (int k = c; k < n; ++k) A[(size_t) (r * n + k)] -= m * A[(size_t) (c * n + k)];
            b[(size_t) r] -= m * b[(size_t) c];
        }
    }
    for (int r = n - 1; r >= 0; --r)
    {
        double s = b[(size_t) r];
        for (int k = r + 1; k < n; ++k) s -= A[(size_t) (r * n + k)] * b[(size_t) k];
        b[(size_t) r] = s / A[(size_t) (r * n + r)];
    }
    return true;
}
} // namespace

void eqTimeConstants (int standard, double v, double& tau2, double& w1)
{
    static const double sp[5] = { 4.7625, 9.525, 19.05, 38.1, 76.2 };
    static const double t2[2][5] = { { 120e-6, 90e-6, 70e-6, 35e-6, 35e-6 },     // IEC
                                     {  90e-6, 90e-6, 50e-6, 50e-6, 50e-6 } };   // NAB
    static const double w1t[2][5] = { { 1.0 / 1590e-6, 1.0 / 3180e-6, 0.0, 0.0, 0.0 },
                                      { 1.0 / 3180e-6, 1.0 / 3180e-6, 1.0 / 3180e-6, 1.0 / 3180e-6, 1.0 / 3180e-6 } };
    const int s = std::clamp (standard, 0, 1);
    const double f = std::clamp (std::log2 (v / sp[0]), 0.0, 4.0);
    const int i = std::min ((int) f, 3);
    const double t = f - i;
    tau2 = std::exp ((1.0 - t) * std::log (t2[s][i]) + t * std::log (t2[s][i + 1]));
    w1 = (1.0 - t) * w1t[s][i] + t * w1t[s][i + 1];
}

double ShelfCascade::analogueDb (double f) const
{
    double d = g0Db;
    for (int i = 0; i < NS; ++i) d += shelfDb (f, fc[i], gDb[i]);
    return d;
}

double ShelfCascade::fit (const double* fHz, const double* tgt, int n, bool warm, double fMax)
{
    // corners log-spaced, 12 Hz .. 24 kHz; a warm start keeps the gains it has
    for (int i = 0; i < NS; ++i) { fc[i] = 12.0 * std::pow (2000.0, (double) i / (NS - 1)); if (! warm) gDb[i] = 0.0; }
    if (! warm) g0Db = tgt[0];
    const int m = NS + 1;
    // The band that matters (to 16 kHz) carries full weight, the top a fifth: the
    // fit must not trade in-band accuracy for the gap region. A small penalty on
    // neighbouring shelves' gain difference keeps the solution smooth, so gain sets
    // stay continuous from speed to speed and interpolate sanely (unpenalised, 16
    // shelves traded huge opposite gains and one interpolated curve put a 50 %
    // feedback loop past unity).
    constexpr double RHO = 0.0, RHO0 = 0.0;
    std::vector<double> wt ((size_t) n);
    for (int k = 0; k < n; ++k) wt[(size_t) k] = 1.0;
    auto penalty = [&] () {
        double p = 0.0;
        for (int i = 1; i < NS; ++i) p += RHO * (gDb[i] - gDb[i - 1]) * (gDb[i] - gDb[i - 1]);
        for (int i = 0; i < NS; ++i) p += RHO0 * gDb[i] * gDb[i];
        return p;
    };
    auto resid = [&] (std::vector<double>& r) {
        double e = 0.0;
        for (int k = 0; k < n; ++k) { r[(size_t) k] = analogueDb (fHz[k]) - tgt[k]; e += wt[(size_t) k] * r[(size_t) k] * r[(size_t) k]; }
        return e + penalty();
    };
    std::vector<double> r ((size_t) n), r2 ((size_t) n), J ((size_t) (n * m)), A ((size_t) (m * m)), b ((size_t) m);
    double err = resid (r), lambda = 1.0e-2;
    for (int it = 0; it < 60; ++it)
    {
        // Jacobian: d/dg0 = 1; for a shelf, d dB / d g = 1 - (wz^2/(w^2+wz^2) + wp^2/(w^2+wp^2)) / 2
        for (int k = 0; k < n; ++k)
        {
            J[(size_t) (k * m)] = 1.0;
            const double w = 2.0 * PI * fHz[k], w2 = w * w;
            for (int i = 0; i < NS; ++i)
            {
                const double sA = std::pow (10.0, gDb[i] / 40.0), wc = 2.0 * PI * fc[i];
                const double wz = wc / sA, wp = wc * sA;
                J[(size_t) (k * m + i + 1)] = 1.0 - 0.5 * (wz * wz / (w2 + wz * wz) + wp * wp / (w2 + wp * wp));
            }
        }
        bool improved = false;
        for (int tries = 0; tries < 8 && ! improved; ++tries)
        {
            for (int a = 0; a < m; ++a)
            {
                double sb = 0.0;
                for (int k = 0; k < n; ++k) sb += wt[(size_t) k] * J[(size_t) (k * m + a)] * r[(size_t) k];
                b[(size_t) a] = -sb;
                for (int c = 0; c < m; ++c)
                {
                    double s = 0.0;
                    for (int k = 0; k < n; ++k) s += wt[(size_t) k] * J[(size_t) (k * m + a)] * J[(size_t) (k * m + c)];
                    A[(size_t) (a * m + c)] = s;
                }
            }
            // the penalty's gradient and curvature (shelf gains are parameters 1..NS)
            for (int i = 0; i < NS; ++i)
            {
                const int a = i + 1;
                double grad = RHO0 * gDb[i];
                A[(size_t) (a * m + a)] += RHO0;
                if (i > 0)      { grad += RHO * (gDb[i] - gDb[i - 1]); A[(size_t) (a * m + a)] += RHO; A[(size_t) (a * m + a - 1)] -= RHO; }
                if (i < NS - 1) { grad += RHO * (gDb[i] - gDb[i + 1]); A[(size_t) (a * m + a)] += RHO; A[(size_t) (a * m + a + 1)] -= RHO; }
                b[(size_t) a] -= grad;
            }
            for (int a = 0; a < m; ++a) A[(size_t) (a * m + a)] *= 1.0 + lambda;
            if (! solve (A, b, m)) { lambda *= 10.0; continue; }
            const double g0s = g0Db; double gs[NS]; for (int i = 0; i < NS; ++i) gs[i] = gDb[i];
            g0Db += b[0];
            for (int i = 0; i < NS; ++i) gDb[i] = std::clamp (gDb[i] + b[(size_t) (i + 1)], -40.0, 40.0);
            const double e2 = resid (r2);
            if (e2 < err) { err = e2; r.swap (r2); lambda = std::max (lambda / 3.0, 1.0e-7); improved = true; }
            else { g0Db = g0s; for (int i = 0; i < NS; ++i) gDb[i] = gs[i]; lambda *= 4.0; }
        }
        if (! improved) break;
    }
    double worst = 0.0;     // in the band that matters
    for (int k = 0; k < n; ++k) if (fHz[k] <= fMax) worst = std::max (worst, std::abs (analogueDb (fHz[k]) - tgt[k]));
    return worst;
}

void ShelfCascade::discretise (double fs)
{
    g0 = std::pow (10.0, g0Db / 20.0);
    for (int i = 0; i < NS; ++i)
    {
        const double A = std::pow (10.0, gDb[i] / 20.0);
        const double f = std::min (fc[i], 0.45 * fs);
        const double wc = 2.0 * PI * f;
        const double wz = wc / std::sqrt (A), wp = wc * std::sqrt (A);
        const double c = wc / std::tan (wc / (2.0 * fs));          // prewarped at the corner
        const double g = wp / wz;                                   // unity at DC
        const double d = c + wp;
        b0[i] = g * (c + wz) / d;
        b1[i] = g * (wz - c) / d;
        a1[i] = (wp - c) / d;
    }
}

} // namespace mt
