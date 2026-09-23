/*  ARTEFACT B2311.67 — the bench.

    This instrument makes a claim: that its spectrum is neither discrete nor
    continuous but the third thing, a Cantor set, and that energy on it spreads
    anomalously. Proving the output is bounded proves none of that. So the
    bench measures the claim.

    Build:  cmake -S test -B test/build ; cmake --build test/build --config Release
*/

#include "../Source/Engine.h"
#include "../Source/Lattice.h"
#include "../Source/Modulation.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <string>

// the same mapping the engine uses for CEILING
static double juceLikeCeil (float sat)
{
    const double c = 0.35 + 0.9 * (1.0 - std::max (0.0f, std::min (1.0f, sat)));
    return c < 0.10 ? 0.10 : (c > 0.94 ? 0.94 : c);
}

static int checks = 0, fails = 0;
static void ok (bool c, const char* what, const char* detail = "")
{
    ++checks;
    if (! c) { ++fails; std::printf ("  FAIL  %s   %s\n", what, detail); }
}
static void head (const char* s) { std::printf ("\n== %s ==\n", s); }

//==============================================================================
//  Sturm sequence: how many eigenvalues of the symmetric tridiagonal
//  M^-1/2 K M^-1/2 lie below lambda. This is the tool that measures a Cantor
//  spectrum — count(lambda) is the integrated density of states, and the
//  intervals where it does not move are the gaps.
struct Tri
{
    std::vector<double> d, e;      // diagonal, off-diagonal (e[i] joins i-1,i)
    int count (double lam) const
    {
        int neg = 0;
        double q = d[0] - lam;
        if (q < 0) ++neg;
        for (size_t i = 1; i < d.size(); ++i)
        {
            if (std::abs (q) < 1e-300) q = 1e-300;
            q = d[i] - lam - e[i] * e[i] / q;
            if (q < 0) ++neg;
        }
        return neg;
    }
};

// K x = w^2 M x, symmetrised
static Tri triFrom (const std::vector<double>& m, const std::vector<double>& k)
{
    const int n = (int) m.size();
    Tri t; t.d.resize ((size_t) n); t.e.assign ((size_t) n, 0.0);
    for (int i = 0; i < n; ++i) t.d[(size_t) i] = (k[(size_t) i] + k[(size_t) i + 1]) / m[(size_t) i];
    for (int i = 1; i < n; ++i) t.e[(size_t) i] = -k[(size_t) i] / std::sqrt (m[(size_t) i - 1] * m[(size_t) i]);
    return t;
}

struct GapReport { int gaps; double gapMeasure, bandMeasure, hi; };

static GapReport measureGaps (const Tri& t, int n, double hi, double minWidthFrac)
{
    const int STEPS = 24000;
    std::vector<int> c ((size_t) STEPS + 1);
    for (int i = 0; i <= STEPS; ++i) c[(size_t) i] = t.count (hi * (double) i / (double) STEPS);
    GapReport r { 0, 0.0, 0.0, hi };
    const double dl = hi / (double) STEPS;
    int run = 0;
    for (int i = 1; i <= STEPS; ++i)
    {
        if (c[(size_t) i] == c[(size_t) (i - 1)] && c[(size_t) i] > 0 && c[(size_t) i] < n) ++run;
        else
        {
            if (run > 0)
            {
                const double w = run * dl;
                if (w > minWidthFrac * hi) { ++r.gaps; r.gapMeasure += w; }
            }
            run = 0;
        }
    }
    if (run > 0) { const double w = run * dl; if (w > minWidthFrac * hi) { ++r.gaps; r.gapMeasure += w; } }
    r.bandMeasure = hi - r.gapMeasure;
    return r;
}

//==============================================================================
static void chainArrays (const std::vector<ab::ChainSite>& cut, const ab::Habit& h,
                         std::vector<double>& m, std::vector<double>& k)
{
    const int n = (int) cut.size();
    m.assign ((size_t) n, 1.0); k.assign ((size_t) n + 1, 1.0);
    double mean = 0.0;
    for (int i = 0; i + 1 < n; ++i) mean += cut[(size_t) (i + 1)].s - cut[(size_t) i].s;
    mean /= std::max (1, n - 1);
    for (int i = 0; i < n; ++i) m[(size_t) i] = std::pow (h.contrast, cut[(size_t) i].field - 0.5);
    for (int i = 0; i <= n; ++i)
    {
        double d = mean;
        if (i > 0 && i < n) d = cut[(size_t) i].s - cut[(size_t) (i - 1)].s;
        k[(size_t) i] = std::pow (std::max (1e-3, d / mean), -h.bondExp);
    }
}

//==============================================================================
int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    std::printf ("ARTEFACT B2311.67 — bench\n");
    std::printf ("silver ratio 1+sqrt2 = %.10f   Pell extents 5 12 29 70 169 408\n", ab::SILVER);

    //--------------------------------------------------------------------
    head ("1 · the cut-and-project is really the octagonal tiling");
    {
        ab::Window w; ab::Habit h = ab::habitOf (0);
        ab::Patch pt;
        ab::buildPatch (pt, w, h, 0.0, 0.0, 12.0, 0);
        int nBase = 0; for (auto& s : pt.sites) if (s.level == 0) ++nBase;
        std::printf ("  vertices %d   edges %d   tiles %d\n", nBase, (int) pt.edges.size(), (int) pt.tiles.size());
        ok (nBase > 400, "the aperture holds a real patch");
        ok (pt.edges.size() > (size_t) nBase, "vertices are joined into a tiling");

        int sq = 0, rh = 0;
        for (auto& t : pt.tiles) (t.kind == 1 ? sq : rh)++;
        const double ratio = rh > 0 ? (double) sq / (double) rh : 0.0;
        std::printf ("  squares %d  rhombi %d   rhombi/square = %.4f   (Ammann-Beenker: sqrt2 = %.4f)\n",
                     sq, rh, sq > 0 ? (double) rh / sq : 0.0, ab::SQRT2);
        ok (std::abs ((double) rh / std::max (1, sq) - ab::SQRT2) < 0.18,
            "rhombi outnumber squares by sqrt2, as Ammann-Beenker requires");
        (void) ratio;

        // edge directions: four, equally used
        int dirCount[4] = { 0,0,0,0 };
        for (auto& e : pt.edges)
        {
            const auto& a = pt.sites[(size_t) e.a]; const auto& b = pt.sites[(size_t) e.b];
            double an = std::atan2 (b.y - a.y, b.x - a.x); if (an < 0) an += ab::PI;
            int d = (int) std::lround (an / (ab::PI * 0.25)) % 4;
            ++dirCount[d];
        }
        int lo = 1 << 30, hi = 0;
        for (int i = 0; i < 4; ++i) { lo = std::min (lo, dirCount[i]); hi = std::max (hi, dirCount[i]); }
        std::printf ("  edge directions  %d %d %d %d\n", dirCount[0], dirCount[1], dirCount[2], dirCount[3]);
        ok (hi < lo * 1.25 + 20, "the four edge directions are equally represented (eightfold)");

        // the artefact contains itself: shrinking the window by the silver
        // ratio must select about 1/silver^2 of the vertices
        ab::Patch p2; ab::buildPatch (p2, w, h, 0.0, 0.0, 12.0, 2);
        int n1 = 0, n2 = 0; for (auto& s : p2.sites) { if (s.level == 1) ++n1; if (s.level == 2) ++n2; }
        const double r1 = (double) n1 / nBase, r2 = (double) n2 / std::max (1, n1);
        std::printf ("  inflation densities  L1/L0 = %.4f  L2/L1 = %.4f   (1/silver^2 = %.4f)\n",
                     r1, r2, 1.0 / (ab::SILVER * ab::SILVER));
        ok (std::abs (r1 - 1.0 / (ab::SILVER * ab::SILVER)) < 0.045, "the first inflation has the right density");
        ok (std::abs (r2 - 1.0 / (ab::SILVER * ab::SILVER)) < 0.075, "the second inflation has the right density");
    }

    //--------------------------------------------------------------------
    head ("2 · the cut is a silver-mean quasiperiodic chain");
    {
        ab::Window w; ab::Habit h = ab::habitOf (0);
        std::vector<ab::ChainSite> cut;
        ab::buildChain (cut, w, h, 0.0, 0.0, 0.0, 0.0, 400, 0.62);
        std::printf ("  sites %d   span %.2f\n", (int) cut.size(), cut.back().s - cut.front().s);
        ok (cut.size() >= 380, "the cut reaches the length it was asked for");

        // spacings must take exactly two values in the ratio 1+sqrt2
        std::vector<double> sp;
        for (size_t i = 1; i < cut.size(); ++i) sp.push_back (cut[i].s - cut[i - 1].s);
        std::sort (sp.begin(), sp.end());
        std::vector<std::pair<double,int>> cl;
        for (double v : sp)
        {
            if (! cl.empty() && std::abs (v - cl.back().first) < 1e-6) ++cl.back().second;
            else cl.push_back ({ v, 1 });
        }
        std::printf ("  distinct spacings: %d  ->", (int) cl.size());
        for (auto& c : cl) std::printf ("  %.6f x%d", c.first, c.second);
        std::printf ("\n");
        ok (cl.size() == 2, "exactly two spacings — the two letters of the word");
        if (cl.size() == 2)
        {
            const double rr = cl[1].first / cl[0].first;
            const double freq = (double) cl[0].second / std::max (1, cl[1].second);
            std::printf ("  long/short = %.6f  (1+sqrt2 = %.6f)   count ratio short/long = %.4f\n",
                         rr, ab::SILVER, freq);
            ok (std::abs (rr - ab::SILVER) < 1e-4 || std::abs (rr - ab::SQRT2) < 1e-4,
                "the two spacings stand in the silver ratio");
        }

        // and the word must not repeat: subword complexity of a quasiperiodic
        // word grows with length; a periodic one saturates
        std::string word;
        for (double v : { 0.0 }) (void) v;
        {
            double lo = 1e9, hi2 = 0;
            for (size_t i = 1; i < cut.size(); ++i) { const double d = cut[i].s - cut[i-1].s; lo = std::min (lo, d); hi2 = std::max (hi2, d); }
            for (size_t i = 1; i < cut.size(); ++i) word += ((cut[i].s - cut[i-1].s) > 0.5 * (lo + hi2)) ? 'L' : 'S';
        }
        int cx[9] {};
        for (int L = 1; L <= 8; ++L)
        {
            std::set<std::string> subs;
            for (size_t i = 0; i + (size_t) L <= word.size(); ++i) subs.insert (word.substr (i, (size_t) L));
            cx[L] = (int) subs.size();
        }
        std::printf ("  subword complexity  p(1..8) = %d %d %d %d %d %d %d %d\n",
                     cx[1],cx[2],cx[3],cx[4],cx[5],cx[6],cx[7],cx[8]);
        ok (cx[8] > cx[4] && cx[4] > cx[2], "complexity keeps growing — the word never repeats");
        ok (cx[8] <= 40, "and grows slowly — this is order, not randomness");
    }

    //--------------------------------------------------------------------
    head ("3 · THE CLAIM: the spectrum is a Cantor set, not a comb and not a band");
    {
        /*  The trap this test fell into first time round: in a chain of N
            sites there are only N eigenvalues, so between every neighbouring
            PAIR there is an interval where the counting function does not
            move. At N = 377 the mean level spacing is 0.27 % of the range, so
            a gap threshold of 0.15 % counted level spacings and reported that
            a period-two crystal had 222 "gaps". It has one.

            A real spectral gap keeps its width as N grows; a level spacing
            shrinks like 1/N. So: measure at two lengths, threshold well above
            the level spacing at the larger, and check the count survives. */
        ab::Window w;
        ab::Habit h = ab::habitOf (0);
        h.crisp = 0.96; h.levels = 2;              // two letters, the classical case
        h.contrast = 4.0; h.bondExp = 1.6;

        auto arrange = [&] (int n, int mode, std::vector<double>& m, std::vector<double>& k)
        {
            std::vector<ab::ChainSite> cut;
            ab::buildChain (cut, w, h, 0.0, 0.0, 0.0, 0.0, n, 0.62);
            chainArrays (cut, h, m, k);
            const int N = (int) m.size();
            if (mode == 1)   // the same two letters, laid periodically
            {
                double a = m[0], b = a; for (double q : m) if (std::abs (q - a) > 1e-9) { b = q; break; }
                double ka = k[1], kb = ka; for (size_t i = 1; i + 1 < k.size(); ++i) if (std::abs (k[i] - ka) > 1e-9) { kb = k[i]; break; }
                for (int i = 0; i < N; ++i) m[(size_t) i] = (i & 1) ? b : a;
                for (int i = 0; i <= N; ++i) k[(size_t) i] = (i & 1) ? kb : ka;
            }
            else if (mode == 2)   // the same two letters, shuffled
            {
                unsigned s = 12345; auto rnd = [&] { s = s * 1664525u + 1013904223u; return (s >> 16) & 0x7fff; };
                for (int i = N - 1; i > 0; --i) std::swap (m[(size_t) i], m[(size_t) (rnd() % (unsigned) (i + 1))]);
                for (int i = N; i > 0; --i)     std::swap (k[(size_t) i], k[(size_t) (rnd() % (unsigned) (i + 1))]);
            }
            return N;
        };

        /*  The fingerprint. A Cantor set has gaps at EVERY scale, so the count
            of gaps wider than eps grows as a power law in 1/eps. A crystal has
            a fixed handful however fine you look. That difference is the whole
            claim, and it is visible in one table. */
        const char* names[3] = { "quasiperiodic", "periodic     ", "disordered   " };
        const double EPS[4] = { 0.040, 0.020, 0.010, 0.005 };   // all >> level spacing at N=2378
        int    gapsE[3][4] {};
        int    gapsSmall[3] {};
        double meas[3] {};

        for (int mode = 0; mode < 3; ++mode)
        {
            std::vector<double> m, k;
            const int n = arrange (2378, mode, m, k);
            double hi = 0; for (int i = 0; i < n; ++i) hi = std::max (hi, 2.0 * (k[(size_t)i] + k[(size_t)i+1]) / m[(size_t)i]);
            const Tri t = triFrom (m, k);
            for (int e = 0; e < 4; ++e)
            {
                auto r = measureGaps (t, n, hi * 1.02, EPS[e]);
                gapsE[mode][e] = r.gaps;
                if (e == 3) meas[mode] = r.gapMeasure / r.hi;
            }
            std::vector<double> m2, k2;
            const int n2 = arrange (408, mode, m2, k2);
            double hi2 = 0; for (int i = 0; i < n2; ++i) hi2 = std::max (hi2, 2.0 * (k2[(size_t)i] + k2[(size_t)i+1]) / m2[(size_t)i]);
            gapsSmall[mode] = measureGaps (triFrom (m2, k2), n2, hi2 * 1.02, 0.010).gaps;
        }

        std::printf ("  gaps wider than   4%%   2%%   1%%  0.5%%  of the range      | at 1%%, N=408\n");
        for (int mode = 0; mode < 3; ++mode)
            std::printf ("  %s   %3d  %3d  %3d  %3d     measure %5.1f %%  |  %3d\n",
                         names[mode], gapsE[mode][0], gapsE[mode][1], gapsE[mode][2], gapsE[mode][3],
                         100.0 * meas[mode], gapsSmall[mode]);

        ok (gapsE[1][3] <= 3, "a period-two crystal has one gap, and looking finer does not find more");
        ok (gapsE[0][3] > 4 * std::max (1, gapsE[1][3]), "the artefact's spectrum is riddled with gaps the crystal does not have");
        ok (gapsE[0][3] > (int) (1.8 * gapsE[0][1]), "and the count keeps GROWING as we look finer — gaps at every scale");
        ok (gapsE[1][3] <= gapsE[1][1] + 2, "...which the crystal's does not do");
        ok (meas[0] > 0.35, "the bands are a thin set: most of the range is gap");
        ok (meas[0] > 3.0 * meas[2], "a disordered chain of the same two letters has almost no gap at all");
        ok (std::abs (gapsE[0][2] - gapsSmall[0]) <= std::max (3, gapsE[0][2] / 3),
            "the gaps survive the chain growing six times longer — spectral, not finite-size");
        std::printf ("  gaps at every scale, surviving N, is what a Cantor spectrum IS.\n");
    }

    //--------------------------------------------------------------------
    head ("4 · transport is anomalous — neither ballistic nor localised");
    {
        /*  The trap here, first time round: with dt at the stability limit the
            packet crosses a site per step, so on a 601-site chain it hits the
            wall after ~300 steps. Sampling from step 1500 to 24000 measured
            nothing but the reflections sloshing, and reported beta < 0 for a
            uniform chain, which is ballistic by definition. Measure while the
            packet is still in flight. */
        auto spread = [] (const std::vector<double>& m, const std::vector<double>& k, int n, double& beta)
        {
            std::vector<double> u ((size_t) n, 0.0), v ((size_t) n, 0.0);
            const int c = n / 2;
            /*  A BROADBAND packet will not show this. In one dimension the
                long-wavelength end propagates ballistically whatever the
                medium, so a delta or a plain Gaussian is dominated by the part
                that always travels, and all three chains measure beta = 1.
                Excite a narrow band in the middle of the spectrum instead. */
            for (int i = 0; i < n; ++i)
            {
                const double d = (double) (i - c) / 7.0;
                v[(size_t) i] = std::exp (-0.5 * d * d) * std::cos (1.5707963 * (double) (i - c));
            }
            double wmax = 0; for (int i = 0; i < n; ++i) wmax = std::max (wmax, 2.0 * (k[(size_t)i]+k[(size_t)i+1])/m[(size_t)i]);
            const double dt = 1.2 / std::sqrt (wmax);
            const int SAMP[7] = { 90, 145, 235, 380, 610, 985, 1590 };   // still well inside the walls
            std::vector<std::pair<double,double>> pts;
            int next = 0;
            for (int step = 1; step <= SAMP[6]; ++step)
            {
                double up = 0.0, uc = u[0];
                for (int i = 0; i < n; ++i)
                {
                    const double un = (i + 1 < n) ? u[(size_t) (i + 1)] : 0.0;
                    const double f = k[(size_t) i] * (up - uc) + k[(size_t) (i + 1)] * (un - uc);
                    v[(size_t) i] += dt * f / m[(size_t) i];
                    u[(size_t) i] = uc + dt * v[(size_t) i];
                    up = uc; uc = un;
                }
                if (next < 7 && step == SAMP[next])
                {
                    ++next;
                    double s0 = 0, s2 = 0;
                    for (int i = 0; i < n; ++i)
                    {
                        const double e = m[(size_t)i] * v[(size_t)i] * v[(size_t)i] + k[(size_t)i] * u[(size_t)i] * u[(size_t)i];
                        s0 += e; s2 += e * (double)(i - c) * (double)(i - c);
                    }
                    if (s0 > 0) pts.push_back ({ std::log ((double) step), 0.5 * std::log (s2 / s0) });
                }
            }
            double sx=0,sy=0,sxx=0,sxy=0; const double N=(double)pts.size();
            for (auto& p : pts) { sx+=p.first; sy+=p.second; sxx+=p.first*p.first; sxy+=p.first*p.second; }
            beta = (N*sxy - sx*sy) / std::max (1e-12, (N*sxx - sx*sx));
        };

        ab::Window w; ab::Habit h = ab::habitOf (0);
        h.crisp = 0.96; h.levels = 2; h.contrast = 6.0; h.bondExp = 1.6;
        std::vector<ab::ChainSite> cut; ab::buildChain (cut, w, h, 0,0, 0,0, 4181, 0.62);
        const int n = (int) cut.size();
        std::vector<double> m, k; chainArrays (cut, h, m, k);
        double bQ = 0, bP = 0, bR = 0;
        spread (m, k, n, bQ);

        std::vector<double> mp = m, kp = k;
        for (int i = 0; i < n; ++i) mp[(size_t) i] = 1.0;
        for (int i = 0; i <= n; ++i) kp[(size_t) i] = 1.0;
        spread (mp, kp, n, bP);

        std::vector<double> mr = m, kr = k;
        unsigned s = 999; auto rnd = [&] { s = s*1664525u+1013904223u; return (s>>16)&0x7fff; };
        for (int i = n - 1; i > 0; --i) std::swap (mr[(size_t)i], mr[(size_t)(rnd()%(unsigned)(i+1))]);
        for (int i = n; i > 0; --i)     std::swap (kr[(size_t)i], kr[(size_t)(rnd()%(unsigned)(i+1))]);
        spread (mr, kr, n, bR);

        std::printf ("  wavepacket spreading exponent  sigma ~ t^beta\n");
        std::printf ("    uniform (ballistic)   beta = %.3f\n", bP);
        std::printf ("    quasiperiodic         beta = %.3f   <-- the artefact\n", bQ);
        std::printf ("    disordered (locked)   beta = %.3f\n", bR);
        ok (bP > 0.85, "a uniform chain is ballistic, as it must be");
        ok (bQ < bP - 0.05 && bQ > bR + 0.02, "the artefact transports anomalously: slower than a crystal, faster than a glass");
    }

    //--------------------------------------------------------------------
    head ("5 · the star: partials, and the rule that a partial is loud when its shadow is small");
    {
        ab::Habit h = ab::habitOf (0); h.starWid = 1.3; h.starTilt = 0.3;
        std::vector<ab::Peak> pk; ab::buildStar (pk, h, 64);
        std::printf ("  orders %d   lambda %.4f .. %.4f\n", (int) pk.size(), pk.front().lam, pk.back().lam);
        ok (pk.size() == 64, "the requested number of orders is produced");
        bool sawUnity = false, allModule = true;
        for (auto& p : pk)
        {
            // every lambda must be p + q*sqrt2 for small integers, i.e. its
            // conjugate must reconstruct: (lam+conj)/2 and (lam-conj)/(2sqrt2) integers
            const double P = 0.5 * (p.lam + p.conj), Q = (p.lam - p.conj) / (2.0 * ab::SQRT2);
            if (std::abs (P - std::round (P)) > 1e-9 || std::abs (Q - std::round (Q)) > 1e-9) allModule = false;
            if (std::abs (p.lam - 1.0) < 1e-9) sawUnity = true;
        }
        ok (allModule, "every order lies in the module Z + Z*sqrt2");
        ok (sawUnity, "the fundamental itself is among them");
        // correlation between smallness of |conj| and loudness
        double best = 0; double bestConj = 99;
        for (auto& p : pk) if (p.amp > best) { best = p.amp; bestConj = std::abs (p.conj); }
        std::printf ("  loudest order has |shadow| = %.4f ; mean |shadow| = ", bestConj);
        double mc = 0; for (auto& p : pk) mc += std::abs (p.conj); mc /= pk.size();
        std::printf ("%.4f\n", mc);
        ok (bestConj < mc, "the loudest order has a smaller shadow than average");
    }

    //--------------------------------------------------------------------
    head ("5b · OBLIQUITY: the slope of the cut, and the staircase it climbs");
    {
        /*  This control was dead for a while, and the first two ways of wiring
            it were measured and thrown away -- one scrambled the chain, the
            other tore the star away from the filament it is supposed to be the
            diffraction OF. What is here now is a linear phason strain: the
            acceptance window slides through the unseen plane as you travel
            along the cut. Four things have to be true of it and none of them
            may be assumed.  */
        auto sig = [] (const std::vector<ab::Peak>& pk)
        {
            std::vector<double> b (20, 0.0); double tot = 0.0;
            for (const auto& p : pk)
            {
                if (p.lam < 0.25 || p.lam > 32.0) continue;
                int i = (int) (std::log2 (p.lam / 0.25) / 7.0 * 20.0);
                i = std::max (0, std::min (19, i)); b[(size_t) i] += p.amp; tot += p.amp;
            }
            if (tot > 0) for (auto& v : b) v /= tot;
            return b;
        };
        auto sigDist = [] (const std::vector<double>& a, const std::vector<double>& b)
        { double s = 0; for (size_t i = 0; i < a.size(); ++i) s += std::abs (a[i]-b[i]); return s*100.0; };
        auto cents = [] (const std::vector<ab::Peak>& pk)
        {
            std::vector<ab::Peak> v = pk;
            std::sort (v.begin(), v.end(), [](const ab::Peak&a,const ab::Peak&b){return a.amp>b.amp;});
            std::vector<double> lam;
            for (int i = 0; i < 10 && i < (int) v.size(); ++i) lam.push_back (v[(size_t)i].lam);
            if (lam.size() < 4) return 1.0e9;
            std::sort (lam.begin(), lam.end());
            double best = 1.0e9;
            for (size_t f = 0; f < lam.size(); ++f)
                for (int m = 1; m <= 6; ++m)
                {
                    const double f0 = lam[f] / (double) m; if (f0 < 1e-6) continue;
                    double sum = 0; int n = 0;
                    for (double L : lam) { const double r = L/f0; if (r < 0.5) continue;
                        const double nr = std::max (1.0, std::floor (r+0.5));
                        sum += std::pow (1200.0*std::log2 (r/nr), 2.0); ++n; }
                    if (n >= 4) best = std::min (best, std::sqrt (sum/(double)n));
                }
            return best;
        };

        ab::Window w; w.scale = 1.0;
        const int NS = 13;
        int habits = 0, lostSites = 0, becameHarmonic = 0;
        std::vector<double> excursion;   // cents from the note to the loudest partial
        double worstGap = 1e9, leastMove = 1e9, worstCentsAtRest = 0, bestCentsAnywhere = 1e9;

        for (int hIdx = 0; hIdx < 256; hIdx += 8)
        {
            const ab::Habit h = ab::habitOf (hIdx);
            const int want = ab::PELL[std::max (0, std::min (5, h.extent))];
            const double bearing = (double) h.cutIndex / 8.0 * ab::PI;

            std::vector<ab::ChainSite> ch0;
            ab::buildChain (ch0, w, h, 0, 0, bearing, 0, want, 0.62, 0.0);
            if (ch0.size() < 16) continue;
            std::vector<ab::Peak> pk0; ab::buildStar (pk0, h, 24, &ch0, h.contrast);
            const auto s0 = sig (pk0);
            const double c0 = cents (pk0);
            ++habits;
            worstCentsAtRest = std::max (worstCentsAtRest, c0 > 1e8 ? 0.0 : c0);

            double move = 0.0, bestC = c0;
            for (int k = 1; k < NS; ++k)
            {
                const double strain = (double) k / (double) (NS-1) * 0.06;
                std::vector<ab::ChainSite> ch;
                ab::buildChain (ch, w, h, 0, 0, bearing, 0, want, 0.62, strain);
                std::vector<ab::Peak> pk; ab::buildStar (pk, h, 24, &ch, h.contrast);

                if (ch.size() != ch0.size()) ++lostSites;
                for (size_t i = 1; i < ch.size(); ++i)
                    worstGap = std::min (worstGap, ch[i].s - ch[i-1].s);

                /*  WHICH partial leads. Note carefully what this is NOT: the
                    note itself cannot move, because every lambda is a ratio to
                    the key that was struck. What can move is which member of
                    the chord is loudest, and this artefact has always let that
                    wander -- measured on the enumerated star this replaced,
                    the loudest partial was something other than the
                    fundamental for 8.3 % of the catalogue at rest, worst case
                    3052 cents. So the thing to hold is not "the fundamental
                    always leads", which was never true of this instrument, but
                    that leaning the cut does not turn a rare excursion into
                    the normal case. */
                double lo = 0, la = -1;
                for (const auto& p : pk) if (p.amp > la) { la = p.amp; lo = p.lam; }
                excursion.push_back (std::abs (1200.0 * std::log2 (std::max (1e-9, lo))));

                move = std::max (move, sigDist (s0, sig (pk)));
                bestC = std::min (bestC, cents (pk));
            }
            leastMove = std::min (leastMove, move);
            bestCentsAnywhere = std::min (bestCentsAnywhere, bestC);
            if (bestC < c0 - 2.0) ++becameHarmonic;
        }

        std::printf ("  %d habits swept, strain 0 .. 0.06 in %d steps\n", habits, NS);
        std::printf ("  the chord always moves: least movement over any habit %.1f (travel gives 3-5)\n", leastMove);
        std::printf ("  %d of %d habits pass measurably nearer a harmonic series on the way\n",
                     becameHarmonic, habits);
        std::printf ("  nearest any habit comes to harmonic %.1f cents (worst at rest %.1f)\n",
                     bestCentsAnywhere, worstCentsAtRest);
        std::printf ("  smallest gap anywhere in the strained chains %.4f\n", worstGap);

        ok (lostSites == 0, "the strain never costs the filament a site");
        ok (worstGap > 0.2, "the strain never makes a bond the rest of the chain cannot live with");
        std::sort (excursion.begin(), excursion.end());
        const double medEx = excursion[excursion.size()/2];
        const double p90Ex = excursion[excursion.size()*9/10];
        int farOff = 0; for (double c : excursion) if (c > 2400.0) ++farOff;
        const double farRate = (double) farOff / (double) excursion.size();
        std::printf ("  which partial leads: median %.0f cents from the note, 90th percentile %.0f, beyond two octaves %.1f %%\n",
                     medEx, p90Ex, 100.0 * farRate);

        /*  Five cents, not zero: a refined ratio lands at 1.00000-something,
            not at the bit pattern for one. */
        ok (medEx < 5.0, "through most of the sweep the fundamental is still the loudest partial");
        ok (farRate < 0.08, "leaning the cut does not routinely hand the lead to a partial two octaves up");
        ok (leastMove > 12.0, "OBLIQUITY moves the sustained chord of EVERY habit, and by more than travel does");
        ok (becameHarmonic * 3 >= habits, "on the way it passes near a harmonic series -- the crystals of the staircase");

        /*  And the one that protects everything already shipped: at rest the
            strain must be arithmetically absent, not merely small. */
        const ab::Habit hh = ab::habitOf (11);
        const int wn = ab::PELL[std::max (0, std::min (5, hh.extent))];
        std::vector<ab::ChainSite> a1, a2;
        ab::buildChain (a1, w, hh, 0, 0, (double) hh.cutIndex/8.0*ab::PI, 0, wn, 0.62);
        ab::buildChain (a2, w, hh, 0, 0, (double) hh.cutIndex/8.0*ab::PI, 0, wn, 0.62, 0.0);
        bool same = a1.size() == a2.size();
        if (same) for (size_t i = 0; i < a1.size(); ++i)
            if (a1[i].s != a2[i].s || a1[i].occ != a2[i].occ || a1[i].field != a2[i].field) same = false;
        ok (same, "a strain of zero is the chain that was there before, to the last bit");
    }

    //--------------------------------------------------------------------
    head ("6 · the engine: in tune, bounded, silent when silent, deterministic");
    {
        ax::Engine e;
        e.prepare (48000.0, 512);
        e.service();
        std::vector<float> L (48000), R (48000);

        // silence in, silence out
        e.process (L.data(), R.data(), 4800);
        double mx = 0; for (int i = 0; i < 4800; ++i) mx = std::max (mx, (double) std::abs (L[(size_t)i]));
        ok (mx == 0.0, "an untouched artefact is exactly silent");

        // pitch: strike a note, measure by Goertzel at the asked frequency
        auto goertzel = [] (const float* x, int n, double f, double sr)
        {
            const double w = 2.0 * ab::PI * f / sr; const double cw = std::cos (w), coeff = 2.0 * cw;
            double s0 = 0, s1 = 0, s2 = 0;
            for (int i = 0; i < n; ++i) { s0 = x[i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) / n;
        };

        /*  THE STAR sounds at the note exactly — its p=1,q=0 order IS the
            fundamental. Measure that directly. */
        for (int note : { 40, 52, 60, 67 })
        {
            ax::Engine g; g.prepare (48000.0, 512); g.p.blend = 1.0f; g.p.conform = 1.0f;
            g.service();
            const double f0 = g.noteHz (note);
            g.noteOn (note, 1.0f);
            g.process (L.data(), R.data(), 24000);
            const double at  = goertzel (L.data() + 4000, 16384, f0, 48000.0);
            const double off = 0.5 * (goertzel (L.data() + 4000, 16384, f0 * 1.115, 48000.0)
                                    + goertzel (L.data() + 4000, 16384, f0 * 0.893, 48000.0));
            std::printf ("  star  note %3d  f0 %8.2f Hz   at f0 / beside it = %7.1f x\n", note, f0, at / std::max (1e-12, off));
            ok (at > off * 6.0, "the star sounds at the note it was given");
        }

        /*  THE FILAMENT has no partial at f0 and is not supposed to: it is a
            body of a given SIZE, and what you hear is its own spectrum. What
            playability requires is that the whole spectrum scale exactly with
            the note — an octave up must double every partial and change
            nothing else. That is the invariant worth testing. */
        {
            /*  Taking the argmax of the spectrum is not a way to measure this:
                which of a hundred near-equal partials happens to be tallest
                flips between notes, and the test reported a factor of 0.51 for
                a spectrum that had in fact scaled perfectly. Compare the whole
                log-frequency spectrum instead and find the shift that lines
                the two up. */
            const int NB = 1200;
            auto logSpectrum = [&] (int note, std::vector<double>& out)
            {
                ax::Engine g; g.prepare (48000.0, 512); g.p.blend = 0.0f; g.p.conform = 1.0f;
                g.p.damptilt = 0.55f; g.p.follow = 0.0f; g.p.cavity = 0.0f; g.service();
                g.noteOn (note, 1.0f);
                g.process (L.data(), R.data(), 32000);
                out.assign ((size_t) NB, 0.0);
                for (int i = 0; i < NB; ++i)
                {
                    const double f = 40.0 * std::pow (400.0, (double) i / (NB - 1));
                    out[(size_t) i] = std::log (1e-10 + goertzel (L.data() + 3000, 16384, f, 48000.0));
                }
            };
            std::vector<double> sa, sb;
            logSpectrum (48, sa); logSpectrum (60, sb);
            const double binsPerOct = (NB - 1) / (std::log (400.0) / std::log (2.0));
            auto meanOf = [] (const std::vector<double>& v) { double s = 0; for (double q : v) s += q; return s / v.size(); };
            const double ma = meanOf (sa), mb = meanOf (sb);
            int bestShift = 0; double bestCorr = -1e18;
            for (int sh = 0; sh < 500; ++sh)
            {
                double num = 0; int cnt = 0;
                for (int i = 0; i + sh < NB; ++i) { num += (sa[(size_t) i] - ma) * (sb[(size_t) (i + sh)] - mb); ++cnt; }
                const double c = num / std::max (1, cnt);
                if (c > bestCorr) { bestCorr = c; bestShift = sh; }
            }
            const double octaves = (double) bestShift / binsPerOct;
            std::printf ("  filament  the whole spectrum of note 60 is that of note 48 shifted by %.4f octaves (want 1.0000)\n", octaves);
            ok (std::abs (octaves - 1.0) < 0.04,
                "the filament's whole spectrum scales exactly with the note, shape unchanged");
        }

        // the grade: our octave is the silver ratio, not two
        ax::Engine g; g.prepare (48000.0, 64);
        const double a = g.noteHz (60), b = g.noteHz (60 + ax::GRADE);
        std::printf ("  seventeen keys up multiplies the frequency by %.6f  (1+sqrt2 = %.6f)\n", b / a, ab::SILVER);
        ok (std::abs (b / a - ab::SILVER) < 1e-4, "the artefact's octave is the silver ratio");
        g.p.conform = 1.0f;
        const double c2 = g.noteHz (72) / g.noteHz (60);
        std::printf ("  at full conformance twelve keys up gives %.6f  (2 = ours)\n", c2);
        ok (std::abs (c2 - 2.0) < 1e-4, "conformance bends the grade onto twelve equal steps");
    }

    //--------------------------------------------------------------------
    head ("7 · travelling through w is continuous, not a sequence of splices");
    {
        ax::Engine e; e.prepare (48000.0, 256); e.p.blend = 0.25f; e.service();
        std::vector<float> L (256), R (256);
        e.noteOn (48, 0.9f);
        // settle
        for (int b = 0; b < 40; ++b) e.process (L.data(), R.data(), 256);
        double settled = 0;
        for (int b = 0; b < 40; ++b)
        {
            e.process (L.data(), R.data(), 256);
            for (int i = 1; i < 256; ++i) settled = std::max (settled, (double) std::abs (L[(size_t)i] - L[(size_t)(i-1)]));
        }
        /*  A CONTROL RUN. The first version of this test blamed the wheel for
            a 1.53 step that was really the cavity ringing up over a second of
            sustain — it would have done exactly the same standing still.
            Measure the still case over the same span before blaming travel. */
        double still = 0;
        {
            ax::Engine c; c.prepare (48000.0, 256); c.p.blend = 0.25f; c.service();
            c.noteOn (48, 0.9f);
            for (int b = 0; b < 240; ++b)
            {
                c.service();
                c.process (L.data(), R.data(), 256);
                if (b >= 40) for (int i = 1; i < 256; ++i) still = std::max (still, (double) std::abs (L[(size_t)i] - L[(size_t)(i-1)]));
            }
            ok (c.runaways.load() == 0, "standing still, the safety net never has to fire");
        }

        // now drag the body through the cut at speed while it sounds
        double moving = 0;
        for (int b = 0; b < 200; ++b)
        {
            e.setTravelDelta (0.02);
            e.service();
            e.process (L.data(), R.data(), 256);
            for (int i = 1; i < 256; ++i) moving = std::max (moving, (double) std::abs (L[(size_t)i] - L[(size_t)(i-1)]));
        }
        std::printf ("  largest sample step   settled %.5f   standing for the same span %.5f   while travelling %.5f\n",
                     settled, still, moving);
        std::printf ("  the body travelled %.2f in w, %d sites now in the cut\n", e.travelDepth(), e.chainN.load());
        ok (moving < std::max (still, settled) * 3.5 + 0.05, "dragging the body through the cut is no rougher than standing still");
        ok (e.runaways.load() == 0, "and the safety net never has to fire while travelling either");
        ok (std::isfinite (moving), "and stays finite");
    }

    //--------------------------------------------------------------------
    head ("8 · the whole catalogue: every habit bounded, audible and distinct");
    {
        std::vector<float> L (12000), R (12000);
        std::vector<std::vector<double>> prints;
        int quiet = 0; double worstPeak = 0;
        for (int hI = 0; hI < ab::habitCount(); ++hI)
        {
            ax::Engine e; e.prepare (48000.0, 512);
            ax::applyHabit (hI, e.p);
            e.service();
            e.noteOn (55, 0.95f); e.process (L.data(), R.data(), 12000);
            double pk = 0, rms = 0;
            for (int i = 0; i < 12000; ++i) { pk = std::max (pk, (double) std::abs (L[(size_t)i])); rms += (double) L[(size_t)i] * L[(size_t)i]; }
            rms = std::sqrt (rms / 12000);
            worstPeak = std::max (worstPeak, pk);
            if (rms < 2e-4) ++quiet;
            /*  Twelve bands in each of two time windows: two specimens that
                happen to share a spectrum still differ in how they arrive and
                how they let go, and eight bands over one window could not see
                that. */
            std::vector<double> fp (24, 0.0);
            for (int wI = 0; wI < 2; ++wI)
            {
                const int a0 = wI == 0 ? 300 : 6000, a1 = wI == 0 ? 4000 : 11800;
                for (int b = 0; b < 12; ++b)
                {
                    const double f = 70.0 * std::pow (2.0, b * 0.62);
                    const double w = 2.0 * ab::PI * f / 48000.0, coeff = 2.0 * std::cos (w);
                    double s0=0,s1=0,s2=0;
                    for (int i = a0; i < a1; ++i) { s0 = L[(size_t)i] + coeff*s1 - s2; s2=s1; s1=s0; }
                    fp[(size_t)(wI * 12 + b)] = std::log (1e-9 + std::sqrt (std::max (0.0, s1*s1+s2*s2-coeff*s1*s2)) / (a1 - a0));
                }
            }
            prints.push_back (fp);
            if (! std::isfinite (pk)) { ok (false, "a habit produced a non-finite sample"); break; }
        }
        std::printf ("  worst peak across 256 habits %.4f   silent habits %d\n", worstPeak, quiet);
        ok (worstPeak < 1.05, "no habit exceeds the ceiling");
        ok (quiet == 0, "every habit sounds");
        double closest = 1e9; int ca = 0, cb = 0;
        for (size_t i = 0; i < prints.size(); ++i)
            for (size_t j = i + 1; j < prints.size(); ++j)
            {
                double d = 0; for (int b = 0; b < 24; ++b) { const double q = prints[i][(size_t)b] - prints[j][(size_t)b]; d += q*q; }
                if (d < closest) { closest = d; ca = (int) i; cb = (int) j; }
            }
        std::printf ("  closest pair of habits: %d and %d, spectral distance %.4f\n", ca, cb, std::sqrt (closest));
        ok (std::sqrt (closest) > 0.25, "no two habits are the same specimen");
    }

    //--------------------------------------------------------------------
    head ("8b · every habit stops when it is let go");
    {
        /*  Nothing else in the bench could see this. Peaks were bounded, the
            catalogue was distinct, travel was smooth — and two habits in
            twenty were RINGING UP after the note was released, to ten
            kilohertz, because the cavity spring read a displacement one sample
            old and a delayed spring is a negative stiffness at the top of the
            band. Bounded is not the same as finished. */
        std::vector<float> L (256), R (256);
        int worst = -1; double worstRatio = 0.0, worstTail = 0.0;
        for (int hI = 0; hI < ab::habitCount(); ++hI)
        {
            ax::Engine e; e.prepare (48000.0, 256);
            ax::applyHabit (hI, e.p); e.service();
            e.noteOn (55, 0.95f);
            double held = 0.0, mid = 0.0, after = 0.0;
            for (int b = 0; b < 700; ++b)                 // 3.7 s
            {
                e.process (L.data(), R.data(), 256);
                if (b == 180) e.noteOff (55);             // let go at ~1 s
                double r = 0; for (int i = 0; i < 256; ++i) r += (double) L[(size_t) i] * L[(size_t) i];
                r = std::sqrt (r / 256);
                if (b >= 120 && b < 180) held = std::max (held, r);
                if (b >= 460 && b < 520) mid   = std::max (mid, r);
                if (b >= 640) after = std::max (after, r);
            }
            /*  A habit is allowed to ring for a long time — LOSS reaches
                fourteen seconds and some specimens are meant to. What is not
                allowed is to get LOUDER. Measure the direction, not the
                length: the tail must be below the held level and still
                falling. Threshold 2 % failed a habit that was simply thirty
                decibels down and dropping, which is a decay, not a fault. */
            const double ratio = after / std::max (1e-9, held);
            const double rising = after / std::max (1e-9, mid);
            if (rising > worstRatio) { worstRatio = rising; worst = hI; worstTail = ratio; }
        }
        std::printf ("  worst habit for hanging on: %d — its tail at 3.4 s is %.1f %% of its tail at 2.5 s, and %.2f %% of the held level\n",
                     worst, 100.0 * worstRatio, 100.0 * worstTail);
        ok (worstRatio < 0.98, "every one of the 256 habits is still FALLING two and a half seconds after release");
        ok (worstTail < 0.30, "...and well below the level it was held at");
    }

    //--------------------------------------------------------------------
    head ("8c · the SUSTAINED tail differs from specimen to specimen");
    {
        /*  Peter's report, made into a test: hold a note on any habit and the
            transient is its own, but what remains once the strike has died was
            the same background every time.

            It had to be. A held note is carried by the STAR, and the star's
            peaks were placed by the window's transform — and the reciprocal
            module of an octagonal quasicrystal belongs to the LATTICE, not to
            what decorates it. Every specimen therefore diffracted into the
            same comb. What distinguishes them is the decoration, so the
            intensities are now the structure factor of the actual chain.

            This measures the steady part only: everything before 1.2 s is
            thrown away, and what is compared is what is LEFT. */
        const int SR = 48000;
        std::vector<float> L (SR * 3), R (SR * 3);
        std::vector<std::vector<double>> tails;
        const int STEP = 4;                       // 64 habits across the catalogue
        for (int hI = 0; hI < ab::habitCount(); hI += STEP)
        {
            ax::Engine e; e.prepare ((double) SR, 512);
            ax::applyHabit (hI, e.p);
            e.p.sustain = 0.75f;                  // hold it up, so there IS a tail
            e.service();
            e.noteOn (52, 0.95f);
            for (int off = 0; off < SR * 3; off += 512)
            {
                e.service();
                e.process (L.data() + off, R.data() + off, std::min (512, SR * 3 - off));
            }
            const int a0 = (int) (SR * 1.7), a1 = (int) (SR * 2.9);
            std::vector<double> fp (16, 0.0);
            for (int b = 0; b < 16; ++b)
            {
                const double f = 70.0 * std::pow (2.0, b * 0.47);
                const double w = 2.0 * ab::PI * f / SR, coeff = 2.0 * std::cos (w);
                double s0 = 0, s1 = 0, s2 = 0;
                for (int i = a0; i < a1; ++i) { s0 = L[(size_t) i] + coeff * s1 - s2; s2 = s1; s1 = s0; }
                fp[(size_t) b] = std::log (1e-9 + std::sqrt (std::max (0.0, s1*s1 + s2*s2 - coeff*s1*s2)) / (a1 - a0));
            }
            tails.push_back (fp);
        }
        double closest = 1e18; int ca = 0, cb = 0; double sum = 0; int pairs = 0;
        for (size_t i = 0; i < tails.size(); ++i)
            for (size_t j = i + 1; j < tails.size(); ++j)
            {
                double d = 0;
                for (int b = 0; b < 16; ++b) { const double q = tails[i][(size_t) b] - tails[j][(size_t) b]; d += q * q; }
                d = std::sqrt (d);
                sum += d; ++pairs;
                if (d < closest) { closest = d; ca = (int) i * STEP; cb = (int) j * STEP; }
            }
        std::printf ("  %d habits, tail measured 1.7-2.9 s after the strike\n", (int) tails.size());
        std::printf ("  closest pair of TAILS: habits %d and %d at %.3f ; mean separation %.3f\n",
                     ca, cb, closest, sum / std::max (1, pairs));
        ok (closest > 0.9, "no two specimens fade into the same background");
        ok (sum / std::max (1, pairs) > 3.0, "and the catalogue's tails are well spread, not clustered");
    }

    //--------------------------------------------------------------------
    head ("8d · the output stage: a compressor, then a brickwall");
    {
        /*  What was here before was a tanh waveshaper, and the giveaway was in
            this very bench: all 256 habits peaked at 0.9349, which is that
            waveshaper's own asymptote. Everything was sitting in the clipper.
            A limiter reduces GAIN; a clipper bends the WAVEFORM. Only one of
            those is inaudible when it is not needed, so test for both: that
            nothing gets past the ceiling, AND that nothing happens at all when
            the signal is quiet. */
        std::vector<float> L (48000), R (48000);

        // (a) nothing gets past the ceiling, over the whole catalogue, hot
        double worst = 0.0; int worstH = -1; double ceilUsed = 0.0;
        for (int hI = 0; hI < ab::habitCount(); hI += 2)
        {
            ax::Engine e; e.prepare (48000.0, 512);
            ax::applyHabit (hI, e.p);
            e.p.level = 1.0f;                       // driven hard on purpose
            e.service();
            e.noteOn (52, 1.0f); e.noteOn (59, 1.0f); e.noteOn (64, 1.0f);
            for (int off = 0; off < 48000; off += 512)
                e.process (L.data() + off, R.data() + off, std::min (512, 48000 - off));
            const float ceilg = juceLikeCeil (e.p.sat);
            ceilUsed = ceilg;
            for (int i = 0; i < 48000; ++i)
            {
                const double a = std::max (std::abs ((double) L[(size_t)i]), std::abs ((double) R[(size_t)i]));
                if (a > worst) { worst = a; worstH = hI; }
            }
        }
        std::printf ("  driven flat out, worst peak across the catalogue: %.4f (habit %d), ceiling %.3f\n",
                     worst, worstH, ceilUsed);
        ok (worst <= ceilUsed * 1.02, "nothing gets past the ceiling, even with three notes at full level");
        ok (worst < 1.0, "and nothing ever reaches digital full scale");

        // (b) when it is not needed it does NOTHING
        {
            ax::Engine e; e.prepare (48000.0, 512);
            e.p.level = 0.10f;
            e.service();
            e.noteOn (52, 0.5f);
            double minGR = 1.0;
            for (int off = 0; off < 48000; off += 512)
            { e.process (L.data() + off, R.data() + off, std::min (512, 48000 - off));
              minGR = std::min (minGR, (double) e.limitGR.load()); }
            std::printf ("  played quietly, the worst gain reduction was %.6f\n", minGR);
            ok (minGR >= 1.0, "played below the threshold, the output stage does not touch the signal at all");
        }

        // (c) it must not pump: on a held chord the gain should settle
        {
            ax::Engine e; e.prepare (48000.0, 512);
            e.p.level = 0.95f;
            e.service();
            e.noteOn (48, 1.0f); e.noteOn (55, 1.0f);
            double lo = 1.0, hi = 0.0;
            for (int off = 0; off < 48000 * 2; off += 512)
            {
                std::vector<float> a (512), b (512);
                e.process (a.data(), b.data(), 512);
                if (off > 24000)     // past the strike, into the sustain
                { const double g = e.limitGR.load(); lo = std::min (lo, g); hi = std::max (hi, g); }
            }
            std::printf ("  on a held chord the gain rides between %.4f and %.4f (%.2f dB of movement)\n",
                         lo, hi, 20.0 * std::log10 (std::max (1e-9, hi / std::max (1e-9, lo))));
            ok (20.0 * std::log10 (std::max (1e-9, hi / std::max (1e-9, lo))) < 6.0,
                "and it does not pump: the gain movement on a held chord stays under six decibels");
        }

        // (d) the look-ahead is real delay and must be declared
        {
            ax::Engine e; e.prepare (48000.0, 512);
            std::printf ("  look-ahead reported to the host: %d samples (%.2f ms)\n",
                         e.latencySamples(), 1000.0 * e.latencySamples() / 48000.0);
            ok (e.latencySamples() > 0 && e.latencySamples() < 1024, "the look-ahead is a sane, declarable latency");
        }
    }

    //--------------------------------------------------------------------
    head ("8e · no specimen lives inside the limiter");
    {
        /*  Peter heard fast clicks on the lower-left swatch of the catalogue,
            which is habit 240, and asked whether it was clipping or the gain
            structure. It was the gain structure. Habit 240 was under gain
            reduction in all 300 blocks of a three-second note, averaging 2.5 dB
            and reaching 7.3, while habit 96 never touched the limiter at all --
            twenty decibels apart, and a limiter riding that hard on a dense
            inharmonic signal is what a fast click is.

            The trims in HabitGain.h fix it, and they are MEASURED, so they go
            stale the moment anything moves the level. This is what notices.
            If it fails, re-run: ab67gaincal > the table (it is idempotent). */
        const int SR = 48000, BLK = 512, NB = 150;
        std::vector<float> L ((size_t) BLK), R ((size_t) BLK);
        int inLimiter = 0, tested = 0;
        double worstBlocks = 0, worstPre = 0, quietest = 1e9;

        for (int h = 0; h < 256; h += 8)
            for (int note : { 40, 58 })
            {
                ax::Engine e; e.prepare ((double) SR, BLK);
                ax::applyHabit (h, e.p);
                e.p.sustain = 0.8f;
                e.service();
                e.preLimitPeak.store (0.0f);
                e.noteOn (note, 0.95f);
                int limited = 0;
                for (int b = 0; b < NB; ++b)
                { e.service(); e.process (L.data(), R.data(), BLK);
                  if (e.limitGR.load() < 0.98f) ++limited; }
                const double pre = e.preLimitPeak.load();
                ++tested;
                if (limited > NB / 2) ++inLimiter;
                worstBlocks = std::max (worstBlocks, (double) limited);
                worstPre = std::max (worstPre, pre);
                quietest = std::min (quietest, pre);
            }

        std::printf ("  %d specimen/note pairs played\n", tested);
        std::printf ("  worst pre-limiter peak %.4f   quietest %.4f   spread %.1f dB\n",
                     worstPre, quietest, 20.0 * std::log10 (worstPre / std::max (1e-9, quietest)));
        std::printf ("  most blocks a single note spent under gain reduction: %.0f of %d\n",
                     worstBlocks, NB);

        ok (inLimiter == 0, "no specimen spends half a note inside the limiter");
        ok (worstPre < 0.62, "no specimen is handed to the output stage far above the target");
        ok (quietest > 0.004, "the trims never reduce a specimen to nothing");
    }

    //--------------------------------------------------------------------
    head ("8f · temperature, and the specimen's own modulation matrix");
    {
        const int NP = ax::numParams();
        std::vector<float> stored ((size_t) NP), eff ((size_t) NP);
        for (int i = 0; i < NP; ++i) stored[(size_t) i] = ax::paramSpec (i).def;

        auto kelvinOf = [] (float t)
        { return ax::TEMP_MIN + (ax::TEMP_MAX - ax::TEMP_MIN) * t; };
        auto indexOf = [&] (const char* id)
        { for (int i = 0; i < NP; ++i) if (std::strcmp (ax::paramSpec (i).id, id) == 0) return i; return -1; };

        const int iTravel = indexOf ("travel"), iCutang = indexOf ("cutang"), iCutpos = indexOf ("cutpos");

        /*  1. FROZEN AT 77 K. Not nearly frozen -- bit-identical, so a cold
            instrument is the instrument as it was before any of this was
            written. */
        {
            ax::Modulator mod; mod.setHabit (23);
            bool same = true;
            for (int step = 0; step < 400; ++step)
            {
                mod.advance (0.01, stored.data());
                mod.apply (stored.data(), eff.data(), ax::TEMP_MIN);
                if (std::memcmp (stored.data(), eff.data(), sizeof (float) * (size_t) NP) != 0) same = false;
            }
            ok (same, "at 77 K nothing moves at all, to the bit");
        }

        /*  2. THE TWO TIERS. At room temperature the material breathes and the
            scene is left exactly where the player framed it. */
        {
            ax::Modulator mod; mod.setHabit (23);
            double worstOrdinary = 0, worstScene = 0;
            for (int step = 0; step < 3000; ++step)
            {
                mod.advance (0.02, stored.data());
                mod.apply (stored.data(), eff.data(), ax::TEMP_ROOM);
                for (int i = 0; i < NP; ++i)
                {
                    const double d = std::abs ((double) eff[(size_t) i] - stored[(size_t) i]);
                    if (i == iTravel || i == iCutang || i == iCutpos) worstScene = std::max (worstScene, d);
                    else worstOrdinary = std::max (worstOrdinary, d);
                }
            }
            std::printf ("  at room temperature: material moves up to %.4f, scene moves %.6f\n",
                         worstOrdinary, worstScene);
            ok (worstScene == 0.0, "at room temperature TRAVERSE and the sounding line do not move at all");
        }

        /*  3. AND AT 800 K THE SCENE JOINS IN -- "maybe at very high
            temperatures", which is what that means. */
        {
            /*  Not every specimen has a scene wire, and that is right: some
                bodies hold their frame even when they are far too hot and
                others do not, which is character rather than a fault. What
                must be true is that a good many of them do.  */
            int withScene = 0; double sceneMoved = 0;
            for (int h = 0; h < 256; ++h)
            {
                ax::Modulator mod; mod.setHabit (h);
                double mv = 0;
                for (int step = 0; step < 240; ++step)
                {
                    mod.advance (0.037, stored.data());
                    mod.apply (stored.data(), eff.data(), ax::TEMP_MAX);
                    for (int i : { iTravel, iCutang, iCutpos })
                        if (i >= 0) mv = std::max (mv, std::abs ((double) eff[(size_t) i] - stored[(size_t) i]));
                }
                if (mv > 0.0) ++withScene;
                sceneMoved = std::max (sceneMoved, mv);
            }
            std::printf ("  at 800 K, %d of 256 specimens stir the scene, by up to %.4f of range\n",
                         withScene, sceneMoved);
            ok (withScene > 100, "at 800 K a good half of the catalogue stirs the scene as well");
            ok (sceneMoved <= (double) ax::EXC_SCENE + 1e-4, "and the scene stays inside its smaller allowance");
        }

        /*  4. THE EXCURSION IS BOUNDED, whatever the wiring does. This is the
            guarantee Peter asked for in place of a cycle check, and it holds
            for every specimen, at the hottest setting, over a long run. */
        {
            double worst = 0; int offender = -1;
            for (int h = 0; h < 256; h += 4)
            {
                ax::Modulator mod; mod.setHabit (h);
                for (int step = 0; step < 400; ++step)
                {
                    mod.advance (0.031, stored.data());
                    mod.apply (stored.data(), eff.data(), ax::TEMP_MAX);
                    for (int i = 0; i < NP; ++i)
                    {
                        const float hi = ax::paramMax (ax::paramSpec (i));
                        const double frac = std::abs ((double) eff[(size_t) i] - stored[(size_t) i]) / hi;
                        if (frac > worst) { worst = frac; offender = i; }
                    }
                }
            }
            std::printf ("  worst excursion anywhere, any specimen, at 800 K: %.4f of range (%s)\n",
                         worst, offender >= 0 ? ax::paramSpec (offender).name : "-");
            ok (worst <= (double) ax::EXC_ORDINARY + 1e-4,
                "no parameter ever leaves its allowance, whatever the matrix does");
        }

        /*  5. A CYCLE CANNOT BE EXPRESSED, so this checks the shape instead:
            every wire reads a source and writes a different destination, and
            the sources fan out, which is what makes one knob move several. */
        {
            int selfWired = 0, fanned = 0, distinct = 0;
            std::set<std::string> shapes;
            for (int h = 0; h < 256; ++h)
            {
                const ax::ModMatrix m = ax::modMatrixFor (h);
                std::string sig; int count[128] = {};
                for (int i = 0; i < m.n; ++i)
                {
                    if (m.w[i].src == m.w[i].dst) ++selfWired;
                    ++count[m.w[i].src & 127];
                    sig += std::to_string (m.w[i].src) + ">" + std::to_string (m.w[i].dst) + ",";
                }
                for (int i = 0; i < 128; ++i) if (count[i] >= 2) { ++fanned; break; }
                shapes.insert (sig);
            }
            distinct = (int) shapes.size();
            std::printf ("  256 matrices: %d distinct, %d have a source driving two or more\n",
                         distinct, fanned);
            ok (selfWired == 0, "no wire drives itself");
            ok (distinct > 240, "the wiring really is unique to each entry in the catalogue");
            ok (fanned == 256, "every specimen has a control that moves several others");
        }

        /*  6. Deterministic: the same specimen, run again, does the same thing.
            A bounce has to match what was heard. */
        {
            ax::Modulator a, b; a.setHabit (77); b.setHabit (77);
            std::vector<float> ea ((size_t) NP), eb ((size_t) NP);
            bool same = true;
            for (int step = 0; step < 500; ++step)
            {
                a.advance (0.017, stored.data()); b.advance (0.017, stored.data());
                a.apply (stored.data(), ea.data(), 640.0f);
                b.apply (stored.data(), eb.data(), 640.0f);
                if (std::memcmp (ea.data(), eb.data(), sizeof (float) * (size_t) NP) != 0) same = false;
            }
            ok (same, "the same specimen modulates identically every time it is played");
        }
    }

    //--------------------------------------------------------------------
    head ("9 · arrests really deform the body, and cost");
    {
        ax::Engine e; e.prepare (48000.0, 512); e.service();
        const int before = (int) e.patch().sites.size();
        e.arrest (1.6, 0.4, true); e.arrest (-1.5, 1.1, true);
        e.service();
        const int after = (int) e.patch().sites.size();
        std::printf ("  sites in the aperture: %d -> %d after two arrests\n", before, after);
        ok (after != before, "pinning a site deforms the window and changes the matter everywhere");
        e.clearArrests(); e.service();
        ok ((int) e.patch().sites.size() == before, "releasing them restores the body exactly");

        std::vector<float> L (512), R (512);
        ax::Engine c; c.prepare (48000.0, 512); c.p.extent = 3.0f; c.p.voicesN = 4.0f; c.service();
        for (int i = 0; i < 4; ++i) c.noteOn (48 + i * 5, 0.9f);
        auto t0 = std::chrono::high_resolution_clock::now();
        int svc = 0;
        for (int b = 0; b < 940; ++b) { c.process (L.data(), R.data(), 512); if ((b % 8) == 0) { c.service(); ++svc; } }
        auto t1 = std::chrono::high_resolution_clock::now();
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        const double audio = 940.0 * 512.0 / 48000.0;
        std::printf ("  four voices, extent 70, %d rebuilds: %.1f x realtime  (%.1f %% of one core)\n",
                     svc, audio / secs, 100.0 * secs / audio);
        ok (audio / secs > 4.0, "four voices run comfortably faster than realtime");
    }

    //--------------------------------------------------------------------
    head ("10 · determinism");
    {
        auto run = [] (std::vector<float>& out)
        {
            ax::Engine e; e.prepare (48000.0, 256);
            ax::applyHabit (77, e.p);
            e.service();
            e.noteOn (50, 0.8f); e.noteOn (57, 0.6f);
            std::vector<float> L (256), R (256);
            out.clear();
            for (int b = 0; b < 60; ++b)
            {
                if (b == 20) e.setTravelDelta (0.35);
                if (b == 30) e.noteOff (50);
                e.service();
                e.process (L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) out.push_back (L[(size_t)i]);
            }
        };
        std::vector<float> a, b;
        run (a); run (b);
        ok (a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0,
            "the same gestures on the same specimen give bit-identical sound");
    }

    //--------------------------------------------------------------------
    head ("11 · the site");
    {
        /*  THE SITE (proxima_site.h). Uncoupled, the engine must be BYTE-
            identical to one that never heard of it; coupled, cold and close,
            the section must rock through w in the bench's time, and the rock
            must be audible — measured, not assumed. The bench plays the site
            itself: a 0.5 Hz phase handed in every service tick, as the
            processor's timer would. */
        struct SiteTake { std::vector<float> L; double tauMin = 1e9, tauMax = -1e9; };
        auto take = [] (float pull, bool everCall) -> SiteTake
        {
            ax::Engine e; e.prepare (48000.0, 256);
            ax::applyHabit (77, e.p);
            e.p.temp = 0.10f;                       // cold: nothing else stirs
            e.service();
            e.noteOn (50, 0.8f); e.noteOn (57, 0.6f);
            std::vector<float> L (256), R (256);
            SiteTake t;
            const int blocks = (int) (12.0 * 48000.0 / 256.0);
            for (int b = 0; b < blocks; ++b)
            {
                const double tm = (double) b * 256.0 / 48000.0;
                if (everCall) e.setSite ((float) std::fmod (tm * 0.5, 1.0), 0.5f, pull);
                if ((b % 6) == 0) e.service();              // ~30 Hz, as the timer does
                e.process (L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) t.L.push_back (L[(size_t) i]);
                t.tauMin = std::min (t.tauMin, e.travelDepth());
                t.tauMax = std::max (t.tauMax, e.travelDepth());
            }
            return t;
        };
        const SiteTake never = take (0.0f, false), off = take (0.0f, true), on = take (1.0f, true);
        const bool same = never.L.size() == off.L.size()
                       && std::memcmp (never.L.data(), off.L.data(), never.L.size() * sizeof (float)) == 0;
        std::printf ("  uncoupled: %s; depth swing %.4f\n", same ? "byte-identical" : "DIFFERS", off.tauMax - off.tauMin);
        ok (same && off.tauMax - off.tauMin < 1e-12, "uncoupled, the site is an exact no-op");

        const double swing = on.tauMax - on.tauMin;
        std::printf ("  coupled at full pull: the section rocks %.3f deep in w (0.500 asked)\n", swing);
        ok (swing > 0.45 && swing < 0.55, "cold and close, the section rocks through w with the bench");

        //  and it is HEARD: the output energy folded on the bench's period
        auto fold = [] (const std::vector<float>& L, double hz) -> double
        {
            double cx = 0, cy = 0, w = 0;
            const int hop = 256;
            for (size_t i = (size_t) (2.0 * 48000.0); i + hop < L.size(); i += hop)
            {
                double en = 0; for (int k = 0; k < hop; ++k) en += (double) L[i + k] * L[i + k];
                const double ph = std::fmod ((double) i / 48000.0 * hz, 1.0) * 2.0 * 3.14159265358979;
                cx += en * std::cos (ph); cy += en * std::sin (ph); w += en;
            }
            return w > 0 ? std::sqrt (cx * cx + cy * cy) / w : 0.0;
        };
        const double rFree = fold (off.L, 0.5), rLock = fold (on.L, 0.5);
        std::printf ("  the sound folded on the bench's 0.5 Hz: %.3f coupled vs %.3f free\n", rLock, rFree);
        ok (rLock > 0.05 && rLock > rFree * 2.0, "the rock is heard: the output breathes with the bench");
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
            ax::Engine e; e.prepare (48000.0, 256); e.service();
            if (pass == 0) { e.setSustainPedal (true); e.allNotesOff(); }
            std::vector<float> L (256), R (256);
            e.noteOn (48, 0.9f);
            for (int k = 0; k < 40; ++k) e.process (L.data(), R.data(), 256);
            e.noteOff (48);
            for (int k = 0; k < 400; ++k) e.process (L.data(), R.data(), 256);
            for (int i = 0; i < 256; ++i) t[pass] = std::max (t[pass], (double) std::abs (L[(size_t) i]));
        }
        ok (t[0] <= t[1] + 1.0e-4, "a panic lets go of the pedal: a note played afterwards still stops");
    }

    std::printf ("\n%d checks, %d failed  —  %s\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");
    return fails == 0 ? 0 : 1;
}
