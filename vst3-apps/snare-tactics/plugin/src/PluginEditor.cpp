#include "PluginEditor.h"
#include "MachineArt.h"      // the drum family's shared machine parts

namespace
{
    //  Kickstart's panel, and its colours, so the two sit together; the snare
    //  adds steel for the wires
    const juce::Colour kBack   { 0xff0e0f11 };
    const juce::Colour kPanel  { 0xff17191d };
    const juce::Colour kPanel2 { 0xff1f2227 };
    const juce::Colour kInk    { 0xffece8df };
    const juce::Colour kDim    { 0xff858c98 };
    const juce::Colour kFaint  { 0xff2c3037 };
    const juce::Colour kHot    { 0xffff5a1f };   // the head
    const juce::Colour kSteel  { 0xff6fd0ff };   // the wires
    const juce::Colour kAmber  { 0xffffb020 };   // the head's pitch

    const juce::Colour kHead   { 0xffff5a1f };
    const juce::Colour kWires  { 0xff6fd0ff };
    const juce::Colour kHitC   { 0xffffb020 };
    const juce::Colour kEra    { 0xff36c9bd };
    const juce::Colour kEcho   { 0xff8ee06a };
    const juce::Colour kTrans  { 0xffa184ff };
    const juce::Colour kDrive  { 0xffff3d4a };
    const juce::Colour kComp   { 0xff4a7dff };
    const juce::Colour kOut    { 0xffcfd3da };

    constexpr int kW = 1400, kH = 724;

    int specIndex (const char* id)
    {
        for (int i = 0; i < st::kNumParams; ++i)
            if (std::strcmp (st::specs()[i].id, id) == 0) return i;
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
}

//==============================================================================
StLook::StLook()
{
    setColour (juce::Slider::textBoxTextColourId, kInk);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kInk);
    setColour (juce::ComboBox::backgroundColourId, kPanel2);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kFaint);
    setColour (juce::ComboBox::arrowColourId, kHot);
    setColour (juce::PopupMenu::backgroundColourId, kPanel2);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::headerTextColourId, kHot);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kHot.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::textColourOffId, kInk);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::TooltipWindow::backgroundColourId, kPanel2);
    setColour (juce::TooltipWindow::textColourId, kInk);
}

void StLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
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

void StLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    //  a steel-bezel push button, lit in its section colour when on
    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),
                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());
}

void StLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    machineart::drawTape (g, r);                  // embossed label tape
    juce::Path tri;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (kHot);
    g.fillPath (tri);
}

juce::Font StLook::getComboBoxFont (juce::ComboBox& b) { return sans (b.getHeight() > 30 ? 17.0f : 14.0f, true); }

void StLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void StLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (kPanel2);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (kSteel.withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (kInk);
    g.setFont (sans (13.5f));
    g.drawFittedText (text, 10, 6, w - 20, h - 12, juce::Justification::centredLeft, 5);
}

juce::Rectangle<int> StLook::getTooltipBounds (const juce::String& text, juce::Point<int> at, juce::Rectangle<int> parent)
{
    const int w = 320;
    const int lines = juce::jmax (1, (int) std::ceil (text.length() / 42.0));
    const int h = 14 + lines * 18;
    auto r = juce::Rectangle<int> (at.x + 14, at.y + 18, w, h);
    return r.constrainedWithin (parent);
}

//==============================================================================
void HitDisplay::setHit (std::vector<float> f, std::vector<float> h, double s, const st::Params& p, double b)
{
    full = std::move (f); head = std::move (h); secs = s; params = p; bpm = b;
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

    const double steps[] = { 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0 };
    double step = 1.0;
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

    //  min/max per column, for one layer
    auto layer = [&] (const std::vector<float>& w, juce::Colour c0, float a0, float a1)
    {
        if (w.empty()) return;
        const int cols = (int) area.getWidth();
        const double per = (double) w.size() / cols;
        std::vector<float> hi ((size_t) cols), lo ((size_t) cols);
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) (c * per), z = juce::jmax (a + 1, (int) ((c + 1) * per));
            float mn = 0, mx = 0;
            for (int i = a; i < z && i < (int) w.size(); ++i) { mn = juce::jmin (mn, w[(size_t) i]); mx = juce::jmax (mx, w[(size_t) i]); }
            hi[(size_t) c] = mid - mx * area.getHeight() * 0.5f;
            lo[(size_t) c] = mid - mn * area.getHeight() * 0.5f;
        }
        juce::Path fill;
        fill.startNewSubPath (area.getX(), hi[0]);
        for (int c = 1; c < cols; ++c) fill.lineTo (area.getX() + (float) c, hi[(size_t) c]);
        for (int c = cols - 1; c >= 0; --c) fill.lineTo (area.getX() + (float) c, lo[(size_t) c]);
        fill.closeSubPath();
        juce::ColourGradient gr (c0.withAlpha (a0), area.getX(), mid, c0.withAlpha (a1), area.getRight(), mid, false);
        g.setGradientFill (gr);
        g.fillPath (fill);
    };
    //  the whole snare in steel, the head alone in warm over it: what is not
    //  orange is the wires, the stick, the clap and the room
    layer (full, kSteel, 0.85f, 0.35f);
    layer (head, kHot, 0.95f, 0.45f);

    //  the head's pitch, on a log axis 60 Hz .. 2 kHz, from the engine's law
    auto yOf = [&] (double hz)
    {
        const double u = (std::log2 (juce::jlimit (60.0, 2000.0, hz)) - std::log2 (60.0)) / (std::log2 (2000.0) - std::log2 (60.0));
        return area.getBottom() - (float) u * area.getHeight();
    };
    g.setFont (mono (10.5f));
    for (double hz : { 100.0, 200.0, 500.0, 1000.0 })
    {
        const float y = yOf (hz);
        g.setColour (kAmber.withAlpha (0.13f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (kAmber.withAlpha (0.55f));
        g.drawText (hz >= 1000.0 ? "1k" : juce::String ((int) hz), (int) area.getRight() - 30, (int) y - 12, 28, 12, juce::Justification::right);
    }
    juce::Path pc;
    const int N = 400;
    const double D = juce::jmax (0.01, params.decay / 1000.0), bend = juce::jmax (0.0005, params.bend / 1000.0);
    for (int i = 0; i <= N; ++i)
    {
        const double t = secs * i / N;
        if (t > D * 1.2) break;
        const double ae = std::exp (-6.9078 * t / D);
        const double semis = params.drop * std::exp (-t / bend) + params.skin * 1.2 * ae;
        const double hz = params.tune * std::pow (2.0, semis / 12.0);
        const float x = area.getX() + (float) (t / secs) * area.getWidth();
        if (i == 0) pc.startNewSubPath (x, yOf (hz)); else pc.lineTo (x, yOf (hz));
    }
    g.setColour (kAmber);
    g.strokePath (pc, juce::PathStrokeType (2.0f));

    //  the readout
    char nb[32];
    const double start = params.tune * std::pow (2.0, (params.drop + params.skin * 1.2) / 12.0);
    juce::String info = "TUNE " + juce::String (juce::roundToInt (params.tune)) + " Hz " + st::noteName (params.tune, nb, 32)
                      + "    STARTS " + juce::String (juce::roundToInt (start)) + " Hz"
                      + "    HEAD " + msText (params.decay / 1000.0)
                      + "    WIRES " + msText (params.sizzle / 1000.0);
    if (params.echo > 0.0f)
    {
        const int ti = juce::jlimit (0, st::kNumTimes - 1, juce::roundToInt (params.time));
        info += "    ECHO " + msText (st::kTimeBeats[ti] * 60.0 / bpm);
    }
    g.setColour (kInk);
    g.setFont (mono (13.0f, true));
    g.drawText (info, (int) b.getX() + 14, (int) b.getY() + 9, (int) b.getWidth() - 250, 18, juce::Justification::left);
    g.setFont (mono (11.0f));
    g.setColour (kHot);
    g.drawText ("HEAD", (int) b.getRight() - 234, (int) b.getY() + 10, 44, 16, juce::Justification::right);
    g.setColour (kSteel);
    g.drawText ("WIRES", (int) b.getRight() - 184, (int) b.getY() + 10, 52, 16, juce::Justification::right);
    g.setColour (kAmber);
    g.drawText ("PITCH (Hz)", (int) b.getRight() - 124, (int) b.getY() + 10, 110, 16, juce::Justification::right);
}

//==============================================================================
namespace
{
    juce::Rectangle<float> padArea (juce::Rectangle<float> b) { return b.withTrimmedRight (70.0f); }
    float padRadius (juce::Rectangle<float> a) { return juce::jmin (a.getWidth(), a.getHeight()) * 0.40f; }
    juce::Point<float> padCentre (juce::Rectangle<float> a) { return a.getCentre().translated (0.0f, -8.0f); }
    constexpr float kRimFrom = 0.80f;       // the outer fifth of the drum is the hoop
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
    bar (left, (pkDb + 48.0f) / 48.0f, kHot, "OUT", meterPeak > 1.0e-4f ? juce::String (pkDb, 0) : juce::String ("-"));
    bar (m, -meterGr / 24.0f, kComp, "GR", juce::String (meterGr, 0));

    //  the drum from above: a chrome hoop (the rim shot) around the head
    const auto a = padArea (getLocalBounds().toFloat());
    const float r = padRadius (a);
    const auto c = padCentre (a);
    g.setColour (juce::Colour (0xff5d636d).interpolatedWith (kAmber, 0.6f * glow));
    g.drawEllipse (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f, r * 0.2f);
    const float hr = r * kRimFrom;
    juce::ColourGradient face (kSteel.interpolatedWith (juce::Colours::white, 0.25f * glow).withAlpha (0.14f + 0.7f * glow),
                               c.x - hr * 0.3f, c.y - hr * 0.4f,
                               kSteel.darker (0.9f).withAlpha (0.35f + 0.5f * glow), c.x + hr, c.y + hr, true);
    g.setGradientFill (face);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    g.setColour (kSteel.withAlpha (0.5f + 0.45f * glow));
    g.drawEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2, 1.5f);
    g.setColour (kInk);
    g.setFont (sans (22.0f, true));
    g.drawText ("HIT", juce::Rectangle<float> (c.x - hr, c.y - 14, hr * 2, 28).toNearestInt(), juce::Justification::centred);
    g.setColour (kDim);
    g.setFont (sans (11.5f));
    g.drawText ("head: snare   hoop: rim shot", juce::Rectangle<float> (a.getX(), c.y + r + 4, a.getWidth(), 16).toNearestInt(),
                juce::Justification::centred);
}

void HitPad::mouseDown (const juce::MouseEvent& e)
{
    //  the centre of the head is the hardest hit; the hoop is a rim shot
    const auto a = padArea (getLocalBounds().toFloat());
    const float r = padRadius (a);
    const float d = e.position.getDistanceFrom (padCentre (a)) / r;
    if (d > 1.05f) return;
    if (d >= kRimFrom) { if (onHit) onHit (1.0f, st::ART_RIM); }
    else if (onHit) onHit (juce::jlimit (0.3f, 1.0f, 1.0f - 0.7f * d / kRimFrom), st::ART_SNARE);
    flash();
}

//==============================================================================
SnareTacticsEditor::Knob* SnareTacticsEditor::knobFor (const char* id)
{
    for (auto& k : knobs) if (std::strcmp (st::specs()[k->spec].id, id) == 0) return k.get();
    return nullptr;
}

SnareTacticsEditor::SnareTacticsEditor (SnareTacticsProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);

    auto colourFor = [] (const char* section, const char* id)
    {
        const juce::String s (section), i (id);
        if (s == "HEAD")  return kHead;
        if (s == "WIRES") return kWires;
        if (s == "HIT")   return kHitC;
        if (s == "ERA")   return kEra;
        if (s == "ECHO")  return kEcho;
        if (i == "attack" || i == "sustain") return kTrans;
        if (i == "drive" || i == "colour")   return kDrive;
        if (i == "comp" || i == "speed")     return kComp;
        return kOut;
    };

    for (int i = 0; i < st::kNumParams; ++i)
    {
        const auto& s = st::specs()[i];
        if (s.kind != st::K_FLOAT) continue;
        auto k = std::make_unique<Knob>();
        k->spec = i;
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, colourFor (s.section, s.id));
        k->slider.setTooltip (s.hint);
        if (juce::String (s.unit) == "bi") k->slider.getProperties().set ("bipolar", true);
        if (juce::String (s.id) == "drive") k->slider.getProperties().set ("knob", "knob-red.png");
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

    // ---- ENGINE ---------------------------------------------------------------
    const int engIdx = specIndex ("engine");
    auto* engParam = proc.apvts.getParameter ("engine");
    engineAttach = std::make_unique<juce::ParameterAttachment> (*engParam, [this] (float v)
    {
        const int sel = juce::roundToInt (v);
        for (int e = 0; e < st::NUM_DRIVE_ENGINES; ++e) engineBtn[e].setToggleState (e == sel, juce::dontSendNotification);
    });
    const char* shortName[st::NUM_DRIVE_ENGINES] = { "IDLE BURN", "HYPERDRIVE", "RAZOR WING", "SUPERNOVA" };
    for (int e = 0; e < st::NUM_DRIVE_ENGINES; ++e)
    {
        auto& b = engineBtn[e];
        b.setButtonText (shortName[e]);
        b.setColour (juce::TextButton::buttonOnColourId, kDrive);
        b.setTooltip (st::specs()[engIdx].hint);
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

    // ---- TIME and KEYS ------------------------------------------------------
    auto choiceBox = [this] (juce::ComboBox& box, const char* id)
    {
        const auto& s = st::specs()[specIndex (id)];
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

    // ---- presets -----------------------------------------------------------
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
        b->setColour (juce::TextButton::buttonOnColourId, kHot);
        addAndMakeVisible (*b);
    }
    saveBtn.setTooltip ("Save this snare as a patch in Documents\\Brokild patches\\Snare Tactics.");
    loadBtn.setTooltip ("Load a patch.");
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save snare", proc.patchFolder().getChildFile (proc.currentName() + ".json"), "*.json");
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
        chooser = std::make_unique<juce::FileChooser> ("Load snare", proc.patchFolder(), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPatch (f)) { refreshPresetBox(); dirty = true; }
                              });
    };

    // ---- the snare ---------------------------------------------------------------
    addAndMakeVisible (display);
    pad.onHit = [this] (float v, int artic) { proc.triggerFromUI (v, artic); };
    addAndMakeVisible (pad);
    const std::pair<juce::TextButton*, int> artics[] = { { &rimBtn, st::ART_RIM }, { &xstickBtn, st::ART_XSTICK }, { &clapBtn, st::ART_CLAP } };
    for (auto [btn, a] : artics)
    {
        btn->setColour (juce::TextButton::buttonOnColourId, kHitC);
        btn->onClick = [this, a = a] { proc.triggerFromUI (1.0f, a); pad.flash(); };
        addAndMakeVisible (*btn);
    }
    rimBtn.setTooltip ("A rim shot: stick on head and hoop together (R, or MIDI note 40).");
    xstickBtn.setTooltip ("A cross-stick: the stick laid across the hoop, the head choked (X, or MIDI note 37).");
    clapBtn.setTooltip ("The clap on its own (C, or MIDI note 39).");

    for (int i = 0; i < st::kNumParams; ++i)
        proc.apvts.addParameterListener (st::specs()[i].id, this);

    setSize (kW, kH);
    rebuildDisplay();
    lastHits = proc.hitCount();
    startTimerHz (30);
}

SnareTacticsEditor::~SnareTacticsEditor()
{
    stopTimer();
    for (int i = 0; i < st::kNumParams; ++i)
        proc.apvts.removeParameterListener (st::specs()[i].id, this);
    setLookAndFeel (nullptr);
}

void SnareTacticsEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    juce::String bank;
    for (int i = 0; i < st::numPresets(); ++i)
    {
        const auto& pr = st::preset (i);
        if (bank != pr.bank) { bank = pr.bank; if (bank != "INIT") presetBox.addSectionHeading (bank); }
        presetBox.addItem (pr.name, i + 1);
    }
    const int cur = proc.getCurrentProgram();
    shownProgram = cur; shownName = proc.currentName();
    if (proc.currentName() == st::preset (cur).name) presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else presetBox.setText (proc.currentName(), juce::dontSendNotification);
}

void SnareTacticsEditor::stepPreset (int delta)
{
    const int n = st::numPresets();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    refreshPresetBox();
    dirty = true;
}

void SnareTacticsEditor::parameterChanged (const juce::String&, float) { dirty = true; }

void SnareTacticsEditor::rebuildDisplay()
{
    dirty = false;
    const auto p = proc.currentParams();
    const double bpm = proc.hostBpm();
    const double sr = 24000.0;
    const int ti = juce::jlimit (0, st::kNumTimes - 1, juce::roundToInt (p.time));
    double len = juce::jmax (p.decay, p.sizzle * (p.tension < 0.5f ? 1.25f : 1.05f), p.clap > 0.0f ? p.sizzle * 0.8f : 0.0f) / 1000.0;
    //  most of a snare's -60 dB tail is invisible at this scale: show three
    //  quarters of it, and a little of the room
    len = len * 0.75 + p.room * 0.8;
    if (p.gate > 0.0f) len = juce::jmin (len, p.gate / 1000.0 + 0.05);
    if (p.echo > 0.0f) len = juce::jmax (len, st::kTimeBeats[ti] * 60.0 / bpm * 2.3);
    const double secs = juce::jlimit (0.12, 3.0, len);
    //  the preview engine's own latency is taken off so t = 0 is the hit
    const int lat = st::Engine::kDecimatorLatency + (int) std::lround (0.0015 * sr);
    const int n = (int) (secs * sr) + lat;
    std::vector<float> full, head;
    st::Engine::renderHit (p, sr, st::ART_SNARE, 1.0f, full, n);
    st::Params hp = p;
    hp.wires = 0.0f; hp.stick = 0.0f; hp.clap = 0.0f; hp.room = 0.0f; hp.echo = 0.0f;
    hp.strike = juce::jmin (hp.strike, 0.69f);        // the head, not the hoop
    st::Engine::renderHit (hp, sr, st::ART_SNARE, 1.0f, head, n);
    full.erase (full.begin(), full.begin() + lat);
    head.erase (head.begin(), head.begin() + lat);
    display.setHit (std::move (full), std::move (head), secs, p, bpm);
}

void SnareTacticsEditor::timerCallback()
{
    const double bpm = proc.hostBpm();
    if (std::abs (bpm - shownBpm) > 0.05)
    {
        shownBpm = bpm;
        bpmLabel.setText (juce::String (bpm, bpm == std::round (bpm) ? 0 : 1) + " BPM", juce::dontSendNotification);
        dirty = true;
    }
    if (dirty && ++ticks >= 2) { ticks = 0; rebuildDisplay(); }
    if (proc.getCurrentProgram() != shownProgram || proc.currentName() != shownName) refreshPresetBox();
    const int h = proc.hitCount();
    if (h != lastHits) { lastHits = h; pad.flash(); }
    pad.decay();
    pad.meterPeak = juce::jmax (proc.meterPeak(), pad.meterPeak * 0.85f);
    pad.meterGr = proc.meterGrDb();
    pad.repaint();
}

bool SnareTacticsEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { proc.triggerFromUI (1.0f, st::ART_SNARE); return true; }
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == 'r') { proc.triggerFromUI (1.0f, st::ART_RIM);    return true; }
    if (c == 'x') { proc.triggerFromUI (1.0f, st::ART_XSTICK); return true; }
    if (c == 'c') { proc.triggerFromUI (1.0f, st::ART_CLAP);   return true; }
    return false;
}

//==============================================================================
void SnareTacticsEditor::paint (juce::Graphics& g)
{
    //  the machine: military field equipment, olive drab
    machineart::drawGround (g, getLocalBounds().toFloat(), "ground-snare.jpg", 0.75f, 0.30f);

    auto head = getLocalBounds().removeFromTop (64);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (head);
    g.setColour (kSteel);
    g.fillRect (0, 62, getWidth(), 2);
    const auto plate = machineart::drawNameplate (g, { 14.0f, 4.0f, 320.0f, 56.0f }, "plate-snare.png");
    if (! plate.isEmpty())
    {
        g.setColour (kInk.withAlpha (0.7f));
        g.setFont (mono (11.0f));
        g.drawText (juce::String ("BROKILD  ") + ST_BUILD_ID, (int) plate.getRight() + 10, 38, 150, 14, juce::Justification::left);
    }
    else
    {

    //  the wordmark: two sticks crossed, then the name
    for (int i = 0; i < 2; ++i)
    {
        juce::Path s;
        const float x0 = 20.0f, x1 = 50.0f, y0 = 16.0f, y1 = 48.0f;
        if (i == 0) s.addQuadrilateral (x0, y0 + 3, x0 + 4, y0, x1, y1 - 3, x1 - 4, y1);
        else        s.addQuadrilateral (x1 - 4, y0, x1, y0 + 3, x0 + 4, y1, x0, y1 - 3);
        g.setColour (i == 0 ? kSteel : kHot);
        g.fillPath (s);
        g.fillEllipse (i == 0 ? x0 - 2.0f : x1 - 5.0f, y0 - 3.0f, 7.0f, 7.0f);   // the tips
    }
    g.setColour (kInk);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)).withExtraKerningFactor (0.10f));
    g.drawText ("SNARE TACTICS", 64, 12, 330, 40, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (mono (11.0f));
    g.drawText (juce::String ("BROKILD  ") + ST_BUILD_ID, 66, 44, 200, 14, juce::Justification::left);
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

void SnareTacticsEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const char*> ids)
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

void SnareTacticsEditor::resized()
{
    sections.clear();

    presetBox.setBounds (470, 14, 360, 36);
    prevBtn.setBounds (430, 14, 34, 36);
    nextBtn.setBounds (836, 14, 34, 36);
    saveBtn.setBounds (kW - 180, 16, 76, 32);
    loadBtn.setBounds (kW - 96, 16, 76, 32);

    // the snare: the display, and the pad with its three buttons under it
    display.setBounds (16, 76, 980, 256);
    const int px = 1008, pw = kW - 16 - px;
    pad.setBounds (px, 76, pw, 214);
    {
        auto row = juce::Rectangle<int> (px, 296, pw, 36);
        const int bw = (row.getWidth() - 16) / 3;
        rimBtn.setBounds (row.removeFromLeft (bw));  row.removeFromLeft (8);
        xstickBtn.setBounds (row.removeFromLeft (bw)); row.removeFromLeft (8);
        clapBtn.setBounds (row);
    }

    auto sec = [&] (const char* title, juce::Colour c, juce::Rectangle<int> r)
    {
        sections.push_back ({ title, c, r });
        return r.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (10);
    };

    // row 1: HEAD | WIRES | HIT
    const int y1 = 344, h1 = 172;
    layoutKnobs (sec ("HEAD",  kHead,  { 16,   y1, 664, h1 }), { "tune", "drop", "bend", "decay", "wave", "skin", "ring", "body" });
    layoutKnobs (sec ("WIRES", kWires, { 692,  y1, 340, h1 }), { "wires", "sizzle", "tension", "air" });
    layoutKnobs (sec ("HIT",   kHitC,  { 1044, y1, kW - 16 - 1044, h1 }), { "strike", "stick", "clap", "spread" });

    // row 2: ERA | ECHO | TRANSIENT | DRIVE | COMP | OUT
    const int y2 = y1 + h1 + 12, h2 = kH - 16 - y2;
    layoutKnobs (sec ("ERA", kEra, { 16, y2, 259, h2 }), { "grit", "room", "gate" });

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
        const int bh = juce::jmin (26, (engineCol.getHeight() - 6) / st::NUM_DRIVE_ENGINES);
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
