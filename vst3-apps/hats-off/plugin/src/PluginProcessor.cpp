#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <brokild_paths.h>

namespace
{
    juce::String valueText (const ho::PSpec& s, float v)
    {
        const juce::String u (s.unit);
        if (s.kind == ho::K_CHOICE)
        {
            const auto items = juce::StringArray::fromTokens (s.choices, "|", "");
            return items[juce::jlimit (0, items.size() - 1, juce::roundToInt (v))];
        }
        if (u == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        if (u == "in") return juce::String (v, 1) + " in";
        if (juce::String (s.id) == "pitch") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
        if (u == "st") return juce::String (v, 1) + " st";
        if (u == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
        if (u == "%")  return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        if (u == "bi") { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + " %"; }
        if (u == "dB") return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        return juce::String (v, 2);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HatsOffProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < ho::kNumParams; ++i)
    {
        const auto& s = ho::specs()[i];
        const juce::ParameterID pid { s.id, 1 };
        if (s.kind == ho::K_CHOICE)
            l.add (std::make_unique<juce::AudioParameterChoice> (
                pid, s.name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        else
        {
            juce::NormalisableRange<float> r (s.lo, s.hi);
            if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
            const ho::PSpec* sp = &s;
            l.add (std::make_unique<juce::AudioParameterFloat> (
                pid, s.name, r, s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [sp] (float v, int) { return valueText (*sp, v); })));
        }
    }
    return l;
}

HatsOffProcessor::HatsOffProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HATSOFF", layout())
{
    for (int i = 0; i < ho::kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (ho::specs()[i].id);
    //  a fresh instance IS the Init preset, level and all
    applyParams (ho::presetParams (0));
}

bool HatsOffProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void HatsOffProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.setSize (1, juce::jmax (1, samplesPerBlock), false, false, true);
    setLatencySamples (engine.latencySamples());
}

ho::Params HatsOffProcessor::currentParams() const
{
    ho::Params p;
    for (int i = 0; i < ho::kNumParams; ++i)
        p.*(ho::specs()[i].member) = raw[(size_t) i]->load();
    return p;
}

void HatsOffProcessor::applyParams (const ho::Params& p)
{
    for (int i = 0; i < ho::kNumParams; ++i)
        if (auto* prm = apvts.getParameter (ho::specs()[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(ho::specs()[i].member)));
}

void HatsOffProcessor::setCurrentProgram (int index)
{
    currentPreset = juce::jlimit (0, ho::numPresets() - 1, index);
    presetName = ho::preset (currentPreset).name;
    applyParams (ho::presetParams (currentPreset));
}

//==============================================================================
void HatsOffProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const ho::Params p = currentParams();

    //  the echo follows the host's tempo; on its own it runs at 120
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) { bpm.store (*b); }
    engine.setTempo (bpm.load());

    if (const int v = uiHit.exchange (0); v > 0)
        engine.noteOnArtic (uiArtic.load(), v / 127.0f, uiStrike.load());

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (outR == nullptr)
    {
        if (scratch.getNumSamples() < n) scratch.setSize (1, n, false, false, true);
        outR = scratch.getWritePointer (0);
    }

    int done = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > done) { engine.process (p, outL + done, outR + done, at - done); done = at; }
        engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    }
    if (done < n) engine.process (p, outL + done, outR + done, n - done);

    //  a mono bus gets the sum of the two sides
    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) outL[i] = 0.5f * (outL[i] + outR[i]);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
void HatsOffProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HatsOffProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        currentPreset = (int) tree.getProperty ("preset", 0);
        presetName = tree.getProperty ("presetName", ho::preset (currentPreset).name).toString();
        apvts.replaceState (tree);
    }
}

//==============================================================================
juce::File HatsOffProcessor::patchFolder() const
{
    return brokild::patchFolder ("Hats Off", { "hats-off" });
}

bool HatsOffProcessor::saveUserPatch (const juce::File& f, const juce::String& name)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("app", "hats-off");
    obj->setProperty ("build", HO_BUILD_ID);
    obj->setProperty ("name", name);
    auto* vals = new juce::DynamicObject();
    const auto p = currentParams();
    for (int i = 0; i < ho::kNumParams; ++i)
        vals->setProperty (ho::specs()[i].id, p.*(ho::specs()[i].member));
    obj->setProperty ("params", juce::var (vals));
    const bool ok = f.replaceWithText (juce::JSON::toString (juce::var (obj), false));
    if (ok) presetName = name;
    return ok;
}

bool HatsOffProcessor::loadUserPatch (const juce::File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v["app"].toString() != "hats-off") return false;
    //  defaults first, then whatever the file names: a patch from an older
    //  build lands every parameter it did not know about at its default
    ho::Params p;
    for (int i = 0; i < ho::kNumParams; ++i) p.*(ho::specs()[i].member) = ho::specs()[i].def;
    if (auto* d = v["params"].getDynamicObject())
        for (const auto& kv : d->getProperties())
            ho::setById (p, kv.name.toString().toRawUTF8(), (float) (double) kv.value);
    applyParams (p);
    presetName = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
    return true;
}

juce::AudioProcessorEditor* HatsOffProcessor::createEditor() { return new HatsOffEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HatsOffProcessor(); }
