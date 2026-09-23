// The scene carries each room's plan polygon, so the panel draws the room the
// engine is actually rendering rather than a rectangle it assumes.
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
  rep(`    float rt[NUM_ROOMS][4] = {};          // seconds at 250, 1k, 4k, 8k`,
      `    float rt[NUM_ROOMS][4] = {};          // seconds at 250, 1k, 4k, 8k
    // each room's plan polygon, following the folds
    int   planN[NUM_ROOMS] = {};
    float planX[NUM_ROOMS][10] = {}, planY[NUM_ROOMS][10] = {};`);
});

edit("Source/Engine.cpp", rep => {
  rep(`    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        sc.rt[r][0] = rooms[(size_t) r].rt60[1]; sc.rt[r][1] = rooms[(size_t) r].rt60[3];
        sc.rt[r][2] = rooms[(size_t) r].rt60[5]; sc.rt[r][3] = rooms[(size_t) r].rt60[6];
    }`,
`    const RoomGeom* gg = geomStore.empty() ? nullptr : reinterpret_cast<const RoomGeom*> (geomStore.data());
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        sc.rt[r][0] = rooms[(size_t) r].rt60[1]; sc.rt[r][1] = rooms[(size_t) r].rt60[3];
        sc.rt[r][2] = rooms[(size_t) r].rt60[5]; sc.rt[r][3] = rooms[(size_t) r].rt60[6];
        sc.planN[r] = 0;
        if (gg != nullptr)
        {
            const int n = std::min (10, gg[r].np);
            for (int i = 0; i < n; ++i) { sc.planX[r][i] = gg[r].px[i]; sc.planY[r][i] = gg[r].py[i]; }
            sc.planN[r] = n;
        }
    }`);
});

edit("Source/PluginProcessor.cpp", rep => {
  rep(`    obj->setProperty ("rt", rt);`,
`    obj->setProperty ("rt", rt);

    juce::Array<juce::var> plan;
    for (int r = 0; r < NUM_ROOMS; ++r)
    {
        juce::Array<juce::var> poly;
        for (int i = 0; i < sc.planN[r]; ++i)
        {
            juce::Array<juce::var> pt; pt.add (sc.planX[r][i]); pt.add (sc.planY[r][i]);
            poly.add (juce::var (pt));
        }
        plan.add (juce::var (poly));
    }
    obj->setProperty ("plan", plan);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("scene carries the plan polygons");
