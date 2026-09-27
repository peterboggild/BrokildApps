#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Png16.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

using namespace ht;

//==============================================================================
static juce::String fmtValue (const PSpec& s, float v)
{
    switch (s.kind)
    {
        case KP_LIST: return juce::String (s.list ? s.list[listIndex (s, v)] : "");
        case KP_INT:  return juce::String ((int) std::lround (s.lo + v * (s.hi - s.lo)));
        case KP_BIPOL: return juce::String ((int) std::round ((v * 2.0f - 1.0f) * 100.0f)) + " %";
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
        default: return juce::String ((int) std::round (v * 100.0f)) + " %";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout HighTideAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
HighTideAudioProcessor::HighTideAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HIGHTIDE", createParameterLayout())
{
    for (int i = 0; i < numParams(); ++i) ids.add (paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    bwfxRack.setWorldModConsumed (true);
    //  a fresh instance opens on the first factory patch
    applyFactory (0);
    startTimerHz (30);
}

HighTideAudioProcessor::~HighTideAudioProcessor() {}

bool HighTideAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HighTideAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < numParams(); ++i) paramSpec (i).get (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

//==============================================================================
void HighTideAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
void HighTideAudioProcessor::timerCallback()
{
    bwfxRack.service();
    emitParamEcho();
    emitBalls();
    ++tickCount;
}

void HighTideAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
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

void HighTideAudioProcessor::emitParamEcho()
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
static std::vector<float> b64floats (const juce::String& s)
{
    juce::MemoryOutputStream mo;
    juce::Base64::convertFromBase64 (mo, s);
    std::vector<float> f (mo.getDataSize() / sizeof (float));
    std::memcpy (f.data(), mo.getData(), f.size() * sizeof (float));
    return f;
}

void HighTideAudioProcessor::handleUiMessage (const juce::var& payload)
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

    if (k == "p")            setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);
    else if (k == "note")
    {
        const int nt = (int) o->getProperty ("n");
        const float vel = o->hasProperty ("vel") ? (float) (double) o->getProperty ("vel") : 0.8f;
        if ((bool) o->getProperty ("on")) engine.noteOn (nt, vel > 0 ? vel : 0.8f);
        else                              engine.noteOff (nt);
    }
    else if (k == "drop")    engine.drop ((float) (double) o->getProperty ("z"), (float) (double) o->getProperty ("e"),
                                          (int) o->getProperty ("n"), (bool) o->getProperty ("on"));
    else if (k == "panic")   wantPanic.store (true);
    else if (k == "terr")
    {
        const int x0 = (int) o->getProperty ("x0"), nx = (int) o->getProperty ("nx");
        const int z0 = (int) o->getProperty ("z0"), nz = (int) o->getProperty ("nz");
        const auto f = b64floats (o->getProperty ("d").toString());
        if (nx > 0 && nz > 0 && f.size() >= (size_t) nx * nz) { engine.terrain().writeRegion (x0, nx, z0, nz, f.data()); patchIsUser = false; }
    }
    else if (k == "terrAll")
    {
        const auto f = b64floats (o->getProperty ("d").toString());
        if (f.size() >= (size_t) NX * NZ) engine.terrain().setAll (f.data());
    }
    else if (k == "lanes")   { lanes = lanesFromVar (o->getProperty ("j")); engine.setLanes (lanes); }
    else if (k == "factory") { applyFactory ((int) o->getProperty ("i")); emitTerrain(); emitLanes(); emitBwfx(); emitPatch(); lastSent.assign ((size_t) ids.size(), -999.0f); }
    else if (k == "presetScan")   presetScan();
    else if (k == "presetLoad")   presetLoad (o->getProperty ("path").toString());
    else if (k == "save")         presetSaveAs();
    else if (k == "open")         presetOpenDialog();
    else if (k == "presetFolder") presetPickFolder();
    else if (k == "pngExport")    pngExport();
    else if (k == "pngImport")    pngImport();
    else if (k == "hello")   { uiHasState.store (false); emitInitialState(); }
    else if (k == "bwfx")    { if (bwfx_juce::handleMessage (bwfxRack, apvts, payload)) emitBwfx(); }
}

void HighTideAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

//==============================================================================
void HighTideAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> ps;
    for (int i = 0; i < numParams(); ++i)
    {
        const auto& s = paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", s.id);
        //  the table is UTF-8 source; a bare char* would be read as Latin-1
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
    o->setProperty ("build", HT_BUILD_ID);
    juce::Array<juce::var> fac;
    for (int i = 0; i < numFactory(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", juce::String (factory (i).name));
        e->setProperty ("g", juce::String (factory (i).group));
        fac.add (juce::var (e));
    }
    o->setProperty ("factory", fac);
    auto* pn = new juce::DynamicObject();
    pn->setProperty ("name", patchName); pn->setProperty ("user", patchIsUser);
    o->setProperty ("patch", juce::var (pn));
    emitToUi ("initialState", juce::var (o));
    uiHasState.store (true);
    emitTerrain(); emitLanes(); emitBwfx(); emitPatch(); presetScan();
}

void HighTideAudioProcessor::emitTerrain()
{
    if (! emitToUi) return;
    const auto& t = engine.terrain();
    auto* o = new juce::DynamicObject();
    o->setProperty ("nx", NX); o->setProperty ("nz", NZ);
    o->setProperty ("xmin", XMIN); o->setProperty ("xmax", XMAX); o->setProperty ("umax", UMAX);
    o->setProperty ("d", juce::Base64::toBase64 (t.u.data(), t.u.size() * sizeof (float)));
    emitToUi ("terrain", juce::var (o));
}

void HighTideAudioProcessor::emitLanes()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("j", lanesToVar (lanes));
    emitToUi ("lanes", juce::var (o));
}

void HighTideAudioProcessor::emitPatch()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", patchName);
    o->setProperty ("user", patchIsUser);
    emitToUi ("patch", juce::var (o));
}

void HighTideAudioProcessor::emitBwfx()
{
    if (! emitToUi) return;
    emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

void HighTideAudioProcessor::emitBalls()
{
    if (! emitToUi || ! uiHasState.load()) return;
    static BallView views[MAXVOICES];
    const int n = engine.voicesView (views, MAXVOICES);
    static float scope[SCOPE_N];
    engine.scopeView (scope);

    auto* o = new juce::DynamicObject();
    o->setProperty ("t", engine.engineTime());
    o->setProperty ("lvl", (double) engine.outLevel());
    o->setProperty ("scope", juce::Base64::toBase64 (scope, sizeof (scope)));
    juce::Array<juce::var> vs;
    for (int i = 0; i < n; ++i)
    {
        const BallView& b = views[i];
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", b.id); e->setProperty ("n", b.note); e->setProperty ("h", b.held);
        e->setProperty ("age", (double) b.age); e->setProperty ("z", (double) b.z); e->setProperty ("zt", (double) b.zt);
        e->setProperty ("x", (double) b.x); e->setProperty ("xa", (double) b.xa); e->setProperty ("e", (double) b.e);
        e->setProperty ("tide", (double) b.tide); e->setProperty ("rock", (double) b.rock);
        juce::Array<juce::var> u;
        for (int q = 1; q < b.nBalls; ++q) { u.add ((double) b.bx[q]); u.add ((double) b.bz[q]); }
        e->setProperty ("u", u);
        vs.add (juce::var (e));
    }
    o->setProperty ("v", vs);
    emitToUi ("balls", juce::var (o));
}

//==============================================================================
juce::var HighTideAudioProcessor::lanesToVar (const Lanes& l) const
{
    auto laneVar = [] (const Lane& ln)
    {
        auto pts = [] (const std::vector<LanePoint>& v)
        {
            juce::Array<juce::var> a;
            for (const auto& q : v) { auto* e = new juce::DynamicObject(); e->setProperty ("t", (double) q.t); e->setProperty ("v", (double) q.v); e->setProperty ("e", q.e); a.add (juce::var (e)); }
            return juce::var (a);
        };
        auto* o = new juce::DynamicObject();
        o->setProperty ("on", pts (ln.on));
        o->setProperty ("off", pts (ln.off));
        if (ln.hasLoop) { auto* lp = new juce::DynamicObject(); lp->setProperty ("a", (double) ln.loopA); lp->setProperty ("b", (double) ln.loopB); o->setProperty ("loop", juce::var (lp)); }
        else o->setProperty ("loop", juce::var());
        return juce::var (o);
    };
    auto* o = new juce::DynamicObject();
    o->setProperty ("ver", 1);
    o->setProperty ("pin", laneVar (l.pin));
    o->setProperty ("tide", laneVar (l.tide));
    o->setProperty ("rock", laneVar (l.rock));
    return juce::var (o);
}

Lanes HighTideAudioProcessor::lanesFromVar (const juce::var& v) const
{
    Lanes l;
    auto readLane = [] (const juce::var& lv, Lane& ln)
    {
        auto pts = [] (const juce::var& arr, std::vector<LanePoint>& out)
        {
            out.clear();
            if (auto* a = arr.getArray())
                for (const auto& q : *a)
                {
                    LanePoint p;
                    p.t = (float) (double) q.getProperty ("t", 0.0);
                    p.v = (float) (double) q.getProperty ("v", 0.0);
                    p.e = (int) q.getProperty ("e", 2);
                    out.push_back (p);
                }
        };
        pts (lv.getProperty ("on", juce::var()), ln.on);
        pts (lv.getProperty ("off", juce::var()), ln.off);
        const auto lp = lv.getProperty ("loop", juce::var());
        if (lp.isObject())
        {
            ln.hasLoop = true;
            ln.loopA = (float) (double) lp.getProperty ("a", 0.0);
            ln.loopB = (float) (double) lp.getProperty ("b", 1.0);
        }
    };
    if (v.isObject())
    {
        readLane (v.getProperty ("pin", juce::var()), l.pin);
        readLane (v.getProperty ("tide", juce::var()), l.tide);
        readLane (v.getProperty ("rock", juce::var()), l.rock);
    }
    return l;
}

void HighTideAudioProcessor::applyParams (const Params& p)
{
    Params q = p;
    for (int i = 0; i < numParams(); ++i)
        setParamById (paramSpec (i).id, paramSpec (i).get (q), false);
}

void HighTideAudioProcessor::applyFactory (int i)
{
    const int n = numFactory();
    if (n <= 0) return;
    i = juce::jlimit (0, n - 1, i);
    Terrain t; Lanes l; Params p;
    factory (i).build (t, l, p);
    engine.terrain().setAll (t.u.data());
    lanes = l; engine.setLanes (lanes);
    applyParams (p);
    bwfxRack.fromJson ("");
    patchName = factory (i).name; patchIsUser = false;
}

//==============================================================================
juce::String HighTideAudioProcessor::terrainPngBase64() const
{
    const auto png = png16::write (engine.terrain().u.data(), NX, NZ, UMAX);
    return juce::Base64::toBase64 (png.getData(), png.getSize());
}

bool HighTideAudioProcessor::applyTerrainPngBase64 (const juce::String& b64)
{
    juce::MemoryOutputStream mo;
    if (! juce::Base64::convertFromBase64 (mo, b64)) return false;
    std::vector<float> vals; int w = 0, h = 0;
    if (! png16::read (mo.getData(), mo.getDataSize(), vals, w, h, UMAX)) return false;
    //  resample to NX x NZ (bilinear) if the image is another size
    std::vector<float> out ((size_t) NX * NZ);
    for (int j = 0; j < NZ; ++j)
        for (int i = 0; i < NX; ++i)
        {
            const float fx = (float) i / (float) (NX - 1) * (float) (w - 1), fy = (float) j / (float) (NZ - 1) * (float) (h - 1);
            const int x0 = juce::jlimit (0, w - 1, (int) fx), y0 = juce::jlimit (0, h - 1, (int) fy);
            const int x1 = juce::jmin (w - 1, x0 + 1), y1 = juce::jmin (h - 1, y0 + 1);
            const float tx = fx - (float) x0, ty = fy - (float) y0;
            const float a = vals[(size_t) y0 * w + x0], b = vals[(size_t) y0 * w + x1];
            const float c = vals[(size_t) y1 * w + x0], d = vals[(size_t) y1 * w + x1];
            out[(size_t) j * NX + i] = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
        }
    engine.terrain().setAll (out.data());
    return true;
}

//==============================================================================
void HighTideAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson().c_str()), nullptr);
    state.setProperty ("terrain", terrainPngBase64(), nullptr);
    state.setProperty ("lanes", juce::JSON::toString (lanesToVar (lanes), true), nullptr);
    state.setProperty ("patchName", patchName, nullptr);
    state.setProperty ("patchUser", patchIsUser, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HighTideAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        const juce::String rack = tree.getProperty ("bwfx", juce::String()).toString();
        const juce::String terr = tree.getProperty ("terrain", juce::String()).toString();
        const juce::String ln   = tree.getProperty ("lanes", juce::String()).toString();
        patchName = tree.getProperty ("patchName", patchName).toString();
        patchIsUser = (bool) tree.getProperty ("patchUser", false);
        for (const char* k : { "bwfx", "terrain", "lanes", "patchName", "patchUser" }) tree.removeProperty (k, nullptr);
        apvts.replaceState (tree);
        bwfxRack.fromJson (rack.toRawUTF8());
        if (terr.isNotEmpty()) applyTerrainPngBase64 (terr);
        if (ln.isNotEmpty()) { juce::var v; if (! juce::JSON::parse (ln, v).failed()) { lanes = lanesFromVar (v); engine.setLanes (lanes); } }
        emitInitialState();
    }
}

//==============================================================================
//  the house patch files
juce::PropertiesFile& HighTideAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "HighTide";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File HighTideAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved) && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = brokild::patchFolder ("High Tide", {});
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("High Tide Patches");
        }
    }
    return presetFolder;
}

void HighTideAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var HighTideAudioProcessor::presetScanDir (const juce::File& dir, int depth)
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

void HighTideAudioProcessor::presetScan()
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

void HighTideAudioProcessor::presetPickFolder()
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

juce::String HighTideAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i) pv->setProperty (ids[i], (double) raw[(size_t) i]->load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "high-tide");
    o->setProperty ("kind", "patch");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("build", juce::String (HT_BUILD_ID));
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("terrain", terrainPngBase64());
    o->setProperty ("lanes", lanesToVar (lanes));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (o), false);
}

void HighTideAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject()) { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "high-tide") { notice ("THAT IS NOT A HIGH TIDE PATCH"); return; }
    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr) { setParamById (kv.name.toString(), (float) (double) kv.value, false); ++applied; }
    if (v.hasProperty ("terrain")) applyTerrainPngBase64 (v.getProperty ("terrain", "").toString());
    lanes = lanesFromVar (v.getProperty ("lanes", juce::var())); engine.setLanes (lanes);
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    patchName = name; patchIsUser = true;
    emitTerrain(); emitLanes(); emitBwfx(); emitPatch();
    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" - " + juce::String (applied) + " VALUES, TERRAIN, PINS");
}

void HighTideAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto suggested = dir.getChildFile ((patchIsUser ? patchName : juce::String ("Tide")) + ".json");
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

void HighTideAudioProcessor::presetOpenDialog()
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

void HighTideAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

void HighTideAudioProcessor::pngExport()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Export the terrain as a 16-bit PNG", dir.getChildFile (patchName + ".png"), "*.png");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            if (! file.hasFileExtension ("png")) file = file.withFileExtension ("png");
            const auto png = png16::write (engine.terrain().u.data(), NX, NZ, UMAX);
            const bool ok = file.replaceWithData (png.getData(), png.getSize());
            notice (ok ? "TERRAIN WRITTEN: " + file.getFileName().toUpperCase() : "COULD NOT WRITE THE PNG");
        });
}

void HighTideAudioProcessor::pngImport()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Import a heightmap PNG as the terrain", dir, "*.png");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            juce::MemoryBlock mb; f.loadFileAsData (mb);
            std::vector<float> vals; int w = 0, h = 0;
            if (! png16::read (mb.getData(), mb.getSize(), vals, w, h, UMAX)) { notice ("THAT IS NOT A PNG I CAN READ (greyscale or RGB, not interlaced)"); return; }
            juce::MemoryBlock png = png16::write (vals.data(), w, h, UMAX);
            applyTerrainPngBase64 (juce::Base64::toBase64 (png.getData(), png.getSize()));
            patchIsUser = false; patchName = f.getFileNameWithoutExtension();
            emitTerrain(); emitPatch();
            notice ("TERRAIN FROM " + f.getFileName().toUpperCase() + " (" + juce::String (w) + " x " + juce::String (h) + ")");
        });
}

juce::AudioProcessorEditor* HighTideAudioProcessor::createEditor() { return new HighTideAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HighTideAudioProcessor(); }
