#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "Modulation.h"
#include "bwfx.h"
#include "proxima_site.h"

/*  The processor owns everything; the panel is a view onto the body.

    The one thing worth noticing here is that the artefact has to be REBUILT —
    the cut-and-project run again, the chain re-derived, the tuning anchor
    re-solved — whenever the fourth-dimensional depth moves. That work is
    message-thread work, and it must happen with the editor closed as well,
    because an automation lane on TRAVERSE is a perfectly good way to play
    this instrument. So a processor-side timer drives Engine::service(),
    exactly as the world rack's own service is driven. */
class ArtefactAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    ArtefactAudioProcessor();
    ~ArtefactAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 16.0; }

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
    void timerService();                               // driven by the editor, ~30 Hz

    ax::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };

    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;     // engine.service() + bwfxRack.service()

    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitBody();                   // the tiling, the cut, the star — every frame

    /*  The specimen's own modulation matrix. It sits HERE rather than in the
        engine because the engine's parameters are refreshed wholesale from the
        host every block: stored values in, effective values out, and the
        engine never learns the difference. That is also what makes a feedback
        loop unrepresentable — see Modulation.h. */
    ax::Modulator modulator;
    std::vector<float> paramStored, paramEffective;
    void refreshParams (double dtSeconds);
    int  habitIndex = 0, tempIndex = 0;
    int  lastStarGen = -1;             // the star is sent when it changes, not every frame
    void emitBwfx();
    void notice (const juce::String& msg);
    void setParamById (const juce::String& id, float value, bool fromUi = false);
    void applyHabitIndex (int index);
    void randomise();

    // ---- the page's performance events, audio-thread bound ----------------
    struct UiEvent { int kind = 0; int note = 0; float v = 0.0f; };
    static constexpr int EVQ = 256;
    std::array<UiEvent, EVQ> evq;
    std::atomic<int> evWrite { 0 }, evRead { 0 };
    void pushEvent (const UiEvent& e);

    // ---- patches on disk ---------------------------------------------------
    juce::PropertiesFile& userSettings();
    juce::File patchFolder();
    void patchScan();
    void patchSaveAs();
    void patchOpenDialog();
    void patchLoad (const juce::String& path);
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<juce::FileChooser> activeChooser;

    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;
    juce::StringArray ids;
    int statePushTick = 0;
    int bodyTick = 0;
    int lastHabitSent = -1;

    std::atomic<bool> wantPanic { false };
    std::array<std::atomic<bool>, 128> midiHeld;

    /*  THE SITE — what the four findings share (proxima_site.h). Stepped
        from the processor's own timer so it runs with the editor closed; the
        engine is handed three plain numbers, and at pull zero the rock it
        makes of them is exactly 0.0. The climate travels through TEMPERATURE,
        which is a host parameter, so a shared move is visible, automatable
        and undoable in any DAW. */
    void siteStep();
    void emitSite();
    proxima::Client site;
    double sitePhase = 0.0, siteLastMs = 0.0;
    float  siteAppliedK = -1.0f, siteLastLocalK = -1.0f;
    float  siteCoh = 0.0f, sitePull = 0.0f, siteK = 0.0f, siteLocalK = 0.0f;
    int    siteOthers = 0, siteTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtefactAudioProcessor)
};
