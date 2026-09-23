/*  Broken walls, wired in. A room whose walls are square keeps the shoebox image
    source - exact, fast, and bit-identical to what it has always done. A room
    with a wall broken goes through the general search in Geometry.h.

    The polygon's own area and perimeter feed the absorption too, so pushing a
    wall out really does make the room a little bigger and a little longer.
*/
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " for " + a.slice(0, 70)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

edit("Source/Engine.h", rep => {
  // forward declarations: Geometry.h includes Engine.h, so the types live there
  rep(`struct RoomSurfaces
{`,
`struct WallBreak;
struct RoomBreaks;
struct RoomGeom;

struct RoomSurfaces
{`);
  rep(`    int   ceilMat[NUM_ROOMS]  = { -1, -1, -1 };`,
      `    int   ceilMat[NUM_ROOMS]  = { -1, -1, -1 };
    // two breakable walls per room: where along each, and how far out of the room
    float breakAlong[NUM_ROOMS][2] = { { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f } };
    float breakPush [NUM_ROOMS][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };`);
  rep(`    void addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase);`,
      `    void addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase);
    // the same job for a room that is no longer a box
    void addPlanImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase);
    bool roomIsBroken (int room) const;`);
  rep(`    RoomSurfaces surfNow[NUM_ROOMS];`,
      `    RoomSurfaces surfNow[NUM_ROOMS];
    std::vector<char> geomStore;          // one RoomGeom per room, built when a break moves
    float breakNow[NUM_ROOMS][4] = {};    // along0, push0, along1, push1`);
});

edit("Source/Engine.cpp", rep => {
  rep(`#include "Engine.h"
#include "HrtfData.h"`, `#include "Engine.h"
#include "Geometry.h"
#include "HrtfData.h"`);

  // the absorption takes the real polygon when there is one
  rep(`static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7)
{
    const Room& R = ROOMS[room];
    const float S = R.surface();
    // the three kinds of surface, by area
    const float aFloor = (R.x1 - R.x0) * (R.y1 - R.y0);
    const float aCeil  = aFloor;
    const float aWall  = std::max (0.0f, S - aFloor - aCeil);`,
`static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7,
                            float planArea = 0, float perimeter = 0)
{
    const Room& R = ROOMS[room];
    // a broken wall really does change the room's size: take the polygon's own
    // area and perimeter when one has been built
    const float aFloor = planArea > 0 ? planArea : (R.x1 - R.x0) * (R.y1 - R.y0);
    const float aCeil  = aFloor;
    const float aWall  = perimeter > 0 ? perimeter * R.h : std::max (0.0f, R.surface() - 2.0f * aFloor);
    const float S = aFloor + aCeil + aWall;
    const float V = aFloor * R.h;`);
  rep(`        const float abar = std::min (0.98f, A / S);
        const float m = AIR_DB_PER_KM[b] / (1000.0f * 4.343f);
        alphaBar7[b] = abar;
        sabineArea7[b] = A;
        rt7[b] = 0.161f * R.volume() / (-S * std::log (1.0f - abar) + 4.0f * m * R.volume());`,
      `        const float abar = std::min (0.98f, A / S);
        const float m = AIR_DB_PER_KM[b] / (1000.0f * 4.343f);
        alphaBar7[b] = abar;
        sabineArea7[b] = A;
        rt7[b] = 0.161f * V / (-S * std::log (1.0f - abar) + 4.0f * m * V);`);

  // the geometry cache
  rep(`void Engine::rebuildCouplings()`,
`bool Engine::roomIsBroken (int room) const
{
    return std::abs (breakNow[room][1]) > 1.0e-4f || std::abs (breakNow[room][3]) > 1.0e-4f;
}

void Engine::rebuildCouplings()`);

  rep(`    if (! changed && ! sourcesChanged) return;`,
`    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        const float want[4] = { cur.breakAlong[r][0], cur.breakPush[r][0], cur.breakAlong[r][1], cur.breakPush[r][1] };
        for (int k = 0; k < 4; ++k)
            if (std::abs (breakNow[r][k] - want[k]) > 1.0e-4f) { breakNow[r][k] = want[k]; changed = true; }
    }
    if (! changed && ! sourcesChanged) return;

    // rebuild the plan of every room whose walls have moved
    {
        RoomGeom* g = reinterpret_cast<RoomGeom*> (geomStore.data());
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            RoomBreaks br;
            br.w[0].along = breakNow[r][0]; br.w[0].push = breakNow[r][1];
            br.w[1].along = breakNow[r][2]; br.w[1].push = breakNow[r][3];
            g[r] = buildRoomGeom (r, br);
        }
    }`);

  rep(`            float abar[NBAND], A[NBAND];
            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60);`,
`            const RoomGeom* gg = reinterpret_cast<const RoomGeom*> (geomStore.data());
            float abar[NBAND], A[NBAND];
            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60, gg[r].area, gg[r].perimeter);`);

  // allocate the cache at prepare, before the first updateRoomAcoustics
  rep(`    for (int r = 0; r < NUM_ROOMS; ++r) measureEfficiency (r);`,
`    geomStore.assign (sizeof (RoomGeom) * NUM_ROOMS, 0);
    {
        RoomGeom* g = reinterpret_cast<RoomGeom*> (geomStore.data());
        RoomBreaks flat;
        for (int r = 0; r < NUM_ROOMS; ++r) g[r] = buildRoomGeom (r, flat);
    }

    for (int r = 0; r < NUM_ROOMS; ++r) measureEfficiency (r);`);

  // the general search, and the choice of which to use
  rep(`void Engine::addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    const int mat = matNow[room];`,
`/*  A room that is no longer a box. Every ordered sequence of surfaces up to the
    given order is a candidate; tracePath mirrors the source across them in turn
    and then walks back from the listener, and a sequence survives only if every
    reflection point really lands on the surface it is supposed to and nothing is
    in the way. Costs more than the shoebox arithmetic and runs only for a room
    whose walls have actually been moved. */
void Engine::addPlanImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    const RoomGeom& g = reinterpret_cast<const RoomGeom*> (geomStore.data())[room];

    auto emit = [&] (const int* seq, int nseq)
    {
        if (nspecs >= MAX_PATHS - 8) return;
        Vec3 hits[3]; float len = 0;
        if (! tracePath (g, S, L, seq, nseq, hits, len)) return;
        // a reflection that lands in an open doorway has left the room
        for (int k = 0; k < nseq; ++k)
            if (bounceHitsOpening (room, g.s[seq[k]].wallId, hits[k])) return;

        PathSpec& sp = specs[(size_t) nspecs];
        sp = PathSpec();
        uint32_t id = 0;
        for (int k = 0; k < nseq; ++k) id = id * 16u + (uint32_t) (seq[k] + 1);
        sp.key = keyBase + 0x2000u + id;
        sp.kind = nseq == 0 ? PathKind::Direct : (nseq == 1 ? PathKind::Refl1 : PathKind::Refl2);
        for (int k = 0; k < nseq; ++k)
        {
            const int sm = surfNow[room].of (g.s[seq[k]].wallId);
            for (int band = 0; band < NBAND; ++band)
                sp.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                    (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));
        }
        sp.npts = 0; sp.pts[sp.npts++] = S;
        for (int k = 0; k < nseq; ++k) sp.pts[sp.npts++] = hits[k];
        sp.pts[sp.npts++] = L;
        const Vec3 arriveFrom = nseq > 0 ? hits[nseq - 1] : S;
        const Vec3 departTo   = nseq > 0 ? hits[0] : L;
        finishSpec (sp, arriveFrom, departTo, len);
        if (nseq == 0)
        {
            const float sy = std::sin (rad (lisYaw)), cy = std::cos (rad (lisYaw));
            const Vec3 rightV (sy, -cy, 0.0f);
            const float half = 0.5f * target.earSpan;
            sp.gainL = len / std::max ((S - (L - rightV * half)).len(), 0.08f);
            sp.gainR = len / std::max ((S - (L + rightV * half)).len(), 0.08f);
        }
        ++nspecs;
    };

    int seq[3];
    emit (seq, 0);
    if (maxOrder >= 1)
        for (int i = 0; i < g.n; ++i) { seq[0] = i; emit (seq, 1); }
    if (maxOrder >= 2)
        for (int i = 0; i < g.n; ++i)
            for (int j = 0; j < g.n; ++j)
            {
                if (i == j) continue;
                seq[0] = i; seq[1] = j; emit (seq, 2);
            }
}

void Engine::addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    if (roomIsBroken (room)) { addPlanImagePaths (room, S, L, maxOrder, keyBase); return; }
    const int mat = matNow[room];`);

  // carry the new parameters through the per-block smoothing
  rep(`        { cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r]; }`,
      `        {
            cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r];
            for (int k = 0; k < 2; ++k)
            {
                cur.breakAlong[r][k] += k1 * (target.breakAlong[r][k] - cur.breakAlong[r][k]);
                cur.breakPush[r][k]  += k1 * (target.breakPush[r][k]  - cur.breakPush[r][k]);
            }
        }`);
  rep(`        const float k = 1.0f - std::exp (-(float) n / (0.025f * (float) fs));
        auto slerpYaw`,
      `        const float k = 1.0f - std::exp (-(float) n / (0.025f * (float) fs));
        const float k1 = 1.0f - std::exp (-(float) n / (0.12f * (float) fs));   // walls move slowly
        auto slerpYaw`);
  rep(`        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] = p.door[d];
        mixNow = p.mix; trimOut = dbToLin (p.outputDb);`,
      `        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] = p.door[d];
        for (int r = 0; r < NUM_ROOMS; ++r)
            for (int k = 0; k < 2; ++k) { cur.breakAlong[r][k] = p.breakAlong[r][k]; cur.breakPush[r][k] = p.breakPush[r][k]; }
        mixNow = p.mix; trimOut = dbToLin (p.outputDb);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("broken walls wired in");
