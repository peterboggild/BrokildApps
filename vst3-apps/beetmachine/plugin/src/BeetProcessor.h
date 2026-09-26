#pragma once

// BEETMACHINE - eight slots, each an empty space or a real instance of one of
// the three Brokild drums (Kickstart, Snare Tactics, Hats Off), compiled from
// their own source. Beetmachine feeds each slot MIDI and pulls its audio the
// way a host would, so every drum sounds and edits exactly as it does alone.
//
// What Beetmachine adds on top:
//   - a note per slot (C1 row or the GM row, or learnt), layering allowed;
//   - latency alignment, so a kick and a snare played together land together;
//   - chokes: any slot can be silenced by a hit on any other (a 3 ms fade,
//     applied at the exact output sample the choking hit sounds);
//   - level / pan / mute per slot as host parameters, solo, and routing to
//     the main mix, the slot's own output pair, or both;
//   - themed kits as host programs.

#include <JuceHeader.h>
#include "BeetKits.h"
#include <bwfx.h>
#include <bwfx_juce.h>

class BeetProcessor  : public juce::AudioProcessor,
                       private juce::Timer
{
public:
    BeetProcessor();
    ~BeetProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Beetmachine"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    //  the host's programs are the themed kits
    int getNumPrograms() override { return beet::numKits(); }
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int index) override { loadKit (index); }
    const juce::String getProgramName (int index) override { return beet::kit (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    //  slots - everything below is MESSAGE THREAD unless it says otherwise
    int  slotType (int s) const               { return slots[(size_t) s].type.load(); }
    juce::AudioProcessor* slotProcessor (int s) const { return slots[(size_t) s].proc.get(); }
    void setSlotType (int s, int type);
    void setSlotPreset (int s, int program);
    int  slotPreset (int s) const;
    juce::String slotPresetName (int s) const;

    int  slotNote (int s) const               { return slots[(size_t) s].note.load(); }
    void setSlotNote (int s, int note);
    int  slotOut (int s) const                { return slots[(size_t) s].out.load(); }
    void setSlotOut (int s, int mode)         { slots[(size_t) s].out = juce::jlimit (0, 2, mode); }
    unsigned slotChokedBy (int s) const       { return slots[(size_t) s].chokedBy.load(); }
    void setSlotChokedBy (int s, unsigned m)  { slots[(size_t) s].chokedBy = m & ~(1u << s) & 0xffu; }
    bool slotSolo (int s) const               { return slots[(size_t) s].solo.load(); }
    void setSlotSolo (int s, bool on)         { slots[(size_t) s].solo = on; }
    bool slotOutputConnected (int s) const;   // is the slot's own output pair enabled by the host?
    int  slotHits (int s) const               { return slots[(size_t) s].hits.load(); }
    float slotLevel (int s) const             { return slots[(size_t) s].meter.load(); }

    void audition (int s, float velocity01)   { slots[(size_t) s].audition = juce::jlimit (1, 127, juce::roundToInt (velocity01 * 127.0f)); }
    void panic()                              { panicRequest = true; }

    //  notes
    enum { MAP_C1 = 0, MAP_GM, MAP_CUSTOM };
    //  what the notes ARE, worked out from the notes themselves - a stored flag
    //  went stale (LEARN and a project load set it without checking) and read
    //  CUSTOM over a plain C1 row
    int  noteMap() const
    {
        bool c3 = true, gm = true;
        for (int s = 0; s < beet::NUM_SLOTS; ++s)
        {
            const int n = slots[(size_t) s].note.load();
            c3 = c3 && n == beet::C1_ROW[s];
            gm = gm && n == beet::GM_ROW[s];
        }
        return c3 ? MAP_C1 : gm ? MAP_GM : MAP_CUSTOM;
    }
    void setNoteMap (int map);
    void learnNote (int s)                    { learnSlot = s; }
    int  learningSlot() const                 { return learnSlot.load(); }

    //  kits
    void loadKit (int index);

    //  the BWFX rack on the main mix. A kit leaves it alone: a kit is the
    //  drums' sounds, levels, pans and chokes, and the rack is the bus.
    bwfx::Rack& rack() { return bwfxRack; }
    int  kitIndex() const                     { return currentKit; }
    juce::StringArray missingPresets;         // names a kit asked for that a drum does not have

    //  alignment, for the bench
    int  typeLatency (int type) const         { return latency[(size_t) type]; }
    int  alignedLatency() const               { return maxLatency; }

    //  Replaced drums are parked here, never deleted while their editor may be
    //  open. The editor calls this once it has let go of any drum editor.
    void collectGarbage();

    //  bumped whenever the set of slots changes shape (type, kit, state load),
    //  so the editor knows to rebuild what it shows
    std::atomic<int> layoutVersion { 0 };

private:
    bwfx::Rack bwfxRack;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;
    std::unique_ptr<juce::AudioProcessor> makeDrum (int type) const;
    void prepareDrum (juce::AudioProcessor&) const;
    void resetSlotAudio (int s);

    struct Event { juce::int64 at; bool restore; };

    struct Slot
    {
        std::unique_ptr<juce::AudioProcessor> proc;
        std::atomic<int>      type { beet::EMPTY };
        std::atomic<int>      note { 60 };
        std::atomic<int>      out  { beet::OUT_MIX };
        std::atomic<unsigned> chokedBy { 0 };
        std::atomic<bool>     solo { false };
        std::atomic<int>      audition { 0 };
        std::atomic<int>      hits { 0 };
        std::atomic<float>    meter { 0.0f };
        std::atomic<float>*   keys = nullptr;           // the drum's own KEYS switch, read live

        //  audio thread only
        juce::AudioBuffer<float> buf;
        juce::MidiBuffer         midi;
        juce::AudioBuffer<float> ring;      // latency alignment
        int ringPos = 0, delay = 0;
        float gain = 1.0f, target = 1.0f;   // the choke envelope
        float lastL = 1.0f, lastR = 1.0f;   // level x pan, ramped per block
        std::array<Event, 64> events {};
        int numEvents = 0;
    };

    void pushEvent (Slot&, juce::int64 at, bool restore);

    std::array<Slot, beet::NUM_SLOTS> slots;
    std::array<std::atomic<float>*, beet::NUM_SLOTS> pLevel {}, pPan {}, pMute {};
    std::atomic<float>* pMaster = nullptr;

    juce::SpinLock swapLock;                  // held by the audio thread for a block,
                                              // by the message thread only to swap a pointer
    juce::CriticalSection graveLock;
    std::vector<std::unique_ptr<juce::AudioProcessor>> graveyard;

    double sr = 48000.0;
    int    blockSize = 512;
    bool   prepared = false;
    std::array<int, beet::NUM_TYPES> latency {};
    int    maxLatency = 0;
    juce::int64 clock = 0;

    int currentKit = 0;
    std::atomic<int>  noteMapMode { MAP_C1 };
    std::atomic<int>  learnSlot { -1 };
    std::atomic<bool> panicRequest { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeetProcessor)
};
