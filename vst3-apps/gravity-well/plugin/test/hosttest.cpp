/*  GRAVITY WELL - the host harness.

    The REAL wrapper, in a host's own terms: parameters, buses, MIDI, state.
    No DAW, no audio device, no window.  The bench proves the engine; this
    proves the thing a DAW actually loads, and the gap between those two is
    where "I cannot see the new parameters" and "it plays nothing" live.

    A DAW hands back JUCE's own host-side wrapper parameters, whose paramID
    is empty - so everything here matches on NAME, which is what a DAW shows
    anyway.  Matching on id reported every parameter missing while printing
    them two lines above.
*/
#include <JuceHeader.h>
#include "PluginProcessor.h"

static int checks = 0, fails = 0;
static void CHECK (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) { ++fails; std::printf ("  FAIL  %s\n", what.toRawUTF8()); }
}
static void say (const char* s) { std::printf ("-- %s\n", s); }

static float rmsOf (const juce::AudioBuffer<float>& b)
{
    double s = 0.0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        { const double x = b.getSample (c, i); s += x * x; }
    const int n = juce::jmax (1, b.getNumChannels() * b.getNumSamples());
    return (float) std::sqrt (s / n);
}

//  A host that says it is playing at 128 BPM, so the sequencer has a clock.
struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 128.0;
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        return p;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const double sr = 48000.0;
    const int    bs = 256;

    std::printf ("GRAVITY WELL host harness\n\n");

    GravityWellAudioProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, bs);
    proc.prepareToPlay (sr, bs);

    say ("it is an instrument, with MIDI in and a stereo out");
    CHECK (proc.acceptsMidi(), "the plug-in does not accept MIDI");
    CHECK (! proc.producesMidi(), "the plug-in claims to produce MIDI");
    CHECK (proc.getTotalNumOutputChannels() == 2, "the output is not stereo");
    CHECK (proc.hasEditor(), "there is no editor");

    say ("every engine parameter reaches the host, by name");
    {
        auto& params = proc.getParameters();
        juce::StringArray names;
        for (auto* p : params)
        {
            const auto n = p->getName (128);
            if (n.contains ("MIDI CC") || n == "Bypass") continue;   // JUCE's own
            names.add (n);
        }
        int missing = 0;
        juce::String firstMissing;
        for (int i = 0; i < gw::numParams(); ++i)
            if (! names.contains (juce::String (gw::spec (i).label)))
            { ++missing; if (firstMissing.isEmpty()) firstMissing = gw::spec (i).label; }
        std::printf ("     %d host parameters, %d from the table, %d missing\n",
                     names.size(), gw::numParams(), missing);
        CHECK (missing == 0, "parameters missing from the host list, first: " + firstMissing);

        //  the five macros are the rack's ENTIRE host surface
        int macros = 0;
        for (const auto& n : names) if (n.startsWith ("BWFX MACRO")) ++macros;
        CHECK (macros == 5, "there are not exactly five BWFX macros");
    }

    say ("a parameter set through the host reads back");
    {
        auto* p = proc.apvts.getParameter ("fa_cut");
        CHECK (p != nullptr, "A CUTOFF is not a host parameter");
        if (p != nullptr)
        {
            p->setValueNotifyingHost (p->convertTo0to1 (0.75f));
            const float got = proc.apvts.getRawParameterValue ("fa_cut")->load();
            std::printf ("     A CUTOFF set 0.750, read %.3f\n", got);
            CHECK (std::abs (got - 0.75f) < 0.01f, "A CUTOFF did not read back");
        }
    }

    say ("silence in, silence out - a fresh instance must not sound");
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        for (int i = 0; i < 8; ++i) proc.processBlock (buf, midi);
        CHECK (rmsOf (buf) < 1.0e-6f, "a fresh instance makes sound with no MIDI");
    }

    say ("a MIDI note sounds, and stops when it is released");
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 0);
        float loud = 0.f;
        for (int i = 0; i < 40; ++i) { proc.processBlock (buf, midi); midi.clear(); loud = juce::jmax (loud, rmsOf (buf)); }
        std::printf ("     note on rms %.5f\n", loud);
        CHECK (loud > 1.0e-3f, "a MIDI note made no sound");

        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, midi); midi.clear();
        for (int i = 0; i < 400; ++i) proc.processBlock (buf, midi);
        std::printf ("     after all-notes-off rms %.7f\n", rmsOf (buf));
        CHECK (rmsOf (buf) < 1.0e-4f, "the plug-in kept sounding after all notes off");
    }

    say ("a panic lets go of the sustain pedal");
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);   // pedal down
        midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 1);
        proc.processBlock (buf, midi); midi.clear();
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, midi); midi.clear();
        //  the NEXT note must not be held for ever by a pedal nobody pressed
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi); midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        proc.processBlock (buf, midi); midi.clear();
        for (int i = 0; i < 500; ++i) proc.processBlock (buf, midi);
        std::printf ("     tail after the panic %.7f\n", rmsOf (buf));
        CHECK (rmsOf (buf) < 1.0e-4f, "a note after a panic is held for ever - the pedal is stuck");
    }

    say ("every factory program loads and is named");
    {
        CHECK (proc.getNumPrograms() == gw::NUM_FACTORY, "the program count is not the factory bank");
        int unnamed = 0;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            if (proc.getProgramName (i).isEmpty()) ++unnamed;
        }
        CHECK (unnamed == 0, "some factory programs have no name");
        proc.setCurrentProgram (0);
    }

    say ("the state round-trips: parameters, the pattern and the rack");
    {
        proc.setParamById ("fa_cut", 0.31f, false);
        proc.setParamById ("macro_mass", 0.62f, false);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);

        proc.setParamById ("fa_cut", 0.90f, false);
        proc.setParamById ("macro_mass", 0.10f, false);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());

        const float a = proc.apvts.getRawParameterValue ("fa_cut")->load();
        const float b = proc.apvts.getRawParameterValue ("macro_mass")->load();
        std::printf ("     restored A CUTOFF %.3f (0.310)   MASS %.3f (0.620)\n", a, b);
        CHECK (std::abs (a - 0.31f) < 0.01f, "A CUTOFF did not survive the state round trip");
        CHECK (std::abs (b - 0.62f) < 0.01f, "MASS did not survive the state round trip");
        CHECK (mb.getSize() > 0, "the state is empty");
    }

    say ("the sequencer takes the host's clock");
    {
        FakeHead head;
        proc.setPlayHead (&head);
        proc.setParamById ("seq_on", 1.0f, false);
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 0);
        int seen = 0, lastStep = -2;
        for (int i = 0; i < 400; ++i)
        {
            proc.processBlock (buf, midi); midi.clear();
            head.ppq += (double) bs / sr * head.bpm / 60.0;
            const int s = proc.engine.seqStep();
            if (s != lastStep) { lastStep = s; ++seen; }
        }
        std::printf ("     the sequencer visited %d steps at %.0f BPM\n", seen, head.bpm);
        CHECK (seen > 4, "the sequencer did not advance on the host clock");
        proc.setParamById ("seq_on", 0.0f, false);
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, midi);
        proc.setPlayHead (nullptr);
    }

    std::printf ("\n%d checks, %d failures\n", checks, fails);
    if (! fails) std::printf ("ALL CLEAR\n");
    return fails ? 1 : 0;
}
