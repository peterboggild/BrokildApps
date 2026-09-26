#pragma once

// HATS OFF - the wrapper: host parameters (generated from ho_params.h),
// MIDI, the host tempo for the echo, state, factory presets as host programs,
// and user patches on disk in the house folder
// (Documents\Brokild patches\Hats Off).

#include <JuceHeader.h>

#include "../engine/ho_engine.h"
#include "../engine/ho_presets.h"

class HatsOffProcessor  : public juce::AudioProcessor
{
public:
    HatsOffProcessor();
    ~HatsOffProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 12.0; }    // a 24-inch ride, and the echo

    int getNumPrograms() override { return ho::numPresets(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return ho::preset (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    ho::Params currentParams() const;
    void applyParams (const ho::Params&);            // message thread

    //  a hit from the panel; lands at the start of the next block
    //  strike < 0 keeps the dialled STRIKE; the pad passes where it was clicked
    void triggerFromUI (float velocity01, int artic = ho::ART_DIALLED, float strike = -1.0f)
    {
        uiArtic.store (juce::jlimit (0, ho::NUM_ARTICS - 1, artic));
        uiStrike.store (strike);
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

    ho::Engine engine;
    std::array<std::atomic<float>*, ho::kNumParams> raw {};
    std::atomic<int> uiHit { 0 }, uiArtic { 0 };
    std::atomic<float> uiStrike { -1.0f };
    std::atomic<double> bpm { 120.0 };
    int currentPreset = 0;
    juce::String presetName { "Init" };
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HatsOffProcessor)
};
