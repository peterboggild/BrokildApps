#pragma once

// BEETMACHINE - the look: a factory control cabinet. Drawn in code until the
// decals arrive (assets/drum-decals/BRIEF.md); every part here maps to a part
// in that brief, so the art replaces the drawing one piece at a time.

#include <JuceHeader.h>

namespace beetcol
{
    const juce::Colour cabinet  (0xff34414b);   // hammer-finish grey-blue
    const juce::Colour cabinet2 (0xff27323a);
    const juce::Colour bay      (0xff1c2329);
    const juce::Colour steel    (0xff8b949b);
    const juce::Colour ink      (0xffe9e4d8);   // cream lettering
    const juce::Colour faint    (0xff7d8a93);
    const juce::Colour amber    (0xffffb23e);
    const juce::Colour bwfxTeal (0xff35c9c0);     // BWFX's one mandated accent, fleet-wide
    const juce::Colour red      (0xffd8342a);
    const juce::Colour green    (0xff7fdc7a);
    const juce::Colour tape     (0xff111315);   // embossed label tape
}

class BeetLook  : public juce::LookAndFeel_V4
{
public:
    BeetLook();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;

    //  parts used directly by the panel
    static void drawRivet (juce::Graphics&, float cx, float cy, float r);
    static void drawLamp (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float glow01);
    static void drawTapeLabel (juce::Graphics&, juce::Rectangle<float>, const juce::String&, float fontH);
    static void drawBay (juce::Graphics&, juce::Rectangle<float>, bool selected);
    static juce::Font stencil (float h);
    static juce::Font mono (float h);

    //  the decals (plugin/art, from assets/drum-decals via tools/ingest-decals.ps1),
    //  looked up by their file name; an invalid Image when the art is absent,
    //  in which case every part falls back to its drawing
    static juce::Image art (const char* file);
    static void drawNineSlice (juce::Graphics&, const juce::Image&, juce::Rectangle<float> dest,
                               int srcBorder, float destBorder);
    static void drawTiled (juce::Graphics&, const juce::Image&, juce::Rectangle<float> dest, float scale);

    //  component properties the drawing reads
    //    "role" = "hit" | "panic" | "tiny" | "seg"   (buttons)
};
