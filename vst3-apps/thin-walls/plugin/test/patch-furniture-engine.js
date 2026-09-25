// Furniture in the engine (2026-09-25): catalogue, absorption into Eyring,
// scattering per bounce, occlusion by Maekawa over/under/round each piece,
// reflections off hard tops. Validates every anchor, then writes.
const fs = require('fs');
const path = require('path');
const src = path.join(__dirname, '..', 'Source');
const miss = [];
const files = {};
function load(f) { if (!files[f]) files[f] = fs.readFileSync(path.join(src, f), 'utf8'); }
function rep(f, a, b, count = 1) {
  load(f);
  const n = files[f].split(a).length - 1;
  if (n !== count) { miss.push(f + ': ' + JSON.stringify(a.slice(0, 80)) + ' x' + n); return; }
  files[f] = files[f].split(a).join(b);
}
// replace the k-th occurrence (0-based) of a
function repNth(f, a, k, b) {
  load(f);
  const s = files[f];
  let i = -1;
  for (let j = 0; j <= k; ++j) { i = s.indexOf(a, i + 1); if (i < 0) { miss.push(f + ': nth ' + k + ' of ' + a.slice(0, 50)); return; } }
  files[f] = s.slice(0, i) + b + s.slice(i + a.length);
}

const E = 'Engine.cpp', H = 'Engine.h';

// ---- header: the acoustic key of the layout
rep(H, '    FurnItem lastFurnAc[MAX_FURN]; int lastNfurnAc = -1;',
       '    FurnItem lastFurnAc[MAX_FURN]; int lastNfurnAc = -1;\n    int furnKeyAc[MAX_FURN] = {}; int furnKeyN = -1;   // type and room per piece: what the Eyring sums depend on');

// ---- the catalogue
rep(E, 'const float BAND_HZ[NBAND] = { 125, 250, 500, 1000, 2000, 4000, 8000 };',
`const float BAND_HZ[NBAND] = { 125, 250, 500, 1000, 2000, 4000, 8000 };

/*  The furniture catalogue. Absorption is the EQUIVALENT ABSORPTION AREA of the
    whole object in m^2 per octave band, the form the tables give for objects
    (Kuttruff, Room Acoustics, and the usual per-seat and per-person figures):
    an upholstered three-seater is about three upholstered seats; a person
    standing is Kuttruff's standing adult; the rug is a cut-pile carpet's alpha
    times its area (the floor under it is subtracted where it is used); the
    curtain is a heavy pleated drape. The hard pieces absorb little and matter
    by what they block and reflect. Scattering areas are an equivalent
    rough-surface area: books and upholstery high, a flat-fronted wardrobe low. */
const FurnSpec FURN[NUM_FURN_TYPES] =
{
    //  id          name            w     d     h     zb    zt    occl   top    topA   floor  absorption 125 .. 8k                               scatter
    { "sofa",     "SOFA",         2.10f, 0.90f, 0.85f, 0.00f, 0.85f, true,  false, 0.00f, false, { 0.60f, 1.10f, 1.50f, 1.70f, 1.75f, 1.70f, 1.60f }, 3.0f },
    { "armchair", "ARMCHAIR",     0.85f, 0.85f, 0.90f, 0.00f, 0.90f, true,  false, 0.00f, false, { 0.22f, 0.40f, 0.52f, 0.58f, 0.60f, 0.58f, 0.55f }, 1.5f },
    { "bed",      "BED",          2.05f, 1.60f, 0.55f, 0.00f, 0.55f, true,  false, 0.00f, false, { 0.35f, 0.70f, 1.30f, 1.70f, 1.85f, 1.85f, 1.75f }, 3.0f },
    { "rug",      "RUG",          2.40f, 1.70f, 0.02f, 0.00f, 0.02f, false, false, 0.00f, true,  { 0.08f, 0.24f, 0.57f, 1.51f, 2.45f, 2.65f, 2.65f }, 0.0f },
    { "curtain",  "CURTAIN",      2.40f, 0.15f, 2.50f, 0.00f, 2.50f, false, false, 0.00f, false, { 0.84f, 2.10f, 3.30f, 4.32f, 4.20f, 3.90f, 3.60f }, 1.0f },
    { "bookcase", "BOOKCASE",     1.00f, 0.35f, 2.00f, 0.00f, 2.00f, true,  false, 0.00f, false, { 0.20f, 0.30f, 0.50f, 0.60f, 0.60f, 0.60f, 0.60f }, 4.0f },
    { "table",    "TABLE",        1.60f, 0.90f, 0.75f, 0.71f, 0.75f, true,  true,  0.07f, false, { 0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f }, 1.5f },
    { "piano",    "GRAND PIANO",  2.10f, 1.50f, 1.00f, 0.35f, 1.00f, true,  true,  0.05f, false, { 0.30f, 0.25f, 0.15f, 0.10f, 0.08f, 0.08f, 0.08f }, 2.5f },
    { "wardrobe", "WARDROBE",     1.20f, 0.60f, 2.10f, 0.00f, 2.10f, true,  false, 0.00f, false, { 0.10f, 0.08f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f }, 2.0f },
    { "person",   "PERSON",       0.50f, 0.30f, 1.75f, 0.00f, 1.75f, true,  false, 0.00f, false, { 0.15f, 0.33f, 0.44f, 0.46f, 0.50f, 0.50f, 0.50f }, 1.0f },
};`);

// ---- Eyring: furniture into A
rep(E, `static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7,
                            float planArea = 0, float perimeter = 0)`,
`static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7,
                            float planArea = 0, float perimeter = 0,
                            const FurnItem* furn = nullptr, int nfurn = 0)`);
rep(E, `            A += std::max (0.0f, area) * std::pow (10.0f, -WALL_TL_DB[b] * 0.1f);
        }
`, `            A += std::max (0.0f, area) * std::pow (10.0f, -WALL_TL_DB[b] * 0.1f);
        }
        // the furniture standing in this room (a rug replaces the floor it covers)
        for (int i = 0; i < nfurn; ++i)
        {
            const FurnItem& it = furn[i];
            if (it.type < 0 || it.type >= NUM_FURN_TYPES || roomOf (it.x, it.y) != room) continue;
            const FurnSpec& F = FURN[it.type];
            float a = F.absorb[b];
            if (F.coversFloor) a -= F.w * F.d * MATERIAL_ALPHA[surf.floorMat()][b];
            A += std::max (0.0f, a);
        }
`);
rep(E, `// the old shape, every surface the same: what the bench asks for`,
`void Engine::eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, surf, doorAperture, a, A, out7, 0, 0, furn, nfurn);
}

void Engine::absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, const FurnItem* furn, int nfurn, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, surf, doorAperture, a, out7, rt, 0, 0, furn, nfurn);
}

// the old shape, every surface the same: what the bench asks for`);

// ---- updateRoomAcoustics: notice the layout, feed it to Eyring, scatter
rep(E, `    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved`,
`    /*  Furniture. Any move re-poses the pieces (occlusion and reflections read the
        poses every block); only a change of WHICH pieces stand in WHICH room
        touches Eyring, so dragging a sofa round its room does not re-run the
        room acoustics on every mouse event. */
    {
        const int nf = std::min (std::max (cur.nfurn, 0), MAX_FURN);
        bool moved = nf != lastNfurnAc;
        for (int i = 0; i < nf && ! moved; ++i)
            moved = cur.furn[i].type != lastFurnAc[i].type || cur.furn[i].x != lastFurnAc[i].x
                 || cur.furn[i].y != lastFurnAc[i].y || cur.furn[i].yaw != lastFurnAc[i].yaw;
        if (moved)
        {
            lastNfurnAc = nf;
            for (int i = 0; i < nf; ++i) lastFurnAc[i] = cur.furn[i];
            updateFurniture();
            bool acoustic = nf != furnKeyN;
            for (int i = 0; i < nf; ++i)
            {
                const int key = (furnPose[i].type + 1) * 8 + (furnPose[i].room + 1);
                if (key != furnKeyAc[i]) { furnKeyAc[i] = key; acoustic = true; }
            }
            furnKeyN = nf;
            if (acoustic) changed = true;
        }
    }
    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved`);
rep(E, `            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter);`,
`            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter, cur.furn, nfurnNow);
            // the furniture's scattering: 1 - exp (-2 sum / S), capped; exactly 0 dB with none
            {
                float sum = 0;
                for (int i = 0; i < nfurnNow; ++i)
                    if (furnPose[i].type >= 0 && furnPose[i].room == r) sum += FURN[furnPose[i].type].scatter;
                const float Sr = gg[r].area > 0 ? 2.0f * gg[r].area + gg[r].perimeter * ROOMS[r].h : ROOMS[r].surface();
                furnScatter[r]   = sum > 0 ? std::min (0.6f, 1.0f - std::exp (-2.0f * sum / Sr)) : 0.0f;
                furnScatterDb[r] = sum > 0 ? 10.0f * std::log10 (1.0f - furnScatter[r]) : 0.0f;
            }`);

// ---- every wall bounce keeps (1 - s) of its specular energy
repNth(E, '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));', 2,
          '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[rs];');
repNth(E, '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));', 1,
          '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[room];');
repNth(E, '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));', 0,
          '(1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band]))) + furnScatterDb[room];');
rep(E, `(1.0f - MATERIAL_SCATTER[matNow[rl]][band])));`, `(1.0f - MATERIAL_SCATTER[matNow[rl]][band]))) + furnScatterDb[rl];`);

// ---- the layout follows the target every block
rep(E, `            cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r];`,
       `            cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r];
            if (r == 0) { cur.nfurn = target.nfurn; for (int i = 0; i < MAX_FURN; ++i) cur.furn[i] = target.furn[i]; }`);

// ---- buildPaths: reflections off furniture per source, then occlusion over everything
rep(E, `        if (rs >= 0 && rs == rl) addImagePaths (rs, S, lisPos, 2, 0u);
        addPortalPaths (S, rs, rl);
        addTransmissionPaths (S, rs, rl);
    }`, `        if (rs >= 0 && rs == rl) addImagePaths (rs, S, lisPos, 2, 0u);
        addPortalPaths (S, rs, rl);
        addTransmissionPaths (S, rs, rl);
        addFurnitureReflections (S, rs, rl);
    }
    applyOcclusion();`);

// ---- the functions, placed after addDiffraction and portalPoint
rep(E, `void Engine::buildPaths()
{`, `//==============================================================================
// furniture geometry
namespace
{
    inline float len3 (float x, float y, float z) { return std::sqrt (x * x + y * y + z * z); }

    // Liang-Barsky: the part of the 2D segment a->b inside |x| <= hw, |y| <= hd
    bool clipRect (float ax, float ay, float bx, float by, float hw, float hd, float& u0, float& u1)
    {
        u0 = 0; u1 = 1;
        const float dx = bx - ax, dy = by - ay;
        auto clip = [&] (float p, float q)
        {
            if (std::abs (p) < 1e-9f) return q >= 0;
            const float r = q / p;
            if (p < 0) { if (r > u1) return false; if (r > u0) u0 = r; }
            else       { if (r < u0) return false; if (r < u1) u1 = r; }
            return true;
        };
        return clip (-dx, ax + hw) && clip (dx, hw - ax) && clip (-dy, ay + hd) && clip (dy, hd - ay) && u1 > u0;
    }

    /*  The shortest way round the rectangle in the plane from p to q, both
        outside it: the convex hull of p, q and the four corners has two chains
        between p and q, one down each side; the shorter is the answer. */
    float aroundRect (float px, float py, float qx, float qy, float hw, float hd)
    {
        const float X[6] = { px, qx, -hw, hw, hw, -hw }, Y[6] = { py, qy, -hd, -hd, hd, hd };
        int idx[6] = { 0, 1, 2, 3, 4, 5 };
        std::sort (idx, idx + 6, [&] (int a, int b) { return X[a] < X[b] || (X[a] == X[b] && Y[a] < Y[b]); });
        auto cross = [&] (int o, int a, int b) { return (X[a] - X[o]) * (Y[b] - Y[o]) - (Y[a] - Y[o]) * (X[b] - X[o]); };
        int H[14]; int k = 0;
        for (int i = 0; i < 6; ++i) { while (k >= 2 && cross (H[k - 2], H[k - 1], idx[i]) <= 0) --k; H[k++] = idx[i]; }
        for (int i = 4, t = k + 1; i >= 0; --i) { while (k >= t && cross (H[k - 2], H[k - 1], idx[i]) <= 0) --k; H[k++] = idx[i]; }
        --k;
        int ip = -1, iq = -1;
        for (int i = 0; i < k; ++i) { if (H[i] == 0) ip = i; if (H[i] == 1) iq = i; }
        if (ip < 0 || iq < 0 || k < 3) return -1.0f;
        auto seg = [&] (int a, int b) { return std::hypot (X[H[b]] - X[H[a]], Y[H[b]] - Y[H[a]]); };
        float c1 = 0; for (int i = ip; i != iq; i = (i + 1) % k) c1 += seg (i, (i + 1) % k);
        float c2 = 0; for (int i = iq; i != ip; i = (i + 1) % k) c2 += seg (i, (i + 1) % k);
        return std::min (c1, c2);
    }
}

void Engine::updateFurniture()
{
    nfurnNow = 0;
    const int nf = std::min (std::max (cur.nfurn, 0), MAX_FURN);
    for (int i = 0; i < nf; ++i)
    {
        const FurnItem& it = cur.furn[i];
        FurnPose& p = furnPose[i];
        p = FurnPose();
        if (it.type >= 0 && it.type < NUM_FURN_TYPES)
        {
            const FurnSpec& F = FURN[it.type];
            const float a = rad (it.yaw);
            p.cx = it.x; p.cy = it.y; p.c = std::cos (a); p.s = std::sin (a);
            p.hw = 0.5f * F.w; p.hd = 0.5f * F.d; p.zb = F.zb; p.zt = F.zt;
            p.type = it.type; p.room = roomOf (it.x, it.y);
        }
        nfurnNow = i + 1;
    }
}

/*  The Maekawa path difference of the leg P->Q round piece i. Blocked: the
    shortest way over the top, under the bottom (a table, a piano on its legs)
    or round the sides, less the straight line. Clear: minus how much longer a
    path touching the piece at its point nearest the leg would be - the lit side
    of the same curve, so an edge comes and goes without a step. */
float Engine::furnitureDelta (int i, const Vec3& P, const Vec3& Q) const
{
    if (i < 0 || i >= nfurnNow) return -1.0e9f;
    const FurnPose& f = furnPose[i];
    if (f.type < 0 || ! FURN[f.type].occludes) return -1.0e9f;
    auto loc = [&] (const Vec3& p, float& x, float& y)
    {
        const float dx = p.x - f.cx, dy = p.y - f.cy;
        x = f.c * dx + f.s * dy; y = -f.s * dx + f.c * dy;
    };
    float px, py, qx, qy; loc (P, px, py); loc (Q, qx, qy);
    const float pz = P.z, qz = Q.z;
    const float m = 1.0f;     // beyond this the lit side is below a quarter of a dB at 125 Hz
    if (std::max (px, qx) < -f.hw - m || std::min (px, qx) > f.hw + m
     || std::max (py, qy) < -f.hd - m || std::min (py, qy) > f.hd + m
     || std::max (pz, qz) < f.zb - m || std::min (pz, qz) > f.zt + m) return -1.0e9f;
    const float e = 1.0e-3f;
    auto inside = [&] (float x, float y, float z) { return std::abs (x) < f.hw - e && std::abs (y) < f.hd - e && z > f.zb + e && z < f.zt - e; };
    if (inside (px, py, pz) || inside (qx, qy, qz)) return -1.0e9f;     // nothing blocks itself
    const float straight = len3 (qx - px, qy - py, qz - pz);
    if (straight < 1.0e-4f) return -1.0e9f;

    float u0 = 0, u1 = 0;
    const bool xy = clipRect (px, py, qx, qy, f.hw, f.hd, u0, u1);
    const float z0 = pz + (qz - pz) * u0, z1 = pz + (qz - pz) * u1;
    const bool blocked = xy && std::max (z0, z1) > f.zb && std::min (z0, z1) < f.zt;
    if (blocked)
    {
        const float ax = px + (qx - px) * u0, ay = py + (qy - py) * u0;
        const float bx = px + (qx - px) * u1, by = py + (qy - py) * u1;
        float best = 1.0e9f;
        {   // over the top
            const float za = std::max (z0, f.zt), zb2 = std::max (z1, f.zt);
            const float L = len3 (ax - px, ay - py, za - pz) + len3 (bx - ax, by - ay, zb2 - za) + len3 (qx - bx, qy - by, qz - zb2);
            best = std::min (best, L - straight);
        }
        if (f.zb > 0.02f)
        {   // under it
            const float za = std::min (z0, f.zb), zb2 = std::min (z1, f.zb);
            const float L = len3 (ax - px, ay - py, za - pz) + len3 (bx - ax, by - ay, zb2 - za) + len3 (qx - bx, qy - by, qz - zb2);
            best = std::min (best, L - straight);
        }
        const bool pin = std::abs (px) <= f.hw && std::abs (py) <= f.hd;
        const bool qin = std::abs (qx) <= f.hw && std::abs (qy) <= f.hd;
        if (! pin && ! qin)
        {   // round the sides
            const float a2 = aroundRect (px, py, qx, qy, f.hw, f.hd);
            if (a2 > 0) best = std::min (best, len3 (a2, 0.0f, qz - pz) - straight);
        }
        return std::max (1.0e-5f, best);
    }
    // clear: the point of the box nearest the leg (distance is convex along it)
    auto nearest = [&] (float u, float& cx, float& cy, float& cz)
    {
        const float x = px + (qx - px) * u, y = py + (qy - py) * u, z = pz + (qz - pz) * u;
        cx = std::max (-f.hw, std::min (f.hw, x)); cy = std::max (-f.hd, std::min (f.hd, y)); cz = std::max (f.zb, std::min (f.zt, z));
        return len3 (x - cx, y - cy, z - cz);
    };
    float lo = 0, hi = 1, cx, cy, cz;
    for (int it = 0; it < 30; ++it)
    {
        const float m1 = lo + (hi - lo) / 3.0f, m2 = hi - (hi - lo) / 3.0f;
        if (nearest (m1, cx, cy, cz) < nearest (m2, cx, cy, cz)) hi = m2; else lo = m1;
    }
    nearest (0.5f * (lo + hi), cx, cy, cz);
    const float L = len3 (cx - px, cy - py, cz - pz) + len3 (qx - cx, qy - cy, qz - cz);
    return -(L - straight);
}

void Engine::applyOcclusion()
{
    if (nfurnNow == 0) return;
    for (int i = 0; i < nspecs; ++i)
    {
        PathSpec& s = specs[(size_t) i];
        if (s.kind == PathKind::DoorField || s.npts < 2) continue;
        for (int k = 0; k + 1 < s.npts; ++k)
            for (int f = 0; f < nfurnNow; ++f)
            {
                if (f == s.selfItem) continue;
                const float d = furnitureDelta (f, s.pts[k], s.pts[k + 1]);
                if (d > -0.5f) addDiffraction (s, d);
            }
    }
}

/*  A hard horizontal top - a table, a closed piano lid - mirrors the source like
    a floor would, but it is finite: while the mirror point is on the top the
    path is line of sight with the lit-side edge term of its nearest edge, and
    once the point leaves it the path bends at the edge with Maekawa's loss, so
    it fades out rather than vanishing when you step past the table. */
void Engine::addFurnitureReflections (const Vec3& S, int rs, int rl)
{
    if (rs < 0 || rs != rl || nfurnNow == 0) return;
    for (int i = 0; i < nfurnNow; ++i)
    {
        const FurnPose& f = furnPose[i];
        if (f.type < 0 || f.room != rs) continue;
        const FurnSpec& F = FURN[f.type];
        if (! F.reflectTop) continue;
        const float zt = f.zt;
        if (S.z <= zt + 0.02f || lisPos.z <= zt + 0.02f) continue;
        if (pathsFull (2)) return;
        const Vec3 I (S.x, S.y, 2.0f * zt - S.z);
        const float tt = (zt - I.z) / (lisPos.z - I.z);
        const Vec3 R = I + (lisPos - I) * tt;
        const float dx = R.x - f.cx, dy = R.y - f.cy;
        const float rx = f.c * dx + f.s * dy, ry = -f.s * dx + f.c * dy;
        const bool on = std::abs (rx) <= f.hw && std::abs (ry) <= f.hd;
        float ex = rx, ey = ry;
        if (on)
        {
            if (f.hw - std::abs (rx) < f.hd - std::abs (ry)) ex = rx >= 0 ? f.hw : -f.hw;
            else                                             ey = ry >= 0 ? f.hd : -f.hd;
        }
        else { ex = std::max (-f.hw, std::min (f.hw, rx)); ey = std::max (-f.hd, std::min (f.hd, ry)); }
        const Vec3 Ed (f.cx + f.c * ex - f.s * ey, f.cy + f.s * ex + f.c * ey, zt);
        const float straight = (lisPos - I).len();
        const float viaE = (Ed - I).len() + (lisPos - Ed).len();
        const float delta = on ? -(viaE - straight) : (viaE - straight);
        if (delta > 1.5f) continue;
        const Vec3 X = on ? R : Ed;
        PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
        s.key = 0x60000u + (uint32_t) i * 8u;
        s.kind = PathKind::Refl1;
        s.selfItem = i;
        const float topDb = 10.0f * std::log10 (std::max (1.0e-4f, 1.0f - F.topAlpha));
        for (int b = 0; b < NBAND; ++b) s.bandDb[b] += topDb + furnScatterDb[rs];
        addDiffraction (s, delta);
        s.npts = 0; s.pts[s.npts++] = S; s.pts[s.npts++] = X; s.pts[s.npts++] = lisPos;
        const float len = on ? straight : (X - S).len() + (lisPos - X).len();
        finishSpec (s, X, X, len);
        ++nspecs;
    }
}

void Engine::buildPaths()
{`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
