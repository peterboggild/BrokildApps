#include "MachineArt.h"
#include "MachineArtData.h"

namespace machineart
{
    juce::Image image (const char* file)
    {
        for (int i = 0; i < MachineArtData::namedResourceListSize; ++i)
            if (juce::String (MachineArtData::originalFilenames[i]) == file)
            {
                int size = 0;
                if (const char* data = MachineArtData::getNamedResource (MachineArtData::namedResourceList[i], size))
                    return juce::ImageCache::getFromMemory (data, size);
            }
        return {};
    }

    void drawTiled (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> d, float scale)
    {
        //  an image fill repeats by itself; the transform sets its scale and origin
        g.saveState();
        g.reduceClipRegion (d.toNearestInt());
        g.setFillType (juce::FillType (img, juce::AffineTransform::scale (scale).translated (d.getX(), d.getY())));
        g.fillRect (d);
        g.restoreState();
    }

    void drawNineSlice (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> d, int b, float db)
    {
        //  corners at their own proportions, edges stretched along one axis only
        const int w = img.getWidth(), h = img.getHeight();
        const int xs[] = { juce::roundToInt (d.getX()), juce::roundToInt (d.getX() + db),
                           juce::roundToInt (d.getRight() - db), juce::roundToInt (d.getRight()) };
        const int ys[] = { juce::roundToInt (d.getY()), juce::roundToInt (d.getY() + db),
                           juce::roundToInt (d.getBottom() - db), juce::roundToInt (d.getBottom()) };
        const int sx[] = { 0, b, w - b, w }, sy[] = { 0, b, h - b, h };
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                g.drawImage (img, xs[c], ys[r], xs[c + 1] - xs[c], ys[r + 1] - ys[r],
                                  sx[c], sy[r], sx[c + 1] - sx[c], sy[r + 1] - sy[r]);
    }

    void drawRivet (juce::Graphics& g, float cx, float cy, float r)
    {
        juce::ColourGradient cg (juce::Colour (0xffd9dde0), cx - r * 0.5f, cy - r * 0.5f,
                                 juce::Colour (0xff3a4148), cx + r, cy + r, true);
        g.setGradientFill (cg);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 0.8f);
    }

    void drawGround (juce::Graphics& g, juce::Rectangle<float> r, const char* file, float scale, float darken)
    {
        const auto img = image (file);
        if (img.isValid()) drawTiled (g, img, r, scale);
        else { g.setColour (juce::Colour (0xff2a2d31)); g.fillRect (r); }
        if (darken > 0.0f) { g.setColour (juce::Colours::black.withAlpha (darken)); g.fillRect (r); }
    }

    juce::Rectangle<float> drawNameplate (juce::Graphics& g, juce::Rectangle<float> area, const char* file)
    {
        const auto img = image (file);
        if (! img.isValid()) return {};
        const float k = juce::jmin (area.getWidth() / img.getWidth(), area.getHeight() / img.getHeight());
        auto r = juce::Rectangle<float> (area.getX(), area.getY(), img.getWidth() * k, img.getHeight() * k)
                     .withY (area.getCentreY() - img.getHeight() * k * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));                  // it stands proud of the paint
        g.fillRoundedRectangle (r.reduced (4.0f).translated (2.0f, 3.0f), 4.0f);
        g.drawImage (img, r, juce::RectanglePlacement::stretchToFit);
        return r;
    }

    void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, float corner, float alpha)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));                  // recess shadow
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);
        juce::ColourGradient bg (juce::Colour (0xff23262b).withAlpha (alpha), r.getX(), r.getY(),
                                 juce::Colour (0xff141619).withAlpha (alpha), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, corner);
        juce::ColourGradient edge (juce::Colour (0xffa9b1b6).withAlpha (0.55f), r.getX(), r.getY(),
                                   juce::Colour (0xff3d4349).withAlpha (0.55f), r.getX(), r.getBottom(), false);
        g.setGradientFill (edge);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.4f);
        const float i = 7.0f;
        drawRivet (g, r.getX() + i, r.getY() + i, 2.4f);
        drawRivet (g, r.getRight() - i, r.getY() + i, 2.4f);
        drawRivet (g, r.getX() + i, r.getBottom() - i, 2.4f);
        drawRivet (g, r.getRight() - i, r.getBottom() - i, 2.4f);
    }

    bool drawKnob (juce::Graphics& g, juce::Rectangle<float> box, float angle, const char* file, bool enabled)
    {
        const auto img = image (file);
        if (! img.isValid()) return false;
        const float d = juce::jmin (box.getWidth(), box.getHeight());
        const auto c = box.getCentre();
        g.setColour (juce::Colours::black.withAlpha (0.4f));                   // it sits ON the plate
        g.fillEllipse (juce::Rectangle<float> (d * 0.84f, d * 0.84f).withCentre (c).translated (1.5f, 2.5f));
        g.setOpacity (enabled ? 1.0f : 0.45f);
        g.drawImageTransformed (img, juce::AffineTransform::scale (d / (float) img.getWidth())
                                        .translated (-d * 0.5f, -d * 0.5f)
                                        .rotated (angle)
                                        .translated (c.x, c.y), false);
        g.setOpacity (1.0f);
        return true;
    }

    bool drawPalmButton (juce::Graphics& g, juce::Rectangle<float> box, bool pressed, float glow, juce::Colour glowColour)
    {
        const auto img = image ("hit.png");
        if (! img.isValid()) return false;
        const float d0 = juce::jmin (box.getWidth(), box.getHeight());
        auto b = box.withSizeKeepingCentre (d0, d0);
        if (glow > 0.01f)
        {
            juce::ColourGradient halo (glowColour.withAlpha (0.55f * glow), b.getCentreX(), b.getCentreY(),
                                       glowColour.withAlpha (0.0f), b.getCentreX() + d0 * 0.62f, b.getCentreY(), true);
            g.setGradientFill (halo);
            g.fillEllipse (b.expanded (d0 * 0.12f));
        }
        //  the pressed drawing is registered with the up one (05-buttons.png),
        //  so holding the button swaps the picture and the plate stays put
        const auto down = image ("hit-down.png");
        g.drawImage (pressed && down.isValid() ? down : img, b, juce::RectanglePlacement::centred);
        const auto head = b.reduced (d0 * 0.24f);
        if (glow > 0.01f) { g.setColour (glowColour.withAlpha (0.22f * glow)); g.fillEllipse (head); }
        return true;
    }

    void drawSteelButton (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour onColour,
                          bool over, bool down, bool enabled)
    {
        r = r.reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 3.0f);
        juce::ColourGradient bz (juce::Colour (0xffa9b1b6), r.getX(), r.getY(), juce::Colour (0xff4f575d), r.getX(), r.getBottom(), false);
        g.setGradientFill (bz);
        g.fillRoundedRectangle (r, 3.0f);
        auto face = r.reduced (2.0f).translated (0.0f, down ? 1.0f : 0.0f);
        const juce::Colour fc = ! enabled ? juce::Colour (0xff2a2f33) : on ? onColour : juce::Colour (0xff202528);
        juce::ColourGradient fg (fc.brighter (0.25f), face.getX(), face.getY(), fc.darker (0.35f), face.getX(), face.getBottom(), false);
        g.setGradientFill (fg);
        g.fillRoundedRectangle (face, 2.0f);
        if (on) { g.setColour (onColour.withAlpha (0.25f)); g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f); }
        if (over && ! down) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (face, 2.0f); }
    }

    void drawTape (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto t = image ("tape.png");
        if (t.isValid()) drawTiled (g, t, r, r.getHeight() / (float) t.getHeight());
        else { g.setColour (juce::Colour (0xff111315)); g.fillRoundedRectangle (r, 2.5f); }
    }

    void drawBezel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        juce::ColourGradient bz (juce::Colour (0xffb9c0c5), r.getX(), r.getY(), juce::Colour (0xff454b51), r.getX(), r.getBottom(), false);
        g.setGradientFill (bz);
        juce::Path ring;
        ring.addRoundedRectangle (r, corner);
        ring.addRoundedRectangle (r.reduced (4.0f), juce::jmax (1.0f, corner - 3.0f));
        ring.setUsingNonZeroWinding (false);
        g.fillPath (ring);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (4.0f), juce::jmax (1.0f, corner - 3.0f), 1.0f);
        const float i = 9.0f;
        drawRivet (g, r.getX() + i - 7.0f, r.getY() + i - 7.0f, 2.0f);
        drawRivet (g, r.getRight() - i + 7.0f, r.getY() + i - 7.0f, 2.0f);
        drawRivet (g, r.getX() + i - 7.0f, r.getBottom() - i + 7.0f, 2.0f);
        drawRivet (g, r.getRight() - i + 7.0f, r.getBottom() - i + 7.0f, 2.0f);
    }
}
