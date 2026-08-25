#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::String CloneWarsProcessor::globalParamId (int g)
{
    return "g_" + juce::String (cw::globalId (g));
}
juce::String CloneWarsProcessor::voiceParamId (int v, int f)
{
    return "v" + juce::String (v + 1).paddedLeft ('0', 2) + "_"
         + juce::String (cw::voiceFieldId (f));
}

// Parameter shapes per field: {min, max, step}. Discrete selectors get a step
// of 1 so hosts show them as stepped automation lanes.
struct Shape { float min, max, step, def; };
static Shape voiceShape (int f)
{
    using namespace cw;
    switch (f)
    {
        case vfWave:    return { 0, 3, 1, 0 };
        case vfFoot:    return { 0, 3, 1, 1 };
        case vfTune:    return { -1, 1, 0, 0 };
        case vfLfoWave: return { 0, 3, 1, 0 };
        case vfLoop:
        case vfMute:
        case vfSolo:    return { 0, 1, 1, 0 };
        case vfNote:    return { 0, 2, 1, 0 };
        case vfLevel:   return { 0, 1, 0, 0.8f };
        case vfCut:     return { 0, 1, 0, 0.65f };
        case vfPan:     return { -1, 1, 0, 0 };
        default:        return { 0, 1, 0, 0.3f };
    }
}
static Shape globalShape (int g)
{
    using namespace cw;
    switch (g)
    {
        case gTemperA:      return { 0, 2, 1, 0 };
        case gTemperB:      return { 0, 2, 1, 2 };
        case gLatchA: case gLatchB: case gBassMono: case gHpf:
        case gDrone:                return { 0, 1, 1, 1 };
        case gHq:                   return { 0, 2, 1, 1 };  // LOW / HQ / XHQ
        case gSpringFreeze:         return { 0, 1, 1, 0 };
        case gWar:                  return { 0, 1, 0, 0.5f };
        case gMaster:               return { 0, 1, 0, 0.75f };
        default:                    return { 0, 1, 0, 0.3f };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
CloneWarsProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    cw::Patch defs;
    cw::defaultPatch (defs);

    for (int g = 0; g < cw::numGlobals; ++g)
    {
        const auto sh = globalShape (g);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { globalParamId (g), 1 },
            "G " + juce::String (cw::globalId (g)),
            juce::NormalisableRange<float> (sh.min, sh.max, sh.step),
            defs.global[g]));
    }
    for (int v = 0; v < cw::kVoices; ++v)
        for (int f = 0; f < cw::numVoiceFields; ++f)
        {
            const auto sh = voiceShape (f);
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { voiceParamId (v, f), 1 },
                juce::String (v < cw::kArmySize ? "A" : "B")
                    + juce::String (v + 1).paddedLeft ('0', 2) + " "
                    + juce::String (cw::voiceFieldId (f)),
                juce::NormalisableRange<float> (sh.min, sh.max, sh.step),
                defs.voice[v][f]));
        }
    return layout;
}

//==============================================================================
CloneWarsProcessor::CloneWarsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int g = 0; g < cw::numGlobals; ++g)
    {
        Slot s; s.global = g;
        slots[globalParamId (g)] = s;
        apvts.addParameterListener (globalParamId (g), this);
    }
    for (int v = 0; v < cw::kVoices; ++v)
        for (int f = 0; f < cw::numVoiceFields; ++f)
        {
            Slot s; s.voice = v; s.field = f;
            slots[voiceParamId (v, f)] = s;
            apvts.addParameterListener (voiceParamId (v, f), this);
        }

    // every fresh instance is a distinct unit off the production line
    unitSeed = (uint32_t) juce::Random::getSystemRandom().nextInt64();
    engine.setUnitSeed (unitSeed.load());
    hqRaw = apvts.getRawParameterValue (globalParamId (cw::gHq));
    pushAllParamsToEngine();
}

CloneWarsProcessor::~CloneWarsProcessor()
{
    for (auto& kv : slots)
        apvts.removeParameterListener (kv.first, this);
}

void CloneWarsProcessor::pushAllParamsToEngine()
{
    for (auto& kv : slots)
    {
        const float v = apvts.getRawParameterValue (kv.first)->load();
        if (kv.second.global >= 0) engine.setGlobal (kv.second.global, v);
        else                       engine.setVoice (kv.second.voice, kv.second.field, v);
    }
}

//==============================================================================
void CloneWarsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

bool CloneWarsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CloneWarsProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // Offline render always gets the extra-high-quality engine, whatever the
    // panel switch says — CPU load only matters in real time.
    engine.setGlobal (cw::gHq, isNonRealtime() ? 2.0f
                                               : (hqRaw != nullptr ? hqRaw->load() : 1.0f));

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())        engine.noteOn  (msg.getNoteNumber());
        else if (msg.isNoteOff())  engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    midi.clear();

    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // the hull remembers: age and wear accrue while the unit is audibly playing
    const float out = engine.masterMeterL() + engine.masterMeterR();
    if (out > 1.0e-3f)
    {
        ageSamples.fetch_add (n, std::memory_order_relaxed);
        const double secs = (double) n / getSampleRate();
        wearPoints.store (wearPoints.load (std::memory_order_relaxed)
                          + secs * (0.02 + 0.05 * (double) juce::jmin (1.0f, out)),
                          std::memory_order_relaxed);
    }
}

//==============================================================================
void CloneWarsProcessor::parameterChanged (const juce::String& id, float newValue)
{
    // May arrive on the audio thread. Engine entry points are plain atomic
    // stores, so pushing directly is safe; the UI echo is deferred.
    auto it = slots.find (id);
    if (it != slots.end())
    {
        if (it->second.global >= 0) engine.setGlobal (it->second.global, newValue);
        else engine.setVoice (it->second.voice, it->second.field, newValue);
    }
    if (suppressEcho.load()) return;
    const juce::ScopedLock sl (dirtyLock);
    dirtyParams.addIfNotAlreadyThere (id);
}

//==============================================================================
void CloneWarsProcessor::applySeed (uint32_t seed)
{
    cw::Patch p;
    currentCategory = cw::generatePatch (seed, p);

    suppressEcho = true;
    for (int g = 0; g < cw::numGlobals; ++g)
        if (auto* par = apvts.getParameter (globalParamId (g)))
            par->setValueNotifyingHost (par->convertTo0to1 (p.global[g]));
    for (int v = 0; v < cw::kVoices; ++v)
        for (int f = 0; f < cw::numVoiceFields; ++f)
            if (auto* par = apvts.getParameter (voiceParamId (v, f)))
                par->setValueNotifyingHost (par->convertTo0to1 (p.voice[v][f]));
    suppressEcho = false;

    pushAllParamsToEngine();
    sendInitToUi();
}

//==============================================================================
void CloneWarsProcessor::handleUiMessage (const juce::var& m)
{
    const juce::String k = m.getProperty ("k", juce::var()).toString();

    if (k == "param")
    {
        if (auto* par = apvts.getParameter (m.getProperty ("id", juce::var()).toString()))
        {
            suppressEcho = true;
            par->setValueNotifyingHost (
                par->convertTo0to1 ((float) (double) m.getProperty ("v", 0.0)));
            suppressEcho = false;
        }
    }
    else if (k == "seed")
    {
        const int n = (int) m.getProperty ("n", 0);
        const bool isB = (bool) m.getProperty ("b", false);
        if (isB) currentSeedB = n;
        else     { currentSeedA = n; applySeed ((uint32_t) n); }
    }
    else if (k == "note")                      // on-screen keyboard
    {
        const int n = (int) m.getProperty ("n", -1);
        if (n >= 0 && n < 128)
        {
            if ((int) m.getProperty ("on", 0) != 0) engine.noteOn (n);
            else                                    engine.noteOff (n);
        }
    }
    else if (k == "repair")
    {
        wearPoints = 0.0;
        ageSamples = 0;
        wearSeed = (uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30);
    }
    else if (k == "tour")
    {
        wearPoints.store (wearPoints.load() + 400.0);
        ageSamples.fetch_add ((int64_t) (800.0 * 3600.0 * getSampleRate()));
    }
    else if (k == "getinit")
    {
        sendInitToUi();
    }
}

void CloneWarsProcessor::sendInitToUi()
{
    if (! emitToUi) return;
    auto* obj = new juce::DynamicObject();
    auto* params = new juce::DynamicObject();
    for (auto& kv : slots)
        params->setProperty (kv.first, apvts.getRawParameterValue (kv.first)->load());
    obj->setProperty ("params", juce::var (params));
    obj->setProperty ("ageSec", (double) ageSamples.load() / juce::jmax (1.0, getSampleRate()));
    obj->setProperty ("wear", wearPoints.load());
    obj->setProperty ("wearSeed", (juce::int64) wearSeed.load());
    obj->setProperty ("seedA", currentSeedA.load());
    obj->setProperty ("seedB", currentSeedB.load());
    obj->setProperty ("category", currentCategory);
    emitToUi ("init", juce::var (obj));
}

void CloneWarsProcessor::timerService()
{
    if (! emitToUi) return;

    // parameter echo (host automation → page)
    juce::StringArray dirty;
    { const juce::ScopedLock sl (dirtyLock); dirty.swapWith (dirtyParams); }
    for (const auto& id : dirty)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        obj->setProperty ("v", apvts.getRawParameterValue (id)->load());
        emitToUi ("hostParam", juce::var (obj));
    }

    // meters + odometer
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> vu;
    for (int v = 0; v < cw::kVoices; ++v) vu.add (engine.voiceMeter (v));
    obj->setProperty ("vu", vu);
    obj->setProperty ("l", engine.masterMeterL());
    obj->setProperty ("r", engine.masterMeterR());
    obj->setProperty ("ageSec", (double) ageSamples.load() / juce::jmax (1.0, getSampleRate()));
    obj->setProperty ("wear", wearPoints.load());
    emitToUi ("meters", juce::var (obj));
}

//==============================================================================
void CloneWarsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("apvts", apvts.copyState().toXmlString());
    root->setProperty ("ageSamples", (juce::int64) ageSamples.load());
    root->setProperty ("wear", wearPoints.load());
    root->setProperty ("wearSeed", (juce::int64) wearSeed.load());
    root->setProperty ("unitSeed", (juce::int64) unitSeed.load());
    root->setProperty ("seedA", currentSeedA.load());
    root->setProperty ("seedB", currentSeedB.load());

    const auto json = juce::JSON::toString (juce::var (root), true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void CloneWarsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, sizeInBytes));
    if (! v.isObject()) return;

    if (auto xml = juce::XmlDocument::parse (v.getProperty ("apvts", juce::var()).toString()))
    {
        suppressEcho = true;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        suppressEcho = false;
    }
    ageSamples = (juce::int64) v.getProperty ("ageSamples", 0);
    wearPoints = (double) v.getProperty ("wear", 300.0);
    wearSeed   = (uint32_t) (juce::int64) v.getProperty ("wearSeed", 1337);
    unitSeed   = (uint32_t) (juce::int64) v.getProperty ("unitSeed", (juce::int64) 0xC70BE5);
    currentSeedA = (int) v.getProperty ("seedA", 42);
    currentSeedB = (int) v.getProperty ("seedB", 137);

    engine.setUnitSeed (unitSeed.load());
    pushAllParamsToEngine();
    sendInitToUi();
}

juce::AudioProcessorEditor* CloneWarsProcessor::createEditor()
{
    return new CloneWarsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CloneWarsProcessor();
}
