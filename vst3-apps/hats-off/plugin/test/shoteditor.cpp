// HATS OFF panel snapshot. Renders the REAL editor to PNGs for a handful
// of presets, and measures what a picture alone would only suggest: every
// control is on the panel, none overlaps another, no label or value is cut
// off, and the hit display actually drew both the head and the wires.
//
//   stshot <out-dir>

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
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    HatsOffProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* se = dynamic_cast<HatsOffEditor*> (ed.get());
    CHECK (se != nullptr, "no editor");
    if (se == nullptr) return 1;
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    int continuous = 0;
    for (int i = 0; i < ho::kNumParams; ++i) if (ho::specs()[i].kind == ho::K_FLOAT) ++continuous;
    std::printf ("  editor %d x %d, %d knobs\n", ed->getWidth(), ed->getHeight(), se->numKnobs());
    CHECK (se->numKnobs() == continuous, "%d knobs for %d continuous parameters", se->numKnobs(), continuous);

    // ---- geometry: on the panel, no overlaps, nothing cut off ---------------
    std::vector<juce::Component*> kids;
    for (auto* k : ed->getChildren()) kids.push_back (k);
    const auto bounds = ed->getLocalBounds();
    int outside = 0, overlaps = 0, cut = 0;
    for (size_t i = 0; i < kids.size(); ++i)
    {
        auto* a = kids[i];
        if (! a->isVisible() || a->getWidth() == 0) continue;
        if (! bounds.contains (a->getBounds())) { ++outside; std::printf ("  outside: %s %s\n", typeid (*a).name(), a->getBounds().toString().toRawUTF8()); }
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
            juce::String widest;
            for (double v : { s->getMinimum(), s->getMaximum(), (s->getMinimum() + s->getMaximum()) * 0.5, s->getMaximum() * 0.99 })
            {
                const auto t = s->getTextFromValue (v);
                if (t.length() > widest.length()) widest = t;
            }
            const float tw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (15.0f)), widest);
            if (tw > (float) juce::jmin (s->getTextBoxWidth(), s->getWidth()) - 4.0f)
            { ++cut; std::printf ("  value cut: %s in %d\n", widest.toRawUTF8(), s->getWidth()); }
            if (s->getHeight() < 80) { ++cut; std::printf ("  knob too small: %d px\n", s->getHeight()); }
        }
        if (auto* c = dynamic_cast<juce::ComboBox*> (a))
        {
            //  every item must fit its box, not only the one showing
            const auto f = juce::Font (juce::FontOptions ((float) (c->getHeight() > 30 ? 17.0f : 14.0f), juce::Font::bold));
            for (int k = 0; k < c->getNumItems(); ++k)
            {
                const float tw = juce::GlyphArrangement::getStringWidth (f, c->getItemText (k));
                if (tw > (float) c->getWidth() - 30.0f) { ++cut; std::printf ("  menu item cut: %s in %d\n", c->getItemText (k).toRawUTF8(), c->getWidth()); }
            }
        }
        if (auto* b = dynamic_cast<juce::TextButton*> (a))
        {
            const float tw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (juce::jmin (15.0f, b->getHeight() * 0.6f))), b->getButtonText());
            if (tw > (float) b->getWidth() - 4.0f) { ++cut; std::printf ("  button text cut: %s in %d\n", b->getButtonText().toRawUTF8(), b->getWidth()); }
        }
    }
    std::printf ("  %d components: %d outside, %d overlapping, %d cut off or too small\n",
                 (int) kids.size(), outside, overlaps, cut);
    CHECK (outside == 0 && overlaps == 0 && cut == 0, "the panel layout is broken");

    // ---- pictures, and the display actually draws the cymbal ---------------------
    const char* shots[] = { "Studio Hi-Hat 14", "808 Open Hat", "Jazz Ride 20", "Crash 18", "China 18", "909 Closed Hat", "Dub Hat Echo", "Ride Bell" };
    const char* names[] = { "hat", "808open", "ride", "crash", "china", "909", "dub", "bell" };
    for (int i = 0; i < 8; ++i)
    {
        const int idx = ho::presetByName (shots[i]);
        CHECK (idx >= 0, "no preset %s", shots[i]);
        proc.setCurrentProgram (idx);
        se->refreshDisplayNow();
        shoot (*ed, dir.getChildFile (juce::String ("panel-") + names[i] + ".png"));
        {
            const auto d = ed->createComponentSnapshot (se->displayBounds(), false, 2.0f);
            const auto out = dir.getChildFile (juce::String ("hit-") + names[i] + ".png");
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat().writeImageToStream (d, os);
        }
    }
    //  the hit shows in brass, its brightness curve in steel
    proc.setCurrentProgram (ho::presetByName ("Crash 18"));
    se->refreshDisplayNow();
    const auto img = ed->createComponentSnapshot (se->displayBounds(), false, 1.0f);
    int brass = 0, steel = 0;
    for (int y = 40; y < img.getHeight() - 20; ++y)
        for (int x = 0; x < img.getWidth() - 40; ++x)
        {
            const auto c = img.getPixelAt (x, y);
            if (c.getRed() > 170 && c.getGreen() > 110 && c.getBlue() < 100) ++brass;
            if (c.getBlue() > 170 && c.getRed() < 150) ++steel;
        }
    std::printf ("  the hit display: %d brass pixels, %d brightness-curve pixels\n", brass, steel);
    CHECK (brass > 3000 && steel > 300, "the display did not draw the hit and its brightness");

    shoot (*ed, dir.getChildFile ("panel-2x.png"), 2.0f);
    std::printf ("\n%d checks - %s\n  wrote %s\n", checks, failures == 0 ? "ALL CLEAR" : "FAILURES",
                 dir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
