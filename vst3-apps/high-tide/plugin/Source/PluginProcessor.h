#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns the instrument; the page is a view onto it.

    What lives here and nowhere else: the canonical terrain and lanes (the
    page sculpts and pins, the native side keeps the truth and writes the
    project state and the patch files), the parameter echo (a value the page
    did not move itself is sent back so the control follows), the balls
    stream the panel draws from, and the house patch-file vocabulary.
*/
class HighTideAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    HighTideAudioProcessor();
    ~HighTideAudioProcessor() override;

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

    ht::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void emitInitialState();
    void emitTerrain();
    void emitLanes();
    void emitBalls();
    void emitBwfx();
    void emitPatch();
    void emitParamEcho();
    void notice (const juce::String& msg);

    //  terrain + lanes as text, both ways
    juce::String terrainPngBase64() const;
    bool  applyTerrainPngBase64 (const juce::String& b64);
    juce::var lanesToVar (const ht::Lanes& l) const;
    ht::Lanes lanesFromVar (const juce::var& v) const;
    void  applyFactory (int i);
    void  applyParams (const ht::Params& p);

    //  the house patch files (Documents/Brokild patches/High Tide)
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
    void pngExport();
    void pngImport();

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;

    ht::Lanes lanes;                 // the canonical copy (the engine holds its own)
    juce::String patchName { "FIRST LIGHT" };
    bool patchIsUser = false;
    bool sustainOn = false;
    float wheel = 0, pressure = 0;
    std::atomic<bool> wantPanic { false };
    int tickCount = 0;

    std::unique_ptr<juce::PropertiesFile> settings;
    juce::File presetFolder;
    std::unique_ptr<juce::FileChooser> activeChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HighTideAudioProcessor)
};
