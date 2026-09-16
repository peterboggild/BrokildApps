// LEGION panel snapshot. Legion has no standalone and no web panel, so the
// only way to judge the editor is to render it and look at it — the house
// lesson, paid for several times over: a control can be working and still be
// invisible, and grep cannot see a layer that paints nothing.
//
// Renders the real LegionEditor twice, rack closed and rack open, and writes
// two PNGs. It also MEASURES the thing that went wrong once already: with the
// rack open, no control belonging to the main panel may still be visible
// underneath it. That check is the regression gate; the pictures are for eyes.
//
//   legionshot <out-dir>

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    //  Every component that belongs to the main panel, i.e. everything the
    //  rack overlay is supposed to cover. Found by walking the editor's own
    //  children rather than by listing them, so a control added later is
    //  covered by this test without anyone remembering to add it.
    void collectMainPanel (juce::Component& editor, juce::Component* overlay,
                           std::vector<juce::Component*>& into)
    {
        for (auto* c : editor.getChildren())
            if (c != overlay)
                into.push_back (c);
    }

    juce::TextButton* findRackButton (juce::Component& editor)
    {
        for (auto* c : editor.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText().containsIgnoreCase ("BWFX"))
                    return b;
        return nullptr;
    }

    void shoot (juce::Component& c, const juce::File& out)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), false, 1.0f);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;

    const juce::File dir = argc > 1 ? juce::File (juce::String (argv[1]))
                                    : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    LegionProcessor proc;
    proc.prepareToPlay (48000.0, 256);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    CHECK (ed != nullptr, "no editor");
    if (ed == nullptr) return 1;

    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    std::printf ("  editor %d x %d\n", ed->getWidth(), ed->getHeight());

    auto* rackButton = findRackButton (*ed);
    CHECK (rackButton != nullptr, "no BWFX button on the panel");
    if (rackButton == nullptr) return 1;

    //  ---- closed ---------------------------------------------------------
    shoot (*ed, dir.getChildFile ("panel-rack-closed.png"));

    //  ---- open -----------------------------------------------------------
    rackButton->setToggleState (true, juce::sendNotificationSync);

    //  the overlay is whatever the editor put on top; take the last visible
    //  child, which is what JUCE paints last
    juce::Component* overlay = nullptr;
    for (auto* c : ed->getChildren())
        if (c->isVisible() && c != rackButton)
            overlay = c;

    CHECK (overlay != nullptr, "nothing became visible when BWFX was pressed");
    shoot (*ed, dir.getChildFile ("panel-rack-open.png"));

    //  ---- the regression gate -------------------------------------------
    //  With the rack open, the main panel must be COVERED. Two independent
    //  ways for that to be true, and either is fine: the controls are hidden,
    //  or an opaque child of the editor is painted over them.
    std::vector<juce::Component*> main;
    collectMainPanel (*ed, overlay, main);

    int stillShowing = 0;
    for (auto* c : main)
        if (c->isVisible() && c != rackButton)
            ++stillShowing;

    const bool covers = overlay != nullptr
                        && overlay->isOpaque()
                        && overlay->getBounds().contains (ed->getLocalBounds());

    std::printf ("  rack open: overlay %s, %d main-panel control(s) still visible\n",
                 covers ? "opaque and full-bleed" : "NOT covering", stillShowing);

    CHECK (covers || stillShowing == 0,
           "the rack does not cover the panel: %d main controls show through",
           stillShowing);

    //  the rack must also actually be on top of the paint order
    if (overlay != nullptr)
        CHECK (ed->getIndexOfChildComponent (overlay) == ed->getNumChildComponents() - 1,
               "the rack is not the last child, so the panel paints over it");

    //  ---- and it must go away again --------------------------------------
    rackButton->setToggleState (false, juce::sendNotificationSync);
    CHECK (overlay == nullptr || ! overlay->isVisible(),
           "the rack is still visible after closing");
    shoot (*ed, dir.getChildFile ("panel-rack-closed-again.png"));

    std::printf ("\n%d checks — %s\n", checks,
                 failures == 0 ? "ALL CLEAR" : "FAILURES");
    std::printf ("  wrote %s\n", dir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
