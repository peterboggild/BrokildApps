#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns every value; the panel is a view. Same architecture as
    The Mars Wars, Blade Ruiner and Hairfryer: the APVTS layout, the engine
    copy in processBlock, the recipes, the randomiser and the page's
    parameter list all walk bk::paramSpec(), the one table in Engine.cpp.

    MIDI from the host and notes/wheels from the on-screen keyboard both end
    up in the engine, but never from the message thread: the page's events go
    through a lock-free queue that processBlock drains. */
class BlackRiderAudioProcessor : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    BlackRiderAudioProcessor();
    ~BlackRiderAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

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

    bk::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    // Brokild World FX — the shared rack, one extra stage after the engine.
    // Additive and default empty (bit-transparent); state is an opaque blob.
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };

    std::function<void (const juce::String&, const juce::var&)> emitToUi;
    std::function<void (bool)> onWide;          // the page opened/closed the patch-bay wing
    std::atomic<bool> uiWide { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // The rack's message-thread work (reverb IR builds) must run with the
    // editor closed too — a project restore can enable the reverb first.
    void timerCallback() override { bwfxRack.service(); }
    void emitBwfx();

    void handleOne (const juce::var& m);
    void emitInitialState();
    void notice (const juce::String& msg);
    void applyParamsStruct (const bk::Params& q);
    void applySeed (int seed);
    void emitSeedInfo (int seed);
    static int seedCatOf (int seed);
    void randomiseAll();
    void setParamById (const juce::String& id, float value, bool fromUi = false);
    int lastSeedSent = -1;

    // ---- the page's performance events, audio-thread bound ---------------
    struct UiEvent { int kind = 0; int note = 0; float v = 0.0f; };   // kind: 1 note on, 2 note off, 3 bend, 4 wheel, 5 all off
    static constexpr int EVQ = 256;
    std::array<UiEvent, EVQ> evq;
    std::atomic<int> evWrite { 0 }, evRead { 0 };
    void pushEvent (const UiEvent& e);

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
    std::atomic<float> hostBend { 0.0f }, hostWheel { 0.0f }, hostAt { 0.0f };   // for the panel's wheels
    std::array<std::atomic<bool>, 128> midiHeld;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackRiderAudioProcessor)
};
