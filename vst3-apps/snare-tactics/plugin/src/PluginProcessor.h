#pragma once

// SNARE TACTICS - the wrapper: host parameters (generated from st_params.h),
// MIDI, the host tempo for the echo, state, factory presets as host programs,
// and user patches on disk in the house folder
// (Documents\Brokild patches\Snare Tactics).

#include <JuceHeader.h>

#include "../engine/st_engine.h"
#include "../engine/st_presets.h"

class SnareTacticsProcessor  : public juce::AudioProcessor
{
public:
    SnareTacticsProcessor();
    ~SnareTacticsProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }     // the echo

    int getNumPrograms() override { return st::numPresets(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return st::preset (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    st::Params currentParams() const;
    void applyParams (const st::Params&);            // message thread

    //  a hit from the panel; lands at the start of the next block
    void triggerFromUI (float velocity01, int artic = st::ART_SNARE)
    {
        uiArtic.store (juce::jlimit (0, st::NUM_ARTICS - 1, artic));
        uiHit.store (juce::jlimit (1, 127, juce::roundToInt (velocity01 * 127.0f)));
    }

    int   hitCount() const    { return engine.hitCount(); }
    float meterPeak() const   { return engine.meterPeak(); }
    float meterGrDb() const   { return engine.meterGrDb(); }
    double hostBpm() const    { return bpm.load(); }

    //  user patches
    juce::File patchFolder() const;
    bool saveUserPatch (const juce::File&, const juce::String& name);
    bool loadUserPatch (const juce::File&);
    juce::String currentName() const { return presetName; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    st::Engine engine;
    std::array<std::atomic<float>*, st::kNumParams> raw {};
    std::atomic<int> uiHit { 0 }, uiArtic { 0 };
    std::atomic<double> bpm { 120.0 };
    int currentPreset = 0;
    juce::String presetName { "Init" };
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareTacticsProcessor)
};
