#pragma once

#include <JuceHeader.h>
#include "Engine.h"
#include "bwfx.h"

/*  The processor owns the instrument; the page is a view onto it.

    Everything the host can automate comes from the ONE parameter table in
    Engine.h, walked by a macro in three places that cannot disagree: the
    APVTS layout, the per-block read, and the id list the page is told about.
    A read-order bug is not expressible.

    What lives here and nowhere else: the sequencer pattern (32 steps of six
    bytes, in the STATE BLOB and never as host parameters - 192 automation
    lanes would be unusable, and FMR set that precedent), the BWFX rack blob,
    and the parameter echo so that a value the page did not move itself still
    reaches the control.
*/
class GravityWellAudioProcessor : public juce::AudioProcessor,
                                  private juce::Timer
{
public:
    GravityWellAudioProcessor();
    ~GravityWellAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    const juce::String getName() const override  { return JucePlugin_Name; }
    bool acceptsMidi() const override            { return true; }
    bool producesMidi() const override           { return false; }
    bool isMidiEffect() const override           { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override                { return gw::NUM_FACTORY; }
    int getCurrentProgram() override             { return currentProgram; }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override;
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

    gw::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    bwfx::Rack bwfxRack;

    std::atomic<bool> uiHasState { false };
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void emitInitialState();
    void emitBwfx();
    void emitPattern();
    void emitScope();
    void emitParamEcho();
    void notice (const juce::String& msg);

    void applyFactory (int i);
    juce::var patternToVar() const;
    void      patternFromVar (const juce::var& v);

    //  the house patch folder (Documents/Brokild patches/Gravity Well)
    juce::PropertiesFile& userSettings();
    juce::File presetFolderOrDefault();
    void presetScan();
    void presetSave (const juce::String& name, int slot);
    void presetLoad (const juce::String& path);
    void presetDelete (int slot);         //  moves a slot's file into the Deleted folder
    juce::String patchJson (const juce::String& name);
    void applyPatchJson (const juce::String& json, const juce::String& name);

    juce::StringArray ids;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> lastSent;

    gw::Step pattern[gw::MAX_STEPS];

    juce::String patchName { "SCHWARZSCHILD" };
    int          patchSlot = -1;          //  32..199 for a numbered user patch, else -1
    int   currentProgram = 0;
    bool  sustainOn = false;
    float wheel = 0.f, pressure = 0.f, bend = 0.f;
    std::atomic<bool> wantPanic { false };
    int   tickCount = 0;

    std::unique_ptr<juce::PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GravityWellAudioProcessor)
};
