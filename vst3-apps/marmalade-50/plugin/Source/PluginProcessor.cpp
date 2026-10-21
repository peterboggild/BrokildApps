#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef MM_BUILD_ID
 #define MM_BUILD_ID "0.0.0"
#endif

//==============================================================================
/*  The whole parameter surface, in one table: the front panel's controls,
    then the things a plugin needs that the amp never had. New rows go at
    the END - processBlock reads in table order, so an inserted row would
    silently rewire everything after it. */
const PSpec MM_SPECS[] =
{
    { "channel",  "CHANNEL",  0.0f,  2, "PREAMP 1|PREAMP 2" },   // the footswitchable channel relays
    { "gain1",    "GAIN 1",   0.40f, 0, nullptr },
    { "shape",    "SHAPE",    0.50f, 0, nullptr },
    { "volume1",  "VOLUME 1", 0.50f, 0, nullptr },
    { "gain2",    "GAIN 2",   0.40f, 0, nullptr },
    { "treble",   "TREBLE",   0.50f, 0, nullptr },
    { "middle",   "MIDDLE",   0.50f, 0, nullptr },
    { "bass",     "BASS",     0.50f, 0, nullptr },
    { "volume2",  "VOLUME 2", 0.50f, 0, nullptr },
    { "reverb",   "REVERB",   0.30f, 0, nullptr },
    { "reverbon", "REVERB ON", 1.0f, 2, nullptr },             // RL2
    { "atten",    "ATTEN",    0.30f, 0, nullptr },               // RV10: clockwise attenuates
    { "attenon",  "ATTEN ON", 0.0f,  2, nullptr },               // RL3
    { "output",   "OUTPUT",   0.5f,  0, nullptr },               // -24 .. +12 dB
    { "cab",      "CAB",      1.0f,  2, nullptr },
    { "quality",  "QUALITY",  0.0f,  2, "NORMAL|HIGH" }
};
const int MM_NUM_PARAMS = (int) (sizeof (MM_SPECS) / sizeof (MM_SPECS[0]));

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Marmalade50AudioProcessor::createParameterLayout()
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
                    [] (float v, int) { return juce::String (mm::Engine50::outputDb (v), 1) + " dB"; })));
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
Marmalade50AudioProcessor::Marmalade50AudioProcessor()
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
    /*  The springs, set once: the engine calls this on the audio thread
        every chunk. A measured tank IR when one is loaded; otherwise the
        built-in spring model (SpringTank.h), which is an estimate. */
    engine.setTank ([this] (float* io, int n)
    {
        if (! tankLoaded.load()) { springModel.process (io, n); return; }
        float* ch[1] = { io };
        juce::dsp::AudioBlock<float> block (ch, 1, (size_t) n);
        springs.process (juce::dsp::ProcessContextReplacing<float> (block));
    });
    startTimerHz (30);
}

Marmalade50AudioProcessor::~Marmalade50AudioProcessor() = default;

bool Marmalade50AudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void Marmalade50AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    current.quality = (int) paramPtr[(size_t) (MM_NUM_PARAMS - 1)]->load();   // QUALITY is the last row
    engine.setParams (current);
    engine.prepare (sampleRate, samplesPerBlock);
    preparedRate = sampleRate;
    preparedBlock = samplesPerBlock;
    preparedQuality = current.quality;
    setLatencySamples (engine.latencySamples());

    cabinet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 1 });
    cabinet.reset();
    springs.prepare ({ sampleRate, (juce::uint32) juce::jmax (samplesPerBlock, mm::Engine50::CHUNK), 1 });
    springs.reset();
    springModel.prepare (sampleRate, mm::Engine50::CHUNK);
    handTankToEngine();
}

/*  The engine asks for the tank's output a chunk ahead (see Engine50.h), so
    the IR goes in advanced by one chunk at the session's rate: its first
    CHUNK samples dropped, which for a spring tank (first arrival tens of
    milliseconds in) are silence. The trim is in the IR's own samples, so it
    is redone when the session's rate changes. */
void Marmalade50AudioProcessor::handTankToEngine()
{
    // the convolution takes a new IR from this thread while the audio thread runs it
    if (! tankLoaded.load() || preparedRate <= 0.0 || tankIr.getNumSamples() == 0) return;
    const int drop = (int) std::lround (mm::Engine50::CHUNK * tankIrRate / preparedRate);
    const int len = tankIr.getNumSamples() - drop;
    if (len <= 0) return;
    juce::AudioBuffer<float> trimmed (1, len);
    trimmed.copyFrom (0, 0, tankIr, 0, drop, len);
    springs.loadImpulseResponse (std::move (trimmed), tankIrRate, juce::dsp::Convolution::Stereo::no,
                                 juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

//==============================================================================
void Marmalade50AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, numSamples);

    // in the table's own order
    int k = 0;
    current.channel  = (int) paramPtr[(size_t) k++]->load();
    current.gain1    = paramPtr[(size_t) k++]->load();
    current.shape    = paramPtr[(size_t) k++]->load();
    current.volume1  = paramPtr[(size_t) k++]->load();
    current.gain2    = paramPtr[(size_t) k++]->load();
    current.treble   = paramPtr[(size_t) k++]->load();
    current.middle   = paramPtr[(size_t) k++]->load();
    current.bass     = paramPtr[(size_t) k++]->load();
    current.volume2  = paramPtr[(size_t) k++]->load();
    current.reverb   = paramPtr[(size_t) k++]->load();
    current.reverbOn = paramPtr[(size_t) k++]->load() > 0.5f;
    current.atten    = paramPtr[(size_t) k++]->load();
    current.attenOn  = paramPtr[(size_t) k++]->load() > 0.5f;
    current.output   = paramPtr[(size_t) k++]->load();
    cabOn            = paramPtr[(size_t) k++]->load() > 0.5f;
    current.quality  = (int) paramPtr[(size_t) k++]->load();

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
    // the amp is mono: a stereo input is summed into it, and both sides get the amp
    if (R != nullptr)
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
    engine.process (L, nullptr, numSamples);

    if (cabOn && irLoaded.load())
    {
        juce::dsp::AudioBlock<float> block (&L, 1, (size_t) numSamples);
        cabinet.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
    if (R != nullptr) std::memcpy (R, L, sizeof (float) * (size_t) numSamples);
}

//==============================================================================
/*  Read the file, scale it to unit energy (exactly as test/render.cpp does,
    so a render and the plugin agree), and hand it to the convolution. JUCE
    resamples it to the session rate. */
bool Marmalade50AudioProcessor::loadImpulseResponse (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0) return false;

    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 10.0));
    juce::AudioBuffer<float> ir ((int) r->numChannels, len);
    r->read (&ir, 0, len, 0, true, true);

    // a stereo IR is summed: the amp is mono, and so is one speaker and one mic
    juce::AudioBuffer<float> mono (1, len);
    mono.clear();
    for (int c = 0; c < ir.getNumChannels(); ++c) mono.addFrom (0, 0, ir, c, 0, len, 1.0f / (float) ir.getNumChannels());

    double e2 = 0.0;
    for (int i = 0; i < len; ++i) e2 += (double) mono.getSample (0, i) * mono.getSample (0, i);
    if (e2 <= 0.0) return false;
    mono.applyGain ((float) (1.0 / std::sqrt (e2)));

    cabinet.loadImpulseResponse (std::move (mono), r->sampleRate, juce::dsp::Convolution::Stereo::no,
                                 juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    irPath = f.getFullPathName();
    irName = f.getFileNameWithoutExtension();
    irLoaded = true;
    uiHasState = false;              // the panel shows the name
    return true;
}

bool Marmalade50AudioProcessor::loadTank (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0) return false;

    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 8.0));
    juce::AudioBuffer<float> ir ((int) r->numChannels, len);
    r->read (&ir, 0, len, 0, true, true);
    juce::AudioBuffer<float> mono (1, len);
    mono.clear();
    for (int c = 0; c < ir.getNumChannels(); ++c) mono.addFrom (0, 0, ir, c, 0, len, 1.0f / (float) ir.getNumChannels());

    tankIr = std::move (mono);
    tankIrRate = r->sampleRate;
    tankPath = f.getFullPathName();
    tankName = f.getFileNameWithoutExtension();
    tankLoaded = true;
    handTankToEngine();
    uiHasState = false;
    return true;
}

void Marmalade50AudioProcessor::clearTank()
{
    tankLoaded = false;
    tankPath.clear();
    tankName.clear();
    uiHasState = false;
}

void Marmalade50AudioProcessor::chooseTank()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a reverb tank impulse response",
                                                   juce::File (tankPath).existsAsFile() ? juce::File (tankPath).getParentDirectory()
                                                                                        : juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadTank (f);
                          });
}

void Marmalade50AudioProcessor::clearImpulseResponse()
{
    irLoaded = false;
    irPath.clear();
    irName.clear();
    uiHasState = false;
}

void Marmalade50AudioProcessor::chooseImpulseResponse()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a cabinet impulse response",
                                                   juce::File (irPath).existsAsFile() ? juce::File (irPath).getParentDirectory()
                                                                                      : juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadImpulseResponse (f);
                          });
}

//==============================================================================
void Marmalade50AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;
    // the IR by path: the project remembers which cabinet, not a copy of it
    xml->setAttribute ("irPath", irPath);
    xml->setAttribute ("tankPath", tankPath);
    copyXmlToBinary (*xml, dest);
}

void Marmalade50AudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    const juce::String path = xml->getStringAttribute ("irPath");
    const juce::String tpath = xml->getStringAttribute ("tankPath");
    xml->removeAttribute ("irPath");
    xml->removeAttribute ("tankPath");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    const juce::File f (path);
    if (path.isEmpty() || ! f.existsAsFile() || ! loadImpulseResponse (f))
    {
        clearImpulseResponse();
        if (path.isNotEmpty()) irName = "MISSING: " + f.getFileName();
    }
    const juce::File tf (tpath);
    if (tpath.isEmpty() || ! tf.existsAsFile() || ! loadTank (tf))
    {
        clearTank();
        if (tpath.isNotEmpty()) tankName = "MISSING: " + tf.getFileName();
    }
    uiHasState = false;          // make the panel take the new values
}

//==============================================================================
void Marmalade50AudioProcessor::handleUiMessage (const juce::var& payload)
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
    if (k == "loadir")   { chooseImpulseResponse(); return; }
    if (k == "clearir")  { clearImpulseResponse(); return; }
    if (k == "loadtank") { chooseTank(); return; }
    if (k == "cleartank"){ clearTank(); return; }

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
void Marmalade50AudioProcessor::emitInitialState()
{
    if (! emitToUi) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("build", MM_BUILD_ID);
    obj->setProperty ("ok", engine.ok());
    obj->setProperty ("error", juce::String (engine.error()));
    obj->setProperty ("ir", irName);
    obj->setProperty ("irLoaded", irLoaded.load());
    obj->setProperty ("tank", tankName);
    obj->setProperty ("tankLoaded", tankLoaded.load());

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
void Marmalade50AudioProcessor::timerCallback()
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
juce::AudioProcessorEditor* Marmalade50AudioProcessor::createEditor()
{
    return new Marmalade50AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Marmalade50AudioProcessor();
}
