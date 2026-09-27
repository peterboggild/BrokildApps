// HATS OFF host harness. The bench proves the engine; this proves the WRAPPER,
// the part a DAW touches: the parameter table, MIDI timing and the GM cymbal
// map, the choke, the latency report, the host tempo reaching the echo,
// programs, state and user patches. No DAW, no audio device, no window.

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kBlock = 256;

    struct FakeHead : juce::AudioPlayHead
    {
        double bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p; p.setBpm (bpm); p.setIsPlaying (true); return p;
        }
    };

    struct Ev { int at; int note; int vel; };

    juce::AudioBuffer<float> run (HatsOffProcessor& p, int blocks, std::vector<Ev> evs)
    {
        juce::AudioBuffer<float> all (2, blocks * kBlock), blk (2, kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            for (auto& e : evs)
                if (e.at >= b * kBlock && e.at < (b + 1) * kBlock)
                    midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), e.at - b * kBlock);
            blk.clear();
            p.processBlock (blk, midi);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, b * kBlock, blk, ch, 0, kBlock);
        }
        return all;
    }
    juce::AudioBuffer<float> run (HatsOffProcessor& p, int blocks, int noteAt = -1, int note = 51, int vel = 127)
    {
        if (noteAt < 0) return run (p, blocks, std::vector<Ev> {});
        return run (p, blocks, std::vector<Ev> { { noteAt, note, vel } });
    }
    float peak (const juce::AudioBuffer<float>& b, int ch = 0, int a = 0, int n = -1)
    {
        if (n < 0) n = b.getNumSamples() - a;
        return b.getMagnitude (ch, a, n);
    }
    float rms (const juce::AudioBuffer<float>& b, int a, int n) { return b.getRMSLevel (0, a, n); }
    void setP (HatsOffProcessor& proc, const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    float diff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float m = 0; for (int i = 0; i < a.getNumSamples(); ++i) m = std::max (m, std::abs (a.getSample (0, i) - b.getSample (0, i))); return m;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("HATS OFF host harness\n\n");

    HatsOffProcessor proc;
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
        CHECK (before == ho::kNumParams, "%d host parameters, the table has %d", before, ho::kNumParams);
        std::printf ("  %d host parameters, uniquely named\n", before);
        CHECK (proc.acceptsMidi() && proc.getTotalNumInputChannels() == 0, "not an instrument that takes MIDI");
        auto* size = proc.apvts.getParameter ("size");
        CHECK (size != nullptr && size->getText (size->convertTo0to1 (20.0f), 32) == "20.0 in", "SIZE does not read in inches");
    }

    // -- silence, then a note -------------------------------------------------
    {
        const auto s = run (proc, 40);
        CHECK (peak (s) == 0.0f, "no MIDI, and yet sound (%.3g)", peak (s));

        proc.setCurrentProgram (ho::presetByName ("909 Closed Hat"));
        proc.prepareToPlay (kFs, kBlock);
        const auto a = run (proc, 100, 1000, 42);
        const float pk = peak (a);
        float lr = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) lr = std::max (lr, std::abs (a.getSample (0, i) - a.getSample (1, i)));
        std::printf ("  909 Closed Hat on MIDI note 42: peak %.3f, left/right differ by %.3g (WIDTH 0)\n", pk, lr);
        CHECK (pk > 0.2f && pk <= 1.0f, "a note plays at %.3f", pk);
        CHECK (lr == 0.0f, "at WIDTH 0 and no echo, both sides identical");

        const int lat = proc.getLatencySamples();
        int onset = -1;
        for (int i = 0; i < a.getNumSamples(); ++i) if (std::abs (a.getSample (0, i)) > 0.01f) { onset = i; break; }
        std::printf ("  latency reported %d; a note at sample 1000 is heard from sample %d\n", lat, onset);
        CHECK (lat == 23 + 8 + 72, "latency %d", lat);
        CHECK (onset >= 1000 + lat - 8 && onset <= 1000 + lat + 30, "the note lands at %d", onset);
    }

    // -- the GM cymbal map through the host --------------------------------------
    {
        proc.setCurrentProgram (ho::presetByName ("Studio Hi-Hat 14"));
        setP (proc, "keys", (float) ho::KEYS_GM);
        auto take = [&] (int note) { proc.prepareToPlay (kFs, kBlock); return run (proc, 150, 10, note); };
        const auto closed = take (42), open = take (46), pedal = take (44), ride = take (51);
        const int tail = (int) (0.4 * kFs);
        const float cT = rms (closed, tail, 4800), oT = rms (open, tail, 4800);
        std::printf ("  GM KIT: 400 ms after the hit, open (46) %.1f dB over closed (42); pedal (44) differs from closed by %.3f\n",
                     20 * std::log10 ((oT + 1e-9f) / (cT + 1e-9f)), diff (closed, pedal));
        CHECK (oT > 30.0f * (cT + 1e-9f), "note 46 is not open");
        CHECK (diff (closed, pedal) > 0.05f && diff (closed, ride) > 0.05f, "the GM notes do not change the articulation");
        setP (proc, "keys", (float) ho::KEYS_FIXED);
        const auto fixed42 = take (42), fixed51 = take (51);
        //  under FIXED every note is the cymbal as dialled; the hits differ only
        //  by the per-hit variation, so compare their level, not their samples
        const float l42 = rms (fixed42, 0, 4800), l51 = rms (fixed51, 0, 4800);
        CHECK (std::abs (20 * std::log10 (l42 / l51)) < 1.5f, "under FIXED, note 42 is not the dialled cymbal");
    }

    // -- the choke, as a DAW sends it -------------------------------------------------
    {
        proc.setCurrentProgram (ho::presetByName ("808 Open Hat"));
        proc.prepareToPlay (kFs, kBlock);
        setP (proc, "keys", (float) ho::KEYS_GM);
        const auto on = run (proc, 120, { { 10, 46, 127 }, { 9600, 42, 100 } });
        setP (proc, "choke", 0.0f);
        proc.prepareToPlay (kFs, kBlock);
        const auto off = run (proc, 120, { { 10, 46, 127 }, { 9600, 42, 100 } });
        const float a = rms (on, 14400, 4800), b = rms (off, 14400, 4800);
        std::printf ("  808 Open Hat, then a closed one 200 ms later: 300-400 ms %.1f dB with CHOKE, %.1f without\n",
                     20 * std::log10 (a + 1e-9f), 20 * std::log10 (b + 1e-9f));
        CHECK (b > 30.0f * (a + 1e-9f), "the choke does not work through the host");
    }

    // -- the host's tempo reaches the echo -------------------------------------
    {
        FakeHead head; head.bpm = 90.0;
        proc.setPlayHead (&head);
        proc.setCurrentProgram (ho::presetByName ("Skank Hat"));    // an eighth-note echo
        proc.prepareToPlay (kFs, kBlock);
        const auto a = run (proc, 200, 10, 51);
        const int lat = proc.getLatencySamples();
        const int at = 10 + lat + (int) std::lround (0.5 * 60.0 / 90.0 * kFs);     // an eighth at 90: 333 ms
        const float rep = peak (a, 0, at - 200, 2400), before = peak (a, 0, at - 7200, 2400);
        float lr = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) lr = std::max (lr, std::abs (a.getSample (0, i) - a.getSample (1, i)));
        std::printf ("  host at 90 BPM: panel reads %.1f; an eighth-note repeat at 333 ms %.3f (before it %.3f); L/R differ %.3f\n",
                     proc.hostBpm(), rep, before, lr);
        CHECK (std::abs (proc.hostBpm() - 90.0) < 1e-9, "the host tempo is not read");
        CHECK (rep > 3.0f * before && rep > 0.02f, "the echo does not land on the host's eighth");
        CHECK (lr > 0.01f, "the echo is not stereo");
        proc.setPlayHead (nullptr);
    }

    // -- programs: every factory preset loads exactly and plays bounded ------
    {
        float worst = 0; bool exact = true;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto want = ho::presetParams (i), got = proc.currentParams();
            for (int k = 0; k < ho::kNumParams; ++k)
            {
                const auto& s = ho::specs()[k];
                const float tol = 1.0e-4f * (s.hi - s.lo);
                if (std::abs (want.*(s.member) - got.*(s.member)) > tol)
                { exact = false; std::printf ("  %s: %s %g != %g\n", ho::preset (i).name, s.id, got.*(s.member), want.*(s.member)); }
            }
            proc.prepareToPlay (kFs, kBlock);
            const auto x = run (proc, 60, 10, 51);
            worst = std::max ({ worst, peak (x, 0), peak (x, 1) });
            CHECK (peak (x) > 0.05f, "%s is silent in the host", ho::preset (i).name);
        }
        std::printf ("  %d programs load exactly; loudest peak %.3f\n", proc.getNumPrograms(), worst);
        CHECK (exact, "a program does not load its own values");
        CHECK (worst <= 1.0f, "a program leaves full scale");
    }

    // -- the pad and its buttons ------------------------------------------------
    {
        proc.setCurrentProgram (0);
        for (int a = 0; a < ho::NUM_ARTICS; ++a)
        {
            proc.prepareToPlay (kFs, kBlock);
            const int before = proc.hitCount();
            proc.triggerFromUI (0.9f, a, a == 0 ? 0.8f : -1.0f);
            const auto x = run (proc, 40);
            CHECK (proc.hitCount() == before + 1 && peak (x) > 0.02f, "the panel's articulation %d does not hit", a);
        }
        std::printf ("  the pad (with a strike position) and closed, pedal, open, bell and edge all hit\n");
    }

    // -- a mono bus ---------------------------------------------------------------
    {
        HatsOffProcessor m;
        juce::AudioProcessor::BusesLayout lay;
        lay.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (m.setBusesLayout (lay), "a mono output is refused");
        m.setCurrentProgram (ho::presetByName ("Crash 18"));
        m.prepareToPlay (kFs, kBlock);
        juce::AudioBuffer<float> b (1, kBlock);
        float pk = 0;
        for (int i = 0; i < 100; ++i)
        {
            juce::MidiBuffer midi;
            if (i == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 49, (juce::uint8) 127), 0);
            b.clear(); m.processBlock (b, midi);
            pk = std::max (pk, b.getMagnitude (0, 0, kBlock));
        }
        CHECK (pk > 0.05f && pk <= 1.0f, "a mono bus plays at %.3f", pk);
    }

    // -- state and user patches ---------------------------------------------------
    {
        const int dr = ho::presetByName ("Dark Ride 22");
        proc.setCurrentProgram (dr);
        setP (proc, "decay", 4321.0f);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        HatsOffProcessor fresh;
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto a = proc.currentParams(), b = fresh.currentParams();
        bool same = true;
        for (int k = 0; k < ho::kNumParams; ++k)
            same = same && std::abs (a.*(ho::specs()[k].member) - b.*(ho::specs()[k].member)) < 1.0e-3f * std::max (1.0f, ho::specs()[k].hi);
        CHECK (same, "the state round trip changed a parameter");
        CHECK (fresh.getCurrentProgram() == dr && fresh.currentName() == "Dark Ride 22", "the preset name did not survive");

        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hats-off-test.json");
        CHECK (proc.saveUserPatch (f, "My Test Cymbal"), "saving a patch failed");
        HatsOffProcessor other;
        CHECK (other.loadUserPatch (f), "loading a patch failed");
        const auto c = other.currentParams();
        bool same2 = true;
        for (int k = 0; k < ho::kNumParams; ++k)
            same2 = same2 && std::abs (a.*(ho::specs()[k].member) - c.*(ho::specs()[k].member)) < 1.0e-3f * std::max (1.0f, ho::specs()[k].hi);
        CHECK (same2 && other.currentName() == "My Test Cymbal", "a user patch did not round-trip");
        f.deleteFile();
        std::printf ("  state and user patches round-trip; DECAY 4321 ms came back %.1f\n", c.decay);
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
