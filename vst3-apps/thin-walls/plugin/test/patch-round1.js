// Round 1 fixes after the first bench run. Exact-count anchors; nothing written on a miss.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => {
    const c = s.split(a).length - 1;
    if (c !== n) { misses.push(rel + ": " + c + " matches for: " + a.slice(0, 80)); return; }
    s = s.split(a).join(b);
  };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

// ---- Engine.cpp
edit("Source/Engine.cpp", rep => {
  // 1. the read must advance within the block
  rep("        float x = feed.read (std::min ((float) maxDelay, std::max (1.0f, p.delay)));",
      "        const float back = (float) (n - 1 - i);      // the whole block is already written\n        float x = feed.read (std::min ((float) maxDelay, std::max (1.0f, p.delay + back)));");
  rep("            const float xb = feed.read (std::min ((float) maxDelay, std::max (1.0f, p.delayB)));",
      "            const float xb = feed.read (std::min ((float) maxDelay, std::max (1.0f, p.delayB + back)));");

  // 2. exact four-anchor shelf fit
  rep(`void BandFilter::setBandsDb (const float* db)
{
    // gain at 250, shelves carrying the change to 1 k, 4 k, and the slope beyond 8 k
    g  = dbToLin (db[1]);
    k1 = dbToLin (db[3] - db[1]) - 1.0f;
    k2 = dbToLin (db[5] - db[3]) - 1.0f;
    k3 = dbToLin (std::max (-40.0f, std::min (12.0f, 2.0f * (db[6] - db[5])))) - 1.0f;
}`,
`/*  A first-order shelf of HF gain d (dB) at frequency ratio r = f / fc:
    |H|^2 = ((1 + r^2 + k r^2)^2 + k^2 r^2) / (1 + r^2)^2, k = 10^(d/20) - 1 */
static float shelfDb (float d, float r)
{
    const float k = std::pow (10.0f, d * 0.05f) - 1.0f;
    const float r2 = r * r;
    const float num = (1.0f + r2 + k * r2) * (1.0f + r2 + k * r2) + k * k * r2;
    const float den = (1.0f + r2) * (1.0f + r2);
    return 10.0f * std::log10 (num / den);
}

void BandFilter::setBandsDb (const float* db)
{
    // the four anchors: 250 Hz sets the gain, then 1 k, 4 k and 8 k are hit
    // EXACTLY by the three shelves (a plain first-order shelf only realises
    // ~80 % of its gain one octave above its corner, so the fit is iterated)
    const float t0 = db[1], t1 = db[3], t2 = db[5], t3 = db[6];
    static const float F1 = shelfDb (1.0f, 2.0f), F2 = shelfDb (1.0f, 2.0f), F3 = shelfDb (1.0f, 1.0f);
    float gd = t0, d1 = t1 - t0, d2 = t2 - t1, d3 = (t3 - t2) / F3;
    auto clampD = [] (float v) { return std::max (-48.0f, std::min (24.0f, v)); };
    for (int it = 0; it < 4; ++it)
    {
        auto total = [&] (float f) { return gd + shelfDb (d1, f / 500.0f) + shelfDb (d2, f / 2000.0f) + shelfDb (d3, f / 8000.0f); };
        gd = t0 - (total (250.0f) - gd);
        d1 = clampD (d1 + (t1 - total (1000.0f)) / F1);
        d2 = clampD (d2 + (t2 - total (4000.0f)) / F2);
        d3 = clampD (d3 + (t3 - total (8000.0f)) / F3);
    }
    g  = dbToLin (gd);
    k1 = dbToLin (d1) - 1.0f;
    k2 = dbToLin (d2) - 1.0f;
    k3 = dbToLin (d3) - 1.0f;
}`);

  // 3. mix / output snap on the first parameter set
  rep("        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] = p.door[d];\n        paramsFresh = false;",
      "        for (int d = 0; d < NUM_DOORS; ++d) doorNow[d] = p.door[d];\n        mixNow = p.mix; trimOut = dbToLin (p.outputDb);\n        paramsFresh = false;");

  // 4. loop-free coupling: a directed graph rooted at the source's room
  rep("    if (lastTypeAc != cur.srcType) { lastTypeAc = cur.srcType; changed = true; }\n    if (! changed) return;",
      "    if (lastTypeAc != cur.srcType) { lastTypeAc = cur.srcType; changed = true; }\n    const int rs = std::max (0, roomOf (srcPos.x, srcPos.y));\n    if (lastSrcRoomAc != rs) { lastSrcRoomAc = rs; changed = true; }\n    if (! changed) return;");
  rep(`void Engine::rebuildCouplings()
{
    for (auto& c : couplings)
    {`,
`/*  Two coupled feedback networks with long decays have modal peaks tens of dB
    above their average gain, and at a coincident peak the loop between them
    exceeds unity however small the energy-balance coefficient is - that is the
    measured failure of the first build (a PLASTER hall ran away at +40 dB/s).
    So the energy flows one way only: from the source's room into its
    neighbours, and from the better-connected neighbour into the third room,
    never back. The flow back into the source room is a second-order term in
    real coupled rooms too. */
void Engine::rebuildCouplings()
{
    const int rs = lastSrcRoomAc < 0 ? 0 : lastSrcRoomAc;
    auto tauAt1k = [&] (int a, int b)
    {
        float t = 0;
        for (int d = 0; d < NUM_DOORS; ++d)
            if ((DOORS[d].roomA == a && DOORS[d].roomB == b) || (DOORS[d].roomB == a && DOORS[d].roomA == b)) t += doorTau[d][3];
        return t;
    };
    couplings.clear();
    int q[2]; int nq = 0;
    for (int r = 0; r < NUM_ROOMS; ++r) if (r != rs) q[nq++] = r;
    auto add = [&] (int from, int to) { Coupling c; c.from = from; c.to = to; c.filt.setCoeffs ((float) fs); couplings.push_back (c); };
    add (rs, q[0]); add (rs, q[1]);
    if (tauAt1k (rs, q[0]) >= tauAt1k (rs, q[1])) add (q[0], q[1]); else add (q[1], q[0]);

    for (auto& c : couplings)
    {`);
});

// ---- Engine.h
edit("Source/Engine.h", rep => {
  rep("    int   lastTypeAc = -1;", "    int   lastTypeAc = -1;\n    int   lastSrcRoomAc = -1;");
});

// ---- probe.cpp: do not read past the take
edit("test/probe.cpp", rep => {
  rep("        int first = -1; for (int i = 0; i < 4000; ++i)", "        int first = -1; for (int i = 0; i < (int) L.size(); ++i)");
  rep("        for (int i = std::max (0, first - 2); i < first + 60; ++i)", "        for (int i = std::max (0, first - 2); i < std::min ((int) L.size(), first + 60); ++i)");
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("round 1 applied");
