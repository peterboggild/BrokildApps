#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

class PhotoSynthAudioProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::Timer
{
public:
    PhotoSynthAudioProcessor();
    ~PhotoSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    //==========================================================================
    // Bridge (message thread)
    void handleUiMessage (const juce::var& payload);

    // Called by the editor's timer: clock, MIDI relay, recorder status, garbage.
    void timerService();

    ps::Engine engine;
    juce::AudioProcessorValueTreeState apvts;

    // Brokild World FX - the shared rack, additive and default empty.
    bwfx::Rack bwfxRack;

    // Cleared when an editor attaches: until the page says it has the state,
    // timerService keeps re-emitting it so the restore cannot be missed.
    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady { false };       // page reports the patch is on screen
    std::atomic<float> hostBpm { 120.0f };     // tempo from the host, for the synced effects

    // set by the active editor so the processor can push events to the page
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override { bwfxRack.service(); }   // editor open or not
    void emitBwfx();

    void handleOne (const juce::var& m);
    void handleVoiceParams (int voice, const juce::var& fields);
    void handleVoiceEvents (int voice, const juce::var& evs);
    void applyControlNative (const juce::String& id, float v);
    void emitInitialState();                   // hand the stored UI state to the page
    // ---- file-based presets ----
    void presetPickFolder();                   // ask for a folder, remember it
    void presetScan();                         // send the folder tree to the page
    void presetLoad (const juce::String& path);
    void presetSave (const juce::String& sub, const juce::String& name, const juce::String& json);
    void presetSaveAs (const juce::String& name, const juce::String& json);  // native Save dialog
    void presetOpenDialog();                   // native Open dialog, anywhere on disk
    void rememberPresetFolder (const juce::File& dir);
    juce::var presetScanDir (const juce::File& dir, int depth);
    juce::File presetFolder;
    juce::File presetFolderOrDefault();
    static juce::File installedPresetFolder(); // "User presets" beside the plugin, if writable
    juce::PropertiesFile& userSettings();
    std::unique_ptr<juce::PropertiesFile> settings;   // remembers the folder between sessions

    void syncHostTune();                       // Tune + Fine -> the engine (host MIDI notes)
    void startRecording (bool on);
    void saveRecording();
    void startRender (const juce::var& payload);
    void saveTextFile (const juce::String& name, const juce::String& text);
    void applyRenderSetupTo (ps::Engine& e, const juce::var& payload);

    void collectGarbage();

    // ---- recorder ----
    std::vector<float> recL, recR;
    std::atomic<bool> recOn { false };
    std::atomic<int64_t> recWrite { 0 };
    int64_t recCapacity = 0;
    double recSampleRate = 48000;

    // ---- MIDI -> UI relay (audio thread writes, message thread reads) ----
    struct MidiUiEvent { int on = 0; int note = 0; };
    juce::AbstractFifo midiUiFifo { 256 };
    std::array<MidiUiEvent, 256> midiUiStorage {};

    // ---- host automation relay ----
    int statePushTick = 0;                     // paces the initialState re-send
    juce::CriticalSection dirtyLock;
    juce::StringArray dirtyParams;
    std::atomic<bool> suppressEcho { false };

    // ---- native mirrors for editor-closed automation ----
    struct Mirror
    {
        float delayTimeSec = 0.26f, delayOffsetSec = 0, delayFeedback = 0.34f;
        bool tape = false, freeze = false, darkOn = false;
    } mirror;

    // ---- state ----
    juce::String uiStateJson;                  // full preset JSON from the UI (incl. photos)
    juce::CriticalSection stateLock;

    // ---- event batch garbage ----
    juce::Array<ps::EventBatch*> ownedBatches;

    // ---- offline render / file dialogs ----
    std::unique_ptr<juce::ThreadPool> workPool;
    std::unique_ptr<juce::FileChooser> activeChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioProcessor)
};
