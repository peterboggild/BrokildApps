#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"
#include "proxima_site.h"

/*  The processor owns the object; the panel is a view onto it.

    Two things are worth noticing here.

    The lattice must keep counting with the editor closed. TEMPERATURE is a
    host parameter and an automation lane on it is a perfectly good way to play
    this — warming the thing over a whole piece is the obvious gesture — so the
    counting cannot live in the editor's frame callback.

    And the host's transport is not a nicety, it is the thing the object leans
    towards. The playhead is read on every block and handed to the engine as
    the imposed pulse; when the transport is stopped the object keeps its own
    time from FREE RATE, because something that goes silent when you press stop
    is a plug-in and not an artefact.
*/
class Artefact1AudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    Artefact1AudioProcessor();
    ~Artefact1AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
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
    void handleUiMessage (const juce::var& payload);
    void timerCallback() override;

    void setParamById (const juce::String& id, float value, bool fromUi);
    void dialSpecimen (int index);

    ab1::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void emitInitialState();
    void emitBody();
    void emitBwfx();
    void notice (const juce::String& msg);

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;

    int  specimen = 0;
    uint32_t lastRateHash = 0;
    int      rateTick = 0;
    bool midiHeld[128] {};
    std::atomic<bool> wantPanic { false };

    //  the panel pokes the lattice; the audio thread must not be interrupted,
    //  so pokes are queued and applied between blocks
    struct Poke { int x, y, radius; float amount; };
    std::vector<Poke> pokeQueue;
    juce::CriticalSection pokeLock;

    /*  THE SITE — what the four findings share (proxima_site.h). Stepped
        from the timer; the engine gets a second imposed pulse, at pull zero
        an exact no-op. */
    void siteStep();
    void emitSite();
    proxima::Client site;
    int    tempIdx = -1;
    double sitePhase = 0.0, siteLastMs = 0.0;
    float  siteAppliedK = -1.0f, siteLastLocalK = -1.0f;
    float  siteCoh = 0.0f, sitePull = 0.0f, siteK = 0.0f, siteLocalK = 0.0f;
    int    siteOthers = 0, siteTick = 0;

    /*  A host-parameter change the page did not make itself — automation, a
        dialled specimen, THE SITE — is echoed back to it, or the slider stays
        where it was while the sound moves and the whole thing reads as broken. */
    void emitParamEcho();
    std::vector<float> lastSent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Artefact1AudioProcessor)
};
