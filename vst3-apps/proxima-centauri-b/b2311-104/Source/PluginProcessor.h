#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"
#include "proxima_site.h"

/*  The processor owns the object; the panel is a view onto it.

    Two things worth noticing.

    The web must keep carrying with the editor closed. AMBIENT is a host
    parameter and warming the grid over a whole piece is the obvious gesture,
    so nothing acoustic can live in the editor's frame callback.

    And a thermal order is a NOTE: the object heats a conduit until its
    resonance is the note. So MIDI is not decoration — it is how you command a
    temperature, which is how you command a pitch. Sustain holds the valve;
    the wheel bends by scaling the ordered pitch, which the servo then chases.
*/
class Artefact104AudioProcessor : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    Artefact104AudioProcessor();
    ~Artefact104AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

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

    ab104::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void emitInitialState();
    void emitWeb();
    void emitBwfx();
    void notice (const juce::String& msg);

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;

    int  lastGeomStamp = -1;
    int  webTick = 0;
    bool sustainOn = false;
    std::atomic<bool> wantPanic { false };

    /*  THE SITE — what the four findings share (proxima_site.h). Stepped
        from the timer, never the audio thread; the engine is handed three
        plain numbers. With sharing off the pull is exactly zero and the
        engine's site path is never entered. */
    void siteStep();
    void emitSite();
    proxima::Client site;
    int    ambientIdx = -1;
    double sitePhase = 0.0, siteLastMs = 0.0;
    float  siteAppliedK = -1.0f, siteLastLocalK = -1.0f;
    float  siteCoh = 0.0f, sitePull = 0.0f, siteK = 0.0f, siteLocalK = 0.0f;
    int    siteOthers = 0, siteTick = 0;

    /*  A host-parameter change the page did not make itself — automation, a
        patch, THE SITE — is echoed back to it, or the slider stays where it
        was while the sound moves and the whole thing reads as broken. */
    void emitParamEcho();
    std::vector<float> lastSent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Artefact104AudioProcessor)
};
