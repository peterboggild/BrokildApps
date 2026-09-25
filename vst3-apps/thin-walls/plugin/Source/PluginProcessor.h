#pragma once

#include <JuceHeader.h>
#include "TakeExport.h"
#include "Engine.h"
#include <string>
#include <vector>

//==============================================================================
/*  One row per host parameter. The APVTS layout, processBlock's read, the
    initial-state push and the panel are all walked from this ONE table (the
    Hairfryer pattern), so the read-order bug cannot be expressed. Built once at
    start-up because four sources repeat eight rows each. */
struct PSpec
{
    std::string id;
    std::string name;
    float       def;
    bool        stepped;        // a choice, not a continuous value
    int         steps;          // number of choices when stepped
    std::string choices;        // pipe separated names, or empty
};

const std::vector<PSpec>& twSpecs();

//==============================================================================
class ThinWallsAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    ThinWallsAudioProcessor();
    ~ThinWallsAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;
    void readParams();
    void emitScene();
    void emitWav();
    void applyPatchJson (const juce::var& v);
    juce::var patchJson() const;
    void loadWav (const juce::File& f);

    tw::Engine engine;
    tw::Params current;

    std::vector<std::atomic<float>*> paramPtr;
    std::vector<float> lastSent;

    // panel-only state, carried in the project but never a host parameter
    int  showRays = 1;
    int  selectedSource = 0;

    // the test signal: a file looped in place of the MAIN input
    struct Wav
    {
        juce::AudioBuffer<float> data;      // stereo, at its own rate
        double rate = 0;
        juce::String name, path;
        double seconds = 0;
    };
    juce::CriticalSection wavLock;
    std::shared_ptr<Wav> wav;
    std::atomic<bool> wavPlaying { false };
    std::atomic<float> wavGain { 0.75f };
    double wavPos = 0;
    bool wavDirty = true;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::AudioFormatManager formats;

    double hostRate = 48000.0;
    int statePushTick = 0;
    bool auxWasConnected = false;

    // the raw parameter values of this block, in table order (no allocation on the audio thread)
    std::vector<float> rawNow;

    // ---- furniture: project state, edited by the page, copied to the engine each block
    juce::SpinLock furnLock;
    tw::FurnItem furnLayout[tw::MAX_FURN];
    int furnCount = 0;
    std::atomic<int> furnVersion { 0 };
    int furnVersionAudio = -1;                 // audio thread: what it last copied
    bool furnDirtyUi = false;                  // message thread: tell the page
    juce::var furnJson() const;
    void setFurnFromVar (const juce::var& items);

    // ---- the take and its export
    juce::SpinLock recLock;                    // the audio thread only TRIES it
    std::shared_ptr<tw::TakeData> take;
    tw::TakeData* recTarget = nullptr;         // non-null while recording (guarded by recLock)
    int recFurnVersion = -1;                   // audio thread
    std::atomic<bool> recCapped { false };
    juce::String recState { "idle" }, recText, lastFile;
    double recProgress = 0;
    bool recDirty = true;
    int recTick = 0;
    void startRecording();
    void stopRecording (const juce::String& why);
    void emitRec();
    void beginExport (int w, int h, int fps);
    void sendPlan();
    void finishExport();
    void failExport (const juce::String& why);
    std::unique_ptr<juce::Thread> renderJob;
    std::unique_ptr<tw::Mp4Writer> mp4;
    juce::File mp4File;
    int vidW = 0, vidH = 0, vidFps = 30, vidN = 0, vidNext = 0;
    std::vector<int16_t> vidPcm;               // interleaved stereo at vidPcmRate
    int vidPcmRate = 48000;
    juce::int64 vidPcmWritten = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThinWallsAudioProcessor)
};
