/*  The calibration is a pure function of the room and the sample rate - the delay
    lengths are deterministic and the chain sizing rule is fixed - so it is
    computed once and remembered. Without that, every Engine pays for it: the
    bench went from a minute to over ten, and a host calling prepareToPlay would
    have waited seconds.

    Also shortened: the run only has to reach -25 dB for the decay and, for the
    level, 0.75 of a decay already holds 99.997 % of the energy of an exponential.
    The five-second point cost thirteen seconds of simulation and now costs four.
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

edit("Source/Engine.cpp", rep => {
  rep(`#include <algorithm>
#include <cstring>`,
`#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>`);

  rep(`void Engine::measureEfficiency (int r)
{
    RoomField& FF = rooms[(size_t) r];`,
`/*  Remembered per room and sample rate: the delay lengths are deterministic and
    the sizing rule is fixed, so the answer cannot differ between two Engines at
    the same rate. */
namespace
{
    struct FieldCal { float eff[RoomField::NCAL]; float trim[RoomField::NCAL]; };
    std::map<std::pair<int, int>, FieldCal> g_calCache;
    std::mutex g_calMutex;
}

void Engine::measureEfficiency (int r)
{
    RoomField& FF = rooms[(size_t) r];
    const std::pair<int, int> key { r, (int) std::lround (fs) };
    {
        std::lock_guard<std::mutex> lock (g_calMutex);
        const auto it = g_calCache.find (key);
        if (it != g_calCache.end())
        {
            for (int c = 0; c < RoomField::NCAL; ++c) { FF.calEff[c] = it->second.eff[c]; FF.calTrim[c] = it->second.trim[c]; }
            FF.efficiency = FF.calEff[1];
            FF.sizeDiffusers (fs, 1.0f);
            for (int i = 0; i < RoomField::N; ++i) { FF.line[(size_t) i].clear(); FF.loss[(size_t) i].reset(); }
            for (size_t q = 0; q < FF.ap.size(); ++q) { std::fill (FF.ap[q].begin(), FF.ap[q].end(), 0.0f); FF.apW[q] = 0; }
            return;
        }
    }`);

  // a shorter run: -25 dB is all the decay needs, and 0.75 RT holds the energy
  rep(`        const int total = (int) ((2.6 * RTc + 0.15) * fs);`,
      `        // -25 dB for the decay, and 0.75 of a decay already holds 99.997 % of
        // an exponential's energy, so there is nothing to gain past it
        const int total = (int) ((0.8 * RTc + 0.20) * fs);`);

  rep(`    FF.efficiency = FF.calEff[1];
    return;
}`,
`    {
        std::lock_guard<std::mutex> lock (g_calMutex);
        FieldCal fc;
        for (int c = 0; c < RoomField::NCAL; ++c) { fc.eff[c] = FF.calEff[c]; fc.trim[c] = FF.calTrim[c]; }
        g_calCache[key] = fc;
    }
    FF.efficiency = FF.calEff[1];
    return;
}`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("the calibration is computed once per room and rate");
