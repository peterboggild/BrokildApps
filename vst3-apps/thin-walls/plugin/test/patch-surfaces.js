/*  A material per surface: walls, floor and ceiling separately, per room.

    Why it is worth more than it looks: the floor and ceiling bounces are the two
    earliest reflections between a standing source and a standing listener (a
    source 2 m away at 1.2 m high puts them 4 ms behind the direct sound, 5 dB
    down), and until now they could only be treated by deadening the walls with
    them. A carpet is the most lopsided absorber in a normal room - almost
    nothing below 250 Hz, a great deal above 2 kHz - so "floor = ABSORBING" is a
    real and very audible tuning control, and "ceiling = ABSORBING" is the cloud
    every control room has over the desk.

    THE KEMPER RULE: floorMat and ceilMat default to -1, meaning "whatever the
    walls are", so every room that sounds a particular way today still does,
    sample for sample. The bench sets p.material[r] and gets all six surfaces, as
    it always did.
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
  rep(`int   roomOf (float x, float y);                       // -1 outside every room`,
`/*  Which material each of a room's six surfaces wears. A floor or ceiling of -1
    means "the same as the walls", which is what every room did before there was
    a choice - so a default Params is bit-identical to the old one. */
struct RoomSurfaces
{
    int wall = 1, floor = -1, ceil = -1;
    int floorMat() const { return floor < 0 ? wall : floor; }
    int ceilMat()  const { return ceil  < 0 ? wall : ceil;  }
    // imagePath numbers the walls 0 x0, 1 x1, 2 y0, 3 y1, 4 floor, 5 ceiling
    int of (int wallId) const { return wallId == 4 ? floorMat() : (wallId == 5 ? ceilMat() : wall); }
};

int   roomOf (float x, float y);                       // -1 outside every room`);

  rep(`    int   material[NUM_ROOMS] = { 1, 1, 2 };`,
      `    int   material[NUM_ROOMS] = { 1, 1, 2 };        // the walls
    int   floorMat[NUM_ROOMS] = { -1, -1, -1 };     // -1: the same as the walls
    int   ceilMat[NUM_ROOMS]  = { -1, -1, -1 };`);

  rep(`    static void  eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7);`,
      `    static void  eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7);
    static void  eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7);`);
  rep(`    static void  absorptionArea (int room, const int* materials, const float* doorAperture, float* out7);`,
      `    static void  absorptionArea (int room, const int* materials, const float* doorAperture, float* out7);
    static void  absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7);`);
  rep(`    int   matNow[NUM_ROOMS] = { -1, -1, -1 };`,
      `    RoomSurfaces surfNow[NUM_ROOMS];
    int   matNow[NUM_ROOMS] = { -1, -1, -1 };`);
});

edit("Source/Engine.cpp", rep => {
  // the absorption sum, per surface
  rep(`static void roomAbsorption (int room, const int* materials, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7)
{
    const Room& R = ROOMS[room];
    const float S = R.surface();
    for (int b = 0; b < NBAND; ++b)
    {
        float A = S * MATERIAL_ALPHA[materials[room]][b];
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            if (doorWall (room, d) < 0) continue;
            const float a = doorAperture[d];
            const float tau = a + (1.0f - a) * std::pow (10.0f, -LEAF_TL_DB[b] * 0.1f);
            A += DOORS[d].area() * (tau - MATERIAL_ALPHA[materials[room]][b]);
        }`,
`static void roomAbsorption (int room, const RoomSurfaces& surf, const float* doorAperture,
                            float* alphaBar7, float* sabineArea7, float* rt7)
{
    const Room& R = ROOMS[room];
    const float S = R.surface();
    // the three kinds of surface, by area
    const float aFloor = (R.x1 - R.x0) * (R.y1 - R.y0);
    const float aCeil  = aFloor;
    const float aWall  = std::max (0.0f, S - aFloor - aCeil);
    for (int b = 0; b < NBAND; ++b)
    {
        float A = aWall  * MATERIAL_ALPHA[surf.wall][b]
                + aFloor * MATERIAL_ALPHA[surf.floorMat()][b]
                + aCeil  * MATERIAL_ALPHA[surf.ceilMat()][b];
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            if (doorWall (room, d) < 0) continue;
            const float a = doorAperture[d];
            const float tau = a + (1.0f - a) * std::pow (10.0f, -LEAF_TL_DB[b] * 0.1f);
            A += DOORS[d].area() * (tau - MATERIAL_ALPHA[surf.wall][b]);   // a door is in a wall
        }`);

  rep(`void Engine::eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, materials, doorAperture, a, A, out7);
}

void Engine::absorptionArea (int room, const int* materials, const float* doorAperture, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, materials, doorAperture, a, out7, rt);
}`,
`void Engine::eyringRt60 (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, surf, doorAperture, a, A, out7);
}

void Engine::absorptionArea (int room, const RoomSurfaces& surf, const float* doorAperture, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, surf, doorAperture, a, out7, rt);
}

// the old shape, every surface the same: what the bench asks for
void Engine::eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7)
{
    RoomSurfaces s; s.wall = materials[room];
    eyringRt60 (room, s, doorAperture, out7);
}

void Engine::absorptionArea (int room, const int* materials, const float* doorAperture, float* out7)
{
    RoomSurfaces s; s.wall = materials[room];
    absorptionArea (room, s, doorAperture, out7);
}`);

  // the engine's own book-keeping
  rep(`    for (int r = 0; r < NUM_ROOMS; ++r) if (matNow[r] != cur.material[r]) { matNow[r] = cur.material[r]; changed = true; }`,
`    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        RoomSurfaces s; s.wall = cur.material[r]; s.floor = cur.floorMat[r]; s.ceil = cur.ceilMat[r];
        if (s.wall != surfNow[r].wall || s.floor != surfNow[r].floor || s.ceil != surfNow[r].ceil)
        { surfNow[r] = s; matNow[r] = s.wall; changed = true; }
    }`);
  rep(`            roomAbsorption (r, matNow, doorNow, abar, A, F.rt60);`,
      `            roomAbsorption (r, surfNow[r], doorNow, abar, A, F.rt60);`);
  rep(`        cur.material[0] = target.material[0]; cur.material[1] = target.material[1]; cur.material[2] = target.material[2];`,
      `        for (int r = 0; r < NUM_ROOMS; ++r)
        { cur.material[r] = target.material[r]; cur.floorMat[r] = target.floorMat[r]; cur.ceilMat[r] = target.ceilMat[r]; }`);

  // each bounce charged to the surface it hit
  rep(`                    // per bounce: what the surface absorbs, and what it scatters
                    // out of the specular direction. The scattered part is not
                    // lost - the late field's calibration picks it up.
                    for (int i = 0; i < nb; ++i)
                        for (int band = 0; band < NBAND; ++band)
                            s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                (1.0f - MATERIAL_ALPHA[mat][band]) * (1.0f - MATERIAL_SCATTER[mat][band])));`,
`                    /*  Per bounce: what THAT surface absorbs, and what it scatters
                        out of the specular direction. The scattered part is not
                        lost - the late field's calibration picks it up. The floor
                        and the ceiling carry their own material, which is the whole
                        point of a carpet or a cloud. */
                    for (int i = 0; i < nb; ++i)
                    {
                        const int sm = surfNow[room].of (walls[i]);
                        for (int band = 0; band < NBAND; ++band)
                            s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));
                    }`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("per-surface materials in the engine");
