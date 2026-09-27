/*  What does a DAW actually see of 1984?

    The bench drives the ENGINE and the live probe drives the PANEL; this is
    the link between them - the VST3 wrapper as a host loads it. It scans the
    bundle, instantiates it, prints the parameter list in a host's own terms,
    then proves the plug-in is alive: a MIDI note makes sound, a parameter
    moved through the host reads back, a saved state restores, and the
    plug-in is silent with no note.

      n84host                  the installed bundle
      n84host <path.vst3>
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

static int checks = 0, fails = 0;
static void check (bool ok, const char* what) { ++checks; if (! ok) ++fails; std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what); }

static float renderPeak (juce::AudioPluginInstance& inst, juce::MidiBuffer& midi, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512);
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        juce::MidiBuffer m; if (b == 0) m = midi;
        inst.processBlock (buf, m);
        peak = std::max (peak, buf.getMagnitude (0, 512));
    }
    return peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File house ("C:/Program Files/Common Files/VST3/Brokild/Brokild collection");
    const juce::String path = argc > 1 ? juce::String (argv[1]) : house.getChildFile ("1984.vst3").getFullPathName();
    std::printf ("scanning %s\n", path.toRawUTF8());

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    juce::KnownPluginList list;
    juce::OwnedArray<juce::PluginDescription> found;
    for (int i = 0; i < fm.getNumFormats(); ++i)
        if (auto* f = fm.getFormat (i))
            if (f->getName().containsIgnoreCase ("VST3"))
                list.scanAndAddFile (path, true, found, *f);
    if (found.isEmpty()) { std::printf ("FAILED: the host could not scan that bundle\n"); return 1; }

    juce::String err;
    auto inst = fm.createPluginInstance (*found[0], 48000.0, 512, err);
    if (inst == nullptr) { std::printf ("FAILED to instantiate: %s\n", err.toRawUTF8()); return 1; }
    inst->prepareToPlay (48000.0, 512);

    std::printf ("\n%s  -  %s  -  %s\n", inst->getName().toRawUTF8(), found[0]->pluginFormatName.toRawUTF8(),
                 found[0]->isInstrument ? "instrument" : "effect");
    /*  JUCE's VST3 wrapper emulates 16 x 130 MIDI controllers as hidden
        parameters (2080 of them) plus a Bypass, so a synth is "offered" 2212.
        A DAW hides those; so does this count. */
    int np = 0, automatable = 0; bool sawMacro5 = false, sawIL = false;
    for (auto* p : inst->getParameters())
    {
        const juce::String nm = p->getName (32);
        if (nm.startsWith ("MIDI CC") || nm == "Bypass") continue;
        ++np;
        if (p->isAutomatable()) ++automatable;
        if (nm.equalsIgnoreCase ("BWFX MACRO 5")) sawMacro5 = true;
        if (nm.equalsIgnoreCase ("I INITIAL LVL")) sawIL = true;
    }
    std::printf ("the host is offered %d of ours (%d in all, with the emulated MIDI CCs)\n", np, inst->getParameters().size());
    if (argc > 2) for (auto* p : inst->getParameters()) std::printf ("  %-18s %s  %s\n", p->getName (32).toRawUTF8(), p->isAutomatable() ? "auto" : "    ", p->getCurrentValueAsText().toRawUTF8());

    check (found[0]->isInstrument, "the host sees an instrument");
    check (np == 131, "131 parameters: 126 of the table plus the five BWFX macros");
    check (automatable == np - 2, "every parameter automatable except QUALITY and PATCH");
    check (sawMacro5 && sawIL, "BWFX MACRO 5 and I INITIAL LVL are in the list by name");

    // silence with no note
    { juce::MidiBuffer none; check (renderPeak (*inst, none, 20) == 0.0f, "no note: exactly silent"); }
    // a note makes sound in a host
    {
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 57, 0.8f), 0);
        const float pk = renderPeak (*inst, on, 40);
        check (pk > 0.02f && pk <= 1.0f, "a MIDI note-on plays (peak between 0.02 and 1.0)");
        std::printf ("      peak %.3f\n", pk);
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
        renderPeak (*inst, off, 400);
    }
    // move a parameter through the host and read it back
    juce::AudioProcessorParameter* lpf = nullptr;
    for (auto* p : inst->getParameters()) if (p->getName (32).equalsIgnoreCase ("I LPF")) lpf = p;
    check (lpf != nullptr, "I LPF found by name");
    if (lpf != nullptr)
    {
        lpf->setValueNotifyingHost (0.25f);
        check (std::abs (lpf->getValue() - 0.25f) < 1e-4f, "I LPF set to 0.25 reads back 0.25");
        std::printf ("      shows as %s\n", lpf->getCurrentValueAsText().toRawUTF8());
    }
    // state round trip keeps the moved value
    {
        juce::MemoryBlock state;
        inst->getStateInformation (state);
        if (lpf != nullptr) lpf->setValueNotifyingHost (0.9f);
        inst->setStateInformation (state.getData(), (int) state.getSize());
        check (lpf != nullptr && std::abs (lpf->getValue() - 0.25f) < 1e-3f, "state saved with 0.25, moved to 0.9, restored: reads 0.25");
        std::printf ("      state is %d bytes\n", (int) state.getSize());
    }
    // a second instance in the same process
    {
        juce::String err2;
        auto inst2 = fm.createPluginInstance (*found[0], 48000.0, 512, err2);
        check (inst2 != nullptr, "a second instance instantiates beside the first");
        if (inst2) { inst2->prepareToPlay (48000.0, 512); inst2->releaseResources(); }
    }
    inst->releaseResources();
    inst.reset();
    std::printf ("\n%d checks, %d failed%s\n", checks, fails, fails ? "" : " - ALL CLEAR");
    return fails ? 1 : 0;
}
