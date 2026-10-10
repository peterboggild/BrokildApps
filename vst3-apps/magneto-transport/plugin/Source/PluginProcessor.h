#pragma once

#include <JuceHeader.h>
#include "Engine.h"

//==============================================================================
/*  MAGNETO TRANSPORT. The APVTS layout, processBlock's read, the panel's initial
    state and the bench all walk mt::SPECS, so a parameter is one row in one
    table and there is no read order to get wrong. */
class MagnetoTransportAudioProcessor : public juce::AudioProcessor,
                                       private juce::Timer
{
public:
    MagnetoTransportAudioProcessor();
    ~MagnetoTransportAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 30.0; }

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
    // page reload the editor never sees leaves the processor believing the old
    // ack and the new page waits for a state that never comes.
    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;
    float physical (int i) const;

    mt::Engine engine;
    mt::Params current;

    std::array<std::atomic<float>*, mt::NUM_SPECS> raw {};
    std::array<float, mt::NUM_SPECS> lastSent {};
    std::vector<float> scratchL, scratchR;

    int statePushTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnetoTransportAudioProcessor)
};
