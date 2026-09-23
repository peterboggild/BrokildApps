/*  ARTEFACT B2311.22 — specimen generation.
    See Specimen.h for the design; every step here is deterministic from the
    catalog number, and alienness is enforced by measurement at the end.
*/

#include "Specimen.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ab
{

namespace
{

//  ---------------------------------------------------------------- graphs --
struct G
{
    int n = 0;
    int ne = 0;
    int ea[kMaxEdges], eb[kMaxEdges];
    float ew[kMaxEdges];

    bool has (int a, int b) const
    {
        for (int e = 0; e < ne; ++e)
            if ((ea[e] == a && eb[e] == b) || (ea[e] == b && eb[e] == a)) return true;
        return false;
    }
    void add (int a, int b, float w)
    {
        if (a == b || a < 0 || b < 0 || a >= n || b >= n) return;
        if (ne >= kMaxEdges || has (a, b)) return;
        ea[ne] = a; eb[ne] = b; ew[ne] = w; ++ne;
    }
};

//  Topology families. Each produces spectra with a different character —
//  small-world rings give clustered bands, trees give sparse gapped ladders,
//  bipartite structures mirror themselves, communities give formant clumps.
void buildGraph (Rng& r, int family, G& g)
{
    const int n = kMinNodes + r.irange (kMaxNodes - kMinNodes + 1);
    g.n = n;
    g.ne = 0;
    auto w = [&r]() { return 0.25f + 1.55f * r.uni() * r.uni(); };

    switch (family)
    {
        case 0:   // ring with chords (small world)
        {
            for (int i = 0; i < n; ++i) g.add (i, (i + 1) % n, w());
            const int chords = 3 + r.irange (n / 4);
            for (int c = 0; c < chords; ++c)
                g.add (r.irange (n), r.irange (n), w());
            break;
        }
        case 1:   // random tree with occasional vines
        {
            for (int i = 1; i < n; ++i)
            {
                //  prefer recent nodes -> long dangling limbs
                const int reach = 1 + r.irange (std::min (i, 6));
                g.add (i, i - reach, w());
            }
            const int vines = r.irange (4);
            for (int v = 0; v < vines; ++v)
                g.add (r.irange (n), r.irange (n), w() * 0.4f);
            break;
        }
        case 2:   // bipartite-leaning: two shores, edges mostly across
        {
            const int shoreA = n / 2 + r.irange (5) - 2;
            for (int i = 0; i < n; ++i)
            {
                const int deg = 1 + r.irange (3);
                for (int d = 0; d < deg; ++d)
                {
                    if (i < shoreA) g.add (i, shoreA + r.irange (n - shoreA), w());
                    else            g.add (i, r.irange (shoreA), w());
                }
            }
            break;
        }
        case 3:   // communities: 2-4 dense blobs, sparse bridges
        {
            const int k = 2 + r.irange (3);
            int cut[5] = { 0 };
            for (int c = 1; c < k; ++c) cut[c] = cut[c - 1] + n / k;
            cut[k] = n;
            for (int c = 0; c < k; ++c)
            {
                const int lo = cut[c], hi = cut[c + 1];
                const int m = hi - lo;
                const int dens = m + r.irange (m * 2);
                for (int e = 0; e < dens; ++e)
                    g.add (lo + r.irange (m), lo + r.irange (m), w());
                //  spanning inside the blob so nothing is orphaned
                for (int i = lo + 1; i < hi; ++i) g.add (i, lo + r.irange (i - lo), w());
            }
            for (int c = 1; c < k; ++c)          // one thin bridge per border
                g.add (cut[c] - 1, cut[c], w() * 0.3f);
            break;
        }
        case 4:   // near-regular tangle (expander-ish)
        {
            for (int i = 1; i < n; ++i) g.add (i, r.irange (i), w());   // connected
            const int extra = n + r.irange (n);
            for (int e = 0; e < extra; ++e) g.add (r.irange (n), r.irange (n), w());
            break;
        }
        default:  // 5: chain of pearls — segments of ring strung on a line
        {
            int at = 0;
            int prevAnchor = -1;
            while (at < n - 3)
            {
                const int m = std::min (4 + r.irange (6), n - at);
                for (int i = 0; i < m; ++i) g.add (at + i, at + (i + 1) % m, w());
                if (prevAnchor >= 0) g.add (prevAnchor, at, w() * 0.5f);
                prevAnchor = at + r.irange (m);
                at += m;
            }
            for (; at < n; ++at) g.add (at, r.irange (at), w());
            break;
        }
    }
    //  guarantee connectivity: union-find sweep, bridge any stray component
    int parent[kMaxNodes];
    for (int i = 0; i < n; ++i) parent[i] = i;
    auto find = [&parent] (int x) { while (parent[x] != x) x = parent[x] = parent[parent[x]]; return x; };
    for (int e = 0; e < g.ne; ++e)
    {
        int a = find (g.ea[e]), b = find (g.eb[e]);
        if (a != b) parent[a] = b;
    }
    for (int i = 1; i < n; ++i)
        if (find (i) != find (0))
        {
            g.add (i, 0, 0.4f);
            parent[find (i)] = find (0);
        }
}

//  ---------------------------------------------------- Jacobi eigensolver --
//  Cyclic Jacobi on the symmetric Laplacian. N <= 72: milliseconds. A is
//  destroyed; V returns eigenvectors as COLUMNS (V[i*n+m] = component i of
//  eigenvector m); d returns eigenvalues. Then sorted ascending.
void jacobi (double* A, double* V, double* d, int n)
{
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            V[i * n + j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 60; ++sweep)
    {
        double off = 0;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q)
                off += A[p * n + q] * A[p * n + q];
        if (off < 1e-18) break;

        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q)
            {
                const double apq = A[p * n + q];
                if (std::fabs (apq) < 1e-13) continue;
                const double app = A[p * n + p], aqq = A[q * n + q];
                const double theta = 0.5 * (aqq - app) / apq;
                const double t = (theta >= 0 ? 1.0 : -1.0)
                               / (std::fabs (theta) + std::sqrt (theta * theta + 1.0));
                const double c = 1.0 / std::sqrt (t * t + 1.0);
                const double s = t * c;

                for (int i = 0; i < n; ++i)
                {
                    const double aip = A[i * n + p], aiq = A[i * n + q];
                    A[i * n + p] = c * aip - s * aiq;
                    A[i * n + q] = s * aip + c * aiq;
                }
                for (int i = 0; i < n; ++i)
                {
                    const double api = A[p * n + i], aqi = A[q * n + i];
                    A[p * n + i] = c * api - s * aqi;
                    A[q * n + i] = s * api + c * aqi;
                }
                for (int i = 0; i < n; ++i)
                {
                    const double vip = V[i * n + p], viq = V[i * n + q];
                    V[i * n + p] = c * vip - s * viq;
                    V[i * n + q] = s * vip + c * viq;
                }
            }
    }
    for (int i = 0; i < n; ++i) d[i] = A[i * n + i];
}

//  --------------------------------------------- the non-harmonicity floor --
//  Best harmonic fit: search fundamental candidates u; for each, RMS cents
//  deviation of the leading ratios from nearest integer multiples of u.
//  Return the MINIMUM over u — the closest any harmonic series comes.
float harmonicMargin (const float* ratio, int nModes)
{
    const int use = std::min (nModes, 28);      // the audible-weight leaders
    float best = 1.0e9f;
    for (float u = 0.19f; u <= 1.02f; u += 0.004f)
    {
        double sum = 0;
        int cnt = 0;
        for (int k = 0; k < use; ++k)
        {
            const float m = ratio[k] / u;
            const float nearest = std::round (m);
            if (nearest < 1.0f) continue;
            const float cents = 1200.0f * std::log2 (m / nearest);
            sum += (double) cents * cents;
            ++cnt;
        }
        if (cnt >= use / 2)
        {
            const float rms = (float) std::sqrt (sum / (double) cnt);
            best = std::min (best, rms);
        }
    }
    return best;
}

} // anonymous

//  ------------------------------------------------------------- generator --
/*  Everything downstream of the graph: eigenmodes, spectrum, the 4D body,
    ear weights, coupling, excitation path, ladder. Split out of
    generateSpecimen so a body whose edges have been REMODELLED by
    listening can be re-solved without being reinvented. */
static void deriveFromGraph (const G& g, Specimen& out)
{
    const int n = g.n;
    out.nNodes = n;
    out.nEdges = g.ne;
    for (int e = 0; e < g.ne; ++e)
    {
        out.edgeA[e] = g.ea[e];
        out.edgeB[e] = g.eb[e];
        out.edgeW[e] = g.ew[e];
    }

    //  weighted Laplacian
    static thread_local std::array<double, (size_t) kMaxNodes * kMaxNodes> A {}, V {};
    static thread_local std::array<double, kMaxNodes> d {};
    std::fill (A.begin(), A.end(), 0.0);
    for (int e = 0; e < g.ne; ++e)
    {
        const int a = g.ea[e], b = g.eb[e];
        const double w = g.ew[e];
        A[(size_t) a * n + b] -= w;
        A[(size_t) b * n + a] -= w;
        A[(size_t) a * n + a] += w;
        A[(size_t) b * n + b] += w;
    }
    //  keep a copy for the residual check
    static thread_local std::array<double, (size_t) kMaxNodes * kMaxNodes> A0 {};
    std::copy (A.begin(), A.begin() + (size_t) n * n, A0.begin());

    jacobi (A.data(), V.data(), d.data(), n);

    //  sort ascending by eigenvalue
    int order[kMaxNodes];
    for (int i = 0; i < n; ++i) order[i] = i;
    std::sort (order, order + n, [&] (int a, int b) { return d[a] < d[b]; });

    const int nm = std::min (n - 1, kMaxModes);
    out.nModes = nm;

    //  eigen residual on a few modes (bench honesty)
    double resid = 0;
    for (int chk = 1; chk < n; chk += std::max (1, n / 6))
    {
        const int m = order[chk];
        for (int i = 0; i < n; ++i)
        {
            double av = 0;
            for (int j = 0; j < n; ++j) av += A0[(size_t) i * n + j] * V[(size_t) j * n + m];
            resid = std::max (resid, std::fabs (av - d[m] * V[(size_t) i * n + m]));
        }
    }
    out.eigenResidual = (float) resid;

    /*  The body: four eigenvectors ARE its spatial axes (mode = dimension).
        But an axis has to spread the body along itself, and a LOCALISED
        eigenvector — one large node, the rest flat, entirely ordinary in
        tree and community topologies — does the opposite: it puts the whole
        creature at a single coordinate. Three catalog entries were mute
        because their w axis did exactly that: no extent in the fourth
        dimension means no cross-section, so nothing was ever in the slab.
        So: rank the lowest dozen modes by inverse participation ratio and
        take the four most delocalised, then restore eigenvalue order. */
    auto axis = [&] (int which, float* dst)
    {
        const int m = order[std::min (which, n - 1)];
        //  robust normalisation: median-centred, 10-90 percentile scale,
        //  soft-bounded. Min/max lets one outlier squash everything else.
        double raw[kMaxNodes], srt[kMaxNodes];
        for (int i = 0; i < n; ++i) raw[i] = V[(size_t) i * n + m];
        std::copy (raw, raw + n, srt);
        std::sort (srt, srt + n);
        const double med = srt[n / 2];
        const double p10 = srt[(int) (n * 0.10)];
        const double p90 = srt[(int) (n * 0.90)];
        const double sc = std::max (1e-12, std::max (p90 - med, med - p10));
        for (int i = 0; i < n; ++i)
            dst[i] = (float) std::tanh ((raw[i] - med) / sc);
    };
    {
        auto ipr = [&] (int idx) -> double
        {
            const int m = order[idx];
            double s2 = 0, s4 = 0;
            for (int i = 0; i < n; ++i)
            {
                const double v = V[(size_t) i * n + m];
                s2 += v * v; s4 += v * v * v * v;
            }
            return s2 > 1e-18 ? s4 / (s2 * s2) : 1.0;    // 1 = one node only
        };
        int cand[16], nc = 0;
        for (int c = 1; c <= std::min (n - 1, 13) && nc < 16; ++c) cand[nc++] = c;
        std::sort (cand, cand + nc, [&] (int a, int b) { return ipr (a) < ipr (b); });
        int ax[4];
        for (int t = 0; t < 4; ++t) ax[t] = cand[nc > t ? t : nc - 1];
        std::sort (ax, ax + 4);            // keep them in eigenvalue order
        axis (ax[0], out.px.data());
        axis (ax[1], out.py.data());
        axis (ax[2], out.pz.data());
        axis (ax[3], out.pw.data());
    }

    //  spectrum: membrane law f ~ sqrt(lambda), per-specimen stretch
    const double l1 = std::max (1e-9, d[order[1]]);
    const float gamma = out.genGamma;
    for (int k = 0; k < nm; ++k)
    {
        const double lam = std::max (l1, d[order[k + 1]]);
        out.ratio[k] = (float) std::pow (std::sqrt (lam / l1), (double) gamma);
    }
    //  spread control: scale so the top mode lands somewhere 7..26 x
    const float wantTop = out.genWantTop;
    const float haveTop = std::max (1.001f, out.ratio[nm - 1]);
    const float stretch = std::log (wantTop) / std::log (haveTop);
    for (int k = 0; k < nm; ++k)
        out.ratio[k] = std::pow (out.ratio[k], stretch);

    //  per-mode: ear amplitudes, phase skew, decay bias, eigenvector table
    //  ear nodes = extremes of the long axis (first spatial eigenvector)
    {
        int lo = 0, hi = 0;
        for (int i = 1; i < n; ++i)
        {
            if (out.px[i] < out.px[lo]) lo = i;
            if (out.px[i] > out.px[hi]) hi = i;
        }
        out.earL = lo; out.earR = hi;
    }
    for (int k = 0; k < nm; ++k)
    {
        const int m = order[k + 1];
        double pr = 0;      // participation (localisation) for decay bias
        for (int i = 0; i < n; ++i)
        {
            const double v = V[(size_t) i * n + m];
            out.vec[(size_t) k * n + i] = (float) v;
            pr += v * v * v * v;
        }
        //  inverse participation ratio in [1/n, 1]: 1 = fully localised
        out.decayBias[k] = (float) pr;

        const float vl = out.vec[(size_t) k * n + out.earL];
        const float vr = out.vec[(size_t) k * n + out.earR];
        out.ampL[k] = std::fabs (vl);
        out.ampR[k] = std::fabs (vr);
        //  opposite eigenvector sign at the ears -> a small interaural
        //  phase skew (width without mono cancellation)
        out.phaseSkew[k] = (vl * vr < 0.0f) ? 0.42f : 0.0f;
    }
    //  normalise ear amplitude balance so no mode is silent in both ears
    for (int k = 0; k < nm; ++k)
    {
        const float mx = std::max (out.ampL[k], out.ampR[k]);
        const float floor_ = 0.12f * mx + 1e-6f;
        out.ampL[k] = std::max (out.ampL[k], floor_);
        out.ampR[k] = std::max (out.ampR[k], floor_);
    }

    //  REVIVAL grid: snap every partial's ratio so that all pairwise
    //  frequency differences become multiples of a slow delta. The exact
    //  recurrence period (at f0 = 220 Hz reference) lands at 1.5..4 s.
    {
        //  (drawn by the generator; preserved across a re-solve)
        //  delta in ratio units at the reference: dr = 1/(f0 * T)
        const float dr = 1.0f / (220.0f * out.revivalSeconds);
        for (int k = 0; k < nm; ++k)
            out.ratioSnap[k] = std::max (dr, std::round (out.ratio[k] / dr) * dr);
    }

    //  mode coupling along real edges: C_kl = sum over edges (i,j) of
    //  w * (vk(i) vl(j) + vk(j) vl(i)) — how strongly the anatomy lets
    //  energy cross between two modes. Keep the strongest per mode.
    {
        for (int k = 0; k < nm; ++k)
        {
            float bestW[kCouplePer] = { 0, 0, 0, 0 };
            int   bestI[kCouplePer] = { -1, -1, -1, -1 };
            for (int l = 0; l < nm; ++l)
            {
                if (l == k) continue;
                double c = 0;
                for (int e = 0; e < g.ne; ++e)
                {
                    const int a = g.ea[e], b = g.eb[e];
                    c += (double) g.ew[e]
                       * (out.vec[(size_t) k * n + a] * out.vec[(size_t) l * n + b]
                        + out.vec[(size_t) k * n + b] * out.vec[(size_t) l * n + a]);
                }
                const float ac = (float) std::fabs (c);
                //  insert into top-4
                for (int t = 0; t < kCouplePer; ++t)
                    if (ac > bestW[t])
                    {
                        for (int u = kCouplePer - 1; u > t; --u)
                        {
                            bestW[u] = bestW[u - 1];
                            bestI[u] = bestI[u - 1];
                        }
                        bestW[t] = ac;
                        bestI[t] = l;
                        break;
                    }
            }
            float norm = 0;
            for (int t = 0; t < kCouplePer; ++t) norm = std::max (norm, bestW[t]);
            for (int t = 0; t < kCouplePer; ++t)
            {
                out.coupleTo[(size_t) k * kCouplePer + t] = bestI[t];
                out.coupleW [(size_t) k * kCouplePer + t] = norm > 0 ? bestW[t] / norm : 0.0f;
            }
        }
    }

    //  excitation path: hub (max weighted degree) -> most remote node
    {
        float deg[kMaxNodes] = { 0 };
        for (int e = 0; e < g.ne; ++e)
        {
            deg[g.ea[e]] += g.ew[e];
            deg[g.eb[e]] += g.ew[e];
        }
        int hub = 0;
        for (int i = 1; i < n; ++i) if (deg[i] > deg[hub]) hub = i;

        //  BFS for the farthest node, remembering parents
        int dist[kMaxNodes], par[kMaxNodes], q[kMaxNodes];
        for (int i = 0; i < n; ++i) { dist[i] = -1; par[i] = -1; }
        int qh = 0, qt = 0;
        q[qt++] = hub; dist[hub] = 0;
        while (qh < qt)
        {
            const int u = q[qh++];
            for (int e = 0; e < g.ne; ++e)
            {
                int v = -1;
                if (g.ea[e] == u) v = g.eb[e];
                else if (g.eb[e] == u) v = g.ea[e];
                if (v >= 0 && dist[v] < 0)
                {
                    dist[v] = dist[u] + 1;
                    par[v] = u;
                    q[qt++] = v;
                }
            }
        }
        int far = hub;
        for (int i = 0; i < n; ++i) if (dist[i] > dist[far]) far = i;
        //  path far -> hub, then reverse
        int tmp[kMaxPath], tl = 0, at = far;
        while (at >= 0 && tl < kMaxPath) { tmp[tl++] = at; at = par[at]; }
        out.pathLen = tl;
        for (int i = 0; i < tl; ++i) out.path[i] = tmp[tl - 1 - i];   // hub first
    }

    //  sidereal ladder: unique sorted ratios (coarse dedupe at 12 cents)
    {
        float sorted_[kMaxModes];
        std::copy (out.ratio.begin(), out.ratio.begin() + nm, sorted_);
        std::sort (sorted_, sorted_ + nm);
        int L = 0;
        for (int k = 0; k < nm; ++k)
        {
            if (L > 0 && 1200.0f * std::log2 (sorted_[k] / out.ladder[L - 1]) < 12.0f)
                continue;
            out.ladder[L++] = sorted_[k];
        }
        out.ladderLen = L;
    }
}

void generateSpecimen (int num, Specimen& out)
{
    for (int salt = 0; salt < 80; ++salt)
    {
        out = Specimen();
        out.catalog = num;
        out.saltUsed = salt;

        Rng r ((uint64_t) num * 0x51D2FA9ull + (uint64_t) salt * 0xB2311ull + 22u);
        out.family = num % 6;                    // families interleave the catalog
        G g;
        buildGraph (r, out.family, g);

        /*  the generator's three constants, drawn HERE in their original
            order so the Rng sequence — and therefore all 300 catalog
            entries — is bit-identical to before the refactor */
        out.genGamma       = 0.85f + 0.45f * r.uni();
        out.genWantTop     = 7.0f + 19.0f * r.uni();
        out.revivalSeconds = 1.5f + 2.5f * r.uni();

        /*  the natural speaking register: fixed per catalog number, so
            two bodies' ladders genuinely may or may not overlap */
        {
            uint64_t h = (uint64_t) num * 0x9E3779B97f4A7C15ull + 0x2311ull;
            h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
            const float u = (float) ((h >> 40) & 0xFFFF) / 65535.0f;
            out.voiceHz = 58.0f * std::pow (2.0f, 1.75f * u);   // 58..195 Hz
        }

        deriveFromGraph (g, out);
        //  THE FLOOR: no specimen ships near a harmonic series
        out.harmonicMarginCents = harmonicMargin (out.ratio.data(), out.nModes);
        if (out.harmonicMarginCents >= kHarmonicFloor)
            return;                            // passed: this is the specimen
        //  else: too earthly — salt and regenerate (deterministic retry)
    }
    //  80 salts without passing would be extraordinary; ship the last one
    //  anyway rather than crash — the bench will flag it loudly.
}

void resolveSpecimen (Specimen& io)
{
    //  the graph is the specimen's own, edge weights and all: rebuilding
    //  G from it and re-deriving is exactly 'this body, changed'
    G g;
    g.n = io.nNodes;
    g.ne = io.nEdges;
    for (int e = 0; e < io.nEdges; ++e)
    {
        g.ea[e] = io.edgeA[e];
        g.eb[e] = io.edgeB[e];
        g.ew[e] = io.edgeW[e];
    }
    deriveFromGraph (g, io);
    io.harmonicMarginCents = harmonicMargin (io.ratio.data(), io.nModes);
}

float spectralKinship (const Specimen& a, const Specimen& b)
{
    /*  How much of b's ladder a can physically answer: for every partial
        of b, the closest partial of a in cents, folded through a
        tolerance the resonators actually have. Both ladders are taken at
        their own speaking registers — that is the whole point. */
    if (a.nModes <= 0 || b.nModes <= 0) return 0.0f;
    double acc = 0;
    for (int j = 0; j < b.nModes; ++j)
    {
        const float fb = b.voiceHz * b.ratio[j];
        if (fb < 20.0f || fb > 12000.0f) continue;
        float best = 1e9f;
        for (int i = 0; i < a.nModes; ++i)
        {
            const float fa = a.voiceHz * a.ratio[i];
            const float c = std::fabs (1200.0f * std::log2 (fa / fb));
            if (c < best) best = c;
        }
        //  ~70 cents is the half-power width of a Q-25 resonator
        acc += std::exp (-(double) best * best / (2.0 * 70.0 * 70.0));
    }
    return (float) (acc / (double) b.nModes);
}

void fieldPosition (const Specimen& s, float& fx, float& fy)
{
    //  fixed random projection of a spectral feature vector: honest distance,
    //  deterministic forever, no iterative embedding to drift
    float feat[16] = { 0 };
    for (int k = 0; k < s.nModes; ++k)
    {
        const float t = (float) k / (float) std::max (1, s.nModes - 1);
        const int bin = std::min (7, (int) (t * 8.0f));
        feat[bin]     += s.ratio[k] / (1.0f + s.ratio[s.nModes - 1]);
        feat[8 + bin] += s.decayBias[k];
    }
    float x = 0, y = 0;
    for (int i = 0; i < 16; ++i)
    {
        //  fixed +-1 projection masks (arbitrary constants, frozen forever)
        x += feat[i] * (((0x5DEECE66Du >> i) & 1) ? 1.0f : -1.0f);
        y += feat[i] * (((0x2545F491u  >> i) & 1) ? 1.0f : -1.0f);
    }
    fx = std::tanh (x * 0.35f);
    fy = std::tanh (y * 0.35f);
}

} // namespace ab
