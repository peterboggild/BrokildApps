#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <brokild_paths.h>

namespace
{
    juce::String valueText (const ks::PSpec& s, float v)
    {
        const juce::String u (s.unit);
        if (s.kind == ks::K_CHOICE)
            return juce::StringArray::fromTokens (s.choices, "|", "")[juce::jlimit (0, 3, juce::roundToInt (v))];
        if (s.kind == ks::K_BOOL) return v > 0.5f ? "ON" : "OFF";
        if (u == "Hz") return juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
        if (u == "st") return juce::String (v, 1) + " st";
        if (u == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
        if (u == "%")  return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        if (u == "bi") { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + " %"; }
        if (u == "dB") return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        return juce::String (v, 2);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout KickstartProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < ks::kNumParams; ++i)
    {
        const auto& s = ks::specs()[i];
        const juce::ParameterID pid { s.id, 1 };
        if (s.kind == ks::K_CHOICE)
            l.add (std::make_unique<juce::AudioParameterChoice> (
                pid, s.name, juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
        else if (s.kind == ks::K_BOOL)
            l.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
        else
        {
            juce::NormalisableRange<float> r (s.lo, s.hi);
            if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
            const ks::PSpec* sp = &s;
            l.add (std::make_unique<juce::AudioParameterFloat> (
                pid, s.name, r, s.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [sp] (float v, int) { return valueText (*sp, v); })));
        }
    }
    return l;
}

KickstartProcessor::KickstartProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KICKSTART", layout())
{
    for (int i = 0; i < ks::kNumParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (ks::specs()[i].id);
    //  a fresh instance IS the Init preset, level and all
    applyParams (ks::presetParams (0));
}

bool KickstartProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void KickstartProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
}

ks::Params KickstartProcessor::currentParams() const
{
    ks::Params p;
    for (int i = 0; i < ks::kNumParams; ++i)
        p.*(ks::specs()[i].member) = raw[(size_t) i]->load();
    return p;
}

void KickstartProcessor::applyParams (const ks::Params& p)
{
    for (int i = 0; i < ks::kNumParams; ++i)
        if (auto* prm = apvts.getParameter (ks::specs()[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(ks::specs()[i].member)));
}

void KickstartProcessor::setCurrentProgram (int index)
{
    currentPreset = juce::jlimit (0, ks::numPresets() - 1, index);
    presetName = ks::preset (currentPreset).name;
    applyParams (ks::presetParams (currentPreset));
}

//==============================================================================
void KickstartProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const ks::Params p = currentParams();

    if (const int v = uiHit.exchange (0); v > 0)
        engine.noteOn (36, v / 127.0f);

    float* out = buffer.getWritePointer (0);
    int done = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > done) { engine.process (p, out + done, at - done); done = at; }
        engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    }
    if (done < n) engine.process (p, out + done, n - done);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n);
}

//==============================================================================
void KickstartProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void KickstartProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        currentPreset = (int) tree.getProperty ("preset", 0);
        presetName = tree.getProperty ("presetName", ks::preset (currentPreset).name).toString();
        apvts.replaceState (tree);
    }
}

//==============================================================================
juce::File KickstartProcessor::patchFolder() const
{
    return brokild::patchFolder ("Kickstart", { "kickstart" });
}

bool KickstartProcessor::saveUserPatch (const juce::File& f, const juce::String& name)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("app", "kickstart");
    obj->setProperty ("build", KS_BUILD_ID);
    obj->setProperty ("name", name);
    auto* vals = new juce::DynamicObject();
    const auto p = currentParams();
    for (int i = 0; i < ks::kNumParams; ++i)
        vals->setProperty (ks::specs()[i].id, p.*(ks::specs()[i].member));
    obj->setProperty ("params", juce::var (vals));
    const bool ok = f.replaceWithText (juce::JSON::toString (juce::var (obj), false));
    if (ok) presetName = name;
    return ok;
}

bool KickstartProcessor::loadUserPatch (const juce::File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject() || v["app"].toString() != "kickstart") return false;
    //  defaults first, then whatever the file names: a patch from an older
    //  build lands every parameter it did not know about at its default
    ks::Params p = ks::presetParams (0);
    for (int i = 0; i < ks::kNumParams; ++i) p.*(ks::specs()[i].member) = ks::specs()[i].def;
    if (auto* d = v["params"].getDynamicObject())
        for (const auto& kv : d->getProperties())
            ks::setById (p, kv.name.toString().toRawUTF8(), (float) (double) kv.value);
    applyParams (p);
    presetName = v["name"].toString().isNotEmpty() ? v["name"].toString() : f.getFileNameWithoutExtension();
    return true;
}

juce::AudioProcessorEditor* KickstartProcessor::createEditor() { return new KickstartEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickstartProcessor(); }
