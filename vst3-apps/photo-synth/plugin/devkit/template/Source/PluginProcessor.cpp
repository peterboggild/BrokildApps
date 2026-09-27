#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ps;

//==============================================================================
// One entry per automatable control, in the slider's own units so the host's
// automation lane reads like the interface.
struct ControlDef { const char* id; float min, max, def; };
static const ControlDef kControls[] =
{
    { "vol", 0, 100, 70 },
    // … one per slider you want automatable …
};

juce::AudioProcessorValueTreeState::ParameterLayout
PluginProcessorTemplate::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& c : kControls)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { c.id, 1 }, c.id,
            juce::NormalisableRange<float> (c.min, c.max, 0.0f), c.def));
    return layout;
}

//==============================================================================
PluginProcessorTemplate::PluginProcessorTemplate()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (const auto& c : kControls) apvts.addParameterListener (c.id, this);
    workPool = std::make_unique<juce::ThreadPool> (1);
}

PluginProcessorTemplate::~PluginProcessorTemplate()
{
    for (const auto& c : kControls) apvts.removeParameterListener (c.id, this);
    workPool->removeAllJobs (true, 4000);
    for (auto* b : ownedBatches) delete b;
}

void PluginProcessorTemplate::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
}

bool PluginProcessorTemplate::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void PluginProcessorTemplate::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        const int off = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);

        if (msg.isNoteOn() || msg.isNoteOff())
        {
            if (msg.isNoteOn()) engine.hostNoteOn (msg.getNoteNumber(), off);
            else                engine.hostNoteOff (msg.getNoteNumber(), off);

            int s1, n1, s2, n2;                       // relay to the UI
            midiUiFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0)
            {
                midiUiStorage[(size_t) s1] = { msg.isNoteOn() ? 1 : 0, msg.getNoteNumber() };
                midiUiFifo.finishedWrite (1);
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine.hostAllNotesOff();
        }
    }
    midiMessages.clear();

    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
void PluginProcessorTemplate::handleUiMessage (const juce::var& payload)
{
    if (auto* batch = payload.getProperty ("b", juce::var()).getArray())
    {
        for (const auto& m : *batch) handleOne (m);
        return;
    }
    handleOne (payload);
}

void PluginProcessorTemplate::handleOne (const juce::var& m)
{
    const juce::String k = m.getProperty ("k", juce::var()).toString();

    if (k == "p")                                   // parameter
    {
        Command c;
        c.param = (int16_t) (int) m.getProperty ("i", 0);
        if (c.param < 0 || c.param >= numParams) return;
        const int mode = (int) m.getProperty ("m", 1);
        c.f1 = (float) (double) m.getProperty ("v", 0.0);
        c.f2 = (float) (double) m.getProperty ("tc", 0.0);
        c.d1 = (double) m.getProperty ("t", -1.0);
        c.type = mode == 2 ? Command::cancelParam
               : mode == 0 ? Command::setParamValue
                           : Command::setParamTarget;
        engine.pushCommand (c);
        if (mode != 2) engine.shadowParams[(size_t) c.param] = c.f1;
    }
    else if (k == "v")        { /* voice fields  → Command::setVoiceField */ }
    else if (k == "ve")       { /* voice events  → EventBatch + ownedBatches */ }
    else if (k == "control")  // the page moved a slider: mirror to the host
    {
        if (auto* param = apvts.getParameter (m.getProperty ("id", juce::var()).toString()))
        {
            suppressEcho = true;
            param->setValueNotifyingHost (param->convertTo0to1 ((float) (double) m.getProperty ("v", 0.0)));
            suppressEcho = false;
        }
    }
    else if (k == "state")    // the page's preset blob, assets included
    {
        const juce::ScopedLock sl (stateLock);
        uiStateJson = m.getProperty ("j", juce::var()).toString();
    }
    else if (k == "getstate" && emitToUi)
    {
        juce::String j;
        { const juce::ScopedLock sl (stateLock); j = uiStateJson; }
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("j", j);
        obj->setProperty ("fs", engine.getSampleRate());
        emitToUi ("initialState", juce::var (obj));
    }
}

//==============================================================================
void PluginProcessorTemplate::parameterChanged (const juce::String& parameterID, float)
{
    // Can arrive on the audio thread — only record that it is dirty.
    if (suppressEcho.load()) return;
    const juce::ScopedLock sl (dirtyLock);
    dirtyParams.addIfNotAlreadyThere (parameterID);
}

void PluginProcessorTemplate::applyControlNative (const juce::String& id, float v)
{
    // With no editor open there is no page to apply the slider's mapping, so
    // reimplement it here for the parameters that can work headless.
    auto setP = [this] (int param, float value, float tc)
    {
        Command c; c.type = Command::setParamTarget;
        c.param = (int16_t) param; c.f1 = value; c.f2 = tc; c.d1 = -1;
        engine.shadowParams[(size_t) param] = value;
        engine.pushCommand (c);
    };

    if (id == "vol") setP (pMaster, v / 100.0f, 0.03f);
    // … the rest …
}

void PluginProcessorTemplate::timerService()
{
    if (emitToUi)                                   // clock for the page
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("t", engine.getCurrentTime());
        obj->setProperty ("fs", engine.getSampleRate());
        emitToUi ("clock", juce::var (obj));
    }

    juce::StringArray dirty;                        // host automation
    { const juce::ScopedLock sl (dirtyLock); dirty.swapWith (dirtyParams); }
    for (const auto& id : dirty)
    {
        const float v = apvts.getRawParameterValue (id)->load();
        if (emitToUi)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("id", id);
            obj->setProperty ("v", v);
            emitToUi ("hostParam", juce::var (obj));
        }
        else applyControlNative (id, v);
    }

    {                                               // MIDI → UI
        int s1, n1, s2, n2;
        midiUiFifo.prepareToRead (midiUiFifo.getNumReady(), s1, n1, s2, n2);
        auto emitOne = [this] (const MidiUiEvent& e)
        {
            if (! emitToUi) return;
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("on", e.on);
            obj->setProperty ("note", e.note);
            emitToUi ("midiIn", juce::var (obj));
        };
        for (int i = 0; i < n1; ++i) emitOne (midiUiStorage[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) emitOne (midiUiStorage[(size_t) (s2 + i)]);
        midiUiFifo.finishedRead (n1 + n2);
    }

    collectGarbage();
}

void PluginProcessorTemplate::collectGarbage()
{
    // Free bulk payloads the audio thread has finished copying. Never delete
    // these on the audio thread.
    for (int i = ownedBatches.size(); --i >= 0;)
        if (ownedBatches.getUnchecked (i)->consumed.load())
        {
            delete ownedBatches.getUnchecked (i);
            ownedBatches.remove (i);
        }
}

//==============================================================================
void PluginProcessorTemplate::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("apvts", apvts.copyState().toXmlString());
    { const juce::ScopedLock sl (stateLock); root->setProperty ("ui", uiStateJson); }
    // root->setProperty ("engine", engine.snapshotState());

    const auto json = juce::JSON::toString (juce::var (root), true);
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void PluginProcessorTemplate::setStateInformation (const void* data, int sizeInBytes)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, sizeInBytes));
    if (! v.isObject()) return;

    if (auto xml = juce::XmlDocument::parse (v.getProperty ("apvts", juce::var()).toString()))
    {
        suppressEcho = true;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        suppressEcho = false;
    }
    { const juce::ScopedLock sl (stateLock); uiStateJson = v.getProperty ("ui", juce::var()).toString(); }
    // engine.applyStateSnapshot (v.getProperty ("engine", juce::var()));

    if (emitToUi)                    // an open editor gets the restored page state
    {
        juce::String j;
        { const juce::ScopedLock sl (stateLock); j = uiStateJson; }
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("j", j);
        emitToUi ("initialState", juce::var (obj));
    }
}

juce::AudioProcessorEditor* PluginProcessorTemplate::createEditor()
{
    return new PluginEditorTemplate (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessorTemplate();
}
