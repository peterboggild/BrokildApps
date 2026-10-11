#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "brokild_paths.h"

#ifndef MT_BUILD_ID
 #define MT_BUILD_ID "0.0.0"
#endif

namespace
{
    // The panel's view of the tape path, cm relative to the record head.
    constexpr int    VIEW_POINTS = 240;
    constexpr double VIEW_FROM   = -6.0;
    constexpr double VIEW_TO     = 27.0;

    juce::NormalisableRange<float> rangeFor (const mt::Spec& s)
    {
        if (s.kind == mt::Kind::Log)
        {
            const float lo = s.lo, hi = s.hi;
            return { lo, hi,
                     [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                     [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); } };
        }
        return { s.lo, s.hi };
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MagnetoTransportAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < mt::NUM_SPECS; ++i)
    {
        const auto& s = mt::SPECS[i];
        const juce::ParameterID pid { s.id, 1 };
        switch (s.kind)
        {
            case mt::Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, s.name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
                break;
            case mt::Kind::Toggle:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
                break;
            case mt::Kind::Lin:
            case mt::Kind::Log:
            {
                // what a host prints: the physical value with the precision it deserves
                const juce::String unit (s.unit);
                const int dec = unit == "%" ? 0 : (unit == "dB" ? 1 : 2);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, rangeFor (s), s.def,
                    juce::AudioParameterFloatAttributes().withLabel (s.unit)
                        .withStringFromValueFunction ([dec] (float v, int) { return juce::String (v, dec); })));
            }
                break;
        }
    }
    return layout;
}

//==============================================================================
MagnetoTransportAudioProcessor::MagnetoTransportAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAGNETO", createParameterLayout())
{
    for (int i = 0; i < mt::NUM_SPECS; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (mt::SPECS[i].id);
        lastSent[(size_t) i] = -1.0e9f;
    }
    quality = juce::jlimit (0, 2, settings().getIntValue ("quality", 1));   // a fresh instance takes the user's last choice
    engine.setQuality (quality);
    snapshotPreset ("Default", -1);                      // a fresh instance is the default machine, clean
    startTimerHz (30);
}

MagnetoTransportAudioProcessor::~MagnetoTransportAudioProcessor()
{
    stopTimer();
}

bool MagnetoTransportAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // mono -> mono, mono -> stereo, stereo -> stereo
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out == juce::AudioChannelSet::mono())   return in == juce::AudioChannelSet::mono();
    if (out == juce::AudioChannelSet::stereo()) return in == juce::AudioChannelSet::mono()
                                                    || in == juce::AudioChannelSet::stereo();
    return false;
}

//==============================================================================
void MagnetoTransportAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < mt::NUM_SPECS; ++i)
        current.*(mt::SPECS[i].member) = raw[(size_t) i]->load();
    engine.setParams (current);
    engine.setQuality (quality);
    engine.prepare (sampleRate, samplesPerBlock);
    lastRate = sampleRate; lastBlock = samplesPerBlock;
    setLatencySamples (engine.latencySamples());
    scratchL.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    scratchR.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
}

void MagnetoTransportAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // The table, in any order: each row knows its own member.
    for (int i = 0; i < mt::NUM_SPECS; ++i)
        current.*(mt::SPECS[i].member) = raw[(size_t) i]->load();
    engine.setParams (current);

    double bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto t = pos->getBpm()) bpm = *t;
    engine.setBpm (bpm);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    if (programme.load() && wrapperType == wrapperType_Standalone)
    {
        if ((int) prog.size() < n) prog.resize ((size_t) n);
        renderProgramme (prog.data(), n);
        inL = inR = prog.data();
    }

    if (numOut > 1)
    {
        // The engine reads both inputs before it writes a sample, so in place is safe;
        // a mono input feeds the same pointer to both sides.
        engine.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else
    {
        if ((int) scratchL.size() < n) { scratchL.resize ((size_t) n); scratchR.resize ((size_t) n); }
        engine.process (inL, inL, scratchL.data(), scratchR.data(), n);
        float* o = buffer.getWritePointer (0);
        for (int k = 0; k < n; ++k) o[k] = 0.5f * (scratchL[(size_t) k] + scratchR[(size_t) k]);
    }
}

//==============================================================================
void MagnetoTransportAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        // ACCUMULATE: the loop's wear and the loop it belongs to travel with the project
        double w; int st; double len;
        engine.wearState (w, st, len);
        xml->setAttribute ("mtWear", w);
        xml->setAttribute ("mtWearStock", st);
        xml->setAttribute ("mtWearLoop", len);
        xml->setAttribute ("mtQuality", quality);
        xml->setAttribute ("mtPreset", presetName);
        xml->setAttribute ("mtPresetFactory", presetFactory);
        copyXmlToBinary (*xml, dest);
    }
}

void MagnetoTransportAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    if (xml->hasAttribute ("mtWear"))
        engine.setWearState (xml->getDoubleAttribute ("mtWear"), xml->getIntAttribute ("mtWearStock", -1), xml->getDoubleAttribute ("mtWearLoop"));
    const int q = xml->getIntAttribute ("mtQuality", quality);
    const juce::String pn = xml->getStringAttribute ("mtPreset", "Default");
    const int pf = xml->getIntAttribute ("mtPresetFactory", -1);
    for (auto* a : { "mtWear", "mtWearStock", "mtWearLoop", "mtQuality", "mtPreset", "mtPresetFactory" }) xml->removeAttribute (a);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    snapshotPreset (pn, pf);
    if (q != quality) setQuality (q);
    uiHasState = false;
}

//==============================================================================
/*  The standalone's programme: a four-bar phrase of plucked notes at 120 BPM, each a
    damped tone with a second harmonic, so the echoes have something to be echoes of.
    Only the standalone ever renders it (processBlock checks the wrapper). */
void MagnetoTransportAudioProcessor::renderProgramme (float* out, int n)
{
    static const float notes[16] = { 220.0f, 0, 261.63f, 329.63f, 0, 392.0f, 329.63f, 0,
                                     220.0f, 261.63f, 0, 329.63f, 0, 196.0f, 0, 0 };
    const double fs = lastRate > 0 ? lastRate : 48000.0, step = 0.25, bar = 16 * step;
    for (int i = 0; i < n; ++i)
    {
        const double t = std::fmod (progT, bar);
        const int k = (int) (t / step);
        const double tn = t - k * step;
        const float f = notes[k];
        float v = 0.0f;
        if (f > 0)
        {
            const double env = (1.0 - std::exp (-tn * 400.0)) * std::exp (-tn * 9.0);
            const double ph = 2.0 * juce::MathConstants<double>::pi * f * tn;
            v = (float) (0.45 * env * (std::sin (ph) + 0.35 * std::sin (2.0 * ph) + 0.12 * std::sin (3.0 * ph)));
        }
        out[i] = v;
        progT += 1.0 / fs;
    }
}

float MagnetoTransportAudioProcessor::physical (int i) const
{
    return raw[(size_t) i]->load();
}

void MagnetoTransportAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (! payload.isObject()) return;
    const juce::String k = payload.getProperty ("k", {}).toString();

    if (k == "hello")
    {
        uiHasState = false;
        uiReady = true;
        emitInitialState();
        return;
    }
    if (k == "stateack") { uiHasState = true; return; }

    if (k == "touch")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        if (auto* prm = apvts.getParameter (id))
        {
            // Wrapped in a gesture or a host's learn-from-touch never sees it.
            if ((bool) payload.getProperty ("down", false)) prm->beginChangeGesture();
            else                                            prm->endChangeGesture();
        }
        return;
    }

    if (k == "patternSet") { patternSet(); return; }
    if (k == "presetList") { emitPresets(); return; }
    if (k == "presetLoad")
    {
        if (payload.hasProperty ("factory")) loadFactory ((int) payload.getProperty ("factory", -1));
        else loadUser (payload.getProperty ("user", {}).toString());
        emitPresets();
        return;
    }
    if (k == "presetSave") { saveUser (payload.getProperty ("name", {}).toString()); emitPresets(); return; }
    if (k == "presetDelete") { deleteUser (payload.getProperty ("user", {}).toString()); emitPresets(); return; }
    if (k == "quality") { setQuality ((int) payload.getProperty ("v", 1)); return; }
    if (k == "programme") { programme = (bool) payload.getProperty ("v", false); return; }
    if (k == "settings")
    {
        if (payload.hasProperty ("tooltips")) settings().setValue ("tooltips", (bool) payload.getProperty ("tooltips", true));
        if (payload.hasProperty ("delay"))    settings().setValue ("delay", juce::jlimit (0.2, 3.0, (double) payload.getProperty ("delay", 2.0)));
        settings().saveIfNeeded();
        return;
    }

    if (k == "p")
    {
        // The page speaks physical units (cm, cm/s, dB, index); the host
        // parameter is normalised.
        const juce::String id = payload.getProperty ("id", {}).toString();
        const float v = (float) (double) payload.getProperty ("v", 0.0);
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            for (int i = 0; i < mt::NUM_SPECS; ++i)
                if (id == mt::SPECS[i].id) lastSent[(size_t) i] = physical (i);   // no echo
        }
        return;
    }
}

void MagnetoTransportAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("build", MT_BUILD_ID);
    obj->setProperty ("rate", engine.internalRate());

    juce::Array<juce::var> params;
    for (int i = 0; i < mt::NUM_SPECS; ++i)
    {
        const auto& s = mt::SPECS[i];
        auto* po = new juce::DynamicObject();
        po->setProperty ("id", s.id);
        po->setProperty ("name", s.name);
        po->setProperty ("kind", s.kind == mt::Kind::Lin ? "lin" : s.kind == mt::Kind::Log ? "log"
                                : s.kind == mt::Kind::Choice ? "choice" : "toggle");
        po->setProperty ("lo", s.lo);
        po->setProperty ("hi", s.hi);
        po->setProperty ("def", s.def);
        po->setProperty ("unit", s.unit);
        po->setProperty ("choices", s.choices ? juce::String (s.choices) : juce::String());
        po->setProperty ("hint", juce::String (juce::CharPointer_UTF8 (s.hint)));
        po->setProperty ("v", physical (i));
        params.add (juce::var (po));
        lastSent[(size_t) i] = physical (i);
    }
    obj->setProperty ("params", params);
    obj->setProperty ("quality", quality);
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("tooltips", settings().getBoolValue ("tooltips", true));
        so->setProperty ("delay", settings().getDoubleValue ("delay", 2.0));
        obj->setProperty ("settings", juce::var (so));
    }
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("name", presetName);
        po->setProperty ("dirty", presetDirty());
        obj->setProperty ("preset", juce::var (po));
    }
    emitToUi ("initialState", juce::var (obj));
}

//==============================================================================
juce::PropertiesFile& MagnetoTransportAudioProcessor::settings()
{
    if (settingsFile == nullptr)
    {
        // the user's preferences, not the project's: %APPDATA%\Brokild\Magneto Transport.settings
        juce::PropertiesFile::Options o;
        o.applicationName = "Magneto Transport";
        o.folderName = "Brokild";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        settingsFile = std::make_unique<juce::PropertiesFile> (o);
    }
    return *settingsFile;
}

void MagnetoTransportAudioProcessor::setQuality (int q)
{
    q = juce::jlimit (0, 2, q);
    settings().setValue ("quality", q);
    settings().saveIfNeeded();
    if (q == quality && engine.getQuality() == q) return;
    quality = q;
    // the internal rate changes: a fresh tape, threaded with the audio callback held off
    if (lastRate > 0.0)
    {
        suspendProcessing (true);
        engine.setQuality (q);
        engine.prepare (lastRate, lastBlock);
        suspendProcessing (false);
        setLatencySamples (engine.latencySamples());
    }
    else engine.setQuality (q);
}

void MagnetoTransportAudioProcessor::patternSet()
{
    // HEAD PATTERN (BUGLIST 1): a one-shot snap in rail order around the tempo head,
    // written to the three position parameters once; the carriages travel there
    double rail[3], out[3]; int mask = 0;
    for (int i = 0; i < 3; ++i) rail[i] = engine.tele.headCm[i].load();
    const int pt = (int) std::lround (physical (mt::indexOf ("pattern")));
    const int th = (int) std::lround (physical (mt::indexOf ("synchead")));
    mt::headPatternRail (pt, th, rail, out, mask);
    const char* ids[3] = { "h1pos", "h2pos", "h3pos" };
    for (int i = 0; i < 3; ++i)
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ids[i])))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) out[i]));
            prm->endChangeGesture();
        }
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    o->setProperty ("ok", mask == 0);
    o->setProperty ("outMask", mask);
    juce::Array<juce::var> pos; for (double v : out) pos.add (v);
    o->setProperty ("pos", pos);
    emitToUi ("patternResult", juce::var (o));
}

//==============================================================================
void MagnetoTransportAudioProcessor::applyParams (const mt::Params& p)
{
    for (int i = 0; i < mt::NUM_SPECS; ++i)
    {
        const auto& s = mt::SPECS[i];
        if (juce::String (s.id) == "bypass" || juce::String (s.id) == "mech") continue;
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (s.id)))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(s.member)));
            prm->endChangeGesture();
        }
    }
}

void MagnetoTransportAudioProcessor::snapshotPreset (const juce::String& name, int factory)
{
    presetName = name; presetFactory = factory;
    for (int i = 0; i < mt::NUM_SPECS; ++i) presetSnap[(size_t) i] = physical (i);
    presetSnapValid = true;
}

bool MagnetoTransportAudioProcessor::presetDirty() const
{
    if (! presetSnapValid) return false;
    for (int i = 0; i < mt::NUM_SPECS; ++i)
    {
        const juce::String id (mt::SPECS[i].id);
        if (id == "bypass" || id == "clear" || id == "run" || id == "mech") continue;   // performance, not the patch
        const float v = raw[(size_t) i]->load();
        if (std::abs (v - presetSnap[(size_t) i]) > 1.0e-4f * (1.0f + std::abs (v))) return true;
    }
    return false;
}

void MagnetoTransportAudioProcessor::loadFactory (int i)
{
    if (i < 0 || i >= mt::NUM_FACTORY) return;
    mt::Params p;
    for (int s = 0; s < mt::NUM_SPECS; ++s) p.*(mt::SPECS[s].member) = physical (s);
    if (! mt::applyFactoryPreset (i, p)) return;
    applyParams (p);
    snapshotPreset (mt::FACTORY[i].name, i);
}

juce::File MagnetoTransportAudioProcessor::userFolder() const
{
    return brokild::patchFolder ("Magneto Transport", { "magneto-transport" });
}

bool MagnetoTransportAudioProcessor::saveUser (const juce::String& nameIn)
{
    const juce::String name = nameIn.trim();
    if (name.isEmpty() || name.containsAnyOf ("\\/:*?\"<>|") || name.length() > 60) return false;
    const auto dir = userFolder();
    if (! dir.isDirectory()) return false;
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "magneto-transport");
    o->setProperty ("build", MT_BUILD_ID);
    o->setProperty ("name", name);
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < mt::NUM_SPECS; ++i)
        if (juce::String (mt::SPECS[i].id) != "bypass" && juce::String (mt::SPECS[i].id) != "mech") pv->setProperty (mt::SPECS[i].id, physical (i));
    o->setProperty ("params", juce::var (pv));
    if (! dir.getChildFile (name + ".json").replaceWithText (juce::JSON::toString (juce::var (o)))) return false;
    snapshotPreset (name, -1);
    return true;
}

bool MagnetoTransportAudioProcessor::loadUser (const juce::String& name)
{
    const auto f = userFolder().getChildFile (name + ".json");
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v.getProperty ("app", {}).toString() != "magneto-transport") return false;
    const auto pv = v.getProperty ("params", {});
    mt::Params p;
    // what the file names, over the defaults: a parameter added after it was saved
    // takes its default, which is what the patch would have sounded like then
    for (int s = 0; s < mt::NUM_SPECS; ++s)
    {
        const auto& sp = mt::SPECS[s];
        float val = sp.def;
        if (juce::String (sp.id) == "bypass" || juce::String (sp.id) == "mech") val = physical (s);
        else if (pv.hasProperty (sp.id)) val = juce::jlimit (sp.lo, sp.hi, (float) (double) pv.getProperty (sp.id, sp.def));
        p.*(sp.member) = val;
    }
    applyParams (p);
    snapshotPreset (name, -1);
    return true;
}

void MagnetoTransportAudioProcessor::deleteUser (const juce::String& name)
{
    const auto dir = userFolder();
    const auto f = dir.getChildFile (name + ".json");
    if (! f.existsAsFile()) return;
    const auto bin = dir.getChildFile ("Deleted");
    bin.createDirectory();
    f.moveFileTo (bin.getNonexistentChildFile (name, ".json", false));
}

void MagnetoTransportAudioProcessor::emitPresets()
{
    if (! emitToUi) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> fac, usr;
    for (int i = 0; i < mt::NUM_FACTORY; ++i)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("index", i); e->setProperty ("name", mt::FACTORY[i].name); e->setProperty ("group", mt::FACTORY[i].group);
        fac.add (juce::var (e));
    }
    const auto dir = userFolder();
    if (dir.isDirectory())
    {
        auto list = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        list.sort();
        for (const auto& f : list)
        {
            auto* e = new juce::DynamicObject();
            e->setProperty ("name", f.getFileNameWithoutExtension());
            usr.add (juce::var (e));
        }
    }
    o->setProperty ("factory", fac);
    o->setProperty ("user", usr);
    auto* c = new juce::DynamicObject();
    c->setProperty ("name", presetName); c->setProperty ("factory", presetFactory); c->setProperty ("dirty", presetDirty());
    o->setProperty ("current", juce::var (c));
    emitToUi ("presets", juce::var (o));
}

//==============================================================================
void MagnetoTransportAudioProcessor::timerCallback()
{
    engine.service();          // tape allocation for splices; must run with the editor closed

    // REPRO: the host is told head 1's delay (from the message thread, as JUCE wants)
    if (const int lat = engine.latencySamples(); lat != getLatencySamples())
        setLatencySamples (lat);

    if (! emitToUi) return;
    if (! uiHasState)
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // Echo whatever the page did not move itself: automation, host undo, a
    // state load. A control that works but never moves on screen reads as broken.
    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < mt::NUM_SPECS; ++i)
        {
            const float v = physical (i);
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-6f * (1.0f + std::abs (v)))
            {
                lastSent[(size_t) i] = v;
                auto* po = new juce::DynamicObject();
                po->setProperty ("id", mt::SPECS[i].id);
                po->setProperty ("v", v);
                changed.add (juce::var (po));
            }
        }
        if (! changed.isEmpty())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("params", changed);
            emitToUi ("hostParam", juce::var (obj));
        }
    }

    // The machine, as the simulation has it.
    {
        auto& t = engine.tele;
        auto* o = new juce::DynamicObject();
        o->setProperty ("speed", t.speedCms.load());
        o->setProperty ("target", t.targetCms.load());
        o->setProperty ("cap", t.capstanAngle.load());
        o->setProperty ("supA", t.supplyAngle.load());
        o->setProperty ("takA", t.takeupAngle.load());
        o->setProperty ("supR", t.supplyR.load());
        o->setProperty ("takR", t.takeupR.load());
        o->setProperty ("loopA", t.loopAngle.load());
        o->setProperty ("mode", t.mode.load());
        o->setProperty ("loopLen", t.loopLenM.load());
        o->setProperty ("syncOn", t.syncActive.load());
        o->setProperty ("syncClamp", t.syncClamped.load());
        o->setProperty ("splice", t.splicing.load());
        o->setProperty ("in", t.inPeak.exchange (0.0f));
        o->setProperty ("out", t.outPeak.exchange (0.0f));
        o->setProperty ("rec", t.recPeak.exchange (0.0f));
        o->setProperty ("wf", t.wowFlutter.load());
        o->setProperty ("wear", t.wear.load());
        o->setProperty ("stock", t.stock.load());
        o->setProperty ("looper", t.looper.load());
        o->setProperty ("recording", t.recording.load());
        o->setProperty ("eraseHc", t.eraseHc.load());
        o->setProperty ("reRecDb", t.reRecDb.load());
        o->setProperty ("barsFit", t.barsFit.load());
        o->setProperty ("barsNow", t.barsNow.load());
        {
            juce::Array<juce::var> be;
            for (int i = 0; i < 6; ++i) be.add (t.brokenEff[i].load());
            o->setProperty ("brokenEff", be);
        }
        o->setProperty ("clogUm", t.clogUm.load());
        o->setProperty ("huntPct", t.huntPct.load());
        o->setProperty ("crease", t.crease.load());
        o->setProperty ("repro", t.repro.load());
        o->setProperty ("reproMs", t.reproMs.load());
        o->setProperty ("vuIn", 10.0 * std::log10 (juce::jmax (1.0e-12, t.inMs.load())));
        o->setProperty ("vuOut", 10.0 * std::log10 (juce::jmax (1.0e-12, t.outMs.load())));
        o->setProperty ("quality", t.quality.load());
        o->setProperty ("latencyMs", 1000.0 * getLatencySamples() / juce::jmax (1.0, getSampleRate()));
        {
            // HEAD PATTERN: would the current pattern fit, from where the heads are?
            double rail[3], out[3]; int mask = 0;
            for (int i = 0; i < 3; ++i) rail[i] = t.headCm[i].load();
            mt::headPatternRail ((int) std::lround (physical (mt::indexOf ("pattern"))), (int) std::lround (physical (mt::indexOf ("synchead"))), rail, out, mask);
            auto* pf = new juce::DynamicObject();
            pf->setProperty ("mask", mask);
            o->setProperty ("patternFit", juce::var (pf));
        }
        if (++presetTick >= 15) { presetTick = 0; const bool d = presetDirty(); if (d != lastDirty) { lastDirty = d; emitPresets(); } }
        juce::Array<juce::var> hp, hv, hk;
        for (int i = 0; i < mt::NUM_HEADS; ++i)
        {
            hp.add (t.headCm[i].load());
            hv.add (t.headVelCms[i].load());
            hk.add (t.headPeak[i].exchange (0.0f));
        }
        o->setProperty ("heads", hp);
        o->setProperty ("headVel", hv);
        o->setProperty ("headPk", hk);

        float view[VIEW_POINTS];
        engine.tapeView (view, VIEW_POINTS, VIEW_FROM, VIEW_TO);
        juce::Array<juce::var> tv;
        tv.ensureStorageAllocated (VIEW_POINTS);
        for (float x : view) tv.add (juce::roundToInt (juce::jlimit (0.0f, 4.0f, x) * 1000.0f));
        o->setProperty ("tape", tv);
        o->setProperty ("tapeFrom", VIEW_FROM);
        o->setProperty ("tapeTo", VIEW_TO);
        emitToUi ("tele", juce::var (o));
    }
}

//==============================================================================
juce::AudioProcessorEditor* MagnetoTransportAudioProcessor::createEditor()
{
    return new MagnetoTransportAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MagnetoTransportAudioProcessor();
}
