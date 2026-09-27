/*  STUDIO - a fifth material that turns any of the three rooms into a treated
    recording room. Peter: "symmetric, acoustically treated yet feeling live
    (like Abbey Road studio 2, but with just the right amount and type of
    acoustic treatment)... just one option called STUDIO which converts any of
    the rooms into a studio recording configuration."

    Three things make a room a studio, and all three are separate physics:

    1. A FLAT DECAY. Ordinary surfaces absorb the top end far more than the
       bottom, so an ordinary room decays bass-heavy. Treatment is bass traps at
       the low end and restraint at the high end: alpha near 0.3 and gently
       FALLING with frequency, the fall paying for the air absorption that eats
       the top of a large room. The result is an RT60 that hardly moves across
       the band, which is the single most recognisable property of a good room.

    2. LIVE, NOT DEAD. Alpha around 0.25-0.30 leaves the hall at about 0.7 s and
       the living room at about 0.4 s - a room you would keep a drum kit in, not
       a vocal booth.

    3. DIFFUSION. This is the part a "treated" room is really made of, and it is
       not absorption. A diffuser keeps the energy in the room and takes it out
       of the specular direction, which is what removes flutter echo and the
       boxy comb filtering while leaving the room live. Modelled as the ISO 17497
       scattering coefficient s: a reflection keeps (1-alpha)(1-s) of the energy
       specularly, and the (1-alpha)s that is scattered is picked up by the late
       field automatically, because the field's level is already calibrated as
       "the whole reverberant energy minus what the images carry".

    Plus the splayed walls Peter asked for. The image-source method needs a
    shoebox, so the geometry stays rectangular; what splay actually does to the
    sound - stop the reflections landing at exact multiples of the room
    dimension, so the comb never builds - is modelled directly, as a small
    deterministic perturbation of each image's path length and bearing
    (Vorlander's image-source randomisation, used for exactly this).

    THE EXISTING FOUR MATERIALS GET SCATTERING 0 AND SPLAY 0, so every room that
    sounds a particular way today still does, sample for sample.
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
  rep("constexpr int NUM_MATERIALS = 4;", "constexpr int NUM_MATERIALS = 5;   // the fifth is STUDIO");
  rep(`extern const float MATERIAL_ALPHA[NUM_MATERIALS][NBAND];   // octave-band absorption`,
      `extern const float MATERIAL_ALPHA[NUM_MATERIALS][NBAND];   // octave-band absorption
extern const float MATERIAL_SCATTER[NUM_MATERIALS][NBAND]; // ISO 17497 scattering coefficient
extern const float MATERIAL_SPLAY[NUM_MATERIALS];          // metres of image perturbation per order`);
});

edit("Source/Engine.cpp", rep => {
  rep(`const char* MATERIAL_NAMES[NUM_MATERIALS] = { "ABSORBING", "FURNISHED", "PLASTER", "TILED" };`,
      `const char* MATERIAL_NAMES[NUM_MATERIALS] = { "ABSORBING", "FURNISHED", "PLASTER", "TILED", "STUDIO" };`);
  rep(`    { 0.02f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f },
};`,
`    { 0.02f, 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f },
    /*  STUDIO: bass traps at the bottom, restraint at the top. The gentle fall
        with frequency is what pays for air absorption, so the decay comes out
        flat across the band instead of bass-heavy. About 0.7 s in the hall and
        0.4 s in the living room - treated, still live. */
    { 0.32f, 0.30f, 0.28f, 0.26f, 0.24f, 0.21f, 0.18f },
};

/*  How much of a reflection leaves the specular direction (ISO 17497). The four
    original materials are zero on purpose: every room that sounds a particular
    way today still does, sample for sample. A studio's diffusers take hold above
    their design frequency, which is why this rises with it. */
const float MATERIAL_SCATTER[NUM_MATERIALS][NBAND] =
{
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0.10f, 0.22f, 0.38f, 0.52f, 0.60f, 0.65f, 0.68f },
};

// splayed walls, as metres of deterministic perturbation per reflection order
const float MATERIAL_SPLAY[NUM_MATERIALS] = { 0, 0, 0, 0, 0.11f };`);

  // a deterministic perturbation, so the same room is the same room every time
  rep(`void Engine::addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    const int mat = matNow[room];`,
`/*  Splayed walls. A reflection off a wall tilted by a degree or two arrives a
    little early or late and from a slightly different bearing, and - the point -
    the higher orders stop landing at exact multiples of the room dimension, so
    the comb never builds. Deterministic in the image indices: the same room is
    the same room on every render, and the bench can memcmp it. */
static void splayOf (int nx, int ny, int nz, int order, float amount, float& dLen, float& dAz)
{
    if (amount <= 0 || order == 0) { dLen = 0; dAz = 0; return; }
    uint32_t h = (uint32_t) ((nx + 8) * 73856093) ^ (uint32_t) ((ny + 8) * 19349663) ^ (uint32_t) ((nz + 8) * 83492791);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    const float a = ((h & 0xffff) / 32768.0f) - 1.0f;          // -1 .. 1
    const float b = (((h >> 16) & 0xffff) / 32768.0f) - 1.0f;
    dLen = a * amount * (float) order;
    dAz  = b * 2.2f * (float) order;                            // degrees: twice a ~1 degree tilt
}

void Engine::addImagePaths (int room, const Vec3& S, const Vec3& L, int maxOrder, uint32_t keyBase)
{
    const int mat = matNow[room];`);

  rep(`                    for (int i = 0; i < nb; ++i)
                        for (int band = 0; band < NBAND; ++band)
                            s.bandDb[band] += 10.0f * std::log10 (1.0f - MATERIAL_ALPHA[mat][band]);`,
`                    // per bounce: what the surface absorbs, and what it scatters
                    // out of the specular direction. The scattered part is not
                    // lost - the late field's calibration picks it up.
                    for (int i = 0; i < nb; ++i)
                        for (int band = 0; band < NBAND; ++band)
                            s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                (1.0f - MATERIAL_ALPHA[mat][band]) * (1.0f - MATERIAL_SCATTER[mat][band])));`);

  rep(`                    const Vec3 arriveFrom = nb > 0 ? b[0] : S;
                    const Vec3 departTo = nb > 0 ? b[nb - 1] : L;
                    finishSpec (s, arriveFrom, departTo, len);`,
`                    const Vec3 arriveFrom = nb > 0 ? b[0] : S;
                    const Vec3 departTo = nb > 0 ? b[nb - 1] : L;
                    float dLen = 0, dAz = 0;
                    splayOf (nx, ny, nz, order, MATERIAL_SPLAY[mat], dLen, dAz);
                    finishSpec (s, arriveFrom, departTo, std::max (0.2f, len + dLen));
                    s.az += dAz;`);
});

// the portal paths that carry one reflection
edit("Source/Engine.cpp", rep => {
  rep(`                for (int band = 0; band < NBAND; ++band) s.bandDb[band] += 10.0f * std::log10 (1.0f - MATERIAL_ALPHA[matNow[rs]][band]);`,
      `                for (int band = 0; band < NBAND; ++band) s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f, (1.0f - MATERIAL_ALPHA[matNow[rs]][band]) * (1.0f - MATERIAL_SCATTER[matNow[rs]][band])));`);
  rep(`            for (int band = 0; band < NBAND; ++band) s.bandDb[band] += 10.0f * std::log10 (1.0f - MATERIAL_ALPHA[matNow[rl]][band]);`,
      `            for (int band = 0; band < NBAND; ++band) s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f, (1.0f - MATERIAL_ALPHA[matNow[rl]][band]) * (1.0f - MATERIAL_SCATTER[matNow[rl]][band])));`);
});

// the host parameter: five choices now, and the defaults are indices, not fractions
edit("Source/PluginProcessor.cpp", rep => {
  rep(`        v.push_back ({ "mat1",    "LARGE ROOM WALLS",  1.0f / 3,     true,  4, "ABSORBING|FURNISHED|PLASTER|TILED" });
        v.push_back ({ "mat2",    "SMALL ROOM WALLS",  1.0f / 3,     true,  4, "ABSORBING|FURNISHED|PLASTER|TILED" });
        v.push_back ({ "mat3",    "GIANT ROOM WALLS",  2.0f / 3,     true,  4, "ABSORBING|FURNISHED|PLASTER|TILED" });`,
`        const std::string mats = "ABSORBING|FURNISHED|PLASTER|TILED|STUDIO";
        const float mn = (float) (NUM_MATERIALS - 1);
        v.push_back ({ "mat1",    "LARGE ROOM WALLS",  1.0f / mn,    true,  NUM_MATERIALS, mats });
        v.push_back ({ "mat2",    "SMALL ROOM WALLS",  1.0f / mn,    true,  NUM_MATERIALS, mats });
        v.push_back ({ "mat3",    "GIANT ROOM WALLS",  2.0f / mn,    true,  NUM_MATERIALS, mats });`);
  rep(`    current.material[0] = nextChoice (4);
    current.material[1] = nextChoice (4);
    current.material[2] = nextChoice (4);`,
      `    current.material[0] = nextChoice (NUM_MATERIALS);
    current.material[1] = nextChoice (NUM_MATERIALS);
    current.material[2] = nextChoice (NUM_MATERIALS);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("STUDIO material added");
