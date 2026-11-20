#include "de_find.h"
#include "de_filters.h"

#include <algorithm>
#include <cmath>

namespace de
{
    namespace { constexpr double kPi = 3.14159265358979323846; }

    void fft (std::vector<double>& re, std::vector<double>& im, bool inverse)
    {
        const size_t n = re.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * kPi / (double) len * (inverse ? 1.0 : -1.0);
            const double wr = std::cos (ang), wi = std::sin (ang);
            for (size_t i = 0; i < n; i += len)
            {
                double cr = 1.0, ci = 0.0;
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const size_t a = i + k, b = i + k + len / 2;
                    const double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr; im[a] += ti;
                    const double nr = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr; cr = nr;
                }
            }
        }
    }

    FindResult findFundamental (const float* x, int n, double fs, double lo, double hi)
    {
        FindResult r;
        if (n < (int) (0.2 * fs) || lo <= 0.0 || hi <= lo) return r;

        //  ---- the strokes: 5 ms energy frames, an onset where the energy
        //  jumps 9 dB over the recent floor and sits within 30 dB of the loudest
        const int fr = std::max (16, (int) (0.005 * fs));
        const int nf = n / fr;
        std::vector<double> e ((size_t) nf, 0.0);
        double emax = 0.0;
        for (int f = 0; f < nf; ++f)
        {
            double s = 0.0;
            for (int i = 0; i < fr; ++i) { const double v = x[f * fr + i]; s += v * v; }
            e[(size_t) f] = s / fr;
            emax = std::max (emax, e[(size_t) f]);
        }
        if (emax <= 1e-12) return r;

        std::vector<int> onsets;
        int lastOnset = -1000;
        for (int f = 1; f < nf; ++f)
        {
            double floorE = 1e-12;
            for (int k = std::max (0, f - 8); k < f; ++k) floorE = std::max (floorE, e[(size_t) k]);
            const bool jump = e[(size_t) f] > 8.0 * floorE && e[(size_t) f] > emax * 1e-3;
            //  a stroke is at least 60 ms after the previous one
            if (jump && (f - lastOnset) * fr > (int) (0.06 * fs)) { onsets.push_back (f * fr); lastOnset = f; }
        }
        if (onsets.empty()) return r;

        //  ---- the averaged spectrum of the strokes, just after the stick -----
        const int N = 1 << 15;                      // zero-padded to ~1.5 Hz bins
        const int skip = (int) (0.012 * fs);        // past the strike
        const int maxLen = std::min ((int) (0.35 * fs), N);
        std::vector<double> acc ((size_t) N / 2, 0.0), re ((size_t) N), im ((size_t) N);
        int used = 0;
        for (size_t k = 0; k < onsets.size(); ++k)
        {
            const int a = onsets[k] + skip;
            const int z = std::min ({ n, k + 1 < onsets.size() ? onsets[k + 1] : n, a + maxLen });
            const int len = z - a;
            if (len < (int) (0.05 * fs)) continue;
            std::fill (re.begin(), re.end(), 0.0);
            std::fill (im.begin(), im.end(), 0.0);
            double pw = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (len - 1));
                re[(size_t) i] = x[a + i] * w;
                pw += re[(size_t) i] * re[(size_t) i];
            }
            if (pw <= 1e-18) continue;
            fft (re, im);
            const double norm = 1.0 / std::sqrt (pw);        // every stroke counts equally
            for (int b = 0; b < N / 2; ++b) acc[(size_t) b] += std::hypot (re[(size_t) b], im[(size_t) b]) * norm;
            ++used;
        }
        if (used == 0) return r;

        //  ---- the strongest peak in the drum's range -------------------------
        const double bin = fs / N;
        const int b0 = std::max (2, (int) (lo / bin)), b1 = std::min (N / 2 - 2, (int) (hi / bin) + 1);
        int best = -1;
        for (int b = b0; b <= b1; ++b)
            if (acc[(size_t) b] > acc[(size_t) b - 1] && acc[(size_t) b] >= acc[(size_t) b + 1]
                && (best < 0 || acc[(size_t) b] > acc[(size_t) best])) best = b;
        if (best < 0) return r;

        const double ya = std::log (acc[(size_t) best - 1] + 1e-30), yb = std::log (acc[(size_t) best] + 1e-30),
                     yc = std::log (acc[(size_t) best + 1] + 1e-30);
        const double den = ya - 2.0 * yb + yc;
        const double off = std::fabs (den) > 1e-12 ? 0.5 * (ya - yc) / den : 0.0;
        r.hz = (best + std::max (-0.5, std::min (0.5, off))) * bin;
        r.hits = used;

        std::vector<double> range (acc.begin() + b0, acc.begin() + b1 + 1);
        std::nth_element (range.begin(), range.begin() + (long) range.size() / 2, range.end());
        const double med = std::max (1e-30, range[range.size() / 2]);
        r.confidence = 20.0 * std::log10 (acc[(size_t) best] / med);
        return r;
    }

    LevelResult measureLevels (const float* x, int n, double fs, double keyLo, double keyHi)
    {
        LevelResult r;
        if (n < (int) (0.1 * fs)) return r;

        //  the gate's detector, exactly: key band-pass, |.|, 10 ms peak decay
        const Coefs hp = design (F_HP, keyLo, fs, 0, 0.70710678118654752);
        const Coefs lp = design (F_LP, keyHi, fs, 0, 0.70710678118654752);
        Biquad bh, bl;
        const double aEnv = std::exp (-1.0 / (0.010 * fs));
        std::vector<float> env ((size_t) n);
        double e = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double k = std::fabs (bl.tick (lp, bh.tick (hp, (double) x[i])));
            e = std::max (k, e * aEnv);
            env[(size_t) i] = (float) e;
        }

        //  the events: the same energy onsets as FIND DRUM, but down to 40 dB
        //  under the loudest, so spill is seen as well as the drum
        const int fr = std::max (16, (int) (0.005 * fs));
        const int nf = n / fr;
        std::vector<double> en ((size_t) nf, 0.0);
        double emax = 0.0;
        for (int f = 0; f < nf; ++f)
        {
            double s = 0.0;
            for (int i = 0; i < fr; ++i) { const double v = x[f * fr + i]; s += v * v; }
            en[(size_t) f] = s / fr;
            emax = std::max (emax, en[(size_t) f]);
        }
        if (emax <= 1e-14) return r;
        std::vector<int> on;
        int last = -1000;
        for (int f = 1; f < nf; ++f)
        {
            double floorE = 1e-14;
            for (int k = std::max (0, f - 8); k < f; ++k) floorE = std::max (floorE, en[(size_t) k]);
            if (en[(size_t) f] > 8.0 * floorE && en[(size_t) f] > emax * 1e-4 && (f - last) * fr > (int) (0.06 * fs))
            { on.push_back (std::max (0, (f - 1) * fr)); last = f; }
        }
        if (on.empty()) return r;

        auto db = [] (double v) { return 20.0 * std::log10 (std::max (1e-10, v)); };
        struct Ev { int at; double pk, key; };
        std::vector<Ev> ev;
        for (size_t k = 0; k < on.size(); ++k)
        {
            const int a = on[k];
            const int z = std::min ({ n, a + (int) (0.06 * fs), k + 1 < on.size() ? on[k + 1] : n });
            double pk = 0.0, key = 0.0;
            for (int i = a; i < z; ++i) { pk = std::max (pk, (double) std::fabs (x[i])); key = std::max (key, (double) env[(size_t) i]); }
            ev.push_back ({ a, db (pk), db (key) });
        }
        double keyMax = -400.0;
        for (const auto& v : ev) keyMax = std::max (keyMax, v.key);

        std::vector<double> peaks;
        std::vector<int> ownAt;
        r.keyMinDb = 0.0;
        for (const auto& v : ev)
        {
            if (v.key >= keyMax - 12.0) { peaks.push_back (v.pk); ownAt.push_back (v.at); r.keyMinDb = std::min (r.keyMinDb, v.key); }
            else { ++r.spill; r.spillDb = std::max (r.spillDb, v.key); }
        }
        r.hits = (int) peaks.size();
        std::sort (peaks.begin(), peaks.end());
        r.peakDb = peaks.size() % 2 ? peaks[peaks.size() / 2] : 0.5 * (peaks[peaks.size() / 2 - 1] + peaks[peaks.size() / 2]);

        //  the floor between the drum's own hits, well after each one has rung
        //  (the 95th percentile, so a steady wash of spill counts and a click does not)
        std::vector<float> quiet;
        size_t o = 0;
        for (int i = 0; i < n; ++i)
        {
            while (o + 1 < ownAt.size() && ownAt[o + 1] <= i) ++o;
            const bool before = i < ownAt[0];
            const bool after = ownAt[o] <= i && i - ownAt[o] > (int) (0.4 * fs);
            if ((before || after) && (o + 1 >= ownAt.size() || ownAt[o + 1] - i > (int) (0.01 * fs)))
                quiet.push_back (env[(size_t) i]);
        }
        if (quiet.size() > (size_t) (0.05 * fs))
        {
            std::nth_element (quiet.begin(), quiet.begin() + (long) (quiet.size() * 95 / 100), quiet.end());
            r.spillDb = std::max (r.spillDb, db (quiet[quiet.size() * 95 / 100]));
        }
        return r;
    }

    void spectrumDb (const float* ring, int ringSize, int writePos, int size, std::vector<float>& out)
    {
        std::vector<double> re ((size_t) size), im ((size_t) size, 0.0);
        double wsum = 0.0;
        for (int i = 0; i < size; ++i)
        {
            const int k = (writePos - size + i + ringSize * 4) % ringSize;
            const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (size - 1));
            re[(size_t) i] = ring[k] * w;
            wsum += w;
        }
        fft (re, im);
        out.resize ((size_t) size / 2);
        const double norm = 2.0 / wsum;               // a full-scale sine reads 0 dB
        for (int b = 0; b < size / 2; ++b)
            out[(size_t) b] = (float) (20.0 * std::log10 (std::max (1e-12, std::hypot (re[(size_t) b], im[(size_t) b]) * norm)));
    }
}
