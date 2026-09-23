#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"
#include "proxima_site.h"

/*  ARTEFACT B2311.22 — the host side. Native-first, house rules: the APVTS
    is the single source of truth (built from the SPECS table so a read-order
    bug is inexpressible), the page is a view, and everything human about the
    product — parameter names, patches, the DAW itself — is the HARNESS the
    expedition bolted onto the object.  */
class ArtefactAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    ArtefactAudioProcessor();
    ~ArtefactAudioProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Artefact B2311.22"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void handleUiMessage (const juce::var&);   // message thread
    void timerService();                       // driven by the editor timer

    ab::Engine engine;
    float lumPeak = 1e-6f;      // adaptive headroom for the glow stream
    float lumPeak2 = 1e-6f;     // ...and for the second body's
    int   otherLoadedUi = -1;
    int   accentSeen = -1;
    void  emitOther();
    juce::String accentToString (const float* a, int n) const;
    void  accentFromState (const juce::XmlElement& xml);
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::atomic<bool> uiReady    { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override { bwfxRack.service(); surveyStep(); siteStep(); }

    /*  THE SITE — what the four findings share (proxima_site.h). Stepped
        from the processor's own timer so it runs with the editor closed;
        the engine is handed three plain numbers, at pull zero a no-op. */
    void siteStep();
    void emitSite();
    proxima::Client site;
    int    tempIdx = -1;
    double sitePhase = 0.0, siteLastMs = 0.0;
    float  siteAppliedK = -1.0f, siteLastLocalK = -1.0f;
    float  siteCoh = 0.0f, sitePull = 0.0f, siteK = 0.0f, siteLocalK = 0.0f;
    int    siteOthers = 0, siteTick = 0;
    void handleOne (const juce::var& m);
    void emitInitialState();
    void emitSpecimen();
    void emitBwfx();
    void notice (const juce::String&);
    void setParamById (const juce::String& id, float v);

    //  the survey: the field's 300 positions computed a few per tick in the
    //  background, so the field fills in as the expedition scans
    void surveyStep();
    int surveyAt = 0;
    std::array<float, ab::kCatalog> fieldX {}, fieldY {};
    std::array<uint8_t, ab::kCatalog> fieldFam {};
    bool surveyDone = false, surveySent = false;

    //  patches (the house folder; app tag "artefact-b2311")
    juce::File presetFolder;
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);
    void presetSaveAs();
    void presetOpenDialog();
    std::unique_ptr<juce::FileChooser> activeChooser;

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;
    std::atomic<bool> wantPanic { false };

    int  specLoadedUi = -1;
    int  secDiv = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtefactAudioProcessor)
};
