#pragma once

// RITE OF PASSAGE — the wrapper.
//
// The engine lives in engine/ and knows nothing about JUCE. This file is
// host parameters, buses, state and the BWFX rack.
//
// THE AUTOMATABLE SURFACE IS SMALL AND FIXED (§10). One slider is the whole
// transition; ARRIVAL is its own trigger so that scrubbing cannot fire a
// drop; and the handful of globals below are the only other lanes a DAW ever
// sees. Six slot assignments, twelve settings each, the score, the places and
// the tail modes all live in ONE opaque blob inside plugin state — so
// re-assigning a slot cannot orphan an automation lane, because there was
// never a lane bound to it.

#include <JuceHeader.h>

#include "../engine/rop_rack.h"
#include <bwfx.h>
#include <bwfx_juce.h>

namespace rop_ids
{
    static constexpr const char* position = "position";
    static constexpr const char* arrival  = "arrival";
    static constexpr const char* mix      = "mix";
    static constexpr const char* output   = "output";
    static constexpr const char* spread   = "spread";
    static constexpr const char* turn     = "turn";
    static constexpr const char* monogate = "monogate";
}

class RiteProcessor  : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    RiteProcessor();
    ~RiteProcessor() override;

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "RITE"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    rop::Rack& rack() { return engine; }
    bwfx::Rack& bwfxRack() { return worldFx; }

    // the rite — everything the score is, as one string (§10)
    juce::String riteToJson() const;
    void riteFromJson (const juce::String&);

    bool handleRackMessage (const juce::var& m) { return bwfx_juce::handleMessage (worldFx, apvts, m); }
    double loudness() const { return engine.lufs(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;

    rop::Rack engine;
    bwfx::Rack worldFx;

    std::atomic<float>* pPos = nullptr;
    std::atomic<float>* pArrival = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;
    std::atomic<float>* pSpread = nullptr;
    std::atomic<float>* pTurn = nullptr;
    std::atomic<float>* pGate = nullptr;

    bool lastArrival = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiteProcessor)
};
