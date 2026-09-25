#include "Fdtd.h"
#include "Geometry.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#if defined (_M_X64) || defined (__x86_64__)
 #include <immintrin.h>
 #define TW_PAUSE() _mm_pause()
#else
 #define TW_PAUSE() ((void) 0)
#endif

namespace tw
{

namespace
{
    constexpr double PI_D = 3.141592653589793;

    // normal-incidence admittance for a given absorption (energy) coefficient
    inline float betaOf (float alpha)
    {
        const float r = std::sqrt (std::max (0.0f, 1.0f - std::min (0.999f, std::max (0.0f, alpha))));
        return (1.0f - r) / (1.0f + r);
    }
    inline float lowAlpha (const float* a7) { return 0.5f * (a7[0] + a7[1]); }

    // a Kaiser-windowed sinc low pass, symmetric, 2D + 1 taps, unity at DC
    std::vector<float> lowPassFir (double fs, double fc, int D)
    {
        auto I0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double beta = 6.5;
        std::vector<float> h ((size_t) (2 * D + 1));
        double sum = 0;
        for (int j = -D; j <= D; ++j)
        {
            const double x = 2.0 * fc / fs * j;
            const double sinc = j == 0 ? 1.0 : std::sin (PI_D * x) / (PI_D * x);
            const double r = (double) j / (double) D;
            const double w = I0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / I0 (beta);
            h[(size_t) (j + D)] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& v : h) v = (float) (v / sum);
        return h;
    }

    /*  The same low pass as a continuous function of time, tabulated finely:
        the grid and the host do not share a clock. Normalised to unit area, so
        a constant goes through the resampler unchanged. */
    struct Kernel
    {
        static constexpr int P = 128;
        int D = 0; std::vector<float> t;
        Kernel (double fs, double fc, int d) : D (d)
        {
            auto I0 = [] (double x) { double s = 1, q = 1; for (int k = 1; k < 40; ++k) { q *= (x / (2 * k)) * (x / (2 * k)); s += q; } return s; };
            const double beta = 6.5;
            t.assign ((size_t) (2 * D * P + 2), 0.0f);
            double sum = 0;
            for (int i = 0; i <= 2 * D * P; ++i)
            {
                const double u = (double) i / P - D;                 // samples
                const double x = 2.0 * fc / fs * u;
                const double sinc = std::abs (x) < 1e-12 ? 1.0 : std::sin (PI_D * x) / (PI_D * x);
                const double q = u / D;
                const double w = I0 (beta * std::sqrt (std::max (0.0, 1.0 - q * q))) / I0 (beta);
                t[(size_t) i] = (float) (2.0 * fc / fs * sinc * w);
                sum += t[(size_t) i];
            }
            const double norm = (double) P / sum;              // unit area over samples
            for (auto& v : t) v = (float) (v * norm);
        }
        inline float at (double u) const
        {
            const double p = (u + D) * P;
            if (p < 0 || p >= 2.0 * D * P) return 0.0f;
            const int i = (int) p; const float fr = (float) (p - i);
            return t[(size_t) i] + fr * (t[(size_t) i + 1] - t[(size_t) i]);
        }
    };

    /*  2nd-order Butterworth high pass. Its impulse response has zero area AND
        zero first moment, which is what a source injected as pressure needs:
        what the grid is fed is (the second derivative of) the volume a cone
        pushes out, and a cone that never comes back fills a closed room with
        air - a DC offset that grows with every sound, measured at 0.78 of full
        scale in a four-second take before this. A real loudspeaker returns to
        rest; so does this. 10 Hz is below every mode of the apartment. */
    void highPass2 (float* x, int n, double fs, double fc)
    {
        const double w = 2.0 * PI_D * fc / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + al, b0 = (1.0 + cw) * 0.5 / a0, b1 = -(1.0 + cw) / a0, a1 = -2.0 * cw / a0, a2 = (1.0 - al) / a0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double v = x[i], y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = v; y2 = y1; y1 = y; x[i] = (float) y;
        }
    }

    struct Barrier
    {
        std::atomic<int> count { 0 }, gen { 0 };
        int n = 1;
        void wait()
        {
            const int g = gen.load (std::memory_order_acquire);
            if (count.fetch_add (1, std::memory_order_acq_rel) + 1 == n)
            {
                count.store (0, std::memory_order_relaxed);
                gen.fetch_add (1, std::memory_order_release);
                return;
            }
            int spins = 0;
            while (gen.load (std::memory_order_acquire) == g)
            {
                if (++spins < 4000) TW_PAUSE(); else std::this_thread::yield();
            }
        }
    };

    struct BCell { int idx; int nb[6]; float B; };
    struct Run { int start, len; };

    struct Grid
    {
        double dx = 0.1;
        int nx = 0, ny = 0, nz = 0, sy = 0, sz = 0;
        std::vector<int8_t> room;             // -1: not air
        std::vector<Run> runs;                // interior cells, contiguous along x
        std::vector<BCell> bnd;               // cells with at least one wall
        long long airCells = 0;

        int index (int i, int j, int k) const { return i + nx * (j + ny * k); }
        double cx (int i) const { return (i - 0.5) * dx; }

        void size (double d)
        {
            dx = d;
            nx = (int) std::ceil (APARTMENT_W / dx) + 2;
            ny = (int) std::ceil (APARTMENT_D / dx) + 2;
            double hmax = 0; for (int r = 0; r < NUM_ROOMS; ++r) hmax = std::max (hmax, (double) ROOMS[r].h);
            nz = (int) std::ceil (hmax / dx) + 2;
            sy = nx; sz = nx * ny;
        }

        // the air, the walls and what they are made of, for one state of the apartment
        void build (const Params& P)
        {
            const size_t N = (size_t) nx * ny * nz;
            room.assign (N, (int8_t) -1);
            RoomGeom g[NUM_ROOMS];
            RoomSurfaces surf[NUM_ROOMS];
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                RoomBreaks br;
                br.w[0].along = P.breakAlong[r][0]; br.w[0].push = P.breakPush[r][0];
                br.w[1].along = P.breakAlong[r][1]; br.w[1].push = P.breakPush[r][1];
                g[r] = buildRoomGeom (r, br);
                surf[r].wall = P.material[r]; surf[r].floor = P.floorMat[r]; surf[r].ceil = P.ceilMat[r];
            }
            // furniture that blocks, as solid cells with an admittance of their own
            struct FB { float cx, cy, c, s, hw, hd, zb, zt, beta; int room; };
            std::vector<FB> fb;
            struct FR { float cx, cy, c, s, hw, hd, beta; int room; };
            std::vector<FR> rugs;
            for (int i = 0; i < std::min (std::max (P.nfurn, 0), MAX_FURN); ++i)
            {
                const FurnItem& it = P.furn[i];
                if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
                const FurnSpec& F = FURN[it.type];
                const int rr = roomOf (it.x, it.y);
                if (rr < 0) continue;
                const float a = it.yaw * 3.14159265f / 180.0f;
                if (F.coversFloor)
                {
                    rugs.push_back ({ it.x, it.y, std::cos (a), std::sin (a), 0.5f * F.w, 0.5f * F.d,
                                      betaOf (std::min (0.95f, lowAlpha (F.absorb) / std::max (0.1f, F.w * F.d))), rr });
                    continue;
                }
                if (! F.occludes) continue;               // a curtain barely touches the low end
                const float area = F.w * F.d + 2.0f * (F.w + F.d) * (F.zt - F.zb);
                fb.push_back ({ it.x, it.y, std::cos (a), std::sin (a), 0.5f * F.w, 0.5f * F.d, F.zb, std::max (F.zb + 0.05f, F.zt),
                                betaOf (std::min (0.95f, lowAlpha (F.absorb) / std::max (0.1f, area))), rr });
            }
            auto inBox = [&] (int r, double x, double y, double z) -> int
            {
                for (size_t i = 0; i < fb.size(); ++i)
                {
                    const FB& b = fb[i];
                    if (b.room != r || z < b.zb || z > b.zt) continue;
                    const double dx0 = x - b.cx, dy0 = y - b.cy;
                    const double lx = dx0 * b.c + dy0 * b.s, ly = -dx0 * b.s + dy0 * b.c;
                    if (std::abs (lx) <= b.hw && std::abs (ly) <= b.hd) return (int) i;
                }
                return -1;
            };
            std::vector<int16_t> box (N, (int16_t) -1);
            for (int k = 1; k < nz - 1; ++k)
                for (int j = 1; j < ny - 1; ++j)
                    for (int i = 1; i < nx - 1; ++i)
                    {
                        const double x = cx (i), y = cx (j), z = cx (k);
                        for (int r = 0; r < NUM_ROOMS; ++r)
                        {
                            if (z <= 0 || z >= ROOMS[r].h) continue;
                            if (! insidePolygon (g[r], (float) x, (float) y)) continue;
                            const int bi = inBox (r, x, y, z);
                            if (bi >= 0) { box[(size_t) index (i, j, k)] = (int16_t) bi; break; }
                            room[(size_t) index (i, j, k)] = (int8_t) r;
                            break;
                        }
                    }
            // the doors' open strips
            float lo[NUM_DOORS], hi[NUM_DOORS];
            for (int d = 0; d < NUM_DOORS; ++d)
            {
                Engine::doorOpenStrip (d, P.door[d], lo[d], hi[d]);
                if (P.door[d] <= 0.02f) lo[d] = hi[d] = -1e9f;
            }
            float betaWall[NUM_ROOMS], betaFloor[NUM_ROOMS], betaCeil[NUM_ROOMS];
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                const auto m = [&] (int id) { return std::min (std::max (surf[r].of (id), 0), NUM_MATERIALS - 1); };
                const float wallArea = std::max (1.0f, g[r].perimeter * g[r].height);
                const float share = std::min (1.0f, std::max (0.0f, P.panelArea[r]) / wallArea);
                float aw = lowAlpha (MATERIAL_ALPHA[m (0)]);
                aw += share * std::max (0.0f, 0.5f * (PANEL_ALPHA[0] + PANEL_ALPHA[1]) - aw);
                betaWall[r] = betaOf (aw);
                betaFloor[r] = betaOf (lowAlpha (MATERIAL_ALPHA[m (4)]));
                betaCeil[r] = betaOf (lowAlpha (MATERIAL_ALPHA[m (5)]));
            }
            const float betaLeaf = betaOf (0.10f);

            // is the face between two air cells of different rooms an open doorway?
            auto doorOpenAt = [&] (int ra, int rb, double fx, double fy, double fz, int axis) -> int
            {
                for (int d = 0; d < NUM_DOORS; ++d)
                {
                    const Door& D = DOORS[d];
                    if (! ((D.roomA == ra && D.roomB == rb) || (D.roomA == rb && D.roomB == ra))) continue;
                    if (D.axis != axis) continue;
                    const double span = axis == 0 ? fy : fx;
                    if (fz > D.height || span < D.s0 || span > D.s1) continue;
                    return (span >= lo[d] && span <= hi[d]) ? 1 : 2;     // 1 open, 2 the leaf
                }
                return 0;
            };

            runs.clear(); bnd.clear(); airCells = 0;
            const int off[6] = { -1, 1, -sy, sy, -sz, sz };
            for (int k = 1; k < nz - 1; ++k)
                for (int j = 1; j < ny - 1; ++j)
                {
                    int runStart = -1;
                    for (int i = 1; i < nx - 1; ++i)
                    {
                        const int idx = index (i, j, k);
                        const int r = room[(size_t) idx];
                        bool interior = false;
                        if (r >= 0)
                        {
                            ++airCells;
                            BCell c; c.idx = idx; c.B = 0;
                            bool open[6];
                            for (int f = 0; f < 6; ++f)
                            {
                                const int nb = idx + off[f];
                                const int rn = room[(size_t) nb];
                                open[f] = false;
                                float beta = 0;
                                if (rn == r) open[f] = true;
                                else if (rn >= 0)
                                {
                                    // a different room's air: a doorway, a leaf or a party wall
                                    const double fx = x0Face (i, f), fy = y0Face (j, f), fz = cx (k);
                                    const int dd = doorOpenAt (r, rn, fx, fy, fz, f < 2 ? 0 : 1);
                                    if (dd == 1) open[f] = true;
                                    else beta = dd == 2 ? betaLeaf : betaWall[r];
                                }
                                else if (box[(size_t) nb] >= 0) beta = fb[(size_t) box[(size_t) nb]].beta;
                                else if (f == 4)
                                {
                                    beta = betaFloor[r];
                                    for (const FR& rg : rugs)
                                    {
                                        if (rg.room != r) continue;
                                        const double dx0 = cx (i) - rg.cx, dy0 = cx (j) - rg.cy;
                                        if (std::abs (dx0 * rg.c + dy0 * rg.s) <= rg.hw && std::abs (-dx0 * rg.s + dy0 * rg.c) <= rg.hd) { beta = rg.beta; break; }
                                    }
                                }
                                else if (f == 5) beta = betaCeil[r];
                                else beta = betaWall[r];
                                if (! open[f]) c.B += 0.5f * beta;     // the wall is at the face: half the cell's time derivative
                            }
                            interior = open[0] && open[1] && open[2] && open[3] && open[4] && open[5];
                            if (! interior)
                            {
                                for (int f = 0; f < 6; ++f)
                                {
                                    const int opp = f ^ 1;
                                    (void) opp;
                                    // the wall sits on the face: the missing neighbour mirrors THIS cell
                                    c.nb[f] = open[f] ? idx + off[f] : idx;
                                }
                                bnd.push_back (c);
                            }
                        }
                        if (interior) { if (runStart < 0) runStart = idx; }
                        else if (runStart >= 0) { runs.push_back ({ runStart, idx - runStart }); runStart = -1; }
                    }
                    if (runStart >= 0) { runs.push_back ({ runStart, index (nx - 1, j, k) - runStart }); }
                }
        }
        double x0Face (int i, int f) const { return f == 0 ? (i - 1) * dx : (f == 1 ? i * dx : cx (i)); }
        double y0Face (int j, int f) const { return f == 2 ? (j - 1) * dx : (f == 3 ? j * dx : cx (j)); }
    };

    bool sameGeometry (const Params& a, const Params& b)
    {
        for (int d = 0; d < NUM_DOORS; ++d) if (std::abs (a.door[d] - b.door[d]) > 0.05f) return false;
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            if (a.material[r] != b.material[r] || a.floorMat[r] != b.floorMat[r] || a.ceilMat[r] != b.ceilMat[r]) return false;
            if (std::abs (a.panelArea[r] - b.panelArea[r]) > 0.1f) return false;
            for (int k = 0; k < 2; ++k)
                if (std::abs (a.breakAlong[r][k] - b.breakAlong[r][k]) > 0.02f || std::abs (a.breakPush[r][k] - b.breakPush[r][k]) > 0.03f) return false;
        }
        if (a.nfurn != b.nfurn) return false;
        for (int i = 0; i < std::min (a.nfurn, MAX_FURN); ++i)
        {
            const FurnItem &f = a.furn[i], &g = b.furn[i];
            if (f.type != g.type || std::abs (f.x - g.x) > 0.05f || std::abs (f.y - g.y) > 0.05f || std::abs (f.yaw - g.yaw) > 5.0f) return false;
        }
        return true;
    }

    // trilinear weights of a point over the 8 cells round it, air cells only, renormalised
    int trilinear (const Grid& G, const Vec3& p, int* idx, float* w)
    {
        const double u = p.x / G.dx + 0.5, v = p.y / G.dx + 0.5, q = p.z / G.dx + 0.5;
        const int i0 = (int) std::floor (u), j0 = (int) std::floor (v), k0 = (int) std::floor (q);
        const float fu = (float) (u - i0), fv = (float) (v - j0), fq = (float) (q - k0);
        int n = 0; float sum = 0;
        for (int c = 0; c < 8; ++c)
        {
            const int i = i0 + (c & 1), j = j0 + ((c >> 1) & 1), k = k0 + ((c >> 2) & 1);
            if (i < 1 || j < 1 || k < 1 || i >= G.nx - 1 || j >= G.ny - 1 || k >= G.nz - 1) continue;
            const int id = G.index (i, j, k);
            if (G.room[(size_t) id] < 0) continue;
            const float ww = ((c & 1) ? fu : 1 - fu) * (((c >> 1) & 1) ? fv : 1 - fv) * (((c >> 2) & 1) ? fq : 1 - fq);
            if (ww <= 0) continue;
            idx[n] = id; w[n] = ww; sum += ww; ++n;
        }
        if (sum > 1e-6f) for (int i = 0; i < n; ++i) w[i] /= sum;
        return n;
    }
}

bool fdtdLowBand (const std::vector<Params>& blocks, int pblock, const std::vector<float>* mono, int n, double fs,
                  std::vector<float>& L, std::vector<float>& R, FdtdStats* stats,
                  std::atomic<float>* progress, float p0, float p1, const std::atomic<bool>* cancel)
{
    const auto tStart = std::chrono::steady_clock::now();
    L.assign ((size_t) std::max (1, n), 0.0f);
    R.assign ((size_t) std::max (1, n), 0.0f);
    if (n <= 0 || blocks.empty()) return true;

    // one grid update per k host samples, at the Courant limit of the leapfrog scheme
    /*  10 cm cells: every wall, door jamb and ceiling of the apartment is a
        multiple of that, so each one falls exactly on a cell face and a room
        is exactly as long in the grid as it is in the drawing (its modes land
        where c / 2L puts them). The grid then runs at c sqrt 3 / dx = 5941 Hz,
        which no host rate divides, so the resampling is fractional. */
    const double dx = 0.1;
    const double fsf = SPEED_OF_SOUND * std::sqrt (3.0) / dx;
    const double r = fs / fsf;                   // host samples per grid step
    const float lam = (float) (1.0 / std::sqrt (3.0));
    Grid G; G.size (dx);
    G.build (blocks[0]);
    int rebuilds = 0;
    const Params* built = &blocks[0];

    // the sources, band-limited below ~700 Hz and brought to the grid's rate
    const int D = std::max (16, (int) std::ceil (40.0 * r));
    const Kernel h (fs, 700.0, D);
    const int M = (int) (n / r) + 2;
    std::vector<float> xs[MAX_SOURCES];
    for (int s = 0; s < MAX_SOURCES; ++s)
    {
        if (mono[s].empty()) continue;
        xs[s].assign ((size_t) M, 0.0f);
        const float* x = mono[s].data();
        for (int m = 0; m < M; ++m)
        {
            const double tc = m * r;
            const int c = (int) std::floor (tc);
            double acc = 0;
            const int j0 = std::max (-D, -c), j1 = std::min (D, n - 1 - c);
            for (int j = j0; j <= j1; ++j) acc += (double) h.at ((double) (c + j) - tc) * x[c + j];
            xs[s][(size_t) m] = (float) acc;
        }
        highPass2 (xs[s].data(), M, fsf, 10.0);
    }
    // in free space the pressure at r is (injected) * 3 dx / (4 pi r): scale so that it is x / r
    const float inj = (float) (4.0 * PI_D / (3.0 * dx));

    const size_t N = (size_t) G.nx * G.ny * G.nz;
    std::vector<float> A (N, 0.0f), B (N, 0.0f);
    float* cur = A.data();
    float* prev = B.data();
    std::vector<float> outL ((size_t) M, 0.0f), outR ((size_t) M, 0.0f);

    // work split
    const int nth = std::max (1, std::min (16, (int) std::thread::hardware_concurrency() - 1));
    std::vector<int> runSplit, bndSplit;
    auto split = [&]()
    {
        runSplit.assign ((size_t) nth + 1, 0); bndSplit.assign ((size_t) nth + 1, 0);
        long long total = 0; for (const Run& r : G.runs) total += r.len;
        long long acc = 0; int t = 1;
        for (int i = 0; i < (int) G.runs.size(); ++i)
        {
            acc += G.runs[(size_t) i].len;
            while (t < nth && acc >= total * t / nth) runSplit[(size_t) t++] = i + 1;
        }
        while (t <= nth) runSplit[(size_t) t++] = (int) G.runs.size();
        for (int i = 0; i <= nth; ++i) bndSplit[(size_t) i] = (int) ((long long) G.bnd.size() * i / nth);
    };
    split();

    Barrier bar; bar.n = nth;
    std::atomic<bool> quit { false };
    int step = 0;
    const float third = 1.0f / 3.0f;
    const int sy = G.sy, sz = G.sz;

    auto update = [&] (int t)
    {
        float* c = cur; float* p = prev;
        for (int ri = runSplit[(size_t) t]; ri < runSplit[(size_t) t + 1]; ++ri)
        {
            const Run& r = G.runs[(size_t) ri];
            float* __restrict po = p + r.start;
            const float* cc = c + r.start;
            for (int i = 0; i < r.len; ++i)
                po[i] = third * (cc[i - 1] + cc[i + 1] + cc[i - sy] + cc[i + sy] + cc[i - sz] + cc[i + sz]) - po[i];
        }
        for (int bi = bndSplit[(size_t) t]; bi < bndSplit[(size_t) t + 1]; ++bi)
        {
            const BCell& b = G.bnd[(size_t) bi];
            const float pc = c[b.idx];
            const float S = c[b.nb[0]] + c[b.nb[1]] + c[b.nb[2]] + c[b.nb[3]] + c[b.nb[4]] + c[b.nb[5]];
            const float lb = lam * b.B;
            p[b.idx] = (2.0f * pc + third * (S - 6.0f * pc) - (1.0f - lb) * p[b.idx]) / (1.0f + lb);
        }
    };

    auto posAt = [&] (int m, int s, bool listener, float side) -> Vec3
    {
        const double t = (double) m * r / pblock;
        const int b0 = std::min ((int) t, (int) blocks.size() - 1), b1 = std::min (b0 + 1, (int) blocks.size() - 1);
        const float f = (float) std::min (1.0, t - b0);
        const Params &a = blocks[(size_t) b0], &b = blocks[(size_t) b1];
        if (listener)
        {
            Vec3 pa = clampIntoRooms (Vec3 (a.lisX, a.lisY, EAR_HEIGHT), 0.15f), pb = clampIntoRooms (Vec3 (b.lisX, b.lisY, EAR_HEIGHT), 0.15f);
            if (roomOf (pa.x, pa.y) != roomOf (pb.x, pb.y)) pb = pa;
            const Vec3 c = pa + (pb - pa) * f;
            const float yaw = a.lisYaw * 3.14159265f / 180.0f;
            const Vec3 right (std::sin (yaw), -std::cos (yaw), 0.0f);
            return c + right * (side * 0.5f * a.earSpan);
        }
        Vec3 pa = clampIntoRooms (Vec3 (a.src[s].x, a.src[s].y, a.src[s].z), 0.15f), pb = clampIntoRooms (Vec3 (b.src[s].x, b.src[s].y, b.src[s].z), 0.15f);
        if (roomOf (pa.x, pa.y) != roomOf (pb.x, pb.y)) pb = pa;
        return pa + (pb - pa) * f;
    };

    // between updates: the new field is complete; inject, listen, and move the walls if they moved
    auto serial = [&]() -> bool
    {
        std::swap (cur, prev);                        // what was written is now the present
        int id[8]; float w[8];
        for (int s = 0; s < MAX_SOURCES; ++s)
        {
            if (xs[s].empty()) continue;
            const float v = xs[s][(size_t) step] * inj;
            if (v == 0.0f) continue;
            const int c = trilinear (G, posAt (step, s, false, 0), id, w);
            for (int i = 0; i < c; ++i) cur[id[i]] += v * w[i];
        }
        for (int e = 0; e < 2; ++e)
        {
            const int c = trilinear (G, posAt (step, 0, true, e == 0 ? -1.0f : 1.0f), id, w);
            float acc = 0; for (int i = 0; i < c; ++i) acc += cur[id[i]] * w[i];
            (e == 0 ? outL : outR)[(size_t) step] = acc;
        }
        ++step;
        if (step >= M) return false;
        // a door swung, a wall moved, the furniture changed: rebuild the air
        const int b = std::min ((int) ((double) step * r / pblock), (int) blocks.size() - 1);
        if (b != (int) ((double) (step - 1) * r / pblock) && ! sameGeometry (*built, blocks[(size_t) b]))
        {
            built = &blocks[(size_t) b];
            G.build (*built);
            for (size_t i = 0; i < N; ++i) if (G.room[i] < 0) { cur[i] = 0; prev[i] = 0; }
            split();
            ++rebuilds;
        }
        if ((step & 255) == 0)
        {
            if (cancel != nullptr && cancel->load()) return false;
            if (progress != nullptr) progress->store (p0 + (p1 - p0) * (float) step / (float) M);
        }
        return true;
    };

    auto worker = [&] (int t)
    {
        for (;;)
        {
            update (t);
            bar.wait();
            if (t == 0 && ! serial()) quit = true;
            bar.wait();
            if (quit.load()) return;
        }
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < nth; ++t) pool.emplace_back (worker, t);
    worker (0);
    for (auto& th : pool) th.join();
    if (cancel != nullptr && cancel->load()) return false;

    // and at the ears, the same: nothing below the apartment's lowest mode can be real
    highPass2 (outL.data(), M, fsf, 10.0);
    highPass2 (outR.data(), M, fsf, 10.0);
    // back to the host rate: zero stuffing and the same low pass
    for (int m = 0; m < M; ++m)
    {
        const float a = outL[(size_t) m] * (float) r, b = outR[(size_t) m] * (float) r;
        if (a == 0.0f && b == 0.0f) continue;
        // the engine hears every path HALF samples late (its interpolation kernel): so does this
        const double tc = m * r + FracDelay::HALF;
        const int c = (int) std::floor (tc);
        const int j0 = std::max (-D, -c), j1 = std::min (D, n - 1 - c);
        for (int j = j0; j <= j1; ++j)
        {
            const float hv = h.at ((double) (c + j) - tc);
            L[(size_t) (c + j)] += a * hv;
            R[(size_t) (c + j)] += b * hv;
        }
    }
    if (stats != nullptr)
    {
        stats->dx = dx; stats->rate = fsf; stats->cells = G.airCells; stats->steps = step; stats->rebuilds = rebuilds;
        stats->seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - tStart).count();
    }
    return true;
}

} // namespace tw
