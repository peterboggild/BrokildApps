// Engine: the HRTF becomes swappable (a shared, immutable set), and a set can be
// loaded from a SOFA file (HrtfSofa.cpp). Validates, then writes.
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
const E = 'Engine.cpp', H = 'Engine.h';

// ---------------------------------------------------------------- the class
rep(H, `class Hrtf
{
public:
    void prepare (double fs);
    int  numTaps() const { return ntap; }
    // az: degrees, 0 front, 90 right; el: degrees, up positive.
    // Writes ntap taps for each ear and the ITD in samples (positive = right ear later).
    void lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const;
private:
    int ntap = 0;
    double fs = 48000.0;
    std::vector<float> taps;   // [dir][ear][ntap]
    std::vector<float> itd;    // [dir] in samples at fs
    void blendDir (int dir, float w, float* l, float* r, float& it) const;
};`, `class Hrtf
{
public:
    void prepare (double fs);                   // the built-in MIT KEMAR set
    /*  A PERSONAL set from a SOFA file (AES69), resampled to fs, sampled onto the
        same elevation rings as the built-in set but around the FULL circle (a
        real head is not symmetric, so nothing is mirrored), and diffuse-field
        equalised as the built-in set is, so the two can be compared by ear.
        Needs libmysofa (HrtfSofa.cpp); false with a reason when it fails. */
    bool loadSofa (const std::string& file, double fs, std::string& error);
    int  numTaps() const { return ntap; }
    // az: degrees, 0 front, 90 right; el: degrees, up positive.
    // Writes ntap taps for each ear and the ITD in samples (positive = right ear later).
    void lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const;
    const std::string& name() const { return label; }
    const std::string& file() const { return path; }
    bool personal() const { return full; }
private:
    int ntap = 0;
    double fs = 48000.0;
    std::vector<float> taps;   // [dir][ear][ntap]
    std::vector<float> itd;    // [dir] in samples at fs
    bool full = false;         // a loaded set: whole circles per ring
    std::vector<int> ringN, ringFirst;
    std::string label = "MIT KEMAR", path;
    void blendDir (int dir, float w, float* l, float* r, float& it) const;
};`);
rep(H, `#include <atomic>`, `#include <atomic>
#include <memory>
#include <string>`);
rep(H, `    int ntap = 128;
    Hrtf hrtf;`, `    int ntap = 128;
    std::shared_ptr<const Hrtf> hrtfSet;       // swapped whole, never edited in place
    void rebuildDiffuseHrtf();`);
rep(H, `    const RenderQuality& renderQuality() const { return quality; }`, `    const RenderQuality& renderQuality() const { return quality; }
    /*  Swap the head. Called on the audio thread between blocks (the processor
        hands the set over with a try-lock), or before rendering; the caller keeps
        the old set alive so nothing is freed here. prepare() keeps a personal set
        (the caller reloads it at a new rate) and otherwise builds MIT KEMAR. */
    void setHrtf (std::shared_ptr<const Hrtf> h);
    const Hrtf& hrtf() const { return *hrtfSet; }`);

// ---------------------------------------------------------------- lookup: whole circles for a loaded set
rep(E, `void Hrtf::lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const
{
    float az = std::fmod (azDeg, 360.0f); if (az < 0) az += 360.0f;
    bool mirror = false;
    if (az > 180.0f) { az = 360.0f - az; mirror = true; }`, `void Hrtf::lookup (float azDeg, float elDeg, float* left, float* right, float& itdSamples) const
{
    float az = std::fmod (azDeg, 360.0f); if (az < 0) az += 360.0f;
    if (full)
    {
        // a personal set: rings of the same elevations, azimuths all the way round
        const float el = std::max ((float) hrtfdata::RING_ELEV[0], std::min ((float) hrtfdata::RING_ELEV[hrtfdata::NRING - 1], elDeg));
        float rf = (el - hrtfdata::RING_ELEV[0]) / 10.0f;
        int r0 = (int) std::floor (rf); if (r0 >= hrtfdata::NRING - 1) r0 = hrtfdata::NRING - 2;
        const float wr = rf - r0;
        std::fill (left, left + ntap, 0.0f); std::fill (right, right + ntap, 0.0f);
        float it = 0;
        for (int k = 0; k < 2; ++k)
        {
            const int ring = r0 + k;
            const float wring = k == 0 ? 1.0f - wr : wr;
            if (wring <= 0) continue;
            const int n = ringN[(size_t) ring], first = ringFirst[(size_t) ring];
            if (n == 1) { blendDir (first, wring, left, right, it); continue; }
            const float step = 360.0f / (float) n;
            const float af = az / step;
            const int a0 = ((int) std::floor (af)) % n, a1 = (a0 + 1) % n;
            const float wa = af - std::floor (af);
            blendDir (first + a0, wring * (1.0f - wa), left, right, it);
            blendDir (first + a1, wring * wa,          left, right, it);
        }
        itdSamples = it;
        return;
    }
    bool mirror = false;
    if (az > 180.0f) { az = 360.0f - az; mirror = true; }`);

// ---------------------------------------------------------------- the engine holds a shared set
rep(E, `    hrtf.prepare (fs);
    ntap = hrtf.numTaps();`, `    if (hrtfSet == nullptr || ! hrtfSet->personal())
    {
        auto h = std::make_shared<Hrtf>();
        h->prepare (fs);
        hrtfSet = h;
    }
    ntap = hrtfSet->numTaps();`);
// slot buffers sized for the longest set any head may bring
rep(E, `        int n = 64; while (n < ntap + maxItd + SUB_BLOCK + 4 * FracDelay::TAPS) n <<= 1;
        s.hist.assign ((size_t) n, 0.0f); s.hmask = n - 1; s.hw = 0;
        s.zL.assign ((size_t) (SUB_BLOCK + maxItd + ntap + 4 * FracDelay::TAPS + 32), 0.0f);`,
`        int n = 64; while (n < MAX_TAPS + maxItd + SUB_BLOCK + 4 * FracDelay::TAPS) n <<= 1;
        s.hist.assign ((size_t) n, 0.0f); s.hmask = n - 1; s.hw = 0;
        s.zL.assign ((size_t) (SUB_BLOCK + maxItd + MAX_TAPS + 4 * FracDelay::TAPS + 32), 0.0f);`);
rep(E, `    // the eight fixed diffuse directions, ITD baked into the taps
    {
        std::vector<float> l ((size_t) ntap), rr ((size_t) ntap);`, `    rebuildDiffuseHrtf();

    geomStore.assign (sizeof (RoomGeom) * NUM_ROOMS, 0);
    {
        RoomGeom* g = reinterpret_cast<RoomGeom*> (geomStore.data());
        RoomBreaks flat;
        for (int r = 0; r < NUM_ROOMS; ++r) g[r] = buildRoomGeom (r, flat);
    }

    for (int r = 0; r < NUM_ROOMS; ++r) measureEfficiency (r);

    reset();
    paramsFresh = true;
    for (int s = 0; s < MAX_SOURCES; ++s) { lastSrcRoomAc[s] = -1; lastTypeAc[s] = -1; lastDirAc[s] = -1; lastActiveAc[s] = false; lastLevelAc[s] = -1; }
    updateRoomAcoustics (true);
}

void Engine::setHrtf (std::shared_ptr<const Hrtf> h)
{
    if (h == nullptr) return;
    hrtfSet = std::move (h);
    ntap = hrtfSet->numTaps();
    rebuildDiffuseHrtf();
}

// the eight fixed diffuse directions, ITD baked into the taps
void Engine::rebuildDiffuseHrtf()
{
    for (int r = 0; r < NUM_ROOMS; ++r)
        for (int j = 0; j < 8; ++j)
        {
            std::fill (rooms[(size_t) r].dL[(size_t) j].begin(), rooms[(size_t) r].dL[(size_t) j].end(), 0.0f);
            std::fill (rooms[(size_t) r].dR[(size_t) j].begin(), rooms[(size_t) r].dR[(size_t) j].end(), 0.0f);
        }
    {
        std::vector<float> l ((size_t) ntap), rr ((size_t) ntap);`);
rep(E, `            hrtf.lookup (az, el, l.data(), rr.data(), it);
            const int dl = it > 0 ? 0 : (int) std::lround (-it);`, `            hrtfSet->lookup (az, el, l.data(), rr.data(), it);
            const int dl = it > 0 ? 0 : (int) std::lround (-it);`);
rep(E, `        hrtf.lookup (sp.az, sp.el, tmpA.data(), tmpB.data(), it);`, `        hrtfSet->lookup (sp.az, sp.el, tmpA.data(), tmpB.data(), it);`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
