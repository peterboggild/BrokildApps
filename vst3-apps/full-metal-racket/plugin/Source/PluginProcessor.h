#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns every value; the panel is a view. Same architecture as
    Mars Wars, Blade Ruiner, Hairfryer and Black Rider: the APVTS layout, the
    engine copy in processBlock, the kits, the randomiser and the page's
    parameter list all walk fmr::paramSpec(), the one table in Engine.cpp — so
    the read-order class of bug is not expressible.

    Triggers from the panel never touch the engine on the message thread: they
    go through a lock-free queue that processBlock drains. */
class FmrAudioProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    FmrAudioProcessor();
    ~FmrAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

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

    fmr::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    // Brokild World FX — the shared rack, one stage after the engine.
    // Additive and default empty (bit-transparent); state is an opaque blob.
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };

    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::AudioProcessor::BusesProperties makeBuses();

    void timerCallback() override { bwfxRack.service(); }   // IR builds, editor closed too

    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitBwfx();
    void emitSeq();
    void emitKit();

    // ---- the sequencer and the captured kits, as opaque state -------------
    juce::var  seqVar() const;
    void       seqApply (const juce::var& v);
    juce::var  kitsVar() const;
    void       kitsApply (const juce::var& v);

    // ---- patches on disk --------------------------------------------------
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    static juce::File installedPresetFolder();
    void rememberPresetFolder (const juce::File& dir);
    void presetScan();
    void presetSaveAs();
    void presetOpenDialog();
    void presetLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;
    juce::File presetFolder;
    int kitIndex = 0;
    juce::String patchName;              // empty unless a .fmrkit is loaded
    void notice (const juce::String& msg);
    void setParamById (const juce::String& id, float value, bool fromUi = false);
    void applyKitIndex (int i);
    void randomiseAll();

    // ---- the panel's triggers, audio-thread bound ------------------------
    struct UiEvent { int kind = 0; int chan = 0; float v = 0.0f; };  // 1 trigger, 2 all off
    static constexpr int EVQ = 256;
    std::array<UiEvent, EVQ> evq;
    std::atomic<int> evWrite { 0 }, evRead { 0 };
    void pushEvent (const UiEvent& e);

    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;
    juce::StringArray ids;
    int statePushTick = 0;
    int meterTick = 0;

    std::atomic<bool> wantPanic { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmrAudioProcessor)
};
