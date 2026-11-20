#pragma once

// DRUMEQ - the filters. Every band is a second-order section whose MAGNITUDE
// is matched to the analog prototype, not bilinear-warped onto it.
//
// Why: the bilinear transform squeezes the whole analog axis into the half
// band below Nyquist, so a 10 kHz air shelf or a 12 kHz bell at 44.1 kHz comes
// out narrower and lower than asked ("cramping") - exactly where cymbals,
// snare crack and kick click live. Matching instead (M. Vicanek, "Matched
// Second Order Digital Filters", 2016):
//   * the POLES come from the analog poles by impulse invariance, so the
//     resonance sits where the analog one does;
//   * the numerator is solved so the digital magnitude equals the analog one
//     at DC, at Nyquist and at the centre frequency, and the in-between is
//     smooth because both are rational of the same order.
// A boost is designed as the matching cut and INVERTED: the cut's poles are
// always below its centre frequency, so a high shelf boosted near Nyquist
// never asks for a pole above Nyquist (which impulse invariance cannot place).
// That also makes a boost and a cut of the same size exact mirror images.
//
// Everything here is double precision; the sections run in transposed direct
// form II (one state pair per channel).

#include <cmath>

namespace de
{
    enum FType { F_LP, F_HP, F_PEAK, F_LSHELF, F_HSHELF };

    struct Coefs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;     // a0 = 1
        bool identity() const { return b0 == 1 && b1 == 0 && b2 == 0 && a1 == 0 && a2 == 0; }
    };

    struct Biquad
    {
        double z1 = 0, z2 = 0;
        inline double tick (const Coefs& c, double x)
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0; }
    };

    namespace detail
    {
        constexpr double kPi = 3.14159265358979323846;

        //  the analog prototype, normalised to w0 = 1, as numerator / denominator
        //  coefficients (s^2, s, 1)
        struct Proto { double n2, n1, n0, d2, d1, d0; };

        inline Proto proto (FType t, double gainDb, double Q)
        {
            const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
            switch (t)
            {
                case F_LP:     return { 0, 0, 1,          1, 1.0 / Q, 1 };
                case F_HP:     return { 1, 0, 0,          1, 1.0 / Q, 1 };
                case F_PEAK:   return { 1, A / Q, 1,      1, 1.0 / (A * Q), 1 };
                case F_LSHELF: return { A, A * sA / Q, A * A,   A, sA / Q, 1 };
                case F_HSHELF: return { A * A, A * sA / Q, A,   1, sA / Q, A };
            }
            return { 0, 0, 1, 0, 0, 1 };
        }

        //  |N(jw)|^2 for a quadratic in s
        inline double mag2 (double c2, double c1, double c0, double w)
        {
            const double re = c0 - c2 * w * w, im = c1 * w;
            return re * re + im * im;
        }
    }

    //  |H| of the analog prototype at frequency f (Hz), in dB
    inline double analogMagDb (FType t, double f0, double gainDb, double Q, double f)
    {
        const auto p = detail::proto (t, gainDb, Q);
        const double w = f / f0;
        const double m = detail::mag2 (p.n2, p.n1, p.n0, w) / std::fmax (1e-300, detail::mag2 (p.d2, p.d1, p.d0, w));
        return 10.0 * std::log10 (std::fmax (1e-30, m));
    }

    //  |H| of a digital section at frequency f, in dB
    inline double magDb (const Coefs& c, double f, double fs)
    {
        const double w = 2.0 * detail::kPi * f / fs;
        const double cw = std::cos (w), c2w = std::cos (2 * w), sw = std::sin (w), s2w = std::sin (2 * w);
        const double nr = c.b0 + c.b1 * cw + c.b2 * c2w, ni = -(c.b1 * sw + c.b2 * s2w);
        const double dr = 1.0 + c.a1 * cw + c.a2 * c2w, di = -(c.a1 * sw + c.a2 * s2w);
        return 10.0 * std::log10 (std::fmax (1e-30, (nr * nr + ni * ni) / std::fmax (1e-300, dr * dr + di * di)));
    }

    namespace detail
    {
        //  the numerator, given |N|^2 = B0 phi0 + B1 phi1 + B2 phi2 >= 0, as a
        //  MINIMUM-PHASE quadratic (zeros inside the unit circle). Minimum phase
        //  matters twice: it is the least smeared drum transient for a given
        //  magnitude, and the inverse of the section (a boost built from a
        //  cut) is then stable.
        inline void factor (double B0, double B1, double B2, double& b0, double& b1, double& b2)
        {
            const double s0 = std::sqrt (std::fmax (0.0, B0)), s1 = std::sqrt (std::fmax (0.0, B1));
            //  b0+b1+b2 = +-s0, b0-b1+b2 = +-s1; Fejer-Riesz guarantees one of
            //  the sign choices factors
            bool ok = false;
            for (int sg = 0; sg < 2 && ! ok; ++sg)
            {
                const double t1 = sg == 0 ? s1 : -s1;
                const double W = 0.5 * (s0 + t1), d = W * W + B2;
                if (d < -1e-14 * (1.0 + W * W + std::fabs (B2))) continue;
                b1 = 0.5 * (s0 - t1);
                b0 = 0.5 * (W + std::sqrt (std::fmax (0.0, d)));
                b2 = W - b0;
                ok = true;
            }
            if (! ok) { b0 = std::sqrt (std::fmax (0.0, 0.5 * (B0 + B1))); b1 = b2 = 0.0; return; }

            //  reflect any zero outside the unit circle inside, keeping |N|
            if (b0 == 0.0) { b0 = b1; b1 = b2; b2 = 0.0; }
            const double p = b1 / b0, q = b2 / b0, disc = p * p - 4.0 * q;
            if (disc >= 0.0)
            {
                double r1 = 0.5 * (-p + std::sqrt (disc)), r2 = 0.5 * (-p - std::sqrt (disc)), g = b0;
                if (std::fabs (r1) > 1.0) { g *= std::fabs (r1); r1 = 1.0 / r1; }
                if (std::fabs (r2) > 1.0) { g *= std::fabs (r2); r2 = 1.0 / r2; }
                b0 = g; b1 = -g * (r1 + r2); b2 = g * r1 * r2;
            }
            else if (std::fabs (q) > 1.0)       // complex pair outside: reflect both
            {
                //  b0 (z - r)(z - r*) has the magnitude of b0 |r|^2 (z - 1/r*)(z - 1/r)
                const double nb0 = b0 * q, nb1 = b0 * p, nb2 = b0;
                b0 = nb0; b1 = nb1; b2 = nb2;
            }
        }
    }

    //  the matched design: analog poles by impulse invariance, the numerator by
    //  a weighted least-squares fit of |H|^2 to the analog curve from DC to
    //  Nyquist (relative error, so a -18 dB notch counts as much as a +18 dB
    //  peak), with the centre frequency weighted ten times
    inline Coefs designMatched (FType t, double f0, double fs, double gainDb, double Q)
    {
        using namespace detail;
        const auto p = proto (t, gainDb, Q);
        const double w0 = 2.0 * kPi * f0 / fs;

        //  analog poles: d2 s^2 + d1 s + d0, normalised frequency units
        const double wp = std::sqrt (p.d0 / p.d2);                 // times w0
        const double zeta = (p.d1 / p.d2) / (2.0 * wp);
        const double wpd = wp * w0;
        double a1, a2;
        a2 = std::exp (-2.0 * zeta * wpd);
        if (zeta <= 1.0) a1 = -2.0 * std::exp (-zeta * wpd) * std::cos (std::sqrt (1.0 - zeta * zeta) * wpd);
        else             a1 = -2.0 * std::exp (-zeta * wpd) * std::cosh (std::sqrt (zeta * zeta - 1.0) * wpd);

        //  |A(e^jw)|^2 = A0 phi0 + A1 phi1 + A2 phi2, phi1 = sin^2(w/2)
        const double A0 = (1 + a1 + a2) * (1 + a1 + a2);
        const double A1 = (1 - a1 + a2) * (1 - a1 + a2);
        const double A2 = -4.0 * a2;

        auto analog2 = [&] (double wDigital)
        {
            const double w = wDigital / w0;
            return mag2 (p.n2, p.n1, p.n0, w) / std::fmax (1e-300, mag2 (p.d2, p.d1, p.d0, w));
        };

        Coefs c; c.a1 = a1; c.a2 = a2;

        //  the fitting grid: log spaced from 5 Hz to Nyquist, plus the centre
        constexpr int K = 64;
        double M[3][3] = {}, v[3] = {}, mh = 0, vh = 0;
        for (int k = 0; k <= K; ++k)
        {
            const double w = (k == K) ? w0 : 2.0 * kPi * (5.0 / fs) * std::pow ((0.5 * fs) / 5.0, (double) k / (K - 1));
            const double wt = (k == K) ? 10.0 : 1.0;
            const double x = std::sin (0.5 * w) * std::sin (0.5 * w);
            const double ph[3] = { 1.0 - x, x, 4.0 * x * (1.0 - x) };
            const double T = analog2 (w) * (A0 * ph[0] + A1 * ph[1] + A2 * ph[2]);
            if (T <= 1e-300) continue;
            for (int i = 0; i < 3; ++i)
            {
                const double ri = ph[i] / T;
                v[i] += wt * wt * ri;
                for (int j = 0; j < 3; ++j) M[i][j] += wt * wt * ri * (ph[j] / T);
            }
            const double xh = 16.0 * x * x / T;    // the high-pass's single unknown
            mh += wt * wt * xh * xh; vh += wt * wt * xh;
        }

        if (t == F_HP)
        {
            //  a high-pass keeps its double zero at DC: N = b0 (1 - z^-1)^2
            const double cc = vh / std::fmax (1e-300, mh);
            c.b0 = std::sqrt (std::fmax (0.0, cc));
            c.b1 = -2.0 * c.b0; c.b2 = c.b0;
            return c;
        }

        auto Qx = [] (const double* b, double x) { return b[0] * (1 - x) + b[1] * x + 4.0 * b[2] * x * (1 - x); };
        auto minQ = [&] (const double* b)
        {
            //  4 B2 x (1-x) has x^2 coefficient -4 B2: the quadratic is CONVEX
            //  (an interior minimum) when B2 < 0
            double mn = std::fmin (Qx (b, 0.0), Qx (b, 1.0));
            if (b[2] < 0.0)
            {
                const double xv = 0.5 + (b[1] - b[0]) / (8.0 * b[2]);
                if (xv > 0.0 && xv < 1.0) mn = std::fmin (mn, Qx (b, xv));
            }
            return mn;
        };

        //  FIRST CHOICE: match DC, Nyquist and the centre exactly (Vicanek).
        //  A treble band then cannot move the low end of the drum, nor a bass
        //  band its top, and the centre gain is what the knob says.
        double B[3] = { A0 * analog2 (0.0), A1 * analog2 (kPi), 0.0 };
        {
            const double s = std::sin (0.5 * w0), x0 = s * s, ph2 = 4.0 * x0 * (1.0 - x0);
            if (ph2 > 1e-12)
                B[2] = (analog2 (w0) * (A0 * (1 - x0) + A1 * x0 + A2 * ph2) - B[0] * (1 - x0) - B[1] * x0) / ph2;
        }

        //  ...unless |N|^2 would go negative somewhere (no real filter has
        //  that magnitude): then the least-squares fit over the whole band
        if (minQ (B) < 0.0)
        {
            auto det3 = [] (double a[3][3])
            {
                return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
                     - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                     + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
            };
            const double D = det3 (M);
            for (int i = 0; i < 3; ++i)
            {
                double Mi[3][3];
                for (int r = 0; r < 3; ++r) for (int q = 0; q < 3; ++q) Mi[r][q] = (q == i) ? v[r] : M[r][q];
                B[i] = det3 (Mi) / D;
            }
        }

        //  |N|^2 must not go negative anywhere on the circle: Q(x) is a
        //  quadratic in x on [0,1]; lift it uniformly if its minimum dips
        //  (phi0 + phi1 = 1, so adding d to B0 and B1 adds d everywhere)
        {
            const double mn = minQ (B);
            if (mn < 0.0) { B[0] -= mn * 1.000001; B[1] -= mn * 1.000001; }
        }

        factor (B[0], B[1], B[2], c.b0, c.b1, c.b2);
        return c;
    }

    //  the RBJ cookbook design (bilinear, prewarped at the corner): exact at
    //  the corner and in double precision well conditioned at ANY frequency.
    //  Its only fault is cramping near Nyquist, which is negligible for a
    //  corner below fs/32 (the matched design takes over above that).
    inline Coefs designBilinear (FType t, double f0, double fs, double gainDb, double Q)
    {
        const double w0 = 2.0 * detail::kPi * f0 / fs, cw = std::cos (w0), sw = std::sin (w0);
        const double A = std::pow (10.0, gainDb / 40.0), al = sw / (2.0 * Q), sA = std::sqrt (A);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (t)
        {
            case F_LP:  b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case F_HP:  b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
            case F_PEAK: b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
            case F_LSHELF:
                b0 = A * ((A + 1) - (A - 1) * cw + 2 * sA * al);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - 2 * sA * al);
                a0 = (A + 1) + (A - 1) * cw + 2 * sA * al;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - 2 * sA * al;
                break;
            case F_HSHELF:
                b0 = A * ((A + 1) + (A - 1) * cw + 2 * sA * al);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - 2 * sA * al);
                a0 = (A + 1) - (A - 1) * cw + 2 * sA * al;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - 2 * sA * al;
                break;
        }
        Coefs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }

    //  where the two designs hand over (as a fraction of the sample rate)
    constexpr double kMatchFrom = 1.0 / 32.0;

    //  the band as the engine uses it: identity at 0 dB (bit-exact). Below
    //  fs/32 the cookbook design; above it the matched one - except the
    //  high-pass (always low, where the cookbook is exact) and a low-pass
    //  cornered above 0.4 fs (impulse invariance places a pole that close to
    //  Nyquist badly; its skirt is above 20 kHz anyway). Of the matched
    //  boost and cut, the version whose poles are LOWER is designed and the
    //  other is its exact inverse, so no pole is ever asked for above Nyquist
    //  (a high shelf's boost and a low shelf's cut put them above the corner).
    inline Coefs design (FType t, double f0, double fs, double gainDb, double Q)
    {
        f0 = std::fmin (std::fmax (f0, 5.0), 0.48 * fs);
        Q = std::fmin (std::fmax (Q, 0.1), 20.0);
        const bool eqType = (t == F_PEAK || t == F_LSHELF || t == F_HSHELF);
        if (eqType && gainDb == 0.0) return {};
        if (f0 < kMatchFrom * fs || t == F_HP || (t == F_LP && f0 > 0.4 * fs)) return designBilinear (t, f0, fs, gainDb, Q);
        //  (a bell's cut has overdamped poles that impulse invariance places
        //  badly near Nyquist; its boost has clean resonant ones, so a cut is
        //  the inverted boost)
        const bool invert = (t == F_HSHELF && gainDb > 0.0) || (t == F_LSHELF && gainDb < 0.0)
                         || (t == F_PEAK && gainDb < 0.0);
        if (invert)
        {
            const Coefs c = designMatched (t, f0, fs, -gainDb, Q);
            Coefs r;
            r.b0 = 1.0 / c.b0;
            r.b1 = c.a1 / c.b0;
            r.b2 = c.a2 / c.b0;
            r.a1 = c.b1 / c.b0;
            r.a2 = c.b2 / c.b0;
            return r;
        }
        return designMatched (t, f0, fs, gainDb, Q);
    }

    //  stable, i.e. both poles strictly inside the unit circle
    inline bool stable (const Coefs& c)
    {
        return std::fabs (c.a2) < 1.0 && std::fabs (c.a1) < 1.0 + c.a2;
    }
}
