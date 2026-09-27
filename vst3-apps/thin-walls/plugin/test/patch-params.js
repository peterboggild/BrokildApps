/*  The host parameters for the two new features: a material per surface, and a
    breakable wall. 47 becomes 65.

    Floor and ceiling default to "AS WALLS", and both folds default to no push,
    so a fresh instance and every project saved before today is exactly the room
    it was.
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

edit("Source/PluginProcessor.cpp", rep => {
  rep(`        const std::string mats = "ABSORBING|FURNISHED|PLASTER|TILED|STUDIO";
        const float mn = (float) (NUM_MATERIALS - 1);
        v.push_back ({ "mat1",    "LARGE ROOM WALLS",  1.0f / mn,    true,  NUM_MATERIALS, mats });
        v.push_back ({ "mat2",    "SMALL ROOM WALLS",  1.0f / mn,    true,  NUM_MATERIALS, mats });
        v.push_back ({ "mat3",    "GIANT ROOM WALLS",  2.0f / mn,    true,  NUM_MATERIALS, mats });`,
`        const std::string mats = "ABSORBING|FURNISHED|PLASTER|TILED|STUDIO";
        const std::string surfMats = "AS WALLS|" + mats;             // the default is to follow the walls
        const float mn = (float) (NUM_MATERIALS - 1);
        const char* roomName[NUM_ROOMS] = { "LARGE", "SMALL", "GIANT" };
        const float wallDef[NUM_ROOMS] = { 1.0f / mn, 1.0f / mn, 2.0f / mn };
        for (int r = 0; r < NUM_ROOMS; ++r)
        {
            const std::string n = std::to_string (r + 1);
            const std::string R = std::string (roomName[r]) + " ";
            v.push_back ({ "mat" + n, R + "ROOM WALLS",   wallDef[r], true, NUM_MATERIALS,     mats });
            v.push_back ({ "flr" + n, R + "ROOM FLOOR",   0.0f,       true, NUM_MATERIALS + 1, surfMats });
            v.push_back ({ "cel" + n, R + "ROOM CEILING", 0.0f,       true, NUM_MATERIALS + 1, surfMats });
            // the two walls of this room that carry no doorway can be broken in two
            for (int k = 0; k < 2; ++k)
            {
                const std::string kk = k == 0 ? "A" : "B";
                v.push_back ({ "fold" + n + (k == 0 ? "a" : "b"),    R + "FOLD " + kk,          0.5f, false, 0, "" });
                v.push_back ({ "foldp" + n + (k == 0 ? "a" : "b"),   R + "FOLD " + kk + " POS", 0.5f, false, 0, "" });
            }
        }`);

  rep(`    current.material[0] = nextChoice (NUM_MATERIALS);
    current.material[1] = nextChoice (NUM_MATERIALS);
    current.material[2] = nextChoice (NUM_MATERIALS);`,
`    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        current.material[r] = nextChoice (NUM_MATERIALS);
        // 0 means "as walls", which the engine spells -1
        current.floorMat[r] = nextChoice (NUM_MATERIALS + 1) - 1;
        current.ceilMat[r]  = nextChoice (NUM_MATERIALS + 1) - 1;
        for (int k = 0; k < 2; ++k)
        {
            current.breakPush[r][k]  = (next() - 0.5f) * 1.2f;     // +-0.6 m
            current.breakAlong[r][k] = 0.15f + next() * 0.7f;      // 0.15 .. 0.85 along the wall
        }
    }`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("host parameters: 47 -> 65");
