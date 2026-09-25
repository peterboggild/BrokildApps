// Processor v4 (2026-09-25): the furniture layout as project/preset state, the
// take recorder, the offline render and the MP4 export. Validates every anchor,
// then writes.
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
const P = 'PluginProcessor.cpp', H = 'PluginProcessor.h';
const RENDERJOB = "namespace\n{\n    /*  Renders a take through a fresh engine, block by block on the take's own\n        parameter grid, off the message thread. Nothing here touches the live\n        engine, so the plug-in keeps playing while an export renders. */\n    struct RenderJob : juce::Thread\n    {\n        std::shared_ptr<TakeData> take;\n        juce::AudioBuffer<float> out;\n        std::atomic<float> progress { 0.0f };\n        std::atomic<bool> ok { false };\n        explicit RenderJob (std::shared_ptr<TakeData> t) : juce::Thread (\"Thin Walls take render\"), take (std::move (t)) {}\n        void run() override\n        {\n            const TakeData& T = *take;\n            const int n = T.length.load();\n            out.setSize (2, std::max (1, n));\n            out.clear();\n            auto eng = std::make_unique<Engine>();\n            eng->prepare (T.rate, TakeData::PBLOCK);\n            const int nb = T.nblocks.load(), nf = T.nfurn.load();\n            int fi = -1;\n            Params P;\n            for (int s = 0; s < n; s += TakeData::PBLOCK)\n            {\n                if (threadShouldExit()) return;\n                const int m = std::min (TakeData::PBLOCK, n - s);\n                const int b = std::min (s / TakeData::PBLOCK, nb - 1);\n                if (b >= 0) rawToParams (&T.params[(size_t) b * (size_t) T.np], P);\n                while (fi + 1 < nf && T.furn[(size_t) (fi + 1)].sample <= s) ++fi;\n                if (fi >= 0)\n                {\n                    P.nfurn = T.furn[(size_t) fi].n;\n                    for (int i = 0; i < MAX_FURN; ++i) P.furn[i] = T.furn[(size_t) fi].items[i];\n                }\n                eng->setParams (P);\n                eng->process (T.input.getReadPointer (0, s), T.input.getReadPointer (1, s),\n                              T.aux ? T.input.getReadPointer (2, s) : nullptr, T.aux ? T.input.getReadPointer (3, s) : nullptr,\n                              out.getWritePointer (0, s), out.getWritePointer (1, s), m);\n                progress = (float) (s + m) / (float) std::max (1, n);\n            }\n            ok = true;\n        }\n    };\n}\n";

// ================================================================ header
rep(H, '#include <JuceHeader.h>', '#include <JuceHeader.h>\n#include "TakeExport.h"');
rep(H, `    double hostRate = 48000.0;
    int statePushTick = 0;
    bool auxWasConnected = false;`,
`    double hostRate = 48000.0;
    int statePushTick = 0;
    bool auxWasConnected = false;

    // the raw parameter values of this block, in table order (no allocation on the audio thread)
    std::vector<float> rawNow;

    // ---- furniture: project state, edited by the page, copied to the engine each block
    juce::SpinLock furnLock;
    tw::FurnItem furnLayout[tw::MAX_FURN];
    int furnCount = 0;
    std::atomic<int> furnVersion { 0 };
    int furnVersionAudio = -1;                 // audio thread: what it last copied
    bool furnDirtyUi = false;                  // message thread: tell the page
    juce::var furnJson() const;
    void setFurnFromVar (const juce::var& items);

    // ---- the take and its export
    juce::SpinLock recLock;                    // the audio thread only TRIES it
    std::shared_ptr<tw::TakeData> take;
    tw::TakeData* recTarget = nullptr;         // non-null while recording (guarded by recLock)
    int recFurnVersion = -1;                   // audio thread
    std::atomic<bool> recCapped { false };
    juce::String recState { "idle" }, recText, lastFile;
    double recProgress = 0;
    bool recDirty = true;
    int recTick = 0;
    void startRecording();
    void stopRecording (const juce::String& why);
    void emitRec();
    void beginExport (int w, int h, int fps);
    void sendPlan();
    void finishExport();
    void failExport (const juce::String& why);
    std::unique_ptr<juce::Thread> renderJob;
    std::unique_ptr<tw::Mp4Writer> mp4;
    juce::File mp4File;
    int vidW = 0, vidH = 0, vidFps = 30, vidN = 0, vidNext = 0;
    std::vector<int16_t> vidPcm;               // interleaved stereo at vidPcmRate
    int vidPcmRate = 48000;
    juce::int64 vidPcmWritten = 0;`);

// ================================================================ readParams -> rawToParams
load(P);
{
  const s = files[P];
  const a = s.indexOf('void ThinWallsAudioProcessor::readParams()\n{');
  const endMark = '    current.earSpan  = 0.15f + next() * 0.85f;\n}';
  const b = s.indexOf(endMark);
  if (a < 0 || b < 0 || b < a) miss.push('readParams body');
  else {
    let body = s.slice(a, b + endMark.length);
    const nLoads = body.split('paramPtr[k++]->load()').length - 1;
    if (nLoads !== 2) miss.push('readParams: expected 2 paramPtr loads, found ' + nLoads);
    body = body.split('paramPtr[k++]->load()').join('raw[k++]');
    body = body.replace('void ThinWallsAudioProcessor::readParams()\n{',
      '/*  Raw host values, in table order, to engine units. Shared by the live\n' +
      '    processBlock and the offline take render, so a take replays through\n' +
      '    exactly the arithmetic that played it. */\n' +
      'static void rawToParams (const float* raw, tw::Params& current)\n{');
    body += '\n\n' + RENDERJOB + '\n\nvoid ThinWallsAudioProcessor::readParams()\n{\n' +
            '    for (size_t i = 0; i < paramPtr.size(); ++i) rawNow[i] = paramPtr[i]->load();\n' +
            '    rawToParams (rawNow.data(), current);\n}';
    files[P] = s.slice(0, a) + body + s.slice(b + endMark.length);
  }
}

// constructor / destructor
rep(P, `    for (size_t i = 0; i < specs.size(); ++i)
        paramPtr[i] = apvts.getRawParameterValue (specs[i].id);`,
`    for (size_t i = 0; i < specs.size(); ++i)
        paramPtr[i] = apvts.getRawParameterValue (specs[i].id);
    rawNow.assign (specs.size(), 0.0f);`);
rep(P, 'ThinWallsAudioProcessor::~ThinWallsAudioProcessor() = default;',
`ThinWallsAudioProcessor::~ThinWallsAudioProcessor()
{
    stopTimer();
    if (renderJob != nullptr) renderJob->stopThread (4000);
    if (mp4 != nullptr) mp4->abandon();
}`);

// ================================================================ processBlock
rep(P, `    readParams();
    engine.setParams (current);

    auto main = getBusBuffer (buffer, true, 0);`,
`    // the furniture layout, if the page changed it (never wait for the lock here)
    if (furnVersion.load() != furnVersionAudio)
    {
        const juce::SpinLock::ScopedTryLockType tl (furnLock);
        if (tl.isLocked())
        {
            for (int i = 0; i < MAX_FURN; ++i) current.furn[i] = furnLayout[i];
            current.nfurn = furnCount;
            furnVersionAudio = furnVersion.load();
        }
    }
    readParams();
    engine.setParams (current);

    auto main = getBusBuffer (buffer, true, 0);`);
rep(P, `    // the engine writes the main output over the main input; the aux bus is read only
    float* oL = buffer.getWritePointer (0);`,
`    // the take: what the engine is about to hear, and everything that shapes it
    {
        const juce::SpinLock::ScopedTryLockType tl (recLock);
        if (tl.isLocked() && recTarget != nullptr)
        {
            TakeData& T = *recTarget;
            const int pos = T.length.load();
            const int m = std::min (n, T.capacity - pos);
            if (m > 0)
            {
                T.input.copyFrom (0, pos, mL, m);
                T.input.copyFrom (1, pos, mR, m);
                if (T.aux)
                {
                    if (aL != nullptr) { T.input.copyFrom (2, pos, aL, m); T.input.copyFrom (3, pos, aR, m); }
                    else { T.input.clear (2, pos, m); T.input.clear (3, pos, m); }
                }
                const int maxBlocks = (int) (T.params.size() / (size_t) T.np);
                for (int g = ((pos + TakeData::PBLOCK - 1) / TakeData::PBLOCK) * TakeData::PBLOCK; g < pos + m; g += TakeData::PBLOCK)
                {
                    const int b = g / TakeData::PBLOCK;
                    if (b >= maxBlocks) break;
                    std::copy (rawNow.begin(), rawNow.end(), T.params.begin() + (size_t) b * (size_t) T.np);
                    T.nblocks = b + 1;
                }
                if (furnVersionAudio != recFurnVersion)
                {
                    const int fi = T.nfurn.load();
                    if (fi < (int) T.furn.size())
                    {
                        auto& snap = T.furn[(size_t) fi];
                        snap.sample = pos; snap.n = current.nfurn;
                        for (int i = 0; i < MAX_FURN; ++i) snap.items[i] = current.furn[i];
                        T.nfurn = fi + 1;
                    }
                    recFurnVersion = furnVersionAudio;
                }
                T.length = pos + m;
            }
            if (pos + m >= T.capacity) { recTarget = nullptr; recCapped = true; }
        }
    }

    // the engine writes the main output over the main input; the aux bus is read only
    float* oL = buffer.getWritePointer (0);`);

// ================================================================ state
rep(P, `        xml->setAttribute ("wavgain", (double) wavGain.load());`,
`        xml->setAttribute ("wavgain", (double) wavGain.load());
        xml->setAttribute ("furn", juce::JSON::toString (furnJson(), true));`);
rep(P, `    const juce::String wavPath = xml->getStringAttribute ("wavpath", {});
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath" }) xml->removeAttribute (a);`,
`    const juce::String wavPath = xml->getStringAttribute ("wavpath", {});
    // a project from before furniture has none: an empty apartment
    setFurnFromVar (juce::JSON::parse (xml->getStringAttribute ("furn", "[]")));
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn" }) xml->removeAttribute (a);`);
rep(P, `    obj->setProperty ("showrays", showRays);
    return juce::var (obj);
}`, `    obj->setProperty ("showrays", showRays);
    obj->setProperty ("furn", furnJson());
    return juce::var (obj);
}`);
rep(P, `    if (v.hasProperty ("showrays")) showRays = (int) v.getProperty ("showrays", 1);
    uiHasState = false;
}`, `    if (v.hasProperty ("showrays")) showRays = (int) v.getProperty ("showrays", 1);
    // a placement saved before furniture existed leaves the furniture alone
    if (v.hasProperty ("furn")) setFurnFromVar (v.getProperty ("furn", {}));
    uiHasState = false;
}

//==============================================================================
juce::var ThinWallsAudioProcessor::furnJson() const
{
    juce::Array<juce::var> a;
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (furnLock));
    for (int i = 0; i < furnCount; ++i)
    {
        const FurnItem& it = furnLayout[i];
        if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", juce::String (FURN[it.type].id));
        o->setProperty ("x", it.x); o->setProperty ("y", it.y); o->setProperty ("yaw", it.yaw);
        a.add (juce::var (o));
    }
    return juce::var (a);
}

void ThinWallsAudioProcessor::setFurnFromVar (const juce::var& items)
{
    FurnItem next[MAX_FURN]; int n = 0;
    if (const auto* arr = items.getArray())
        for (const auto& v : *arr)
        {
            if (n >= MAX_FURN) break;
            const juce::String id = v.getProperty ("t", {}).toString();
            int type = -1;
            for (int t = 0; t < NUM_FURN_TYPES; ++t) if (id == FURN[t].id) { type = t; break; }
            if (type < 0) continue;
            next[n].type = type;
            next[n].x = juce::jlimit (0.0f, APARTMENT_W, (float) (double) v.getProperty ("x", 0.0));
            next[n].y = juce::jlimit (0.0f, APARTMENT_D, (float) (double) v.getProperty ("y", 0.0));
            next[n].yaw = (float) std::fmod ((double) v.getProperty ("yaw", 0.0) + 3600.0, 360.0);
            ++n;
        }
    {
        const juce::SpinLock::ScopedLockType sl (furnLock);
        for (int i = 0; i < MAX_FURN; ++i) furnLayout[i] = i < n ? next[i] : FurnItem();
        furnCount = n;
        ++furnVersion;
    }
}`);

// ================================================================ messages
rep(P, `    if (k == "presetDefault")
    {
        for (const auto& s : twSpecs())
            if (auto* prm = apvts.getParameter (s.id)) prm->setValueNotifyingHost (prm->getDefaultValue());
        uiHasState = false;
        return;
    }`, `    if (k == "presetDefault")
    {
        for (const auto& s : twSpecs())
            if (auto* prm = apvts.getParameter (s.id)) prm->setValueNotifyingHost (prm->getDefaultValue());
        setFurnFromVar (juce::var (juce::Array<juce::var>()));
        uiHasState = false;
        return;
    }

    if (k == "furn") { setFurnFromVar (payload.getProperty ("items", {})); return; }

    if (k == "recStart") { startRecording(); return; }
    if (k == "recStop")  { stopRecording ("take held"); return; }
    if (k == "recCam")
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        if (recTarget != nullptr && take != nullptr && take->cam.size() < 200000)
            take->cam.push_back ({ (float) (double) payload.getProperty ("t", 0.0),
                                   (float) (double) payload.getProperty ("pitch", 0.0),
                                   (float) (double) payload.getProperty ("fov", 70.0) });
        return;
    }
    if (k == "vidBegin")
    {
        beginExport ((int) payload.getProperty ("w", 1920), (int) payload.getProperty ("h", 1080), (int) payload.getProperty ("fps", 30));
        return;
    }
    if (k == "vidFrame")
    {
        if (mp4 == nullptr || ! mp4->isOpen()) return;
        const int i = (int) payload.getProperty ("i", -1);
        if (i != vidNext) { failExport ("frame " + juce::String (i) + " arrived where " + juce::String (vidNext) + " was due"); return; }
        juce::MemoryOutputStream jpg;
        if (! juce::Base64::convertFromBase64 (jpg, payload.getProperty ("jpg", {}).toString())) { failExport ("frame " + juce::String (i) + " was not base64"); return; }
        const juce::Image img = juce::ImageFileFormat::loadFrom (jpg.getData(), jpg.getDataSize());
        if (! img.isValid()) { failExport ("frame " + juce::String (i) + " was not an image"); return; }
        juce::String err;
        if (! mp4->writeFrame (img, err)) { failExport (err); return; }
        // the sound up to the end of this frame, so the two streams interleave
        const juce::int64 total = (juce::int64) vidPcm.size() / 2;
        const juce::int64 upTo = std::min (total, (juce::int64) ((double) (i + 1) * vidPcmRate / vidFps));
        if (upTo > vidPcmWritten)
        {
            if (! mp4->writeAudio (vidPcm.data() + vidPcmWritten * 2, (int) (upTo - vidPcmWritten), err)) { failExport (err); return; }
            vidPcmWritten = upTo;
        }
        ++vidNext;
        recProgress = vidN > 0 ? (double) vidNext / vidN : 1.0;
        recText = "exporting frame " + juce::String (vidNext) + " of " + juce::String (vidN);
        if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("i", i); emitToUi ("vidAck", juce::var (o)); }
        return;
    }
    if (k == "vidEnd") { finishExport(); return; }
    if (k == "vidCancel")
    {
        if (renderJob != nullptr) { renderJob->stopThread (4000); renderJob.reset(); }
        if (mp4 != nullptr) mp4->abandon();
        mp4.reset();
        recState = take != nullptr && take->length.load() > 0 ? "ready" : "idle";
        recText = "export cancelled"; recProgress = 0; recDirty = true;
        return;
    }
    if (k == "reveal") { if (juce::File (lastFile).existsAsFile()) juce::File (lastFile).revealToUser(); return; }`);

// ================================================================ initialState: the catalogue and the layout
rep(P, `    obj->setProperty ("params", params);
    emitToUi ("initialState", juce::var (obj));`, `    obj->setProperty ("params", params);
    {
        juce::Array<juce::var> cat;
        for (int t = 0; t < NUM_FURN_TYPES; ++t)
        {
            const FurnSpec& F = FURN[t];
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", juce::String (F.id)); o->setProperty ("name", juce::String (F.name));
            o->setProperty ("w", F.w); o->setProperty ("d", F.d); o->setProperty ("h", F.h);
            o->setProperty ("zb", F.zb); o->setProperty ("zt", F.zt);
            o->setProperty ("occludes", F.occludes ? 1 : 0); o->setProperty ("reflectTop", F.reflectTop ? 1 : 0);
            o->setProperty ("absorb1k", F.absorb[3]);
            cat.add (juce::var (o));
        }
        obj->setProperty ("furniture", cat);
        obj->setProperty ("furn", furnJson());
    }
    furnDirtyUi = false;
    emitToUi ("initialState", juce::var (obj));`);

// ================================================================ timer: furniture echo, take state
rep(P, `    if (wavDirty) { wavDirty = false; emitWav(); }
    emitScene();
}`, `    if (wavDirty) { wavDirty = false; emitWav(); }
    emitScene();

    // a layout the page did not make (preset, project) goes to the page
    if (furnDirtyUi)
    {
        furnDirtyUi = false;
        auto* o = new juce::DynamicObject(); o->setProperty ("items", furnJson());
        emitToUi ("furn", juce::var (o));
    }

    // the take: the recorder hit its limit, the render finished, or just progress
    if (recState == "recording" && recCapped.exchange (false)) stopRecording ("stopped at the four-minute limit");
    if (recState == "rendering" && renderJob != nullptr && ! renderJob->isThreadRunning()) sendPlan();
    if (recDirty || ((recState == "recording" || recState == "rendering" || recState == "exporting") && ++recTick >= 3))
    {
        recTick = 0; recDirty = false;
        emitRec();
    }
}`);

// the page is told when a preset or a project brought a layout
rep(P, `        const juce::var v = juce::JSON::parse (f);
                if (v.isObject() && v.getProperty ("app", {}).toString() == "thin-walls") { applyPatchJson (v); text = "loaded " + f.getFileName(); }`,
`        const juce::var v = juce::JSON::parse (f);
                if (v.isObject() && v.getProperty ("app", {}).toString() == "thin-walls") { applyPatchJson (v); furnDirtyUi = true; text = "loaded " + f.getFileName(); }`);

// ================================================================ the take machinery, appended before createEditor
rep(P, `//==============================================================================
juce::AudioProcessorEditor* ThinWallsAudioProcessor::createEditor()`,
`//==============================================================================
// the take: record, render offline, export

void ThinWallsAudioProcessor::emitRec()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("state", recState);
    double sec = 0;
    { const juce::SpinLock::ScopedLockType sl (recLock); if (take != nullptr) sec = take->seconds(); }
    o->setProperty ("sec", sec);
    o->setProperty ("max", 240.0);
    o->setProperty ("progress", recProgress);
    o->setProperty ("file", lastFile);
    o->setProperty ("text", recText);
    emitToUi ("rec", juce::var (o));
}

void ThinWallsAudioProcessor::startRecording()
{
    if (renderJob != nullptr || (mp4 != nullptr && mp4->isOpen())) { recText = "an export is running"; recDirty = true; return; }
    auto t = std::make_shared<TakeData>();
    t->rate = hostRate > 0 ? hostRate : 48000.0;
    t->capacity = (int) (t->rate * 240.0);
    t->aux = getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    t->input.setSize (t->aux ? 4 : 2, t->capacity);
    t->input.clear();
    t->np = (int) twSpecs().size();
    t->params.assign ((size_t) (t->capacity / TakeData::PBLOCK + 2) * (size_t) t->np, 0.0f);
    t->furn.resize (1024);
    t->cam.reserve (8192);
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        take = t;
        recTarget = take.get();
        recFurnVersion = -1;
        recCapped = false;
    }
    recState = "recording"; recText = "recording"; recProgress = 0; recDirty = true;
}

void ThinWallsAudioProcessor::stopRecording (const juce::String& why)
{
    {
        const juce::SpinLock::ScopedLockType sl (recLock);
        recTarget = nullptr;
    }
    const bool have = take != nullptr && take->length.load() > (int) (0.2 * take->rate);
    recState = have ? "ready" : "idle";
    recText = have ? why + ": " + juce::String (take->seconds(), 1) + " s" : "nothing recorded";
    recDirty = true;
}

void ThinWallsAudioProcessor::failExport (const juce::String& why)
{
    if (renderJob != nullptr) { renderJob->stopThread (4000); renderJob.reset(); }
    if (mp4 != nullptr) mp4->abandon();
    mp4.reset();
    recState = "error"; recText = "export failed: " + why; recProgress = 0; recDirty = true;
    if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", recText); emitToUi ("notice", juce::var (o)); }
}

void ThinWallsAudioProcessor::beginExport (int w, int h, int fps)
{
    if (recState == "recording") stopRecording ("take held");
    if (take == nullptr || take->length.load() <= 0) { failExport ("there is no take - record one first"); return; }
    if (renderJob != nullptr) return;
    vidW = juce::jlimit (320, 3840, w) & ~1;
    vidH = juce::jlimit (240, 2160, h) & ~1;
    vidFps = juce::jlimit (12, 60, fps);
    renderJob = std::make_unique<RenderJob> (take);
    renderJob->startThread();
    recState = "rendering"; recText = "rendering the sound of the take"; recProgress = 0; recDirty = true;
}

void ThinWallsAudioProcessor::sendPlan()
{
    auto* job = dynamic_cast<RenderJob*> (renderJob.get());
    if (job == nullptr) return;
    if (! job->ok.load()) { failExport ("the render stopped"); return; }
    const TakeData& T = *take;
    const int n = T.length.load();

    // the sound, 16-bit stereo at 44.1 or 48 kHz (AAC takes nothing else)
    vidPcmRate = (std::abs (T.rate - 44100.0) < 1.0) ? 44100 : 48000;
    const double ratio = T.rate / vidPcmRate;
    const int outN = (int) std::floor (n / ratio);
    std::vector<float> ch[2];
    for (int c = 0; c < 2; ++c)
    {
        ch[c].assign ((size_t) std::max (outN, 1), 0.0f);
        if (std::abs (ratio - 1.0) < 1e-9) std::copy (job->out.getReadPointer (c), job->out.getReadPointer (c) + outN, ch[c].begin());
        else { juce::LagrangeInterpolator li; li.process (ratio, job->out.getReadPointer (c), ch[c].data(), outN); }
    }
    vidPcm.assign ((size_t) outN * 2, 0);
    for (int i = 0; i < outN; ++i)
        for (int c = 0; c < 2; ++c)
            vidPcm[(size_t) i * 2 + (size_t) c] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround (ch[c][(size_t) i] * 32767.0f));
    vidPcmWritten = 0;
    renderJob.reset();

    // the file
    mp4File = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("Thin Walls videos")
                  .getChildFile ("Thin Walls take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".mp4");
    mp4 = std::make_unique<Mp4Writer>();
    const int kbps = juce::jlimit (4000, 60000, (int) ((double) vidW * vidH * vidFps * 0.12 / 1000.0));
    juce::String err;
    if (! mp4->open (mp4File, vidW, vidH, vidFps, kbps, vidPcmRate, err)) { failExport (err); return; }

    // the plan: every host parameter at every frame, normalised as the page knows them
    const auto& specs = twSpecs();
    const double seconds = n / T.rate;
    vidN = std::max (1, (int) std::ceil (seconds * vidFps));
    vidNext = 0;
    auto* plan = new juce::DynamicObject();
    plan->setProperty ("fps", vidFps); plan->setProperty ("n", vidN); plan->setProperty ("seconds", seconds);
    plan->setProperty ("w", vidW); plan->setProperty ("h", vidH);
    juce::Array<juce::var> ids;
    for (const auto& s : specs) ids.add (juce::String (s.id));
    plan->setProperty ("ids", ids);
    juce::Array<juce::var> frames;
    const int nb = T.nblocks.load();
    for (int i = 0; i < vidN; ++i)
    {
        const int b = juce::jlimit (0, std::max (0, nb - 1), (int) ((double) i / vidFps * T.rate / TakeData::PBLOCK));
        juce::Array<juce::var> row;
        for (size_t j = 0; j < specs.size(); ++j)
        {
            const float raw = nb > 0 ? T.params[(size_t) b * (size_t) T.np + j] : 0.0f;
            const float v = specs[j].stepped ? raw / (float) std::max (1, specs[j].steps - 1) : raw;
            row.add (std::round (v * 100000.0f) / 100000.0f);
        }
        frames.add (juce::var (row));
    }
    plan->setProperty ("frames", frames);
    juce::Array<juce::var> fz;
    for (int f = 0; f < T.nfurn.load(); ++f)
    {
        const auto& snap = T.furn[(size_t) f];
        auto* o = new juce::DynamicObject();
        o->setProperty ("f", (int) std::ceil (snap.sample / T.rate * vidFps));
        juce::Array<juce::var> items;
        for (int i = 0; i < snap.n; ++i)
        {
            const FurnItem& it = snap.items[i];
            if (it.type < 0 || it.type >= NUM_FURN_TYPES) continue;
            auto* io = new juce::DynamicObject();
            io->setProperty ("t", juce::String (FURN[it.type].id));
            io->setProperty ("x", it.x); io->setProperty ("y", it.y); io->setProperty ("yaw", it.yaw);
            items.add (juce::var (io));
        }
        o->setProperty ("items", items);
        fz.add (juce::var (o));
    }
    plan->setProperty ("furn", fz);
    juce::Array<juce::var> cam;
    for (const auto& c : T.cam)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", c.t); o->setProperty ("pitch", c.pitch); o->setProperty ("fov", c.fov);
        cam.add (juce::var (o));
    }
    plan->setProperty ("cam", cam);

    recState = "exporting"; recText = "exporting " + juce::String (vidN) + " frames"; recProgress = 0; recDirty = true;
    if (emitToUi) emitToUi ("vidPlan", juce::var (plan));
    else { juce::var keep (plan); }
}

void ThinWallsAudioProcessor::finishExport()
{
    if (mp4 == nullptr || ! mp4->isOpen()) return;
    juce::String err;
    const juce::int64 total = (juce::int64) vidPcm.size() / 2;
    if (total > vidPcmWritten && ! mp4->writeAudio (vidPcm.data() + vidPcmWritten * 2, (int) (total - vidPcmWritten), err)) { failExport (err); return; }
    vidPcmWritten = total;
    if (! mp4->finish (err)) { failExport (err); mp4.reset(); return; }
    mp4.reset();
    lastFile = mp4File.getFullPathName();
    recState = "done"; recProgress = 1.0;
    recText = "saved " + mp4File.getFileName() + " (" + juce::String (mp4File.getSize() / 1048576.0, 1) + " MB)";
    recDirty = true;
    if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", recText); emitToUi ("notice", juce::var (o)); }
}

//==============================================================================
juce::AudioProcessorEditor* ThinWallsAudioProcessor::createEditor()`);

// the render progress, while it renders
rep(P, `    if (recState == "rendering" && renderJob != nullptr && ! renderJob->isThreadRunning()) sendPlan();`,
`    if (recState == "rendering" && renderJob != nullptr)
    {
        if (auto* job = dynamic_cast<RenderJob*> (renderJob.get())) recProgress = job->progress.load();
        if (! renderJob->isThreadRunning()) sendPlan();
    }`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
