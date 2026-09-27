#include "LateRays.h"
#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace tw
{

namespace
{
    constexpr float PI_F = 3.14159265358979f;
    constexpr float SCATTER_FLOOR = 0.10f;   // real walls scatter this much; applied only past the engine's own orders
    constexpr float CBIN = 0.005f;           // the transmission stage's coarse bins, seconds

    struct Rng
    {
        uint64_t s;
        explicit Rng (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) { if (s == 0) s = 1; }
        inline uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
        inline float uni() { return (float) (next() >> 40) * (1.0f / 16777216.0f); }   // [0, 1)
    };

    inline Vec3 norm (const Vec3& v) { const float l = v.len(); return l > 1e-12f ? v * (1.0f / l) : Vec3 (0, 0, 1); }
    inline Vec3 cross (const Vec3& a, const Vec3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

    // a cosine-weighted direction about n (a Lambertian surface)
    Vec3 lambert (const Vec3& n, Rng& r)
    {
        const Vec3 t = norm (std::abs (n.x) > 0.5f ? cross (n, Vec3 (0, 1, 0)) : cross (n, Vec3 (1, 0, 0)));
        const Vec3 b = cross (n, t);
        const float u1 = r.uni(), u2 = r.uni();
        const float rr = std::sqrt (u1), ph = 2.0f * PI_F * u2;
        const float x = rr * std::cos (ph), y = rr * std::sin (ph), z = std::sqrt (std::max (0.0f, 1.0f - u1));
        return norm (t * x + b * y + n * z);
    }

    struct Box
    {
        float cx, cy, c, s, hw, hd, zb, zt;
        float alpha[NBAND];
        int room;
        bool pass;           // a curtain: sound goes through, losing alpha
    };
    struct Rug { float cx, cy, c, s, hw, hd; float alpha[NBAND]; int room; };

    /*  A panel that passes sound into the next room: a closed (part of a) door
        leaf, or the solid part of a party wall. Span along the plane, z up. */
    struct Panel
    {
        int ra = 0, rb = 0;
        int axis = 0; float pos = 0, s0 = 0, s1 = 0, h = 0;
        int nex = 0; float exS0[3] = {}, exS1[3] = {}, exH[3] = {};   // excluded (the doorways in a wall, the open strip of a leaf)
        float tau[NBAND] = {};
        bool contains (float span, float z) const
        {
            if (span < s0 || span > s1 || z < 0 || z > h) return false;
            for (int i = 0; i < nex; ++i) if (span >= exS0[i] && span <= exS1[i] && z <= exH[i]) return false;
            return true;
        }
    };

    enum { HIT_PLAIN = 0, HIT_OPEN, HIT_LEAF, HIT_PARTY };

    struct World
    {
        RoomGeom g[NUM_ROOMS];
        float alpha[NUM_ROOMS][6][NBAND];
        float scat[NUM_ROOMS][6][NBAND];
        float lo[NUM_DOORS], hi[NUM_DOORS], ap[NUM_DOORS];
        std::vector<Box> boxes;
        std::vector<Rug> rugs;
        Panel panel[NUM_DOORS + 3]; int npanel = 0;
        int leafPanel[NUM_DOORS] = { -1, -1, -1 };
        float airK[NBAND];          // energy decay per metre (natural log)

        void build (const Params& P)
        {
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                RoomBreaks br;
                br.w[0].along = P.breakAlong[r][0]; br.w[0].push = P.breakPush[r][0];
                br.w[1].along = P.breakAlong[r][1]; br.w[1].push = P.breakPush[r][1];
                g[r] = buildRoomGeom (r, br);
                RoomSurfaces surf; surf.wall = P.material[r]; surf.floor = P.floorMat[r]; surf.ceil = P.ceilMat[r];
                const float wallArea = std::max (1.0f, g[r].perimeter * g[r].height);
                const float share = std::min (1.0f, std::max (0.0f, P.panelArea[r]) / wallArea);
                for (int w = 0; w < 6; ++w)
                {
                    const int m = std::min (std::max (surf.of (w), 0), NUM_MATERIALS - 1);
                    for (int b = 0; b < NBAND; ++b)
                    {
                        float a = MATERIAL_ALPHA[m][b];
                        // acoustic art: its absorber smeared over the walls it hangs on
                        if (w < 4) a += share * std::max (0.0f, PANEL_ALPHA[b] - a);
                        alpha[r][w][b] = std::min (0.99f, a);
                        scat[r][w][b] = MATERIAL_SCATTER[m][b];
                    }
                }
            }
            for (int d = 0; d < NUM_DOORS; ++d)
            {
                ap[d] = std::min (1.0f, std::max (0.0f, P.door[d]));
                Engine::doorOpenStrip (d, ap[d], lo[d], hi[d]);
                if (ap[d] <= 0.02f) { lo[d] = hi[d] = -1e9f; }
            }
            for (int b = 0; b < NBAND; ++b) airK[b] = Engine::airDbPerMetre (b) * 0.2302585f;

            // the transmitting panels
            npanel = 0;
            for (int d = 0; d < NUM_DOORS; ++d)
            {
                const Door& D = DOORS[d];
                if (ap[d] >= 0.999f) continue;
                Panel& p = panel[npanel];
                p = Panel();
                p.ra = D.roomA; p.rb = D.roomB; p.axis = D.axis; p.pos = D.pos; p.s0 = D.s0; p.s1 = D.s1; p.h = D.height;
                if (ap[d] > 0.02f) { p.nex = 1; p.exS0[0] = lo[d]; p.exS1[0] = hi[d]; p.exH[0] = D.height + 1.0f; }
                for (int b = 0; b < NBAND; ++b) p.tau[b] = Engine::leafTransmission (b);
                leafPanel[d] = npanel++;
            }
            for (int ra = 0; ra < NUM_ROOMS; ++ra)
                for (int rb = ra + 1; rb < NUM_ROOMS; ++rb)
                {
                    int axis; float pos, s0, s1, h;
                    if (! Engine::partyWall (ra, rb, axis, pos, s0, s1, h)) continue;
                    Panel& p = panel[npanel];
                    p = Panel();
                    p.ra = ra; p.rb = rb; p.axis = axis; p.pos = pos; p.s0 = s0; p.s1 = s1; p.h = h;
                    for (int d = 0; d < NUM_DOORS; ++d)
                    {
                        const Door& D = DOORS[d];
                        if (! ((D.roomA == ra && D.roomB == rb) || (D.roomA == rb && D.roomB == ra))) continue;
                        if (p.nex < 3) { p.exS0[p.nex] = D.s0; p.exS1[p.nex] = D.s1; p.exH[p.nex] = D.height; ++p.nex; }
                    }
                    for (int b = 0; b < NBAND; ++b) p.tau[b] = Engine::wallTransmission (b);
                    ++npanel;
                }

            // the furniture
            boxes.clear(); rugs.clear();
            const int nf = std::min (std::max (P.nfurn, 0), MAX_FURN);
            for (int i = 0; i < nf; ++i)
            {
                const FurnItem& it = P.furn[i];
                if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
                const FurnSpec& F = FURN[it.type];
                const float a = it.yaw * PI_F / 180.0f;
                const int room = roomOf (it.x, it.y);
                if (room < 0) continue;
                if (F.coversFloor)
                {
                    Rug r; r.cx = it.x; r.cy = it.y; r.c = std::cos (a); r.s = std::sin (a); r.hw = 0.5f * F.w; r.hd = 0.5f * F.d; r.room = room;
                    for (int b = 0; b < NBAND; ++b) r.alpha[b] = std::min (0.95f, F.absorb[b] / std::max (0.1f, F.w * F.d));
                    rugs.push_back (r);
                    continue;
                }
                Box x; x.cx = it.x; x.cy = it.y; x.c = std::cos (a); x.s = std::sin (a); x.hw = 0.5f * F.w; x.hd = 0.5f * F.d;
                x.zb = F.zb; x.zt = std::max (F.zb + 0.02f, F.zt); x.room = room; x.pass = ! F.occludes;
                const float hgt = x.zt - x.zb;
                // equivalent absorption area over what a ray can hit: a curtain is met from both
                // sides and passes what it does not absorb, a box is its top, its sides and a raised bottom
                const float area = x.pass ? 2.0f * F.w * hgt
                                          : F.w * F.d * (x.zb > 0.05f ? 2.0f : 1.0f) + 2.0f * (F.w + F.d) * hgt;
                for (int b = 0; b < NBAND; ++b) x.alpha[b] = std::min (0.95f, F.absorb[b] / std::max (0.05f, area));
                boxes.push_back (x);
            }
        }

        // what a point on wall w of room r is
        int classify (int r, int w, const Vec3& X, int& door, int& pnl) const
        {
            door = -1; pnl = -1;
            if (w > 3) return HIT_PLAIN;
            for (int d = 0; d < NUM_DOORS; ++d)
            {
                if (Engine::doorWallOf (r, d) != w) continue;
                const Door& D = DOORS[d];
                const float span = D.axis == 0 ? X.y : X.x;
                const float plane = D.axis == 0 ? X.x : X.y;
                if (std::abs (plane - D.pos) > 0.01f) continue;
                if (span < D.s0 || span > D.s1 || X.z > D.height) continue;
                door = d;
                if (span >= lo[d] && span <= hi[d]) return HIT_OPEN;
                pnl = leafPanel[d];
                return pnl >= 0 ? HIT_LEAF : HIT_PLAIN;
            }
            for (int i = 0; i < npanel; ++i)
            {
                const Panel& p = panel[i];
                if (p.ra != r && p.rb != r) continue;
                bool isLeaf = false; for (int d = 0; d < NUM_DOORS; ++d) if (leafPanel[d] == i) isLeaf = true;
                if (isLeaf) continue;
                const float plane = p.axis == 0 ? X.x : X.y;
                const float span = p.axis == 0 ? X.y : X.x;
                if (std::abs (plane - p.pos) > 0.01f) continue;
                if (p.contains (span, X.z)) { pnl = i; return HIT_PARTY; }
            }
            return HIT_PLAIN;
        }
    };

    // ray against an oriented box: entry and exit distances, and the entry face normal
    bool hitBox (const Box& b, const Vec3& P, const Vec3& D, float& tIn, float& tOut, Vec3& nIn)
    {
        const float dx = P.x - b.cx, dy = P.y - b.cy;
        const float o[3] = { dx * b.c + dy * b.s, -dx * b.s + dy * b.c, P.z };
        const float v[3] = { D.x * b.c + D.y * b.s, -D.x * b.s + D.y * b.c, D.z };
        const float lo[3] = { -b.hw, -b.hd, b.zb }, hi[3] = { b.hw, b.hd, b.zt };
        float t0 = -1e30f, t1 = 1e30f; int ax = -1; float sgn = 0;
        for (int k = 0; k < 3; ++k)
        {
            if (std::abs (v[k]) < 1e-9f) { if (o[k] < lo[k] || o[k] > hi[k]) return false; continue; }
            float ta = (lo[k] - o[k]) / v[k], tb = (hi[k] - o[k]) / v[k];
            float s = -1.0f;                 // entering through the low face
            if (ta > tb) { std::swap (ta, tb); s = 1.0f; }
            if (ta > t0) { t0 = ta; ax = k; sgn = s; }
            t1 = std::min (t1, tb);
            if (t0 > t1) return false;
        }
        if (t1 < 1e-5f || ax < 0) return false;
        tIn = t0; tOut = t1;
        float nl[3] = { 0, 0, 0 }; nl[ax] = sgn;
        nIn = Vec3 (nl[0] * b.c - nl[1] * b.s, nl[0] * b.s + nl[1] * b.c, nl[2]);
        return true;
    }

    bool onRug (const Rug& r, const Vec3& X)
    {
        const float dx = X.x - r.cx, dy = X.y - r.cy;
        const float lx = dx * r.c + dy * r.s, ly = -dx * r.s + dy * r.c;
        return std::abs (lx) <= r.hw && std::abs (ly) <= r.hd;
    }

    //--------------------------------------------------------------------------
    // direction bins, head-relative: (front, right, up) unit vectors
    struct DirBins
    {
        Vec3 c[LateEnergy::NDIR];
        float az[LateEnergy::NDIR], el[LateEnergy::NDIR];
        DirBins()
        {
            int k = 0;
            for (int e = 0; e < 2; ++e)
                for (int a = 0; a < 8; ++a)
                {
                    az[k] = 45.0f * a; el[k] = e == 0 ? -20.0f : 25.0f;
                    ++k;
                }
            az[k] = 0; el[k] = 90.0f; ++k;
            az[k] = 0; el[k] = -90.0f; ++k;
            for (int i = 0; i < LateEnergy::NDIR; ++i)
            {
                const float A = az[i] * PI_F / 180.0f, E = el[i] * PI_F / 180.0f;
                c[i] = Vec3 (std::cos (E) * std::cos (A), std::cos (E) * std::sin (A), std::sin (E));
            }
        }
        int nearest (const Vec3& v) const
        {
            int best = 0; float bd = -2;
            for (int i = 0; i < LateEnergy::NDIR; ++i) { const float d = c[i].dot (v); if (d > bd) { bd = d; best = i; } }
            return best;
        }
    };
    const DirBins& dirBins() { static const DirBins d; return d; }

    //--------------------------------------------------------------------------
    struct Receiver
    {
        Vec3 L; int room = 0; float R = 0.3f, R2 = 0.09f, invV = 1;
        bool visible[NUM_ROOMS] = { false, false, false };
        float cy = 1, sy = 0;         // listener yaw
    };

    struct Tracer
    {
        const World& W;
        const Receiver& Rx;
        float Tmax;                    // metres of travel
        int order;
        int srcRoom;
        // stage one: the fine histogram; stage two: coarse bins of its own
        LateEnergy* fine = nullptr;
        std::vector<float>* coarse = nullptr;       // [(b * NDIR + d) * ncb + t]
        int ncb = 0;
        // transmission: incident energy per panel side per band per coarse bin
        std::vector<float>* incident = nullptr;     // [((panel * 2 + side) * NBAND + b) * ncb + t]
        long long crossings = 0, excluded = 0;

        Tracer (const World& w, const Receiver& r, float tmax, int ord, int rs) : W (w), Rx (r), Tmax (tmax), order (ord), srcRoom (rs) {}

        void deposit (const float* e, const Vec3& P, const Vec3& D, float dist, float tseg, bool engineHas)
        {
            const Vec3 v = Rx.L - P;
            const float tc = v.dot (D);
            const float d2 = v.dot (v) - tc * tc;
            if (d2 >= Rx.R2) return;
            const float half = std::sqrt (Rx.R2 - d2);
            const float t0 = std::max (0.0f, tc - half), t1 = std::min (tseg, tc + half);
            if (t1 <= t0) return;
            ++crossings;
            if (engineHas) { ++excluded; return; }
            const float chord = t1 - t0, tm = 0.5f * (t0 + t1);
            // where the sound comes FROM, head-relative
            const Vec3 a = D * -1.0f;
            const Vec3 hr (a.x * Rx.cy + a.y * Rx.sy, a.x * Rx.sy - a.y * Rx.cy, a.z);
            const int dir = dirBins().nearest (hr);
            const float tsec = (dist + tm) / SPEED_OF_SOUND;
            const float w = chord * Rx.invV;
            if (fine != nullptr)
            {
                const int bin = (int) (tsec / fine->binSec);
                if (bin < 0 || bin >= fine->nbins) return;
                for (int b = 0; b < NBAND; ++b) fine->at (b, dir, bin) += e[b] * std::exp (-W.airK[b] * tm) * w;
            }
            else if (coarse != nullptr)
            {
                const int bin = (int) (tsec / CBIN);
                if (bin < 0 || bin >= ncb) return;
                for (int b = 0; b < NBAND; ++b) (*coarse)[((size_t) (b * LateEnergy::NDIR + dir)) * (size_t) ncb + (size_t) bin] += e[b] * std::exp (-W.airK[b] * tm) * w;
            }
        }

        /*  Follow one ray. spec: it is still a pure specular chain from the
            source, which is what the engine's image paths are made of. */
        void trace (Vec3 P, Vec3 D, int room, float* e, float e0, Rng& rng, bool firstStage)
        {
            bool spec = firstStage;
            int bSrc = 0, bOther = 0, doors = 0, bounces = 0;
            float dist = 0;
            const float floorE = e0 * 1.0e-7f;
            for (int guard = 0; guard < 20000; ++guard)
            {
                const RoomGeom& g = W.g[room];
                // the nearest surface ahead
                float tw = 1e30f; int si = -1;
                for (int i = 0; i < g.n; ++i)
                {
                    const Surface& s = g.s[i];
                    const float dn = D.dot (s.n);
                    if (dn > -1e-7f) continue;                    // moving away from it (or along it)
                    const float t = (s.p - P).dot (s.n) / dn;
                    if (t < 1e-5f || t >= tw) continue;
                    const Vec3 X = P + D * t;
                    if (! onSurface (g, s, X)) continue;
                    tw = t; si = i;
                }
                // furniture of this room
                float tb = 1e30f, tbOut = 0; int bi = -1; Vec3 bn;
                for (int i = 0; i < (int) W.boxes.size(); ++i)
                {
                    const Box& b = W.boxes[(size_t) i];
                    if (b.room != room) continue;
                    float t0, t1; Vec3 n;
                    if (! hitBox (b, P, D, t0, t1, n)) continue;
                    if (t0 < 1e-5f) { if (! b.pass) continue; t0 = 1e-5f; }   // starting inside: only a curtain lets go
                    if (t0 < tb) { tb = t0; tbOut = t1; bi = i; bn = n; }
                }
                if (si < 0 && bi < 0) return;                     // escaped (numerically): let it go
                const float tseg = std::min (tw, tb);
                if (tseg > 1e29f) return;

                // the receiver, on this leg
                if (Rx.visible[room])
                {
                    bool engineHas = false;
                    if (spec)
                    {
                        const int total = bSrc + bOther;
                        if (srcRoom == Rx.room) engineHas = doors == 0 && total <= order;
                        else if (doors == 1) engineHas = (bOther == 0 && bSrc <= 2) || (bSrc == 0 && bOther == 1);
                        else if (doors == 2) engineHas = total == 0;
                    }
                    deposit (e, P, D, dist, tseg, engineHas);
                }

                // the air, over the leg
                for (int b = 0; b < NBAND; ++b) e[b] *= std::exp (-W.airK[b] * tseg);
                dist += tseg;
                if (dist > Tmax) return;
                float emax = 0; for (int b = 0; b < NBAND; ++b) emax = std::max (emax, e[b]);
                if (emax < floorE) return;

                if (bi >= 0 && tb <= tw)
                {
                    const Box& bx = W.boxes[(size_t) bi];
                    if (bx.pass)
                    {
                        // through the curtain, losing what it absorbs
                        for (int b = 0; b < NBAND; ++b) e[b] *= 1.0f - bx.alpha[b];
                        const float tx = std::min (tbOut, tw) + 1e-4f - tb;
                        P = P + D * (tb + std::max (0.0f, tx));
                        dist += std::max (0.0f, tx);
                        continue;
                    }
                    P = P + D * tb;
                    for (int b = 0; b < NBAND; ++b) e[b] *= 1.0f - bx.alpha[b];
                    ++bounces; (room == srcRoom ? bSrc : bOther) += 1;
                    spec = false;
                    D = lambert (bn, rng);
                    P = P + bn * 1e-4f;
                    continue;
                }

                // a room surface
                const Surface& s = g.s[si];
                P = P + D * tw;
                int door = -1, pnl = -1;
                const int kind = s.vertical ? W.classify (room, s.wallId, P, door, pnl) : HIT_PLAIN;
                if (kind == HIT_OPEN)
                {
                    const Door& Dd = DOORS[door];
                    room = Dd.roomA == room ? Dd.roomB : Dd.roomA;
                    ++doors;
                    continue;
                }
                // what it is made of
                float alpha[NBAND], sc[NBAND];
                for (int b = 0; b < NBAND; ++b) { alpha[b] = W.alpha[room][s.wallId][b]; sc[b] = W.scat[room][s.wallId][b]; }
                if (s.wallId == 4)
                    for (const Rug& r : W.rugs)
                        if (r.room == room && onRug (r, P)) { for (int b = 0; b < NBAND; ++b) alpha[b] = std::max (alpha[b], r.alpha[b]); break; }
                if (kind == HIT_LEAF || kind == HIT_PARTY)
                {
                    const Panel& pn = W.panel[pnl];
                    // the direct sound onto a panel is the engine's own transmission path
                    if (incident != nullptr && (bounces > 0 || doors > 0))
                    {
                        const int side = room == pn.ra ? 0 : 1;
                        const int bin = (int) (dist / SPEED_OF_SOUND / CBIN);
                        if (bin >= 0 && bin < ncb)
                            for (int b = 0; b < NBAND; ++b)
                                (*incident)[(((size_t) pnl * 2 + (size_t) side) * NBAND + (size_t) b) * (size_t) ncb + (size_t) bin] += e[b];
                    }
                    for (int b = 0; b < NBAND; ++b)
                        alpha[b] = kind == HIT_LEAF ? pn.tau[b] : std::min (0.99f, alpha[b] + pn.tau[b]);
                }
                ++bounces; (room == srcRoom ? bSrc : bOther) += 1;
                // beyond the orders the engine traced, a real wall's own roughness
                if (bounces > order) for (int b = 0; b < NBAND; ++b) sc[b] = std::max (sc[b], SCATTER_FLOOR);
                float p = 0; for (int b = 0; b < NBAND; ++b) p += sc[b];
                p = std::min (0.98f, p / NBAND);
                for (int b = 0; b < NBAND; ++b) e[b] *= 1.0f - alpha[b];
                if (p > 1e-6f && rng.uni() < p)
                {
                    for (int b = 0; b < NBAND; ++b) e[b] *= sc[b] / p;
                    D = lambert (s.n, rng);
                    spec = false;
                }
                else
                {
                    if (p > 1e-6f) for (int b = 0; b < NBAND; ++b) e[b] *= (1.0f - sc[b]) / (1.0f - p);
                    D = D - s.n * (2.0f * D.dot (s.n));
                }
            }
        }
    };

    void setupReceiver (const Params& P, Receiver& R, const World& W)
    {
        R.L = clampIntoRooms (Vec3 (P.lisX, P.lisY, EAR_HEIGHT), 0.15f);
        R.room = std::max (0, roomOf (R.L.x, R.L.y));
        const float yaw = P.lisYaw * PI_F / 180.0f;
        R.cy = std::cos (yaw); R.sy = std::sin (yaw);
        R.R = std::min (0.6f, std::max (0.18f, 0.07f * std::cbrt (W.g[R.room].volume())));
        R.R2 = R.R * R.R;
        for (int r = 0; r < NUM_ROOMS; ++r) R.visible[r] = r == R.room;
        // a listener in a doorway hears the room beyond it through the opening
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            const Door& D = DOORS[d];
            if (D.roomA != R.room && D.roomB != R.room) continue;
            const float plane = D.axis == 0 ? R.L.x : R.L.y, span = D.axis == 0 ? R.L.y : R.L.x;
            if (std::abs (plane - D.pos) < R.R && span >= W.lo[d] && span <= W.hi[d])
                R.visible[D.roomA == R.room ? D.roomB : D.roomA] = true;
        }
        // the sphere's volume that lies in the rooms that can see it
        Rng rng (77);
        int in = 0, tot = 0;
        for (int i = 0; i < 20000; ++i)
        {
            const Vec3 q ((rng.uni() * 2 - 1) * R.R, (rng.uni() * 2 - 1) * R.R, (rng.uni() * 2 - 1) * R.R);
            if (q.dot (q) > R.R2) continue;
            ++tot;
            const Vec3 x = R.L + q;
            const int r = roomOf (x.x, x.y);
            if (r >= 0 && R.visible[r] && x.z > 0 && x.z < W.g[r].height && insidePolygon (W.g[r], x.x, x.y)) ++in;
        }
        const float frac = tot > 0 ? std::max (0.2f, (float) in / (float) tot) : 1.0f;
        R.invV = 1.0f / (frac * 4.0f / 3.0f * PI_F * R.R * R.R * R.R);
    }
}

//==============================================================================
float lateSeconds (const Params& P)
{
    float worst = 0.3f;
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        RoomSurfaces s; s.wall = P.material[r]; s.floor = P.floorMat[r]; s.ceil = P.ceilMat[r];
        float rt[NBAND];
        Engine::eyringRt60 (r, s, P.door, P.furn, P.nfurn, rt);
        for (int b = 1; b < NBAND - 1; ++b) worst = std::max (worst, rt[b]);
    }
    return std::min (8.0f, 1.1f * worst + 0.15f);
}

void lateDirection (int d, float& azDeg, float& elDeg)
{
    azDeg = dirBins().az[d]; elDeg = dirBins().el[d];
}

void traceLate (const Params& P, int s, const LateRayOptions& o, LateEnergy& out, LateEnergy* transmitted)
{
    World W; W.build (P);
    Receiver Rx; setupReceiver (P, Rx, W);
    const float Tsec = o.maxSeconds > 0 ? o.maxSeconds : lateSeconds (P);
    out.binSec = 0.001f;
    out.nbins = (int) std::ceil (Tsec / out.binSec) + 1;
    out.e.assign ((size_t) NBAND * LateEnergy::NDIR * (size_t) out.nbins, 0.0f);
    out.receiverRadius = Rx.R; out.receiverVolume = 1.0f / Rx.invV;
    out.crossings = out.excluded = out.transmitted = 0;
    if (transmitted != nullptr) { transmitted->binSec = out.binSec; transmitted->nbins = out.nbins; transmitted->e.assign (out.e.size(), 0.0f); }
    LateEnergy& via = transmitted != nullptr ? *transmitted : out;
    if (s < 0 || s >= MAX_SOURCES) return;

    const SourceParams& sp = P.src[s];
    const Vec3 S = clampIntoRooms (Vec3 (sp.x, sp.y, sp.z), 0.15f);
    const int rs = std::max (0, roomOf (S.x, S.y));
    const float yaw = sp.yaw * PI_F / 180.0f;
    const Vec3 axis (std::cos (yaw), std::sin (yaw), 0.0f);

    const int ncb = (int) std::ceil (Tsec / CBIN) + 1;
    std::vector<float> incident ((size_t) W.npanel * 2 * NBAND * (size_t) ncb, 0.0f);

    Tracer T (W, Rx, Tsec * SPEED_OF_SOUND, o.imageOrder, rs);
    T.fine = &out;
    T.incident = &incident; T.ncb = ncb;
    Rng rng (o.seed * 7919u + (uint32_t) s * 104729u + 1u);
    const int N = std::max (100, o.rays);
    const float e0 = 4.0f * PI_F / (float) N;
    const float golden = PI_F * (3.0f - std::sqrt (5.0f));
    const float spin = rng.uni() * 2.0f * PI_F;
    float e[NBAND];
    for (int i = 0; i < N; ++i)
    {
        // stratified over the sphere (a Fibonacci lattice, jittered)
        const float z = 1.0f - 2.0f * ((float) i + rng.uni()) / (float) N;
        const float rr = std::sqrt (std::max (0.0f, 1.0f - z * z));
        const float ph = golden * (float) i + spin;
        const Vec3 D (rr * std::cos (ph), rr * std::sin (ph), z);
        const float cosT = D.dot (axis);
        for (int b = 0; b < NBAND; ++b) e[b] = e0 * std::pow (10.0f, Engine::directivityDb (sp, cosT, b) * 0.1f);
        T.trace (S, D, rs, e, e0, rng, true);
    }
    out.crossings = T.crossings; out.excluded = T.excluded;

    // ---- stage two: what the panels pass on, re-radiated into the next room
    std::vector<float> resp ((size_t) NBAND * LateEnergy::NDIR * (size_t) ncb);
    for (int pi = 0; pi < W.npanel; ++pi)
        for (int side = 0; side < 2; ++side)
        {
            const Panel& pn = W.panel[pi];
            const size_t base = ((size_t) pi * 2 + (size_t) side) * NBAND * (size_t) ncb;
            double through = 0;
            for (int b = 0; b < NBAND; ++b)
                for (int t = 0; t < ncb; ++t) through = std::max (through, (double) incident[base + (size_t) b * ncb + (size_t) t] * pn.tau[b]);
            if (through < 1e-9) continue;
            const int from = side == 0 ? pn.ra : pn.rb, to = side == 0 ? pn.rb : pn.ra;
            if (! Rx.visible[to] && W.ap[0] + W.ap[1] + W.ap[2] < 0.05f) continue;   // nowhere for it to reach the listener
            const Room& RT = ROOMS[to];
            const float dirSign = pn.axis == 0 ? (RT.x0 >= pn.pos - 1e-3f ? 1.0f : -1.0f) : (RT.y0 >= pn.pos - 1e-3f ? 1.0f : -1.0f);
            const Vec3 n = pn.axis == 0 ? Vec3 (dirSign, 0, 0) : Vec3 (0, dirSign, 0);
            (void) from;

            std::fill (resp.begin(), resp.end(), 0.0f);
            Tracer T2 (W, Rx, Tsec * SPEED_OF_SOUND, o.imageOrder, to);
            T2.coarse = &resp; T2.ncb = ncb;
            const int M = std::max (50, o.panelRays);
            const float em = 1.0f / (float) M;
            for (int i = 0; i < M; ++i)
            {
                float span = 0, z = 0;
                for (int tries = 0; tries < 64; ++tries)
                {
                    span = pn.s0 + rng.uni() * (pn.s1 - pn.s0);
                    z = rng.uni() * std::min (pn.h, RT.h);
                    if (pn.contains (span, z)) break;
                }
                const Vec3 X = pn.axis == 0 ? Vec3 (pn.pos, span, z) : Vec3 (span, pn.pos, z);
                for (int b = 0; b < NBAND; ++b) e[b] = em;
                T2.trace (X + n * 1e-3f, lambert (n, rng), to, e, em, rng, false);
            }
            // the room beyond answers every moment of what arrived
            for (int b = 0; b < NBAND; ++b)
            {
                const float* inc = &incident[base + (size_t) b * ncb];
                for (int d = 0; d < LateEnergy::NDIR; ++d)
                {
                    const float* R = &resp[((size_t) (b * LateEnergy::NDIR + d)) * (size_t) ncb];
                    int last = ncb - 1; while (last >= 0 && R[last] <= 0) --last;
                    if (last < 0) continue;
                    for (int t0 = 0; t0 < ncb; ++t0)
                    {
                        const float a = inc[t0] * pn.tau[b];
                        if (a <= 0) continue;
                        const int lim = std::min (last, ncb - 1 - t0);
                        for (int t = 0; t <= lim; ++t)
                        {
                            if (R[t] <= 0) continue;
                            const float v = a * R[t];
                            const int f0 = (int) std::lround ((double) (t0 + t) * CBIN / out.binSec);
                            const int nf = (int) std::lround (CBIN / out.binSec);
                            for (int k = 0; k < nf && f0 + k < via.nbins; ++k) via.at (b, d, f0 + k) += v / (float) nf;
                        }
                    }
                }
            }
            out.transmitted += T2.crossings;
        }
}

//==============================================================================
namespace fftc
{
    int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

    void fft (std::complex<float>* a, int n, bool inverse)
    {
        thread_local std::vector<std::complex<float>> tw;
        thread_local int twN = 0;
        if (twN != n)
        {
            tw.resize ((size_t) n / 2);
            for (int k = 0; k < n / 2; ++k)
            {
                const double ang = -2.0 * 3.141592653589793 * (double) k / (double) n;
                tw[(size_t) k] = std::complex<float> ((float) std::cos (ang), (float) std::sin (ang));
            }
            twN = n;
        }
        // bit reversal
        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    std::complex<float> w = tw[(size_t) (k * step)];
                    if (inverse) w = std::conj (w);
                    const std::complex<float> u = a[i + k], v = a[i + k + half] * w;
                    a[i + k] = u + v; a[i + k + half] = u - v;
                }
        }
        if (inverse) { const float s = 1.0f / (float) n; for (int i = 0; i < n; ++i) a[i] *= s; }
    }
}

//==============================================================================
namespace
{
    // RBJ biquad, direct form I
    struct Bq
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void set (bool high, float fc, float Q, float fs)
        {
            const float w = 2.0f * PI_F * fc / fs, cw = std::cos (w), al = std::sin (w) / (2.0f * Q);
            const float a0 = 1.0f + al;
            if (high) { b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); b2 = b0; }
            else      { b0 = (1.0f - cw) * 0.5f; b1 = 1.0f - cw;    b2 = b0; }
            b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2.0f * cw / a0; a2 = (1.0f - al) / a0;
        }
        inline float run (float x) { const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
        // |H|^2 at f
        float power (float f, float fs) const
        {
            const std::complex<double> z = std::polar (1.0, -2.0 * 3.141592653589793 * f / fs);
            const auto num = (double) b0 + (double) b1 * z + (double) b2 * z * z;
            const auto den = 1.0 + (double) a1 * z + (double) a2 * z * z;
            return (float) std::norm (num / den);
        }
    };

    /*  One band's filter: a 4th-order Butterworth high pass at its lower edge
        and low pass at its upper edge. Adjacent edges are power complementary,
        so bands of equal energy sum to white. */
    struct BandPath
    {
        Bq f[4]; int n = 0;
        void make (int b, float fs)
        {
            static const float EDGE[NBAND - 1] = { 176.8f, 353.6f, 707.1f, 1414.2f, 2828.4f, 5656.9f };
            const float Q1 = 0.5411961f, Q2 = 1.3065630f;
            n = 0;
            if (b > 0 && EDGE[b - 1] < 0.45f * fs) { f[n++].set (true, EDGE[b - 1], Q1, fs); f[n++].set (true, EDGE[b - 1], Q2, fs); }
            /*  The lowest band stops at 25 Hz rather than running to DC: a ray means
                nothing that far down, and spread flat to 0 Hz its tail rang at 20 Hz
                and at DC as if they were 125 Hz - measured as a DC offset in a bounce. */
            if (b == 0) { f[n++].set (true, 25.0f, Q1, fs); f[n++].set (true, 25.0f, Q2, fs); }
            if (b < NBAND - 1 && EDGE[b] < 0.45f * fs) { f[n++].set (false, EDGE[b], Q1, fs); f[n++].set (false, EDGE[b], Q2, fs); }
        }
        inline float run (float x) { for (int i = 0; i < n; ++i) x = f[i].run (x); return x; }
        // the share of white noise's power this band passes
        float fraction (float fs) const
        {
            double acc = 0; const int K = 4096;
            for (int k = 0; k < K; ++k)
            {
                const float fr = (k + 0.5f) * 0.5f * fs / K;
                double p = 1; for (int i = 0; i < n; ++i) p *= f[i].power (fr, fs);
                acc += p;
            }
            return (float) std::max (1e-6, acc / K);
        }
    };
}

void makeHeadSpectra (const Hrtf& head, float earSpan, int fftN, std::vector<std::complex<float>>& out)
{
    const int nt = head.numTaps();
    std::vector<float> l ((size_t) nt), r ((size_t) nt);
    out.assign ((size_t) LateEnergy::NDIR * (size_t) fftN, std::complex<float> (0, 0));
    for (int d = 0; d < LateEnergy::NDIR; ++d)
    {
        float az, el, it = 0; lateDirection (d, az, el);
        head.lookup (az, el, l.data(), r.data(), it);
        it *= earSpan / (2.0f * HEAD_RADIUS);
        const int dl = FracDelay::HALF + (it > 0 ? 0 : (int) std::lround (-it));
        const int dr = FracDelay::HALF + (it > 0 ? (int) std::lround (it) : 0);
        std::complex<float>* a = &out[(size_t) d * (size_t) fftN];
        for (int t = 0; t < nt; ++t)
        {
            if (t + dl < fftN) a[t + dl] += std::complex<float> (l[(size_t) t], 0.0f);
            if (t + dr < fftN) a[t + dr] += std::complex<float> (0.0f, r[(size_t) t]);
        }
        fftc::fft (a, fftN, false);
    }
}

void synthLateIr (const LateEnergy& E, const std::vector<std::complex<float>>& H, int fftN,
                  double fs, std::vector<float>& L, std::vector<float>& R)
{
    const int Ns = std::max (1, (int) std::lround (E.binSec * fs));
    const int len = std::min (fftN, E.nbins * Ns);
    L.assign ((size_t) fftN, 0.0f); R.assign ((size_t) fftN, 0.0f);
    if (E.nbins <= 0 || (int) H.size() < LateEnergy::NDIR * fftN) return;

    BandPath proto[NBAND];
    for (int b = 0; b < NBAND; ++b) proto[b].make (b, (float) fs);

    /*  The envelope, smoothed over a window that widens with time: a few rays
        cross the sphere per millisecond late on, and a bare 1 ms histogram would
        put their counting noise into the tail as a roughness no room has. */
    std::vector<float> amp ((size_t) NBAND * LateEnergy::NDIR * (size_t) E.nbins, 0.0f);
    std::vector<double> pre ((size_t) E.nbins + 1);
    bool any = false;
    for (int b = 0; b < NBAND; ++b)
        for (int d = 0; d < LateEnergy::NDIR; ++d)
        {
            pre[0] = 0;
            for (int t = 0; t < E.nbins; ++t) pre[(size_t) t + 1] = pre[(size_t) t] + E.at (b, d, t);
            if (pre[(size_t) E.nbins] <= 0) continue;
            any = true;
            float* a = &amp[((size_t) (b * LateEnergy::NDIR + d)) * (size_t) E.nbins];
            for (int t = 0; t < E.nbins; ++t)
            {
                const int h = std::min (60, (int) (0.04f * (float) t));
                const int t0 = std::max (0, t - h), t1 = std::min (E.nbins - 1, t + h);
                const double mean = (pre[(size_t) t1 + 1] - pre[(size_t) t0]) / (double) (t1 - t0 + 1);
                /*  A ray's band energy is the WHOLE broadband impulse's energy as
                    if every frequency behaved like this band (the direct sound
                    reads 1/r^2 in every band). So it is a spectral density: white
                    noise of variance E/Ns, of which the band filter keeps its own
                    share - exactly the band's part of a broadband impulse. */
                a[t] = (float) std::sqrt (std::max (0.0, mean) / (double) Ns);
            }
        }
    if (! any) return;

    std::vector<std::complex<float>> acc ((size_t) fftN, std::complex<float> (0, 0)), z ((size_t) fftN);
    std::vector<float> x[2] = { std::vector<float> ((size_t) len), std::vector<float> ((size_t) len) };
    for (int d0 = 0; d0 < LateEnergy::NDIR; d0 += 2)
    {
        for (int q = 0; q < 2; ++q)
        {
            const int d = d0 + q;
            std::fill (x[q].begin(), x[q].end(), 0.0f);
            if (d >= LateEnergy::NDIR) continue;
            for (int b = 0; b < NBAND; ++b)
            {
                const float* a = &amp[((size_t) (b * LateEnergy::NDIR + d)) * (size_t) E.nbins];
                int lastBin = E.nbins - 1; while (lastBin >= 0 && a[lastBin] <= 0) --lastBin;
                if (lastBin < 0) continue;
                BandPath bp = proto[b];
                Rng rng (0xB00Bull + (uint64_t) (d * 131 + b * 7919));
                const int end = std::min (len, (lastBin + 2) * Ns);
                for (int i = 0; i < end; ++i)
                {
                    // bin centres at (t + 0.5) Ns: linear between them
                    const float ft = ((float) i + 0.5f) / (float) Ns - 0.5f;
                    const int t = std::max (0, std::min (E.nbins - 1, (int) std::floor (ft)));
                    const float u = std::max (0.0f, std::min (1.0f, ft - (float) t));
                    const float ga = a[t] + (t + 1 < E.nbins ? (a[t + 1] - a[t]) * u : 0.0f);
                    const float w = (rng.uni() * 2.0f - 1.0f) * 1.7320508f;     // unit variance
                    x[q][(size_t) i] += ga * bp.run (w);
                }
            }
        }
        // two real directions in one transform
        std::fill (z.begin(), z.end(), std::complex<float> (0, 0));
        for (int i = 0; i < len; ++i) z[(size_t) i] = std::complex<float> (x[0][(size_t) i], x[1][(size_t) i]);
        fftc::fft (z.data(), fftN, false);
        const std::complex<float>* Ha = &H[(size_t) d0 * (size_t) fftN];
        const std::complex<float>* Hb = d0 + 1 < LateEnergy::NDIR ? &H[(size_t) (d0 + 1) * (size_t) fftN] : nullptr;
        for (int k = 0; k < fftN; ++k)
        {
            const std::complex<float> Zk = z[(size_t) k], Zn = std::conj (z[(size_t) ((fftN - k) & (fftN - 1))]);
            const std::complex<float> Xa = 0.5f * (Zk + Zn);
            const std::complex<float> Xb = std::complex<float> (0.0f, -0.5f) * (Zk - Zn);
            acc[(size_t) k] += Xa * Ha[k];
            if (Hb != nullptr) acc[(size_t) k] += Xb * Hb[k];
        }
    }
    fftc::fft (acc.data(), fftN, true);
    for (int i = 0; i < fftN; ++i) { L[(size_t) i] = acc[(size_t) i].real(); R[(size_t) i] = acc[(size_t) i].imag(); }
}

} // namespace tw
