#pragma once

#include <JuceHeader.h>
#include "Engine.h"

class PluginProcessorTemplate : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginProcessorTemplate();
    ~PluginProcessorTemplate() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }   // cover your longest tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    /** Called from the page (message thread). */
    void handleUiMessage (const juce::var& payload);

    /** Called by the editor's timer: clock, MIDI relay, automation, GC. */
    void timerService();

    ps::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    /** Set by the editor while it exists; null when there is no UI. */
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void handleOne (const juce::var& message);
    void applyControlNative (const juce::String& id, float value);   // editor closed
    void collectGarbage();

    // MIDI → UI relay (audio thread writes, message thread reads)
    struct MidiUiEvent { int on = 0; int note = 0; };
    juce::AbstractFifo midiUiFifo { 256 };
    std::array<MidiUiEvent, 256> midiUiStorage {};

    // host automation relay
    juce::CriticalSection dirtyLock;
    juce::StringArray dirtyParams;
    std::atomic<bool> suppressEcho { false };

    // the page's own state blob (presets, assets), stored opaquely
    juce::CriticalSection stateLock;
    juce::String uiStateJson;

    // bulk payloads awaiting collection
    juce::Array<ps::EventBatch*> ownedBatches;

    std::unique_ptr<juce::FileChooser> activeChooser;
    std::unique_ptr<juce::ThreadPool> workPool;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessorTemplate)
};
