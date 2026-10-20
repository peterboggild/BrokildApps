#pragma once

#include <JuceHeader.h>
#include "Engine50.h"

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
class Marmalade50AudioProcessor : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    Marmalade50AudioProcessor();
    ~Marmalade50AudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 3.0; }      // the springs ring

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

    /*  TANK: a measured impulse response of the reverb tank - drive volts
        across its input coil to volts at its output, as measured, not
        normalised. Without one the reverb circuit runs and the springs are
        silent. Message thread only. */
    void chooseTank();
    bool loadTank (const juce::File& f);
    void clearTank();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    void handTankToEngine();

    mm::Engine50 engine;
    juce::dsp::Convolution cabinet;
    juce::dsp::Convolution springs;                 // zero latency: the engine needs the return a chunk ahead
    juce::AudioBuffer<float> tankIr;                // as read, at its own rate, for a re-trim when the rate changes
    double tankIrRate = 0.0;
    juce::String tankPath, tankName;
    std::atomic<bool> tankLoaded { false };
    bool cabOn = true;
    juce::String irPath, irName;
    std::atomic<bool> irLoaded { false };
    std::unique_ptr<juce::FileChooser> chooser;
    mm::Params50 current;
    double preparedRate = 0.0;
    int preparedBlock = 512;
    int preparedQuality = -1;

    std::array<std::atomic<float>*, 24> paramPtr {};
    std::array<float, 24> lastSent {};
    int statePushTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Marmalade50AudioProcessor)
};
