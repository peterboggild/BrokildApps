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
        case vfFoot:    return { 0, 5, 1, 2 };
        case vfTune:    return { -1, 1, 0, 0 };
        case vfLfoWave: return { 0, 3, 1, 0 };
        case vfLoop:
        case vfMute:
        case vfSolo:    return { 0, 1, 1, 0 };
        case vfNote:    return { 0, 2, 1, 0 };
        case vfPw:      return { 0, 1, 0, 0.5f };
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
        case gLfoSync:              return { 0, 1, 1, 0 };
        case gCutoff:               return { 0, 1, 0, 0.5f };
        case gFxMix:                return { 0, 1, 0, 1.0f };
        case gLfoDiv:               return { 0, 9, 1, 5 };
        case gNoteMode:             return { 0, 2, 1, 1 };  // UNISON/TREATY/WAR
        case gEnvFilt:              return { 0, 1, 0, 0.45f };
        case gWar:                  return { 0, 1, 0, 0.5f };
        case gRanks:                return { 0, 1, 0, 0.5f };
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
    // ...and wears its own scars, in its own pattern, starting from undamaged
    wearSeed = (uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30);
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
        else if (msg.isPitchWheel())
            engine.setBend (((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f);
        else if (msg.isController() && msg.getControllerNumber() == 1)
            engine.setMod ((float) msg.getControllerValue() / 127.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    midi.clear();

    // host tempo for LFO SYNC; harmless when the host offers none
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engine.setBpm (*bpm);

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
void CloneWarsProcessor::applyMorph (float t, bool repaintUi)
{
    // Glide the console from seed A to seed B: continuous parameters are
    // interpolated; STEPPED ones (wave, footage, temper, switches) each defect
    // at their own deterministic threshold staggered across the travel, so the
    // console changes sides one clone at a time instead of all at the midpoint.
    cw::Patch a, b;
    cw::generatePatch ((uint32_t) currentSeedA.load(), a);
    currentCategory = cw::generatePatch ((uint32_t) currentSeedA.load(), a);
    cw::generatePatch ((uint32_t) currentSeedB.load(), b);
    uint32_t stag = 0x243F6A88u;
    auto blend = [t, &stag] (float av, float bv, bool stepped) mutable
    {
        stag = stag * 1664525u + 1013904223u;
        const float thresh = 0.08f + 0.84f * (float) (stag >> 8) / 16777216.0f;
        return stepped ? (t < thresh ? av : bv) : av + t * (bv - av);
    };
    suppressEcho = true;
    for (int g = 0; g < cw::numGlobals; ++g)
        if (auto* par = apvts.getParameter (globalParamId (g)))
            par->setValueNotifyingHost (par->convertTo0to1 (
                blend (a.global[g], b.global[g], globalShape (g).step > 0.0f)));
    for (int v = 0; v < cw::kVoices; ++v)
        for (int f = 0; f < cw::numVoiceFields; ++f)
            if (auto* par = apvts.getParameter (voiceParamId (v, f)))
                par->setValueNotifyingHost (par->convertTo0to1 (
                    blend (a.voice[v][f], b.voice[v][f], voiceShape (f).step > 0.0f)));
    suppressEcho = false;
    pushAllParamsToEngine();
    if (repaintUi) sendInitToUi();
}

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
    else if (k == "morph")
    {
        const float t = std::clamp ((float) (double) m.getProperty ("v", 0.0), 0.0f, 1.0f);
        morphT.store (t);
        applyMorph (t, t <= 0.0f || t >= 1.0f);
    }
    else if (k == "__never__")
    {
        const float t = 0;
        cw::Patch a, b;
        cw::generatePatch ((uint32_t) currentSeedA.load(), a);
        cw::generatePatch ((uint32_t) currentSeedB.load(), b);
        // Continuous parameters glide; STEPPED ones (wave, footage, temper,
        // switches) cannot - so instead of every one of them snapping together
        // at the midpoint (an audible cliff at 49->50), each defects at its own
        // deterministic threshold, staggered across 0.08..0.92 of the travel.
        // The console changes sides one clone at a time.
        uint32_t stag = 0x243F6A88u;
        auto blend = [t, &stag] (float av, float bv, bool stepped) mutable
        {
            stag = stag * 1664525u + 1013904223u;
            const float thresh = 0.08f + 0.84f * (float) (stag >> 8) / 16777216.0f;
            return stepped ? (t < thresh ? av : bv) : av + t * (bv - av);
        };
        suppressEcho = true;
        for (int g = 0; g < cw::numGlobals; ++g)
            if (auto* par = apvts.getParameter (globalParamId (g)))
                par->setValueNotifyingHost (par->convertTo0to1 (
                    blend (a.global[g], b.global[g], globalShape (g).step > 0.0f)));
        for (int v = 0; v < cw::kVoices; ++v)
            for (int f = 0; f < cw::numVoiceFields; ++f)
                if (auto* par = apvts.getParameter (voiceParamId (v, f)))
                    par->setValueNotifyingHost (par->convertTo0to1 (
                        blend (a.voice[v][f], b.voice[v][f], voiceShape (f).step > 0.0f)));
        suppressEcho = false;
        pushAllParamsToEngine();
    }
    else if (k == "seed")
    {
        // MORPH's position is authoritative: loading a seed re-applies the
        // blend AT that position, so the knob and the console never disagree.
        const int n = (int) m.getProperty ("n", 0);
        const bool isB = (bool) m.getProperty ("b", false);
        if (isB) currentSeedB = n; else currentSeedA = n;
        const float t = morphT.load();
        if (! isB && t <= 0.001f) applySeed ((uint32_t) n);
        else                      applyMorph (t, true);
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
    else if (k == "bend")
    {
        engine.setBend ((float) (double) m.getProperty ("v", 0.0) * 2.0f);
    }
    else if (k == "mod")
    {
        engine.setMod ((float) (double) m.getProperty ("v", 0.0));
    }
    else if (k == "scatter")
    {
        engine.scatterLfoPhases();
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
    obj->setProperty ("morph", morphT.load());
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
    root->setProperty ("morphT", (double) morphT.load());

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
    wearPoints = (double) v.getProperty ("wear", 0.0);
    wearSeed   = (uint32_t) (juce::int64) v.getProperty ("wearSeed", 1337);
    unitSeed   = (uint32_t) (juce::int64) v.getProperty ("unitSeed", (juce::int64) 0xC70BE5);
    currentSeedA = (int) v.getProperty ("seedA", 42);
    currentSeedB = (int) v.getProperty ("seedB", 137);
    // restore the position but do NOT re-apply the blend: the saved params
    // already carry blend + any edits, and re-applying would wipe the edits
    morphT = (float) (double) v.getProperty ("morphT", 0.0);

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
