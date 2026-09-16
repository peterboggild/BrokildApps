#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String pctText (float v, int)   { return juce::String ((int) std::lround (v)) + " %"; }
    juce::String dbText  (float v, int)   { return v <= -59.5f ? juce::String ("-inf")
                                                               : juce::String (v, 1) + " dB"; }
    juce::String panText (float v, int)
    {
        const int p = (int) std::lround (v);
        if (p == 0) return "C";
        return (p < 0 ? "L" : "R") + juce::String (std::abs (p));
    }
}

// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout LegionProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { legion_ids::mix, 1 }, "MIX",
        juce::NormalisableRange<float> (0.0f, 100.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { legion_ids::output, 1 }, "OUTPUT",
        juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { legion_ids::humanize, 1 }, "HUMANISE",
        juce::NormalisableRange<float> (0.0f, 100.0f), 35.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));

    l.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { legion_ids::detail, 1 }, "DETAIL",
        juce::StringArray { "TIGHT", "NATURAL", "SMOOTH" }, 1));

    l.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { legion_ids::rackPos, 1 }, "BWFX ON",
        juce::StringArray { "HARMONY", "MASTER" }, 0));

    for (int v = 0; v < legion::kVoices; ++v)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { legion_ids::voice (v, "on"), 1 },
            "VOICE " + juce::String (v + 1), v == 0));

        for (int s = 0; s < kNumVoiceSpecs; ++s)
        {
            const auto& sp = kVoiceSpecs[s];
            float def = sp.def;
            if (juce::String (sp.id) == "pitch") def = kDefaultPitch[v];
            if (juce::String (sp.id) == "pan")   def = kDefaultPan[v];

            const juce::String unit (sp.unit);
            auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                juce::String (sp.id) == "pan"   ? std::function<juce::String(float,int)> (panText)
              : unit == " dB"                   ? std::function<juce::String(float,int)> (dbText)
              : [unit] (float x, int) { return juce::String (x, unit == " ct" || unit == " ms" ? 0 : 2) + unit; });

            l.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { legion_ids::voice (v, sp.id), 1 },
                "V" + juce::String (v + 1) + " " + sp.name,
                juce::NormalisableRange<float> (sp.lo, sp.hi, sp.step), def, attrs));
        }
    }

    bwfx_juce::addMacroParameters (l);
    return l;
}

// ---------------------------------------------------------------------------
LegionProcessor::LegionProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LEGION", layout())
{
    pMix      = apvts.getRawParameterValue (legion_ids::mix);
    pOutput   = apvts.getRawParameterValue (legion_ids::output);
    pHumanize = apvts.getRawParameterValue (legion_ids::humanize);
    pDetail   = apvts.getRawParameterValue (legion_ids::detail);
    pRackPos  = apvts.getRawParameterValue (legion_ids::rackPos);

    for (int v = 0; v < legion::kVoices; ++v)
    {
        pVoiceOn[v] = apvts.getRawParameterValue (legion_ids::voice (v, "on"));
        for (int s = 0; s < kNumVoiceSpecs; ++s)
            pVoice[v][s] = apvts.getRawParameterValue (legion_ids::voice (v, kVoiceSpecs[s].id));
    }

    apvts.addParameterListener (legion_ids::detail, this);

    //  the rack's IR builds and other message-thread work must keep happening
    //  with the editor CLOSED — a project restore can enable the reverb long
    //  before the window opens (BWFX-DESIGN.md, the hard-won rules)
    startTimerHz (15);
}

LegionProcessor::~LegionProcessor()
{
    stopTimer();
    apvts.removeParameterListener (legion_ids::detail, this);
}

void LegionProcessor::timerCallback()
{
    bwfxRack.service();

    //  DETAIL changed the window, so the reported latency changed with it.
    //  Renegotiating latency is a message-thread job; the audio thread only
    //  ever sets the flag.
    if (latencyDirty.exchange (false))
        setLatencySamples (engine.latencySamples());
}

void LegionProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == legion_ids::detail)
    {
        engine.setDetail ((int) pDetail->load());
        latencyDirty.store (true);
    }
}

// ---------------------------------------------------------------------------
bool LegionProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in  = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    //  mono in / stereo out is the normal way to use this: one singer, a
    //  choir spread across the image
    return out.size() >= in.size();
}

void LegionProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setDetail ((int) pDetail->load());
    engine.prepare (sampleRate, samplesPerBlock);
    bwfxRack.prepare (sampleRate, samplesPerBlock);

    harmBuf.setSize (2, samplesPerBlock, false, false, true);
    dryBuf .setSize (2, samplesPerBlock, false, false, true);

    for (auto* g : { &dryGain, &wetGain, &outGain }) g->reset (sampleRate, 0.02);

    setLatencySamples (engine.latencySamples());
}

// ---------------------------------------------------------------------------
void LegionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0) return;

    if (harmBuf.getNumSamples() < n) { harmBuf.setSize (2, n, false, false, true);
                                       dryBuf .setSize (2, n, false, false, true); }

    // ---- gather the controls ----------------------------------------------
    params.humanize = pHumanize->load() * 0.01f;
    for (int v = 0; v < legion::kVoices; ++v)
    {
        auto& vp = params.v[v];
        vp.on        = pVoiceOn[v]->load() > 0.5f;
        vp.semitones = pVoice[v][0]->load();
        vp.cents     = pVoice[v][1]->load();
        vp.formant   = pVoice[v][2]->load();
        vp.follow    = pVoice[v][3]->load() * 0.01f;
        vp.gainDb    = pVoice[v][4]->load();
        vp.pan       = pVoice[v][5]->load() * 0.01f;
        vp.delayMs   = pVoice[v][6]->load();
    }

    // ---- the engine --------------------------------------------------------
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numIn > 1 ? 1 : 0);

    engine.process (params, inL, inR, n,
                    harmBuf.getWritePointer (0), harmBuf.getWritePointer (1),
                    dryBuf .getWritePointer (0), dryBuf .getWritePointer (1));

    // ---- BWFX --------------------------------------------------------------
    double bpm = 0.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm().orFallback (0.0);
            ppq = pos->getPpqPosition().orFallback (-1.0);
            playing = pos->getIsPlaying();
        }
    bwfxRack.setTransport (bpm, ppq, playing);
    bwfx_juce::pushMacros (bwfxRack, apvts);

    const bool rackOnHarmony = pRackPos->load() < 0.5f;
    if (rackOnHarmony)
        bwfxRack.process (harmBuf.getWritePointer (0), harmBuf.getWritePointer (1), n);

    // ---- the mix -----------------------------------------------------------
    /*  Equal power, so the loudness does not dip in the middle of the knob's
        travel. MIX 0 is the singer alone and bit-identical to the input up
        to the output trim (the dry path is a delay line and nothing else);
        MIX 100 is the choir alone. */
    const float m = juce::jlimit (0.0f, 1.0f, pMix->load() * 0.01f);
    dryGain.setTargetValue (std::cos (m * 0.5f * juce::MathConstants<float>::pi));
    wetGain.setTargetValue (std::sin (m * 0.5f * juce::MathConstants<float>::pi));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load(), -24.0f));

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (numOut > 1 ? 1 : 0);
    const float* hL = harmBuf.getReadPointer (0);
    const float* hR = harmBuf.getReadPointer (1);
    const float* dL = dryBuf .getReadPointer (0);
    const float* dR = dryBuf .getReadPointer (1);

    if (numOut > 1)
    {
        for (int i = 0; i < n; ++i)
        {
            const float d = dryGain.getNextValue(), w = wetGain.getNextValue(), o = outGain.getNextValue();
            outL[i] = (dL[i] * d + hL[i] * w) * o;
            outR[i] = (dR[i] * d + hR[i] * w) * o;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float d = dryGain.getNextValue(), w = wetGain.getNextValue(), o = outGain.getNextValue();
            outL[i] = (dL[i] * d + 0.5f * (hL[i] + hR[i]) * w) * o;
        }
    }

    if (! rackOnHarmony)
    {
        if (numOut > 1)
        {
            bwfxRack.process (outL, outR, n);
        }
        else
        {
            //  the rack is a stereo processor and must never be handed the
            //  same pointer twice; dryBuf has been consumed by now, so its
            //  right channel is the scratch side
            float* scratch = dryBuf.getWritePointer (1);
            std::memcpy (scratch, outL, sizeof (float) * (size_t) n);
            bwfxRack.process (outL, scratch, n);
        }
    }
}

// ---------------------------------------------------------------------------
void LegionProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    //  the rack rides inside the host's own state as ONE opaque string, the
    //  BWFX contract: unknown keys are ignored, missing modules get defaults,
    //  so a rack from a newer build round-trips through an older one
    state.setProperty ("bwfx", juce::String (bwfxRack.toJson()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void LegionProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;

        const juce::String blob = tree.getProperty ("bwfx", juce::String()).toString();
        apvts.replaceState (tree);
        bwfxRack.fromJson (blob.toStdString());      // "" = the default empty rack

        engine.setDetail ((int) pDetail->load());
        latencyDirty.store (true);
    }
}

juce::AudioProcessorEditor* LegionProcessor::createEditor()
{
    return new LegionEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LegionProcessor();
}
