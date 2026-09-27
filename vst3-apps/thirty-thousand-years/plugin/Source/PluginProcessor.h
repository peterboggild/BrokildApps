#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "Presets.h"
#include "bwfx.h"

/*  The processor owns every value; the panel is a view (PROTOCOL.md). The
    APVTS layout, the engine copy in processBlock, the presets, the scenes,
    the matrix's destinations and the page's parameter list all walk
    tty::paramSpec(), the one table in Params.cpp.

    MIDI from the host and notes from the on-screen keyboard both end up in
    the engine, but never from the message thread: the page's events go
    through a lock-free queue that processBlock drains. Scenes, macro maps,
    matrix slots and shapes are written from the message thread as plain
    fields (a torn read costs one control tick at most); clips (captures,
    imports) are handed over by atomic pointer swap and kept alive here. */
class TTYAudioProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    TTYAudioProcessor();
    ~TTYAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 60.0; }

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

    // ---- the page
    void handleUiMessage (const juce::var& payload);
    void timerService();                                  // called by the editor at 30 Hz
    std::function<void (const juce::String&, const juce::var&)> emitToUi;
    std::atomic<bool> uiHasState { false }, uiReady { false };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;                        // 15 Hz, editor or not

    // messages
    void handleOne (const juce::var& m);
    void setParamById (const juce::String& id, float value, bool fromUi = false);
    void applyParamsStruct (const tty::Params& q, bool musicalOnly);
    void applyPatchIndex (int i);
    void applyPatchJson (const juce::String& json, const juce::String& name);
    juce::String patchJson (const juce::String& name);
    void mutate (float amount, int lock);
    void snapshotForUndo();
    void undo();
    void abStore (int slot); void abRecall (int slot);
    void sceneOp (const juce::String& op, int i, const juce::String& name);
    void importClip (const juce::File& f);
    void importDialog();
    void scalaFile (const juce::File& f);
    void scalaDialog();
    void snapshotCapture();
    void notice (const juce::String& msg);

    // emits
    void emitInitialState();
    void emitPatchInfo();
    void emitBwfx();
    void emitMacros(); void emitSlots(); void emitMseg(); void emitScenes();
    void emitParamEcho();
    void emitEff();
    void emitMeter();

    // serialisation of the blobs
    juce::var macrosVar(); void macrosFromVar (const juce::var&);
    juce::var slotsVar();  void slotsFromVar (const juce::var&);
    juce::var msegVar();   void msegFromVar (const juce::var&);
    juce::var scenesVar(); void scenesFromVar (const juce::var&);
    juce::var scalaVar();  void scalaFromVar (const juce::var&);
    juce::String captureBase64(); void captureFromBase64 (const juce::String&);

    // presets on disk
    juce::PropertiesFile& userSettings();
    juce::File installedPresetFolder();
    juce::File presetFolderOrDefault();
    void rememberPresetFolder (const juce::File& dir);
    juce::var presetScanDir (const juce::File& dir, int depth);
    void presetScan();
    void presetPickFolder();
    void presetSaveAs();
    void presetOpenDialog();
    void presetLoad (const juce::String& path);

    tty::Engine engine;
    bwfx::Rack bwfxRack;
    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;
    juce::File presetFolder;
    juce::AudioFormatManager formats;

    struct UiEvent { int kind = 0; int note = 0; float v = 0.0f; };
    static constexpr int EVQ = 256;
    UiEvent evq[EVQ]; std::atomic<int> evRead { 0 }, evWrite { 0 };
    void pushEvent (const UiEvent& e);

    int statePushTick = 0;
    juce::String loadedName;
    int lastPatchSent = -2;
    juce::String sceneNames[tty::NUM_SCENES] = { "AWAKENING", "OCCUPATION", "COLLAPSE", "AFTERMATH" };

    // undo / A-B
    struct Snapshot { tty::Params p; std::string rack; tty::Params scene[tty::NUM_SCENES]; bool sceneSet[tty::NUM_SCENES]; tty::MacroDest macro[tty::NUM_MACROS][tty::MACRO_DESTS]; tty::Slot slots[tty::NUM_SLOTS]; tty::Mseg mseg[4]; bool valid = false; };
    Snapshot undoSnap, ab[2];
    void takeSnapshot (Snapshot& s); void applySnapshot (const Snapshot& s);

    // clips
    std::vector<std::unique_ptr<tty::Clip>> clips;        // kept alive; the engine reads through pointers
    bool captureWasOn = false, rememberCapture = false;
    juce::String importPath, importName;

    // scala
    std::vector<float> scalaCents; float scalaPeriod = 1200.0f; juce::String scalaName;

    std::atomic<bool> wantPanic { false };
    std::atomic<float> hostBend { 0.0f }, hostWheel { 0.0f }, hostAt { 0.0f };
    std::array<std::atomic<bool>, 128> midiHeld;
    juce::AudioBuffer<float> auxCopy;
    bool fractureSeen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TTYAudioProcessor)
};
