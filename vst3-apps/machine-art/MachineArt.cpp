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

    namespace
    {
        juce::Rectangle<float> disc (juce::Point<float> c, float r) { return { c.x - r, c.y - r, r * 2.0f, r * 2.0f }; }

        //  a metal ring between radii r0 < r1: lit from the top left, a bevel
        //  highlight on the outer edge and a shadow on the inner one
        void drawRing (juce::Graphics& g, juce::Point<float> c, float r0, float r1, juce::Colour light, juce::Colour dark)
        {
            juce::Path ring;
            ring.addEllipse (disc (c, r1));
            ring.addEllipse (disc (c, r0));
            ring.setUsingNonZeroWinding (false);
            juce::ColourGradient m (light, c.x - r1 * 0.7f, c.y - r1 * 0.7f, dark, c.x + r1 * 0.7f, c.y + r1 * 0.7f, false);
            m.addColour (0.5, light.interpolatedWith (dark, 0.55f));
            g.setGradientFill (m);
            g.fillPath (ring);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawEllipse (disc (c, r1 - 0.8f), 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawEllipse (disc (c, r1), 1.2f);
            g.drawEllipse (disc (c, r0), 1.4f);
        }

        //  a slotted screw head, the kind the decals carry on every plate
        void drawScrew (juce::Graphics& g, juce::Point<float> p, float r, float angle)
        {
            juce::ColourGradient s (juce::Colour (0xffcdb892), p.x - r * 0.5f, p.y - r * 0.6f,
                                    juce::Colour (0xff5a4a33), p.x + r, p.y + r, true);
            g.setGradientFill (s);
            g.fillEllipse (disc (p, r));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (disc (p, r), 0.8f);
            const auto d = juce::Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.8f);
            g.drawLine ({ p - d, p + d }, juce::jmax (0.8f, r * 0.28f));
        }

        //  a coated drum head: cream, mottled, darker at the rim, worn in the middle
        void drawCoatedHead (juce::Graphics& g, juce::Point<float> c, float r, float glow, juce::Colour glowColour, int seed)
        {
            juce::ColourGradient h (juce::Colour (0xffe6dfcd), c.x - r * 0.35f, c.y - r * 0.4f,
                                    juce::Colour (0xff8e866f), c.x + r, c.y + r, true);
            g.setGradientFill (h);
            g.fillEllipse (disc (c, r));
            juce::Random rnd (seed);                                       // fixed: the grain never crawls
            for (int i = 0; i < (int) (r * r * 0.06f); ++i)
            {
                const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi, d = r * std::sqrt (rnd.nextFloat()) * 0.97f;
                g.setColour (juce::Colour (0xff3b3326).withAlpha (0.05f + 0.10f * rnd.nextFloat()));
                g.fillEllipse (c.x + std::cos (a) * d, c.y + std::sin (a) * d, 1.0f + rnd.nextFloat(), 1.0f + rnd.nextFloat());
            }
            juce::ColourGradient wear (juce::Colour (0xff4a4031).withAlpha (0.28f), c.x, c.y,
                                       juce::Colour (0xff4a4031).withAlpha (0.0f), c.x + r * 0.42f, c.y, true);
            g.setGradientFill (wear);                                      // where the beater lands
            g.fillEllipse (disc (c, r * 0.42f));
            if (glow > 0.01f)
            {
                juce::ColourGradient lit (glowColour.withAlpha (0.75f * glow), c.x, c.y,
                                          glowColour.withAlpha (0.12f * glow), c.x + r, c.y, true);
                g.setGradientFill (lit);
                g.fillEllipse (disc (c, r));
            }
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (disc (c, r), 1.0f);
        }
    }

    void drawPad (juce::Graphics& g, juce::Point<float> c, float r, Pad pad, float glow,
                  juce::Colour glowColour, float zone, const juce::String& label, const juce::Font& font)
    {
        //  it sits ON the panel: a soft shadow first
        for (int i = 3; i > 0; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillEllipse (disc (c.translated (0.0f, r * 0.05f), r + (float) i * r * 0.025f));
        }

        float headR = r;
        if (pad == Pad::Kick || pad == Pad::Snare)
        {
            //  the hoop. On the snare it spans exactly the rim-shot zone.
            const float inner = pad == Pad::Snare ? r * zone : r * 0.86f;
            const bool chrome = pad == Pad::Snare;
            const auto warm = juce::Colour (0xffffb020);
            drawRing (g, c, inner, r,
                      (chrome ? juce::Colour (0xffe4e8ea) : juce::Colour (0xff7d8186)).interpolatedWith (warm, 0.35f * glow),
                      (chrome ? juce::Colour (0xff3c4248) : juce::Colour (0xff1f2124)).interpolatedWith (warm.darker (0.8f), 0.35f * glow));
            const int lugs = chrome ? 10 : 8;                              // tension rods, one screw each
            for (int i = 0; i < lugs; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / (float) lugs;
                drawScrew (g, c.getPointOnCircumference ((inner + r) * 0.5f, a), (r - inner) * 0.30f, a * 1.7f + 0.4f);
            }
            headR = inner - 1.0f;
            drawCoatedHead (g, c, headR, glow, glowColour, pad == Pad::Kick ? 31 : 47);
        }
        else if (pad == Pad::Hats)
        {
            //  bronze: a lathe-turned bow, a raised bell inside `zone`
            juce::ColourGradient bz (juce::Colour (0xffe9c46a).interpolatedWith (juce::Colours::white, 0.25f * glow),
                                     c.x - r * 0.45f, c.y - r * 0.5f,
                                     juce::Colour (0xff6b4a17), c.x + r, c.y + r, true);
            bz.addColour (0.55, juce::Colour (0xffb98a33));
            g.setGradientFill (bz);
            g.fillEllipse (disc (c, r));
            for (int i = 0; i < 28; ++i)                                   // lathe grooves
            {
                const float rr = r * (zone + (1.0f - zone) * ((float) i + 0.5f) / 28.0f);
                g.setColour ((i % 2 == 0 ? juce::Colour (0xff3a2708) : juce::Colours::white).withAlpha (i % 2 == 0 ? 0.16f : 0.07f));
                g.drawEllipse (disc (c, rr), 0.7f);
            }
            for (int k = 0; k < 2; ++k)                                    // the sheen a lathe leaves: two opposite wedges
            {
                juce::Path w;
                const float a0 = -2.25f + (float) k * juce::MathConstants<float>::pi;
                w.addPieSegment (disc (c, r), a0, a0 + 0.55f, zone);
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.fillPath (w);
            }
            if (glow > 0.01f)
            {
                juce::ColourGradient lit (glowColour.withAlpha (0.55f * glow), c.x, c.y, glowColour.withAlpha (0.0f), c.x + r, c.y, true);
                g.setGradientFill (lit);
                g.fillEllipse (disc (c, r));
            }
            g.setColour (juce::Colour (0xff3a2708).withAlpha (0.8f));
            g.drawEllipse (disc (c, r), 1.4f);
            const float br = r * zone;                                     // the bell
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (disc (c.translated (0.0f, br * 0.12f), br * 1.04f));
            juce::ColourGradient bell (juce::Colour (0xfff4d98f), c.x - br * 0.4f, c.y - br * 0.5f,
                                       juce::Colour (0xff8a6320), c.x + br, c.y + br, true);
            g.setGradientFill (bell);
            g.fillEllipse (disc (c, br));
            g.setColour (juce::Colour (0xff1a1206));                        // the mounting hole
            g.fillEllipse (disc (c, juce::jmax (2.0f, br * 0.16f)));
            headR = 0.0f;                                                  // no lettering on a cymbal
        }
        else                                                               // Stop
        {
            const float inner = r * 0.72f;
            drawRing (g, c, inner, r, juce::Colour (0xfff0cf3a), juce::Colour (0xff9a7a10));
            juce::Random rnd (73);                                         // chipped paint
            for (int i = 0; i < 26; ++i)
            {
                const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi, d = inner + (r - inner) * rnd.nextFloat();
                g.setColour (juce::Colour (0xff2a2418).withAlpha (0.55f));
                g.fillEllipse (c.x + std::cos (a) * d, c.y + std::sin (a) * d, 1.0f + 1.5f * rnd.nextFloat(), 1.0f + rnd.nextFloat());
            }
            const float hr = inner - 1.0f;
            juce::ColourGradient red (juce::Colour (0xffff6a55).interpolatedWith (juce::Colours::white, 0.2f * glow),
                                      c.x - hr * 0.35f, c.y - hr * 0.45f, juce::Colour (0xff6e0c08), c.x + hr, c.y + hr, true);
            g.setGradientFill (red);
            g.fillEllipse (disc (c, hr));
            g.setColour (juce::Colours::white.withAlpha (0.35f));          // the gloss
            g.fillEllipse (c.x - hr * 0.55f, c.y - hr * 0.65f, hr * 0.7f, hr * 0.35f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (disc (c, hr), 1.0f);
            headR = 0.0f;
        }

        if (headR > 0.0f && label.isNotEmpty())
        {
            g.setFont (font);
            g.setColour (juce::Colour (0xff2b241c).withAlpha (0.82f));     // stencilled in dark ink on the head
            g.drawText (label, disc (c, headR).toNearestInt(), juce::Justification::centred, false);
        }
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
