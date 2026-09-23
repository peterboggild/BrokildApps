#pragma once

#include <JuceHeader.h>
#include "Engine.h"

/*  The processor owns every value. The panel — and the phone — are views:
    they say "move this", and are told when anything moved for another
    reason. Nothing about the patch lives in the browser, so a reopened
    editor cannot lose it and the plugin is as playable with the window shut. */
class MarsWarsAudioProcessor : public juce::AudioProcessor
{
public:
    MarsWarsAudioProcessor();
    ~MarsWarsAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return false; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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

    mw::Engine engine;
    juce::AudioProcessorValueTreeState apvts;


    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };

    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::StringArray paramIds();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    void handleOne (const juce::var& m);
    void emitInitialState();
    void notice (const juce::String& msg);
    void randomiseAll();
    /*  fromUi suppresses the echo back to the panel, because the panel is
        already showing the value it just sent. Anything else — the phone, a
        preset, the randomiser — must NOT suppress it, or the panel silently
        goes out of step with the plugin. */
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

    // ---- spectrum ---------------------------------------------------------
    static constexpr int FFT_ORDER = 10, FFT_SIZE = 1 << FFT_ORDER;
    juce::dsp::FFT fft { FFT_ORDER };
    std::array<float, FFT_SIZE * 2> fftScratch {};
    std::array<float, FFT_SIZE * 2> ring {};
    std::atomic<int> ringWrite { 0 };
    std::array<float, mw::SCOPE_BINS> spectrum {};
    std::array<float, FFT_SIZE> window {};

    std::atomic<bool> wantPanic { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsWarsAudioProcessor)
};
