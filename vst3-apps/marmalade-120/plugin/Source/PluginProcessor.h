#pragma once

#include <JuceHeader.h>
#include "Engine.h"

//==============================================================================
/*  One row per host parameter. The APVTS layout, processBlock's read, the
    initial-state push and the panel are all walked from this ONE table (the
    Hairfryer pattern, as in Battlestar Overdrive). */
struct PSpec
{
    const char* id;
    const char* name;
    float       def;            // normalised 0..1 for floats, the index for choices
    int         steps;          // 0: continuous; 2: a switch; more: a rotary switch
    const char* choices;        // pipe separated names for a choice, or null
};

extern const PSpec MM_SPECS[];
extern const int   MM_NUM_PARAMS;

//==============================================================================
class Marmalade120AudioProcessor : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    Marmalade120AudioProcessor();
    ~Marmalade120AudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.5; }

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

    // The page sends {k:"hello"} on every boot, which clears this, so a reload
    // the editor never sees still gets a fresh state.
    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady { false };

    /*  CAB: a measured cabinet impulse response, convolved after the amp.
        Nothing about a cabinet is made up here - without an IR, CAB passes
        the 8 ohm tap through. Message thread only. */
    void chooseImpulseResponse();
    bool loadImpulseResponse (const juce::File& f);
    void clearImpulseResponse();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    mm::Engine engine;
    juce::dsp::Convolution cabinet;
    bool cabOn = true;
    juce::String irPath, irName;
    std::atomic<bool> irLoaded { false };
    std::unique_ptr<juce::FileChooser> chooser;
    mm::Params current;
    double preparedRate = 0.0;
    int preparedBlock = 512;
    int preparedQuality = -1;

    std::array<std::atomic<float>*, 16> paramPtr {};
    std::array<float, 16> lastSent {};
    int statePushTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Marmalade120AudioProcessor)
};
