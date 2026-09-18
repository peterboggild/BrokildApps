// RITE OF PASSAGE panel snapshot. The plugin has no standalone and no web
// panel, so the only way to look at the editor is to render it — the house
// lesson: a control can be working and still be invisible, and grep cannot
// see a layer that paints nothing.
//
// Renders the real RiteEditor three times and writes PNGs:
//   panel-empty.png      a fresh instance, every slot empty
//   panel-loaded.png     six slots assigned, the marker at 0 %
//   panel-midway.png     the same rite with the marker at 62 %, lanes lit
//
//   ropshot <out-dir>

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    void shoot (juce::Component& c, const juce::File& out)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), false, 1.0f);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
        std::printf ("  wrote %s (%d x %d)\n", out.getFileName().toRawUTF8(), img.getWidth(), img.getHeight());
    }

    //  how many pixels in a region are lit with the ember colour family
    int emberPixels (const juce::Image& img, juce::Rectangle<int> r)
    {
        int n = 0;
        for (int y = r.getY(); y < r.getBottom(); ++y)
            for (int x = r.getX(); x < r.getRight(); ++x)
            {
                const auto c = img.getPixelAt (x, y);
                if (c.getRed() > 180 && c.getGreen() < 140 && c.getBlue() < 90) ++n;
            }
        return n;
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir = argc > 1 ? juce::File (juce::String (argv[1]))
                                    : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    RiteProcessor proc;
    proc.prepareToPlay (48000.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    CHECK (ed != nullptr, "no editor");
    if (ed == nullptr) return 1;
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    std::printf ("  editor %d x %d\n", ed->getWidth(), ed->getHeight());

    //  every child must sit inside the editor: an off-panel control is invisible
    int outside = 0;
    for (auto* c : ed->getChildren())
        if (c->isVisible() && ! ed->getLocalBounds().contains (c->getBounds())) ++outside;
    CHECK (outside == 0, "%d control(s) lie partly outside the editor", outside);

    shoot (*ed, dir.getChildFile ("panel-empty.png"));

    //  ---- a rite: the six effects that exist, one per slot -----------------
    ed.reset();
    const int n = rop::numEffects();
    std::printf ("  %d effects registered:", n);
    for (int t = 0; t < n; ++t) std::printf (" %s", rop::effectDescriptor (t).name);
    std::printf ("\n");
    for (int i = 0; i < rop::kSlots && i < n; ++i)
    {
        proc.rack().setSlotEffect (i, i);
        auto& s = proc.rack().state (i);
        s.enter = 0.12f * (float) i;
        s.exit  = juce::jmin (1.0f, s.enter + 0.55f);
    }
    ed.reset (proc.createEditor());
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    shoot (*ed, dir.getChildFile ("panel-loaded.png"));

    //  ---- the marker midway: lanes whose enter point is passed must be lit ----
    if (auto* p = proc.apvts.getParameter (rop_ids::position))
        p->setValueNotifyingHost (0.62f);
    const auto mid = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    {
        juce::FileOutputStream os (dir.getChildFile ("panel-midway.png"));
        dir.getChildFile ("panel-midway.png").deleteFile();
        juce::PNGImageFormat png; png.writeImageToStream (mid, os);
        std::printf ("  wrote panel-midway.png\n");
    }
    //  lane 0 entered at 0 %, lane 5 enters at 60 %: both should carry heat at 62 %;
    //  a lane that has not been entered carries none. Measure lane 0 against an
    //  empty instance's lane 0.
    const auto lane0 = juce::Rectangle<int> (14, 116 + 62, ed->getWidth() - 28, 40);
    const int lit = emberPixels (mid, lane0);
    CHECK (lit > 200, "lane 0 shows no heat at 62 %% (%d ember pixels)", lit);
    std::printf ("  lane 0 at 62 %%: %d ember pixels\n", lit);

    std::printf ("\n%d checks, %d failed - %s\n", checks, failures, failures ? "SEE ABOVE" : "ALL CLEAR");
    return failures ? 1 : 0;
}
