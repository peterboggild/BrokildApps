// Engine: a RenderQuality chosen at prepare (reflection order, path and slot
// budgets). The default is exactly the live engine. Validates, then writes.
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

rep(H, `constexpr float PATH_FADE_S = 0.025f;`, `constexpr float PATH_FADE_S = 0.025f;

/*  How hard the engine may work. The DEFAULT is the live plug-in, exactly: the
    offline render of a take (the video export, the bounce) may ask for more -
    image sources to a higher order, and path and slot budgets to carry them.
    Broken rooms stay at second order (their general search grows as surfaces
    to the power of the order). */
struct RenderQuality
{
    int order = 2;              // image-source order in the listener's room
    int maxPaths = 192;         // = MAX_PATHS
    int maxSlots = 256;         // = MAX_SLOTS
};`);
rep(H, `    void prepare (double sampleRate, int maxBlock);`,
       `    void prepare (double sampleRate, int maxBlock, const RenderQuality& q = RenderQuality());
    // the identity of the image (nx, ny, nz) - unique to order 7 either side
    static uint32_t imageKey (int nx, int ny, int nz) { return (uint32_t) ((nx + 8) * 289 + (ny + 8) * 17 + (nz + 8)); }
    const RenderQuality& renderQuality() const { return quality; }`);
rep(H, `    std::array<PathSlot, MAX_SLOTS> slots;`, `    RenderQuality quality;
    std::vector<PathSlot> slots;           // quality.maxSlots, sized at prepare`);
rep(H, `    bool pathsFull (int margin) { if (nspecs < MAX_PATHS - margin) return false; ++pathsDropped; return true; }`,
       `    bool pathsFull (int margin) { if (nspecs < quality.maxPaths - margin) return false; ++pathsDropped; return true; }`);
rep(H, `    std::array<PathSpec, MAX_PATHS> specs; int nspecs = 0;`, `    std::vector<PathSpec> specs; int nspecs = 0;   // quality.maxPaths, sized at prepare`);

rep(E, `void Engine::prepare (double sampleRate, int maxBlockSize)
{`, `void Engine::prepare (double sampleRate, int maxBlockSize, const RenderQuality& q)
{
    quality = q;
    quality.order = std::max (1, std::min (7, quality.order));
    quality.maxPaths = std::max (64, quality.maxPaths);
    quality.maxSlots = std::max (quality.maxPaths + 32, quality.maxSlots);
    slots.assign ((size_t) quality.maxSlots, PathSlot());
    specs.assign ((size_t) quality.maxPaths, PathSpec());`);
rep(E, `                    s.key = keyBase + (uint32_t) ((nx + 2) * 25 + (ny + 2) * 5 + (nz + 2));`,
       `                    s.key = keyBase + imageKey (nx, ny, nz);`);
rep(E, `                        s.key = (uint32_t) ((nx + 2) * 25 + (ny + 2) * 5 + (nz + 2));`,
       `                        s.key = imageKey (nx, ny, nz);`);
rep(E, `        if (rs >= 0 && rs == rl) addImagePaths (rs, S, lisPos, 2, 0u);`,
       `        if (rs >= 0 && rs == rl) addImagePaths (rs, S, lisPos, quality.order, 0u);`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
