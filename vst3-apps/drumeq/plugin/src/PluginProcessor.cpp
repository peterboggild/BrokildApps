#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <brokild_paths.h>

namespace
{
    juce::String valueText (const de::PSpec& s, float v)
    {
        const juce::String u (s.unit);
        if (s.kind == de::K_CHOICE)
            return juce::StringArray::fromTokens (s.choices, "|", "")[juce::jmax (0, juce::roundToInt (v))];
        if (s.kind == de::K_BOOL) return v > 0.5f ? "ON" : "OFF";
        if (u == "Hz")
        {
            if (s.member == &de::Params::csc && v <= 20.5f) return "OFF";
            return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                : juce::String (juce::roundToInt (v)) + " Hz";
        }
        if (u == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s"
                                           : (v < 10.0f ? juce::String (v, v < 1.0f ? 2 : 1) : juce::String (juce::roundToInt (v))) + " ms";
        if (u == "%")  return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        if (u == "x")  return juce::String (v, v < 10.0f ? 1 : 0) + ":1";
        if (u == "Q")  return juce::String (v, 2);
        //  a RANGE and a KNEE are amounts, not gains: no sign
        if (u == "dB" && (s.member == &de::Params::grange || s.member == &de::Params::cknee)) return juce::String (v, 1) + " dB";
        if (u == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
        return juce::String (v, 2);
    }

    juce::var paramsToVar (const de::Params& p)
    {
        auto* o = new juce::DynamicObject();
        for (int i = 0; i < de::kNumParams; ++i)
        {
            const auto& s = de::specs()[i];
            if (s.member == &de::Params::source) continue;
            o->setProperty (s.id, p.*(s.member));
        }
        return juce::var (o);
    }

    void varToParams (const juce::var& v, de::Params& p)
    {
        if (auto* d = v.getDynamicObject())
            for (const auto& kv : d->getProperties())
                if (kv.name.toString() != "source")
                    de::setById (p, kv.name.toString().toRawUTF8(), (float) (double) kv.value);
    }

    int sourceByName (const juce::String& n)
    {
        for (int s = 0; s < de::kNumSources; ++s)
            if (n.equalsIgnoreCase (de::source (s).name)) return s;
        return -1;
    }

    juce::String safeFileName (const juce::String& s)
    {
        return juce::File::createLegalFileName (s.trim()).replaceCharacters ("/\\", "--");
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DrumEQProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < de::kNumParams; ++i)
    {
        const auto& s = de::specs()[i];
        const juce::ParameterID pid { s.id, 1 };
        if (s.kind == de::K_CHOICE)
            l.add (std::make_unique<juce::AudioParameterChoice> (
                pid, s.name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def,
                juce::AudioParameterChoiceAttributes().withAutomatable (s.automatable)));
        else if (s.kind == de::K_BOOL)
            l.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f,
                juce::AudioParameterBoolAttributes().withAutomatable (s.automatable)));
        else
        {
            juce::NormalisableRange<float> r (s.lo, s.hi);
            if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
            const de::PSpec* sp = &s;
            l.add (std::make_unique<juce::AudioParameterFloat> (
                pid, s.name, r, s.def,
                juce::AudioParameterFloatAttributes()
                    .withAutomatable (s.automatable)
                    .withStringFromValueFunction ([sp] (float v, int) { return valueText (*sp, v); })));
        }
    }
    return l;
}

DrumEQProcessor::DrumEQProcessor() : DrumEQProcessor (ProxyTag {})
{
    proxyMode = false;
    kit.open();
    startTimerHz (30);      // 30 Hz: a MASTER's knob drag reaches the track without audible steps
}

DrumEQProcessor::DrumEQProcessor (ProxyTag)
    : AudioProcessor (BusesProperties()
                        .withInput  ("In",  juce::AudioChannelSet::mono(), true)
                        .withOutput ("Out", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "DRUMEQ", layout())
{
    for (int i = 0; i < de::kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (de::specs()[i].id);

    //  a fresh instance is the KICK IN's STANDARD
    applyParams (de::presetParams (de::S_KICK_IN, 0));
    lastSource = de::S_KICK_IN;
    proxyMode = true;       // the plain constructor turns this off and joins the kit
}

DrumEQProcessor::~DrumEQProcessor()
{
    stopTimer();
    if (masterOn) kit.releaseMaster();
    kit.close();
}

bool DrumEQProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void DrumEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0);
}

de::Params DrumEQProcessor::currentParams() const
{
    de::Params p;
    for (int i = 0; i < de::kNumParams; ++i)
        p.*(de::specs()[i].member) = raw[(size_t) i]->load();
    return p;
}

int DrumEQProcessor::currentSource() const
{
    return juce::jlimit (0, de::kNumSources - 1, juce::roundToInt (raw[0]->load()));
}

void DrumEQProcessor::applyParams (const de::Params& p)
{
    for (int i = 0; i < de::kNumParams; ++i)
        if (auto* prm = apvts.getParameter (de::specs()[i].id))
        {
            const float v = p.*(de::specs()[i].member);
            const float n = prm->convertTo0to1 (v);
            if (std::abs (prm->getValue() - n) > 1.0e-7f) prm->setValueNotifyingHost (n);
        }
}

//==============================================================================
void DrumEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, buffer.getNumChannels(), getTotalNumInputChannels());
    if (nch <= 0 || n <= 0) return;
    const de::Params p = currentParams();
    float* ch[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (p, ch, nch, n);
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

//==============================================================================
void DrumEQProcessor::selectSource (int s)
{
    s = juce::jlimit (0, de::kNumSources - 1, s);
    if (auto* prm = apvts.getParameter ("source"))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) s));
    kitTick();
}

void DrumEQProcessor::loadFactoryPreset (int i)
{
    const int s = currentSource();
    applyParams (de::presetParams (s, i));
    presetName = de::preset (s, i).name;
}

void DrumEQProcessor::applyKitValues (const float* v, int n, const juce::String& name)
{
    de::Params p = currentParams();
    if (n >= de::kKitValues) de::fromKitValues (p, v);
    applyParams (p);
    presetName = name;
    //  what we just took is what the kit already has: not a change to publish
    de::toKitValues (currentParams(), lastPublished.data());
    lastPublishedName = presetName;
    publishedOnce = true;
}

void DrumEQProcessor::kitTick()
{
    if (proxyMode) return;
    const int s = currentSource();

    //  ---- a new drum on this track ----------------------------------------
    if (s != lastSource)
    {
        lastSource = s;
        float v[drumkit::kVals]; int n = 0; std::string nm;
        //  a linked instance joins the kit: if the session (or a loaded kit)
        //  already has this drum, take it; otherwise the drum's STANDARD
        if (link && kit.read (s, v, n, nm) && n >= de::kKitValues)
            applyKitValues (v, n, juce::String (nm));
        else
        {
            applyParams (de::presetParams (s, 0));
            presetName = de::preset (s, 0).name;
            publishedOnce = false;
        }
        seenEditSeq = kit.editSeqOf (s);         // a new drum does not replay an old edit
        if (masterOn && editTarget == s) editTarget = -1;
    }

    if (! kit.isOpen()) return;
    kit.beat (s);
    if (masterPending) { masterPending = false; if (link) setMaster (true); }
    {
        const juce::String shared (kit.kitName());
        if (shared.isNotEmpty()) kitLabel = shared;       // the session's kit has one name
    }

    //  ---- a kit was loaded somewhere -----------------------------------------
    uint32_t gen = 0;
    if (kit.newKit (gen))
    {
        kitLabel = juce::String (kit.kitName());
        if (link || forceKitApply)
        {
            float v[drumkit::kVals]; int n = 0; std::string nm; uint32_t g = 0;
            if (kit.read (s, v, n, nm, &g) && g == gen && n >= de::kKitValues)
                applyKitValues (v, n, juce::String (nm));
        }
    }
    forceKitApply = false;

    //  ---- a MASTER edited this drum: every linked track on it takes it ------------
    if (link)
    {
        float v[drumkit::kVals]; int n = 0; std::string nm; drumkit::Link::Meta m;
        if (kit.read (s, v, n, nm, nullptr, &m) && m.editSeq != seenEditSeq)
        {
            seenEditSeq = m.editSeq;
            if (m.writer != kit.myId() && n >= de::kKitValues) applyKitValues (v, n, juce::String (nm));
        }
    }

    //  ---- this track as the MASTER --------------------------------------------------
    if (masterOn && (! link || ! kit.isMaster()))
    {
        //  unlinked, or another track took the role
        masterOn = false;
        editTarget = -1;
    }
    if (masterOn && editTarget >= 0) syncProxy();

    //  ---- publish this drum when it changed -------------------------------------
    std::array<float, de::kKitValues> cur {};
    de::toKitValues (currentParams(), cur.data());
    if (! publishedOnce || cur != lastPublished || presetName != lastPublishedName)
    {
        //  the master's own drum is an edit too: a second track on it follows
        if (masterOn) kit.edit (s, cur.data(), de::kKitValues, presetName.toStdString());
        else          kit.publish (s, cur.data(), de::kKitValues, presetName.toStdString());
        if (masterOn) seenEditSeq = kit.editSeqOf (s);
        lastPublished = cur;
        lastPublishedName = presetName;
        publishedOnce = true;
    }

    //  ---- a master elsewhere is looking at this drum: show it what we hear ---------
    {
        int t = -1, ms = -1;
        if (link && ! masterOn && kit.masterAlive (&t, &ms) && t == s) showLiveView (s);
    }

    //  ---- FIND asked for by the master ---------------------------------------------------
    if (link && ! masterOn && kit.takeFind (s))
    {
        remoteKeep = kit.findKeepLevel (s);
        startFind();
        remoteFind = true;
    }
    if (remoteFind)
    {
        kit.findProgress (s, (float) findSeconds());
        if (findSeconds() >= 6.0 || ! finding())
        {
            const auto r = finishFind();       // with the master's KEEP LEVEL
            remoteFind = false;
            kit.findAnswer (s, r.heard(), (float) r.hz, r.hits, r.inDelta);
        }
    }
}

//==============================================================================
//  MASTER
bool DrumEQProcessor::setMaster (bool on)
{
    if (proxyMode) return false;
    if (! on)
    {
        if (masterOn) kit.releaseMaster();
        masterOn = false;
        editTarget = -1;
        return true;
    }
    if (! link || ! kit.isOpen()) return false;
    kit.claimMaster();
    masterOn = true;
    editTarget = -1;
    publishedOnce = false;          // the role starts with this drum as an edit
    return true;
}

bool DrumEQProcessor::masterElsewhere (int& masterSource, int& masterTarget) const
{
    if (masterOn) return false;
    return kit.masterAlive (&masterTarget, &masterSource);
}

void DrumEQProcessor::setTarget (int s)
{
    if (! masterOn) return;
    s = juce::jlimit (0, de::kNumSources - 1, s);
    if (s == currentSource()) { editTarget = -1; kit.setMasterTarget (-1); return; }
    if (proxy == nullptr)
    {
        proxy = std::make_unique<DrumEQProcessor> (ProxyTag {});
        proxy->prepareToPlay (getSampleRate() > 0.0 ? getSampleRate() : 48000.0, 512);
    }
    editTarget = s;
    loadProxyFromSlot();
    kit.setMasterTarget (s);
}

void DrumEQProcessor::loadProxyFromSlot()
{
    if (proxy == nullptr || editTarget < 0) return;
    auto& px = *proxy;
    if (auto* prm = px.apvts.getParameter ("source")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) editTarget));
    px.lastSource = editTarget;

    float v[drumkit::kVals]; int n = 0; std::string nm; drumkit::Link::Meta m;
    if (kit.read (editTarget, v, n, nm, nullptr, &m) && n >= de::kKitValues)
    {
        de::Params p = de::presetParams (editTarget, 0);
        de::fromKitValues (p, v);
        p.source = (float) editTarget;
        px.applyParams (p);
        px.presetName = juce::String (nm);
        proxySeq = m.seq;
    }
    else
    {
        //  a drum no track has played yet: its STANDARD, stored when first touched
        px.applyParams (de::presetParams (editTarget, 0));
        px.presetName = de::preset (editTarget, 0).name;
        proxySeq = 0;
    }
    de::toKitValues (px.currentParams(), proxySent.data());
    proxySentName = px.presetName;
}

void DrumEQProcessor::syncProxy()
{
    if (proxy == nullptr) return;
    auto& px = *proxy;
    //  pull: a track on that drum changed it from its own panel, or a kit was loaded
    {
        float v[drumkit::kVals]; int n = 0; std::string nm; drumkit::Link::Meta m;
        if (kit.read (editTarget, v, n, nm, nullptr, &m) && m.seq != proxySeq)
        {
            proxySeq = m.seq;
            const bool mine = m.writer == kit.myId() && m.gen == 0;
            if (! mine && n >= de::kKitValues) loadProxyFromSlot();
        }
    }
    //  push: the panel moved something
    std::array<float, de::kKitValues> cur {};
    de::toKitValues (px.currentParams(), cur.data());
    if (cur != proxySent || px.presetName != proxySentName)
    {
        kit.edit (editTarget, cur.data(), de::kKitValues, px.presetName.toStdString());
        proxySent = cur;
        proxySentName = px.presetName;
        float v[drumkit::kVals]; int n = 0; std::string nm; drumkit::Link::Meta m;
        if (kit.read (editTarget, v, n, nm, nullptr, &m)) proxySeq = m.seq;
    }
}

void DrumEQProcessor::showLiveView (int s)
{
    //  the panel's spectra, folded onto log bins, and the meters
    float pre[drumkit::kSpec], post[drumkit::kSpec];
    const double fs = juce::jmax (8000.0, engine.sampleRate());
    for (int pass = 0; pass < 2; ++pass)
    {
        de::spectrumDb (pass == 0 ? engine.ringPre() : engine.ringPost(), de::Engine::kRing, engine.ringPos(), 4096, specScratch);
        float* out = pass == 0 ? pre : post;
        const int bins = (int) specScratch.size();
        for (int k = 0; k < drumkit::kSpec; ++k)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) k / drumkit::kSpec);
            const double f1 = 20.0 * std::pow (1000.0, (double) (k + 1) / drumkit::kSpec);
            int i0 = (int) (f0 / (fs * 0.5) * bins), i1 = juce::jmax (i0 + 1, (int) (f1 / (fs * 0.5) * bins));
            i0 = juce::jlimit (1, bins - 1, i0); i1 = juce::jlimit (i0 + 1, bins, i1);
            float mx = -200.0f;
            for (int i = i0; i < i1; ++i) mx = juce::jmax (mx, specScratch[(size_t) i]);
            out[k] = mx;
        }
    }
    const float meters[4] = { engine.meterIn(), engine.meterOut(), engine.meterGrDb(), engine.meterGateDb() };
    kit.showView (s, pre, post, meters, (float) fs);
}

//==============================================================================
juce::File DrumEQProcessor::presetFolder() const
{
    const auto root = brokild::patchFolder ("DrumEQ", { "drumeq" });
    auto d = root.getChildFile (de::source (currentSource()).name);
    d.createDirectory();
    return d;
}

juce::Array<juce::File> DrumEQProcessor::userPresets() const
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    return files;
}

bool DrumEQProcessor::saveUserPreset (const juce::File& f, const juce::String& name)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "drumeq");
    o->setProperty ("type", "preset");
    o->setProperty ("build", DE_BUILD_ID);
    o->setProperty ("source", de::source (currentSource()).name);
    o->setProperty ("name", name);
    o->setProperty ("params", paramsToVar (currentParams()));
    const bool ok = f.replaceWithText (juce::JSON::toString (juce::var (o), false));
    if (ok) presetName = name;
    return ok;
}

bool DrumEQProcessor::loadUserPreset (const juce::File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v["app"].toString() != "drumeq") return false;
    //  this drum's STANDARD first, then whatever the file names: a preset from
    //  an older build lands every parameter it did not know at the standard
    de::Params p = de::presetParams (currentSource(), 0);
    varToParams (v["params"], p);
    applyParams (p);
    presetName = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
    return true;
}

//==============================================================================
juce::File DrumEQProcessor::kitFolder() const
{
    auto d = brokild::patchFolder ("DrumEQ", { "drumeq" }).getChildFile ("Kits");
    d.createDirectory();
    return d;
}

juce::Array<juce::File> DrumEQProcessor::userKits() const
{
    auto files = kitFolder().findChildFiles (juce::File::findFiles, false, "*.drumkit");
    files.sort();
    return files;
}

bool DrumEQProcessor::saveKit (const juce::File& f, const juce::String& name, juce::String& report)
{
    kitTick();        // this drum first, as it is now

    int counts[de::kNumSources];
    kit.presence (counts);
    const uint32_t gen = kit.kitGen();

    juce::Array<juce::var> drums;
    juce::StringArray named;
    for (int s = 0; s < de::kNumSources; ++s)
    {
        float v[drumkit::kVals]; int n = 0; std::string nm; uint32_t g = 0;
        drumkit::Link::Meta m;
        const bool have = kit.read (s, v, n, nm, &g, &m) && n >= de::kKitValues;
        //  a drum is in the kit if a track plays it now, the kit loaded last
        //  brought it, or a MASTER set it up for a track still to come
        if (! have || ! (counts[s] > 0 || (g != 0 && g == gen) || m.editSeq > 0)) continue;
        de::Params p = de::presetParams (s, 0);
        de::fromKitValues (p, v);
        auto* d = new juce::DynamicObject();
        d->setProperty ("source", de::source (s).name);
        d->setProperty ("preset", juce::String (nm));
        d->setProperty ("params", paramsToVar (p));
        drums.add (juce::var (d));
        named.add (de::source (s).name);
    }
    if (! kit.isOpen())
    {
        //  no shared kit (not Windows): the kit is this drum alone
        auto* d = new juce::DynamicObject();
        d->setProperty ("source", de::source (currentSource()).name);
        d->setProperty ("preset", presetName);
        d->setProperty ("params", paramsToVar (currentParams()));
        drums.add (juce::var (d));
        named.add (de::source (currentSource()).name);
    }

    auto* o = new juce::DynamicObject();
    o->setProperty ("app", "drumeq");
    o->setProperty ("type", "kit");
    o->setProperty ("build", DE_BUILD_ID);
    o->setProperty ("name", name);
    o->setProperty ("drums", drums);
    if (! f.replaceWithText (juce::JSON::toString (juce::var (o), false))) { report = "could not write " + f.getFullPathName(); return false; }

    kitLabel = name;
    if (kit.isOpen()) kit.commitKit (gen, name.toStdString());      // the name, for everyone; no reload
    report = juce::String (named.size()) + (named.size() == 1 ? " drum: " : " drums: ") + named.joinIntoString (", ");
    return true;
}

void DrumEQProcessor::writeKitFromParams (const std::vector<std::pair<int, de::Params>>& drums,
                                          const std::vector<juce::String>& names, const juce::String& kitTitle)
{
    if (kit.isOpen())
    {
        const uint32_t gen = kit.beginKit();
        for (size_t i = 0; i < drums.size(); ++i)
        {
            float v[de::kKitValues];
            de::toKitValues (drums[i].second, v);
            kit.writeKitDrum (gen, drums[i].first, v, de::kKitValues, names[i].toStdString());
        }
        kit.commitKit (gen, kitTitle.toStdString());
        forceKitApply = true;            // the instance that loaded it takes it, linked or not
        kitTick();
    }
    else
    {
        for (size_t i = 0; i < drums.size(); ++i)
            if (drums[i].first == currentSource())
            {
                de::Params p = drums[i].second; p.source = (float) currentSource();
                applyParams (p); presetName = names[i];
            }
        kitLabel = kitTitle;
    }
}

bool DrumEQProcessor::loadKitFile (const juce::File& f, juce::String& report)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v["app"].toString() != "drumeq" || v["type"].toString() != "kit")
    { report = f.getFileName() + " is not a DrumEQ kit"; return false; }

    std::vector<std::pair<int, de::Params>> drums;
    std::vector<juce::String> names;
    juce::StringArray named;
    if (auto* arr = v["drums"].getArray())
        for (const auto& d : *arr)
        {
            const int s = sourceByName (d["source"].toString());
            if (s < 0) continue;
            de::Params p = de::presetParams (s, 0);
            varToParams (d["params"], p);
            drums.push_back ({ s, p });
            names.push_back (d["preset"].toString().isNotEmpty() ? d["preset"].toString() : juce::String ("KIT"));
            named.add (de::source (s).name);
        }
    if (drums.empty()) { report = f.getFileName() + " holds no drums"; return false; }
    const juce::String title = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
    writeKitFromParams (drums, names, title);
    report = juce::String (named.size()) + (named.size() == 1 ? " drum: " : " drums: ") + named.joinIntoString (", ");
    return true;
}

void DrumEQProcessor::loadFactoryKit (int i, juce::String& report)
{
    const auto& k = de::kit (i);
    std::vector<std::pair<int, de::Params>> drums;
    std::vector<juce::String> names;
    for (int s = 0; s < de::kNumSources; ++s)
    {
        drums.push_back ({ s, de::presetParams (s, k.pick[s]) });
        names.push_back (de::preset (s, k.pick[s]).name);
    }
    writeKitFromParams (drums, names, k.name);
    report = "all " + juce::String (de::kNumSources) + " drums";
}

//==============================================================================
FindReport DrumEQProcessor::finishFind()
{
    engine.armFind (false);
    FindReport rep;
    const int s = currentSource();
    const auto& sd = de::source (s);
    const float* x = engine.findData();
    const int n = engine.findLength();
    const double fs = engine.sampleRate();
    const de::Params p0 = currentParams();

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };

    //  ---- the pitch: drums that have one --------------------------------------------
    if (sd.findHi > 0.0f)
    {
        const auto r = de::findFundamental (x, n, fs, sd.findLo, sd.findHi);
        if (r.hz > 0.0)
        {
            static const char* ids[4] = { "b1f", "b2f", "b3f", "b4f" };
            const auto& band = sd.band[sd.findBand];
            set (ids[sd.findBand], (float) juce::jlimit ((double) band.lo, (double) band.hi, r.hz));
            rep.hz = r.hz;
        }
    }

    //  ---- the levels: every drum ---------------------------------------------------------
    //  The capture is after the trim, so the trim moves by what the hits miss
    //  the target by. Everything after it - gate, EQ drive, compressor - then
    //  meets the drum at the level its preset was written for, on every track.
    const auto lv = de::measureLevels (x, n, fs, p0.gkeylo, p0.gkeyhi);
    rep.hits = lv.hits;
    rep.spill = lv.spill;
    if (lv.ok())
    {
        const auto& sp = de::specs()[de::indexOf ("in")];
        const float want = p0.in + (kTargetPeakDb - (float) lv.peakDb);
        const float newIn = juce::jlimit (sp.lo, sp.hi, want);
        rep.clamped = std::abs (newIn - want) > 0.05f;
        rep.inDelta = newIn - p0.in;
        rep.levelSet = std::abs (rep.inDelta) >= 0.05f;
        if (rep.levelSet) set ("in", newIn);

        if ((remoteFind ? remoteKeep : keepLvl) && rep.levelSet)
        {
            const auto& so = de::specs()[de::indexOf ("out")];
            const float newOut = juce::jlimit (so.lo, so.hi, p0.out - rep.inDelta);
            rep.outDelta = newOut - p0.out;
            set ("out", newOut);
        }

        //  the gate: between the loudest spill and the quietest hit, both moved
        //  by the new trim, its 4 dB hysteresis kept clear of the spill
        const double hitDb = lv.keyMinDb + rep.inDelta;
        double thr;
        if (lv.spillDb > -199.0)
        {
            const double spillDb = lv.spillDb + rep.inDelta;
            rep.spillHeard = true;
            const double lo = spillDb + 5.0, hi = hitDb - 3.0;
            thr = lo <= hi ? juce::jlimit (lo, hi, 0.5 * (spillDb + hitDb)) : 0.5 * (spillDb + hitDb);
        }
        else thr = hitDb - 18.0;          // nothing else heard: well under the drum
        const auto& sg = de::specs()[de::indexOf ("gthr")];
        rep.gthr = juce::jlimit (sg.lo, sg.hi, (float) thr);
        rep.gateSet = true;
        set ("gthr", rep.gthr);
    }
    return rep;
}

//==============================================================================
void DrumEQProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("link", link, nullptr);
    state.setProperty ("kitName", kitLabel, nullptr);
    state.setProperty ("build", DE_BUILD_ID, nullptr);
    state.setProperty ("master", masterOn, nullptr);
    state.setProperty ("keepLevel", keepLvl, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DrumEQProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        presetName = tree.getProperty ("presetName", "STANDARD").toString();
        link = (bool) tree.getProperty ("link", true);
        kitLabel = tree.getProperty ("kitName", "").toString();
        keepLvl = (bool) tree.getProperty ("keepLevel", false);
        //  a track saved as the master takes the role back on its first kit tick
        masterPending = link && (bool) tree.getProperty ("master", false);
        for (auto* k : { "master", "keepLevel" }) tree.removeProperty (k, nullptr);
        apvts.replaceState (tree);
        //  a restored drum is a drum, not a change of drum: no defaults on top
        lastSource = currentSource();
        publishedOnce = false;
    }
}

juce::AudioProcessorEditor* DrumEQProcessor::createEditor() { return new DrumEQEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumEQProcessor(); }
