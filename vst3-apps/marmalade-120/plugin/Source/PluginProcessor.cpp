#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef MM_BUILD_ID
 #define MM_BUILD_ID "0.0.0"
#endif

//==============================================================================
/*  The whole parameter surface, in one table: the amp's six controls, then
    the three things a plugin needs that the amp never had. New rows go at the
    END - processBlock reads in table order, so an inserted row would silently
    rewire everything after it. */
const PSpec MM_SPECS[] =
{
    { "gain",    "GAIN",    0.55f,       0, nullptr },
    { "bite",    "BITE",    0.25f,       0, nullptr },     // the HF drive network
    { "bass",    "BASS",    0.50f,       0, nullptr },
    { "treble",  "TREBLE",  0.55f,       0, nullptr },
    { "voice",   "VOICE",   2.0f,        mm::NUM_FAC, "1|2|3|4|5|6" },   // the six-way capacitor switch
    { "master",  "MASTER",  0.35f,       0, nullptr },
    { "output",  "OUTPUT",  2.0f / 3.0f, 0, nullptr },     // -24 .. +12 dB, 0 dB here
    { "cab",     "CAB",     1.0f,        2, nullptr },
    { "quality", "QUALITY", 0.0f,        2, "NORMAL|HIGH" }
};
const int MM_NUM_PARAMS = (int) (sizeof (MM_SPECS) / sizeof (MM_SPECS[0]));

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Marmalade120AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < MM_NUM_PARAMS; ++i)
    {
        const auto& s = MM_SPECS[i];
        if (s.choices != nullptr)
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        else if (s.steps == 2)
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { s.id, 1 }, s.name, s.def > 0.5f));
        else if (juce::String (s.id) == "output")
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [] (float v, int) { return juce::String (mm::Engine::outputDb (v), 1) + " dB"; })));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { s.id, 1 }, s.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [] (float v, int) { return juce::String (v * 10.0f, 1); })));      // a pot reads 0 - 10
    }
    return layout;
}

//==============================================================================
Marmalade120AudioProcessor::Marmalade120AudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MARMALADE", createParameterLayout())
{
    for (int i = 0; i < MM_NUM_PARAMS; ++i)
    {
        paramPtr[(size_t) i] = apvts.getRawParameterValue (MM_SPECS[i].id);
        lastSent[(size_t) i] = -999.0f;
    }
    startTimerHz (30);
}

Marmalade120AudioProcessor::~Marmalade120AudioProcessor() = default;

bool Marmalade120AudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void Marmalade120AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    current.quality = (int) paramPtr[8]->load();
    engine.setParams (current);
    engine.prepare (sampleRate, samplesPerBlock);
    preparedRate = sampleRate;
    preparedBlock = samplesPerBlock;
    preparedQuality = current.quality;
    setLatencySamples (engine.latencySamples());
}

//==============================================================================
void Marmalade120AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, numSamples);

    // in the table's own order
    int k = 0;
    current.gain    = paramPtr[(size_t) k++]->load();
    current.hfDrive = paramPtr[(size_t) k++]->load();
    current.bass    = paramPtr[(size_t) k++]->load();
    current.treble  = paramPtr[(size_t) k++]->load();
    current.fac     = (int) paramPtr[(size_t) k++]->load();
    current.master  = paramPtr[(size_t) k++]->load();
    current.output  = paramPtr[(size_t) k++]->load();
    current.cab     = paramPtr[(size_t) k++]->load() > 0.5f;
    current.quality = (int) paramPtr[(size_t) k++]->load();

    engine.setParams (current);

    /*  QUALITY changes the oversampling, which is a new prepare. It costs an
        allocation on this thread once per flip of a switch nobody automates,
        which is cheaper than any of the ways of avoiding it. */
    if (current.quality != preparedQuality && preparedRate > 0.0)
    {
        engine.prepare (preparedRate, juce::jmax (preparedBlock, numSamples));
        preparedQuality = current.quality;
        setLatencySamples (engine.latencySamples());
    }

    float* L = buffer.getWritePointer (0);
    float* R = (numOut > 1) ? buffer.getWritePointer (1) : nullptr;
    engine.process (L, R, numSamples);
}

//==============================================================================
void Marmalade120AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr) copyXmlToBinary (*xml, dest);
}

void Marmalade120AudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    uiHasState = false;          // make the panel take the new values
}

//==============================================================================
void Marmalade120AudioProcessor::handleUiMessage (const juce::var& payload)
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
            // wrapped in a gesture or a host's "learn from a touch" never sees it
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
            for (int i = 0; i < MM_NUM_PARAMS; ++i)
                if (id == MM_SPECS[i].id) lastSent[(size_t) i] = v;   // do not echo it back
        }
    }
}

//==============================================================================
void Marmalade120AudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("build", MM_BUILD_ID);
    obj->setProperty ("ok", engine.ok());
    obj->setProperty ("error", juce::String (engine.error()));

    juce::Array<juce::var> params;
    for (int i = 0; i < MM_NUM_PARAMS; ++i)
    {
        auto* po = new juce::DynamicObject();
        po->setProperty ("id", MM_SPECS[i].id);
        po->setProperty ("name", MM_SPECS[i].name);
        po->setProperty ("steps", MM_SPECS[i].steps);
        if (auto* prm = apvts.getParameter (MM_SPECS[i].id))
        {
            po->setProperty ("v", prm->getValue());
            po->setProperty ("def", prm->getDefaultValue());
        }
        params.add (juce::var (po));
        lastSent[(size_t) i] = -999.0f;
    }
    obj->setProperty ("params", params);
    emitToUi ("initialState", juce::var (obj));
}

//==============================================================================
void Marmalade120AudioProcessor::timerCallback()
{
    if (! emitToUi) return;

    if (! uiHasState)
    {
        if (++statePushTick >= 3) { statePushTick = 0; emitInitialState(); }
        return;
    }

    // echo what the page did not move itself: automation, presets, host undo
    {
        juce::Array<juce::var> changed;
        for (int i = 0; i < MM_NUM_PARAMS; ++i)
        {
            auto* prm = apvts.getParameter (MM_SPECS[i].id);
            if (prm == nullptr) continue;
            const float v = prm->getValue();
            if (std::abs (v - lastSent[(size_t) i]) > 1.0e-5f)
            {
                lastSent[(size_t) i] = v;
                auto* po = new juce::DynamicObject();
                po->setProperty ("id", MM_SPECS[i].id);
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

    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("in",    engine.inLevel());
        obj->setProperty ("out",   engine.outLevel());
        obj->setProperty ("watts", engine.watts());
        obj->setProperty ("sag",   engine.sag());
        obj->setProperty ("glow",  engine.plateGlow());
        obj->setProperty ("os",    engine.oversampling());
        emitToUi ("meter", juce::var (obj));
    }
}

//==============================================================================
juce::AudioProcessorEditor* Marmalade120AudioProcessor::createEditor()
{
    return new Marmalade120AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Marmalade120AudioProcessor();
}
