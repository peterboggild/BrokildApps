#include "BeetLook.h"
#include "MachineArt.h"
#include "BeetKits.h"          // the drum types a slot pad is drawn as

using namespace beetcol;

//  one parts set for the whole drum family (vst3-apps/machine-art); two of
//  Beetmachine's own names map onto the shared files
juce::Image BeetLook::art (const char* file)
{
    const juce::String f (file);
    if (f == "ground.jpg") return machineart::image ("ground-beet.jpg");
    if (f == "plate.png")  return machineart::image ("plate-blank.png");
    return machineart::image (file);
}

void BeetLook::drawNineSlice (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> d, int b, float db)
{
    //  corners at their own proportions, edges stretched along one axis only,
    //  so rivets and hinges stay round whatever the bay's size
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

void BeetLook::drawTiled (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> d, float scale)
{
    //  an image fill repeats by itself; the transform only sets its scale and origin
    g.saveState();
    g.reduceClipRegion (d.toNearestInt());
    g.setFillType (juce::FillType (img, juce::AffineTransform::scale (scale).translated (d.getX(), d.getY())));
    g.fillRect (d);
    g.restoreState();
}

BeetLook::BeetLook()
{
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::PopupMenu::backgroundColourId, cabinet2);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TooltipWindow::backgroundColourId, tape);
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::Label::textColourId, ink);
}

juce::Font BeetLook::stencil (float h)
{
    return juce::Font (juce::FontOptions ("Arial Black", h, juce::Font::bold));
}
juce::Font BeetLook::mono (float h)
{
    return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::bold));
}

void BeetLook::drawRivet (juce::Graphics& g, float cx, float cy, float r)
{
    juce::ColourGradient cg (juce::Colour (0xffd9dde0), cx - r * 0.5f, cy - r * 0.5f,
                             juce::Colour (0xff3a4148), cx + r, cy + r, true);
    g.setGradientFill (cg);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 0.8f);
}

void BeetLook::drawLamp (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float glow)
{
    glow = juce::jlimit (0.0f, 1.0f, glow);
    {
        //  the jewel lamp decal: off and on share one bezel, so blending them
        //  lights the glass and nothing else moves
        static const juce::Image off = art ("lamp-off.png"), on = art ("lamp-on.png");
        if (off.isValid() && on.isValid())
        {
            auto d = r.expanded (r.getWidth() * 0.55f);
            g.drawImage (off, d, juce::RectanglePlacement::centred);
            if (glow > 0.01f)
            {
                g.setOpacity (glow);
                g.drawImage (on, d, juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
            }
            juce::ignoreUnused (c);
            return;
        }
    }
    const auto cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getWidth() * 0.5f;
    if (glow > 0.02f)
    {
        juce::ColourGradient halo (c.withAlpha (0.55f * glow), cx, cy, c.withAlpha (0.0f), cx + rad * 2.6f, cy, true);
        g.setGradientFill (halo);
        g.fillEllipse (r.expanded (rad * 1.6f));
    }
    g.setColour (juce::Colour (0xff9aa3a9));                       // chrome bezel
    g.fillEllipse (r.expanded (2.5f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (r.expanded (2.5f), 1.0f);
    const auto lens = c.darker (2.4f).interpolatedWith (c.brighter (0.35f), glow);
    juce::ColourGradient lg (lens.brighter (0.6f), cx - rad * 0.4f, cy - rad * 0.4f, lens.darker (0.5f), cx + rad, cy + rad, true);
    g.setGradientFill (lg);
    g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (0.35f + 0.3f * glow));  // facet highlight
    g.fillEllipse (cx - rad * 0.5f, cy - rad * 0.55f, rad * 0.45f, rad * 0.3f);
}

void BeetLook::drawTapeLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float fontH)
{
    static const juce::Image t = art ("tape.png");
    if (t.isValid()) drawTiled (g, t, r, r.getHeight() / (float) t.getHeight());
    else { g.setColour (tape); g.fillRoundedRectangle (r, 2.0f); }
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawLine (r.getX() + 2, r.getY() + 1.5f, r.getRight() - 2, r.getY() + 1.5f, 1.0f);
    g.setFont (mono (fontH));
    g.setColour (juce::Colours::black);                              // embossing: shadow then face
    g.drawText (text, r.translated (0.8f, 0.8f), juce::Justification::centred, false);
    g.setColour (ink.withAlpha (0.92f));
    g.drawText (text, r, juce::Justification::centred, false);
}

void BeetLook::drawBay (juce::Graphics& g, juce::Rectangle<float> r, bool selected)
{
    static const juce::Image frame = art ("bay.png");
    if (frame.isValid())
    {
        //  the machine bay: a dark recess, the riveted frame drawn nine-slice so
        //  its corners keep their shape, and an amber rim when selected
        g.setColour (bay);
        g.fillRect (r.reduced (8.0f));
        drawNineSlice (g, frame, r, 44, 15.0f);
        if (selected)
        {
            g.setColour (amber.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.2f);
            g.setColour (amber.withAlpha (0.18f));
            g.drawRoundedRectangle (r.reduced (4.0f), 4.0f, 5.0f);
        }
        return;
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));            // recess shadow
    g.fillRoundedRectangle (r.translated (0, 2), 5.0f);
    juce::ColourGradient bg (bay.brighter (0.12f), r.getX(), r.getY(), bay, r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? amber : steel.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 2.4f : 1.2f);
    const float i = 7.0f;
    drawRivet (g, r.getX() + i, r.getY() + i, 2.6f);
    drawRivet (g, r.getRight() - i, r.getY() + i, 2.6f);
    drawRivet (g, r.getX() + i, r.getBottom() - i, 2.6f);
    drawRivet (g, r.getRight() - i, r.getBottom() - i, 2.6f);
}

//==============================================================================
//  knobs: black fluted bakelite, aluminium cap, white pointer; a stamped
//  scale of ticks around them
void BeetLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                 float a0, float a1, juce::Slider& s)
{
    const float d = (float) juce::jmin (w, h) - 8.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f, r = d * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    g.setColour (ink.withAlpha (0.55f));                             // scale
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * i / 10.0f;
        const float r0 = r + 2.0f, r1 = r + (i % 5 == 0 ? 6.0f : 4.0f);
        g.drawLine (cx + r0 * std::sin (a), cy - r0 * std::cos (a), cx + r1 * std::sin (a), cy - r1 * std::cos (a), 1.0f);
    }
    if (bipolar)
    {
        g.setColour (amber);
        g.fillEllipse (cx - 1.8f, cy - r - 8.5f, 3.6f, 3.6f);
    }

    {
        //  the bakelite knob decal, pointer drawn straight up, turned by the
        //  value; the steel skirt under it does not turn
        //  No steel skirt: at panel size it covered the stamped scale, and a
        //  knob whose setting cannot be read is a usability fault, not a look.
        static const juce::Image knob = art ("knob.png"), red = art ("knob-red.png");
        const bool isRed = (bool) s.getProperties().getWithDefault ("red", false);
        const auto& img = isRed ? red : knob;
        if (img.isValid())
        {
            const auto box = juce::Rectangle<float> (d, d).withCentre ({ cx, cy });
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (box.reduced (d * 0.08f).translated (1.5f, 2.5f));
            g.drawImageTransformed (img, juce::AffineTransform::scale (d / (float) img.getWidth())
                                            .translated (-d * 0.5f, -d * 0.5f)
                                            .rotated (ang)
                                            .translated (cx, cy), false);
            //  the setting, readable at any size: an amber mark on the scale
            //  where the pointer points
            const float rm = r + 4.0f;
            g.setColour (amber);
            g.fillEllipse (cx + rm * std::sin (ang) - 2.4f, cy - rm * std::cos (ang) - 2.4f, 4.8f, 4.8f);
            return;
        }
    }

    juce::Path flutes;                                               // fluted skirt
    const int nF = 18;
    for (int i = 0; i < nF * 2; ++i)
    {
        const float a = ang + juce::MathConstants<float>::twoPi * i / (nF * 2);
        const float rr = (i % 2 == 0) ? r : r * 0.9f;
        const float px = cx + rr * std::sin (a), py = cy - rr * std::cos (a);
        if (i == 0) flutes.startNewSubPath (px, py); else flutes.lineTo (px, py);
    }
    flutes.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (flutes, juce::AffineTransform::translation (1.2f, 2.0f));
    juce::ColourGradient body (juce::Colour (0xff3a3a3c), cx - r * 0.6f, cy - r * 0.6f, juce::Colour (0xff0a0a0b), cx + r, cy + r, true);
    g.setGradientFill (body);
    g.fillPath (flutes);

    const float cr = r * 0.52f;                                      // aluminium cap
    juce::ColourGradient cap (juce::Colour (0xffe8ebed), cx - cr * 0.6f, cy - cr * 0.6f, juce::Colour (0xff6f777d), cx + cr, cy + cr, true);
    g.setGradientFill (cap);
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);

    g.setColour (juce::Colours::white);                              // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.4f, -r * 0.98f, 2.8f, r * 0.62f, 1.2f);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

//==============================================================================
juce::Font BeetLook::getTextButtonFont (juce::TextButton& b, int h)
{
    const auto role = b.getProperties()["role"].toString();
    if (role == "hit" || role == "panic") return stencil ((float) h * 0.30f);
    return mono (juce::jmin ((float) h * 0.62f, 13.0f));
}

void BeetLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto role = b.getProperties()["role"].toString();
    const bool on = b.getToggleState();

    if (role == "hit" || role == "panic")                            // the drawn pads
    {
        //  the family's shared pad (machine-art): each slot's pad is its own
        //  drum - a kick head, a snare in its chrome hoop, a bronze cymbal - and
        //  STOP is the red head in the yellow ring. Pressing lights it.
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        const float glow = down ? 1.0f : (over ? 0.2f : 0.0f);
        const int drum = (int) b.getProperties().getWithDefault ("drum", (int) beet::KICK);
        machineart::Pad kind = machineart::Pad::Kick;
        float zone = 0.0f;
        if (role == "panic")        kind = machineart::Pad::Stop;
        else if (drum == beet::SNARE) { kind = machineart::Pad::Snare; zone = 0.80f; }   // Snare Tactics' kRimFrom
        else if (drum == beet::HATS)  { kind = machineart::Pad::Hats;  zone = 0.22f; }   // Hats Off's kBellFrom
        machineart::drawPad (g, r.getCentre(), d * 0.46f, kind, glow, amber, zone, {}, stencil (12.0f));
        return;
    }

    //  square steel-bezel push buttons; lit amber when on
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 3.0f);
    juce::ColourGradient bz (juce::Colour (0xffa9b1b6), r.getX(), r.getY(), juce::Colour (0xff4f575d), r.getX(), r.getBottom(), false);
    g.setGradientFill (bz);
    g.fillRoundedRectangle (r, 3.0f);
    auto face = r.reduced (2.0f).translated (0, down ? 1.0f : 0.0f);
    const juce::Colour lit = role == "bwfx" ? bwfxTeal : amber;     // the rack button lights teal
    const juce::Colour fc = ! b.isEnabled() ? juce::Colour (0xff2a2f33)
                          : on ? lit : juce::Colour (0xff202528);
    juce::ColourGradient fg (fc.brighter (0.25f), face.getX(), face.getY(), fc.darker (0.35f), face.getX(), face.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (face, 2.0f);
    if (on) { g.setColour (lit.withAlpha (0.25f)); g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f); }
    if (over && ! down) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (face, 2.0f); }
}

void BeetLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const auto role = b.getProperties()["role"].toString();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    if (role == "hit" || role == "panic")
    {
        return;             // the drawn pad is the button; STOP's label is beside it
        g.setColour (ink.withAlpha (0.92f));
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred, false);
        return;
    }
    const bool on = b.getToggleState();
    if (role == "bwfx")
    {
        //  the BWFX globe beside the word, as every Brokild synth wears it
        auto r = b.getLocalBounds().toFloat().reduced (2.0f).translated (0.0f, down ? 1.0f : 0.0f);
        const juce::Colour c = on ? juce::Colours::black : bwfxTeal;
        const float d = r.getHeight() * 0.62f;
        auto globe = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + 6.0f + d * 0.5f, r.getCentreY() });
        g.setColour (c);
        g.drawEllipse (globe, 1.4f);
        g.drawEllipse (globe.reduced (d * 0.28f, 0.0f), 1.0f);
        g.drawLine (globe.getX(), globe.getCentreY(), globe.getRight(), globe.getCentreY(), 1.0f);
        g.drawText ("BWFX", r.withTrimmedLeft (d + 10.0f), juce::Justification::centred, false);
        return;
    }
    g.setColour (! b.isEnabled() ? faint.withAlpha (0.4f) : on ? juce::Colours::black : ink);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (2).translated (0, down ? 1 : 0), juce::Justification::centred, false);
}

//==============================================================================
//  combo boxes are embossed label tape with a steel arrow
void BeetLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    static const juce::Image t = art ("tape.png");
    if (t.isValid()) drawTiled (g, t, r, r.getHeight() / (float) t.getHeight());
    else { g.setColour (tape); g.fillRoundedRectangle (r, 2.5f); }
    g.setColour (box.hasKeyboardFocus (true) ? amber.withAlpha (0.6f) : steel.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 2.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 11.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (amber.withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.fillPath (arrow);
}

juce::Font BeetLook::getComboBoxFont (juce::ComboBox& box)
{
    return mono (juce::jmin (13.5f, box.getHeight() * 0.56f));
}

void BeetLook::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (5, 1, box.getWidth() - 22, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
    l.setJustificationType (juce::Justification::centredLeft);
}

void BeetLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (tape);
    g.setColour (amber.withAlpha (0.6f));
    g.drawRect (0, 0, w, h, 1);
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 6);
}
