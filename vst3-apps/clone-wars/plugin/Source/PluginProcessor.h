#pragma once

#include <JuceHeader.h>
#include <map>
#include "Core/cw_core.h"
#include "bwfx.h"

//==============================================================================
class CloneWarsProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    CloneWarsProcessor();
    ~CloneWarsProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "Clone Wars"; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 6.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // UI bridge
    void handleUiMessage (const juce::var& payload);
    void timerService();                       // called by the editor's timer
    std::function<void (const juce::String&, const juce::var&)> emitToUi;

    static juce::String globalParamId (int g);
    static juce::String voiceParamId (int v, int f);

    cw::Engine engine;

    // Brokild World FX — the shared rack, one extra stage after the engine.
    // Default empty = bit-transparent (proven in cwtest). State is an opaque
    // JSON blob keyed by module/param ids, so BWFX updates need no changes here.
    bwfx::Rack bwfxRack;

private:
    // The rack's message-thread work (reverb IR builds) must run with the
    // editor closed too — a DAW project restore can enable the reverb long
    // before the window ever opens.
    void timerCallback() override { bwfxRack.service(); }
    void handleBwfxMessage (const juce::var& m);
    void sendBwfxToUi();
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void pushAllParamsToEngine();
    void applySeed (uint32_t seed);
    // 0-99 factory archive; 100-199 user slot files. False = empty slot.
    // bwfxBlob (optional out): the patch's own BWFX rack — empty string for
    // factory seeds and pre-BWFX user slots (a patch stores its own rack).
    bool resolvePatch (int n, cw::Patch& out, juce::String& catName,
                       juce::String* bwfxBlob = nullptr);
    juce::File userPatchFolder();
    void saveUserSlot (int n, const juce::String& name);
    void sendSlotListToUi();
    void applyMorph (float t, bool repaintUi);
    std::atomic<float> morphT { 0.0f };
    int morphUiTick = 0;
    void sendInitToUi();

    juce::AudioProcessorValueTreeState apvts;

    // parameterID → engine slot, resolved once at construction
    struct Slot { int global = -1; int voice = -1; int field = -1; };
    std::map<juce::String, Slot> slots;

    // the hull remembers: per-instance, travels with the DAW project
    std::atomic<int64_t>  ageSamples { 0 };      // time spent audibly playing
    // A unit leaves the factory undamaged, and the scars are the instance's
    // own - never a patch's. wearSeed is drawn per instance in the ctor.
    std::atomic<double>   wearPoints { 0.0 };
    std::atomic<uint32_t> wearSeed { 1337 };
    std::atomic<uint32_t> unitSeed { 0xC70BE5u };
    std::atomic<int>      currentSeedA { 42 }, currentSeedB { 137 };
    juce::String          currentCategory { "default" };

    std::atomic<float>* hqRaw = nullptr;
    std::atomic<bool> suppressEcho { false };
    juce::CriticalSection dirtyLock;
    juce::StringArray dirtyParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloneWarsProcessor)
};
