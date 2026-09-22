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

#include <functional>

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
    /*  WHERE THE LANES ARE, asked of the panel rather than remembered.

    A hard-coded rectangle drifted the moment the header grew; a scan for the
    hottest row then found the POSITION slider, whose track is the same ember
    as a live lane. Neither can say which band is a LANE.

    Every lane carries an effect menu with numEffects()+1 entries and nothing
    else on the panel does, so those menus are the lanes and their own bounds
    give each band. */
void findLanes (juce::Component& c, std::vector<juce::Rectangle<int>>& out)
{
    for (auto* k : c.getChildren())
    {
        if (auto* cb = dynamic_cast<juce::ComboBox*> (k))
            if (cb->getNumItems() == rop::numEffects() + 1)
            {
                const auto b = c.getLocalArea (cb, cb->getLocalBounds());
                out.push_back ({ 0, b.getY() - 4, 10000, b.getHeight() + 8 });
            }
        findLanes (*k, out);
    }
}

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

    /*  THE LANE MENU IS TWO FLOORS (§12a). The eighteen are the list and the
        World rack is behind one door — and the reason to check it HERE is
        that ComboBox finds a sub-menu item only because its iterators
        recurse. If that ever stopped being true, every wrapped module would
        silently become unselectable AND unreadable, with nothing else in
        the build to say so. */
    {
        std::vector<juce::ComboBox*> boxes;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* k : c.getChildren())
            {
                if (auto* cb = dynamic_cast<juce::ComboBox*> (k))
                    if (cb->getNumItems() == rop::numEffects() + 1) boxes.push_back (cb);
                walk (*k);
            }
        };
        walk (*ed);
        CHECK ((int) boxes.size() == rop::kSlots, "%d lane menus, not %d", (int) boxes.size(), rop::kSlots);

        if (! boxes.empty())
        {
            int top = 0, inWorld = 0;
            juce::String door;
            for (juce::PopupMenu::MenuItemIterator it (*boxes[0]->getRootMenu(), false); it.next();)
            {
                const auto& item = it.getItem();
                if (item.subMenu != nullptr) { door = item.text; inWorld = item.subMenu->getNumItems(); }
                else if (item.itemID != 0)   ++top;
            }
            std::printf ("  the lane menu            %d on top, then \"%s\" with %d\n",
                         top, door.toRawUTF8(), inWorld);
            CHECK (top == rop::numNativeEffects() + 1, "%d items above the door, not EMPTY plus the natives", top);
            CHECK (inWorld == rop::numEffects() - rop::numNativeEffects(),
                   "the World submenu holds %d of %d", inWorld, rop::numEffects() - rop::numNativeEffects());
            CHECK (door == "BROKILD WORLD FX", "the door is labelled \"%s\"", door.toRawUTF8());

            //  every wrapped module is selectable and reads back, submenu or not
            for (int t = rop::numNativeEffects(); t < rop::numEffects(); ++t)
            {
                boxes[0]->setSelectedId (t + 2, juce::dontSendNotification);
                CHECK (boxes[0]->getSelectedId() == t + 2,
                       "%s cannot be selected from the submenu", rop::effectDescriptor (t).id);
                CHECK (boxes[0]->getText() == rop::effectDescriptor (t).name,
                       "%s reads back as \"%s\"", rop::effectDescriptor (t).id,
                       boxes[0]->getText().toRawUTF8());
            }
            boxes[0]->setSelectedId (1, juce::dontSendNotification);
        }
    }

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
    std::vector<juce::Rectangle<int>> lanes;
    findLanes (*ed, lanes);
    std::sort (lanes.begin(), lanes.end(),
               [] (const juce::Rectangle<int>& a, const juce::Rectangle<int>& b)
               { return a.getY() < b.getY(); });
    CHECK ((int) lanes.size() == rop::kSlots, "found %d lanes on the panel, not %d",
           (int) lanes.size(), rop::kSlots);
    const auto lane0 = lanes.empty() ? juce::Rectangle<int> (0, 0, 1, 1)
                                     : lanes[0].withWidth (mid.getWidth());
    const int lit = emberPixels (mid, lane0);
    CHECK (lit > 200, "lane 1 shows no heat at 62 %% (%d ember pixels at y=%d)", lit, lane0.getY());
    std::printf ("  lane 1 at 62 %%: %d ember pixels (its band at y=%d)\n", lit, lane0.getY());

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
    //  the other half of the promise, measured in the same band
    const int cold = emberPixels (flat, lane0);
    CHECK (cold < 60, "a slot with A == B still shows %d ember pixels — the lane is "
                      "claiming a travel it cannot make", cold);
    std::printf ("  lane 1 with A == B: %d ember pixels\n", cold);
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

    // -- CAN THE BWFX EFFECTS BE SWITCHED ORDER? --------------------------------
    {
        auto& rk = proc.bwfxRack();
        const int n = bwfx::numModuleTypes();
        std::vector<int> before ((size_t) n), after ((size_t) n);

        //  the DN button on the FIRST pedal, found the way a user finds it
        juce::Button* dn = nullptr;
        std::function<void (juce::Component&)> findDn = [&] (juce::Component& c)
        {
            for (auto* k : c.getChildren())
            {
                if (auto* b = dynamic_cast<juce::Button*> (k))
                    if (b->getButtonText() == "DN") { dn = b; return; }
                findDn (*k);
                if (dn != nullptr) return;
            }
        };
        findDn (*ed);
        CHECK (dn != nullptr, "the BWFX rack has no reorder button");

        if (dn != nullptr)
        {
            rk.getOrder (before.data());
            if (dn->onClick) dn->onClick();
            rk.getOrder (after.data());
            std::printf ("  BWFX order: %s then %s  ->  %s then %s\n",
                         bwfx::moduleDescriptor (before[0]).name,
                         bwfx::moduleDescriptor (before[1]).name,
                         bwfx::moduleDescriptor (after[0]).name,
                         bwfx::moduleDescriptor (after[1]).name);
            CHECK (after[0] == before[1] && after[1] == before[0],
                   "DN did not swap the first two pedals in the rack's own order");
        }

        /*  and the order has to MATTER. TUBE then GRIT is a saturator feeding a
            crusher; GRIT then TUBE is a crusher feeding a saturator, and those
            are different sounds. If they measured the same, the reorder would
            be moving names around a list nothing reads. */
        int tube = -1, grit = -1;
        for (int t = 0; t < n; ++t)
        {
            const juce::String nm (bwfx::moduleDescriptor (t).name);
            if (nm == "TUBE") tube = t;
            if (nm == "GRIT") grit = t;
        }
        if (tube >= 0 && grit >= 0)
        {
            auto render = [&] (bool tubeFirst, std::vector<float>& out)
            {
                rk.clearState();
                /*  BY NAME, AND TO THE TOP OF ITS OWN RANGE. The first version of
                    this set parameter INDEX 1 on TUBE and index 0 on GRIT, which
                    are TONE and CRUSH - so it left TUBE at its default 8 dB of
                    drive and turned GRIT DOWN from 25 to 5. Both effects were then
                    close to inert, the swap measured 0.019, and that reads exactly
                    like reordering that does not work. Address a parameter by its
                    id, never by a remembered index (the BWFX macro lesson). */
                auto crank = [&] (int type, const char* id)
                {
                    const auto& dd = bwfx::moduleDescriptor (type);
                    for (int q = 0; q < dd.numParams; ++q)
                        if (juce::String (dd.params[q].id) == id)
                            { rk.setParam (type, q, dd.params[q].hi); return; }
                    CHECK (false, "no parameter %s on %s", id, dd.name);
                };
                rk.setEnabled (tube, true);  crank (tube, "drive");
                rk.setEnabled (grit, true);  crank (grit, "crush");
                std::vector<int> ord ((size_t) n);
                rk.getOrder (ord.data());
                //  put the two under test at the front, in the order asked for
                std::vector<int> want;
                want.push_back (tubeFirst ? tube : grit);
                want.push_back (tubeFirst ? grit : tube);
                for (int t2 = 0; t2 < n; ++t2)
                    if (t2 != tube && t2 != grit) want.push_back (t2);
                rk.setOrder (want.data(), n);

                proc.prepareToPlay (48000.0, 256);
                juce::AudioBuffer<float> buf (2, 256 * 40);
                juce::Random r (0x51ED);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < buf.getNumSamples(); ++i)
                        buf.setSample (ch, i, 0.35f * (r.nextFloat() * 2.0f - 1.0f));
                juce::MidiBuffer mb;
                for (int i = 0; i + 256 <= buf.getNumSamples(); i += 256)
                {
                    juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 2, i, 256);
                    proc.processBlock (blk, mb);
                }
                out.assign ((size_t) buf.getNumSamples(), 0.0f);
                for (int i = 0; i < buf.getNumSamples(); ++i) out[(size_t) i] = buf.getSample (0, i);
            };
            std::vector<float> a, b;
            render (true,  a);
            render (false, b);
            double num = 0.0, den = 0.0;
            for (size_t i = a.size() / 2; i < a.size(); ++i)
            {
                const double d = (double) a[i] - (double) b[i];
                num += d * d; den += (double) a[i] * a[i];
            }
            const double apart = std::sqrt (num / std::max (1.0e-20, den));
            std::printf ("  TUBE->GRIT against GRIT->TUBE: %.3f apart\n", apart);
            CHECK (apart > 0.02,
                   "swapping two BWFX effects changed nothing (%.4f) — the order is "
                   "being moved on the screen and nowhere else", apart);
            rk.clearState();
        }
    }

    std::printf ("\n%d checks, %d failed - %s\n", checks, failures, failures ? "SEE ABOVE" : "ALL CLEAR");
    return failures ? 1 : 0;
}
