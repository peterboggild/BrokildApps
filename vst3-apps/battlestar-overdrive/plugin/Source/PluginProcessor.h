#pragma once

#include <JuceHeader.h>
#include "Engine.h"

//==============================================================================
/*  One row per host parameter. The APVTS layout, processBlock's read, the
    initial-state push and the panel are all walked from this ONE table, so the
    read-order bug that needs a paramcheck script elsewhere cannot be expressed
    here (the Hairfryer pattern). */
struct PSpec
{
    const char* id;
    const char* name;
    float       def;
    bool        stepped;        // a choice, not a continuous value
    int         steps;          // number of choices when stepped
    const char* choices;        // pipe separated names, or null
};

extern const PSpec BO_SPECS[];
extern const int   BO_NUM_PARAMS;

//==============================================================================
class BattlestarOverdriveAudioProcessor : public juce::AudioProcessor,
                                          private juce::Timer
{
public:
    BattlestarOverdriveAudioProcessor();
    ~BattlestarOverdriveAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //--------------------------------------------------------------------------
    void handleUiMessage (const juce::var& payload);
    void emitInitialState();

    juce::AudioProcessorValueTreeState apvts;
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    // The page sends {k:"hello"} on every boot, which clears this. Without it a
    // page reload the editor never sees (a WebView2 crash recovery, a devtools
    // reload) leaves the processor believing the old ack and the new page waits
    // for a state that never comes.
    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    bo::Engine engine;
    bo::Params current;

    std::array<std::atomic<float>*, 12> paramPtr {};
    std::array<float, 12> lastSent {};

    // Panel-only state, carried in the project but never a host parameter.
    juce::String knobSkin { "chrome" };

    int statePushTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BattlestarOverdriveAudioProcessor)
};
