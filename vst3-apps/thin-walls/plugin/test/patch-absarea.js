/*  The bench asked a freshly prepared Engine for its absorption area, but that is
    only recomputed while audio is running, so every material read back the same
    default and every critical distance came out 0.69 m. Expose the same pure
    function the engine uses, beside eyringRt60.
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
  rep(`    static void  eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7);`,
      `    static void  eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7);
    // the room's equivalent absorption area per band, m^2: the material over every
    // surface, each door counted at its own aperture, the party walls at what they pass
    static void  absorptionArea (int room, const int* materials, const float* doorAperture, float* out7);`);
});

edit("Source/Engine.cpp", rep => {
  rep(`void Engine::eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, materials, doorAperture, a, A, out7);
}`,
`void Engine::eyringRt60 (int room, const int* materials, const float* doorAperture, float* out7)
{
    float a[NBAND], A[NBAND];
    roomAbsorption (room, materials, doorAperture, a, A, out7);
}

void Engine::absorptionArea (int room, const int* materials, const float* doorAperture, float* out7)
{
    float a[NBAND], rt[NBAND];
    roomAbsorption (room, materials, doorAperture, a, out7, rt);
}`);
});

edit("test/bench.cpp", rep => {
  rep(`            float A = 0;
            { Engine* q = fresh (fs); q->setParams (p); A = q->field (r).absorptionArea[3]; delete q; }`,
      `            float Ab[NBAND]; Engine::absorptionArea (r, p.material, p.door, Ab);
            const float A = Ab[3];`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("absorptionArea exposed");
