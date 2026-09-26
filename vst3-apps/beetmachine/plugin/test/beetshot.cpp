/*  beetshot - render Beetmachine's REAL editor to PNG, because a panel is
    checked by looking at it. Also asserts the things a picture cannot be
    trusted to show: that the selected slot's own drum panel is actually
    there, fits its bay, and follows the selection.

      beetshot <output folder>                                               */

#include <JuceHeader.h>
#include "../src/BeetProcessor.h"
#include "../src/BeetEditor.h"

static int fails = 0;
static void check (bool ok, const juce::String& what)
{
    if (! ok) ++fails;
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
}

static void shoot (BeetEditor& ed, const juce::File& f, float scale = 1.0f)
{
    auto img = ed.contentComponent().createComponentSnapshot (ed.contentComponent().getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("  wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();

    BeetProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    std::unique_ptr<BeetEditor> ed (dynamic_cast<BeetEditor*> (p.createEditor()));
    ed->setSize (beetui::W, beetui::H);

    auto drumPanel = [&] () -> juce::Component* {
        for (auto* c : ed->contentComponent().getChildren())
            if (dynamic_cast<juce::AudioProcessorEditor*> (c) != nullptr) return c;
        return nullptr;
    };

    std::printf ("\nBEETMACHINE panel\n");
    ed->selectSlot (0);
    auto* k = drumPanel();
    check (k != nullptr && k->isVisible(), "slot 1 selected: the Kickstart panel is shown");
    if (k) check (k->getBounds().getBottom() <= beetui::H && k->getY() >= beetui::PANEL_Y && k->getRight() <= beetui::W,
                  "and it sits inside its bay (" + k->getBounds().toString() + ")");
    shoot (*ed, dir.getChildFile ("beet-kick.png"));
    shoot (*ed, dir.getChildFile ("beet-kick-2x.png"), 2.0f);     // for the manual's close-ups

    ed->selectSlot (4);
    auto* h = drumPanel();
    check (h != nullptr && h != k, "slot 5 selected: a different drum panel (Hats Off) replaces it");
    if (h) check (h->getBounds().getBottom() <= beetui::H && h->getRight() <= beetui::W, "and it fits (" + h->getBounds().toString() + ")");
    shoot (*ed, dir.getChildFile ("beet-hats.png"));
    shoot (*ed, dir.getChildFile ("beet-hats-2x.png"), 2.0f);

    p.setSlotType (7, beet::EMPTY);
    ed->selectSlot (7);
    check (drumPanel() == nullptr, "an empty slot shows no drum panel");
    {
        //  an empty card hides what it cannot use, or its "empty bay" text
        //  lands on top of disabled knobs (it did, 2026-09-26)
        std::vector<SlotCard*> cs;
        for (auto* c : ed->contentComponent().getChildren())
            if (auto* sc = dynamic_cast<SlotCard*> (c)) cs.push_back (sc);
        auto shown = [] (juce::Component* c) { int n = 0; for (auto* k : c->getChildren()) n += k->isVisible() ? 1 : 0; return n; };
        check (cs.size() == (size_t) beet::NUM_SLOTS, "eight slot cards");
        if (cs.size() == (size_t) beet::NUM_SLOTS)
            check (shown (cs[7]) + 8 == shown (cs[6]), "the empty card hides its preset row, pad, knobs and M/S ("
                   + juce::String (shown (cs[7])) + " shown against " + juce::String (shown (cs[6])) + ")");
    }
    shoot (*ed, dir.getChildFile ("beet-empty.png"));

    //  change a slot's type while its panel is open: the old panel must go
    //  before the old drum does (this is where a dangling editor would crash)
    ed->selectSlot (2);
    p.setSlotType (2, beet::HATS);
    ed->selectSlot (2);
    check (drumPanel() != nullptr, "changing the selected slot's drum swaps its panel without crashing");

    //  --- what a DAW session does, each followed by "is the panel there?" ---
    //  (Peter saw "SLOT 1 IS EMPTY" on a Kickstart slot in the host.)
    std::printf ("\nsession replay\n");
    auto panelFor = [&] (int s, const char* what) {
        ed->selectSlot (s);
        auto* d = drumPanel();
        check (d != nullptr, juce::String (what) + ": slot " + juce::String (s + 1) + " shows its drum's panel");
    };
    panelFor (0, "first open");
    p.setSlotPreset (0, 8);                                  panelFor (0, "after a preset change");
    p.loadKit (1);                                           panelFor (0, "after loading another kit");
    p.setSlotType (0, beet::EMPTY);  ed->selectSlot (0);
    p.setSlotType (0, beet::KICK);                           panelFor (0, "emptied and refilled");
    {
        juce::MemoryBlock mb; p.getStateInformation (mb);
        p.setStateInformation (mb.getData(), (int) mb.getSize());
    }                                                        panelFor (0, "after the project reloads its state");
    ed.reset();                                              // the DAW closes the window
    ed.reset (dynamic_cast<BeetEditor*> (p.createEditor()));
    ed->setSize (beetui::W, beetui::H);                      panelFor (0, "window closed and reopened");
    panelFor (4, "then another slot");
    panelFor (0, "and back");
    {
        //  the host reloads state while the window is CLOSED, then opens it
        juce::MemoryBlock mb; p.getStateInformation (mb);
        ed.reset();
        p.setStateInformation (mb.getData(), (int) mb.getSize());
        ed.reset (dynamic_cast<BeetEditor*> (p.createEditor()));
        ed->setSize (beetui::W, beetui::H);
    }                                                        panelFor (0, "state loaded with the window closed");
    for (int s = 0; s < beet::NUM_SLOTS; ++s)                panelFor (s, "every slot in turn");

    ed.reset();
    std::printf ("\n%s\n\n", fails == 0 ? "ALL CLEAR" : "FAILURES");
    return fails == 0 ? 0 : 1;
}
