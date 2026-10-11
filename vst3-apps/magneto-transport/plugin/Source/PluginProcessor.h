#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "Presets.h"

//==============================================================================
/*  Magneto Transport. The APVTS layout, processBlock's read, the panel's initial
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

    // the factory presets are the host's programs too
    int getNumPrograms() override { return mt::NUM_FACTORY; }
    int getCurrentProgram() override { return juce::jmax (0, presetFactory); }
    void setCurrentProgram (int i) override { loadFactory (i); }
    const juce::String getProgramName (int i) override { return (i >= 0 && i < mt::NUM_FACTORY) ? juce::String (mt::FACTORY[i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

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
    // the standalone's built-in programme (a plucked phrase), for photographs and a first listen
    std::atomic<bool> programme { false };
    double progT = 0.0;
    std::vector<float> prog;
    void renderProgramme (float* out, int n);

    /*  Presets (docs/PROTOCOL.md). Message thread. */
    void loadFactory (int i);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    void deleteUser (const juce::String& name);
    void emitPresets();
    juce::File userFolder() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyParams (const mt::Params& p);     // every parameter but BYPASS, with gestures
    void snapshotPreset (const juce::String& name, int factory);
    bool presetDirty() const;
    void setQuality (int q);
    void patternSet();
    juce::PropertiesFile& settings();
    std::unique_ptr<juce::PropertiesFile> settingsFile;
    juce::String presetName { "Default" };
    int presetFactory = -1;
    std::array<float, mt::NUM_SPECS> presetSnap {};
    bool presetSnapValid = false;
    int quality = 1;
    double lastRate = 0.0; int lastBlock = 0;
    void timerCallback() override;
    float physical (int i) const;

    mt::Engine engine;
    mt::Params current;

    std::array<std::atomic<float>*, mt::NUM_SPECS> raw {};
    std::array<float, mt::NUM_SPECS> lastSent {};
    std::vector<float> scratchL, scratchR;

    int statePushTick = 0;
    int presetTick = 0;
    bool lastDirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnetoTransportAudioProcessor)
};
