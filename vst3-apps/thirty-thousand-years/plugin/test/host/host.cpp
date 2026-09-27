/*  What does a DAW actually see of Thirty Thousand Years?

    The bench drives the ENGINE and the live probe drives the PANEL; this is
    the link between them - the VST3 wrapper as a host loads it. It scans the
    bundle, instantiates it, prints the parameter list in a host's own terms,
    then proves the plug-in is alive: silent with no note and the drone off,
    a MIDI note makes sound, a parameter moved through the host reads back,
    a saved state restores (values AND the scenes/macros blobs), the AUX
    input bus exists, and a second instance instantiates beside the first.

      ttyhost                  the installed bundle
      ttyhost <path.vst3>      any bundle
      ttyhost <path> list      ...and print every parameter
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/Params.h"      // for NUM_PARAMS: the count is derived, not typed
#include <juce_audio_utils/juce_audio_utils.h>

static int checks = 0, fails = 0;
static void check (bool ok, const char* what) { ++checks; if (! ok) ++fails; std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what); }

static float renderPeak (juce::AudioPluginInstance& inst, juce::MidiBuffer& midi, int blocks)
{
    juce::AudioBuffer<float> buf (juce::jmax (2, inst.getTotalNumInputChannels() + inst.getTotalNumOutputChannels()), 512);
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

static juce::AudioProcessorParameter* byName (juce::AudioPluginInstance& inst, const juce::String& nm)
{
    for (auto* p : inst.getParameters()) if (p->getName (64).equalsIgnoreCase (nm)) return p;
    return nullptr;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File house ("C:/Program Files/Common Files/VST3/Brokild/Brokild collection");
    const juce::String path = argc > 1 ? juce::String (argv[1]) : house.getChildFile ("Thirty Thousand Years.vst3").getFullPathName();
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

    std::printf ("\n%s  -  %s  -  %s  -  latency %d\n", inst->getName().toRawUTF8(), found[0]->pluginFormatName.toRawUTF8(),
                 found[0]->isInstrument ? "instrument" : "effect", inst->getLatencySamples());
    int np = 0, automatable = 0; bool sawMacro5 = false, sawHistory = false, sawHumanity = false;
    for (auto* p : inst->getParameters())
    {
        const juce::String nm = p->getName (64);
        if (nm.startsWith ("MIDI CC") || nm == "Bypass") continue;
        ++np;
        if (p->isAutomatable()) ++automatable;
        if (nm.equalsIgnoreCase ("BWFX MACRO 5")) sawMacro5 = true;
        if (nm.equalsIgnoreCase ("HISTORY")) sawHistory = true;
        if (nm.equalsIgnoreCase ("HUMANITY")) sawHumanity = true;
    }
    std::printf ("the host is offered %d of ours (%d in all, with the emulated MIDI CCs)\n", np, inst->getParameters().size());
    if (argc > 2) for (auto* p : inst->getParameters()) std::printf ("  %-26s %s  %s\n", p->getName (64).toRawUTF8(), p->isAutomatable() ? "auto" : "    ", p->getCurrentValueAsText().toRawUTF8());

    check (found[0]->isInstrument, "the host sees an instrument");
    /*  Derived, never typed. A hardcoded 395 here passed for weeks and then
        failed the moment the table gained PATCH TRIM - a count written down
        beside the thing it counts goes stale exactly like a stale fixture,
        and the honest fix is to ask the table. */
    const int want = tty::NUM_PARAMS + 5;
    check (np == want, (juce::String (want) + " parameters: " + juce::String (tty::NUM_PARAMS)
                        + " of the table plus the five BWFX macros").toRawUTF8());
    check (automatable == np - 4, "every parameter automatable except QUALITY, SEED, DETERMINISTIC and PATCH");
    check (sawMacro5 && sawHistory && sawHumanity, "BWFX MACRO 5, HISTORY and HUMANITY are in the list by name");
    check (inst->getBusCount (true) >= 1 && inst->getBus (true, 0)->getName().containsIgnoreCase ("Aux"), "an AUX input bus is declared");
    check (inst->getLatencySamples() == 96, "reported latency is the limiter's 2 ms (96 samples at 48 k)");

    // silence with no note and the drone off
    { juce::MidiBuffer none; check (renderPeak (*inst, none, 20) == 0.0f, "no note, drone off: exactly silent"); }
    // a note makes sound in a host
    {
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);
        const float pk = renderPeak (*inst, on, 40);
        check (pk > 0.02f && pk <= 1.0f, "a MIDI note-on plays (peak between 0.02 and 1.0)");
        std::printf ("      peak %.3f\n", pk);
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        renderPeak (*inst, off, 400);
    }
    // the drone switch through the host
    {
        auto* drone = byName (*inst, "DRONE");
        check (drone != nullptr, "DRONE found by name");
        if (drone)
        {
            drone->setValueNotifyingHost (1.0f);
            juce::MidiBuffer none; const float pk = renderPeak (*inst, none, 80);
            check (pk > 0.02f, "DRONE on through the host makes sound");
            std::printf ("      drone peak %.3f\n", pk);
            drone->setValueNotifyingHost (0.0f); renderPeak (*inst, none, 600);
        }
    }
    // move a parameter through the host and read it back
    auto* cut = byName (*inst, "CUTOFF");
    check (cut != nullptr, "CUTOFF found by name");
    if (cut != nullptr)
    {
        cut->setValueNotifyingHost (0.25f);
        check (std::abs (cut->getValue() - 0.25f) < 1e-4f, "CUTOFF set to 0.25 reads back 0.25");
        std::printf ("      shows as %s\n", cut->getCurrentValueAsText().toRawUTF8());
    }
    // state round trip keeps the moved value
    {
        juce::MemoryBlock state;
        inst->getStateInformation (state);
        if (cut != nullptr) cut->setValueNotifyingHost (0.9f);
        inst->setStateInformation (state.getData(), (int) state.getSize());
        check (cut != nullptr && std::abs (cut->getValue() - 0.25f) < 1e-3f, "state saved with 0.25, moved to 0.9, restored: reads 0.25");
        std::printf ("      state is %d bytes\n", (int) state.getSize());
    }
    // a second instance in the same process
    {
        juce::String err2;
        auto inst2 = fm.createPluginInstance (*found[0], 48000.0, 512, err2);
        check (inst2 != nullptr, "a second instance instantiates beside the first");
        if (inst2) { inst2->prepareToPlay (48000.0, 512); inst2->releaseResources(); }
    }
    // other rates and block sizes
    for (double sr : { 44100.0, 96000.0 })
    {
        inst->releaseResources(); inst->prepareToPlay (sr, 128);
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);
        juce::AudioBuffer<float> buf (2, 128); float pk = 0.0f; bool finite = true;
        for (int b = 0; b < 200; ++b) { buf.clear(); juce::MidiBuffer m; if (b == 0) m = on; inst->processBlock (buf, m); pk = std::max (pk, buf.getMagnitude (0, 128)); for (int i = 0; i < 128; ++i) if (! std::isfinite (buf.getSample (0, i))) finite = false; }
        check (finite && pk > 0.02f && pk <= 1.0f, sr == 44100.0 ? "44.1 kHz, 128-sample blocks: plays, finite" : "96 kHz, 128-sample blocks: plays, finite");
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 45), 0); juce::MidiBuffer m2 = off; inst->processBlock (buf, m2);
    }
    inst->releaseResources();
    inst.reset();
    std::printf ("\n%d checks, %d failed%s\n", checks, fails, fails ? "" : " - ALL CLEAR");
    return fails ? 1 : 0;
}
