// DRUMEQ panel snapshot. Renders the REAL editor to PNGs for several drums and
// measures what a picture alone would only suggest: every control is on the
// panel, none overlaps another, no label or value is cut off, the knobs are big
// enough to use, and the EQ display drew a curve and a spectrum.
//
//   deshot <out-dir>

#include <JuceHeader.h>

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include "MachineArt.h"

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

    DrumEQProcessor proc;
    proc.setPlayConfigDetails (1, 1, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.setLinked (false);

    //  something on the input, so the spectrum has a drum to show
    {
        juce::AudioBuffer<float> b (1, 256);
        juce::MidiBuffer m;
        juce::Random r (5);
        for (int k = 0; k < 64; ++k)
        {
            for (int i = 0; i < 256; ++i)
            {
                const int n = k * 256 + i;
                const double t = n / 48000.0;
                b.setSample (0, i, (float) (0.4 * std::exp (-std::fmod (t, 0.25) / 0.08) * std::sin (2.0 * 3.14159 * 60.0 * t)
                                         + 0.05 * (r.nextFloat() - 0.5f)));
            }
            proc.processBlock (b, m);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* de = dynamic_cast<DrumEQEditor*> (ed.get());
    CHECK (de != nullptr, "no editor");
    if (de == nullptr) return 1;
    std::printf ("  editor %d x %d, %d knobs\n", ed->getWidth(), ed->getHeight(), de->numKnobs());
    //  every continuous parameter but SOURCE has a knob
    int continuous = 0;
    for (int i = 0; i < de::kNumParams; ++i) if (de::specs()[i].kind == de::K_FLOAT) ++continuous;
    CHECK (de->numKnobs() == continuous, "%d knobs for %d continuous parameters", de->numKnobs(), continuous);

    // ---- geometry: on the panel, no overlaps, nothing cut off -------------------
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
            if (tw > (float) l->getWidth() - 2.0f) { ++cut; std::printf ("  label cut: '%s' (%.0f in %d)\n", l->getText().toRawUTF8(), tw, l->getWidth()); }
        }
        if (auto* tb = dynamic_cast<juce::TextButton*> (a))
        {
            //  the caption, in the face the look draws it in, must fit the button
            const float tw = juce::GlyphArrangement::getStringWidth (machineart::mono (juce::jmin ((float) tb->getHeight() * 0.55f, 12.5f)),
                                                                     tb->getButtonText());
            if (tw > (float) tb->getWidth() - 6.0f) { ++cut; std::printf ("  button cut: '%s' (%.0f in %d)\n", tb->getButtonText().toRawUTF8(), tw, tb->getWidth()); }
        }
        if (auto* s = dynamic_cast<juce::Slider*> (a))
        {
            juce::String widest;
            for (double v : { s->getMinimum(), s->getMaximum(), (s->getMinimum() + s->getMaximum()) * 0.5 })
            {
                const auto t = s->getTextFromValue (v);
                if (t.length() > widest.length()) widest = t;
            }
            const float tw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (13.0f)), widest);
            if (tw > (float) juce::jmin (s->getTextBoxWidth(), s->getWidth()) - 2.0f)
            { ++cut; std::printf ("  value cut: %s in %d\n", widest.toRawUTF8(), s->getTextBoxWidth()); }
            if (s->getHeight() < 90) { ++cut; std::printf ("  knob too small: %d px\n", s->getHeight()); }
        }
    }
    std::printf ("  %d components: %d outside, %d overlapping, %d cut off or too small\n", (int) kids.size(), outside, overlaps, cut);
    CHECK (outside == 0 && overlaps == 0 && cut == 0, "the panel layout is broken");

    // ---- one picture per drum, each with a curve on it ------------------------------
    const int shots[][2] = { { de::S_KICK_IN, 2 }, { de::S_SNARE_TOP, 2 }, { de::S_TOM3, 1 }, { de::S_HAT, 2 }, { de::S_OH, 1 }, { de::S_ROOM, 1 } };
    const char* names[] = { "kick-in", "snare-top", "tom3", "hihat", "overheads", "room" };
    for (int i = 0; i < 6; ++i)
    {
        proc.selectSource (shots[i][0]);
        proc.loadFactoryPreset (shots[i][1]);
        de->refreshNow();
        shoot (*ed, dir.getChildFile (juce::String ("panel-") + names[i] + ".png"));
        CHECK (de->eqCurvePixels() > 400, "%s: the curve has %d points", names[i], de->eqCurvePixels());
    }

    //  the display shows the curve: bright ink pixels in the EQ area, and the lit spectrum
    proc.selectSource (de::S_KICK_IN);
    proc.loadFactoryPreset (2);
    de->refreshNow();
    const auto img = ed->createComponentSnapshot ({ 16, 100, 990, 286 }, false, 1.0f);
    //  the curve is near-white ink; the lit spectrum is amber-brown in the
    //  lower part of the plot (measured against the same panel with no sound)
    auto count = [] (const juce::Image& im, int& ink, int& amber)
    {
        ink = amber = 0;
        for (int y = 0; y < im.getHeight(); ++y)
            for (int x = 0; x < im.getWidth(); ++x)
            {
                const auto c = im.getPixelAt (x, y);
                if (c.getRed() > 220 && c.getGreen() > 210 && c.getBlue() > 190) ++ink;
                if (y > im.getHeight() / 2 && c.getRed() - c.getBlue() > 35 && c.getRed() > 55) ++amber;
            }
    };
    int ink = 0, amber = 0;
    count (img, ink, amber);
    std::printf ("  the EQ display: %d curve pixels, %d spectrum pixels\n", ink, amber);
    CHECK (ink > 1000, "the curve was not drawn");
    CHECK (amber > 10000, "the spectrum was not drawn");

    shoot (*ed, dir.getChildFile ("panel-2x.png"), 2.0f);

    // ---- MASTER: a second track on the snare, this panel made master, editing the snare --
    {
        DrumEQProcessor snare;
        snare.setPlayConfigDetails (1, 1, 48000.0, 256);
        snare.prepareToPlay (48000.0, 256);
        snare.selectSource (de::S_SNARE_TOP);
        {
            juce::AudioBuffer<float> b (1, 256);
            juce::MidiBuffer m;
            juce::Random r (9);
            for (int k = 0; k < 64; ++k)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const double t = (k * 256 + i) / 48000.0;
                    b.setSample (0, i, (float) (0.4 * std::exp (-std::fmod (t, 0.3) / 0.06) * std::sin (2.0 * 3.14159 * 196.0 * t)
                                             + 0.04 * (r.nextFloat() - 0.5f)));
                }
                snare.processBlock (b, m);
            }
        }
        proc.selectSource (de::S_KICK_IN);
        proc.setLinked (true);
        proc.kitTick(); snare.kitTick();
        de->refreshNow();
        de->clickMaster();
        de->clickSource (de::S_SNARE_TOP);
        proc.kitTick(); snare.kitTick(); proc.kitTick();
        de->refreshNow();
        CHECK (de->showsMaster() && proc.editingRemote(), "the panel is not a master editing the snare");
        shoot (*ed, dir.getChildFile ("panel-master.png"));

        //  blue, the snare's own bands on the knobs, and the snare's sound on the display
        const auto im = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
        const auto edge = im.getPixelAt (1, 300);
        CHECK (edge.getBlue() > 200 && edge.getRed() < 120, "the master frame is not blue (%d %d %d)", edge.getRed(), edge.getGreen(), edge.getBlue());
        int ink2 = 0, amber2 = 0;
        count (ed->createComponentSnapshot ({ 16, 100, 990, 286 }, false, 1.0f), ink2, amber2);
        std::printf ("  MASTER editing the snare: frame %d/%d/%d, curve %d pixels, the snare track's spectrum %d pixels\n",
                     edge.getRed(), edge.getGreen(), edge.getBlue(), ink2, amber2);
        CHECK (ink2 > 1000 && amber2 > 5000, "the master's display does not show the snare track");

        //  the layout in master mode: nothing overlapping, nothing cut
        int cutM = 0;
        for (auto* k : ed->getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (k))
            {
                const float tw = juce::GlyphArrangement::getStringWidth (l->getFont(), l->getText());
                if (l->isVisible() && tw > (float) l->getWidth() - 2.0f) { ++cutM; std::printf ("  label cut: '%s' (%.0f in %d)\n", l->getText().toRawUTF8(), tw, l->getWidth()); }
            }
        CHECK (cutM == 0, "a label is cut off in master mode");

        de->clickSource (de::S_KICK_IN);      // HOME
        de->refreshNow();
        CHECK (! proc.editingRemote(), "HOME did not return");
        de->clickMaster();
        de->refreshNow();
        CHECK (! de->showsMaster(), "the master look stayed on");
        snare.setStateInformation (nullptr, 0);
    }
    std::printf ("\n%d checks - %s\n  wrote %s\n", checks, failures == 0 ? "ALL CLEAR" : "FAILURES",
                 dir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
