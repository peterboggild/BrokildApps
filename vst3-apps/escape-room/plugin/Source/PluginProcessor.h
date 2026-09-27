#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns every value. The page is a view: it sends "move this
    parameter" and is told when anything moved for another reason. Nothing
    about the patch lives in the browser, so a reopened editor cannot lose it
    and the plugin is exactly as playable with the window shut. */
class EscapeRoomAudioProcessor : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    EscapeRoomAudioProcessor();
    ~EscapeRoomAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override { return JucePlugin_Name; }
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

    er::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    // Brokild World FX - the shared rack, additive and default empty.
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };            // page acknowledged initialState
    std::atomic<bool> uiReady    { false };            // page says it is painted

    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::StringArray paramIds();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override { bwfxRack.service(); }   // editor open or not
    void emitBwfx();

    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitWiring();
    juce::var wiringVar() const;
    void notice (const juce::String& msg);        // one line for the ORAKEL

    // ---- patches on disk: ENCODE writes one, DECODE reads one -------------
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    static juce::File installedPresetFolder();    // "User presets" beside the plugin
    void rememberPresetFolder (const juce::File& dir);
    juce::var presetScanDir (const juce::File& dir, int depth);
    void presetScan();
    void presetPickFolder();
    void presetSaveAs();                          // ENCODE
    void presetOpenDialog();                      // DECODE, from anywhere
    void presetLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;
    juce::File presetFolder;

    std::vector<std::atomic<float>*> raw;               // one per id, in paramIds() order
    std::vector<float> lastSent;
    juce::StringArray ids;

    int  statePushTick = 0;
    int  lastSigilSent = -1;
    int  meterTick = 0;

    // notes played on the page (message thread) -> the engine (audio thread)
    struct UiNote { int note = 0; float vel = 0; bool on = false; };
    juce::AbstractFifo noteFifo { 256 };
    std::array<UiNote, 256> noteStore {};

    std::atomic<bool> panicFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EscapeRoomAudioProcessor)
};
