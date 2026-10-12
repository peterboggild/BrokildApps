/*  HOW DIFFERENT ARE THE SPECIMENS? Peter: "the presets sound very much the
    same ... the timbre of most presets is almost identical". This puts a
    number on it.

    For each specimen the same order is played (same note, same velocity, the
    default machine) and the held tone is reduced to a harmonic PROFILE: the
    level of harmonics 2..20 against the fundamental, in dB, plus how much of
    the energy sits OFF the harmonic grid and the spectral centroid. The
    distance between two specimens is the RMS dB difference of their profiles.

    The yardstick that makes the number mean something: the SAME specimen
    with one knob moved across its range (STEEPENING 0.1 vs 0.9) — a change
    anyone hears. If specimens sit far closer to each other than that, the
    catalogue is not carrying timbre.

    usage: ab104timbre [nSpecimens=24] [note=40] [character=-1 (default)]   */
#include "../Source/Engine.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace ab104;
static const int SR = 48000, BLK = 256;

static int pid (const char* id)
{ for (int i = 0; i < numParams(); ++i) if (std::strcmp (paramSpec (i).id, id) == 0) return i; return -1; }

static double goertzel (const std::vector<float>& x, size_t a, size_t b, double f)
{
    const double w = 2.0 * PI * f / SR, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    const size_t n = b - a;
    for (size_t i = 0; i < n; ++i)
    {
        const double hann = 0.5 - 0.5 * std::cos (2.0 * PI * (double) i / (double) (n - 1));
        const double s0 = x[a + i] * hann + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) n;
}

struct Prof { double h[21]; double centroid; double inharm; double rms; double attack; double strike; };

static Prof measure (int spec, int note, float vel, std::vector<std::pair<const char*, float>> set)
{
    Engine e; e.p = Params();
    for (auto& s : set) { int i = pid (s.first); if (i >= 0) paramSpec (i).get (e.p) = s.second; }
    e.prepare (SR, BLK);
    e.requestSpecimen (spec);
    std::vector<float> l (BLK), r (BLK), m;
    e.process (l.data(), r.data(), BLK);
    e.noteOn (note, vel);
    for (int b = 0; b < (int) (2.0 * SR / BLK); ++b)
    {
        e.process (l.data(), r.data(), BLK);
        for (int k = 0; k < BLK; ++k) m.push_back (0.5f * (l[k] + r[k]));
    }
    Prof p {};
    const double f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    const size_t a = (size_t) (0.7 * SR), b = (size_t) (1.9 * SR);
    double e2 = 0; for (size_t i = a; i < b; ++i) e2 += (double) m[i] * m[i];
    p.rms = std::sqrt (e2 / (double) (b - a));
    double hE = 0, cw = 0, cs = 0;
    const double h1 = std::max (1e-12, goertzel (m, a, b, f0));
    for (int k = 1; k <= 20; ++k)
    {
        const double v = goertzel (m, a, b, f0 * k);
        p.h[k] = std::max (-50.0, 20.0 * std::log10 (std::max (1e-12, v) / h1));
        hE += v * v; cw += v * v * k; cs += v * v;
    }
    //  off-grid energy: sample midway between harmonics and at the
    //  quarter points, compared with the harmonics' own energy
    double oE = 0;
    for (int k = 1; k <= 20; ++k)
        for (double q : { 0.25, 0.5, 0.75 })
        { const double v = goertzel (m, a, b, f0 * (k + q)); oE += v * v; }
    p.inharm = 10.0 * std::log10 (std::max (1e-20, oE) / std::max (1e-20, hE));
    p.centroid = cw / std::max (1e-20, cs);
    //  attack: time to 70 % of the held rms envelope
    double tgt = 0.7 * p.rms * 1.4142, t70 = 2.0;
    for (size_t i = 0; i < (size_t) (0.7 * SR); i += 48)
    {
        double pk = 0; for (size_t j = i; j < i + 480 && j < m.size(); ++j) pk = std::max (pk, (double) std::abs (m[j]));
        if (pk >= tgt) { t70 = (double) i / SR; break; }
    }
    p.attack = t70;
    //  the STRIKE: how much of the first 350 ms rings off the harmonic grid
    {
        const size_t s0 = (size_t) (0.01 * SR), s1 = (size_t) (0.36 * SR);
        double hh = 0, oo = 0;
        for (int k = 1; k <= 24; ++k)
        {
            const double v = goertzel (m, s0, s1, f0 * k); hh += v * v;
            for (double q : { 0.25, 0.5, 0.75 }) { const double w = goertzel (m, s0, s1, f0 * (k + q)); oo += w * w; }
        }
        p.strike = 10.0 * std::log10 (std::max (1e-20, oo) / std::max (1e-20, hh));
    }
    return p;
}

static double dist (const Prof& x, const Prof& y)
{
    double s = 0;
    for (int k = 2; k <= 20; ++k) { const double d = x.h[k] - y.h[k]; s += d * d; }
    return std::sqrt (s / 19.0);
}

int main (int argc, char** argv)
{
    const int nS   = argc > 1 ? std::atoi (argv[1]) : 24;
    const int note = argc > 2 ? std::atoi (argv[2]) : 40;
    const float ch = argc > 3 ? (float) std::atof (argv[3]) : -1.0f;
    std::vector<std::pair<const char*, float>> base;
    if (ch >= 0.0f) base.push_back ({ "character", ch });

    std::vector<Prof> P;
    std::printf ("spec  rms     cent  inharm  att    h2    h3    h4    h5    h6    h8   h12\n");
    for (int s = 0; s < nS; ++s)
    {
        const int spec = (s * 37 + 5) % specimenCount();
        P.push_back (measure (spec, note, 0.8f, base));
        const Prof& q = P.back();
        std::printf ("%-8s ", materialName (materialFamilyOf (spec)));
        std::printf ("%03d  %.4f  %5.2f  %6.1f  %.3f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f  strike %5.1f\n",
                     spec, q.rms, q.centroid, q.inharm, q.attack, q.h[2], q.h[3], q.h[4], q.h[5], q.h[6], q.h[8], q.h[12], q.strike);
    }
    std::vector<double> ds;
    for (int i = 0; i < nS; ++i) for (int j = i + 1; j < nS; ++j) ds.push_back (dist (P[i], P[j]));
    std::sort (ds.begin(), ds.end());
    double mean = 0; for (double d : ds) mean += d; mean /= std::max<size_t> (1, ds.size());
    double cMin = 1e9, cMax = -1e9, iMin = 1e9, iMax = -1e9, rMin = 1e9, rMax = -1e9;
    for (auto& q : P)
    {
        cMin = std::min (cMin, q.centroid); cMax = std::max (cMax, q.centroid);
        iMin = std::min (iMin, q.inharm);   iMax = std::max (iMax, q.inharm);
        rMin = std::min (rMin, q.rms);      rMax = std::max (rMax, q.rms);
    }

    auto yard = base; yard.push_back ({ "steepen", 0.10f });
    auto yard2 = base; yard2.push_back ({ "steepen", 0.90f });
    const Prof y1 = measure (5, note, 0.8f, yard), y2 = measure (5, note, 0.8f, yard2);

    std::printf ("\nspecimen-to-specimen profile distance: mean %.2f dB  median %.2f  10%% %.2f  90%% %.2f\n",
                 mean, ds[ds.size() / 2], ds[ds.size() / 10], ds[ds.size() * 9 / 10]);
    std::printf ("centroid %.2f..%.2f harmonics   off-grid %.1f..%.1f dB   level %.1f dB spread\n",
                 cMin, cMax, iMin, iMax, 20.0 * std::log10 (rMax / std::max (1e-12, rMin)));
    std::printf ("yardstick, STEEPENING 0.1 vs 0.9 on one specimen: %.2f dB\n", dist (y1, y2));
    return 0;
}
