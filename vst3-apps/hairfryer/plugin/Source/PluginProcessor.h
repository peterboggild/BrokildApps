#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns every value; the panel is a view. Same architecture as
    The Mars Wars and Blade Ruiner, with one refinement: the APVTS layout,
    the engine copy in processBlock, the recipes and the randomiser all walk
    hf::paramSpec(), the one table in Engine.cpp — so the "parameter list
    and read order drift apart" class of bug has nowhere to live. */
class HairfryerAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    HairfryerAudioProcessor();
    ~HairfryerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return false; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override                { return 1; }
    int getCurrentProgram() override             { return 0; }
    void setCurrentProgram (int) override        {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    //==========================================================================
    void handleUiMessage (const juce::var& payload);   // message thread
    void timerService();                               // driven by the editor

    hf::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    // Brokild World FX - the shared rack, additive and default empty.
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };

    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override { bwfxRack.service(); }   // editor open or not
    void emitBwfx();
    std::vector<float> bwfxMonoR;                            // mono-bus scratch

    void handleOne (const juce::var& m);
    void emitInitialState();
    void notice (const juce::String& msg);
    void applyParamsStruct (const hf::Params& q);      // via setParamById, table-driven
    void randomiseAll();
    void setParamById (const juce::String& id, float value, bool fromUi = false);

    // ---- patches on disk --------------------------------------------------
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    static juce::File installedPresetFolder();
    void rememberPresetFolder (const juce::File& dir);
    juce::var presetScanDir (const juce::File& dir, int depth);
    void presetScan();
    void presetPickFolder();
    void presetSaveAs();
    void presetOpenDialog();
    void presetLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;
    juce::File presetFolder;

    // ---- parameter cache --------------------------------------------------
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;
    juce::StringArray ids;
    int statePushTick = 0;

    std::atomic<bool> wantPanic { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairfryerAudioProcessor)
};
