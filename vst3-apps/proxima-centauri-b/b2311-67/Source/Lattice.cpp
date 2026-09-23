#include "Lattice.h"
#include "HabitGain.h"

#include <algorithm>
#include <unordered_map>

namespace ab
{

//==============================================================================
namespace
{
    inline double sinc (double x) { return std::abs (x) < 1.0e-9 ? 1.0 : std::sin (x) / x; }

    struct Rng
    {
        uint64_t s;
        explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0xDA3E39CB94B95BDBull) {}
        inline uint64_t next()
        {
            uint64_t z = (s += 0x9E3779B97F4A7C15ull);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
        inline double uni() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
        inline double range (double a, double b) { return a + (b - a) * uni(); }
        inline int    pick (int n) { return (int) (next() % (uint64_t) n); }
    };

    // the four E_par basis vectors, and the four E_perp ones
    inline void ePar  (int k, double& x, double& y) { const double a = k * (PI * 0.25);       x = std::cos (a); y = std::sin (a); }
    inline void ePerp (int k, double& x, double& y) { const double a = 3.0 * k * (PI * 0.25); x = std::cos (a); y = std::sin (a); }

    inline uint64_t key4 (int a, int b, int c, int d)
    {
        return   ((uint64_t) (uint16_t) (int16_t) a)
             | (((uint64_t) (uint16_t) (int16_t) b) << 16)
             | (((uint64_t) (uint16_t) (int16_t) c) << 32)
             | (((uint64_t) (uint16_t) (int16_t) d) << 48);
    }
}

//==============================================================================
double Habit::eval (double px, double py) const
{
    double v = 0.0, norm = 0.0;
    for (int i = 0; i < waves; ++i)
    {
        double ux, uy; Window::dir (kdir[i] & 7, ux, uy);
        v    += kamp[i] * std::cos (kmag[i] * (px * ux + py * uy) + kphi[i]);
        norm += std::abs (kamp[i]);
    }
    if (norm > 0.0) v /= norm;
    v += radial * (std::sqrt (px * px + py * py) / APO - 0.6);

    // crisp pushes the field through a threshold: the two- and three-letter
    // alphabets of the classical quasiperiodic Hamiltonians
    if (crisp > 0.0)
    {
        const double g = 1.0 / std::max (1.0e-3, (1.0 - crisp) * 0.9 + 0.02);
        v = std::tanh (v * g);
    }
    double f = 0.5 + 0.5 * std::max (-1.0, std::min (1.0, v));
    if (levels >= 2)
    {
        const double L = (double) levels;
        f = std::floor (f * L * 0.9999) / (L - 1.0);
        f = std::min (1.0, f);
    }
    return f;
}

//==============================================================================
int habitCount() { return 256; }

float habitTrim (int index)
{
    index = ((index % 256) + 256) % 256;
    return HABIT_TRIM[index];
}

Habit habitOf (int index)
{
    Habit h;
    h.index = index = ((index % 256) + 256) % 256;

    /*  The catalogue is laid out by family first, so that neighbouring indices
        are not neighbours in character — an alien collection is arranged by
        what a thing IS, and we interleave so that browsing crosses families. */
    const int fam = index % 8;
    Rng r ((uint64_t) (index * 2654435761u) ^ 0x67B2311ull);

    h.waves = 2 + r.pick (3);
    for (int i = 0; i < 4; ++i)
    {
        { const double u = r.uni(); h.kmag[i] = 0.30 + 3.1 * u * u; }
        h.kdir[i] = r.pick (8);
        h.kamp[i] = r.range (0.25, 1.0) / (1.0 + 0.55 * i);
        h.kphi[i] = r.range (0.0, 2.0 * PI);
    }
    h.radial   = r.uni() < 0.35 ? r.range (-1.2, 1.2) : 0.0;
    h.contrast = r.range (1.6, 9.0);
    h.bondExp  = r.range (0.4, 2.6);
    h.damp     = std::pow (10.0, r.range (-3.6, -2.2));
    h.dampTilt = r.range (0.0, 0.9);
    h.nonlin   = r.uni() < 0.5 ? 0.0 : r.range (0.02, 0.5);
    h.extent   = 1 + r.pick (4);              // PELL 12, 29, 70, 169
    h.cutIndex = r.pick (8);
    h.blend    = r.range (0.05, 0.95);
    h.starWid  = r.range (0.6, 3.4);
    h.starTilt = r.range (0.05, 1.1);
    h.strike   = r.range (0.05, 0.95);
    h.drive    = r.uni() < 0.4 ? r.range (0.05, 0.6) : 0.0;
    h.filmBase = r.range (300.0, 620.0);
    h.filmSpan = r.range (180.0, 900.0) * (r.uni() < 0.5 ? -1.0 : 1.0);
    h.glint    = r.range (0.15, 1.0);

    // families: eight kinds of material, so the catalogue has real range
    switch (fam)
    {
        case 0:  h.crisp = 0.0;  h.levels = 0; break;                                  // continuous
        case 1:  h.crisp = 0.92; h.levels = 2; h.contrast = r.range (3.0, 9.0); break; // two letters
        case 2:  h.crisp = 0.55; h.levels = 3; break;                                  // three letters
        case 3:  h.crisp = 0.0;  h.levels = 0; h.radial = r.range (0.8, 1.8); break;   // bullseye
        case 4:  h.crisp = 0.0;  h.levels = 0; h.waves = 4; h.nonlin = r.range (0.15, 0.6); break;
        case 5:  h.crisp = 0.75; h.levels = 0; h.damp = std::pow (10.0, r.range (-4.2, -3.2)); break; // long
        case 6:  h.crisp = 0.2;  h.levels = 0; h.extent = 0 + r.pick (2); h.blend = r.range (0.0, 0.3); break; // short bar
        default: h.crisp = 0.35; h.levels = 0; h.extent = 3 + r.pick (2); h.blend = r.range (0.6, 1.0); break; // star-led
    }
    return h;
}

//==============================================================================
/*  Enumerate the accepted lattice points whose E_par image lies within
    `radius`. The trick that makes this cheap: the coordinates factor as

        (A,B) -> (x_par, x_perp)      (M,C) -> (y_par, y_perp)

    and x_perp is pinned inside the window, so for each B only three or four A
    survive. The eightfold half-planes couple x and y, so those are tested on
    the product — but the product is already small. */
static void enumerateLevel (Patch& out, const Window& w, const Habit& h,
                            double gx, double gy, double radius, int level, double scale)
{
    Window wl = w;
    wl.scale = w.scale / std::pow (SILVER, (double) level);

    /*  The box the enumeration searches has to include the BULGE. Sized from
        the undeformed window — which is what it was — an arrest pushes the
        acceptance rim out to hold a site and the search never looks that far,
        so the one site the pin exists to keep is the one site that is lost.
        The window grew, the body grew a lobe, and the pin still failed. */
    double maxBulge = 0.0;
    for (int j = 0; j < 8; ++j) maxBulge = std::max (maxBulge, w.bulge[j]);
    const double lim = APO * wl.scale + maxBulge + 0.5;
    const double R   = radius + 1.5;

    struct Half { int i; double par, perp; };
    std::vector<Half> xs, ys;

    const int Bmax = (int) std::ceil ((R + std::abs (gx) + 2.0) / SQRT2) + 2;
    for (int B = -Bmax; B <= Bmax; ++B)
    {
        const double bq = (double) B / SQRT2;
        const int Alo = (int) std::floor (bq + gx - lim);
        const int Ahi = (int) std::ceil  (bq + gx + lim);
        for (int A = Alo; A <= Ahi; ++A)
        {
            const double xp = (double) A + bq, xq = (double) A - bq;
            if (std::abs (xq - gx) > lim || std::abs (xp) > R) continue;
            xs.push_back ({ B, xp, xq });
            // A is recoverable: A = (xp + xq)/2. Store B in .i, A implied.
            xs.back().i = (A << 12) | (B & 0xfff);
        }
    }
    const int Mmax = (int) std::ceil ((R + std::abs (gy) + 2.0) / SQRT2) + 2;
    for (int M = -Mmax; M <= Mmax; ++M)
    {
        const double mq = (double) M / SQRT2;
        const int Clo = (int) std::floor (mq - gy - lim);
        const int Chi = (int) std::ceil  (mq - gy + lim);
        for (int C = Clo; C <= Chi; ++C)
        {
            const double yp = mq + (double) C, yq = mq - (double) C;
            if (std::abs (yq - gy) > lim || std::abs (yp) > R) continue;
            ys.push_back ({ (M << 12) | (C & 0xfff), yp, yq });
        }
    }

    const double r2 = radius * radius;
    for (const auto& X : xs)
    {
        const int A = X.i >> 12;
        const int B = (int) (int16_t) ((X.i & 0xfff) << 4) >> 4;
        for (const auto& Y : ys)
        {
            const int M = Y.i >> 12;
            if (((B ^ M) & 1) != 0) continue;                 // B and M share parity
            if (X.par * X.par + Y.par * Y.par > r2) continue;

            const double occ = wl.occupancy (X.perp - gx, Y.perp - gy);
            if (occ <= 0.0) continue;

            const int C = (int) (int16_t) ((Y.i & 0xfff) << 4) >> 4;
            Site s;
            s.x = (float) (X.par * scale); s.y = (float) (Y.par * scale);
            s.px = (float) X.perp; s.py = (float) Y.perp;
            s.occ = (float) occ;
            s.field = (float) h.eval (X.perp, Y.perp);
            s.n[0] = A; s.n[1] = (M + B) / 2; s.n[2] = C; s.n[3] = (M - B) / 2;
            s.level = level;
            out.sites.push_back (s);
        }
    }
}

void buildPatch (Patch& out, const Window& w, const Habit& h,
                 double gx, double gy, double radius, int levels)
{
    out.sites.clear(); out.edges.clear(); out.tiles.clear();

    const int base = 0;
    enumerateLevel (out, w, h, gx, gy, radius, 0, 1.0);
    const int nBase = (int) out.sites.size();

    std::unordered_map<uint64_t, int> map;
    map.reserve ((size_t) nBase * 2);
    for (int i = base; i < nBase; ++i)
    {
        const auto& s = out.sites[(size_t) i];
        map[key4 (s.n[0], s.n[1], s.n[2], s.n[3])] = i;
    }

    // edges: neighbours differ by one unit vector of Z^4, so an edge of the
    // tiling is a step of exactly one along one of the four axes
    for (int i = base; i < nBase; ++i)
    {
        const auto& s = out.sites[(size_t) i];
        for (int k = 0; k < 4; ++k)
        {
            int n[4] = { s.n[0], s.n[1], s.n[2], s.n[3] };
            ++n[k];
            auto it = map.find (key4 (n[0], n[1], n[2], n[3]));
            if (it != map.end()) out.edges.push_back ({ i, it->second });
        }
    }

    // tiles: the 4-cycles. |k-l| == 2 is a square, otherwise a 45° rhombus
    for (int i = base; i < nBase; ++i)
    {
        const auto& s = out.sites[(size_t) i];
        for (int k = 0; k < 4; ++k)
            for (int l = k + 1; l < 4; ++l)
            {
                int a[4] = { s.n[0], s.n[1], s.n[2], s.n[3] }; ++a[k];
                int b[4] = { s.n[0], s.n[1], s.n[2], s.n[3] }; ++b[l];
                int c[4] = { s.n[0], s.n[1], s.n[2], s.n[3] }; ++c[k]; ++c[l];
                auto ia = map.find (key4 (a[0], a[1], a[2], a[3])); if (ia == map.end()) continue;
                auto ib = map.find (key4 (b[0], b[1], b[2], b[3])); if (ib == map.end()) continue;
                auto ic = map.find (key4 (c[0], c[1], c[2], c[3])); if (ic == map.end()) continue;
                Tile t; t.v[0] = i; t.v[1] = ia->second; t.v[2] = ic->second; t.v[3] = ib->second;
                t.kind = (l - k == 2) ? 1 : 0;
                out.tiles.push_back (t);
            }
    }

    /*  The artefact contains itself. Shrinking the window by the silver ratio
        selects a sub-quasilattice whose spacing is silver times larger — the
        inflation, drawn on top of the tiling it belongs to. */
    for (int L = 1; L <= levels; ++L)
        enumerateLevel (out, w, h, gx, gy, radius, L, 1.0);
}

//==============================================================================
void buildChain (std::vector<ChainSite>& out, const Window& w, const Habit& h,
                 double gx, double gy, double bearingRad, double offset,
                 int wantSites, double halfWidth, double strain)
{
    out.clear();
    const double cx = std::cos (bearingRad), sy = std::sin (bearingRad);
    const double qx = -sy, qy = cx;                 // the perpendicular of the cut
    double maxBulge = 0.0;
    for (int j = 0; j < 8; ++j) maxBulge = std::max (maxBulge, w.bulge[j]);
    const double lim = APO * w.scale + maxBulge + 0.5;

    // Reach far enough along the cut to hold the sites we want (mean spacing
    // of the octagonal quasilattice along an axis is a little under one edge).
    const double S = 0.75 * (double) wantSites + 6.0;

    /*  The enumeration box must cover the window WHEREVER THE STRAIN TAKES IT.
        Sized from the unstrained window it would search the middle of the
        filament and find nothing at either end -- the same mistake the arrests
        already taught once, when a box sized from the undeformed window lost
        the one site a pin existed to keep. The shift is strain * s along the
        cut, s runs to +/- S, and it lands on x and y in proportion to the
        bearing, so each axis is widened by its own share. */
    const double sweep = std::abs (strain) * S;
    const double limx = lim + sweep * std::abs (cx);
    const double limy = lim + sweep * std::abs (sy);
    const double slack = halfWidth + std::max (limx, limy) * 1.5 + 0.6;

    std::vector<ChainSite> hits;
    hits.reserve ((size_t) wantSites * 2 + 32);

    const bool xFree = std::abs (cx) >= std::abs (sy);

    auto emit = [&] (int A, int B, int M, int C, double xp, double xq, double yp, double yq)
    {
        const double d = xp * qx + yp * qy - offset;
        if (std::abs (d) > halfWidth) return;
        const double s = xp * cx + yp * sy;
        if (std::abs (s) > S) return;
        /*  OBLIQUITY: the window this site is tested against is the window as
            it stands HERE, at this point along the cut, not as it stands at
            the middle. Membership is what the strain changes; position is not.
            The shadow recorded is the strained one too, because that is the
            site's actual place in the unseen plane relative to the acceptance
            it was judged by, and the habit's field is read there. */
        const double sx = xq - gx - strain * s * cx;
        const double sy_ = yq - gy - strain * s * sy;
        const double occ = w.occupancy (sx, sy_);
        if (occ <= 0.0) return;
        ChainSite c;
        c.s = s; c.px = sx + gx; c.py = sy_ + gy; c.occ = occ;
        c.field = h.eval (c.px, c.py); c.x = xp; c.y = yp;
        hits.push_back (c);
        (void) A; (void) B; (void) M; (void) C;
    };

    if (xFree)
    {
        const int Bmax = (int) std::ceil ((S / std::max (0.7, std::abs (cx)) + std::abs (gx) + 3.0) / SQRT2) + 2;
        for (int B = -Bmax; B <= Bmax; ++B)
        {
            const double bq = (double) B / SQRT2;
            for (int A = (int) std::floor (bq + gx - limx); A <= (int) std::ceil (bq + gx + limx); ++A)
            {
                const double xp = (double) A + bq, xq = (double) A - bq;
                if (std::abs (xq - gx) > limx) continue;
                // solve the strip condition for y_par, then for M
                const double lo = (offset - slack - xp * qx) / qy;
                const double hi = (offset + slack - xp * qx) / qy;
                const double ylo = std::min (lo, hi), yhi = std::max (lo, hi);
                const int Mlo = (int) std::floor ((ylo + gy - limy) * SQRT2 * 0.5) - 1;
                const int Mhi = (int) std::ceil  ((yhi + gy + limy) * SQRT2 * 0.5) + 1;
                for (int M = Mlo; M <= Mhi; ++M)
                {
                    if (((B ^ M) & 1) != 0) continue;
                    const double mq = (double) M / SQRT2;
                    for (int C = (int) std::floor (mq - gy - limy); C <= (int) std::ceil (mq - gy + limy); ++C)
                    {
                        const double yp = mq + (double) C, yq = mq - (double) C;
                        if (std::abs (yq - gy) > limy) continue;
                        emit (A, B, M, C, xp, xq, yp, yq);
                    }
                }
            }
        }
    }
    else
    {
        const int Mmax = (int) std::ceil ((S / std::max (0.7, std::abs (sy)) + std::abs (gy) + 3.0) / SQRT2) + 2;
        for (int M = -Mmax; M <= Mmax; ++M)
        {
            const double mq = (double) M / SQRT2;
            for (int C = (int) std::floor (mq - gy - limy); C <= (int) std::ceil (mq - gy + limy); ++C)
            {
                const double yp = mq + (double) C, yq = mq - (double) C;
                if (std::abs (yq - gy) > limy) continue;
                const double lo = (offset - slack - yp * qy) / qx;
                const double hi = (offset + slack - yp * qy) / qx;
                const double xlo = std::min (lo, hi), xhi = std::max (lo, hi);
                const int Blo = (int) std::floor ((xlo - gx - limx) * SQRT2 * 0.5) - 1;
                const int Bhi = (int) std::ceil  ((xhi - gx + limx) * SQRT2 * 0.5) + 1;
                for (int B = Blo; B <= Bhi; ++B)
                {
                    if (((B ^ M) & 1) != 0) continue;
                    const double bq = (double) B / SQRT2;
                    for (int A = (int) std::floor (bq + gx - limx); A <= (int) std::ceil (bq + gx + limx); ++A)
                    {
                        const double xp = (double) A + bq, xq = (double) A - bq;
                        if (std::abs (xq - gx) > limx) continue;
                        emit (A, B, M, C, xp, xq, yp, yq);
                    }
                }
            }
        }
    }

    std::sort (hits.begin(), hits.end(), [] (const ChainSite& a, const ChainSite& b) { return a.s < b.s; });

    // de-duplicate coincidences (two lattice points can land on the same
    // position along the cut when the strip is wide)
    std::vector<ChainSite> uniq;
    uniq.reserve (hits.size());
    for (const auto& c : hits)
        if (uniq.empty() || c.s - uniq.back().s > 1.0e-6) uniq.push_back (c);
        else if (c.occ > uniq.back().occ) uniq.back() = c;

    // keep the central run: the cut is a window on the body, centred on it
    const int n = (int) uniq.size();
    if (n <= wantSites) { out = uniq; return; }
    int best = 0; double bestD = 1.0e18;
    for (int i = 0; i + wantSites <= n; ++i)
    {
        const double mid = 0.5 * (uniq[(size_t) i].s + uniq[(size_t) (i + wantSites - 1)].s);
        if (std::abs (mid) < bestD) { bestD = std::abs (mid); best = i; }
    }
    out.assign (uniq.begin() + best, uniq.begin() + best + wantSites);
}

//==============================================================================
/*  The diffraction of the cut. Peaks live at p + q*sqrt2; the intensity of a
    peak is the window's transform evaluated at its Galois conjugate
    p - q*sqrt2 — the window along the cut is an interval, so that transform
    is exactly a sinc. A partial is loud when its shadow is small. */
void buildStar (std::vector<Peak>& out, const Habit& h, int wantPeaks,
                const std::vector<ChainSite>* chain, double contrast)
{
    out.clear();
    std::vector<Peak> all;
    all.reserve (2048);

    for (int p = -22; p <= 22; ++p)
        for (int q = -16; q <= 16; ++q)
        {
            const double lam = (double) p + (double) q * SQRT2;
            if (lam < 0.06 || lam > 64.0) continue;
            const double cj = (double) p - (double) q * SQRT2;
            /*  Intensity is the window's transform at the Galois conjugate —
                exactly a sinc, because the window along a cut is an interval.
                The last factor is not crystallography but physics: a strike
                does not deposit equal energy at every order, and without a
                radiation rolloff the loudest peaks are the high ones with
                nearly vanishing shadows (7 + 5*sqrt2 has conjugate -0.07) and
                the artefact has no fundamental at all. */
            const double a  = std::abs (sinc (h.starWid * cj))
                            * std::exp (-h.starTilt * std::abs (cj))
                            / (0.35 + std::pow (lam, 1.15));
            if (a < 1.0e-5) continue;
            all.push_back ({ lam, cj, a });
        }

    std::sort (all.begin(), all.end(), [] (const Peak& a, const Peak& b) { return a.amp > b.amp; });

    /*  THE STRUCTURE FACTOR OF THE ACTUAL CHAIN.

        The window's transform alone gives every specimen the same diffraction.
        The reciprocal module of an octagonal quasicrystal is a property of the
        LATTICE, not of what decorates it, so the peaks land in the same places
        whatever the habit and only their envelope shifts a little — and since
        a held note is carried by the star, every specimen faded into the same
        background. That is what Peter heard, and he was right.

        What distinguishes one specimen's diffraction from another's is the
        DECORATION: the structure factor is the transform of the decorated
        lattice, a sum over sites of scattering strength times phase. Here the
        scattering strength is the site's own mass, which is the habit's field.
        Computing it from the chain that is actually sounding also makes this
        exactly — rather than approximately — the Fourier transform of the
        FILAMENT, and makes the star shift as the body is drawn through w. */
    if (chain != nullptr && chain->size() >= 16)
    {
        const int n = (int) chain->size();
        const int step = std::max (1, n / 192);
        double mean = 0.0;
        for (int i = 0; i + 1 < n; ++i) mean += (*chain)[(size_t) (i + 1)].s - (*chain)[(size_t) i].s;
        mean = std::max (1.0e-6, mean / (double) (n - 1));
        const double s0 = (*chain)[0].s;

        auto amplitudeAt = [&] (double k)
        {
            double re = 0.0, im = 0.0, ws = 0.0;
            for (int i = 0; i < n; i += step)
            {
                const auto& c = (*chain)[(size_t) i];
                const double m = std::pow (contrast, c.field - 0.5) * c.occ;
                const double ph = k * (c.s - s0);
                re += m * std::cos (ph); im += m * std::sin (ph); ws += m;
            }
            return std::sqrt (re * re + im * im) / std::max (1.0e-9, ws);
        };

        /*  CALIBRATE THE FUNDAMENTAL, do not assume it.

            The mean spacing of a FINITE chain is not the mean spacing of the
            chain: seventy sites of the silver-mean word average 1.16809 where
            the limit is 4 - 2*sqrt2 = 1.17157, three parts in a thousand out.
            Across seventy spacings that error drifts a fifth of a cycle, and
            the sum for lambda = 1 partly cancels — which is exactly how the
            note itself fell out of the artefact's own diffraction.

            Rather than substitute one assumed constant for another (the limit
            is only right for a cut along an axis; at another bearing the
            spacings are different numbers entirely), find the peak: scan a
            narrow band around unity and refine. Every other order follows,
            because the module scales with its own fundamental. */
        double k0 = 2.0 * PI / mean;
        {
            double bestL = 1.0, bestA = -1.0;
            for (int i = 0; i <= 56; ++i)
            {
                const double lam = 0.93 + 0.14 * (double) i / 56.0;
                const double a = amplitudeAt (k0 * lam);
                if (a > bestA) { bestA = a; bestL = lam; }
            }
            double lo = bestL - 0.0025, hi = bestL + 0.0025;
            for (int it = 0; it < 18; ++it)
            {
                const double m1 = lo + (hi - lo) * 0.382, m2 = lo + (hi - lo) * 0.618;
                if (amplitudeAt (k0 * m1) > amplitudeAt (k0 * m2)) hi = m2; else lo = m1;
            }
            k0 *= 0.5 * (lo + hi);
        }

        /*  FIND the partials; do not assume them.

            What was here evaluated the structure factor at ratios taken from
            the module Z + Z*sqrt2 -- the module of the UNSTRAINED lattice. So
            long as the cut lies along the lattice that is exactly right and
            the two agree to the last digit. Lean the cut, though, and the
            chain's own module is no longer that one: every assumed ratio falls
            off its Bragg condition at once, all of them together, and the star
            stops being the diffraction of the filament. Measured, when the
            module was tilted and the chain was not: the chord moved 62 units
            in the first 2.5 per cent of the knob and then merely wandered.

            So the peaks are now WHERE THE CHAIN PUTS THEM. Scan the structure
            factor across the range, take its local maxima, refine each one.
            The answer is the true diffraction of whatever chain is actually
            sounding -- strained, unstrained, any bearing, any window -- and at
            zero strain it returns the module it used to assume, because that
            is where the Bragg peaks of an unstrained chain are.

            The scan is fine enough to resolve a peak: the width of a Bragg
            peak is one part in the number of periods across the chain, about
            0.006 in lambda for 169 sites, so a step of 0.0015 puts four
            samples on the narrowest peak there can be. */
        const double lamLo = 0.06, lamHi = 64.0, dLam = 0.0015;
        const int nScan = (int) ((lamHi - lamLo) / dLam) + 1;

        /*  A phase recurrence, because 42 000 scan points times 170 sites is
            eight million sines and that is a tenth of a second. Advancing each
            site's phasor by a fixed rotation costs a complex multiply instead.
            Re-seeded from the trigonometry every 256 steps: a rotation applied
            forty thousand times in succession drifts in both angle and
            magnitude, and this has to be deterministic to the last bit. */
        std::vector<double> mass, ds;
        mass.reserve ((size_t) n / (size_t) step + 2);
        ds.reserve   ((size_t) n / (size_t) step + 2);
        double wsum = 0.0;
        for (int i = 0; i < n; i += step)
        {
            const auto& c = (*chain)[(size_t) i];
            const double m = std::pow (contrast, c.field - 0.5) * c.occ;
            mass.push_back (m); ds.push_back (c.s - s0); wsum += m;
        }
        const int ns = (int) mass.size();
        wsum = std::max (1.0e-9, wsum);

        std::vector<double> spec ((size_t) nScan, 0.0);
        {
            std::vector<double> cr ((size_t) ns), ci ((size_t) ns);
            const double dk = k0 * dLam;
            std::vector<double> rr ((size_t) ns), ri ((size_t) ns);
            for (int j = 0; j < ns; ++j) { rr[(size_t) j] = std::cos (dk * ds[(size_t) j]);
                                           ri[(size_t) j] = std::sin (dk * ds[(size_t) j]); }
            for (int t = 0; t < nScan; ++t)
            {
                if ((t & 255) == 0)
                {
                    const double k = k0 * (lamLo + dLam * (double) t);
                    for (int j = 0; j < ns; ++j)
                    { cr[(size_t) j] = std::cos (k * ds[(size_t) j]);
                      ci[(size_t) j] = std::sin (k * ds[(size_t) j]); }
                }
                double re = 0.0, im = 0.0;
                for (int j = 0; j < ns; ++j)
                { const double m = mass[(size_t) j]; re += m * cr[(size_t) j]; im += m * ci[(size_t) j]; }
                spec[(size_t) t] = std::sqrt (re * re + im * im) / wsum;
                for (int j = 0; j < ns; ++j)
                {
                    const double a = cr[(size_t) j], b = ci[(size_t) j];
                    cr[(size_t) j] = a * rr[(size_t) j] - b * ri[(size_t) j];
                    ci[(size_t) j] = a * ri[(size_t) j] + b * rr[(size_t) j];
                }
            }
        }

        /*  Every strict local maximum is a candidate partial. The rolloff is
            applied here rather than to the scan, so that a tall peak high up
            still registers as a peak and only then is weighed. */
        /*  THE SHADOW, which recovering the peaks empirically nearly lost.

            Every local maximum is a candidate, but not every local maximum is
            a partial anyone should hear. A structure factor tends to unity as
            k tends to zero -- all the mass in phase, the forward beam -- and
            the radiation rolloff, which divides by 0.35 + lambda^1.15,
            MULTIPLIES that skirt by two and a half. Selecting on loudness
            alone therefore fills the star with sub-audible rumble: measured,
            44 per cent of all partials fell below the fundamental, and habit
            16's second, third and fourth loudest sat at lambda 0.082, 0.163
            and 0.120 with shadows of fourteen and twenty.

            The old enumerated code never showed this, and it is worth being
            clear about why, because it was not luck. Its candidates were
            screened by the window's own transform -- |sinc(starWid * conj)|
            times exp(-starTilt * |conj|) -- before the structure factor was
            ever consulted, so a ratio with a shadow of fourteen never reached
            the shortlist. That screen is the instrument's founding rule: a
            partial is loud when its shadow is small. It belongs here too.

            It weighs the CHOICE and not the sound. The amplitude a partial
            finally has is still the structure factor of the chain, exactly as
            measured; the shadow only decides which partials are the artefact's
            and which are the noise floor of the arithmetic. */
        auto shadowOf = [] (double lam)
        {
            double bestErr = 1.0e18, cj = 0.0;
            for (int q = -16; q <= 16; ++q)
            {
                const double pf = lam - (double) q * SQRT2;
                const int p = (int) std::floor (pf + 0.5);
                if (p < -22 || p > 22) continue;
                const double err = std::abs (pf - (double) p);
                if (err < bestErr) { bestErr = err; cj = (double) p - (double) q * SQRT2; }
            }
            return cj;
        };
        auto prior = [&] (double cj)
        {
            return std::abs (sinc (h.starWid * cj)) * std::exp (-h.starTilt * std::abs (cj));
        };

        std::vector<Peak> found;
        found.reserve (1024);
        for (int t = 1; t + 1 < nScan; ++t)
            if (spec[(size_t) t] > spec[(size_t) t - 1] && spec[(size_t) t] >= spec[(size_t) t + 1])
            {
                const double lam = lamLo + dLam * (double) t;
                found.push_back ({ lam, shadowOf (lam),
                                   spec[(size_t) t] / (0.35 + std::pow (lam, 1.15)) });
            }
        std::sort (found.begin(), found.end(), [&] (const Peak& a, const Peak& b)
                   { return a.amp * prior (a.conj) > b.amp * prior (b.conj); });
        if ((int) found.size() > wantPeaks * 4) found.resize ((size_t) wantPeaks * 4);

        /*  Refine the survivors to the true maximum -- a scan step is 1.5
            parts in a thousand, which is two and a half cents, and a partial
            two cents out of place beats against the one that is not. */
        for (auto& pk : found)
        {
            double lo = pk.lam - dLam, hi = pk.lam + dLam;
            for (int it = 0; it < 24; ++it)
            {
                const double m1 = lo + (hi - lo) * 0.382, m2 = lo + (hi - lo) * 0.618;
                if (amplitudeAt (k0 * m1) > amplitudeAt (k0 * m2)) hi = m2; else lo = m1;
            }
            pk.lam = 0.5 * (lo + hi);
            pk.amp = amplitudeAt (k0 * pk.lam) / (0.35 + std::pow (pk.lam, 1.15));
            /*  A partial's conjugate is its DRIFT RATE -- the engine sweeps
                each one by base*lam + drift*conj as the body is drawn through
                w -- so it is re-read at the refined ratio, not left at the one
                the scan grid happened to land on. */
            pk.conj = shadowOf (pk.lam);
        }
        std::sort (found.begin(), found.end(), [&] (const Peak& a, const Peak& b)
                   { return a.amp * prior (a.conj) > b.amp * prior (b.conj); });

        /*  WHAT THE CHAIN CANNOT RESOLVE IS NOT TWO PARTIALS.

            A chain of n periods has a main lobe about 1/n wide and its first
            sidelobes at 1.43/n, thirteen decibels down. Those sidelobes are
            local maxima, so the scan finds them and reports them as partials,
            and they were: habit 8 had 0.99156 and 1.00848 flanking its
            fundamental at -13.0 dB and 1.42/169 away, which is the textbook
            figure to three digits. Keeping them spends three of twenty-four
            partials on one Bragg peak and puts a beat on the fundamental that
            belongs to the length of the chain rather than to the specimen.

            So a candidate within 1.8/n of a louder one already chosen is that
            one, seen again. This is a statement about measurement, not about
            taste: nothing closer than the resolution limit is distinguishable
            by any means from a single peak. */
        {
            const double sep = 1.8 / (double) n;
            std::vector<Peak> kept;
            kept.reserve ((size_t) wantPeaks);
            for (const auto& pk : found)
            {
                bool shadowed = false;
                for (const auto& k : kept)
                    if (std::abs (k.lam - pk.lam) < sep) { shadowed = true; break; }
                if (! shadowed) kept.push_back (pk);
                if ((int) kept.size() >= wantPeaks) break;
            }
            found.swap (kept);
        }
        all.swap (found);
    }

    if ((int) all.size() > wantPeaks) all.resize ((size_t) wantPeaks);
    std::sort (all.begin(), all.end(), [] (const Peak& a, const Peak& b) { return a.lam < b.lam; });

    double sum = 0.0;
    for (const auto& p : all) sum += p.amp;
    if (sum > 0.0) for (auto& p : all) p.amp /= sum;
    out.swap (all);
}

} // namespace ab
