#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String fmtValue (const n84::PSpec& s, float v)
    {
        using namespace n84;
        switch (s.kind)
        {
            case KP_SW:    return v >= 0.5f ? "ON" : "OFF";
            case KP_HZ:
            {
                const float f = xmap (v, s.lo, s.hi);
                return f < 100.0f ? juce::String (f, 2) + " Hz"
                     : f < 1000.0f ? juce::String (f, 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
            }
            case KP_MS:
            {
                const float ms = xmap (v, s.lo, s.hi);
                return ms < 100.0f ? juce::String (ms, 1) + " ms"
                     : ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
            }
            case KP_SEC:   return juce::String (xmap (v, s.lo, s.hi), 2) + " s";
            case KP_GLIDE:
            {
                const float ms = v < 0.01f ? 0.0f : xmap (v, s.lo, s.hi);
                return ms <= 0.0f ? "OFF" : (ms < 1000.0f ? juce::String (ms, 0) + " ms" : juce::String (ms / 1000.0f, 2) + " s");
            }
            case KP_LIST:
            {
                int n = 0; auto names = listNames (s.id, n);
                const int i = juce::jlimit (0, std::max (0, n - 1), (int) std::round (v));
                return names != nullptr && n > 0 ? juce::String (names[i]) : juce::String (i);
            }
            case KP_INT:
            {
                const int i = (int) std::round (v);
                if (juce::String (s.id) == "patch") return juce::String (patchName (i));
                return juce::String (i) + (juce::String (s.id) == "bend" ? " semi" : "");
            }
            case KP_SEMI:  { const int st = (int) std::round ((v - 0.5f) * 24.0f); return (st > 0 ? "+" : "") + juce::String (st) + " semi"; }
            case KP_CENT:  { const float c = (v - 0.5f) * s.lo; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " cents"; }
            case KP_CENTU: return juce::String (v * s.lo, 0) + " cents";
            case KP_BIPOL: { const float c = (v - 0.5f) * 200.0f; return (c >= 0 ? "+" : "") + juce::String (c, 0) + " %"; }
            case KP_VOL:   { const float g = volGain (v); return g <= 0.0f ? "-inf dB" : juce::String (20.0f * std::log10 (g), 1) + " dB"; }
            case KP_PW:    return juce::String (50.0f + v * 45.0f, 0) + " %";
            default:       return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Nineteen84AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < n84::numParams(); ++i)
    {
        const auto& s = n84::paramSpec (i);
        const float hi = n84::paramMax (s);
        const bool stepped = s.kind == n84::KP_LIST || s.kind == n84::KP_INT || s.kind == n84::KP_SW;
        const juce::String sid (s.id);
        const bool automatable = ! (sid == "os" || sid == "patch");   // selectors, not lanes
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi, stepped ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes()
                .withAutomatable (automatable)
                .withStringFromValueFunction ([i] (float v, int) { return fmtValue (n84::paramSpec (i), v); })));
    }
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
Nineteen84AudioProcessor::Nineteen84AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NINETEEN84", createParameterLayout())
{
    for (int i = 0; i < n84::numParams(); ++i) ids.add (n84::paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);
    for (auto& h : midiHeld) h = false;

    applyPatchIndex (0);              // a fresh instance opens on BLADE BRASS
    startTimerHz (15);                // bwfxRack.service(), editor open or not
    bwfxRack.setWorldModConsumed (true);
}

Nineteen84AudioProcessor::~Nineteen84AudioProcessor() = default;

bool Nineteen84AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Nineteen84AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < n84::numParams(); ++i)
        n84::paramSpec (i).ref (engine.p) = raw[(size_t) i]->load();
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

void Nineteen84AudioProcessor::pushEvent (const UiEvent& e)
{
    const int w = evWrite.load();
    const int next = (w + 1) % EVQ;
    if (next == evRead.load()) return;
    evq[(size_t) w] = e;
    evWrite.store (next);
}

//==============================================================================
void Nineteen84AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    buffer.clear();

    if (wantPanic.exchange (false)) { engine.reset(); for (auto& h : midiHeld) h = false; }

    for (int i = 0; i < n84::numParams(); ++i)
        n84::paramSpec (i).ref (engine.p) = raw[(size_t) i]->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 999.0) { engine.p.bpm = *bpm; bwfxRack.setBpm (*bpm); }

    {
        const bwfx::WorldMod wm = bwfxRack.worldMod();
        engine.setWorldMod (wm.detuneCents, wm.panSpread, wm.tremDepth, wm.tremRate, wm.pitchSag, wm.filterMul);
    }

    while (evRead.load() != evWrite.load())
    {
        const auto e = evq[(size_t) evRead.load()];
        evRead.store ((evRead.load() + 1) % EVQ);
        switch (e.kind)
        {
            case 1: engine.noteOn (e.note, e.v); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = true; break;
            case 2: engine.noteOff (e.note); midiHeld[(size_t) juce::jlimit (0, 127, e.note)] = false; break;
            case 3: engine.setBend (e.v); hostBend = e.v; break;
            case 4: engine.setWheel (e.v); hostWheel = e.v; break;
            case 5: engine.allNotesOff(); for (auto& h : midiHeld) h = false; break;
            case 6: engine.setAftertouch (e.v); hostAt = e.v; break;
            default: break;
        }
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = nch >= 2 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> monoR;
    if (R == nullptr) { monoR.assign ((size_t) n, 0.0f); R = monoR.data(); }

    int last = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        if (pos > last) { engine.process (L + last, R + last, pos - last); last = pos; }
        const auto m = meta.getMessage();
        if      (m.isNoteOn())        { engine.noteOn (m.getNoteNumber(), m.getFloatVelocity()); midiHeld[(size_t) m.getNoteNumber()] = true; }
        else if (m.isNoteOff())       { engine.noteOff (m.getNoteNumber()); midiHeld[(size_t) m.getNoteNumber()] = false; }
        else if (m.isPitchWheel())    { const float b = (m.getPitchWheelValue() - 8192) / 8192.0f; engine.setBend (b); hostBend = b; }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc == 1)       { engine.setWheel (v / 127.0f); hostWheel = v / 127.0f; }
            else if (cc == 64) engine.setSustain (v >= 64);
            else if (cc == 120 || cc == 123) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
        }
        else if (m.isChannelPressure()) { engine.setAftertouch (m.getChannelPressureValue() / 127.0f); hostAt = m.getChannelPressureValue() / 127.0f; }
        else if (m.isAftertouch())      { engine.setPolyAftertouch (m.getNoteNumber(), m.getAfterTouchValue() / 127.0f); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.allNotesOff(); for (auto& h : midiHeld) h = false; }
    }
    if (n > last) engine.process (L + last, R + last, n - last);

    bwfx_juce::pushMacros (bwfxRack, apvts);
    bwfxRack.process (L, R, n);

    if (nch == 1) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

//==============================================================================
void Nineteen84AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        xml->setAttribute ("loadedName", loadedName);
        copyXmlToBinary (*xml, dest);
    }
}

void Nineteen84AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            loadedName = xml->getStringAttribute ("loadedName");
            xml->removeAttribute ("bwfx");
            xml->removeAttribute ("loadedName");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
            lastPatchSent = -2;
            emitBwfx();
        }
}

juce::AudioProcessorEditor* Nineteen84AudioProcessor::createEditor()
{
    return new Nineteen84AudioProcessorEditor (*this);
}

//==============================================================================
void Nineteen84AudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void Nineteen84AudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

void Nineteen84AudioProcessor::applyParamsStruct (const n84::Params& q)
{
    n84::Params copy = q;
    for (int i = 0; i < n84::numParams(); ++i)
    {
        const auto& s = n84::paramSpec (i);
        const juce::String id (s.id);
        if (id == "os" || id == "volume" || id == "bend" || id == "tune" || id == "fine") continue;   // the player's, not the patch's
        setParamById (s.id, s.ref (copy));
    }
}

void Nineteen84AudioProcessor::applyPatchIndex (int i)
{
    i = juce::jlimit (0, n84::numPatches() - 1, i);
    n84::Params q;
    n84::applyPatch (i, q);
    applyParamsStruct (q);
    setParamById ("patch", (float) i);
    bwfxRack.clearState();            // a patch stores its own rack; the factory ones carry none
    emitBwfx();
    loadedName = {};
    lastPatchSent = -2;
    emitPatchInfo();
    notice (juce::String ("PATCH ") + juce::String (i + 1).paddedLeft ('0', 2) + " " + DOT + " " + n84::patchName (i) + " " + DOT + " " + n84::patchCategory (i));
}

void Nineteen84AudioProcessor::randomise()
{
    n84::Params q;
    n84::randomPatch ((uint32_t) juce::Time::getHighResolutionTicks(), q);
    applyParamsStruct (q);
    loadedName = "RANDOM";
    lastPatchSent = -2;
    emitPatchInfo();
    notice ("A NEW MACHINE " + DOT + " RANDOM");
}

//==============================================================================
void Nineteen84AudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* o = payload.getDynamicObject())
    {
        const auto batch = o->getProperty ("b");
        if (auto* arr = batch.getArray()) { for (const auto& m : *arr) handleOne (m); return; }
    }
    handleOne (payload);
}

void Nineteen84AudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")
    {
        setParamById (o->getProperty ("id").toString(), (float) (double) o->getProperty ("v"), true);
        if (loadedName.isEmpty()) { loadedName = "*"; lastPatchSent = -2; }   // edited
    }
    else if (k == "ack")    { uiHasState = true; }
    else if (k == "ready")  { uiReady = true; }
    else if (k == "hello")  { uiHasState = false; }
    else if (k == "patch")  { applyPatchIndex ((int) o->getProperty ("i")); }
    else if (k == "random") { randomise(); }
    else if (k == "panic")  { wantPanic = true; notice ("ALL NOTES OFF " + DOT + " RESET"); }
    else if (k == "note")
    {
        UiEvent e; e.kind = (bool) o->getProperty ("on") ? 1 : 2; e.note = (int) o->getProperty ("n");
        e.v = o->hasProperty ("v") ? (float) (double) o->getProperty ("v") : 0.8f;
        pushEvent (e);
    }
    else if (k == "bend")   { UiEvent e; e.kind = 3; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "wheel")  { UiEvent e; e.kind = 4; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "at")     { UiEvent e; e.kind = 6; e.v = (float) (double) o->getProperty ("v"); pushEvent (e); }
    else if (k == "alloff") { UiEvent e; e.kind = 5; pushEvent (e); }
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (o->getProperty ("path").toString()); }
}

//==============================================================================
void Nineteen84AudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = n84::paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", (double) n84::paramMax (s));
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("lo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        e->setProperty ("rank", s.rank);
        int nn = 0; auto names = n84::listNames (s.id, nn);
        if (names != nullptr && nn > 0)
        {
            juce::Array<juce::var> list;
            for (int j = 0; j < nn; ++j) list.add (juce::String (names[j]));
            e->setProperty ("names", list);
        }
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }
    juce::Array<juce::var> patches;
    for (int i = 0; i < n84::numPatches(); ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("n", i);
        e->setProperty ("name", juce::String (n84::patchName (i)));
        e->setProperty ("cat", juce::String (n84::patchCategory (i)));
        patches.add (juce::var (e));
    }
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("product", "1984");
    obj->setProperty ("params", ps);
    obj->setProperty ("patches", patches);
   #ifdef N84_BUILD_ID
    obj->setProperty ("build", juce::String (N84_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
    lastPatchSent = -2;
    emitPatchInfo();
}

void Nineteen84AudioProcessor::emitPatchInfo()
{
    if (! emitToUi) return;
    const int i = (int) std::round (raw[(size_t) ids.indexOf ("patch")]->load());
    auto* o = new juce::DynamicObject();
    if (loadedName.isEmpty())
    {
        o->setProperty ("i", i);
        o->setProperty ("name", juce::String (n84::patchName (i)));
        o->setProperty ("cat", juce::String (n84::patchCategory (i)));
    }
    else if (loadedName == "*")
    {
        o->setProperty ("i", i);
        o->setProperty ("name", juce::String (n84::patchName (i)) + " *");
        o->setProperty ("cat", juce::String (n84::patchCategory (i)));
    }
    else
    {
        o->setProperty ("i", -1);
        o->setProperty ("name", loadedName.toUpperCase());
        o->setProperty ("cat", juce::String ("USER"));
    }
    emitToUi ("patchinfo", juce::var (o));
    lastPatchSent = i;
}

void Nineteen84AudioProcessor::emitBwfx()
{
    if (emitToUi) emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void Nineteen84AudioProcessor::timerService()
{
    if (! emitToUi) return;
    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }
    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < ids.size(); ++i)
        {
            const float v = raw[(size_t) i]->load();
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-5f)
            {
                lastSent[(size_t) i] = v;
                auto* e = new juce::DynamicObject();
                e->setProperty ("id", ids[i]);
                e->setProperty ("v", (double) v);
                changed.add (juce::var (e));
            }
        }
        if (! changed.isEmpty())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("p", changed);
            emitToUi ("hostParam", juce::var (obj));
        }
    }
    {
        const int i = (int) std::round (raw[(size_t) ids.indexOf ("patch")]->load());
        if (i != lastPatchSent) { if (loadedName == "*" || loadedName.isEmpty()) loadedName = {}; emitPatchInfo(); }
    }
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("out",  (double) std::sqrt (std::max (0.0f, engine.outRms)));
        obj->setProperty ("peak", (double) engine.outPeak);
        juce::Array<juce::var> feg, aeg, cut, notes, levels, held, scope;
        for (int r = 0; r < n84::NUM_RANKS; ++r) { feg.add ((double) engine.uiFeg[(size_t) r]); aeg.add ((double) engine.uiAeg[(size_t) r]); cut.add ((double) engine.uiCut[(size_t) r]); }
        for (int v = 0; v < n84::MAX_VOICES; ++v) { notes.add (engine.uiNotes[(size_t) v]); levels.add ((double) engine.uiLevel[(size_t) v]); }
        for (int n = 0; n < 128; ++n) if (midiHeld[(size_t) n]) held.add (n);
        const int w = engine.scopeWrite;
        for (int i = 0; i < 512; ++i) scope.add ((double) engine.scope[(size_t) ((w - 512 + i) & (n84::SCOPE_N - 1))]);
        obj->setProperty ("feg", feg); obj->setProperty ("aeg", aeg); obj->setProperty ("cut", cut);
        obj->setProperty ("lfo", (double) engine.uiLfo);
        obj->setProperty ("wow", (double) engine.uiWow);
        obj->setProperty ("drop", engine.uiDrop);
        obj->setProperty ("notes", notes); obj->setProperty ("levels", levels); obj->setProperty ("held", held);
        obj->setProperty ("scope", scope);
        obj->setProperty ("bend", (double) hostBend.load());
        obj->setProperty ("wheel", (double) hostWheel.load());
        obj->setProperty ("at", (double) hostAt.load());
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
// Patches on disk — the same shape as the other Brokild plugins.
juce::PropertiesFile& Nineteen84AudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Nineteen84";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File Nineteen84AudioProcessor::installedPresetFolder()
{
    return brokild::patchFolder ("1984", { "\"app\":\"1984\"" });
}

juce::File Nineteen84AudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved) && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = installedPresetFolder();
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("1984 Presets");
        }
    }
    return presetFolder;
}

void Nineteen84AudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var Nineteen84AudioProcessor::presetScanDir (const juce::File& dir, int depth)
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

void Nineteen84AudioProcessor::presetScan()
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

void Nineteen84AudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory()) start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    activeChooser = std::make_unique<juce::FileChooser> ("Where the patches live", start);
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{} || ! dir.isDirectory()) return;
            rememberPresetFolder (dir);
            presetScan();
        });
}

juce::String Nineteen84AudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i) pv->setProperty (ids[i], (double) raw[(size_t) i]->load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "1984");
    o->setProperty ("kind", "preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef N84_BUILD_ID
    o->setProperty ("build", juce::String (N84_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));
    return juce::JSON::toString (juce::var (o), false);
}

void Nineteen84AudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject()) { notice ("THAT FILE IS NOT A PATCH"); return; }
    if (v.getProperty ("app", "").toString() != "1984") { notice ("THAT IS NOT A 1984 PATCH"); return; }
    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
        {
            const auto id = kv.name.toString();
            if (id == "os" || id == "volume") continue;
            if (apvts.getParameter (id) != nullptr) { setParamById (id, (float) (double) kv.value); ++applied; }
        }
    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();
    loadedName = name;
    lastSent.assign ((size_t) ids.size(), -999.0f);
    lastPatchSent = -2;
    emitPatchInfo();
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void Nineteen84AudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory()) dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto suggested = dir.getChildFile ("Nocturne.json");
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
                const auto chosen = file.getParentDirectory();
                const auto root = presetFolderOrDefault();
                if (chosen != root && ! chosen.isAChildOf (root)) rememberPresetFolder (chosen);
                loadedName = name; lastPatchSent = -2; emitPatchInfo();
            }
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\"" : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void Nineteen84AudioProcessor::presetOpenDialog()
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

void Nineteen84AudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PATCH IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Nineteen84AudioProcessor();
}
