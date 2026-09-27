#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef BO_BUILD_ID
 #define BO_BUILD_ID "0.0.0"
#endif

//==============================================================================
/*  The whole parameter surface, in one table. Everything the metal has and
    nothing it does not: six pots and one button. */
const PSpec BO_SPECS[] =
{
    { "mix",        "MIX",        1.00f, false, 0,                nullptr },
    { "thrust",     "THRUST",     0.35f, false, 0,                nullptr },
    { "antithrust", "ANTITHRUST", 0.00f, false, 0,                nullptr },
    { "engine",     "ENGINE",     1.00f, true,  bo::NUM_ENGINES,  nullptr },
    { "space",      "SPACE",      0.00f, false, 0,                nullptr },
    { "spectrum",   "SPECTRUM",   0.50f, false, 0,                nullptr },
    { "autorefill", "AUTOREFILL", 1.00f, true,  2,                nullptr },

    /*  Hidden: no pot on the metal, but the host can see, automate and save
        them. Added at the END of the table on purpose - processBlock reads in
        table order, so a trailing row leaves every existing read untouched
        while an inserted one silently rewires everything after it. */
    { "spacewet",   "SPACE WET",  0.50f, false, 0,                nullptr },
    { "spacesync",  "SPACE SYNC", 0.00f, true,  bo::NUM_SYNC,     bo::SYNC_NAMES }
};
const int BO_NUM_PARAMS = (int) (sizeof (BO_SPECS) / sizeof (BO_SPECS[0]));

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BattlestarOverdriveAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < BO_NUM_PARAMS; ++i)
    {
        const auto& s = BO_SPECS[i];

        if (s.choices != nullptr)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        }
        else if (s.stepped && s.steps == bo::NUM_ENGINES)
        {
            juce::StringArray names;
            for (int e = 0; e < bo::NUM_ENGINES; ++e) names.add (bo::engineName (e));
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { s.id, 1 }, s.name, names, (int) s.def));
        }
        else if (s.stepped)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { s.id, 1 }, s.name, s.def > 0.5f));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), s.def));
        }
    }
    return layout;
}

//==============================================================================
BattlestarOverdriveAudioProcessor::BattlestarOverdriveAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BATTLESTAR", createParameterLayout())
{
    for (int i = 0; i < BO_NUM_PARAMS; ++i)
    {
        paramPtr[(size_t) i] = apvts.getRawParameterValue (BO_SPECS[i].id);
        lastSent[(size_t) i] = -999.0f;
    }
    startTimerHz (30);
}

BattlestarOverdriveAudioProcessor::~BattlestarOverdriveAudioProcessor() = default;

bool BattlestarOverdriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void BattlestarOverdriveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
}

//==============================================================================
void BattlestarOverdriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                      juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, numSamples);

    // Read in the table's own order, so a parameter added to the middle of
    // BO_SPECS cannot silently wire everything after it to the wrong thing.
    int k = 0;
    current.mix        = paramPtr[(size_t) k++]->load();
    current.thrust     = paramPtr[(size_t) k++]->load();
    current.antithrust = paramPtr[(size_t) k++]->load();
    current.engine     = (int) paramPtr[(size_t) k++]->load();
    current.space      = paramPtr[(size_t) k++]->load();
    current.spectrum   = paramPtr[(size_t) k++]->load();
    current.autorefill = paramPtr[(size_t) k++]->load() > 0.5f;
    current.spaceWet   = paramPtr[(size_t) k++]->load();
    current.spaceSync  = (int) paramPtr[(size_t) k++]->load();

    // The host clock, for SPACE SYNC. Absent (or stopped at an unknown tempo)
    // leaves bpm at 0, and the engine then behaves exactly as if sync were off.
    current.bpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto t = pos->getBpm())
                current.bpm = *t;

    engine.setParams (current);

    float* L = buffer.getWritePointer (0);
    float* R = (numOut > 1) ? buffer.getWritePointer (1) : L;

    if (numOut > 1)
    {
        engine.process (L, R, numSamples);
    }
    else
    {
        // Mono: run the engine on a scratch pair so the stereo-only parts of
        // SPACE still behave, then keep the left side.
        juce::AudioBuffer<float> scratch (2, numSamples);
        scratch.copyFrom (0, 0, L, numSamples);
        scratch.copyFrom (1, 0, L, numSamples);
        engine.process (scratch.getWritePointer (0), scratch.getWritePointer (1), numSamples);
        buffer.copyFrom (0, 0, scratch, 0, 0, numSamples);
    }
}

//==============================================================================
void BattlestarOverdriveAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        // Panel state, not a host parameter: which knob set the panel wears.
        xml->setAttribute ("knobSkin", knobSkin);
        copyXmlToBinary (*xml, dest);
    }
}

void BattlestarOverdriveAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;
    if (! xml->hasTagName (apvts.state.getType())) return;

    knobSkin = xml->getStringAttribute ("knobSkin", "chrome");
    xml->removeAttribute ("knobSkin");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    uiHasState = false;          // make the panel take the new values
}

//==============================================================================
void BattlestarOverdriveAudioProcessor::handleUiMessage (const juce::var& payload)
{
    if (! payload.isObject()) return;
    const juce::String k = payload.getProperty ("k", {}).toString();

    if (k == "hello")
    {
        // Every page boot clears the ack. Without this a reload the editor
        // never sees leaves the processor believing the old one and the fresh
        // page waits forever for a state that is never sent again.
        uiHasState = false;
        uiReady = true;
        emitInitialState();
        return;
    }

    if (k == "stateack") { uiHasState = true; return; }

    if (k == "skin")
    {
        knobSkin = payload.getProperty ("v", "chrome").toString();
        return;
    }

    if (k == "touch")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        if (auto* prm = apvts.getParameter (id))
        {
            // Wrapped in a gesture or the host's "learn from a touch" never
            // sees the move - Ableton's Configure in particular.
            if ((bool) payload.getProperty ("down", false)) prm->beginChangeGesture();
            else                                            prm->endChangeGesture();
        }
        return;
    }

    if (k == "p")
    {
        const juce::String id = payload.getProperty ("id", {}).toString();
        const float v = (float) (double) payload.getProperty ("v", 0.0);
        if (auto* prm = apvts.getParameter (id))
        {
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
            for (int i = 0; i < BO_NUM_PARAMS; ++i)
                if (id == BO_SPECS[i].id) lastSent[(size_t) i] = v;   // do not echo it back
        }
        return;
    }
}

//==============================================================================
void BattlestarOverdriveAudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("build", BO_BUILD_ID);
    obj->setProperty ("skin", knobSkin);

    juce::Array<juce::var> params;
    for (int i = 0; i < BO_NUM_PARAMS; ++i)
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("id", BO_SPECS[i].id);
        po->setProperty ("name", BO_SPECS[i].name);
        po->setProperty ("stepped", BO_SPECS[i].stepped);
        po->setProperty ("steps", BO_SPECS[i].steps);
        if (auto* prm = apvts.getParameter (BO_SPECS[i].id))
            po->setProperty ("v", prm->getValue());
        params.add (juce::var (po));
        lastSent[(size_t) i] = -999.0f;
    }
    obj->setProperty ("params", params);

    juce::Array<juce::var> engines;
    for (int e = 0; e < bo::NUM_ENGINES; ++e)
    {
        auto* eo = new juce::DynamicObject();
        eo->setProperty ("name", bo::engineName (e));
        eo->setProperty ("blurb", bo::engineBlurb (e));
        engines.add (juce::var (eo));
    }
    obj->setProperty ("engines", engines);

    emitToUi ("initialState", juce::var (obj));
}

//==============================================================================
void BattlestarOverdriveAudioProcessor::timerCallback()
{
    if (! emitToUi) return;

    // Re-push the initial state until the page acknowledges it, rather than
    // answering one request that can be dropped while the browser is not yet
    // on screen.
    if (! uiHasState)
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // Echo any parameter the page did not move itself - automation, a preset
    // load, a host undo. A control that is working but never updates on screen
    // reads as broken, which has cost this workshop five separate rounds.
    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < BO_NUM_PARAMS; ++i)
        {
            auto* prm = apvts.getParameter (BO_SPECS[i].id);
            if (prm == nullptr) continue;
            const float v = prm->getValue();
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-5f)
            {
                lastSent[(size_t) i] = v;
                auto* po = new juce::DynamicObject();
                po->setProperty ("id", BO_SPECS[i].id);
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

    // Meters for the screen.
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("in",    engine.inLevel());
        obj->setProperty ("out",   engine.outLevel());
        obj->setProperty ("fuel",  engine.fuelLevel());
        obj->setProperty ("drive", engine.driveAmount());
        obj->setProperty ("misfire", engine.misfiring());
        obj->setProperty ("empty",   engine.fuelEmpty());
        obj->setProperty ("fizzle",  engine.fizzleAmount());
        obj->setProperty ("peak",    engine.outPeak());
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
juce::AudioProcessorEditor* BattlestarOverdriveAudioProcessor::createEditor()
{
    return new BattlestarOverdriveAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BattlestarOverdriveAudioProcessor();
}
