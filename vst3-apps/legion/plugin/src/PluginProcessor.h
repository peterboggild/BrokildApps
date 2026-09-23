#pragma once

// LEGION — "my name is Legion, for we are many". One voice in, a choir of
// it out, with the pitch of each copy and the size of the body that sings
// it under separate control.
//
// The DSP lives in engine/ and knows nothing about JUCE; this file is the
// wrapper: host parameters, buses, state, and the BWFX rack.
//
// WHERE BWFX SITS. On the HARMONY bus by default, before the dry/wet mix —
// so the rack processes the second voice and leaves the singer alone, which
// is the point (grind the harmony, keep the lead clean). MASTER puts it
// after the mix for when the whole thing should go through the pedals.

#include <JuceHeader.h>

#include "../engine/legion_harmonizer.h"
#include "Params.h"
#include <bwfx.h>
#include <bwfx_juce.h>

class LegionProcessor  : public juce::AudioProcessor,
                         private juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer
{
public:
    LegionProcessor();
    ~LegionProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "LEGION"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack& rack() { return bwfxRack; }

    //  the editor's rack panel edits through here so the state echo and the
    //  macro gesture rules stay in one place (bwfx_juce's contract)
    bool handleRackMessage (const juce::var& m) { return bwfx_juce::handleMessage (bwfxRack, apvts, m); }

    float detectedF0() const { return engine.lastF0(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void parameterChanged (const juce::String& id, float value) override;
    void timerCallback() override;

    legion::Harmonizer engine;
    legion::Params     params;

    bwfx::Rack bwfxRack;
    std::atomic<bool> latencyDirty { false };

    juce::AudioBuffer<float> harmBuf, dryBuf;

    //  cached raw pointers: getRawParameterValue does a string lookup, and
    //  processBlock is not the place for 43 of those
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pDetail = nullptr;
    std::atomic<float>* pRackPos = nullptr;
    std::atomic<float>* pVoice[legion::kVoices][kNumVoiceSpecs] {};
    std::atomic<float>* pVoiceOn[legion::kVoices] {};

    juce::LinearSmoothedValue<float> dryGain, wetGain, outGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LegionProcessor)
};
