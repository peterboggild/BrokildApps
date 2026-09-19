// RITE OF PASSAGE host harness. The bench proves the ENGINE; this proves the
// WRAPPER — the parameter table, the buses, the rite blob, and the one thing
// a DSP bench cannot reach: that ARRIVAL is a TRIGGER and NOT the end of the
// slider, so scrubbing to 100 % does not fire a drop.

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    constexpr double kFs = 48000.0;
    constexpr int kBlock = 256;

    void setP (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plain)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    struct Play : juce::AudioPlayHead
    {
        double bpm = 128.0; long long samples = 0; bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setPpqPosition ((double) samples * bpm / (60.0 * kFs));
            p.setIsPlaying (playing);
            p.setTimeInSamples (samples);
            return p;
        }
    };

    float run (RiteProcessor& proc, Play& ph, int nBlocks, bool silent = true)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> b (2, kBlock);
        float peak = 0;
        for (int i = 0; i < nBlocks; ++i)
        {
            b.clear();
            if (! silent)
                for (int c = 0; c < 2; ++c)
                    for (int j = 0; j < kBlock; ++j)
                        b.setSample (c, j, 0.2f * std::sin (0.05f * (float) (i * kBlock + j)));
            proc.processBlock (b, midi);
            ph.samples += kBlock;
            peak = std::max (peak, b.getMagnitude (0, kBlock));
        }
        return peak;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("RITE OF PASSAGE host harness\n\n");

    RiteProcessor proc;
    Play ph;
    proc.setPlayHead (&ph);
    proc.setPlayConfigDetails (2, 2, kFs, kBlock);
    proc.prepareToPlay (kFs, kBlock);

    // -- the parameter surface is small, fixed and uniquely named ------------
    {
        juce::StringArray ids;
        for (auto* p : proc.getParameters())
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                ids.add (wp->paramID);
        const int before = ids.size();
        ids.removeDuplicates (false);
        CHECK (ids.size() == before, "a parameter id is declared twice");
        CHECK (before == 7 + bwfx::kMacros, "the parameter list is %d, not the fixed surface of §10", before);
        std::printf ("  %d host parameters — the fixed surface\n", before);
    }

    // -- SCRUBBING TO THE END DOES NOT FIRE A DROP (§4) ----------------------
    /*  The fault the design was changed to avoid. Walk the slider all the way
        to 100 %, with silence in and no slot assigned: if the landing were
        tied to the slider, the impact would sound. */
    {
        proc.prepareToPlay (kFs, kBlock);
        for (int k = 0; k <= 100; ++k)
        {
            setP (proc.apvts, rop_ids::position, (float) k);
            run (proc, ph, 1);
        }
        const float peak = run (proc, ph, 40);
        std::printf ("  scrubbed 0 -> 100 %%      peak %.3g (nothing fired)\n", peak);
        CHECK (peak == 0.0f, "reaching the end of the slider fired the arrival (%.3g)", peak);
        CHECK (! proc.rack().arrived(), "the rack thinks it arrived from the slider alone");
    }

    // -- and pressing ARRIVAL does ------------------------------------------
    {
        setP (proc.apvts, rop_ids::arrival, 1.0f);
        //  a bar at 128 BPM is 90 000 samples: the arrival waits for the
        //  boundary, so the test has to wait for it too
        const float peak = run (proc, ph, 400);
        std::printf ("  ARRIVAL pressed          peak %.3f\n", peak);
        CHECK (peak > 0.01f, "ARRIVAL fired nothing (%.3g)", peak);
        CHECK (proc.rack().arrived(), "ARRIVAL did not mark the rack arrived");
        setP (proc.apvts, rop_ids::arrival, 0.0f);
    }

    // -- an empty rite is transparent ----------------------------------------
    {
        RiteProcessor p2;
        Play ph2; p2.setPlayHead (&ph2);
        p2.setPlayConfigDetails (2, 2, kFs, kBlock);
        p2.prepareToPlay (kFs, kBlock);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> b (2, kBlock), ref (2, kBlock);
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < kBlock; ++j)
                b.setSample (c, j, 0.3f * std::sin (0.031f * (float) j + (float) c));
        ref.makeCopyOf (b);
        for (int i = 0; i < 20; ++i) { p2.processBlock (b, midi); ph2.samples += kBlock; }
        //  the last block is the steady state; compare it to what went in
        float worst = 0;
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < kBlock; ++j)
                worst = std::max (worst, std::abs (b.getSample (c, j) - ref.getSample (c, j)));
        std::printf ("  empty rite               error %.3g\n", worst);
        CHECK (worst == 0.0f, "an empty rite is not transparent (%.3g)", worst);
    }

    // -- the rite survives a round trip into a fresh instance ---------------
    {
        const int climb = rop::effectTypeByName ("climb");
        const int tape  = rop::effectTypeByName ("tape");
        proc.rack().setSlotEffect (0, climb);
        proc.rack().setSlotEffect (3, tape);
        proc.rack().state (0).enter = 0.42f;
        proc.rack().state (0).exit  = 0.88f;
        proc.rack().state (0).curve = rop::CurveAccel;
        proc.rack().state (0).place = rop::Place::Side;
        proc.rack().state (3).tail  = rop::Tail::Spill;
        proc.rack().state (0).A[1] = 250.0f;
        proc.rack().state (0).B[1] = 9000.0f;
        setP (proc.apvts, rop_ids::spread, 40.0f);
        proc.bwfxRack().setEnabled (0, true);

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);

        RiteProcessor fresh;
        fresh.setPlayConfigDetails (2, 2, kFs, kBlock);
        fresh.prepareToPlay (kFs, kBlock);
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());

        CHECK (fresh.rack().slotEffect (0) == climb, "slot 0's effect did not survive the round trip");
        CHECK (fresh.rack().slotEffect (3) == tape,  "slot 3's effect did not survive the round trip");
        CHECK (std::abs (fresh.rack().state (0).enter - 0.42f) < 1e-4f, "ENTER did not survive");
        CHECK (fresh.rack().state (0).curve == rop::CurveAccel, "the curve did not survive");
        CHECK (fresh.rack().state (0).place == rop::Place::Side, "PLACE did not survive");
        CHECK (fresh.rack().state (3).tail == rop::Tail::Spill, "the tail mode did not survive");
        CHECK (std::abs (fresh.rack().state (0).B[1] - 9000.0f) < 0.5f, "a B value did not survive");
        CHECK (std::abs (fresh.apvts.getRawParameterValue (rop_ids::spread)->load() - 40.0f) < 0.01f,
               "SPREAD did not survive");
        CHECK (fresh.bwfxRack().getEnabled (0) == proc.bwfxRack().getEnabled (0),
               "the BWFX blob did not survive");
        std::printf ("  state round trip         %d bytes\n", (int) mb.getSize());
    }

    // -- an unknown effect id leaves its slot empty rather than failing ------
    {
        RiteProcessor p3;
        p3.setPlayConfigDetails (2, 2, kFs, kBlock);
        p3.prepareToPlay (kFs, kBlock);
        p3.riteFromJson (R"({"slots":[{"fx":"nosuchthing","on":true,"enter":0.3}]})");
        CHECK (p3.rack().slotEffect (0) == -1, "an unknown effect id did not leave the slot empty");
        CHECK (std::abs (p3.rack().state (0).enter - 0.3f) < 1e-4f,
               "the rest of the slot was discarded with the unknown id");
        std::printf ("  a rite from a newer build tolerated\n");
    }

    // -- buses ----------------------------------------------------------------
    {
        RiteProcessor b;
        using Set = juce::AudioChannelSet;
        CHECK (b.checkBusesLayoutSupported ({ { Set::stereo() }, { Set::stereo() } }), "stereo refused");
        CHECK (b.checkBusesLayoutSupported ({ { Set::mono() },   { Set::mono() } }),   "mono refused");
        std::printf ("  buses                    stereo and mono\n");
    }

    // -- AUTO TRANSITION -------------------------------------------------------
    {
        std::printf ("  auto transition:\n");
        RiteProcessor ap;
        Play aph;
        ap.setPlayHead (&aph);
        ap.setPlayConfigDetails (2, 2, kFs, kBlock);
        ap.prepareToPlay (kFs, kBlock);

        //  the 8th bar of an 8-bar cycle, which is Peter's own example
        auto& a = ap.autoCycle();
        a.on = true; a.bars = 8; a.start = 8.0f; a.end = 9.0f; a.down = false;

        //  park the transport at a given bar of the cycle and read the position
        auto at = [&] (double barLine)
        {
            const double beats = (barLine - 1.0) * 4.0;
            aph.samples = (long long) (beats * 60.0 * kFs / aph.bpm);
            juce::MidiBuffer m; juce::AudioBuffer<float> b (2, kBlock); b.clear();
            ap.processBlock (b, m);
            return ap.effectivePosition();
        };

        const float before = at (4.0), atStart = at (8.0), mid = at (8.5),
                    atEnd  = at (9.0 - 0.01), after = at (2.0);
        std::printf ("    bar 4 %.2f   bar 8 %.2f   bar 8.5 %.2f   bar 9 %.2f\n",
                     before, atStart, mid, atEnd);
        CHECK (before < 0.01f, "before the window the sweep is not at 0 (%.3f)", before);
        CHECK (atStart < 0.02f, "at the start of the window the sweep is not at 0 (%.3f)", atStart);
        CHECK (std::abs (mid - 0.5f) < 0.03f, "half way through the window the sweep is %.3f, not 0.5", mid);
        CHECK (atEnd > 0.97f, "by the end of the window the sweep is only %.3f", atEnd);
        CHECK (after < 0.01f, "the next cycle did not start from 0 (%.3f)", after);

        //  and the other direction is its mirror
        a.down = true;
        const float dStart = at (8.0), dMid = at (8.5), dEnd = at (9.0 - 0.01);
        std::printf ("    100 to 0:  bar 8 %.2f   bar 8.5 %.2f   bar 9 %.2f\n", dStart, dMid, dEnd);
        CHECK (dStart > 0.98f && dEnd < 0.03f && std::abs (dMid - 0.5f) < 0.03f,
               "100 to 0 is not the mirror of 0 to 100 (%.2f %.2f %.2f)", dStart, dMid, dEnd);

        //  a different window, to prove the numbers are read and not assumed
        a.down = false; a.start = 7.0f; a.end = 8.0f;
        const float w2a = at (7.5), w2b = at (8.5);
        std::printf ("    window 7 to 8:  bar 7.5 %.2f   bar 8.5 %.2f\n", w2a, w2b);
        CHECK (std::abs (w2a - 0.5f) < 0.03f, "the 7-to-8 window is not half way at bar 7.5 (%.3f)", w2a);
        CHECK (w2b > 0.97f, "past the 7-to-8 window the sweep is only %.3f", w2b);

        /*  AND WITH AUTO OFF NOTHING CHANGES. This is the check that protects
            every project that already exists: the parameter drives the rack
            exactly as it did before any of this was written. */
        a.on = false;
        setP (ap.apvts, rop_ids::position, 62.0f);
        const float manual = at (3.0);
        std::printf ("    auto off, POSITION at 62 %%:  %.2f\n", manual);
        CHECK (std::abs (manual - 0.62f) < 0.01f,
               "with AUTO off the host parameter no longer drives POSITION (%.3f)", manual);
    }

    std::printf ("\n%d checks", checks);
    if (failures == 0) std::printf (" — ALL CLEAR\n");
    else               std::printf (" — %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
