#pragma once

#include <JuceHeader.h>
#include <map>
#include "Core/cw_core.h"

//==============================================================================
class CloneWarsProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    CloneWarsProcessor();
    ~CloneWarsProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "Clone Wars"; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 6.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // UI bridge
    void handleUiMessage (const juce::var& payload);
    void timerService();                       // called by the editor's timer
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::String globalParamId (int g);
    static juce::String voiceParamId (int v, int f);

    cw::Engine engine;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void pushAllParamsToEngine();
    void applySeed (uint32_t seed);
    void sendInitToUi();

    juce::AudioProcessorValueTreeState apvts;

    // parameterID → engine slot, resolved once at construction
    struct Slot { int global = -1; int voice = -1; int field = -1; };
    std::map<juce::String, Slot> slots;

    // the hull remembers: per-instance, travels with the DAW project
    std::atomic<int64_t>  ageSamples { 0 };      // time spent audibly playing
    // A unit leaves the factory undamaged, and the scars are the instance's
    // own - never a patch's. wearSeed is drawn per instance in the ctor.
    std::atomic<double>   wearPoints { 0.0 };
    std::atomic<uint32_t> wearSeed { 1337 };
    std::atomic<uint32_t> unitSeed { 0xC70BE5u };
    std::atomic<int>      currentSeedA { 42 }, currentSeedB { 137 };
    juce::String          currentCategory { "default" };

    std::atomic<float>* hqRaw = nullptr;
    std::atomic<bool> suppressEcho { false };
    juce::CriticalSection dirtyLock;
    juce::StringArray dirtyParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloneWarsProcessor)
};
