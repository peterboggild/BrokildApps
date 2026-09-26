#include "PluginEditor.h"
#include "MachineArt.h"      // the drum family's shared machine parts

namespace
{
    const juce::Colour kBack   { 0xff0e0f11 };
    const juce::Colour kPanel  { 0xff17191d };
    const juce::Colour kPanel2 { 0xff1f2227 };
    const juce::Colour kInk    { 0xffece8df };
    const juce::Colour kDim    { 0xff858c98 };
    const juce::Colour kFaint  { 0xff2c3037 };
    const juce::Colour kHot    { 0xffff5a1f };   // the kick
    const juce::Colour kAmber  { 0xffffb020 };   // its pitch

    //  one colour per section, and the knobs in it wear it
    const juce::Colour kBody   { 0xffff5a1f };
    const juce::Colour kHit    { 0xffffb020 };
    const juce::Colour kEra    { 0xff36c9bd };
    const juce::Colour kTrans  { 0xffa184ff };
    const juce::Colour kDrive  { 0xffff3d4a };
    const juce::Colour kComp   { 0xff4aa8ff };
    const juce::Colour kOut    { 0xffcfd3da };

    constexpr int kW = 1120, kH = 716;

    int specIndex (const char* id)
    {
        for (int i = 0; i < ks::kNumParams; ++i)
            if (std::strcmp (ks::specs()[i].id, id) == 0) return i;
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
}

//==============================================================================
KsLook::KsLook()
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
    setColour (juce::ToggleButton::textColourId, kInk);
    setColour (juce::ToggleButton::tickColourId, kHot);
    setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    setColour (juce::TooltipWindow::backgroundColourId, kPanel2);
    setColour (juce::TooltipWindow::textColourId, kInk);
}

void KsLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float size = (float) juce::jmin (w, h) - 6.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;

    //  the track, then the value arc (from the centre on a bipolar control)
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

    //  the cap: the bakelite knob decal, turned to the value; the drawn cap
    //  below is the fallback if the art is missing
    {
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

    //  the pointer
    const float px = c.x + std::sin (aVal) * (cr - 3.0f), py = c.y - std::cos (aVal) * (cr - 3.0f);
    const float qx = c.x + std::sin (aVal) * (cr * 0.35f), qy = c.y - std::cos (aVal) * (cr * 0.35f);
    g.setColour (s.isEnabled() ? kInk : kDim);
    g.drawLine (qx, qy, px, py, 2.4f);
}

void KsLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    //  a steel-bezel push button, lit in its section colour when on
    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),
                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());
}

void KsLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    machineart::drawTape (g, r);                  // embossed label tape
    juce::Path tri;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (kHot);
    g.fillPath (tri);
}

juce::Font KsLook::getComboBoxFont (juce::ComboBox&) { return sans (17.0f, true); }

void KsLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 36, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void KsLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (kPanel2);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (kHot.withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (kInk);
    g.setFont (sans (13.5f));
    g.drawFittedText (text, 10, 6, w - 20, h - 12, juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> KsLook::getTooltipBounds (const juce::String& text, juce::Point<int> at, juce::Rectangle<int> parent)
{
    const int w = 300;
    const int lines = juce::jmax (1, (int) std::ceil (text.length() / 40.0));
    const int h = 14 + lines * 18;
    auto r = juce::Rectangle<int> (at.x + 14, at.y + 18, w, h);
    return r.constrainedWithin (parent);
}

//==============================================================================
void HitDisplay::setHit (std::vector<float> w, double s, const ks::Params& p)
{
    wave = std::move (w); secs = s; params = p;
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

    //  time grid
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
        if (x + 64.0f < b.getRight())      //  a label cut by the frame is worse than none
            g.drawText (lab, (int) x + 3, (int) area.getBottom() + 2, 70, 14, juce::Justification::left);
    }
    g.setColour (kFaint);
    g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

    //  the waveform, min/max per column
    if (! wave.empty())
    {
        const int cols = (int) area.getWidth();
        const double per = (double) wave.size() / cols;
        juce::Path fill;
        std::vector<float> hi ((size_t) cols), lo ((size_t) cols);
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) (c * per), z = juce::jmax (a + 1, (int) ((c + 1) * per));
            float mn = 0, mx = 0;
            for (int i = a; i < z && i < (int) wave.size(); ++i) { mn = juce::jmin (mn, wave[(size_t) i]); mx = juce::jmax (mx, wave[(size_t) i]); }
            hi[(size_t) c] = mid - mx * area.getHeight() * 0.5f;
            lo[(size_t) c] = mid - mn * area.getHeight() * 0.5f;
        }
        fill.startNewSubPath (area.getX(), hi[0]);
        for (int c = 1; c < cols; ++c) fill.lineTo (area.getX() + (float) c, hi[(size_t) c]);
        for (int c = cols - 1; c >= 0; --c) fill.lineTo (area.getX() + (float) c, lo[(size_t) c]);
        fill.closeSubPath();
        juce::ColourGradient gr (kHot.withAlpha (0.95f), area.getX(), mid, kHot.withAlpha (0.35f), area.getRight(), mid, false);
        g.setGradientFill (gr);
        g.fillPath (fill);
    }

    //  the pitch curve, on a log axis from 20 Hz to 2 kHz, drawn from the
    //  same law the engine runs (a hard hit, as the pad plays it)
    auto yOf = [&] (double hz)
    {
        const double u = (std::log2 (juce::jlimit (20.0, 2000.0, hz)) - std::log2 (20.0)) / (std::log2 (2000.0) - std::log2 (20.0));
        return area.getBottom() - (float) u * area.getHeight();
    };
    g.setFont (mono (10.5f));
    for (double hz : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0 })
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
    const double cp = 1.0 + 7.0 * params.curve;
    for (int i = 0; i <= N; ++i)
    {
        const double t = secs * i / N;
        const double ae = std::exp (-6.9078 * std::pow (t / D, cp));
        const double semis = params.sweep * std::exp (-t / bend) + params.skin * 2.5 * ae;
        const double hz = params.pitch * std::pow (2.0, semis / 12.0);
        const float x = area.getX() + (float) (t / secs) * area.getWidth();
        if (i == 0) pc.startNewSubPath (x, yOf (hz)); else pc.lineTo (x, yOf (hz));
    }
    g.setColour (kAmber);
    g.strokePath (pc, juce::PathStrokeType (2.0f));

    //  the readout
    char nb[32];
    const double start = params.pitch * std::pow (2.0, (params.sweep + params.skin * 2.5) / 12.0);
    const juce::String info = "PITCH " + juce::String (params.pitch, 1) + " Hz  " + ks::noteName (params.pitch, nb, 32)
                            + "     STARTS " + juce::String (juce::roundToInt (start)) + " Hz"
                            + "     60 dB IN " + (params.decay >= 1000.0f ? juce::String (params.decay / 1000.0f, 2) + " s"
                                                                          : juce::String (juce::roundToInt (params.decay)) + " ms");
    g.setColour (kInk);
    g.setFont (mono (13.0f, true));
    g.drawText (info, (int) b.getX() + 14, (int) b.getY() + 9, (int) b.getWidth() - 200, 18, juce::Justification::left);
    g.setFont (mono (11.0f));
    g.setColour (kHot);
    g.drawText ("HIT", (int) b.getRight() - 170, (int) b.getY() + 10, 40, 16, juce::Justification::right);
    g.setColour (kAmber);
    g.drawText ("PITCH (Hz)", (int) b.getRight() - 124, (int) b.getY() + 10, 110, 16, juce::Justification::right);
}

//==============================================================================
void HitPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    machineart::drawPlate (g, b);

    //  meters on the right
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

    //  the pad
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.40f;
    const auto c = b.getCentre().translated (0.0f, -6.0f);
    if (machineart::drawPalmButton (g, juce::Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c), glow > 0.85f, glow, kHot))
    {
        g.setColour (kDim);
        g.setFont (sans (11.5f));
        g.drawText ("click, space or MIDI", juce::Rectangle<float> (b.getX(), c.y + r + 6, b.getWidth(), 16).toNearestInt(),
                    juce::Justification::centred);
        return;
    }
    juce::ColourGradient face (kHot.interpolatedWith (juce::Colours::white, 0.25f * glow).withAlpha (0.18f + 0.8f * glow),
                               c.x - r * 0.3f, c.y - r * 0.4f,
                               kHot.darker (0.9f).withAlpha (0.45f + 0.5f * glow), c.x + r, c.y + r, true);
    g.setGradientFill (face);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (kHot.withAlpha (0.55f + 0.45f * glow));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
    g.setColour (kInk);
    g.setFont (sans (26.0f, true));
    g.drawText ("HIT", juce::Rectangle<float> (c.x - r, c.y - 16, r * 2, 32).toNearestInt(), juce::Justification::centred);
    g.setColour (kDim);
    g.setFont (sans (11.5f));
    g.drawText ("click, space or MIDI", juce::Rectangle<float> (b.getX(), c.y + r + 6, b.getWidth(), 16).toNearestInt(),
                juce::Justification::centred);
}

void HitPad::mouseDown (const juce::MouseEvent& e)
{
    //  the centre of the skin is the hardest hit, the rim the softest
    auto b = getLocalBounds().toFloat().withTrimmedRight (70.0f);
    const auto c = b.getCentre().translated (0.0f, -6.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.40f;
    const float d = e.position.getDistanceFrom (c) / r;
    if (d > 1.05f) return;
    if (onHit) onHit (juce::jlimit (0.3f, 1.0f, 1.0f - 0.7f * d));
    flash();
}

//==============================================================================
KickstartEditor::Knob* KickstartEditor::knobFor (const char* id)
{
    for (auto& k : knobs) if (std::strcmp (ks::specs()[k->spec].id, id) == 0) return k.get();
    return nullptr;
}

KickstartEditor::KickstartEditor (KickstartProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);

    auto colourFor = [] (const char* section, const char* id)
    {
        const juce::String s (section), i (id);
        if (s == "BODY") return kBody;
        if (s == "HIT")  return kHit;
        if (s == "ERA")  return kEra;
        if (i == "attack" || i == "sustain") return kTrans;
        if (i == "drive" || i == "colour")   return kDrive;
        if (i == "comp" || i == "speed")     return kComp;
        return kOut;
    };

    for (int i = 0; i < ks::kNumParams; ++i)
    {
        const auto& s = ks::specs()[i];
        if (s.kind != ks::K_FLOAT) continue;
        auto k = std::make_unique<Knob>();
        k->spec = i;
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
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
        for (int e = 0; e < ks::NUM_DRIVE_ENGINES; ++e) engineBtn[e].setToggleState (e == sel, juce::dontSendNotification);
    });
    const char* shortName[ks::NUM_DRIVE_ENGINES] = { "IDLE BURN", "HYPERDRIVE", "RAZOR WING", "SUPERNOVA" };
    for (int e = 0; e < ks::NUM_DRIVE_ENGINES; ++e)
    {
        auto& b = engineBtn[e];
        b.setButtonText (shortName[e]);
        b.setColour (juce::TextButton::buttonOnColourId, kDrive);
        b.setTooltip (ks::specs()[engIdx].hint);
        b.onClick = [this, e] { engineAttach->setValueAsCompleteGesture ((float) e); };
        addAndMakeVisible (b);
    }
    engineAttach->sendInitialUpdate();
    engineLabel.setText ("ENGINE", juce::dontSendNotification);
    engineLabel.setFont (sans (12.5f, true));
    engineLabel.setColour (juce::Label::textColourId, kDim);
    engineLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (engineLabel);

    keyToggle.setTooltip (ks::specs()[specIndex ("key")].hint);
    addAndMakeVisible (keyToggle);
    keyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "key", keyToggle);

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
    saveBtn.setTooltip ("Save this kick as a patch in Documents\\Brokild patches\\Kickstart.");
    loadBtn.setTooltip ("Load a patch.");
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save kick", proc.patchFolder().getChildFile (proc.currentName() + ".json"), "*.json");
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
        chooser = std::make_unique<juce::FileChooser> ("Load kick", proc.patchFolder(), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPatch (f)) { refreshPresetBox(); dirty = true; }
                              });
    };

    // ---- the kick ---------------------------------------------------------------
    addAndMakeVisible (display);
    pad.onHit = [this] (float v) { proc.triggerFromUI (v); };
    addAndMakeVisible (pad);

    for (int i = 0; i < ks::kNumParams; ++i)
        proc.apvts.addParameterListener (ks::specs()[i].id, this);

    setSize (kW, kH);
    rebuildDisplay();
    lastHits = proc.hitCount();
    startTimerHz (30);
}

KickstartEditor::~KickstartEditor()
{
    stopTimer();
    for (int i = 0; i < ks::kNumParams; ++i)
        proc.apvts.removeParameterListener (ks::specs()[i].id, this);
    setLookAndFeel (nullptr);
}

void KickstartEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    juce::String bank;
    for (int i = 0; i < ks::numPresets(); ++i)
    {
        const auto& pr = ks::preset (i);
        if (bank != pr.bank) { bank = pr.bank; if (bank != "INIT") presetBox.addSectionHeading (bank); }
        presetBox.addItem (pr.name, i + 1);
    }
    const int cur = proc.getCurrentProgram();
    shownProgram = cur; shownName = proc.currentName();
    if (proc.currentName() == ks::preset (cur).name) presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else presetBox.setText (proc.currentName(), juce::dontSendNotification);
}

void KickstartEditor::stepPreset (int delta)
{
    const int n = ks::numPresets();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    refreshPresetBox();
    dirty = true;
}

void KickstartEditor::parameterChanged (const juce::String&, float) { dirty = true; }

void KickstartEditor::rebuildDisplay()
{
    dirty = false;
    const auto p = proc.currentParams();
    const double sr = 24000.0;
    const double secs = juce::jlimit (0.15, 3.0, p.decay / 1000.0 * 1.2 + p.room * 0.35);
    //  the preview engine's own latency is taken off so t = 0 is the hit
    const int lat = ks::Engine::kDecimatorLatency + (int) std::lround (0.0015 * sr);
    std::vector<float> w;
    ks::Engine::renderHit (p, sr, 36, 1.0f, w, (int) (secs * sr) + lat);
    w.erase (w.begin(), w.begin() + lat);
    display.setHit (std::move (w), secs, p);
}

void KickstartEditor::timerCallback()
{
    if (dirty && ++ticks >= 2) { ticks = 0; rebuildDisplay(); }
    //  a program change can come from the host, not only from this panel
    if (proc.getCurrentProgram() != shownProgram || proc.currentName() != shownName) refreshPresetBox();
    const int h = proc.hitCount();
    if (h != lastHits) { lastHits = h; pad.flash(); }
    pad.decay();
    pad.meterPeak = juce::jmax (proc.meterPeak(), pad.meterPeak * 0.85f);
    pad.meterGr = proc.meterGrDb();
    pad.repaint();
}

bool KickstartEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { proc.triggerFromUI (1.0f); return true; }
    return false;
}

//==============================================================================
void KickstartEditor::paint (juce::Graphics& g)
{
    //  the machine: a steam pile-driver in red-lead primer on cast iron
    machineart::drawGround (g, getLocalBounds().toFloat(), "ground-kick.jpg", 0.75f, 0.32f);

    //  header
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (0, 0, getWidth(), 64);
    g.setColour (kHot);
    g.fillRect (0, 62, getWidth(), 2);
    const auto plate = machineart::drawNameplate (g, { 14.0f, 5.0f, 300.0f, 54.0f }, "plate-kick.png");
    if (! plate.isEmpty())
    {
        g.setColour (kInk.withAlpha (0.7f));
        g.setFont (mono (11.0f));
        g.drawText (juce::String ("BROKILD  ") + KS_BUILD_ID, (int) plate.getRight() + 10, 38, 150, 14, juce::Justification::left);
    }
    else
    {

    //  the wordmark: three slashes for the beater, then the name
    for (int i = 0; i < 3; ++i)
    {
        juce::Path sl;
        const float x = 20.0f + i * 11.0f;
        sl.addQuadrilateral (x + 8, 16, x + 14, 16, x + 6, 46, x, 46);
        g.setColour (kHot.withAlpha (1.0f - i * 0.28f));
        g.fillPath (sl);
    }
    g.setColour (kInk);
    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
    g.drawText ("KICKSTART", 64, 12, 260, 40, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (mono (11.0f));
    g.drawText (juce::String ("BROKILD  ") + KS_BUILD_ID, 66, 44, 200, 14, juce::Justification::left);
    }

    //  sections: riveted plates on the machine
    for (const auto& s : sections)
    {
        machineart::drawPlate (g, s.r.toFloat());
        g.setColour (s.colour);
        g.fillRoundedRectangle (s.r.toFloat().removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
        g.setFont (sans (13.0f, true));
        g.drawText (s.title, s.r.getX() + 12, s.r.getY() + 8, s.r.getWidth() - 24, 16, juce::Justification::left);
    }
}

void KickstartEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const char*> ids)
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

void KickstartEditor::resized()
{
    sections.clear();

    // header controls
    presetBox.setBounds (400, 14, 330, 36);
    prevBtn.setBounds (360, 14, 34, 36);
    nextBtn.setBounds (736, 14, 34, 36);
    saveBtn.setBounds (kW - 180, 16, 76, 32);
    loadBtn.setBounds (kW - 96, 16, 76, 32);

    display.setBounds (16, 76, 760, 262);
    pad.setBounds (788, 76, kW - 16 - 788, 262);

    auto sec = [&] (const char* title, juce::Colour c, juce::Rectangle<int> r)
    {
        sections.push_back ({ title, c, r });
        return r.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (10);
    };

    // row 1: BODY | HIT | ERA
    const int y1 = 352, h1 = 172;
    layoutKnobs (sec ("BODY", kBody, { 16, y1, 640, h1 }), { "pitch", "sweep", "bend", "decay", "curve", "wave", "skin" });
    layoutKnobs (sec ("HIT",  kHit,  { 668, y1, 214, h1 }), { "click", "tone" });
    layoutKnobs (sec ("ERA",  kEra,  { 894, y1, kW - 16 - 894, h1 }), { "grit", "room" });

    // row 2: TRANSIENT | DRIVE | COMP | OUT
    const int y2 = y1 + h1 + 12, h2 = kH - 16 - y2;
    layoutKnobs (sec ("TRANSIENT", kTrans, { 16, y2, 214, h2 }), { "attack", "sustain" });

    auto drive = sec ("DRIVE", kDrive, { 242, y2, 400, h2 });
    {
        auto engineCol = drive.removeFromLeft (132);
        engineLabel.setBounds (engineCol.removeFromTop (16));
        engineCol.removeFromTop (2);
        const int bh = juce::jmin (26, (engineCol.getHeight() - 6) / ks::NUM_DRIVE_ENGINES);
        for (auto& b : engineBtn) { b.setBounds (engineCol.removeFromTop (bh).reduced (4, 2)); }
        layoutKnobs (drive, { "drive", "colour" });
    }
    layoutKnobs (sec ("COMP", kComp, { 654, y2, 214, h2 }), { "comp", "speed" });

    auto out = sec ("OUT", kOut, { 880, y2, kW - 16 - 880, h2 });
    {
        auto keyRow = out.removeFromBottom (24);
        keyToggle.setBounds (keyRow.withSizeKeepingCentre (120, 24));
        layoutKnobs (out, { "level", "velo" });
    }
}
