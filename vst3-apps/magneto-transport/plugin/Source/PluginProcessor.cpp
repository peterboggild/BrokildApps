#include "PluginProcessor.h"
#include "PluginEditor.h"

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
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, rangeFor (s), s.def,
                    juce::AudioParameterFloatAttributes().withLabel (s.unit)));
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
    engine.prepare (sampleRate, samplesPerBlock);
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
        copyXmlToBinary (*xml, dest);
    }
}

void MagnetoTransportAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    if (xml->hasAttribute ("mtWear"))
        engine.setWearState (xml->getDoubleAttribute ("mtWear"), xml->getIntAttribute ("mtWearStock", -1), xml->getDoubleAttribute ("mtWearLoop"));
    for (auto* a : { "mtWear", "mtWearStock", "mtWearLoop" }) xml->removeAttribute (a);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    uiHasState = false;
}

//==============================================================================
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
    emitToUi ("initialState", juce::var (obj));
}

//==============================================================================
void MagnetoTransportAudioProcessor::timerCallback()
{
    engine.service();          // tape allocation for splices; must run with the editor closed

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
