// KICKSTART panel snapshot. Renders the REAL editor to PNGs for a handful of
// presets, and measures what a picture alone would only suggest: every
// control is on the panel, none overlaps another, no label or value is cut
// off, and the hit display actually drew a kick.
//
//   ksshot <out-dir>

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    std::printf ("FAIL @%d: ", __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

namespace
{
    void shoot (juce::Component& c, const juce::File& out, float scale = 1.0f)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), false, scale);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }

    void walk (juce::Component& c, std::vector<juce::Component*>& into)
    {
        for (auto* k : c.getChildren()) { into.push_back (k); }
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    KickstartProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* ke = dynamic_cast<KickstartEditor*> (ed.get());
    CHECK (ke != nullptr, "no editor");
    if (ke == nullptr) return 1;
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    std::printf ("  editor %d x %d, %d knobs\n", ed->getWidth(), ed->getHeight(), ke->numKnobs());
    CHECK (ke->numKnobs() == ks::kNumParams - 2, "%d knobs for %d continuous parameters", ke->numKnobs(), ks::kNumParams - 2);

    // ---- geometry: on the panel, no overlaps, nothing cut off ---------------
    std::vector<juce::Component*> kids;
    walk (*ed, kids);
    const auto bounds = ed->getLocalBounds();
    int outside = 0, overlaps = 0, cut = 0;
    for (size_t i = 0; i < kids.size(); ++i)
    {
        auto* a = kids[i];
        if (! a->isVisible() || a->getWidth() == 0) continue;
        if (! bounds.contains (a->getBounds())) { ++outside; std::printf ("  outside: %s\n", a->getName().toRawUTF8()); }
        for (size_t j = i + 1; j < kids.size(); ++j)
        {
            auto* b = kids[j];
            if (! b->isVisible() || b->getWidth() == 0) continue;
            if (a->getBounds().intersects (b->getBounds()))
            {
                ++overlaps;
                std::printf ("  overlap: %s %s / %s %s\n", typeid (*a).name(), a->getBounds().toString().toRawUTF8(),
                             typeid (*b).name(), b->getBounds().toString().toRawUTF8());
            }
        }
        if (auto* l = dynamic_cast<juce::Label*> (a))
        {
            const float tw = juce::GlyphArrangement::getStringWidth (l->getFont(), l->getText());
            if (tw > (float) l->getWidth() - 2.0f) { ++cut; std::printf ("  label cut: %s\n", l->getText().toRawUTF8()); }
        }
        if (auto* s = dynamic_cast<juce::Slider*> (a))
        {
            //  the widest value this knob can show must fit its text box
            juce::String widest;
            for (double v : { s->getMinimum(), s->getMaximum(), (s->getMinimum() + s->getMaximum()) * 0.5 })
            {
                const auto t = s->getTextFromValue (v);
                if (t.length() > widest.length()) widest = t;
            }
            const float tw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (15.0f)), widest);
            if (tw > (float) juce::jmin (s->getTextBoxWidth(), s->getWidth()) - 4.0f)
            { ++cut; std::printf ("  value cut: %s in %d\n", widest.toRawUTF8(), s->getWidth()); }
            if (s->getHeight() < 80) { ++cut; std::printf ("  knob too small: %d px\n", s->getHeight()); }
        }
    }
    std::printf ("  %d components: %d outside, %d overlapping, %d cut off or too small\n",
                 (int) kids.size(), outside, overlaps, cut);
    CHECK (outside == 0 && overlaps == 0 && cut == 0, "the panel layout is broken");

    // ---- pictures, and the display actually draws the kick ---------------------
    const int shots[] = { 9, 7, 1, 21, 20, 24 };
    const char* names[] = { "909", "808", "studio", "gabber", "rumble", "trap" };
    for (int i = 0; i < 6; ++i)
    {
        proc.setCurrentProgram (shots[i]);
        ke->refreshDisplayNow();
        shoot (*ed, dir.getChildFile (juce::String ("panel-") + names[i] + ".png"));
    }
    //  the kick shows as hot-orange pixels in the display area
    proc.setCurrentProgram (9);
    ke->refreshDisplayNow();
    const auto img = ed->createComponentSnapshot ({ 16, 76, 760, 262 }, false, 1.0f);
    int hot = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
        {
            const auto c = img.getPixelAt (x, y);
            if (c.getRed() > 180 && c.getGreen() < 140 && c.getBlue() < 90) ++hot;
        }
    std::printf ("  the hit display: %d kick-coloured pixels\n", hot);
    CHECK (hot > 4000, "the display did not draw the kick");

    shoot (*ed, dir.getChildFile ("panel-2x.png"), 2.0f);
    std::printf ("\n%d checks - %s\n  wrote %s\n", checks, failures == 0 ? "ALL CLEAR" : "FAILURES",
                 dir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
