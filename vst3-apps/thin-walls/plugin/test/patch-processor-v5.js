// Processor v5 (2026-09-25): wall pictures (storage, panels into the engine),
// the host beat clock, and light + beat per video frame. Validates, then writes.
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
const P = 'PluginProcessor.cpp', H = 'PluginProcessor.h', T = 'TakeExport.h';

// ---------------------------------------------------------------- the take carries panels and the beat
rep(T, `    struct FurnSnap { int sample = 0; int n = 0; FurnItem items[MAX_FURN]; };`,
       `    struct FurnSnap { int sample = 0; int n = 0; FurnItem items[MAX_FURN]; float panelArea[NUM_ROOMS] = { 0, 0, 0 }; };`);
rep(T, `    std::vector<float> params;                  // np raw values per grid point`,
       `    std::vector<float> params;                  // np raw values per grid point
    std::vector<float> beat;                    // 3 per grid point: ppq, bpm, playing (the host's clock)`);

// ---------------------------------------------------------------- header
rep(H, `    bool furnDirtyUi = false;                  // message thread: tell the page`,
`    bool furnDirtyUi = false;                  // message thread: tell the page
    float panelAreaLayout[tw::NUM_ROOMS] = { 0, 0, 0 };   // guarded by furnLock, like the furniture

    // ---- wall pictures: layout + images (base64 JPEG), message thread only
    struct Pic { juce::String id; int room = 0, wall = 0; float along = 0, z = 1.5f, w = 0.8f, aspect = 0.75f; int frame = 0, kind = 0; };
    std::vector<Pic> pics;
    std::map<juce::String, juce::String> picImages;
    bool picsDirtyUi = false;
    juce::var picsJson (bool withImages) const;
    void setPicsFromVar (const juce::var& items, const juce::var& images);

    // ---- the host's clock, for the lamps' beat lock
    std::atomic<double> hostPpq { 0.0 }, hostBpm { 0.0 };
    std::atomic<bool> hostPlaying { false };`);
rep(H, `#include "TakeExport.h"`, `#include "TakeExport.h"
#include <map>`);

// ---------------------------------------------------------------- processBlock: panels with the furniture, the beat
rep(P, `            for (int i = 0; i < MAX_FURN; ++i) current.furn[i] = furnLayout[i];
            current.nfurn = furnCount;`, `            for (int i = 0; i < MAX_FURN; ++i) current.furn[i] = furnLayout[i];
            current.nfurn = furnCount;
            for (int r = 0; r < NUM_ROOMS; ++r) current.panelArea[r] = panelAreaLayout[r];`);
rep(P, `    readParams();
    engine.setParams (current);

    auto main = getBusBuffer (buffer, true, 0);`, `    readParams();
    engine.setParams (current);

    // the host's clock (for the lamps' beat lock), when it has one
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostBpm = pos->getBpm().orFallback (0.0);
            hostPpq = pos->getPpqPosition().orFallback (0.0);
            hostPlaying = pos->getIsPlaying();
        }

    auto main = getBusBuffer (buffer, true, 0);`);
rep(P, `                    std::copy (rawNow.begin(), rawNow.end(), T.params.begin() + (size_t) b * (size_t) T.np);
                    T.nblocks = b + 1;`, `                    std::copy (rawNow.begin(), rawNow.end(), T.params.begin() + (size_t) b * (size_t) T.np);
                    if ((size_t) b * 3 + 2 < T.beat.size())
                    {
                        const double dp = hostBpm.load() / 60.0 * (double) (g - pos) / T.rate;   // the grid point's own ppq
                        T.beat[(size_t) b * 3]     = (float) (hostPpq.load() + (hostPlaying.load() ? dp : 0.0));
                        T.beat[(size_t) b * 3 + 1] = (float) hostBpm.load();
                        T.beat[(size_t) b * 3 + 2] = hostPlaying.load() ? 1.0f : 0.0f;
                    }
                    T.nblocks = b + 1;`);
rep(P, `                        snap.sample = pos; snap.n = current.nfurn;
                        for (int i = 0; i < MAX_FURN; ++i) snap.items[i] = current.furn[i];`, `                        snap.sample = pos; snap.n = current.nfurn;
                        for (int i = 0; i < MAX_FURN; ++i) snap.items[i] = current.furn[i];
                        for (int r = 0; r < NUM_ROOMS; ++r) snap.panelArea[r] = current.panelArea[r];`);

// the render job: panels, and the light at every frame
rep(P, `                    P.nfurn = T.furn[(size_t) fi].n;
                    for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = T.furn[(size_t) fi].items[i];`, `                    P.nfurn = T.furn[(size_t) fi].n;
                    for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = T.furn[(size_t) fi].items[i];
                    for (int r = 0; r < NUM_ROOMS; ++r) P.panelArea[r] = T.furn[(size_t) fi].panelArea[r];`);
rep(P, `                progress = (float) (s + m) / (float) std::max (1, n);`, `                progress = (float) (s + m) / (float) std::max (1, n);
                // the lamps' light at every video frame that falls in this block
                while (fps > 0 && (double) nextFrame / fps * T.rate < (double) (s + m))
                {
                    for (int r = 0; r < NUM_ROOMS; ++r) light.push_back (eng->lightLevel (r));
                    ++nextFrame;
                }`);
rep(P, `        std::atomic<bool> ok { false };
        explicit RenderJob`, `        std::atomic<bool> ok { false };
        int fps = 30, nextFrame = 0;
        std::vector<float> light;          // NUM_ROOMS per video frame
        explicit RenderJob`);
rep(P, `    renderJob = std::make_unique<RenderJob> (take);
    renderJob->startThread();`, `    {
        auto job = std::make_unique<RenderJob> (take);
        job->fps = vidFps;
        renderJob = std::move (job);
    }
    renderJob->startThread();`);
rep(P, `    plan->setProperty ("cam", cam);`, `    plan->setProperty ("cam", cam);
    // the lamps: the light the engine heard at each frame, and the host's beat clock
    {
        juce::Array<juce::var> lt, bt;
        for (int i = 0; i < vidN; ++i)
        {
            juce::Array<juce::var> row;
            for (int r = 0; r < NUM_ROOMS; ++r)
            {
                const size_t k = (size_t) i * NUM_ROOMS + (size_t) r;
                row.add (k < job->light.size() ? std::round (job->light[k] * 1000.0f) / 1000.0f : 0.0f);
            }
            lt.add (juce::var (row));
            const int b = juce::jlimit (0, std::max (0, nb - 1), (int) ((double) i / vidFps * T.rate / TakeData::PBLOCK));
            juce::Array<juce::var> br;
            const size_t kb = (size_t) b * 3;
            br.add (kb + 2 < T.beat.size() ? T.beat[kb] : 0.0f);
            br.add (kb + 2 < T.beat.size() ? T.beat[kb + 1] : 0.0f);
            br.add (kb + 2 < T.beat.size() ? T.beat[kb + 2] : 0.0f);
            bt.add (juce::var (br));
        }
        plan->setProperty ("light", lt);
        plan->setProperty ("beat", bt);
    }`);
// the job must stay alive until the plan is built (it is reset earlier today)
rep(P, `    vidPcmWritten = 0;
    renderJob.reset();
`, `    vidPcmWritten = 0;
    std::unique_ptr<juce::Thread> jobHold = std::move (renderJob);   // its light is read below
`);
rep(P, `    t->params.assign ((size_t) (t->capacity / TakeData::PBLOCK + 2) * (size_t) t->np, 0.0f);`,
       `    t->params.assign ((size_t) (t->capacity / TakeData::PBLOCK + 2) * (size_t) t->np, 0.0f);
    t->beat.assign ((size_t) (t->capacity / TakeData::PBLOCK + 2) * 3, 0.0f);`);

// ---------------------------------------------------------------- scene: light and beat
rep(P, `    obj->setProperty ("pathsDropped", sc.pathsDropped);`, `    obj->setProperty ("pathsDropped", sc.pathsDropped);
    {
        juce::Array<juce::var> lt;
        for (int r = 0; r < NUM_ROOMS; ++r) lt.add (sc.light[r]);
        obj->setProperty ("light", lt);
        auto* b = new juce::DynamicObject();
        b->setProperty ("bpm", hostBpm.load()); b->setProperty ("ppq", hostPpq.load());
        b->setProperty ("playing", hostPlaying.load() ? 1 : 0);
        obj->setProperty ("beat", juce::var (b));
    }`);

// ---------------------------------------------------------------- pictures: state, presets, messages
rep(P, `        xml->setAttribute ("furn", juce::JSON::toString (furnJson(), true));`, `        xml->setAttribute ("furn", juce::JSON::toString (furnJson(), true));
        xml->setAttribute ("pics", juce::JSON::toString (picsJson (true), true));`);
rep(P, `    setFurnFromVar (juce::JSON::parse (xml->getStringAttribute ("furn", "[]")));
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn" }) xml->removeAttribute (a);`,
`    setFurnFromVar (juce::JSON::parse (xml->getStringAttribute ("furn", "[]")));
    {
        const juce::var pv = juce::JSON::parse (xml->getStringAttribute ("pics", "{}"));
        setPicsFromVar (pv.getProperty ("items", {}), pv.getProperty ("images", {}));
    }
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn", "pics" }) xml->removeAttribute (a);`);
rep(P, `    obj->setProperty ("furn", furnJson());
    return juce::var (obj);
}`, `    obj->setProperty ("furn", furnJson());
    obj->setProperty ("pics", picsJson (true));
    return juce::var (obj);
}`);
rep(P, `    if (v.hasProperty ("furn")) setFurnFromVar (v.getProperty ("furn", {}));
    uiHasState = false;
}`, `    if (v.hasProperty ("furn")) setFurnFromVar (v.getProperty ("furn", {}));
    if (v.hasProperty ("pics"))
    {
        const juce::var pv = v.getProperty ("pics", {});
        setPicsFromVar (pv.getProperty ("items", {}), pv.getProperty ("images", {}));
    }
    uiHasState = false;
}

//==============================================================================
juce::var ThinWallsAudioProcessor::picsJson (bool withImages) const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> items;
    for (const auto& p : pics)
    {
        auto* it = new juce::DynamicObject();
        it->setProperty ("id", p.id); it->setProperty ("room", p.room); it->setProperty ("wall", p.wall);
        it->setProperty ("along", p.along); it->setProperty ("z", p.z); it->setProperty ("w", p.w);
        it->setProperty ("aspect", p.aspect); it->setProperty ("frame", p.frame); it->setProperty ("kind", p.kind);
        items.add (juce::var (it));
    }
    o->setProperty ("items", items);
    if (withImages)
    {
        auto* im = new juce::DynamicObject();
        for (const auto& kv : picImages) im->setProperty (juce::Identifier (kv.first), kv.second);
        o->setProperty ("images", juce::var (im));
    }
    return juce::var (o);
}

/*  The pictures' layout (and, when given, their images). A PRINT is visual only;
    an ACOUSTIC PANEL's face area joins its room's absorption through the same
    lock and version the furniture uses, so the audio thread picks it up the
    same way. Images no picture uses any more are dropped. */
void ThinWallsAudioProcessor::setPicsFromVar (const juce::var& items, const juce::var& images)
{
    if (auto* im = images.getDynamicObject())
        for (const auto& nv : im->getProperties())
        {
            const juce::String data = nv.value.toString();
            if (data.length() > 0 && data.length() < 3000000 && picImages.size() < 16) picImages[nv.name.toString()] = data;
        }
    std::vector<Pic> next;
    if (const auto* arr = items.getArray())
        for (const auto& v : *arr)
        {
            if (next.size() >= 8) break;
            Pic p;
            p.id = v.getProperty ("id", {}).toString();
            if (p.id.isEmpty()) continue;
            p.room = juce::jlimit (0, NUM_ROOMS - 1, (int) v.getProperty ("room", 0));
            p.wall = juce::jlimit (0, 3, (int) v.getProperty ("wall", 0));
            p.along = (float) (double) v.getProperty ("along", 0.0);
            p.z = juce::jlimit (0.1f, 4.9f, (float) (double) v.getProperty ("z", 1.5));
            p.w = juce::jlimit (0.1f, 4.0f, (float) (double) v.getProperty ("w", 0.8));
            p.aspect = juce::jlimit (0.1f, 10.0f, (float) (double) v.getProperty ("aspect", 0.75));
            p.frame = juce::jlimit (0, 3, (int) v.getProperty ("frame", 0));
            p.kind = juce::jlimit (0, 1, (int) v.getProperty ("kind", 0));
            next.push_back (p);
        }
    pics = next;
    for (auto it = picImages.begin(); it != picImages.end();)
    {
        bool used = false;
        for (const auto& p : pics) if (p.id == it->first) { used = true; break; }
        it = used ? std::next (it) : picImages.erase (it);
    }
    float area[NUM_ROOMS] = { 0, 0, 0 };
    for (const auto& p : pics) if (p.kind == 1) area[p.room] += p.w * p.w * p.aspect;
    {
        const juce::SpinLock::ScopedLockType sl (furnLock);
        for (int r = 0; r < NUM_ROOMS; ++r) panelAreaLayout[r] = area[r];
        ++furnVersion;
    }
}`);
rep(P, `        setFurnFromVar (juce::var (juce::Array<juce::var>()));
        uiHasState = false;
        return;
    }`, `        setFurnFromVar (juce::var (juce::Array<juce::var>()));
        setPicsFromVar (juce::var (juce::Array<juce::var>()), {});
        uiHasState = false;
        return;
    }

    if (k == "picAdd")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        const juce::String jpg = payload.getProperty ("jpg", {}).toString();
        if (id.isNotEmpty() && jpg.isNotEmpty() && jpg.length() < 3000000 && picImages.size() < 16) picImages[id] = jpg;
        return;
    }
    if (k == "pics") { setPicsFromVar (payload.getProperty ("items", {}), {}); return; }`);
rep(P, `        obj->setProperty ("furn", furnJson());
    }
    furnDirtyUi = false;`, `        obj->setProperty ("furn", furnJson());
    }
    {
        const juce::var pv = picsJson (true);
        obj->setProperty ("pics", pv.getProperty ("items", {}));
        obj->setProperty ("picImages", pv.getProperty ("images", {}));
    }
    furnDirtyUi = false;
    picsDirtyUi = false;`);
rep(P, `    // a layout the page did not make (preset, project) goes to the page
    if (furnDirtyUi)
    {`, `    if (picsDirtyUi)
    {
        picsDirtyUi = false;
        emitToUi ("pics", picsJson (true));
    }
    // a layout the page did not make (preset, project) goes to the page
    if (furnDirtyUi)
    {`);
rep(P, `{ applyPatchJson (v); furnDirtyUi = true; text = "loaded " + f.getFileName(); }`,
       `{ applyPatchJson (v); furnDirtyUi = true; picsDirtyUi = true; text = "loaded " + f.getFileName(); }`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
