// SNARE TACTICS host harness. The bench proves the engine; this proves the
// WRAPPER, the part a DAW touches: the parameter table, MIDI timing and the GM
// kit map, the latency report, the host tempo reaching the echo, programs,
// state and user patches. No DAW, no audio device, no window.

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kBlock = 256;

    //  a host whose transport says a tempo
    struct FakeHead : juce::AudioPlayHead
    {
        double bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p; p.setBpm (bpm); p.setIsPlaying (true); return p;
        }
    };

    juce::AudioBuffer<float> run (SnareTacticsProcessor& p, int blocks, int noteAt = -1, int note = 38, int vel = 127)
    {
        juce::AudioBuffer<float> all (2, blocks * kBlock), blk (2, kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (noteAt >= b * kBlock && noteAt < (b + 1) * kBlock)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), noteAt - b * kBlock);
            blk.clear();
            p.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, b * kBlock, blk, ch, 0, kBlock);
        }
        return all;
    }
    float peak (const juce::AudioBuffer<float>& b, int ch = 0, int a = 0, int n = -1)
    {
        if (n < 0) n = b.getNumSamples() - a;
        return b.getMagnitude (ch, a, n);
    }
    void setP (SnareTacticsProcessor& proc, const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("SNARE TACTICS host harness\n\n");

    SnareTacticsProcessor proc;
    proc.setPlayConfigDetails (0, 2, kFs, kBlock);
    proc.prepareToPlay (kFs, kBlock);

    // -- the parameter table ------------------------------------------------
    {
        juce::StringArray ids;
        for (auto* p : proc.getParameters())
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p)) ids.add (wp->paramID);
        const int before = ids.size();
        ids.removeDuplicates (false);
        CHECK (ids.size() == before, "a parameter id is declared twice");
        CHECK (before == st::kNumParams, "%d host parameters, the table has %d", before, st::kNumParams);
        std::printf ("  %d host parameters, uniquely named\n", before);
        CHECK (proc.acceptsMidi() && proc.getTotalNumInputChannels() == 0, "not an instrument that takes MIDI");
        auto* gate = proc.apvts.getParameter ("gate");
        CHECK (gate != nullptr && gate->getText (0.0f, 32) == "OFF", "GATE at 0 does not say OFF");
    }

    // -- silence, then a note -------------------------------------------------
    {
        const auto s = run (proc, 40);
        CHECK (peak (s) == 0.0f, "no MIDI, and yet sound (%.3g)", peak (s));

        proc.setCurrentProgram (st::presetByName ("909 Snare"));
        proc.prepareToPlay (kFs, kBlock);
        const auto a = run (proc, 100, 1000);
        const float pk = peak (a);
        float lr = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) lr = std::max (lr, std::abs (a.getSample (0, i) - a.getSample (1, i)));
        std::printf ("  909 Snare on a MIDI note: peak %.3f, left/right differ by %.3g\n", pk, lr);
        CHECK (pk > 0.2f && pk <= 1.0f, "a note plays at %.3f", pk);
        CHECK (lr == 0.0f, "with no echo a snare is mono, both sides identical");

        const int lat = proc.getLatencySamples();
        int onset = -1;
        for (int i = 0; i < a.getNumSamples(); ++i) if (std::abs (a.getSample (0, i)) > 0.01f) { onset = i; break; }
        std::printf ("  latency reported %d; a note at sample 1000 is heard from sample %d\n", lat, onset);
        CHECK (lat == 23 + 72, "latency %d", lat);
        CHECK (onset >= 1000 + lat - 8 && onset <= 1000 + lat + 30, "the note lands at %d", onset);
    }

    // -- the GM kit map through the host --------------------------------------
    {
        proc.setCurrentProgram (st::presetByName ("Studio Snare"));
        setP (proc, "keys", (float) st::KEYS_GM);
        proc.prepareToPlay (kFs, kBlock);
        const auto snare = run (proc, 60, 10, 38);
        proc.prepareToPlay (kFs, kBlock);
        const auto clap  = run (proc, 60, 10, 39);
        proc.prepareToPlay (kFs, kBlock);
        const auto xs    = run (proc, 60, 10, 37);
        setP (proc, "keys", (float) st::KEYS_FIXED);
        proc.prepareToPlay (kFs, kBlock);
        const auto fixed = run (proc, 60, 10, 39);
        auto diff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
        { float m = 0; for (int i = 0; i < a.getNumSamples(); ++i) m = std::max (m, std::abs (a.getSample (0, i) - b.getSample (0, i))); return m; };
        std::printf ("  GM KIT: note 39 (clap) differs from 38 by %.3f, 37 (cross-stick) by %.3f; FIXED 39 = 38 to %.3g\n",
                     diff (snare, clap), diff (snare, xs), diff (snare, fixed));
        CHECK (diff (snare, clap) > 0.1f && diff (snare, xs) > 0.1f, "the GM kit notes do not change the articulation");
        CHECK (diff (snare, fixed) == 0.0f, "under FIXED, note 39 is not the snare");
    }

    // -- the host's tempo reaches the echo -------------------------------------
    {
        FakeHead head; head.bpm = 90.0;
        proc.setPlayHead (&head);
        proc.setCurrentProgram (st::presetByName ("Steppers"));   // a quarter-note echo
        proc.prepareToPlay (kFs, kBlock);
        const auto a = run (proc, 200, 10);
        //  at 90 BPM a quarter is 667 ms: the first repeat, on the left, lands there
        const int lat = proc.getLatencySamples();
        const int at = 10 + lat + (int) std::lround (60.0 / 90.0 * kFs);
        const float rep = peak (a, 0, at - 200, 2400), before = peak (a, 0, at - 9600, 2400);
        float lr = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) lr = std::max (lr, std::abs (a.getSample (0, i) - a.getSample (1, i)));
        std::printf ("  host at 90 BPM: panel reads %.1f; a quarter-note repeat at 667 ms %.3f (quiet before it %.3f); L/R differ %.3f\n",
                     proc.hostBpm(), rep, before, lr);
        CHECK (std::abs (proc.hostBpm() - 90.0) < 1e-9, "the host tempo is not read");
        CHECK (rep > 3.0f * before && rep > 0.02f, "the echo does not land on the host's quarter note");
        CHECK (lr > 0.01f, "the echo is not stereo");
        proc.setPlayHead (nullptr);
    }

    // -- programs: every factory preset loads exactly and plays bounded ------
    {
        float worst = 0; bool exact = true;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto want = st::presetParams (i), got = proc.currentParams();
            for (int k = 0; k < st::kNumParams; ++k)
            {
                const auto& s = st::specs()[k];
                const float tol = 1.0e-4f * (s.hi - s.lo);
                if (std::abs (want.*(s.member) - got.*(s.member)) > tol)
                { exact = false; std::printf ("  %s: %s %g != %g\n", st::preset (i).name, s.id, got.*(s.member), want.*(s.member)); }
            }
            proc.prepareToPlay (kFs, kBlock);
            const auto x = run (proc, 60, 10);
            worst = std::max ({ worst, peak (x, 0), peak (x, 1) });
            CHECK (peak (x) > 0.05f, "%s is silent in the host", st::preset (i).name);
        }
        std::printf ("  %d programs load exactly; loudest peak %.3f\n", proc.getNumPrograms(), worst);
        CHECK (exact, "a program does not load its own values");
        CHECK (worst <= 1.0f, "a program leaves full scale");
    }

    // -- the pad and its buttons ------------------------------------------------
    {
        proc.setCurrentProgram (0);
        for (int a = 0; a < st::NUM_ARTICS; ++a)
        {
            proc.prepareToPlay (kFs, kBlock);
            const int before = proc.hitCount();
            proc.triggerFromUI (1.0f, a);
            const auto x = run (proc, 40);
            CHECK (proc.hitCount() == before + 1 && peak (x) > 0.02f, "the panel's articulation %d does not hit", a);
        }
        std::printf ("  the pad plays the snare, the rim shot, the cross-stick and the clap\n");
    }

    // -- a mono bus ---------------------------------------------------------------
    {
        SnareTacticsProcessor m;
        juce::AudioProcessor::BusesLayout lay;
        lay.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (m.setBusesLayout (lay), "a mono output is refused");
        m.setCurrentProgram (st::presetByName ("Tubby Throw"));
        m.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> b (1, kBlock);
        float pk = 0;
        for (int i = 0; i < 100; ++i)
        {
            juce::MidiBuffer midi;
            if (i == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 127), 0);
            b.clear(); m.processBlock (b, midi);
            pk = std::max (pk, b.getMagnitude (0, 0, kBlock));
        }
        CHECK (pk > 0.05f && pk <= 1.0f, "a mono bus plays at %.3f", pk);
    }

    // -- state and user patches ---------------------------------------------------
    {
        const int gab = st::presetByName ("Gabber Snare");
        proc.setCurrentProgram (gab);
        setP (proc, "decay", 777.0f);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        SnareTacticsProcessor fresh;
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto a = proc.currentParams(), b = fresh.currentParams();
        bool same = true;
        for (int k = 0; k < st::kNumParams; ++k)
            same = same && std::abs (a.*(st::specs()[k].member) - b.*(st::specs()[k].member)) < 1.0e-3f;
        CHECK (same, "the state round trip changed a parameter");
        CHECK (fresh.getCurrentProgram() == gab && fresh.currentName() == "Gabber Snare", "the preset name did not survive");

        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("snare-tactics-test.json");
        CHECK (proc.saveUserPatch (f, "My Test Snare"), "saving a patch failed");
        SnareTacticsProcessor other;
        CHECK (other.loadUserPatch (f), "loading a patch failed");
        const auto c = other.currentParams();
        bool same2 = true;
        for (int k = 0; k < st::kNumParams; ++k)
            same2 = same2 && std::abs (a.*(st::specs()[k].member) - c.*(st::specs()[k].member)) < 1.0e-3f;
        CHECK (same2 && other.currentName() == "My Test Snare", "a user patch did not round-trip");
        f.deleteFile();
        std::printf ("  state and user patches round-trip; DECAY 777 ms came back %.1f\n", c.decay);
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
