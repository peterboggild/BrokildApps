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
        /*  GIVE IT SOMEWHERE TO GO. setSlotEffect seeds A and B with the
            effect's own defaults, so a slot straight out of the menu has
            A == B and cannot travel however the lane is drawn — which is
            the fault this panel round exists to make visible. A shot that
            never moves B is a shot of six inert lanes. */
        const auto& d = rop::effectDescriptor (i);
        for (int p = 0; p < d.numParams; ++p)
            s.B[p] = (s.A[p] >= d.params[p].hi) ? d.params[p].lo : d.params[p].hi;
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
    //  the lane geometry, from the panel's own constants (kInset, kHeadH,
    //  kGlobH, kColH, kLaneH) — a hard-coded rectangle silently measured the
    //  wrong strip the moment the layout moved
    const int laneY = 128 + 78 + 15;
    const auto lane0 = juce::Rectangle<int> (42, laneY, ed->getWidth() - 84, 44);
    const int lit = emberPixels (mid, lane0);
    CHECK (lit > 200, "lane 0 shows no heat at 62 %% (%d ember pixels)", lit);
    std::printf ("  lane 0 at 62 %%: %d ember pixels\n", lit);

    /*  The other half of the same promise, and the one the round was for:
        a slot whose A and B are identical must show NO heat at all, however
        far the marker has gone past its ENTER. Without this check the panel
        could go back to lighting a lane that cannot move and nothing would
        say so — which is exactly how the fault shipped in the first place. */
    proc.rack().setSlotEffect (0, 0);               // fresh: A == B again
    {
        auto& s = proc.rack().state (0);
        s.enter = 0.0f; s.exit = 1.0f;
    }
    ed.reset (proc.createEditor());
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    if (auto* p = proc.apvts.getParameter (rop_ids::position))
        p->setValueNotifyingHost (0.62f);
    const auto flat = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    const int cold = emberPixels (flat, lane0);
    CHECK (cold < 50, "a slot with A == B still shows %d ember pixels — the lane is "
                      "claiming a travel it cannot make", cold);
    std::printf ("  lane 0 with A == B: %d ember pixels\n", cold);
    {
        juce::File f = dir.getChildFile ("panel-flat.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png; png.writeImageToStream (flat, os);
        std::printf ("  wrote panel-flat.png\n");
    }

    /*  THE BWFX RACK. It shipped once as a face with five macros and nothing
        else, and nothing here would have noticed — so this opens it and
        measures that it is FULL: teal ink (the rack's one mandated accent)
        across both columns, and specifically on the RIGHT half, because the
        fault Peter found was a panel with a left side and no SPECTRA. */
    {
        //  find the BWFX button by its text and click it
        juce::Button* bwfx = nullptr;
        std::function<void (juce::Component&)> hunt = [&] (juce::Component& c)
        {
            for (auto* k : c.getChildren())
            {
                if (auto* b = dynamic_cast<juce::Button*> (k))
                    if (b->getButtonText() == "BWFX") { bwfx = b; return; }
                hunt (*k);
                if (bwfx != nullptr) return;
            }
        };
        hunt (*ed);

        /*  Arm one of each before opening, so the shot is of a rack somebody
            would actually have and the checks can see a PRESENCE slider. An
            empty rack is the honest default but it proves the least. */
        if (bwfx != nullptr)
        {
            for (int t = 0; t < bwfx::numModuleTypes(); ++t)
                if (juce::String (bwfx::moduleDescriptor (t).name) == "ECHO")
                    proc.bwfxRack().setEnabled (t, true);
            for (int c = 0; c < bwfx::numCharacters(); ++c)
                if (juce::String (bwfx::characterDescriptor (c).name) == "TAPE SEANCE")
                    proc.bwfxRack().setCharArmed (c, true);
        }
        CHECK (bwfx != nullptr, "there is no BWFX button on the panel");
        if (bwfx != nullptr)
        {
            /*  NOT triggerClick(): it posts through MessageManager::callAsync
                and nothing here runs a loop to deliver it, so the snapshot came
                out showing the main panel and the check measured that. The
                button's own onClick is the click, synchronously. */
            if (bwfx->onClick) bwfx->onClick();
            const auto shot = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
            juce::File f = dir.getChildFile ("panel-bwfx.png");
            f.deleteFile();
            { juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (shot, os); }
            std::printf ("  wrote panel-bwfx.png\n");

            /*  ASH, not teal. Teal is the accent on presence sliders and the
                reorder buttons, so an un-armed column has almost none of it and
                a teal count calls a perfectly correct panel empty. The module
                NAMES are ash and are there either way, which is what "this
                column has content" actually means. */
            auto inkIn = [&] (juce::Rectangle<int> r, bool teal)
            {
                int n = 0;
                for (int y = r.getY(); y < r.getBottom(); y += 2)
                    for (int x = r.getX(); x < r.getRight(); x += 2)
                    {
                        const auto c = shot.getPixelAt (x, y);
                        if (teal) { if (c.getGreen() > 120 && c.getBlue() > 110 && c.getRed() < 110) ++n; }
                        else      { if (c.getRed() > 170 && c.getGreen() > 160 && c.getBlue() > 140) ++n; }
                    }
                return n;
            };
            const int w = shot.getWidth(), h = shot.getHeight();
            const juce::Rectangle<int> lc { 0, 90, w / 2, h - 200 }, rc { w / 2, 90, w / 2, h - 200 };
            const int leftInk = inkIn (lc, false), rightInk = inkIn (rc, false);
            const int leftTeal = inkIn (lc, true), rightTeal = inkIn (rc, true);
            std::printf ("  BWFX rack: FX column %d names / %d teal, SPECTRA %d names / %d teal\n",
                         leftInk, leftTeal, rightInk, rightTeal);
            CHECK (leftInk  > 400, "the BWFX rack's FX column is empty (%d ink)", leftInk);
            CHECK (rightInk > 250, "the BWFX rack has no SPECTRA column (%d ink) — that was "
                                   "the whole fault Peter reported", rightInk);
            //  and arming something has to put a PRESENCE slider on both sides
            CHECK (leftTeal  > 200, "arming an FX put no teal control in its column (%d)", leftTeal);
            CHECK (rightTeal > 120, "arming a character put no teal control in its column (%d)", rightTeal);
        }
    }

    std::printf ("\n%d checks, %d failed - %s\n", checks, failures, failures ? "SEE ABOVE" : "ALL CLEAR");
    return failures ? 1 : 0;
}
