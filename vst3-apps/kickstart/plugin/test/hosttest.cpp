// KICKSTART host harness. The bench proves the engine; this proves the WRAPPER,
// the part a DAW touches: the parameter table, MIDI timing, the latency report,
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

    //  render `blocks` blocks; `noteAt` < 0 means no note
    juce::AudioBuffer<float> run (KickstartProcessor& p, int blocks, int noteAt = -1, int note = 36, int vel = 127)
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
    float peak (const juce::AudioBuffer<float>& b, int ch = 0) { return b.getMagnitude (ch, 0, b.getNumSamples()); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("KICKSTART host harness\n\n");

    KickstartProcessor proc;
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
        CHECK (before == ks::kNumParams, "%d host parameters, the table has %d", before, ks::kNumParams);
        std::printf ("  %d host parameters, uniquely named\n", before);
        CHECK (proc.acceptsMidi() && proc.getTotalNumInputChannels() == 0, "not an instrument that takes MIDI");
    }

    // -- silence, then a note -------------------------------------------------
    {
        const auto s = run (proc, 40);
        CHECK (peak (s) == 0.0f, "no MIDI, and yet sound (%.3g)", peak (s));

        proc.setCurrentProgram (9);           // 909 Classic
        proc.prepareToPlay (kFs, kBlock);
        const auto a = run (proc, 100, 1000);
        const float pk = peak (a);
        float lr = 0;
        for (int i = 0; i < a.getNumSamples(); ++i) lr = std::max (lr, std::abs (a.getSample (0, i) - a.getSample (1, i)));
        std::printf ("  909 Classic on a MIDI note: peak %.3f, left/right differ by %.3g\n", pk, lr);
        CHECK (pk > 0.2f && pk <= 1.0f, "a note plays at %.3f", pk);
        CHECK (lr == 0.0f, "a kick is mono, both sides identical");

        //  sample-accurate: the onset lands at the note's position plus the
        //  reported latency, whatever block it falls in
        const int lat = proc.getLatencySamples();
        int onset = -1;
        for (int i = 0; i < a.getNumSamples(); ++i) if (std::abs (a.getSample (0, i)) > 0.01f) { onset = i; break; }
        std::printf ("  latency reported %d; a note at sample 1000 is heard from sample %d\n", lat, onset);
        CHECK (lat == 23 + 72, "latency %d", lat);
        CHECK (onset >= 1000 + lat - 8 && onset <= 1000 + lat + 30, "the note lands at %d", onset);
    }

    // -- programs: every factory preset loads exactly and plays bounded ------
    {
        float worst = 0; bool exact = true;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto want = ks::presetParams (i), got = proc.currentParams();
            for (int k = 0; k < ks::kNumParams; ++k)
            {
                const auto& s = ks::specs()[k];
                const float tol = 1.0e-4f * (s.hi - s.lo);
                if (std::abs (want.*(s.member) - got.*(s.member)) > tol)
                { exact = false; std::printf ("  %s: %s %g != %g\n", ks::preset (i).name, s.id, got.*(s.member), want.*(s.member)); }
            }
            proc.prepareToPlay (kFs, kBlock);
            const auto x = run (proc, 60, 10);
            worst = std::max (worst, peak (x));
            CHECK (peak (x) > 0.05f, "%s is silent in the host", ks::preset (i).name);
        }
        std::printf ("  %d programs load exactly; loudest peak %.3f\n", proc.getNumPrograms(), worst);
        CHECK (exact, "a program does not load its own values");
        CHECK (worst <= 1.0f, "a program leaves full scale");
    }

    // -- the pad on the panel -------------------------------------------------
    {
        proc.prepareToPlay (kFs, kBlock);
        const int before = proc.hitCount();
        proc.triggerFromUI (1.0f);
        const auto a = run (proc, 40);
        CHECK (proc.hitCount() == before + 1 && peak (a) > 0.05f, "the pad does not hit");
    }

    // -- state and user patches ---------------------------------------------------
    {
        proc.setCurrentProgram (21);          // Gabber
        if (auto* p = proc.apvts.getParameter ("decay")) p->setValueNotifyingHost (p->convertTo0to1 (1234.0f));
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        KickstartProcessor fresh;
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto a = proc.currentParams(), b = fresh.currentParams();
        bool same = true;
        for (int k = 0; k < ks::kNumParams; ++k)
            same = same && std::abs (a.*(ks::specs()[k].member) - b.*(ks::specs()[k].member)) < 1.0e-3f;
        CHECK (same, "the state round trip changed a parameter");
        CHECK (fresh.getCurrentProgram() == 21 && fresh.currentName() == "Gabber", "the preset name did not survive");

        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickstart-test.json");
        CHECK (proc.saveUserPatch (f, "My Test Kick"), "saving a patch failed");
        KickstartProcessor other;
        CHECK (other.loadUserPatch (f), "loading a patch failed");
        const auto c = other.currentParams();
        bool same2 = true;
        for (int k = 0; k < ks::kNumParams; ++k)
            same2 = same2 && std::abs (a.*(ks::specs()[k].member) - c.*(ks::specs()[k].member)) < 1.0e-3f;
        CHECK (same2 && other.currentName() == "My Test Kick", "a user patch did not round-trip");
        f.deleteFile();
        std::printf ("  state and user patches round-trip; DECAY 1234 ms came back %.1f\n", c.decay);
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" - ALL CLEAR\n");
    else               std::printf (" - %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
