// Round 3: exact digital shelf fit; a flat-loss debug switch for the probe.
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
  rep(`    void setCoeffs (float fs);`, `    void setCoeffs (float fs);
    float fsHz = 48000.0f;`);
  rep(`    double dbgFieldEnergy[NUM_ROOMS] = { 0, 0, 0 }, dbgInjEnergy[NUM_ROOMS] = { 0, 0, 0 };`,
      `    double dbgFieldEnergy[NUM_ROOMS] = { 0, 0, 0 }, dbgInjEnergy[NUM_ROOMS] = { 0, 0, 0 };
    bool   dbgFlatLoss = false;     // every line loss = its 1 kHz value (a probe's switch)`);
});

edit("Source/Engine.cpp", rep => {
  rep(`void BandFilter::setCoeffs (float fs)
{
    auto coef = [fs] (float fc) { return 1.0f - std::exp (-2.0f * PI * fc / fs); };`,
      `void BandFilter::setCoeffs (float fs)
{
    fsHz = fs;
    auto coef = [fs] (float fc) { return 1.0f - std::exp (-2.0f * PI * fc / fs); };`);
  rep(`/*  A first-order shelf of HF gain d (dB) at frequency ratio r = f / fc:
    |H|^2 = ((1 + r^2 + k r^2)^2 + k^2 r^2) / (1 + r^2)^2, k = 10^(d/20) - 1 */
static float shelfDb (float d, float r)
{
    const float k = std::pow (10.0f, d * 0.05f) - 1.0f;
    const float r2 = r * r;
    const float num = (1.0f + r2 + k * r2) * (1.0f + r2 + k * r2) + k * k * r2;
    const float den = (1.0f + r2) * (1.0f + r2);
    return 10.0f * std::log10 (num / den);
}`,
`/*  The exact response of one digital shelf y = x + k (x - lp x), lp the one-pole
    s += a (x - s): H(z) = 1 + k (1 - a / (1 - (1 - a) z^-1)), in dB at f. */
static float shelfDb (float d, float a, float f, float fs)
{
    const float k = std::pow (10.0f, d * 0.05f) - 1.0f;
    const float w = 2.0f * PI * f / fs;
    const float cw = std::cos (w), sw = std::sin (w);
    // lp = a / (1 - b e^{-jw}), b = 1 - a
    const float b = 1.0f - a;
    const float dr = 1.0f - b * cw, di = b * sw;           // denominator
    const float den = dr * dr + di * di;
    const float lr = a * dr / den, li = -a * di / den;     // lp as a complex number
    const float hr = 1.0f + k * (1.0f - lr), hi = -k * li;
    return 10.0f * std::log10 (std::max (1e-12f, hr * hr + hi * hi));
}`);
  rep(`    const float t0 = db[1], t1 = db[3], t2 = db[5], t3 = db[6];
    static const float F1 = shelfDb (1.0f, 2.0f), F2 = shelfDb (1.0f, 2.0f), F3 = shelfDb (1.0f, 1.0f);
    float gd = t0, d1 = t1 - t0, d2 = t2 - t1, d3 = (t3 - t2) / F3;
    auto clampD = [] (float v) { return std::max (-48.0f, std::min (24.0f, v)); };
    for (int it = 0; it < 4; ++it)
    {
        auto total = [&] (float f) { return gd + shelfDb (d1, f / 500.0f) + shelfDb (d2, f / 2000.0f) + shelfDb (d3, f / 8000.0f); };`,
`    const float t0 = db[1], t1 = db[3], t2 = db[5], t3 = db[6];
    const float F1 = shelfDb (1.0f, a1, 1000.0f, fsHz), F2 = shelfDb (1.0f, a2, 4000.0f, fsHz), F3 = shelfDb (1.0f, a3, 8000.0f, fsHz);
    float gd = t0, d1 = t1 - t0, d2 = t2 - t1, d3 = (t3 - t2) / F3;
    auto clampD = [] (float v) { return std::max (-48.0f, std::min (24.0f, v)); };
    for (int it = 0; it < 5; ++it)
    {
        auto total = [&] (float f) { return gd + shelfDb (d1, a1, f, fsHz) + shelfDb (d2, a2, f, fsHz) + shelfDb (d3, a3, f, fsHz); };`);
  rep(`            for (int b = 0; b < NBAND; ++b) lossDb[b] = -60.0f * (float) F.len[(size_t) i] / ((float) fs * F.rt60[b]);
            F.loss[(size_t) i].setBandsDb (lossDb);`,
      `            for (int b = 0; b < NBAND; ++b) lossDb[b] = -60.0f * (float) F.len[(size_t) i] / ((float) fs * F.rt60[dbgFlatLoss ? 3 : b]);
            F.loss[(size_t) i].setBandsDb (lossDb);`);
});

edit("test/probe.cpp", rep => {
  rep(`    if (what == "filt")`, `    if (what == "rtflat")
    {
        // the network's decay with FLAT losses (the 1 kHz value everywhere) vs Eyring at 1 kHz
        for (int m = 0; m < NUM_MATERIALS; ++m)
        {
            Engine e; e.dbgFlatLoss = true; e.prepare (fs, 256);
            Params p; p.material[0] = p.material[1] = p.material[2] = m;
            p.door[0] = p.door[1] = p.door[2] = 0; p.srcType = 0;
            p.srcX = 2; p.srcY = 2; p.lisX = 4; p.lisY = 3; p.lisYaw = 180;
            p.directDb = -120; p.earlyDb = -120;
            const RoomField& F = e.field (0);
            std::vector<float> L, R; run (e, p, L, R, (int) ((F.rt60[3] * 2.5 + 1) * fs), 10);
            std::vector<double> edc (L.size(), 0.0); double acc = 0;
            for (int i = (int) L.size() - 1; i >= 0; --i) { acc += (double) L[(size_t) i] * L[(size_t) i]; edc[(size_t) i] = acc; }
            const int from = (int) (0.05 * fs); const double top = 10 * std::log10 (edc[(size_t) from]);
            int i5 = -1, i25 = -1;
            for (int i = from; i < (int) L.size(); ++i) { const double d = 10 * std::log10 (edc[(size_t) i]) - top; if (i5 < 0 && d <= -5) i5 = i; if (d <= -25) { i25 = i; break; } }
            std::printf ("%s flat-loss network: T20x3 = %.3f s, design %.3f s (%.0f %%)\\n", MATERIAL_NAMES[m], 3.0 * (i25 - i5) / fs, F.rt60[3], 100.0 * (3.0 * (i25 - i5) / fs / F.rt60[3] - 1));
        }
        return 0;
    }

    if (what == "filt")`);
});
if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("round 3 applied");
