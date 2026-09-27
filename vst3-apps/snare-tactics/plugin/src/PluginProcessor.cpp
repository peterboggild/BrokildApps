#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <brokild_paths.h>

namespace
{
    juce::String valueText (const st::PSpec& s, float v)
    {
        const juce::String u (s.unit);
        if (s.kind == st::K_CHOICE)
        {
            const auto items = juce::StringArray::fromTokens (s.choices, "|", "");
            return items[juce::jlimit (0, items.size() - 1, juce::roundToInt (v))];
        }
        if (juce::String (s.id) == "gate" && v < 0.5f) return "OFF";
        if (u == "Hz") return juce::String (juce::roundToInt (v)) + " Hz";
        if (u == "st") return juce::String (v, 1) + " st";
        if (u == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
        if (u == "%")  return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        if (u == "bi") { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + " %"; }
        if (u == "dB") return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        return juce::String (v, 2);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SnareTacticsProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < st::kNumParams; ++i)
    {
        const auto& s = st::specs()[i];
        const juce::ParameterID pid { s.id, 1 };
        if (s.kind == st::K_CHOICE)
            l.add (std::make_unique<juce::AudioParameterChoice> (
                pid, s.name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        else
        {
            juce::NormalisableRange<float> r (s.lo, s.hi);
            if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
            const st::PSpec* sp = &s;
            l.add (std::make_unique<juce::AudioParameterFloat> (
                pid, s.name, r, s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [sp] (float v, int) { return valueText (*sp, v); })));
        }
    }
    return l;
}

SnareTacticsProcessor::SnareTacticsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SNARETACTICS", layout())
{
    for (int i = 0; i < st::kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (st::specs()[i].id);
    //  a fresh instance IS the Init preset, level and all
    applyParams (st::presetParams (0));
}

bool SnareTacticsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SnareTacticsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.setSize (1, juce::jmax (1, samplesPerBlock), false, false, true);
    setLatencySamples (engine.latencySamples());
}

st::Params SnareTacticsProcessor::currentParams() const
{
    st::Params p;
    for (int i = 0; i < st::kNumParams; ++i)
        p.*(st::specs()[i].member) = raw[(size_t) i]->load();
    return p;
}

void SnareTacticsProcessor::applyParams (const st::Params& p)
{
    for (int i = 0; i < st::kNumParams; ++i)
        if (auto* prm = apvts.getParameter (st::specs()[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(st::specs()[i].member)));
}

void SnareTacticsProcessor::setCurrentProgram (int index)
{
    currentPreset = juce::jlimit (0, st::numPresets() - 1, index);
    presetName = st::preset (currentPreset).name;
    applyParams (st::presetParams (currentPreset));
}

//==============================================================================
void SnareTacticsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const st::Params p = currentParams();

    //  the echo follows the host's tempo; on its own it runs at 120
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) { bpm.store (*b); }
    engine.setTempo (bpm.load());

    if (const int v = uiHit.exchange (0); v > 0)
        engine.noteOnArtic (uiArtic.load(), v / 127.0f);

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
void SnareTacticsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SnareTacticsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        currentPreset = (int) tree.getProperty ("preset", 0);
        presetName = tree.getProperty ("presetName", st::preset (currentPreset).name).toString();
        apvts.replaceState (tree);
    }
}

//==============================================================================
juce::File SnareTacticsProcessor::patchFolder() const
{
    return brokild::patchFolder ("Snare Tactics", { "snare-tactics" });
}

bool SnareTacticsProcessor::saveUserPatch (const juce::File& f, const juce::String& name)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("app", "snare-tactics");
    obj->setProperty ("build", ST_BUILD_ID);
    obj->setProperty ("name", name);
    auto* vals = new juce::DynamicObject();
    const auto p = currentParams();
    for (int i = 0; i < st::kNumParams; ++i)
        vals->setProperty (st::specs()[i].id, p.*(st::specs()[i].member));
    obj->setProperty ("params", juce::var (vals));
    const bool ok = f.replaceWithText (juce::JSON::toString (juce::var (obj), false));
    if (ok) presetName = name;
    return ok;
}

bool SnareTacticsProcessor::loadUserPatch (const juce::File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v["app"].toString() != "snare-tactics") return false;
    //  defaults first, then whatever the file names: a patch from an older
    //  build lands every parameter it did not know about at its default
    st::Params p;
    for (int i = 0; i < st::kNumParams; ++i) p.*(st::specs()[i].member) = st::specs()[i].def;
    if (auto* d = v["params"].getDynamicObject())
        for (const auto& kv : d->getProperties())
            st::setById (p, kv.name.toString().toRawUTF8(), (float) (double) kv.value);
    applyParams (p);
    presetName = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
    return true;
}

juce::AudioProcessorEditor* SnareTacticsProcessor::createEditor() { return new SnareTacticsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnareTacticsProcessor(); }
