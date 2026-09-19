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

    /*  The six quick presets. They carry the RITE and the five global host
        parameters — not POSITION or ARRIVAL, which are the performance and
        would jump under the automation driving them, and not the BWFX rack,
        because every other synth in the fleet leaves the rack alone on a
        patch load and a preset that replaced it would be a nasty surprise. */
    /*  AUTO TRANSITION: the plugin drives POSITION itself from the host
        transport, over a window of bars you choose.

        START and END are BAR LINES within the cycle, 1-indexed the way a DAW
        counts them, so an 8-bar cycle runs from 1.0 to 9.0 and "the 8th bar"
        is 8.0 to 9.0. They snap to quarter bars.

        These are RITE STATE, not host parameters: the design keeps the
        parameter list small and stable on purpose, and this is something you
        set once per song rather than automate. */
    struct AutoCycle
    {
        bool  on    = false;
        int   bars  = 8;          // the cycle length
        float start = 8.0f;       // bar line within the cycle, 1-indexed
        float end   = 9.0f;
        bool  down  = false;      // false 0 -> 100, true 100 -> 0
        bool  arrive = false;     // fire ARRIVAL when the sweep completes
    };
    AutoCycle& autoCycle() { return autoC; }
    const AutoCycle& autoCycle() const { return autoC; }

    //  what the rack is ACTUALLY being driven with, whoever is driving it —
    //  the panel draws its marker and its lane heat from this, so the picture
    //  cannot disagree with the sound
    float effectivePosition() const
    {
        /*  With AUTO off the PARAMETER is the owner, so read it rather than a
            cache only processBlock fills — before any audio has run (a freshly
            opened editor, a stopped transport) that cache is 0 and the panel
            would draw a marker at the far left whatever the slider said. */
        if (! autoC.on)
            if (auto* p = apvts.getRawParameterValue ("position"))
                return p->load() * 0.01f;
        return effPos.load (std::memory_order_relaxed);
    }
    //  where in the cycle the transport is, 1-indexed bars; <0 = no clock
    float autoBarNow() const { return barNow.load (std::memory_order_relaxed); }

    static constexpr int kQuickPresets = 6;
    juce::File   quickPresetFile (int i) const;
    juce::String quickPresetName (int i) const;
    bool         loadQuickPreset (int i);
    void         storeQuickPreset (int i, const juce::String& name = {});
    void         ensureQuickPresets();       // writes the six factories once

    bool handleRackMessage (const juce::var& m) { return bwfx_juce::handleMessage (worldFx, apvts, m); }
    double loudness() const { return engine.lufs(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;

    rop::Rack engine;
    bwfx::Rack worldFx;

    AutoCycle autoC;
    std::atomic<float> effPos { 0.0f };
    std::atomic<float> barNow { -1.0f };
    float lastAutoBar = -1.0f;        // for the edge that fires ARRIVAL

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
