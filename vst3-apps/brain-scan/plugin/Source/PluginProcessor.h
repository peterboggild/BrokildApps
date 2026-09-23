#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns the instrument; the page is a view onto it.

    What lives here and nowhere else: the canonical scan lines (the page edits
    them on the tomography plane, the native side keeps the truth and writes
    the project state and the patch files), the specimen the engine has
    built (sent to the page as bytes for its own rendering), the parameter
    echo (a value the page did not move itself is sent back so the control
    follows), the view the monitor draws from, and the house patch-file
    vocabulary. Chassis: High Tide's, adapted.
*/
class BrainScanAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    BrainScanAudioProcessor();
    ~BrainScanAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

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
    void handleUiMessage (const juce::var& payload);
    void timerCallback() override;
    void setParamById (const juce::String& id, float value, bool fromUi);

    bs::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void emitInitialState();
    void emitVolume();
    void emitLines();
    void emitView();
    void emitBwfx();
    void emitPatch();
    void emitParamEcho();
    void notice (const juce::String& msg);

    juce::var  linesToVar() const;
    bool       linesFromVar (const juce::var& v);
    void       applyFactory (int i);
    void       applyParams (const bs::Params& p);

    //  the house patch files (Documents/Brokild patches/Brain Scan)
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    void rememberPresetFolder (const juce::File& dir);
    juce::var presetScanDir (const juce::File& dir, int depth);
    void presetScan();
    void presetPickFolder();
    void presetSaveAs();
    void presetOpenDialog();
    void presetLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);
    void migrateSpecimen (const juce::String& writtenBy);

    //  the imported volume, if any
    void       doImport (const juce::File& f);
    void       reImport();
    void       emitImport();
    juce::String importCubeBase64() const;
    bool       importCubeFromBase64 (const juce::String& b64);

    std::vector<float> importCube;          // VN^3 in [0,1], empty when none
    juce::String importPath, importNote, importError;
    int   importAxis = 0;
    bool  importStretch = false;
    float importWindowLo = 0, importWindowHi = 0;
    int   importFilled[3] { 0, 0, 0 };
    float importSpacing[3] { 0, 0, 0 };

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;

    bs::Line lines[bs::NLINES];       // the canonical copy (the engine holds its own)
    bs::Line loadedLines[bs::NLINES]; // the lines the current patch / project / factory study came with
    void rememberLoadedLines();
    void clearImport (const juce::String& why);
    juce::String patchName { "ADMISSION" };
    bool patchIsUser = false;
    bool sustainOn = false;
    float wheel = 0, pressure = 0;
    std::atomic<bool> wantPanic { false };
    int tickCount = 0;
    int specimenSent = -1;

    std::unique_ptr<juce::PropertiesFile> settings;
    juce::File presetFolder;
    std::unique_ptr<juce::FileChooser> activeChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainScanAudioProcessor)
};
