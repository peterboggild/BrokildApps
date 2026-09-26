#include "PluginEditor.h"
#include "MachineArt.h"      // the drum family's shared machine parts

#include <complex>

namespace
{
    //  the trilogy's panel: Kickstart's charcoal, the snare's steel, and brass
    const juce::Colour kBack   { 0xff0e0f11 };
    const juce::Colour kPanel  { 0xff17191d };
    const juce::Colour kPanel2 { 0xff1f2227 };
    const juce::Colour kInk    { 0xffece8df };
    const juce::Colour kDim    { 0xff858c98 };
    const juce::Colour kFaint  { 0xff2c3037 };
    const juce::Colour kBrass  { 0xffe8b64c };   // the cymbal
    const juce::Colour kSteel  { 0xff6fd0ff };   // its brightness
    const juce::Colour kHot    { 0xffff5a1f };

    const juce::Colour kMetal  { 0xffe8b64c };
    const juce::Colour kHat    { 0xffff7b54 };
    const juce::Colour kHitC   { 0xfff5d76e };
    const juce::Colour kEq     { 0xff6fd0ff };
    const juce::Colour kEra    { 0xff36c9bd };
    const juce::Colour kEcho   { 0xff8ee06a };
    const juce::Colour kTrans  { 0xffa184ff };
    const juce::Colour kDrive  { 0xffff3d4a };
    const juce::Colour kComp   { 0xff4a7dff };
    const juce::Colour kOut    { 0xffcfd3da };

    constexpr int kW = 1400, kH = 724;

    int specIndex (const char* id)
    {
        for (int i = 0; i < ho::kNumParams; ++i)
            if (std::strcmp (ho::specs()[i].id, id) == 0) return i;
        return -1;
    }
    juce::Font mono (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }
    juce::Font sans (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    juce::String msText (double s)
    {
        return s >= 1.0 ? juce::String (s, 2) + " s" : juce::String (juce::roundToInt (s * 1000.0)) + " ms";
    }
    juce::String noteOf (double hz)
    {
        static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int m = (int) std::lround (69.0 + 12.0 * std::log2 (std::max (1.0, hz) / 440.0));
        return juce::String (N[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }
}

//==============================================================================
HoLook::HoLook()
{
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kInk);
    setColour (juce::ComboBox::backgroundColourId, kPanel2);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kFaint);
    setColour (juce::ComboBox::arrowColourId, kBrass);
    setColour (juce::PopupMenu::backgroundColourId, kPanel2);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::headerTextColourId, kBrass);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kBrass.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::textColourOffId, kInk);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::TooltipWindow::backgroundColourId, kPanel2);
    setColour (juce::TooltipWindow::textColourId, kInk);
}

void HoLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float size = (float) juce::jmin (w, h) - 6.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    const bool bip = (bool) s.getProperties()["bipolar"];
    const float aVal = a0 + pos * (a1 - a0);
    const float aFrom = bip ? (a0 + a1) * 0.5f : a0;
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
    g.setColour (kFaint);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    arc.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (aFrom, aVal), juce::jmax (aFrom, aVal), true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    {
        //  the cap: the bakelite knob decal, turned to the value; the drawn cap
        //  below is the fallback if the art is missing
        const juce::String file = s.getProperties().getWithDefault ("knob", "knob.png").toString();
        const float kd = (r - 5.0f) * 2.0f;
        if (machineart::drawKnob (g, juce::Rectangle<float> (kd, kd).withCentre (c), aVal, file.toRawUTF8(), s.isEnabled()))
            return;
    }
    const float cr = r - 8.0f;
    juce::ColourGradient cap (juce::Colour (0xff3a3f47), c.x - cr * 0.5f, c.y - cr * 0.7f,
                              juce::Colour (0xff121417), c.x + cr * 0.4f, c.y + cr * 0.8f, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);
    const float px = c.x + std::sin (aVal) * (cr - 3.0f), py = c.y - std::cos (aVal) * (cr - 3.0f);
    const float qx = c.x + std::sin (aVal) * (cr * 0.35f), qy = c.y - std::cos (aVal) * (cr * 0.35f);
    g.setColour (s.isEnabled() ? kInk : kDim);
    g.drawLine (qx, qy, px, py, 2.4f);
}

void HoLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    //  a steel-bezel push button, lit in its section colour when on
    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),
                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());
}

void HoLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    machineart::drawTape (g, r);                  // embossed label tape
    juce::Path tri;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (kBrass);
    g.fillPath (tri);
}

juce::Font HoLook::getComboBoxFont (juce::ComboBox& b) { return sans (b.getHeight() > 30 ? 17.0f : 14.0f, true); }

void HoLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void HoLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (kPanel2);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (kBrass.withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (kInk);
    g.setFont (sans (13.5f));
    g.drawFittedText (text, 10, 6, w - 20, h - 12, juce::Justification::centredLeft, 5);
}

juce::Rectangle<int> HoLook::getTooltipBounds (const juce::String& text, juce::Point<int> at, juce::Rectangle<int> parent)
{
    const int w = 320;
    const int lines = juce::jmax (1, (int) std::ceil (text.length() / 42.0));
    const int h = 14 + lines * 18;
    return juce::Rectangle<int> (at.x + 14, at.y + 18, w, h).constrainedWithin (parent);
}

//==============================================================================
void HitDisplay::setHit (std::vector<float> w, std::vector<float> b, double s, const ho::Params& p, double bp)
{
    wave = std::move (w); bright = std::move (b); secs = s; params = p; bpm = bp;
    repaint();
}

void HitDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kBack);
    g.fillRoundedRectangle (b, 8.0f);
    machineart::drawBezel (g, b, 8.0f);

    auto area = b.reduced (14.0f, 12.0f).withTrimmedTop (30.0f).withTrimmedBottom (18.0f);
    const float mid = area.getCentreY();

    const double steps[] = { 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0 };
    double step = 2.0;
    for (double s : steps) if (secs / s <= 10.0) { step = s; break; }
    g.setFont (mono (11.0f));
    for (double t = 0.0; t <= secs + 1e-9; t += step)
    {
        const float x = area.getX() + (float) (t / secs) * area.getWidth();
        g.setColour (kFaint.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (kDim);
        const juce::String lab = step < 0.1 ? juce::String (juce::roundToInt (t * 1000.0)) + " ms"
                                            : juce::String (t, step < 1.0 ? 2 : 1) + " s";
        if (x + 64.0f < b.getRight())
            g.drawText (lab, (int) x + 3, (int) area.getBottom() + 2, 70, 14, juce::Justification::left);
    }
    g.setColour (kFaint);
    g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

    if (! wave.empty())
    {
        const int cols = (int) area.getWidth();
        const double per = (double) wave.size() / cols;
        std::vector<float> hi ((size_t) cols), lo ((size_t) cols);
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) (c * per), z = juce::jmax (a + 1, (int) ((c + 1) * per));
            float mn = 0, mx = 0;
            for (int i = a; i < z && i < (int) wave.size(); ++i) { mn = juce::jmin (mn, wave[(size_t) i]); mx = juce::jmax (mx, wave[(size_t) i]); }
            hi[(size_t) c] = mid - mx * area.getHeight() * 0.5f;
            lo[(size_t) c] = mid - mn * area.getHeight() * 0.5f;
        }
        juce::Path fill;
        fill.startNewSubPath (area.getX(), hi[0]);
        for (int c = 1; c < cols; ++c) fill.lineTo (area.getX() + (float) c, hi[(size_t) c]);
        for (int c = cols - 1; c >= 0; --c) fill.lineTo (area.getX() + (float) c, lo[(size_t) c]);
        fill.closeSubPath();
        juce::ColourGradient gr (kBrass.withAlpha (0.95f), area.getX(), mid, kBrass.withAlpha (0.40f), area.getRight(), mid, false);
        g.setGradientFill (gr);
        g.fillPath (fill);
    }

    //  brightness over time: the spectral centroid, log axis 500 Hz .. 20 kHz
    auto yOf = [&] (double hz)
    {
        const double u = (std::log2 (juce::jlimit (500.0, 20000.0, hz)) - std::log2 (500.0)) / (std::log2 (20000.0) - std::log2 (500.0));
        return area.getBottom() - (float) u * area.getHeight();
    };
    g.setFont (mono (10.5f));
    for (double hz : { 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float y = yOf (hz);
        g.setColour (kSteel.withAlpha (0.12f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (kSteel.withAlpha (0.55f));
        g.drawText (juce::String ((int) (hz / 1000.0)) + "k", (int) area.getRight() - 30, (int) y - 12, 28, 12, juce::Justification::right);
    }
    if (bright.size() > 1)
    {
        juce::Path pc;
        bool started = false;
        for (size_t i = 0; i < bright.size(); ++i)
        {
            if (bright[i] <= 0.0f) { started = false; continue; }
            const float x = area.getX() + (float) ((i + 0.5) / bright.size()) * area.getWidth();
            if (! started) { pc.startNewSubPath (x, yOf (bright[i])); started = true; }
            else pc.lineTo (x, yOf (bright[i]));
        }
        g.setColour (kSteel);
        g.strokePath (pc, juce::PathStrokeType (2.0f));
    }

    const float low = ho::Engine::lowestModeHz (params);
    juce::String info = "SIZE " + juce::String (params.size, 1) + " in    LOWEST " + juce::String (juce::roundToInt (low)) + " Hz "
                      + noteOf (low) + "    " + juce::String (ho::Engine::modeCount (params)) + " MODES"
                      + "    DECAY " + msText (params.decay / 1000.0)
                      + "    OPEN " + juce::String (juce::roundToInt (params.open * 100.0f)) + " %";
    if (params.echo > 0.0f)
    {
        const int ti = juce::jlimit (0, ho::kNumTimes - 1, juce::roundToInt (params.time));
        info += "    ECHO " + msText (ho::kTimeBeats[ti] * 60.0 / bpm);
    }
    g.setColour (kInk);
    g.setFont (mono (13.0f, true));
    g.drawText (info, (int) b.getX() + 14, (int) b.getY() + 9, (int) b.getWidth() - 240, 18, juce::Justification::left);
    g.setFont (mono (11.0f));
    g.setColour (kBrass);
    g.drawText ("HIT", (int) b.getRight() - 214, (int) b.getY() + 10, 40, 16, juce::Justification::right);
    g.setColour (kSteel);
    g.drawText ("BRIGHTNESS (Hz)", (int) b.getRight() - 164, (int) b.getY() + 10, 150, 16, juce::Justification::right);
}

//==============================================================================
namespace
{
    juce::Rectangle<float> padArea (juce::Rectangle<float> b) { return b.withTrimmedRight (70.0f); }
    float padRadius (juce::Rectangle<float> a) { return juce::jmin (a.getWidth(), a.getHeight()) * 0.42f; }
    juce::Point<float> padCentre (juce::Rectangle<float> a) { return a.getCentre().translated (0.0f, -8.0f); }
    constexpr float kBellFrom = 0.22f;       // the dome
}

void HitPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    machineart::drawPlate (g, b);

    auto m = b.removeFromRight (70.0f).reduced (12.0f, 16.0f);
    auto bar = [&] (juce::Rectangle<float> r, float frac, juce::Colour c, const char* label, const juce::String& val)
    {
        auto well = r.withTrimmedBottom (30.0f);
        g.setColour (kBack);
        g.fillRoundedRectangle (well, 3.0f);
        const float hgt = juce::jlimit (0.0f, 1.0f, frac) * well.getHeight();
        g.setColour (c);
        g.fillRoundedRectangle (well.withTop (well.getBottom() - hgt), 3.0f);
        g.setColour (kDim);
        g.setFont (mono (9.5f, true));
        g.drawText (label, r.withTop (well.getBottom() + 2).withHeight (12).toNearestInt(), juce::Justification::centred);
        g.setColour (kInk);
        g.drawText (val, r.withTop (well.getBottom() + 14).withHeight (12).toNearestInt(), juce::Justification::centred);
    };
    const float pkDb = 20.0f * std::log10 (juce::jmax (1.0e-5f, meterPeak));
    auto left = m.removeFromLeft (m.getWidth() * 0.5f - 2.0f);
    m.removeFromLeft (4.0f);
    bar (left, (pkDb + 48.0f) / 48.0f, kBrass, "OUT", meterPeak > 1.0e-4f ? juce::String (pkDb, 0) : juce::String ("-"));
    bar (m, -meterGr / 24.0f, kComp, "GR", juce::String (meterGr, 0));

    //  a cymbal from above: lathe rings on the bow, the dome in the middle
    const auto a = padArea (getLocalBounds().toFloat());
    const float r = padRadius (a);
    const auto c = padCentre (a);
    juce::ColourGradient face (kBrass.interpolatedWith (juce::Colours::white, 0.3f * glow).withAlpha (0.30f + 0.55f * glow),
                               c.x - r * 0.35f, c.y - r * 0.45f,
                               kBrass.darker (1.2f).withAlpha (0.55f + 0.35f * glow), c.x + r, c.y + r, true);
    g.setGradientFill (face);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (kBrass.withAlpha (0.18f + 0.3f * glow));
    for (int i = 1; i < 9; ++i)
    {
        const float rr = r * (kBellFrom + (1.0f - kBellFrom) * i / 9.0f);
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.8f);
    }
    g.setColour (kBrass.withAlpha (0.8f + 0.2f * glow));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
    const float br = r * kBellFrom;
    g.setColour (kBrass.brighter (0.4f).withAlpha (0.55f + 0.4f * glow));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (kBack.withAlpha (0.9f));
    g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
    g.setColour (kDim);
    g.setFont (sans (11.5f));
    g.drawText ("bell: ping   bow: ride   edge: crash", juce::Rectangle<float> (a.getX(), c.y + r + 4, a.getWidth(), 16).toNearestInt(),
                juce::Justification::centred);
}

void HitPad::mouseDown (const juce::MouseEvent& e)
{
    //  where on the cymbal the stick lands: the bell, then out across the bow
    //  to the edge
    const auto a = padArea (getLocalBounds().toFloat());
    const float d = e.position.getDistanceFrom (padCentre (a)) / padRadius (a);
    if (d > 1.05f) return;
    const float strike = d <= kBellFrom ? 0.0f : juce::jlimit (0.0f, 1.0f, (d - kBellFrom) / (1.0f - kBellFrom));
    if (onHit) onHit (0.9f, ho::ART_DIALLED, strike);
    flash();
}

//==============================================================================
HatsOffEditor::Knob* HatsOffEditor::knobFor (const char* id)
{
    for (auto& k : knobs) if (std::strcmp (ho::specs()[k->spec].id, id) == 0) return k.get();
    return nullptr;
}

HatsOffEditor::HatsOffEditor (HatsOffProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);

    auto colourFor = [] (const char* section, const char* id)
    {
        const juce::String s (section), i (id);
        if (s == "METAL") return kMetal;
        if (s == "HAT")   return kHat;
        if (s == "HIT")   return kHitC;
        if (s == "EQ")    return kEq;
        if (s == "ERA")   return kEra;
        if (s == "ECHO")  return kEcho;
        if (i == "attack" || i == "sustain") return kTrans;
        if (i == "drive" || i == "colour")   return kDrive;
        if (i == "comp" || i == "speed")     return kComp;
        return kOut;
    };

    for (int i = 0; i < ho::kNumParams; ++i)
    {
        const auto& s = ho::specs()[i];
        if (s.kind != ho::K_FLOAT) continue;
        auto k = std::make_unique<Knob>();
        k->spec = i;
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, colourFor (s.section, s.id));
        k->slider.setTooltip (s.hint);
        if (juce::String (s.unit) == "bi" || juce::String (s.id) == "pitch") k->slider.getProperties().set ("bipolar", true);
        if (juce::String (s.section) == "METAL") k->slider.getProperties().set ("knob", "knob-bronze.png");
        if (juce::String (s.id) == "drive")      k->slider.getProperties().set ("knob", "knob-red.png");
        k->slider.setDoubleClickReturnValue (true, s.def);
        k->label.setText (s.label, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (sans (12.5f, true));
        k->label.setColour (juce::Label::textColourId, kDim);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, s.id, k->slider);
        knobs.push_back (std::move (k));
    }

    const int engIdx = specIndex ("engine");
    auto* engParam = proc.apvts.getParameter ("engine");
    engineAttach = std::make_unique<juce::ParameterAttachment> (*engParam, [this] (float v)
    {
        const int sel = juce::roundToInt (v);
        for (int e = 0; e < ho::NUM_DRIVE_ENGINES; ++e) engineBtn[e].setToggleState (e == sel, juce::dontSendNotification);
    });
    const char* shortName[ho::NUM_DRIVE_ENGINES] = { "IDLE BURN", "HYPERDRIVE", "RAZOR WING", "SUPERNOVA" };
    for (int e = 0; e < ho::NUM_DRIVE_ENGINES; ++e)
    {
        auto& b = engineBtn[e];
        b.setButtonText (shortName[e]);
        b.setColour (juce::TextButton::buttonOnColourId, kDrive);
        b.setTooltip (ho::specs()[engIdx].hint);
        b.onClick = [this, e] { engineAttach->setValueAsCompleteGesture ((float) e); };
        addAndMakeVisible (b);
    }
    engineAttach->sendInitialUpdate();
    auto smallLabel = [this] (juce::Label& l, const char* text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (sans (12.5f, true));
        l.setColour (juce::Label::textColourId, kDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    smallLabel (engineLabel, "ENGINE");

    chokeBtn.setClickingTogglesState (true);
    chokeBtn.setColour (juce::TextButton::buttonOnColourId, kHat);
    chokeBtn.setTooltip (ho::specs()[specIndex ("choke")].hint);
    addAndMakeVisible (chokeBtn);
    chokeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "choke", chokeBtn);

    auto choiceBox = [this] (juce::ComboBox& box, const char* id)
    {
        const auto& s = ho::specs()[specIndex (id)];
        const auto items = juce::StringArray::fromTokens (s.choices, "|", "");
        for (int i = 0; i < items.size(); ++i) box.addItem (items[i], i + 1);
        box.setTooltip (s.hint);
        addAndMakeVisible (box);
    };
    choiceBox (timeBox, "time");
    choiceBox (keysBox, "keys");
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "time", timeBox);
    keysAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "keys", keysBox);
    smallLabel (timeLabel, "TIME");
    smallLabel (keysLabel, "KEYS");
    bpmLabel.setFont (mono (11.5f));
    bpmLabel.setColour (juce::Label::textColourId, kDim);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setTooltip ("The tempo the echo follows: the host's, or 120 on its own.");
    shownBpm = proc.hostBpm();
    bpmLabel.setText (juce::String (juce::roundToInt (shownBpm)) + " BPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);

    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) { proc.setCurrentProgram (id - 1); dirty = true; }
    };
    addAndMakeVisible (presetBox);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &loadBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, kBrass);
        addAndMakeVisible (*b);
    }
    saveBtn.setTooltip ("Save this cymbal as a patch in Documents\\Brokild patches\\Hats Off.");
    loadBtn.setTooltip ("Load a patch.");
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save cymbal", proc.patchFolder().getChildFile (proc.currentName() + ".json"), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  if (! f.hasFileExtension ("json")) f = f.withFileExtension ("json");
                                  proc.saveUserPatch (f, f.getFileNameWithoutExtension());
                                  refreshPresetBox();
                              });
    };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load cymbal", proc.patchFolder(), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPatch (f)) { refreshPresetBox(); dirty = true; }
                              });
    };

    addAndMakeVisible (display);
    pad.onHit = [this] (float v, int artic, float strike) { proc.triggerFromUI (v, artic, strike); };
    addAndMakeVisible (pad);
    const std::pair<juce::TextButton*, int> artics[] = { { &closedBtn, ho::ART_CLOSED }, { &pedalBtn, ho::ART_PEDAL }, { &openBtn, ho::ART_OPEN } };
    for (auto [btn, a] : artics)
    {
        btn->setColour (juce::TextButton::buttonOnColourId, kHat);
        btn->onClick = [this, a = a] { proc.triggerFromUI (0.9f, a); pad.flash(); };
        addAndMakeVisible (*btn);
    }
    closedBtn.setTooltip ("The hat closed (C, or MIDI note 42).");
    pedalBtn.setTooltip ("The foot alone: the plates clapped shut (P, or MIDI note 44).");
    openBtn.setTooltip ("The hat open (O, or MIDI note 46). A closed hit chokes it when CHOKE is on.");

    for (int i = 0; i < ho::kNumParams; ++i)
        proc.apvts.addParameterListener (ho::specs()[i].id, this);

    setSize (kW, kH);
    rebuildDisplay();
    lastHits = proc.hitCount();
    startTimerHz (30);
}

HatsOffEditor::~HatsOffEditor()
{
    stopTimer();
    for (int i = 0; i < ho::kNumParams; ++i)
        proc.apvts.removeParameterListener (ho::specs()[i].id, this);
    setLookAndFeel (nullptr);
}

void HatsOffEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    juce::String bank;
    for (int i = 0; i < ho::numPresets(); ++i)
    {
        const auto& pr = ho::preset (i);
        if (bank != pr.bank) { bank = pr.bank; if (bank != "INIT") presetBox.addSectionHeading (bank); }
        presetBox.addItem (pr.name, i + 1);
    }
    const int cur = proc.getCurrentProgram();
    shownProgram = cur; shownName = proc.currentName();
    if (proc.currentName() == ho::preset (cur).name) presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else presetBox.setText (proc.currentName(), juce::dontSendNotification);
}

void HatsOffEditor::stepPreset (int delta)
{
    const int n = ho::numPresets();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    refreshPresetBox();
    dirty = true;
}

void HatsOffEditor::parameterChanged (const juce::String&, float) { dirty = true; }

void HatsOffEditor::rebuildDisplay()
{
    dirty = false;
    const auto p = proc.currentParams();
    const double bpm = proc.hostBpm();
    const double sr = 24000.0;
    //  how long this hit rings: the plate's decay shrinks as the hat closes
    const double openDamp = 0.025 + 0.975 * (double) p.open * p.open;
    double len = p.decay / 1000.0 * openDamp * 1.1 + p.room * 0.8;
    if (p.echo > 0.0f)
    {
        const int ti = juce::jlimit (0, ho::kNumTimes - 1, juce::roundToInt (p.time));
        len = juce::jmax (len, ho::kTimeBeats[ti] * 60.0 / bpm * 2.3);
    }
    const double secs = juce::jlimit (0.12, 4.0, len);
    const int lat = ho::Engine::kDecimatorLatency + ho::Engine::kDriveLatency + (int) std::lround (0.0015 * sr);
    const int n = (int) (secs * sr) + lat;
    std::vector<float> L, R;
    ho::Engine::renderHit (p, sr, ho::ART_DIALLED, 0.9f, L, R, n);
    std::vector<float> w ((size_t) (n - lat));
    for (int i = lat; i < n; ++i) w[(size_t) (i - lat)] = 0.5f * (L[(size_t) i] + R[(size_t) i]);

    //  the spectral centroid in ~60 windows across the hit (a 256-point FFT
    //  each), where there is enough signal to have one
    const int nb = 60, N = 256;
    std::vector<float> br ((size_t) nb, 0.0f);
    const int hop = juce::jmax (1, (int) w.size() / nb);
    float wpk = 0.0f; for (float v : w) wpk = juce::jmax (wpk, std::abs (v));
    std::vector<std::complex<double>> tw ((size_t) N);
    for (int i = 0; i < N; ++i) tw[(size_t) i] = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * i / N);
    for (int k = 0; k < nb; ++k)
    {
        const int a = k * hop;
        if (a + N > (int) w.size()) break;
        std::vector<std::complex<double>> x ((size_t) N);
        double e = 0;
        for (int i = 0; i < N; ++i) { const double v = w[(size_t) (a + i)]; e += v * v; x[(size_t) i] = v * (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / N)); }
        if (std::sqrt (e / N) < wpk * 0.01) continue;      // under -40 dB there is nothing to read
        double num = 0, den = 0;
        for (int b = 1; b < N / 2; ++b)
        {
            std::complex<double> s (0, 0);
            for (int i = 0; i < N; ++i) s += x[(size_t) i] * tw[(size_t) ((b * i) % N)];
            const double pw = std::norm (s);
            num += pw * b * sr / N; den += pw;
        }
        br[(size_t) k] = (float) (num / (den + 1e-30));
    }
    //  a three-point median: a noisy window should not draw a spike
    {
        auto m = br;
        for (size_t k = 1; k + 1 < br.size(); ++k)
            if (br[k - 1] > 0.0f && br[k] > 0.0f && br[k + 1] > 0.0f)
                m[k] = std::max (std::min (br[k - 1], br[k]), std::min (std::max (br[k - 1], br[k]), br[k + 1]));
        br = m;
    }
    display.setHit (std::move (w), std::move (br), secs, p, bpm);
}

void HatsOffEditor::timerCallback()
{
    const double bpm = proc.hostBpm();
    if (std::abs (bpm - shownBpm) > 0.05)
    {
        shownBpm = bpm;
        bpmLabel.setText (juce::String (bpm, bpm == std::round (bpm) ? 0 : 1) + " BPM", juce::dontSendNotification);
        dirty = true;
    }
    if (dirty && ++ticks >= 3) { ticks = 0; rebuildDisplay(); }
    if (proc.getCurrentProgram() != shownProgram || proc.currentName() != shownName) refreshPresetBox();
    const int h = proc.hitCount();
    if (h != lastHits) { lastHits = h; pad.flash(); }
    pad.decay();
    pad.meterPeak = juce::jmax (proc.meterPeak(), pad.meterPeak * 0.85f);
    pad.meterGr = proc.meterGrDb();
    pad.repaint();
}

bool HatsOffEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { proc.triggerFromUI (0.9f, ho::ART_DIALLED); return true; }
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == 'c') { proc.triggerFromUI (0.9f, ho::ART_CLOSED); return true; }
    if (c == 'p') { proc.triggerFromUI (0.9f, ho::ART_PEDAL);  return true; }
    if (c == 'o') { proc.triggerFromUI (0.9f, ho::ART_OPEN);   return true; }
    if (c == 'b') { proc.triggerFromUI (0.9f, ho::ART_BELL);   return true; }
    if (c == 'e') { proc.triggerFromUI (0.9f, ho::ART_EDGE);   return true; }
    return false;
}

//==============================================================================
void HatsOffEditor::paint (juce::Graphics& g)
{
    //  the machine: a lathe in a cymbal foundry, machine-tool green
    machineart::drawGround (g, getLocalBounds().toFloat(), "ground-hats.jpg", 0.75f, 0.30f);
    auto head = getLocalBounds().removeFromTop (64);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (head);
    g.setColour (kBrass);
    g.fillRect (0, 62, getWidth(), 2);
    const auto plate = machineart::drawNameplate (g, { 14.0f, 4.0f, 320.0f, 56.0f }, "plate-hats.png");
    if (! plate.isEmpty())
    {
        g.setColour (kInk.withAlpha (0.7f));
        g.setFont (mono (11.0f));
        g.drawText (juce::String ("BROKILD  ") + HO_BUILD_ID, (int) plate.getRight() + 10, 38, 150, 14, juce::Justification::left);
    }
    else
    {

    //  the wordmark: a cymbal on edge, then the name
    {
        juce::Path cym;
        cym.addEllipse (16.0f, 26.0f, 38.0f, 11.0f);
        g.setColour (kBrass.withAlpha (0.9f));
        g.fillPath (cym);
        g.setColour (kBrass.brighter (0.5f));
        g.fillEllipse (29.0f, 21.0f, 12.0f, 9.0f);
        g.setColour (kSteel);
        g.fillRect (34.0f, 12.0f, 2.0f, 12.0f);
        g.fillRect (34.0f, 36.0f, 2.0f, 14.0f);
    }
    g.setColour (kInk);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)).withExtraKerningFactor (0.10f));
    g.drawText ("HATS OFF", 64, 12, 330, 40, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (mono (11.0f));
    g.drawText (juce::String ("BROKILD  ") + HO_BUILD_ID, 66, 44, 200, 14, juce::Justification::left);
    }

    for (const auto& s : sections)
    {
        machineart::drawPlate (g, s.r.toFloat());
        g.setColour (s.colour);
        g.fillRoundedRectangle (s.r.toFloat().removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
        g.setFont (sans (13.0f, true));
        g.drawText (s.title, s.r.getX() + 12, s.r.getY() + 8, s.r.getWidth() - 24, 16, juce::Justification::left);
    }
}

void HatsOffEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const char*> ids)
{
    const int n = (int) ids.size();
    const int w = area.getWidth() / juce::jmax (1, n);
    for (const char* id : ids)
    {
        auto cell = area.removeFromLeft (w);
        if (auto* k = knobFor (id))
        {
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell.reduced (2, 0));
        }
    }
}

void HatsOffEditor::resized()
{
    sections.clear();
    presetBox.setBounds (470, 14, 360, 36);
    prevBtn.setBounds (430, 14, 34, 36);
    nextBtn.setBounds (836, 14, 34, 36);
    saveBtn.setBounds (kW - 180, 16, 76, 32);
    loadBtn.setBounds (kW - 96, 16, 76, 32);

    display.setBounds (16, 76, 980, 256);
    const int px = 1008, pw = kW - 16 - px;
    pad.setBounds (px, 76, pw, 214);
    {
        auto row = juce::Rectangle<int> (px, 296, pw, 36);
        const int bw = (row.getWidth() - 16) / 3;
        closedBtn.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (8);
        pedalBtn.setBounds (row.removeFromLeft (bw));  row.removeFromLeft (8);
        openBtn.setBounds (row);
    }

    auto sec = [&] (const char* title, juce::Colour c, juce::Rectangle<int> r)
    {
        sections.push_back ({ title, c, r });
        return r.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (10);
    };

    const int y1 = 344, h1 = 172;
    layoutKnobs (sec ("METAL", kMetal, { 16, y1, 664, h1 }), { "pitch", "size", "bronze", "density", "bloom", "trash", "decay", "noise" });
    auto hat = sec ("HAT", kHat, { 692, y1, 340, h1 });
    {
        auto chokeRow = hat.removeFromBottom (26);
        chokeBtn.setBounds (chokeRow.withSizeKeepingCentre (120, 26));
        hat.removeFromBottom (4);
        layoutKnobs (hat, { "open", "sizzle", "chick" });
    }
    layoutKnobs (sec ("HIT", kHitC, { 1044, y1, 164, h1 }), { "strike", "stick" });
    layoutKnobs (sec ("EQ",  kEq,   { 1220, y1, kW - 16 - 1220, h1 }), { "cut", "air" });

    const int y2 = y1 + h1 + 12, h2 = kH - 16 - y2;
    layoutKnobs (sec ("ERA", kEra, { 16, y2, 259, h2 }), { "grit", "room", "width" });
    auto echo = sec ("ECHO", kEcho, { 287, y2, 197, h2 });
    {
        auto col = echo.removeFromRight (100);
        layoutKnobs (echo, { "echo" });
        timeLabel.setBounds (col.removeFromTop (16));
        col.removeFromTop (8);
        timeBox.setBounds (col.removeFromTop (30).reduced (4, 0));
        col.removeFromTop (8);
        bpmLabel.setBounds (col.removeFromTop (18));
    }
    layoutKnobs (sec ("TRANSIENT", kTrans, { 496, y2, 178, h2 }), { "attack", "sustain" });
    auto drive = sec ("DRIVE", kDrive, { 686, y2, 302, h2 });
    {
        auto engineCol = drive.removeFromLeft (124);
        engineLabel.setBounds (engineCol.removeFromTop (16));
        engineCol.removeFromTop (2);
        const int bh = juce::jmin (26, (engineCol.getHeight() - 6) / ho::NUM_DRIVE_ENGINES);
        for (auto& b : engineBtn) b.setBounds (engineCol.removeFromTop (bh).reduced (4, 2));
        layoutKnobs (drive, { "drive", "colour" });
    }
    layoutKnobs (sec ("COMP", kComp, { 1000, y2, 178, h2 }), { "comp", "speed" });
    auto out = sec ("OUT", kOut, { 1190, y2, kW - 16 - 1190, h2 });
    {
        auto keyRow = out.removeFromBottom (28);
        keysLabel.setBounds (keyRow.removeFromLeft (52));
        keysBox.setBounds (keyRow.reduced (2, 0));
        out.removeFromBottom (4);
        layoutKnobs (out, { "level", "velo" });
    }
}
