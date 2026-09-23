#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bwfx_juce.h"
#include "brokild_paths.h"

namespace
{
    const juce::String DOT = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));

    juce::String fmtValue (const hf::PSpec& s, float v)
    {
        using namespace hf;
        switch (s.kind)
        {
            case KP_SW:    return v >= 0.5f ? "ON" : "OFF";
            case KP_HZ:
            {
                const float f = xmap (v, s.lo, s.hi);
                return f < 1000.0f ? juce::String (f, 0) + " Hz"
                                   : juce::String (f / 1000.0f, 2) + " kHz";
            }
            case KP_MS:
            {
                const float ms = xmap (v, s.lo, s.hi);
                return ms < 100.0f ? juce::String (ms, 1) + " ms"
                                   : juce::String (ms, 0) + " ms";
            }
            case KP_DB:
            {
                // special spans that are not centred on zero
                if (juce::String (s.id) == "gatethr") return juce::String (hf::lerp (-70.0f, -20.0f, v), 1) + " dB";
                if (juce::String (s.id) == "cthr")    return juce::String (hf::lerp (-50.0f, 0.0f, v), 1) + " dB";
                if (juce::String (s.id) == "cknee")   return juce::String (hf::lerp (0.5f, 18.0f, v), 1) + " dB";
                if (juce::String (s.id) == "cmake")   return juce::String (v * 24.0f, 1) + " dB";
                const float d = (v - 0.5f) * s.lo;
                return (d >= 0 ? "+" : "") + juce::String (d, 1) + " dB";
            }
            case KP_CEIL:  return juce::String (hf::lerp (s.lo, s.hi, v), 1) + " dB";
            case KP_RATIO: return juce::String (xmap (v, s.lo, s.hi), 1)
                                + (juce::String (s.id).endsWith ("q") ? "" : " : 1");
            case KP_OS:    return v < 0.5f ? "1x" : (v < 1.5f ? "2x" : "4x");
            case KP_CHAOS: return "r = " + juce::String (chaosR (v), 3);
            case KP_SUB:
            {
                const float r = subRatio (v);
                if (std::abs (r - 0.25f) < 0.02f)   return "f/4";
                if (std::abs (r - 0.3333f) < 0.02f) return "f/3";
                if (std::abs (r - 0.5f) < 0.03f)    return "f/2";
                if (std::abs (r - 1.0f) < 0.05f)    return "f";
                return juce::String (r, 2) + " f";
            }
            case KP_THROAT:
            {
                const float pc = (v - 0.5f) * 70.0f;
                return (pc >= 0 ? "+" : "") + juce::String (pc, 0) + " %";
            }
            default:       return juce::String ((int) std::round (v * 100.0f)) + " %";
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HairfryerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < hf::numParams(); ++i)
    {
        const auto& s = hf::paramSpec (i);
        const float hi = s.kind == hf::KP_OS ? 2.0f : 1.0f;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, 1 }, s.name,
            juce::NormalisableRange<float> (0.0f, hi,
                                            s.kind == hf::KP_OS || s.kind == hf::KP_SW ? 1.0f : 0.0f),
            s.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [i] (float v, int) { return fmtValue (hf::paramSpec (i), v); })));
    }
    //  the rack's five automatable macros, declared by shared code so
    //  every synth carries the identical five (see bwfx_juce.h)
    bwfx_juce::addMacroParameters (layout);
    return layout;
}

//==============================================================================
HairfryerAudioProcessor::HairfryerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HAIRFRYER", createParameterLayout())
{
    for (int i = 0; i < hf::numParams(); ++i) ids.add (hf::paramSpec (i).id);
    raw.reserve ((size_t) ids.size());
    for (const auto& id : ids) raw.push_back (apvts.getRawParameterValue (id));
    lastSent.assign ((size_t) ids.size(), -999.0f);

    startTimerHz (15);        // bwfxRack.service() - editor open or not
}

HairfryerAudioProcessor::~HairfryerAudioProcessor() = default;

bool HairfryerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void HairfryerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    bwfxMonoR.assign ((size_t) juce::jmax (64, samplesPerBlock), 0.0f);
    setLatencySamples (engine.latencySamples());
}

//==============================================================================
void HairfryerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;

    if (wantPanic.exchange (false)) engine.reset();

    // every value, straight through the table
    for (int i = 0; i < hf::numParams(); ++i)
        hf::paramSpec (i).get (engine.p) = raw[(size_t) i]->load();

    engine.process (buffer.getWritePointer (0),
                    nch >= 2 ? buffer.getWritePointer (1) : nullptr, n);

    // host tempo for the world rack's synced modules
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                bwfxRack.setBpm (*bpm);

    bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros

    // The world rack: one extra stage after the engine (empty = untouched).
    auto* L = buffer.getWritePointer (0);
    if (nch >= 2)
        bwfxRack.process (L, buffer.getWritePointer (1), n);
    else if ((int) bwfxMonoR.size() >= n)
    {
        std::memcpy (bwfxMonoR.data(), L, sizeof (float) * (size_t) n);
        bwfxRack.process (L, bwfxMonoR.data(), n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + bwfxMonoR[(size_t) i]);
    }
}

//==============================================================================
void HairfryerAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));
        copyXmlToBinary (*xml, dest);
    }
}

void HairfryerAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());
            emitBwfx();
            xml->removeAttribute ("bwfx");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            uiHasState = false;
            lastSent.assign ((size_t) ids.size(), -999.0f);
        }
}

juce::AudioProcessorEditor* HairfryerAudioProcessor::createEditor()
{
    return new HairfryerAudioProcessorEditor (*this);
}

//==============================================================================
void HairfryerAudioProcessor::setParamById (const juce::String& id, float value, bool fromUi)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        const int idx = ids.indexOf (id);
        if (idx >= 0) lastSent[(size_t) idx] = fromUi ? value : -999.0f;
    }
}

void HairfryerAudioProcessor::notice (const juce::String& msg)
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("msg", msg);
    emitToUi ("notice", juce::var (o));
}

/*  A Params struct — a recipe, a randomisation — becomes host parameter
    moves through the same table that reads them back. */
void HairfryerAudioProcessor::applyParamsStruct (const hf::Params& q)
{
    hf::Params copy = q;
    for (int i = 0; i < hf::numParams(); ++i)
        setParamById (hf::paramSpec (i).id, hf::paramSpec (i).get (copy));
}

/*  Random cooks a character, not a mix: the strip's levels, the master
    gains and the switches stay where they are; the basket and the heat get
    rolled. */
void HairfryerAudioProcessor::randomiseAll()
{
    juce::Random r (juce::Time::getHighResolutionTicks());

    auto set = [this] (const char* id, float v) { setParamById (id, v); };
    auto roll = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };

    set ("engon", 1.0f);
    set ("blend", roll (0.5f, 1.0f));
    set ("crisp", roll (0.3f, 0.95f));

    const bool two = r.nextFloat() < 0.4f;
    for (const char* pre : { "a", "b" })
    {
        const bool isB = pre[0] == 'b';
        auto id = [pre] (const char* s) { return juce::String (pre) + s; };
        const float lvl = isB ? (two ? roll (0.35f, 0.7f) : 0.0f) : roll (0.55f, 0.85f);
        setParamById (id ("level"), lvl);
        setParamById (id ("chaos"), r.nextFloat());
        setParamById (id ("grip"),  roll (0.3f, 0.95f));
        setParamById (id ("jit"),   roll (0.0f, 0.7f));
        setParamById (id ("rasp"),  roll (0.0f, 0.85f));
        setParamById (id ("tone"),  roll (0.25f, 0.85f));
        setParamById (id ("fold"),  roll (0.0f, 0.6f));
        setParamById (id ("split"), roll (0.0f, 0.8f));
        setParamById (id ("ratio"), r.nextFloat());
        setParamById (id ("throat"),roll (0.2f, 0.8f));
    }

    set ("drive",   roll (0.15f, 0.6f));
    set ("tube",    roll (0.1f, 0.7f));
    set ("sparkle", roll (0.0f, 0.6f));

    notice ("A NEW BASKET " + DOT + " CHAOS DIALLED");
}

//==============================================================================
void HairfryerAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (auto* o = payload.getDynamicObject())
    {
        const auto batch = o->getProperty ("b");
        if (auto* arr = batch.getArray())
        {
            for (const auto& m : *arr) handleOne (m);
            return;
        }
    }
    handleOne (payload);
}

void HairfryerAudioProcessor::handleOne (const juce::var& m)
{
    auto* o = m.getDynamicObject();
    if (o == nullptr) return;
    const juce::String k = o->getProperty ("k").toString();

    if (k == "p")
    {
        setParamById (o->getProperty ("id").toString(),
                      (float) (double) o->getProperty ("v"), true);
    }
    else if (k == "ack")    { uiHasState = true; }
    else if (k == "ready")  { uiReady = true; }
    /*  The page says hello every time it BOOTS — including a reload the
        editor never sees (WebView2 crash recovery, a devtools reload).
        Without this the processor still believes the old page's ack and
        never re-sends initialState, and the new page waits forever. */
    else if (k == "hello")  { uiHasState = false; }
    else if (k == "random") { randomiseAll(); }
    else if (k == "panic")  { wantPanic = true; notice ("RESET"); }
    else if (k == "recipe")
    {
        const int i = (int) o->getProperty ("i");
        hf::Params q;
        hf::applyRecipe (i, q);
        applyParamsStruct (q);
        bwfxRack.clearState();               // a patch stores its own rack
        emitBwfx();
        notice (juce::String ("PROGRAMME ") + hf::recipeName (i) + " " + DOT + " "
                + juce::String (hf::recipeBlurb (i)).toUpperCase());
    }
    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, apvts, m)) emitBwfx(); }
    else if (k == "save")   { presetSaveAs(); }
    else if (k == "open")   { presetOpenDialog(); }
    else if (k == "presetScan")   { presetScan(); }
    else if (k == "presetFolder") { presetPickFolder(); }
    else if (k == "presetLoad")   { presetLoad (o->getProperty ("path").toString()); }
}

//==============================================================================
void HairfryerAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    juce::Array<juce::var> ps;
    for (int i = 0; i < ids.size(); ++i)
    {
        const auto& s = hf::paramSpec (i);
        auto* e = new juce::DynamicObject();
        e->setProperty ("id", ids[i]);
        e->setProperty ("v", (double) raw[(size_t) i]->load());
        e->setProperty ("hi", s.kind == hf::KP_OS ? 2.0 : 1.0);
        e->setProperty ("n", juce::String (s.name));
        e->setProperty ("kind", s.kind);
        e->setProperty ("def", (double) s.def);
        e->setProperty ("flo", (double) s.lo);
        e->setProperty ("fhi", (double) s.hi);
        ps.add (juce::var (e));
        lastSent[(size_t) i] = raw[(size_t) i]->load();
    }

    juce::Array<juce::var> recipes, blurbs;
    for (int i = 0; i < hf::NUM_RECIPES; ++i)
    {
        recipes.add (juce::String (hf::recipeName (i)));
        blurbs.add (juce::String (hf::recipeBlurb (i)));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", ps);
    obj->setProperty ("recipes", recipes);
    obj->setProperty ("blurbs", blurbs);
    obj->setProperty ("latency", engine.latencySamples());
   #ifdef HF_BUILD_ID
    obj->setProperty ("build", juce::String (HF_BUILD_ID));
   #else
    obj->setProperty ("build", juce::String ("dev"));
   #endif
    emitToUi ("initialState", juce::var (obj));
    emitBwfx();
}

void HairfryerAudioProcessor::emitBwfx()
{
    if (emitToUi)
        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));
}

//==============================================================================
void HairfryerAudioProcessor::timerService()
{
    if (! emitToUi) return;

    if (! uiHasState.load())
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // anything the host or another editor moved
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

    // ---- meters -----------------------------------------------------------
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("in",  (double) std::sqrt (std::max (0.0f, engine.inRms)));
        obj->setProperty ("out", (double) std::sqrt (std::max (0.0f, engine.outRms)));
        obj->setProperty ("ggr", (double) engine.gateGr);
        obj->setProperty ("dgr", (double) engine.deEssGr);
        obj->setProperty ("cgr", (double) engine.compGr);
        obj->setProperty ("lgr", (double) engine.limGr);
        obj->setProperty ("f0",  (double) engine.f0Hz);
        obj->setProperty ("cl",  (double) engine.clarity);
        obj->setProperty ("mdb", (double) engine.matchDb);
        obj->setProperty ("oa",  engine.orderA);
        obj->setProperty ("ob",  engine.orderB);

        juce::Array<juce::var> ga, gb;
        for (int i = 0; i < hf::GAIN_HIST; ++i)
        {
            ga.add ((double) engine.gainsA[(size_t) i]);
            gb.add ((double) engine.gainsB[(size_t) i]);
        }
        obj->setProperty ("ga", ga);
        obj->setProperty ("gb", gb);
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
// Patches on disk — the same shape as the other Brokild plugins.
namespace
{
    bool canWriteInto (const juce::File& dir)
    {
        if (! dir.isDirectory()) return false;
        const auto probe = dir.getChildFile (".hairfryer-write-test.tmp");
        if (! probe.replaceWithText ("x")) return false;
        probe.deleteFile();
        return true;
    }
}

juce::PropertiesFile& HairfryerAudioProcessor::userSettings()
{
    if (settings == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Hairfryer";
        o.filenameSuffix      = "settings";
        o.folderName          = "Brokild";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settings;
}

juce::File HairfryerAudioProcessor::installedPresetFolder()
{
    /*  Documents/Brokild patches/Hairfryer/ — see brokild_paths.h.
        It used to be a folder beside the installed bundle, shared with
        every other Brokild plugin; the old contents are migrated once,
        by copying, so nothing there is disturbed. */
    return brokild::patchFolder ("Hairfryer", { "\"hairfryer\"" });
}

juce::File HairfryerAudioProcessor::presetFolderOrDefault()
{
    if (presetFolder == juce::File{})
    {
        const auto saved = userSettings().getValue ("presetFolder", {});
        //  ...unless it is one the plugin wrote down for itself, in a
        //  place an installer replaces. See brokild_paths.h.
        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved)
            && ! brokild::isUnsafePatchFolder (juce::File (saved)))
            presetFolder = juce::File (saved);
        else
        {
            presetFolder = installedPresetFolder();
            if (presetFolder == juce::File{})
                presetFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile ("Hairfryer Presets");
        }
    }
    return presetFolder;
}

void HairfryerAudioProcessor::rememberPresetFolder (const juce::File& dir)
{
    if (! dir.isDirectory()) return;
    presetFolder = dir;
    userSettings().setValue ("presetFolder", dir.getFullPathName());
    userSettings().saveIfNeeded();
}

juce::var HairfryerAudioProcessor::presetScanDir (const juce::File& dir, int depth)
{
    juce::Array<juce::var> items;
    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*",
                                                        juce::File::findFilesAndDirectories))
    {
        const auto f = e.getFile();
        if (f.isHidden()) continue;
        if (f.isDirectory())
        {
            if (depth >= 2) continue;
            auto kids = presetScanDir (f, depth + 1);
            if (kids.size() == 0) continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileName());
            o->setProperty ("d", true);
            o->setProperty ("i", kids);
            items.add (juce::var (o));
        }
        else if (f.hasFileExtension ("json"))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("n", f.getFileNameWithoutExtension());
            o->setProperty ("p", f.getFullPathName());
            items.add (juce::var (o));
        }
    }
    struct Sorter
    {
        static int compareElements (const juce::var& a, const juce::var& b)
        {
            const bool da = (bool) a.getProperty ("d", false);
            const bool db = (bool) b.getProperty ("d", false);
            if (da != db) return da ? -1 : 1;
            return a.getProperty ("n", "").toString().compareNatural (b.getProperty ("n", "").toString());
        }
    };
    Sorter sorter;
    items.sort (sorter);
    return juce::var (items);
}

void HairfryerAudioProcessor::presetScan()
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

void HairfryerAudioProcessor::presetPickFolder()
{
    auto start = presetFolderOrDefault();
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Where the presets live", start);
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (dir == juce::File{} || ! dir.isDirectory()) return;
            rememberPresetFolder (dir);
            presetScan();
        });
}

juce::String HairfryerAudioProcessor::patchJson (const juce::String& name)
{
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < ids.size(); ++i)
        pv->setProperty (ids[i], (double) raw[(size_t) i]->load());

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "hairfryer");
    o->setProperty ("kind", "preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
   #ifdef HF_BUILD_ID
    o->setProperty ("build", juce::String (HF_BUILD_ID));
   #endif
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // a patch stores its own rack
    return juce::JSON::toString (juce::var (o), false);
}

void HairfryerAudioProcessor::applyPatchJson (const juce::String& json, const juce::String& name)
{
    juce::var v;
    if (juce::JSON::parse (json, v).failed() || ! v.isObject())
    { notice ("THAT FILE IS NOT A PRESET"); return; }
    if (v.getProperty ("app", "").toString() != "hairfryer")
    { notice ("THAT IS NOT A HAIRFRYER PRESET"); return; }

    int applied = 0;
    if (auto* o = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& kv : o->getProperties())
            if (apvts.getParameter (kv.name.toString()) != nullptr)
            { setParamById (kv.name.toString(), (float) (double) kv.value); ++applied; }

    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());
    emitBwfx();

    lastSent.assign ((size_t) ids.size(), -999.0f);
    notice ("LOADED \"" + name.toUpperCase() + "\" " + DOT + " " + juce::String (applied) + " VALUES");
}

void HairfryerAudioProcessor::presetSaveAs()
{
    auto dir = presetFolderOrDefault();
    dir.createDirectory();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    const auto suggested = dir.getChildFile ("Basket.json");

    activeChooser = std::make_unique<juce::FileChooser> ("Save this preset", suggested, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
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
            }
            notice (ok ? "SAVED \"" + name.toUpperCase() + "\""
                       : "COULD NOT WRITE " + file.getFullPathName().toUpperCase());
            presetScan();
        });
}

void HairfryerAudioProcessor::presetOpenDialog()
{
    auto dir = presetFolderOrDefault();
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeChooser = std::make_unique<juce::FileChooser> ("Open a preset", dir, "*.json");
    activeChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File{} || ! f.existsAsFile()) return;
            presetLoad (f.getFullPathName());
        });
}

void HairfryerAudioProcessor::presetLoad (const juce::String& path)
{
    const juce::File f (path);
    if (! f.existsAsFile()) { notice ("THAT PRESET IS NO LONGER THERE"); presetScan(); return; }
    applyPatchJson (f.loadFileAsString(), f.getFileNameWithoutExtension());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HairfryerAudioProcessor();
}
