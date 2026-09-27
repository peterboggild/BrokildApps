#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"
#include "Import.h"
#include "ImportJuce.h"

using namespace bs;

//==============================================================================
static juce::String fmtValue (const PSpec& s, float v)
{
    switch (s.kind)
    {
        case KP_LIST: return juce::String (s.list ? s.list[listIndex (s, v)] : "");
        case KP_INT:  return juce::String ((int) std::lround (s.lo + v * (s.hi - s.lo)));
        case KP_BIPOL: return juce::String ((int) std::round ((v * 2.0f - 1.0f) * 100.0f)) + " %";
        case KP_CENT: return juce::String ((int) std::round (v * s.hi)) + " c";
        case KP_HZ:
        {
            const float hz = hzOf (s, v);
            return hz < 10.0f ? juce::String (hz, 2) + " Hz" : juce::String (hz, 1) + " Hz";
        }
        case KP_SEC:
        {
            const float sec = secondsOf (v);
            return sec < 1.0f ? juce::String ((int) std::lround (sec * 1000.0f)) + " ms" : juce::String (sec, 2) + " s";
        }
        case KP_SEMI: return juce::String ((v * 2.0f - 1.0f) * 12.0f, 1) + " st";
        case KP_VOL:
        {
            const float g = 2.4f * v * v;
            return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB";
        }
        case KP_RATIO: return "x" + juce::String (ratioOf (v), 2);
        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout BrainScanAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        /*  SPECIMEN is not automatable: it rebuilds the whole volume, and a
            lane fighting that is unusable — the mood-organ precedent. */
        const bool automatable = std::strcmp (s.id, "specimen") != 0;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
BrainScanAudioProcessor::BrainScanAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BRAINSCAN", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i) ids.add (paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    bwfxRack.setWorldModConsumed (true);
    //  a fresh instance opens on the first factory patch
    applyFactory (0);
    startTimerHz (30);
}

BrainScanAudioProcessor::~BrainScanAudioProcessor() {}

bool BrainScanAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BrainScanAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    engine.service();
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

//==============================================================================
void BrainScanAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) engine.allNotesOff();
    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    engine.setTransport (bpm, ppq, playing);
    bwfxRack.setTransport (bpm, ppq, playing);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isPitchWheel())   engine.setBend ((m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
        else if (m.isSustainPedalOn())  { sustainOn = true;  engine.setSustain (true); }
        else if (m.isSustainPedalOff()) { sustainOn = false; engine.setSustain (false); }
        else if (m.isController() && m.getControllerNumber() == 1) { wheel = m.getControllerValue() / 127.0f; engine.setModWheel (juce::jmax (wheel, pressure)); }
        else if (m.isChannelPressure()) { pressure = m.getChannelPressureValue() / 127.0f; engine.setModWheel (juce::jmax (wheel, pressure)); }
    }

    const bwfx::WorldMod wm = bwfxRack.worldMod();
    engine.setWorldMod (wm.detuneCents, wm.pitchSag, wm.tremDepth, wm.tremRate, wm.filterMul, wm.panSpread);

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : L;
    if (nch > 1) engine.process (L, R, n);
    else { std::vector<float> tmp ((size_t) n); engine.process (L, tmp.data(), n); }

    bwfxRack.process (L, R, n);
    bwfx_juce::pushMacros (bwfxRack, apvts);
    if (nch > 2) for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);
}

//==============================================================================
void BrainScanAudioProcessor::timerCallback()
{
    bwfxRack.service();
    //  the specimen parameter may have moved (panel, patch, project): build it
    //  here, off the audio thread, and hand the page the new bytes
    for (int i = 0; i < numParams(); ++i)
        if (std::strcmp (paramSpec (i).id, "specimen") == 0) engine.p.specimen = raw[(size_t) i]->load();
    engine.serviceAsync();                 // built on a worker; published on a later tick
    if (engine.specimenLoaded() != specimenSent) emitVolume();
    emitParamEcho();
    emitView();
    ++tickCount;
}

void BrainScanAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        const float v = juce::jlimit (0.0f, 1.0f, value);
        if (fromUi)
        {
            const int idx = ids.indexOf (id);
            if (idx >= 0 && idx < (int) lastSent.size()) lastSent[(size_t) idx] = v;
        }
        prm->setValueNotifyingHost (v);
    }
}

void BrainScanAudioProcessor::emitParamEcho()
{
    if (! emitToUi || ! uiHasState.load()) return;
    if (lastSent.size() != (size_t) ids.size())
    {
        lastSent.resize ((size_t) ids.size());
        for (int i = 0; i < ids.size(); ++i) lastSent[(size_t) i] = raw[(size_t) i]->load();
        return;
    }
    juce::Array<juce::var> b;
    for (int i = 0; i < ids.size(); ++i)
    {
        const float v = raw[(size_t) i]->load();
        if (std::abs (v - lastSent[(size_t) i]) > 1e-6f)
        {
            lastSent[(size_t) i] = v;
            auto* e = new juce::DynamicObject();
            e->setProperty ("id", ids[i]);
            e->setProperty ("v", v);
            b.add (juce::var (e));
        }
    }
    if (b.isEmpty()) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("b", b);
    emitToUi ("hostParam", juce::var (o));
}

//==============================================================================
static bs::Line lineFromVar (const juce::var& v, const bs::Line& fallback)
{
    bs::Line l = fallback;
    if (! v.isObject()) return l;
    if (auto* pts = v.getProperty ("pts", juce::var()).getArray())
    {
        const int n = juce::jlimit (0, MAXPTS, pts->size() / 3);
        if (n >= 2)
        {
            l.n = n;
            for (int i = 0; i < n; ++i)
            {
                l.p[i].x = juce::jlimit (0.0f, 1.0f, (float) (double) (*pts)[3 * i]);
                l.p[i].y = juce::jlimit (0.0f, 1.0f, (float) (double) (*pts)[3 * i + 1]);
                l.p[i].z = juce::jlimit (0.0f, 1.0f, (float) (double) (*pts)[3 * i + 2]);
            }
        }
    }
    if (v.hasProperty ("closed")) l.closed = (bool) v.getProperty ("closed", false);
    if (v.hasProperty ("start"))  l.start = juce::jlimit (0.0f, 1.0f, (float) (double) v.getProperty ("start", 0.0));
    if (v.hasProperty ("warp"))   l.warp = juce::jlimit (-0.95f, 0.95f, (float) (double) v.getProperty ("warp", 0.0));
    if (v.hasProperty ("split"))  l.split = juce::jlimit (0, MAXPTS - 2, (int) v.getProperty ("split", 0));
    return l;
}

static juce::var lineToVar (const bs::Line& l)
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> pts;
    for (int i = 0; i < l.n; ++i) { pts.add ((double) l.p[i].x); pts.add ((double) l.p[i].y); pts.add ((double) l.p[i].z); }
    o->setProperty ("pts", pts);
    o->setProperty ("closed", l.closed);
    o->setProperty ("start", (double) l.start);
    o->setProperty ("warp", (double) l.warp);
    if (l.split > 0) o->setProperty ("split", l.split);      // a line never split writes what it always did
    return juce::var (o);
}

juce::var BrainScanAudioProcessor::linesToVar() const
{
    juce::Array<juce::var> a;
    for (int i = 0; i < NLINES; ++i) a.add (lineToVar (lines[i]));
    auto* o = new juce::DynamicObject();
    o->setProperty ("ver", 1);
    o->setProperty ("l", a);
    return juce::var (o);
}

bool BrainScanAudioProcessor::linesFromVar (const juce::var& v)
{
    if (! v.isObject()) return false;
    auto* a = v.getProperty ("l", juce::var()).getArray();
    if (a == nullptr) return false;
    for (int i = 0; i < NLINES && i < a->size(); ++i) lines[i] = lineFromVar ((*a)[i], lines[i]);
    engine.setLines (lines);
    return true;
}

void BrainScanAudioProcessor::applyParams (const Params& p)
{
    Params q = p;
    for (int i = 0; i < numParams(); ++i)
        setParamById (paramSpec (i).id, paramSpec (i).get (q), false);
}

void BrainScanAudioProcessor::applyFactory (int i)
{
    const int n = numFactory();
    if (n <= 0) return;
    i = juce::jlimit (0, n - 1, i);
    Params p;
    factory (i).build (lines, p);
    engine.setLines (lines);
    rememberLoadedLines();
    applyParams (p);
    bwfxRack.fromJson ("");
    patchName = factory (i).name; patchIsUser = false;
}

//==============================================================================
void BrainScanAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* outer = payload.getDynamicObject())
        if (outer->hasProperty ("b"))
        {
            if (auto* arr = outer->getProperty ("b").getArray())
                for (const auto& one : *arr) handleUiMessage (one);
            return;
        }

    auto* o = payload.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")
    {
        const juce::String id = o->getProperty ("id").toString();
        setParamById (id, (float) (double) o->getProperty ("v"), true);
        /*  Peter: "once an external file is imported, I cannot switch to the
            other samples — the switcher works but the image and sound do not
            change." The import OVERRODE the dial, by design, and CLEAR was the
            way out — but a dial that moves and changes nothing reads as
            broken. So: dialling a specimen while an import is in use clears
            the import and takes the specimen. */
        if (id == "specimen" && engine.importedActive()) clearImport ("import cleared - the dial has the room again");
    }
    else if (k == "note")
    {
        const int nt = (int) o->getProperty ("n");
        const float vel = o->hasProperty ("vel") ? (float) (double) o->getProperty ("vel") : 0.8f;
        if ((bool) o->getProperty ("on")) engine.noteOn (nt, vel > 0 ? vel : 0.8f);
        else                              engine.noteOff (nt);
    }
    else if (k == "panic")   wantPanic.store (true);
    else if (k == "line")
    {
        const int i = (int) o->getProperty ("i");
        if (i >= 0 && i < NLINES)
        {
            lines[i] = lineFromVar (payload, lines[i]);
            engine.setLine (i, lines[i]);
            patchIsUser = false;
        }
    }
    else if (k == "lines")   { if (linesFromVar (o->getProperty ("j"))) { patchIsUser = false; emitLines(); } }
    else if (k == "revertLines")
    {
        //  the lines the patch, project or factory study was loaded with
        for (int i = 0; i < NLINES; ++i) lines[i] = loadedLines[i];
        engine.setLines (lines);
        emitLines();
        notice ("lines put back as loaded");
    }
    else if (k == "shape")
    {
        //  a line preset, laid around the line's current centre
        const int i = (int) o->getProperty ("i");
        const int kind = (int) o->getProperty ("kind");
        const uint32_t seed = o->hasProperty ("seed") ? (uint32_t) (int) o->getProperty ("seed")
                                                      : (uint32_t) juce::Random::getSystemRandom().nextInt (100000);
        if (i >= 0 && i < NLINES)
        {
            const bs::Line& cur = lines[i];
            Vec3 c { 0, 0, 0 };
            for (int q = 0; q < cur.n; ++q) { c.x += cur.p[q].x; c.y += cur.p[q].y; c.z += cur.p[q].z; }
            c.x /= (float) cur.n; c.y /= (float) cur.n; c.z /= (float) cur.n;
            bs::Line l = cur;
            switch (kind)
            {
                case 0: l = bs::Line::straight ({ 0.04f, c.y, c.z }, { 0.96f, c.y, c.z }); break;
                case 1: l = bs::Line::circle (c, 0.24f, 2, 10); break;
                case 2: l = bs::Line::circle (c, 0.24f, 0, 10); break;
                case 3: l = bs::Line::helix (c, 0.18f, 0.7f, 1.5f, 12); break;
                case 4: l = bs::Line::walk (seed, 8, false, 0.2f); break;
                case 5: l = bs::Line::walk (seed, 8, true, 0.2f); break;
                default: break;
            }
            lines[i] = l;
            engine.setLine (i, l);
            patchIsUser = false;
            emitLines();
        }
    }
    else if (k == "factory") { applyFactory ((int) o->getProperty ("i")); emitLines(); emitBwfx(); emitPatch(); lastSent.assign ((size_t) ids.size(), -999.0f); }
    else if (k == "presetScan")   presetScan();
    else if (k == "presetLoad")   presetLoad (o->getProperty ("path").toString());
    else if (k == "save")         presetSaveAs();
    else if (k == "open")         presetOpenDialog();
    else if (k == "presetFolder") presetPickFolder();
    else if (k == "import")
    {
        auto start = importPath.isNotEmpty() ? juce::File (importPath).getParentDirectory()
                                             : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        activeChooser = std::make_unique<juce::FileChooser> (
            "A volume: a .nii or .nii.gz, a folder of DICOM, or a folder of images", start);
        activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f == juce::File{}) return;
                doImport (f);
            });
    }
    else if (k == "importPath")
    {
        //  the same import with the path handed in, so it can be driven from a
        //  probe and, later, from a file dropped on the panel
        const juce::File f (o->getProperty ("path").toString());
        if (f.exists()) doImport (f);
        else { importError = "no such file: " + f.getFullPathName(); emitImport(); notice (importError); }
    }
    else if (k == "importClear") clearImport ("the specimen dial is back");
    else if (k == "importOpts")
    {
        if (o->hasProperty ("axis"))    importAxis = juce::jlimit (0, 2, (int) o->getProperty ("axis"));
        if (o->hasProperty ("stretch")) importStretch = (bool) o->getProperty ("stretch");
        reImport();
    }
    else if (k == "hello")   { uiHasState.store (false); specimenSent = -1; emitInitialState(); }
    else if (k == "bwfx")    { if (bwfx_juce::handleMessage (bwfxRack, apvts, payload)) emitBwfx(); }
}

//==============================================================================
void BrainScanAudioProcessor::doImport (const juce::File& f)
{
    bs::SrcVolume src;
    juce::String err;
    if (! bsjuce::loadVolume (f, src, err))
    {
        importError = err;
        emitImport();
        notice (err);
        return;
    }

    bs::ImportOpts opts;
    opts.phaseAxis = importAxis;
    opts.stretch   = importStretch;

    bs::ImportReport rep;
    std::string e;
    std::vector<float> cube ((size_t) bs::VN * bs::VN * bs::VN);
    if (! bs::resampleToCube (src, bs::VN, opts, cube.data(), rep, e))
    {
        importError = juce::String (juce::CharPointer_UTF8 (e.c_str()));
        emitImport();
        notice (importError);
        return;
    }

    importCube.swap (cube);
    importPath = f.getFullPathName();
    importError = {};
    importWindowLo = rep.loValue; importWindowHi = rep.hiValue;
    for (int a = 0; a < 3; ++a) { importFilled[a] = rep.filled[a]; importSpacing[a] = rep.srcSpacing[a]; }
    importNote = juce::String (juce::CharPointer_UTF8 (rep.note.c_str()))
               + "  at " + juce::String (rep.srcSpacing[0], 2) + " x " + juce::String (rep.srcSpacing[1], 2)
               + " x " + juce::String (rep.srcSpacing[2], 2) + " mm";

    engine.setImported (importCube.data());
    specimenSent = -1;
    emitImport(); emitVolume();
    notice ("imported " + f.getFileName() + " - window "
            + juce::String ((int) rep.loValue) + " to " + juce::String ((int) rep.hiValue));
}

void BrainScanAudioProcessor::reImport()
{
    if (importPath.isEmpty()) { emitImport(); return; }
    const juce::File f (importPath);
    if (! f.exists())
    {
        importError = "the file this came from has moved; the volume already imported is kept.";
        emitImport();
        notice (importError);
        return;
    }
    doImport (f);
}

juce::String BrainScanAudioProcessor::importCubeBase64() const
{
    const size_t n = (size_t) bs::VN * bs::VN * bs::VN;
    if (importCube.size() != n) return {};
    std::vector<juce::uint8> bytes (n);
    for (size_t i = 0; i < n; ++i)
        bytes[i] = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (importCube[i] * 255.0f));
    juce::MemoryOutputStream packed;
    {
        juce::GZIPCompressorOutputStream gz (packed, 9);
        gz.write (bytes.data(), bytes.size());
    }
    return juce::Base64::toBase64 (packed.getData(), packed.getDataSize());
}

bool BrainScanAudioProcessor::importCubeFromBase64 (const juce::String& b64)
{
    if (b64.isEmpty()) return false;
    juce::MemoryOutputStream raw;
    if (! juce::Base64::convertFromBase64 (raw, b64)) return false;
    juce::MemoryInputStream src (raw.getData(), raw.getDataSize(), false);
    juce::GZIPDecompressorInputStream gz (&src, false, juce::GZIPDecompressorInputStream::zlibFormat);
    juce::MemoryOutputStream out;
    out.writeFromInputStream (gz, -1);
    const size_t n = (size_t) bs::VN * bs::VN * bs::VN;
    if (out.getDataSize() != n) return false;
    const auto* p = (const juce::uint8*) out.getData();
    importCube.resize (n);
    for (size_t i = 0; i < n; ++i) importCube[i] = (float) p[i] / 255.0f;
    engine.setImported (importCube.data());
    specimenSent = -1;
    return true;
}

void BrainScanAudioProcessor::emitImport()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", engine.importedActive());
    o->setProperty ("path", importPath);
    o->setProperty ("name", importPath.isEmpty() ? juce::String()
                                                 : juce::File (importPath).getFileName());
    o->setProperty ("note", importNote);
    o->setProperty ("err", importError);
    o->setProperty ("axis", importAxis);
    o->setProperty ("stretch", importStretch);
    o->setProperty ("lo", (double) importWindowLo);
    o->setProperty ("hi", (double) importWindowHi);
    juce::Array<juce::var> fl;
    for (int a = 0; a < 3; ++a) fl.add (importFilled[a]);
    o->setProperty ("filled", fl);
    emitToUi ("import", juce::var (o));
}

void BrainScanAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

//==============================================================================
void BrainScanAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ps;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", s.id);
        e->setProperty ("name", juce::String (juce::CharPointer_UTF8 (s.name)));
        e->setProperty ("gloss", juce::String (juce::CharPointer_UTF8 (s.gloss)));
        e->setProperty ("kind", s.kind);
        e->setProperty ("v", raw[(size_t) i]->load());
        e->setProperty ("def", s.def);
        e->setProperty ("lo", s.lo);
        e->setProperty ("hi", s.hi);
        if (s.list != nullptr)
        {
            juce::Array<juce::var> names;
            for (int q = 0; q < s.nlist; ++q) names.add (juce::String (s.list[q]));
            e->setProperty ("list", names);
        }
        ps.add (juce::var (e));
    }
    o->setProperty ("params", ps);
    o->setProperty ("build", BS_BUILD_ID);
    juce::Array<juce::var> fac;
    for (int i = 0; i < numFactory(); ++i) fac.add (juce::String (factory (i).name));
    o->setProperty ("factory", fac);
    juce::Array<juce::var> specs;
    for (int i = 0; i < numSpecimens(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("name", juce::String (specimenName (i)));
        e->setProperty ("gloss", juce::String (juce::CharPointer_UTF8 (specimenGloss (i))));
        specs.add (juce::var (e));
    }
    o->setProperty ("specimens", specs);
    o->setProperty ("vn", VN);
    auto* pn = new juce::DynamicObject();
    pn->setProperty ("name", patchName); pn->setProperty ("user", patchIsUser);
    o->setProperty ("patch", juce::var (pn));
    emitToUi ("initialState", juce::var (o));
    uiHasState.store (true);
    emitVolume(); emitLines(); emitBwfx(); emitPatch(); emitImport(); presetScan();
}

void BrainScanAudioProcessor::emitVolume()
{
    if (! emitToUi) return;
    const Volume& v = engine.volume();
    //  -1 means nothing has been built yet; SPEC_IMPORTED is -2 and IS a volume
    /*  The page renders 64^3 (a 262 144-texel stream and an R8 texture it
        was built around); the engine reads 128^3. It gets the level whose
        side is 64 — the same bytes as before, binomially averaged. */
    const int lv = v.levelWithSide (64);
    if (v.specimen == -1 || v.lod[lv].empty()) return;
    std::vector<uint8_t> bytes (v.lod[lv].size());
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = (uint8_t) juce::jlimit (0, 255, (int) std::lround (v.lod[lv][i] * 255.0f));
    auto* o = new juce::DynamicObject();
    o->setProperty ("n", v.side[lv]);
    o->setProperty ("spec", v.specimen);
    o->setProperty ("name", v.specimen == SPEC_IMPORTED ? juce::String ("IMPORTED")
                                                        : juce::String (specimenName (v.specimen)));
    o->setProperty ("periodic", v.periodicX);
    /*  Hounsfield units, when the volume has them: the bodies are built in HU
        and mapped -1000..2000 onto 0..1; an import's cube spans its window.
        The page then offers a radiographer's presets and prints HU. */
    if (v.specimen == SPEC_IMPORTED)
    {
        juce::Array<juce::var> hu; hu.add ((double) importWindowLo); hu.add ((double) importWindowHi);
        o->setProperty ("hu", hu);
        o->setProperty ("win", juce::String());
    }
    else if (specimenIsHu (v.specimen))
    {
        juce::Array<juce::var> hu; hu.add (-1000.0); hu.add (2000.0);
        o->setProperty ("hu", hu);
        o->setProperty ("win", juce::String (specimenWindow (v.specimen)));
    }
    o->setProperty ("d", juce::Base64::toBase64 (bytes.data(), bytes.size()));
    emitToUi ("volume", juce::var (o));
    specimenSent = v.specimen;
}

void BrainScanAudioProcessor::emitLines()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("j", linesToVar());
    emitToUi ("lines", juce::var (o));
}

void BrainScanAudioProcessor::emitPatch()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", patchName);
    o->setProperty ("user", patchIsUser);
    emitToUi ("patch", juce::var (o));
}

void BrainScanAudioProcessor::emitBwfx()
{
    if (! emitToUi) return;
    emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void BrainScanAudioProcessor::emitView()
{
    if (! emitToUi || ! uiHasState.load()) return;
    static VoiceView views[MAXVOICES];
    const int n = engine.voicesView (views, MAXVOICES);
    static float sw[SCOPE_N], sf[SCOPE_N], sm[SCOPE_N];
    engine.scopeView (sw, sf, sm);

    auto* o = new juce::DynamicObject();
    o->setProperty ("lvl", (double) engine.outLevel());
    o->setProperty ("w", juce::Base64::toBase64 (sw, sizeof (sw)));
    o->setProperty ("f", juce::Base64::toBase64 (sf, sizeof (sf)));
    o->setProperty ("m", juce::Base64::toBase64 (sm, sizeof (sm)));
    juce::Array<juce::var> vs;
    for (int i = 0; i < n; ++i)
    {
        const VoiceView& b = views[i];
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", b.id); e->setProperty ("n", b.note); e->setProperty ("h", b.held);
        e->setProperty ("scan", (double) b.scan); e->setProperty ("cut", (double) b.cutoff);
        e->setProperty ("fp", (double) b.fPhase); e->setProperty ("mp", (double) b.mPhase);
        e->setProperty ("mod", (double) b.mod); e->setProperty ("lvl", (double) b.lvl);
        vs.add (juce::var (e));
    }
    o->setProperty ("v", vs);
    emitToUi ("view", juce::var (o));
}

//==============================================================================
void BrainScanAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson().c_str()), nullptr);
    state.setProperty ("lines", juce::JSON::toString (linesToVar(), true), nullptr);
    /*  the imported cube travels with the project, so a patch is still the
        whole machine when the folder it came from has been tidied away */
    if (engine.importedActive())
    {
        state.setProperty ("importCube", importCubeBase64(), nullptr);
        state.setProperty ("importPath", importPath, nullptr);
        state.setProperty ("importNote", importNote, nullptr);
        state.setProperty ("importAxis", importAxis, nullptr);
        state.setProperty ("importStretch", importStretch, nullptr);
    }
    state.setProperty ("patchName", patchName, nullptr);
    state.setProperty ("patchUser", patchIsUser, nullptr);
    state.setProperty ("build", juce::String (BS_BUILD_ID), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void BrainScanAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        const juce::String built = tree.getProperty ("build", juce::String()).toString();
        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        const juce::String ln   = tree.getProperty ("lines", juce::String()).toString();
        const juce::String cube = tree.getProperty ("importCube", juce::String()).toString();
        patchName = tree.getProperty ("patchName", patchName).toString();
        patchIsUser = (bool) tree.getProperty ("patchUser", false);
        importPath = tree.getProperty ("importPath", juce::String()).toString();
        importNote = tree.getProperty ("importNote", juce::String()).toString();
        importAxis = juce::jlimit (0, 2, (int) tree.getProperty ("importAxis", 0));
        importStretch = (bool) tree.getProperty ("importStretch", false);
        for (const char* k : { "build", "bwfx", "lines", "patchName", "patchUser",
                               "importCube", "importPath", "importNote", "importAxis", "importStretch" })
            tree.removeProperty (k, nullptr);
        apvts.replaceState (tree);
        migrateSpecimen (built);
        bwfxRack.fromJson (rack.toRawUTF8());
        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) linesFromVar (v); }
        rememberLoadedLines();
        /*  a project that had no import must not inherit one from the
            instance this state is being loaded into */
        engine.clearImported();
        importCube.clear();
        if (cube.isNotEmpty() && ! importCubeFromBase64 (cube))
            importError = "the imported volume in this project could not be read back.";
        emitInitialState();
    }
}

//==============================================================================
//  the house patch files
juce::PropertiesFile& BrainScanAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "BrainScan";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File BrainScanAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved) && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = brokild::patchFolder ("Brain Scan", {});
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Brain Scan Patches");
        }
    }
    return presetFolder;
}

void BrainScanAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var BrainScanAudioProcessor::presetScanDir (const juce::File& dir, int depth)
{
    juce::Array<juce::var> items;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto f = e.getFile();
        if (f.isHidden()) continue;
        if (f.isDirectory())
        {
            if (depth >= 2) continue;
            auto kids = presetScanDir (f, depth + 1);
            if (kids.size() == 0) continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileName()); o->setProperty ("d", true); o->setProperty ("i", kids);
            items.add (juce::var (o));
        }
        else if (f.hasFileExtension ("json"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileNameWithoutExtension()); o->setProperty ("p", f.getFullPathName());
            items.add (juce::var (o));
        }
    }
    struct Sorter
    {
        static int compareElements (const juce::var& a, const juce::var& b)
        {
            const bool da = (bool) a.getProperty ("d", false), db = (bool) b.getProperty ("d", false);
            if (da != db) return da ? -1 : 1;
            return a.getProperty ("n", "").toString().compareNatural (b.getProperty ("n", "").toString());
        }
    };
    Sorter sorter; items.sort (sorter);
    return juce::var (items);
}

void BrainScanAudioProcessor::presetScan()
{
    if (! emitToUi) return;
    const auto dir = presetFolderOrDefault();
    const bool there = dir.isDirectory();
    auto* o = new juce::DynamicObject();
    o->setProperty ("folder", dir.getFullPathName());
    o->setProperty ("exists", there);
    o->setProperty ("items", there ? presetScanDir (dir, 0) : juce::var (juce::Array<juce::var>()));
    emitToUi ("presetTree", juce::var (o));
}

void BrainScanAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Where the patches live", start);
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{} || ! dir.isDirectory()) return;
            rememberPresetFolder (dir); presetScan();
        });
}

juce::String BrainScanAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i) pv->setProperty (ids[i], (double) raw[(size_t) i]->load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "brain-scan");
    o->setProperty ("kind", "patch");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("build", juce::String (BS_BUILD_ID));
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("lines", linesToVar());
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (o), false);
}

void BrainScanAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject()) { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "brain-scan") { notice ("THAT IS NOT A BRAIN SCAN PATCH"); return; }
    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr) { setParamById (kv.name.toString(), (float) (double) kv.value, false); ++applied; }
    migrateSpecimen (v.getProperty ("build", juce::var ("")).toString());
    linesFromVar (v.getProperty ("lines", juce::var()));
    rememberLoadedLines();
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    patchName = name; patchIsUser = true;
    emitLines(); emitBwfx(); emitPatch();
    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" - " + juce::String (applied) + " VALUES, SIX LINES");
}

/*  260904.3 gave the SPECIMEN dial twelve slots where it had nine. The
    parameter is normalised (index / (slots - 1)), so a value written by an
    older build lands on the wrong tissue — CORTEX at 8/8 would read TENDON at
    11/11. Remap once, keyed on the build that wrote the patch or project; a
    project with no build id at all predates every build that writes one. */
void BrainScanAudioProcessor::migrateSpecimen (const juce::String& writtenBy)
{
    //  how many slots the dial had when this was written
    int oldSlots = 0;
    if (writtenBy.isEmpty() || writtenBy.compare ("260904.3") < 0) oldSlots = 9;      // before the gritty three
    else if (writtenBy.compare ("260905.1") < 0)                   oldSlots = 12;     // before the bodies
    const int newSlots = numSpecimens();
    if (oldSlots == 0 || oldSlots == newSlots) return;
    auto* rawSpec = apvts.getRawParameterValue ("specimen");
    if (rawSpec == nullptr) return;
    const int idx = juce::jlimit (0, oldSlots - 1, (int) std::lround (rawSpec->load() * (float) (oldSlots - 1)));
    setParamById ("specimen", (float) idx / (float) (newSlots - 1), false);
}

void BrainScanAudioProcessor::rememberLoadedLines()
{
    for (int i = 0; i < NLINES; ++i) loadedLines[i] = lines[i];
}

void BrainScanAudioProcessor::clearImport (const juce::String& why)
{
    engine.clearImported();
    importCube.clear(); importPath = {}; importNote = {}; importError = {};
    specimenSent = -1;
    emitImport(); emitVolume();
    notice (why);
}

void BrainScanAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto suggested = dir.getChildFile ((patchIsUser ? patchName : juce::String ("Scan")) + ".json");
    activeChooser = std::make_unique<juce::FileChooser> ("Save this patch", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            if (! file.hasFileExtension ("json")) file = file.withFileExtension ("json");
            file.getParentDirectory().createDirectory();
            const auto name = file.getFileNameWithoutExtension();
            const bool ok = file.replaceWithText (patchJson (name));
            if (ok)
            {
                const auto chosen = file.getParentDirectory(), root = presetFolderOrDefault();
                if (chosen != root && ! chosen.isAChildOf (root)) rememberPresetFolder (chosen);
                patchName = name; patchIsUser = true; emitPatch();
            }
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\"" : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void BrainScanAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Open a patch", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void BrainScanAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

juce::AudioProcessorEditor* BrainScanAudioProcessor::createEditor() { return new BrainScanAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrainScanAudioProcessor(); }
