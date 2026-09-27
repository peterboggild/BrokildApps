// Processor v6 (2026-09-25): render qualities (LIVE/HIGH/ULTRA/ULTRA+BASS) for the
// export and a new BOUNCE to WAV, through OfflineRender; a personal head (SOFA)
// for live playing and rendering alike. Validates every anchor, then writes.
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
function between(f, startMark, endMark, replacement) {
  load(f);
  const s = files[f];
  const i = s.indexOf(startMark), j = s.indexOf(endMark, i);
  if (i < 0 || j < 0) { miss.push(f + ': between ' + JSON.stringify(startMark.slice(0, 50))); return; }
  files[f] = s.slice(0, i) + replacement + s.slice(j + endMark.length);
}
const P = 'PluginProcessor.cpp', H = 'PluginProcessor.h';

// ---------------------------------------------------------------- header
rep(H, `#include "TakeExport.h"`, `#include "TakeExport.h"
#include "OfflineRender.h"`);
rep(H, `    std::unique_ptr<juce::Thread> renderJob;`, `    std::unique_ptr<juce::Thread> renderJob;
    int vidSound = 0;                          // the export's sound quality (tw::SoundQuality)
    void beginBounce (int sound);
    void finishBounce();

    // ---- the head: MIT KEMAR, or a personal set from a SOFA file
    std::shared_ptr<const tw::Hrtf> head;      // the personal set in use (null = built-in)
    juce::String headPath;
    juce::SpinLock headLock;                   // the audio thread only TRIES it
    std::shared_ptr<const tw::Hrtf> headPending;
    std::atomic<int> headVersion { 0 };
    int headVersionAudio = 0;
    std::vector<std::shared_ptr<const tw::Hrtf>> headRetired;   // never freed on the audio thread
    bool loadHead (const juce::String& file, bool quiet);
    void resetHead();
    void emitHead();`);

// ---------------------------------------------------------------- the render job, now OfflineRender
between(P, `namespace
{
    /*  Renders a take through a fresh engine, block by block on the take's own`, `            ok = true;
        }
    };
}`, `namespace
{
    /*  Renders a take offline at a chosen quality (OfflineRender), off the
        message thread. Nothing here touches the live engine, so the plug-in
        keeps playing while an export or a bounce renders. */
    struct RenderJob : juce::Thread
    {
        std::shared_ptr<TakeData> take;
        juce::AudioBuffer<float> out;
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> ok { false }, cancel { false };
        int fps = 30;
        bool bounce = false;
        tw::SoundQuality quality = tw::SoundQuality::Live;
        std::shared_ptr<const tw::Hrtf> head;
        std::vector<float> light;          // NUM_ROOMS per video frame
        juce::String note;
        explicit RenderJob (std::shared_ptr<TakeData> t) : juce::Thread ("Thin Walls take render"), take (std::move (t)) {}
        ~RenderJob() override { cancel = true; stopThread (8000); }
        void run() override
        {
            const TakeData& T = *take;
            tw::TakeView v;
            v.rate = T.rate; v.length = T.length.load(); v.pblock = TakeData::PBLOCK; v.nblocks = T.nblocks.load();
            for (int c = 0; c < T.input.getNumChannels() && c < 4; ++c) v.in[c] = T.input.getReadPointer (c);
            if (! T.aux) { v.in[2] = nullptr; v.in[3] = nullptr; }
            const float* raw = T.params.data(); const int np = T.np;
            v.paramsAt = [raw, np] (int b, tw::Params& p) { rawToParams (raw + (size_t) b * (size_t) np, p); };
            for (int f = 0; f < T.nfurn.load(); ++f)
            {
                const auto& s = T.furn[(size_t) f];
                v.layouts.push_back ({ s.sample, s.n, s.items, s.panelArea });
            }
            tw::RenderOptions o; o.quality = quality; o.head = head; o.fps = bounce ? 0 : fps;
            tw::RenderOutput r;
            if (! tw::renderTake (v, o, r, &progress, &cancel)) return;
            out.setSize (2, (int) r.L.size());
            out.copyFrom (0, 0, r.L.data(), (int) r.L.size());
            out.copyFrom (1, 0, r.R.data(), (int) r.R.size());
            light = std::move (r.light);
            note = r.note;
            ok = true;
        }
    };
}`);
rep(P, `#include "PluginProcessor.h"`, `#include "PluginProcessor.h"
#include "OfflineRender.h"`);

// ---------------------------------------------------------------- export with a quality, and the bounce
rep(P, `        beginExport ((int) payload.getProperty ("w", 1920), (int) payload.getProperty ("h", 1080), (int) payload.getProperty ("fps", 30));
        return;`, `        vidSound = juce::jlimit (0, 3, (int) payload.getProperty ("sound", 0));
        beginExport ((int) payload.getProperty ("w", 1920), (int) payload.getProperty ("h", 1080), (int) payload.getProperty ("fps", 30));
        return;
    }
    if (k == "bounce") { beginBounce (juce::jlimit (0, 3, (int) payload.getProperty ("sound", 0))); return; }
    if (k == "hrtfOpen")
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a SOFA file (a measured head, AES69)",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.sofa");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const juce::File f = fc.getResult();
            if (f.getFullPathName().isNotEmpty()) loadHead (f.getFullPathName(), false);
        });
        return;
    }
    if (k == "hrtfPath") { loadHead (payload.getProperty ("path", {}).toString(), false); return; }
    if (k == "hrtfDefault") { resetHead(); return;`);
rep(P, `    {
        auto job = std::make_unique<RenderJob> (take);
        job->fps = vidFps;
        renderJob = std::move (job);
    }
    renderJob->startThread();
    recState = "rendering"; recText = "rendering the sound of the take"; recProgress = 0; recDirty = true;`,
`    {
        auto job = std::make_unique<RenderJob> (take);
        job->fps = vidFps;
        job->quality = (tw::SoundQuality) vidSound;
        job->head = head;
        renderJob = std::move (job);
    }
    renderJob->startThread();
    static const char* qn[] = { "", " (HIGH)", " (ULTRA)", " (ULTRA + BASS)" };
    recState = "rendering"; recText = juce::String ("rendering the sound of the take") + qn[vidSound]; recProgress = 0; recDirty = true;`);
rep(P, `    auto* job = dynamic_cast<RenderJob*> (renderJob.get());
    if (job == nullptr) return;
    if (! job->ok.load()) { failExport ("the render stopped"); return; }
    const TakeData& T = *take;`, `    auto* job = dynamic_cast<RenderJob*> (renderJob.get());
    if (job == nullptr) return;
    if (! job->ok.load()) { failExport ("the render stopped"); return; }
    if (job->bounce) { finishBounce(); return; }
    const TakeData& T = *take;`);
rep(P, `void ThinWallsAudioProcessor::sendPlan()
{`, `/*  BOUNCE: the take's sound alone, rendered offline at the chosen quality and
    written as a 24-bit WAV beside the videos. */
void ThinWallsAudioProcessor::beginBounce (int sound)
{
    if (recState == "recording") stopRecording ("take held");
    if (take == nullptr || take->length.load() <= 0) { failExport ("there is no take - record one first"); return; }
    if (renderJob != nullptr || (mp4 != nullptr && mp4->isOpen())) return;
    auto job = std::make_unique<RenderJob> (take);
    job->bounce = true;
    job->quality = (tw::SoundQuality) sound;
    job->head = head;
    renderJob = std::move (job);
    renderJob->startThread();
    static const char* qn[] = { "", " (HIGH)", " (ULTRA)", " (ULTRA + BASS)" };
    recState = "rendering"; recText = juce::String ("bouncing the take") + qn[sound]; recProgress = 0; recDirty = true;
}

void ThinWallsAudioProcessor::finishBounce()
{
    auto* job = dynamic_cast<RenderJob*> (renderJob.get());
    if (job == nullptr) return;
    const juce::File f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("Thin Walls videos")
                             .getChildFile ("Thin Walls take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".wav");
    f.getParentDirectory().createDirectory();
    bool written = false;
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os != nullptr)
            if (auto* w = wav.createWriterFor (os.get(), take->rate, 2, 24, {}, 0))
            {
                os.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                written = writer->writeFromAudioSampleBuffer (job->out, 0, job->out.getNumSamples());
            }
    }
    const juce::String note = job->note;
    renderJob.reset();
    if (! written) { failExport ("could not write " + f.getFullPathName()); return; }
    lastFile = f.getFullPathName();
    recState = "done"; recProgress = 1.0;
    recText = "bounced " + f.getFileName() + " - " + note;
    recDirty = true;
    if (emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", recText); emitToUi ("notice", juce::var (o)); }
}

void ThinWallsAudioProcessor::sendPlan()
{`);
rep(P, `    if (k == "vidCancel")
    {
        if (renderJob != nullptr) { renderJob->stopThread (4000); renderJob.reset(); }`, `    if (k == "vidCancel")
    {
        if (renderJob != nullptr) renderJob.reset();       // the job cancels itself on the way out`);

// ---------------------------------------------------------------- the head
rep(P, `    engine.prepare (sampleRate, samplesPerBlock);`, `    engine.prepare (sampleRate, samplesPerBlock);
    // a personal head is resampled when the set is loaded: load it again at this rate
    if (headPath.isNotEmpty() && (head == nullptr || std::abs (sampleRate - lastHeadRate) > 0.5)) loadHead (headPath, true);
    if (head != nullptr) engine.setHrtf (head);`);
rep(H, `    int vidSound = 0;                          // the export's sound quality (tw::SoundQuality)`, `    int vidSound = 0;                          // the export's sound quality (tw::SoundQuality)
    double lastHeadRate = 0;`);
rep(P, `    readParams();
    engine.setParams (current);
`, `    readParams();
    engine.setParams (current);

    // a new head, if the message thread loaded one (the old one is kept alive there)
    if (headVersion.load() != headVersionAudio)
    {
        const juce::SpinLock::ScopedTryLockType tl (headLock);
        if (tl.isLocked() && headPending != nullptr) { engine.setHrtf (headPending); headVersionAudio = headVersion.load(); }
    }
`);
rep(P, `        xml->setAttribute ("pics", juce::JSON::toString (picsJson (true), true));`, `        xml->setAttribute ("pics", juce::JSON::toString (picsJson (true), true));
        xml->setAttribute ("hrtf", headPath);`);
rep(P, `    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn", "pics" }) xml->removeAttribute (a);`,
`    {
        const juce::String hp = xml->getStringAttribute ("hrtf", {});
        if (hp.isNotEmpty()) { if (hp != headPath) loadHead (hp, true); }
        else if (headPath.isNotEmpty()) resetHead();
    }
    for (auto* a : { "showrays", "selsrc", "wavgain", "wavpath", "furn", "pics", "hrtf" }) xml->removeAttribute (a);`);
rep(P, `    furnDirtyUi = false;
    picsDirtyUi = false;`, `    furnDirtyUi = false;
    picsDirtyUi = false;
    {
        auto* h = new juce::DynamicObject();
        h->setProperty ("name", head != nullptr ? juce::String (head->name()) : juce::String ("MIT KEMAR (built in)"));
        h->setProperty ("path", headPath);
        h->setProperty ("personal", head != nullptr ? 1 : 0);
        obj->setProperty ("hrtf", juce::var (h));
    }`);
rep(P, `//==============================================================================
juce::var ThinWallsAudioProcessor::picsJson (bool withImages) const`, `//==============================================================================
/*  The head. A personal set is loaded and resampled on the message thread and
    handed to the audio thread through a lock it only tries; the previous set is
    kept here so nothing is freed while the audio thread might hold it. */
bool ThinWallsAudioProcessor::loadHead (const juce::String& file, bool quiet)
{
    const juce::File f (file);
    juce::String text;
    bool ok = false;
    if (! f.existsAsFile()) text = "no such file: " + f.getFileName();
    else
    {
        auto h = std::make_shared<tw::Hrtf>();
        std::string err;
        if (h->loadSofa (f.getFullPathName().toStdString(), hostRate > 0 ? hostRate : 48000.0, err))
        {
            {
                const juce::SpinLock::ScopedLockType sl (headLock);
                if (headPending != nullptr) headRetired.push_back (headPending);
                headPending = h;
                ++headVersion;
            }
            if (headRetired.size() > 4) headRetired.erase (headRetired.begin());
            head = h; headPath = f.getFullPathName(); lastHeadRate = hostRate; ok = true;
            text = "head: " + f.getFileName();
        }
        else text = "could not use " + f.getFileName() + ": " + juce::String (err);
    }
    if (! ok && ! quiet && emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", text); emitToUi ("notice", juce::var (o)); }
    if (ok && ! quiet && emitToUi) { auto* o = new juce::DynamicObject(); o->setProperty ("text", text); emitToUi ("notice", juce::var (o)); }
    emitHead();
    return ok;
}

void ThinWallsAudioProcessor::resetHead()
{
    auto h = std::make_shared<tw::Hrtf>();
    h->prepare (hostRate > 0 ? hostRate : 48000.0);
    {
        const juce::SpinLock::ScopedLockType sl (headLock);
        if (headPending != nullptr) headRetired.push_back (headPending);
        headPending = h;
        ++headVersion;
    }
    if (headRetired.size() > 4) headRetired.erase (headRetired.begin());
    head = nullptr; headPath = {};
    emitHead();
}

void ThinWallsAudioProcessor::emitHead()
{
    if (! emitToUi) return;
    auto* h = new juce::DynamicObject();
    h->setProperty ("name", head != nullptr ? juce::String (head->name()) : juce::String ("MIT KEMAR (built in)"));
    h->setProperty ("path", headPath);
    h->setProperty ("personal", head != nullptr ? 1 : 0);
    emitToUi ("hrtf", juce::var (h));
}

//==============================================================================
juce::var ThinWallsAudioProcessor::picsJson (bool withImages) const`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
