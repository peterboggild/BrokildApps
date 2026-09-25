#pragma once

// KICKSTART - the wrapper: host parameters (generated from ks_params.h), MIDI,
// state, factory presets as host programs, and user patches on disk in the
// house folder (Documents\Brokild patches\Kickstart).

#include <JuceHeader.h>

#include "../engine/ks_engine.h"
#include "../engine/ks_presets.h"

class KickstartProcessor  : public juce::AudioProcessor
{
public:
    KickstartProcessor();
    ~KickstartProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.5; }

    int getNumPrograms() override { return ks::numPresets(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return ks::preset (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    //  the controls as the engine sees them, read from the host parameters
    ks::Params currentParams() const;
    void applyParams (const ks::Params&);            // message thread

    //  a hit from the panel's pad; lands at the start of the next block
    void triggerFromUI (float velocity01) { uiHit.store (juce::jlimit (1, 127, juce::roundToInt (velocity01 * 127.0f))); }

    int   hitCount() const    { return engine.hitCount(); }
    float meterPeak() const   { return engine.meterPeak(); }
    float meterGrDb() const   { return engine.meterGrDb(); }

    //  user patches
    juce::File patchFolder() const;
    bool saveUserPatch (const juce::File&, const juce::String& name);
    bool loadUserPatch (const juce::File&);
    juce::String currentName() const { return presetName; }

    static juce::String idOf (int i) { return ks::specs()[i].id; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    ks::Engine engine;
    std::array<std::atomic<float>*, ks::kNumParams> raw {};
    std::atomic<int> uiHit { 0 };
    int currentPreset = 0;
    juce::String presetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickstartProcessor)
};
