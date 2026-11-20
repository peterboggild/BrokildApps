#pragma once

// DRUMEQ - the wrapper: host parameters (generated from de_params.h), mono or
// stereo in place, state, presets per drum, and THE KIT - every instance in
// the session publishing its drum into one shared block (KitLink.h), so a
// whole kit can be saved to one file and loaded back onto every track at once.
//
// The kit runs on a processor-side timer (message thread), so it works with
// every editor closed - which is how a session is usually left.

#include <JuceHeader.h>

#include "../engine/de_engine.h"
#include "../engine/de_sources.h"
#include "../engine/de_find.h"
#include "KitLink.h"

//  what FIND DRUM did: the pitch (drums with one), and the levels (every drum)
struct FindReport
{
    double hz = 0.0;            // the fundamental, 0 = none (or a drum with no pitch)
    int hits = 0;               // the drum's own strokes heard
    int spill = 0;              // other events heard
    bool levelSet = false;      // the input trim moved
    float inDelta = 0.0f;       // by this much (dB)
    bool clamped = false;       // and the trim hit the end of its range
    float outDelta = 0.0f;      // OUT took it back (KEEP LEVEL)
    bool gateSet = false;
    float gthr = 0.0f;
    bool spillHeard = false;
    bool heard() const { return hits > 0; }
};

class DrumEQProcessor  : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    DrumEQProcessor();
    ~DrumEQProcessor() override;

    //  the stand-in that holds a drum the MASTER panel is editing on another
    //  track: parameters and presets only - no kit, no timer, no audio
    struct ProxyTag {};
    explicit DrumEQProcessor (ProxyTag);

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    de::Engine& getEngine() { return engine; }

    //  ---- the controls --------------------------------------------------------
    de::Params currentParams() const;
    void applyParams (const de::Params&);                 // message thread; notifies the host
    int  currentSource() const;
    void selectSource (int s);                             // the source switch on the panel

    //  ---- presets for this drum ------------------------------------------------
    void loadFactoryPreset (int i);
    juce::String currentName() const { return presetName; }
    juce::File presetFolder() const;                        // ...\DrumEQ\<DRUM>
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::File&, const juce::String& name);
    bool loadUserPreset (const juce::File&);

    //  ---- THE KIT ---------------------------------------------------------------
    bool linked() const { return link; }
    void setLinked (bool on) { link = on; if (! on) setMaster (false); }
    juce::String kitName() const { return kitLabel; }
    void kitPresence (int counts[de::kNumSources]) const { kit.presence (counts); }
    bool kitOpen() const { return kit.isOpen(); }
    juce::File kitFolder() const;                           // ...\DrumEQ\Kits
    juce::Array<juce::File> userKits() const;

    //  every drum the session has, to a file; `report` names them
    bool saveKit (const juce::File&, const juce::String& name, juce::String& report);
    //  onto every linked instance (and this one, linked or not)
    bool loadKitFile (const juce::File&, juce::String& report);
    void loadFactoryKit (int i, juce::String& report);

    //  one kit tick: presence, answering a kit load, publishing a change.
    //  Called by the timer; public so the host harness can drive it.
    void kitTick();

    //  ---- MASTER: one panel tunes the whole kit ----------------------------------
    //  Any LINKED track can be the master. Its own audio stays its own drum;
    //  its panel edits any drum - the drum's slot in the kit - and every linked
    //  track on that drum takes the change. One master per session.
    bool isMaster() const { return masterOn; }
    bool setMaster (bool on);                              // false if refused (not linked, no kit)
    int  target() const { return editTarget >= 0 ? editTarget : currentSource(); }
    void setTarget (int s);                                // the master's source row
    //  the processor the panel shows: this one, or the stand-in for the drum
    //  being edited elsewhere
    DrumEQProcessor& view() { return (masterOn && editTarget >= 0 && proxy != nullptr) ? *proxy : *this; }
    bool editingRemote() const { return masterOn && editTarget >= 0; }
    //  another track's panel is the master: its own drum and the drum it edits
    bool masterElsewhere (int& masterSource, int& masterTarget) const;
    //  what the track(s) on the drum being edited hear, while the master is aimed there
    bool remoteView (float* pre, float* post, float meters[4], float& rate) const
        { return kit.readView (target(), pre, post, meters, rate); }

    //  ---- FIND DRUM ---------------------------------------------------------------
    void startFind() { engine.armFind (true); findStartMs = juce::Time::getMillisecondCounter(); }
    bool finding() const { return engine.findArmed(); }
    double findSeconds() const { return engine.findLength() / juce::jmax (1.0, engine.sampleRate()); }
    //  stops listening, analyses, moves this drum's body band onto the
    //  fundamental (drums with a pitch), sets the INPUT so the hits arrive at
    //  the level the presets are written for, and the gate threshold between
    //  the spill and the quietest hit
    FindReport finishFind();
    static constexpr float kTargetPeakDb = -12.0f;
    //  KEEP LEVEL: OUT gives back what FIND added at the input, so the
    //  track's fader balance does not move
    bool keepLevel() const { return keepLvl; }
    void setKeepLevel (bool on) { keepLvl = on; }
    //  FIND on the drum the master is editing: a track on it listens
    void requestRemoteFind (bool keep) { kit.requestFind (target(), keep); }
    bool remoteFindActive() const { return remoteFind; }      // the master asked THIS track to listen
    int  remoteFindState (float& sec, float& hz, int& hits, float& inDelta) const
        { return kit.findState (target(), sec, hz, hits, inDelta); }

    static juce::String idOf (int i) { return de::specs()[i].id; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override { kitTick(); }
    void applyKitValues (const float* v, int n, const juce::String& name);
    void syncProxy();
    void loadProxyFromSlot();
    void showLiveView (int s);
    void writeKitFromParams (const std::vector<std::pair<int, de::Params>>& drums,
                             const std::vector<juce::String>& names, const juce::String& kitTitle);

    de::Engine engine;
    std::array<std::atomic<float>*, de::kNumParams> raw {};

    drumkit::Link kit;
    bool link = true;
    juce::String presetName { "STANDARD" };
    juce::String kitLabel;
    int lastSource = -1;
    bool forceKitApply = false;
    std::array<float, de::kKitValues> lastPublished {};
    juce::String lastPublishedName;
    bool publishedOnce = false;
    juce::uint32 findStartMs = 0;
    bool keepLvl = false;

    //  MASTER
    bool proxyMode = false;
    bool masterOn = false, masterPending = false;
    int editTarget = -1;
    std::unique_ptr<DrumEQProcessor> proxy;
    std::array<float, de::kKitValues> proxySent {};
    juce::String proxySentName;
    uint32_t proxySeq = 0;
    uint32_t seenEditSeq = 0;
    bool remoteFind = false, remoteKeep = false;
    std::vector<float> specScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEQProcessor)
};
