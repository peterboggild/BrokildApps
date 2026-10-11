/*  What does a DAW actually see of Magneto Transport?

    The bench drives the ENGINE and the live probe drives the PANEL; this is the link
    between them: the VST3 wrapper as a host loads it, from the INSTALLED bundle (the
    file a DAW opens). It prints the parameter list in a host's own terms and proves:
    the factory presets are the host's programs; audio passes; REPRO tells the host its
    latency; the host's bypass is ours and is the dry signal delayed by that latency;
    a saved state restores; a second instance runs beside the first.

      mthost                  the installed bundle
      mthost <path.vst3> [-v]
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

static int checks = 0, fails = 0;
static void check (bool ok, const char* what) { ++checks; if (! ok) ++fails; std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what); }

static juce::AudioProcessorParameter* byName (juce::AudioPluginInstance& inst, const char* n)
{
    for (auto* p : inst.getParameters()) if (p->getName (64).equalsIgnoreCase (n)) return p;
    return nullptr;
}

// 1 kHz at 0 VU (-18 dBFS rms) in, the output back
static void render (juce::AudioPluginInstance& inst, int blocks, std::vector<float>* outL, std::vector<float>* in, int64_t& t)
{
    juce::AudioBuffer<float> buf (2, 512);
    const double amp = 0.12589254117941673 * std::sqrt (2.0);
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float x = (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) (t + i) / 48000.0));
            buf.setSample (0, i, x); buf.setSample (1, i, x);
            if (in) in->push_back (x);
        }
        t += 512;
        juce::MidiBuffer m;
        inst.processBlock (buf, m);
        if (outL) for (int i = 0; i < 512; ++i) outL->push_back (buf.getSample (0, i));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1);   // the processor's timer reports latency
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File house ("C:/Program Files/Common Files/VST3/Brokild/Experimental");
    const juce::String path = argc > 1 && argv[1][0] != '-' ? juce::String (argv[1]) : house.getChildFile ("Magneto Transport.vst3").getFullPathName();
    const bool verbose = argc > 1 && juce::String (argv[argc - 1]) == "-v";
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
    int np = 0, automatable = 0;
    for (auto* p : inst->getParameters())
    {
        const juce::String nm = p->getName (64);
        // JUCE adds a "Program" selector for the 24 factory programs; a DAW shows it as that
        if (nm.startsWith ("MIDI CC") || nm == "Program") continue;
        ++np;
        if (p->isAutomatable()) ++automatable;
        if (verbose) std::printf ("  %-22s %s  %s\n", nm.toRawUTF8(), p->isAutomatable() ? "auto" : "    ", p->getCurrentValueAsText().toRawUTF8());
    }
    std::printf ("the host is offered %d parameters\n", np);
    check (! found[0]->isInstrument, "the host sees an effect");
    check (np == 47 && automatable == np, "47 parameters, every one automatable (44 of the machine, HEAD PATTERN, BYPASS, MECH SOUNDS)");
    auto* byp = inst->getBypassParameter();
    check (byp != nullptr && byp->getName (64).equalsIgnoreCase ("BYPASS"), "the host's bypass is the plug-in's own BYPASS");

    // the factory presets are the host's programs
    check (inst->getNumPrograms() == 24 && inst->getProgramName (4) == "Space Echo 1:2:3", "24 programs; program 4 is Space Echo 1:2:3");
    auto* h2 = byName (*inst, "HEAD 2");
    auto* fb = byName (*inst, "FEEDBACK");
    const float fb0 = fb ? fb->getValue() : -1.0f;
    inst->setCurrentProgram (4);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    check (h2 != nullptr && h2->getValue() > 0.5f && fb != nullptr && std::abs (fb->getValue() - fb0) > 1e-3f, "selecting it switches head 2 on and moves FEEDBACK");
    inst->setCurrentProgram (0);

    // audio passes; REPRO reports the latency
    int64_t t = 0;
    {
        std::vector<float> o;
        render (*inst, 200, &o, nullptr, t);
        double e = 0; for (size_t i = o.size() / 2; i < o.size(); ++i) e += (double) o[i] * o[i];
        check (e > 1.0, "a 0 VU tone comes out");
    }
    auto* repro = byName (*inst, "REPRO");
    check (repro != nullptr, "REPRO found by name");
    if (repro) repro->setValueNotifyingHost (1.0f);
    render (*inst, 300, nullptr, nullptr, t);
    const int lat = inst->getLatencySamples();
    std::printf ("      REPRO latency %d samples (%.1f ms)\n", lat, 1000.0 * lat / 48000.0);
    check (lat > 1000 && lat < 48000 * 6, "with REPRO on the host is told head 1's delay");

    // bypass: the dry signal, delayed by exactly that
    {
        byp->setValueNotifyingHost (1.0f);
        std::vector<float> o, in;
        render (*inst, 20, nullptr, nullptr, t);    // past the 20 ms crossfade
        const int64_t t0 = t;
        render (*inst, 60, &o, &in, t);
        // the input t0 + i - lat is a sine we can compute
        const double amp = 0.12589254117941673 * std::sqrt (2.0);
        double worst = 0.0;
        for (size_t i = 0; i < o.size(); ++i)
        {
            const double want = amp * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) (t0 + (int64_t) i - lat) / 48000.0);
            worst = std::max (worst, std::abs (o[i] - want));
        }
        std::printf ("      bypassed: worst %.2e against the input delayed %d samples\n", worst, lat);
        check (worst < 1.0e-5, "BYPASS: the input itself, delayed by the latency the host was told");
        byp->setValueNotifyingHost (0.0f);
        if (repro) repro->setValueNotifyingHost (0.0f);
    }

    // a parameter through the host, and the state round trip
    auto* bias = byName (*inst, "BIAS");
    check (bias != nullptr, "BIAS found by name");
    if (bias)
    {
        bias->setValueNotifyingHost (0.25f);
        check (std::abs (bias->getValue() - 0.25f) < 1e-4f, "BIAS set to 0.25 reads back 0.25");
        std::printf ("      shows as %s\n", bias->getCurrentValueAsText().toRawUTF8());
        juce::MemoryBlock state;
        inst->getStateInformation (state);
        bias->setValueNotifyingHost (0.9f);
        inst->setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (bias->getValue() - 0.25f) < 1e-3f, "state saved with 0.25, moved to 0.9, restored: reads 0.25");
        std::printf ("      state is %d bytes\n", (int) state.getSize());
    }
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
