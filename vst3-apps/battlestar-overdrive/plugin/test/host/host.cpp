/*  What does a DAW actually see?

    Peter: "i cannot see the two hidden controls in the automation panel."

    The bench drives the ENGINE and the live probe drives the PANEL; neither
    touches the one link that matters for that question - the VST3 wrapper as a
    host loads it. This is that link: it scans the INSTALLED bundle (never the
    build output - a DAW opens the installed file and so does this),
    instantiates it, and prints the parameter list in a host's own terms:
    index, id, name, whether it is automatable, and its current value.

      bohost                  the installed bundle
      bohost <path.vst3>
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File house ("C:/Program Files/Common Files/VST3/Brokild/Brokild collection");
    const juce::String path = argc > 1 ? juce::String (argv[1])
                                       : house.getChildFile ("Battlestar Overdrive.vst3").getFullPathName();

    std::printf ("scanning %s\n", path.toRawUTF8());

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);        // JUCE 8: addDefaultFormats() is deleted

    juce::KnownPluginList list;
    juce::OwnedArray<juce::PluginDescription> found;
    bool scanned = false;
    for (int i = 0; i < fm.getNumFormats(); ++i)
        if (auto* f = fm.getFormat (i))
            if (f->getName().containsIgnoreCase ("VST3"))
                scanned = list.scanAndAddFile (path, true, found, *f) || scanned;

    if (found.isEmpty())
    {
        std::printf ("FAILED: the host could not scan that bundle at all\n");
        return 1;
    }

    juce::String err;
    auto inst = fm.createPluginInstance (*found[0], 48000.0, 512, err);
    if (inst == nullptr)
    {
        std::printf ("FAILED to instantiate: %s\n", err.toRawUTF8());
        return 1;
    }
    inst->prepareToPlay (48000.0, 512);

    std::printf ("\n%s  —  %s\n", inst->getName().toRawUTF8(),
                 found[0]->pluginFormatName.toRawUTF8());
    std::printf ("the host is offered %d parameters:\n\n", inst->getParameters().size());
    std::printf ("  %-3s %-16s %-12s %s\n", "#", "name", "automatable", "value");

    int automatable = 0;
    bool sawWet = false, sawSync = false;
    int idx = 0;
    for (auto* p : inst->getParameters())
    {
        /*  Matched on NAME. Hosting a VST3 gives back JUCE's own host-side
            wrapper parameters, not the plug-in's AudioProcessorParameterWithID
            objects - so the id cast is always null here, and an id-based check
            reported BOTH new parameters "MISSING" while printing them in the
            list two lines above. A check that contradicts its own output is
            worse than no check. */
        const juce::String nm = p->getName (32);

        const bool autoOk = p->isAutomatable();
        if (autoOk) ++automatable;
        if (nm.equalsIgnoreCase ("SPACE WET"))  sawWet  = true;
        if (nm.equalsIgnoreCase ("SPACE SYNC")) sawSync = true;

        std::printf ("  %-3d %-16s %-12s %s\n",
                     idx++,
                     nm.toRawUTF8(),
                     autoOk ? "yes" : "NO",
                     p->getCurrentValueAsText().toRawUTF8());
    }

    std::printf ("\n  %d of %d are automatable\n", automatable, inst->getParameters().size());
    std::printf ("  spacewet  %s\n", sawWet  ? "PRESENT" : "MISSING");
    std::printf ("  spacesync %s\n", sawSync ? "PRESENT" : "MISSING");

    /*  And prove they are live, not merely listed: move one and read it back
        through the host's own parameter object. */
    for (auto* p : inst->getParameters())
        if (p->getName (32).equalsIgnoreCase ("SPACE WET"))
            {
                p->setValueNotifyingHost (0.85f);
                std::printf ("  set spacewet to 0.85 -> host reads %.3f (%s)\n",
                             p->getValue(), p->getCurrentValueAsText().toRawUTF8());
            }

    inst->releaseResources();
    inst.reset();
    return (sawWet && sawSync) ? 0 : 1;
}
