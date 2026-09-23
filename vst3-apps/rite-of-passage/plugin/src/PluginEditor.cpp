#include "PluginEditor.h"

using namespace rop;

namespace
{
    //  §12. Warm, burnt, used — and one cold accent that belongs to BWFX.
    const juce::Colour kGround { 0xff141010 };   // charred wood
    const juce::Colour kClay   { 0xff2a211c };
    const juce::Colour kOchre  { 0xffb3452a };
    const juce::Colour kYellow { 0xffd39b3a };
    const juce::Colour kAsh    { 0xffe6ddcd };
    const juce::Colour kSmoke  { 0xff6b625a };
    const juce::Colour kEmber  { 0xffff6a2a };   // heat: only what is moving
    const juce::Colour kTeal   { 0xff35c9c0 };   // the BWFX accent, mandated

    constexpr int kMargin = 16;
    constexpr int kPostW  = 18;
    constexpr int kInset  = kMargin + kPostW + 8;   // where content starts
    constexpr int kHeadH  = 210;   // POSITION, the AUTO strip, the MIX GATE
    constexpr int kGlobH  = 78;
    constexpr int kColH   = 15;                     // the column headings
    constexpr int kLaneH  = 44;
    constexpr int kLaneGap = 6;
    constexpr int kAbRow  = 92;

    /*  A decal is found by its ORIGINAL FILENAME, never by the mangled symbol
        juce_add_binary_data generates — a renamed part would otherwise fail to
        compile instead of simply falling back. An empty Image is a valid
        answer and every caller must cope with one. */
    juce::Image loadDecal (const char* filename)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (juce::String (BinaryData::originalFilenames[i]) == filename)
            {
                int sz = 0;
                if (auto* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], sz))
                    return juce::ImageFileFormat::loadFrom (d, (size_t) sz);
            }
        return {};
    }

    //  cover a rectangle with a tiling decal, darkened to the panel's ground
    void tile (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> r,
               float alpha, float scale = 1.0f)
    {
        if (! img.isValid() || r.isEmpty()) return;
        const int w = juce::jmax (8, (int) (img.getWidth() * scale));
        const int h = juce::jmax (8, (int) (img.getHeight() * scale));
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r);
        g.setOpacity (alpha);
        for (int y = r.getY(); y < r.getBottom(); y += h)
            for (int x = r.getX(); x < r.getRight(); x += w)
                g.drawImage (img, x, y, w, h, 0, 0, img.getWidth(), img.getHeight());
        g.setOpacity (1.0f);
    }

    //  draw an image inside a box, keeping its aspect — never stretched
    void fitImage (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> box,
                   float alpha = 1.0f)
    {
        if (! img.isValid() || box.isEmpty()) return;
        const float s = juce::jmin (box.getWidth() / (float) img.getWidth(),
                                    box.getHeight() / (float) img.getHeight());
        const float w = img.getWidth() * s, h = img.getHeight() * s;
        g.setOpacity (alpha);
        g.drawImage (img, box.getCentreX() - w * 0.5f, box.getCentreY() - h * 0.5f, w, h,
                     0, 0, img.getWidth(), img.getHeight());
        g.setOpacity (1.0f);
    }

    /*  Which glyph belongs to which effect. Looked up by the effect's OWN ID
        rather than by its registry index, so appending an effect — which is
        exactly what the second six were — can never silently re-label the
        panel. The shapes are read off the brief's own cell descriptions:
        cell 5 is a triangle breaking into a scatter of dots (GRAIN), cell 9 an
        open circle with a wedge driven into it (BRAKE), cell 11 a column of
        dots growing to a heavy disc (RISER). GAP has no mark of its own — the
        brief drew eleven and reserved cell 12 for absence — so it falls back
        to its name in text. */
    int glyphForEffect (const char* id)
    {
        struct Pair { const char* id; int cell; };
        static const Pair kMap[] = {
            { "climb", 1 }, { "tape", 2 }, { "stutter", 3 }, { "chop", 4 },
            { "grain", 5 }, { "bloom", 6 }, { "freeze", 7 }, { "reverse", 8 },
            { "brake", 9 }, { "dive", 10 }, { "riser", 11 },
        };
        for (const auto& m : kMap)
            if (std::strcmp (m.id, id) == 0) return m.cell - 1;
        return -1;
    }

    //  a wrapped World module, by its id — the registry's own prefix, so
    //  nothing here needs a second list to fall out of date
    bool isWrappedBwfx (const char* id)
    {
        return id != nullptr && std::strncmp (id, "bwfx.", 5) == 0;
    }

    //  what a parameter's value reads as: a choice by NAME, everything else
    //  with its own unit and a sane number of digits. JUCE's default prints
    //  "800.0000000" for a frequency and "0" for a filter mode.
    juce::String paramText (const ParamDesc& pd, double v)
    {
        if (pd.choices != nullptr)
        {
            juce::StringArray opts;
            opts.addTokens (juce::String (pd.choices), "|", "");
            if (opts.size() > 0)
                return opts[juce::jlimit (0, opts.size() - 1, (int) std::lround (v))];
        }
        const juce::String u (pd.unit != nullptr ? pd.unit : "");

        /*  BWFX carries three SCALED display codes — a rate stored in
            hundredths of a hertz reads as hertz — and a wrapped World
            module brings them onto this panel for the first time. Without
            this, ENSEMBLE's rate reads "45.0 cHz" instead of "0.45 Hz".
            The same three lines are in BwfxPanel.cpp:35, which is where
            they came from. */
        if (u == "cHz") return juce::String (v / 100.0, 2) + " Hz";
        if (u == "dHz") return juce::String (v / 10.0,  1) + " Hz";
        if (u == "cs")  return juce::String (v / 100.0, 2) + " s";

        const double span = std::abs (pd.hi - pd.lo);
        const int dp = span >= 200.0 ? 0 : (span >= 20.0 ? 1 : 2);
        return juce::String (v, dp) + (u.isEmpty() ? juce::String() : " " + u);
    }
}

// ===========================================================================
RiteLook::RiteLook()
{
    knobLarge = loadDecal ("knob-large.png");
    knobSmall = loadDecal ("knob-small.png");
    marker    = loadDecal ("marker.png");
}

void RiteLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                 float pos, float a0, float a1, juce::Slider& s)
{
    auto box = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float d = juce::jmin (box.getWidth(), box.getHeight());
    box = box.withSizeKeepingCentre (d, d);
    const float angle = a0 + pos * (a1 - a0);

    //  the fired-clay cap. Its own moulded tab is the pointer, so the whole
    //  image turns; nothing is drawn on top of it but the heat.
    const juce::Image& img = (d >= 54.0f) ? knobLarge : knobSmall;
    if (img.isValid())
    {
        const float sc = d / (float) img.getWidth();
        auto t = juce::AffineTransform::scale (sc)
                   .translated (box.getX(), box.getY())
                   .rotated (angle, box.getCentreX(), box.getCentreY());
        g.drawImageTransformed (img, t);
    }
    else
    {
        //  the procedural fallback the panel had before the decals arrived
        g.setColour (kClay.brighter (0.25f));
        g.fillEllipse (box);
        g.setColour (kSmoke.withAlpha (0.6f));
        g.drawEllipse (box.reduced (1.0f), 1.2f);
    }

    //  the travelled arc, in the slider's own fill colour
    const float r = d * 0.5f;
    juce::Path arc;
    arc.addCentredArc (box.getCentreX(), box.getCentreY(), r - 1.5f, r - 1.5f,
                       0.0f, a0, angle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (0.85f));
    g.strokePath (arc, juce::PathStrokeType (2.0f));
}

void RiteLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                 float sliderPos, float, float,
                                 juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal || ! marker.isValid()
        || s.getComponentID() != "master")
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0.0f, 0.0f, style, s);
        return;
    }

    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float mid = r.getCentreY();
    const float track = juce::jmin (10.0f, r.getHeight() * 0.32f);

    //  the cord the marker is threaded on
    g.setColour (s.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (r.getX(), mid - track * 0.5f, r.getWidth(), track, track * 0.5f);
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (r.getX(), mid - track * 0.5f,
                            juce::jmax (2.0f, sliderPos - r.getX()), track, track * 0.5f);

    //  the river stone, bound in cord
    const float mh = r.getHeight();
    const float mw = mh * marker.getWidth() / (float) marker.getHeight();
    fitImage (g, marker, { sliderPos - mw * 0.5f, r.getY(), mw, mh });
}

// ===========================================================================
#if 0   // retired: BwfxPanel is the rack now
BwfxOverlay::BwfxOverlay (RiteProcessor& p) : proc (p)
{
    setOpaque (true);                       // the whole point: it HIDES the panel
    setInterceptsMouseClicks (true, true);

    heading.setText ("BROKILD WORLD FX", juce::dontSendNotification);
    heading.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    heading.setColour (juce::Label::textColourId, kTeal);
    addAndMakeVisible (heading);

    note.setText ("The rack runs AFTER the whole chain, so it colours the rite rather than "
                  "taking part in it. It starts empty, and an empty rack is bit-transparent. "
                  "These five macros are the rack's only host parameters: automate them, and "
                  "each one maps whatever control it is assigned to across its full range.",
                  juce::dontSendNotification);
    note.setJustificationType (juce::Justification::topLeft);
    note.setColour (juce::Label::textColourId, kSmoke);
    note.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (note);

    for (int i = 0; i < 5; ++i)
    {
        auto m = std::make_unique<Macro>();
        m->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        m->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        m->slider.setColour (juce::Slider::rotarySliderFillColourId, kTeal);
        m->slider.setColour (juce::Slider::textBoxTextColourId, kAsh);
        m->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        m->label.setText ("MACRO " + juce::String (i + 1), juce::dontSendNotification);
        m->label.setJustificationType (juce::Justification::centred);
        m->label.setColour (juce::Label::textColourId, kSmoke);
        m->label.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (m->slider);
        addAndMakeVisible (m->label);
        m->attach = std::make_unique<SliderAttach> (proc.apvts, bwfx_juce::macroParamId (i), m->slider);
        macros.push_back (std::move (m));
    }

    close.setColour (juce::TextButton::textColourOffId, kAsh);
    close.onClick = [this] { setVisible (false); };
    addAndMakeVisible (close);
}

void BwfxOverlay::paint (juce::Graphics& g)
{
    g.fillAll (kGround);                    // opaque, so nothing shows through
    tile (g, ground, getLocalBounds(), 0.30f);
    g.setColour (kTeal.withAlpha (0.22f));
    g.drawRect (getLocalBounds().reduced (10), 1);
}

void BwfxOverlay::resized()
{
    auto r = getLocalBounds().reduced (34);
    heading.setBounds (r.removeFromTop (26));
    close.setBounds (getWidth() - 110, 26, 76, 24);
    note.setBounds (r.removeFromTop (60));
    r.removeFromTop (18);

    auto row = r.removeFromTop (118);
    const int cw = row.getWidth() / 5;
    for (auto& m : macros)
    {
        auto cell = row.removeFromLeft (cw).reduced (8, 0);
        m->label.setBounds (cell.removeFromTop (14));
        m->slider.setBounds (cell);
    }
}

void BwfxOverlay::mouseDown (const juce::MouseEvent&) {}
#endif

// ===========================================================================
std::unique_ptr<RiteEditor::Knob> RiteEditor::makeKnob (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, kYellow);
    k->slider.setColour (juce::Slider::textBoxTextColourId, kAsh);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, kSmoke);
    k->label.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attach = std::make_unique<SliderAttach> (proc.apvts, id, k->slider);
    return k;
}

// ---------------------------------------------------------------------------
RiteEditor::RiteEditor (RiteProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    dGround  = loadDecal ("ground.png");
    dClay    = loadDecal ("clay.png");
    dAshWear = loadDecal ("ashwear.png");
    dLintel  = loadDecal ("lintel.png");
    dPost    = loadDecal ("post.png");
    dSocket  = loadDecal ("socket.png");
    dArrival = loadDecal ("arrival.png");
    for (int i = 0; i < 12; ++i)
        dGlyph[i] = loadDecal (juce::String::formatted ("glyph-%02d.png", i + 1).toRawUTF8());

    title.setText ("RITE OF PASSAGE", juce::dontSendNotification);
    title.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAsh);
    addAndMakeVisible (title);

    readout.setJustificationType (juce::Justification::centredRight);
    readout.setColour (juce::Label::textColourId, kSmoke);
    readout.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (readout);

    /*  THE BUILD, ON THE FACE. Peter could not tell a new install from an
        old one because nothing on this panel said which it was — the one
        plugin in the fleet without a build id. The effect COUNT is printed
        from numEffects() rather than typed, because a typed number can
        disagree with the registry and is then worse than no number. */
    build.setText (juce::String (ROP_BUILD_ID) + "   "
                     + juce::String (numNativeEffects()) + " EFFECTS"
                     + (numEffects() > numNativeEffects()
                          ? "  + " + juce::String (numEffects() - numNativeEffects()) + " BWFX"
                          : juce::String()),
                   juce::dontSendNotification);
    build.setJustificationType (juce::Justification::centredLeft);
    //  smoke on charred wood was unreadable in the render, which defeats the
    //  whole point of printing it — ash, and a size you can take in at a glance
    build.setColour (juce::Label::textColourId, kAsh.withAlpha (0.66f));
    build.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (build);

    //  the marker travelling through the gate
    position.setComponentID ("master");     // RiteLook gives only this one the marker
    position.setSliderStyle (juce::Slider::LinearHorizontal);
    position.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    position.setColour (juce::Slider::trackColourId, kEmber.withAlpha (0.65f));
    position.setColour (juce::Slider::backgroundColourId, kClay);
    position.setColour (juce::Slider::textBoxTextColourId, kAsh);
    position.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (position);
    positionAttach = std::make_unique<SliderAttach> (proc.apvts, rop_ids::position, position);

    arrival.setClickingTogglesState (true);
    arrival.setColour (juce::TextButton::textColourOffId, kOchre);
    arrival.setColour (juce::TextButton::textColourOnId, kGround);
    arrival.setColour (juce::TextButton::buttonOnColourId, kEmber);
    addAndMakeVisible (arrival);
    arrivalAttach = std::make_unique<ButtonAttach> (proc.apvts, rop_ids::arrival, arrival);

    //  a REAL button, not a word drawn on the panel
    bwfxButton.setColour (juce::TextButton::textColourOffId, kTeal);
    bwfxButton.setColour (juce::TextButton::buttonColourId, kClay);
    bwfxButton.onClick = [this]
    {
        if (overlay == nullptr)
        {
            overlay = std::make_unique<BwfxPanel> (proc.bwfxRack(), proc.apvts);
            overlay->ground = dGround;
            overlay->onClose = [this] { if (overlay != nullptr) overlay->setVisible (false); };
            addAndMakeVisible (*overlay);
            overlay->setBounds (getLocalBounds());
        }
        else overlay->refreshFromRack();
        overlay->setVisible (true);
        overlay->toFront (true);
    };
    addAndMakeVisible (bwfxButton);

    /*  AUTO TRANSITION. It sits under POSITION because it drives POSITION,
        and while it is on the host parameter is ignored — so the slider is
        disabled and DISPLAY-DRIVEN from the processor's effective position,
        which keeps one owner for the value and still lets the master slider
        be the thing you watch. */
    autoHead.setText ("AUTO TRANSITION", juce::dontSendNotification);
    //  ash, not smoke: charred wood swallows smoke whole (the build-id lesson)
    autoHead.setColour (juce::Label::textColourId, kAsh.withAlpha (0.66f));
    autoHead.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    addAndMakeVisible (autoHead);

    autoReadout.setJustificationType (juce::Justification::centredRight);
    autoReadout.setColour (juce::Label::textColourId, kYellow.withAlpha (0.80f));
    autoReadout.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (autoReadout);

    autoOn.setButtonText ("AUTO");
    autoOn.setColour (juce::ToggleButton::tickColourId, kEmber);
    autoOn.setColour (juce::ToggleButton::textColourId, kAsh);
    autoOn.setToggleState (proc.autoCycle().on, juce::dontSendNotification);
    autoOn.onClick = [this]
    {
        proc.autoCycle().on = autoOn.getToggleState();
        //  handing the slider back: when AUTO lets go, the parameter is the
        //  owner again and the slider has to show IT, not the last auto value
        if (! autoOn.getToggleState())
            if (auto* p = proc.apvts.getRawParameterValue (rop_ids::position))
                position.setValue (p->load(), juce::dontSendNotification);
        syncAutoUi();
        repaint();
    };
    addAndMakeVisible (autoOn);

    autoBars.addItemList ({ "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" }, 1);
    {
        const int b = proc.autoCycle().bars;
        autoBars.setSelectedId (b <= 1 ? 1 : (b <= 2 ? 2 : (b <= 4 ? 3 : (b <= 8 ? 4 : 5))),
                                juce::dontSendNotification);
    }
    autoBars.setColour (juce::ComboBox::backgroundColourId, kClay);
    autoBars.setColour (juce::ComboBox::textColourId, kAsh);
    autoBars.onChange = [this]
    {
        static const int kBars[] = { 1, 2, 4, 8, 16 };
        const int idx = juce::jlimit (0, 4, autoBars.getSelectedId() - 1);
        proc.autoCycle().bars = kBars[idx];
        //  the window has to stay inside the cycle it lives in
        const float top = (float) proc.autoCycle().bars + 1.0f;
        autoStart.setRange (1.0, top, 0.25);
        autoEnd.setRange (1.0, top, 0.25);
        proc.autoCycle().start = juce::jlimit (1.0f, top, proc.autoCycle().start);
        proc.autoCycle().end   = juce::jlimit (1.0f, top, proc.autoCycle().end);
        autoStart.setValue (proc.autoCycle().start, juce::dontSendNotification);
        autoEnd.setValue (proc.autoCycle().end, juce::dontSendNotification);
        syncAutoUi();
    };
    addAndMakeVisible (autoBars);

    auto barSlider = [&] (juce::Slider& s, float v, bool isStart)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
        //  QUARTER BARS: the grid it snaps to, so "the 8th bar" is 8.0 to 9.0
        s.setRange (1.0, (double) proc.autoCycle().bars + 1.0, 0.25);
        s.setValue (v, juce::dontSendNotification);
        s.textFromValueFunction = [] (double x) { return "bar " + juce::String (x, 2); };
        s.updateText();
        s.setColour (juce::Slider::thumbColourId, kYellow);
        s.setColour (juce::Slider::trackColourId, kOchre.withAlpha (0.55f));
        s.setColour (juce::Slider::backgroundColourId, kClay);
        s.setColour (juce::Slider::textBoxTextColourId, kAsh);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.onValueChange = [this, &s, isStart]
        {
            (isStart ? proc.autoCycle().start : proc.autoCycle().end) = (float) s.getValue();
            syncAutoUi();
            repaint();
        };
        addAndMakeVisible (s);
    };
    barSlider (autoStart, proc.autoCycle().start, true);
    barSlider (autoEnd,   proc.autoCycle().end,   false);

    autoDir.addItemList ({ "0 to 100", "100 to 0" }, 1);
    autoDir.setSelectedId (proc.autoCycle().down ? 2 : 1, juce::dontSendNotification);
    autoDir.setColour (juce::ComboBox::backgroundColourId, kClay);
    autoDir.setColour (juce::ComboBox::textColourId, kAsh);
    autoDir.onChange = [this]
    {
        proc.autoCycle().down = (autoDir.getSelectedId() == 2);
        syncAutoUi();
    };
    addAndMakeVisible (autoDir);

    autoAfter.addItemList ({ "then RESET", "then HOLD" }, 1);
    autoAfter.setSelectedId (proc.autoCycle().hold ? 2 : 1, juce::dontSendNotification);
    autoAfter.setColour (juce::ComboBox::backgroundColourId, kClay);
    autoAfter.setColour (juce::ComboBox::textColourId, kAsh);
    autoAfter.setTooltip ("what the position does between the end of the window and "
                          "the end of the cycle: drop straight back, or stay at the top");
    autoAfter.onChange = [this]
    {
        proc.autoCycle().hold = (autoAfter.getSelectedId() == 2);
        syncAutoUi();
    };
    addAndMakeVisible (autoAfter);

    autoArrive.setButtonText ("ARRIVE");
    autoArrive.setColour (juce::ToggleButton::tickColourId, kEmber);
    autoArrive.setColour (juce::ToggleButton::textColourId, kAsh);
    autoArrive.setToggleState (proc.autoCycle().arrive, juce::dontSendNotification);
    autoArrive.setTooltip ("fire ARRIVAL when the sweep completes; off by default, "
                           "because an automatic drop is a bigger thing than an "
                           "automatic sweep");
    autoArrive.onClick = [this] { proc.autoCycle().arrive = autoArrive.getToggleState(); syncAutoUi(); };
    addAndMakeVisible (autoArrive);

    /*  THE MIX GATE. Peter: "most often i need the plugin to only activate
        the transition while position is being swept." Two switches, each
        with its own length, and a length of zero is the plain gate. Both
        default off, so nothing about an existing project changes. */
    gateHead.setText ("MIX GATE", juce::dontSendNotification);
    gateHead.setColour (juce::Label::textColourId, kAsh.withAlpha (0.66f));
    gateHead.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    addAndMakeVisible (gateHead);

    gateReadout.setJustificationType (juce::Justification::centredRight);
    gateReadout.setColour (juce::Label::textColourId, kYellow.withAlpha (0.80f));
    gateReadout.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (gateReadout);

    auto gateSwitch = [&] (juce::ToggleButton& b, const juce::String& text, bool state,
                           std::function<void (bool)> set)
    {
        b.setButtonText (text);
        b.setColour (juce::ToggleButton::tickColourId, kEmber);
        b.setColour (juce::ToggleButton::textColourId, kAsh);
        b.setToggleState (state, juce::dontSendNotification);
        b.onClick = [this, &b, set] { set (b.getToggleState()); syncGateUi(); repaint(); };
        addAndMakeVisible (b);
    };
    gateSwitch (fadeInOn,  "FADE IN",  proc.mixGate().fadeIn,
                [this] (bool v) { proc.mixGate().fadeIn = v; });
    gateSwitch (fadeOutOn, "FADE OUT", proc.mixGate().fadeOut,
                [this] (bool v) { proc.mixGate().fadeOut = v; });

    auto lenSlider = [&] (juce::Slider& s, float v, bool isIn)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 16);
        s.setRange (0.0, 50.0, 1.0);          // per cent of the whole travel
        s.setValue (v * 100.0, juce::dontSendNotification);
        //  zero is not "a very short fade", it is a different thing, and the
        //  readout has to say which one you have
        s.textFromValueFunction = [] (double x)
            { return x <= 0.5 ? juce::String ("instant") : (juce::String (juce::roundToInt (x)) + " %"); };
        s.updateText();
        s.setColour (juce::Slider::thumbColourId, kYellow);
        s.setColour (juce::Slider::trackColourId, kOchre.withAlpha (0.55f));
        s.setColour (juce::Slider::backgroundColourId, kClay);
        s.setColour (juce::Slider::textBoxTextColourId, kAsh);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.onValueChange = [this, &s, isIn]
        {
            (isIn ? proc.mixGate().inLen : proc.mixGate().outLen) = (float) (s.getValue() * 0.01);
            syncGateUi();
        };
        addAndMakeVisible (s);
    };
    lenSlider (fadeInLen,  proc.mixGate().inLen,  true);
    lenSlider (fadeOutLen, proc.mixGate().outLen, false);

    globals.push_back (makeKnob (rop_ids::mix,      "MIX"));
    globals.push_back (makeKnob (rop_ids::output,   "OUTPUT"));
    globals.push_back (makeKnob (rop_ids::spread,   "SPREAD"));
    globals.push_back (makeKnob (rop_ids::turn,     "TURN"));
    globals.push_back (makeKnob (rop_ids::monogate, "MONO GATE"));

    for (int i = 0; i < kSlots; ++i)
    {
        auto& L = lanes[i];

        /*  THE MENU IS TWO FLOORS. The eighteen this plugin owns are the
            list; the World rack is behind one door, because a flat
            twenty-eight is a scroll and it would also say that a borrowed
            colour and a transition gesture are the same kind of thing.

            The ids are unchanged — type + 2, EMPTY at 1 — so the submenu
            costs the state nothing: ComboBox::getItemForId searches the
            root menu recursively, and a sub-menu item selects, reads back
            and sets exactly like a top-level one. */
        auto* root = L.fx.getRootMenu();
        //  "EMPTY", not an em dash: juce::String(const char*) reads LATIN-1,
        //  so a UTF-8 dash typed here arrives as mojibake and did (the High
        //  Tide lesson)
        root->addItem (1, "EMPTY");
        for (int t = 0; t < numNativeEffects(); ++t)
            root->addItem (t + 2, effectDescriptor (t).name);

        juce::PopupMenu world;
        for (int t = numNativeEffects(); t < numEffects(); ++t)
            world.addItem (t + 2, effectDescriptor (t).name);
        if (numEffects() > numNativeEffects())
        {
            root->addSeparator();
            root->addSubMenu ("BROKILD WORLD FX", world);
        }

        L.fx.setSelectedId (proc.rack().slotEffect (i) + 2, juce::dontSendNotification);
        L.fx.setColour (juce::ComboBox::backgroundColourId, kClay);
        L.fx.setColour (juce::ComboBox::textColourId, kAsh);
        L.fx.onChange = [this, i]
        {
            if (building) return;
            proc.rack().setSlotEffect (i, lanes[i].fx.getSelectedId() - 2);
            selected = i;
            rebuildSlotEditor();
            resized();
        };
        addAndMakeVisible (L.fx);

        L.on.setColour (juce::ToggleButton::tickColourId, kEmber);
        L.on.setToggleState (proc.rack().state (i).on, juce::dontSendNotification);
        L.on.onClick = [this, i] { proc.rack().state (i).on = lanes[i].on.getToggleState(); };
        addAndMakeVisible (L.on);

        auto lane = [&] (juce::Slider& s, const juce::String& tip, double v)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setRange (0.0, 1.0, 0.001);
            s.setValue (v, juce::dontSendNotification);
            s.setColour (juce::Slider::thumbColourId, kYellow);
            s.setColour (juce::Slider::trackColourId, kOchre.withAlpha (0.5f));
            s.setColour (juce::Slider::backgroundColourId, kClay);
            //  these three are the controls Peter could not read; they now say
            //  what they are and what they are worth on hover as well
            s.setTooltip (tip);
            addAndMakeVisible (s);
        };
        lane (L.enter, "ENTER: where on POSITION this slot starts moving from A toward B", proc.rack().state (i).enter);
        lane (L.exitS, "EXIT: where it arrives at B", proc.rack().state (i).exit);
        lane (L.depth, "DEPTH: how far along A to B it actually gets by EXIT", proc.rack().state (i).depth);
        L.enter.onValueChange = [this, i] { proc.rack().state (i).enter = (float) lanes[i].enter.getValue(); };
        L.exitS.onValueChange = [this, i] { proc.rack().state (i).exit  = (float) lanes[i].exitS.getValue(); };
        L.depth.onValueChange = [this, i] { proc.rack().state (i).depth = (float) lanes[i].depth.getValue(); };

        auto box = [&] (juce::ComboBox& c, const juce::StringArray& items, int sel)
        {
            c.addItemList (items, 1);
            c.setSelectedId (sel + 1, juce::dontSendNotification);
            c.setColour (juce::ComboBox::backgroundColourId, kClay);
            c.setColour (juce::ComboBox::textColourId, kSmoke);
            addAndMakeVisible (c);
        };
        box (L.curve, { "LIN", "ACCEL", "DECEL", "S", "PEAK" }, proc.rack().state (i).curve);
        box (L.place, { "ST", "MID", "SIDE", "L", "R" }, (int) proc.rack().state (i).place);
        //  short enough to fit the cell: "BYPASS" truncated to "BYPA..."
        box (L.tail,  { "STOP", "SPILL", "CLEAR" }, (int) proc.rack().state (i).tail);
        L.curve.onChange = [this, i] { proc.rack().state (i).curve = lanes[i].curve.getSelectedId() - 1; };
        L.place.onChange = [this, i] { proc.rack().state (i).place = (Place) (lanes[i].place.getSelectedId() - 1); };
        L.tail.onChange  = [this, i] { proc.rack().state (i).tail  = (Tail)  (lanes[i].tail.getSelectedId() - 1); };
    }

    abHeading.setColour (juce::Label::textColourId, kOchre);
    abHeading.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (abHeading);

    abHint.setColour (juce::Label::textColourId, kSmoke);
    abHint.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (abHint);

    auto side = [&] (juce::Label& l, const juce::String& t, juce::Colour c)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredRight);
        l.setColour (juce::Label::textColourId, c);
        l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    side (abA, "A", kYellow);
    side (abB, "B", kEmber);

    /*  THE SIX QUICK PRESETS. Left click loads, right click stores over the
        slot — and the right click is why PresetButton exists, because a
        TextButton reports a click and not which button made it. They live
        on disk in the house patch folder, so the six are the user's own
        across every instance rather than six per plugin copy. */
    proc.ensureQuickPresets();
    presetHead.setText ("PRESETS   left load  -  right store", juce::dontSendNotification);
    presetHead.setColour (juce::Label::textColourId, kSmoke);
    presetHead.setFont (juce::FontOptions (9.5f));
    addAndMakeVisible (presetHead);

    for (int i = 0; i < RiteProcessor::kQuickPresets; ++i)
    {
        auto& b = presetBtn[i];
        b.setColour (juce::TextButton::textColourOffId, kAsh);
        b.setColour (juce::TextButton::buttonColourId, kClay);
        b.onClick = [this, i]
        {
            if (! proc.loadQuickPreset (i)) return;
            //  the rite moved underneath every lane control, so the panel has
            //  to be rebuilt from it rather than left showing the old one
            building = true;
            for (int s = 0; s < kSlots; ++s)
            {
                lanes[s].fx.setSelectedId (proc.rack().slotEffect (s) + 2, juce::dontSendNotification);
                lanes[s].on.setToggleState (proc.rack().state (s).on, juce::dontSendNotification);
                lanes[s].enter.setValue (proc.rack().state (s).enter, juce::dontSendNotification);
                lanes[s].exitS.setValue (proc.rack().state (s).exit, juce::dontSendNotification);
                lanes[s].depth.setValue (proc.rack().state (s).depth, juce::dontSendNotification);
                lanes[s].curve.setSelectedId (proc.rack().state (s).curve + 1, juce::dontSendNotification);
                lanes[s].place.setSelectedId ((int) proc.rack().state (s).place + 1, juce::dontSendNotification);
                lanes[s].tail.setSelectedId ((int) proc.rack().state (s).tail + 1, juce::dontSendNotification);
            }
            building = false;
            rebuildSlotEditor();
            markInertLanes();
            resized();
            repaint();
        };
        b.onRightClick = [this, i]
        {
            proc.storeQuickPreset (i, proc.quickPresetName (i) == "EMPTY"
                                        ? juce::String ("QUICK ") + juce::String (i + 1)
                                        : proc.quickPresetName (i));
            refreshPresetNames();
        };
        addAndMakeVisible (b);
    }
    refreshPresetNames();
    syncAutoUi();
    syncGateUi();

    rebuildSlotEditor();
    markInertLanes();
    setSize (1000, kHeadH + kGlobH + kColH + kSlots * (kLaneH + kLaneGap)
                   + 8 + 22 + 18 + 2 * kAbRow + 14);
    startTimerHz (24);
}

RiteEditor::~RiteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
//  an effect that cannot travel names itself in amber, on its own lane.
//  Called from every place the answer can change — NOT only from the timer,
//  because a snapshot never runs one and that is how the panel is reviewed.
/*  Everything the AUTO strip says about itself, in one place. The readout
    names the window in the same words a DAW would, so "the 8th bar" is
    something you can read off rather than work out. */
void RiteEditor::syncAutoUi()
{
    const auto& a = proc.autoCycle();
    for (juce::Component* c : { (juce::Component*) &autoBars,  (juce::Component*) &autoStart,
                                (juce::Component*) &autoEnd,   (juce::Component*) &autoDir,
                                (juce::Component*) &autoAfter, (juce::Component*) &autoArrive })
        c->setEnabled (a.on);

    //  AUTO owns POSITION while it is on, so the slider stops being an input
    //  and becomes a display. Saying so beats letting somebody drag a control
    //  that is being overwritten.
    position.setEnabled (! a.on);

    const float s = juce::jmin (a.start, a.end), e = juce::jmax (a.start, a.end);
    juce::String txt;
    if (! a.on) txt = "off - POSITION is yours to automate";
    else
    {
        txt = juce::String (a.down ? "100 to 0" : "0 to 100")
            + "  over bar " + juce::String (s, 2) + " to " + juce::String (e, 2)
            + " of " + juce::String (a.bars);
        txt += a.hold ? "  then HOLD" : "  then RESET";
        if (a.arrive) txt += "  and ARRIVE";
        const float b = proc.autoBarNow();
        txt += (b < 0.0f) ? "   -   waiting for the transport"
                          : ("   -   at bar " + juce::String (b, 2));
    }
    autoReadout.setText (txt, juce::dontSendNotification);
}

//  what the gate is doing, in the words somebody would use to ask for it
void RiteEditor::syncGateUi()
{
    const auto& g = proc.mixGate();
    fadeInLen.setEnabled (g.fadeIn);
    fadeOutLen.setEnabled (g.fadeOut);

    juce::String t;
    if (! g.fadeIn && ! g.fadeOut) t = "off - the effects are always on, MIX is yours";
    else
    {
        if (g.fadeIn)
            t = (g.inLen <= 0.0f) ? juce::String ("silent at 0 %")
                : ("fades in over the first " + juce::String (juce::roundToInt (g.inLen * 100.0f)) + " %");
        if (g.fadeOut)
        {
            if (t.isNotEmpty()) t += "   -   ";
            t += (g.outLen <= 0.0f) ? juce::String ("silent at 100 %")
                : ("fades out over the last " + juce::String (juce::roundToInt (g.outLen * 100.0f)) + " %");
        }
    }
    gateReadout.setText (t, juce::dontSendNotification);
}

void RiteEditor::refreshPresetNames()
{
    for (int i = 0; i < RiteProcessor::kQuickPresets; ++i)
        presetBtn[i].setButtonText (proc.quickPresetName (i));
}

void RiteEditor::markInertLanes()
{
    for (int i = 0; i < kSlots; ++i)
        lanes[i].fx.setColour (juce::ComboBox::textColourId,
                               (proc.rack().slotEffect (i) >= 0 && ! slotTravels (i)) ? kYellow : kAsh);
}

bool RiteEditor::slotTravels (int slot) const
{
    const int type = proc.rack().slotEffect (slot);
    if (type < 0) return false;
    const auto& d = effectDescriptor (type);
    const auto& s = proc.rack().state (slot);
    for (int p = 0; p < d.numParams; ++p)
        if (std::abs (s.A[p] - s.B[p]) > 1.0e-6f) return true;
    return false;
}

// ---------------------------------------------------------------------------
// The A and B of the selected slot, generated from its descriptor.
void RiteEditor::rebuildSlotEditor()
{
    ab.clear();
    const int type = proc.rack().slotEffect (selected);
    const juce::String slotName = "SLOT " + juce::String (selected + 1);

    if (type < 0)
    {
        abHeading.setText (slotName + " - EMPTY", juce::dontSendNotification);
        abHint.setText ("Choose an effect for this lane. An empty rite passes the audio "
                        "through untouched, which is deliberate, not a fault.",
                        juce::dontSendNotification);
        abHint.setColour (juce::Label::textColourId, kSmoke);
        resized();
        return;
    }

    markInertLanes();
    const auto& d = effectDescriptor (type);
    abHeading.setText (slotName + " - " + d.name + "   " + d.sub, juce::dontSendNotification);

    for (int p = 0; p < d.numParams; ++p)
    {
        auto k = std::make_unique<ABKnob>();
        const auto& pd = d.params[p];
        auto setup = [&] (juce::Slider& s, bool isB)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
            s.setRange (pd.lo, pd.hi, pd.step > 0 ? pd.step : 0.0);
            s.setValue (isB ? proc.rack().state (selected).B[p]
                            : proc.rack().state (selected).A[p], juce::dontSendNotification);
            s.setColour (juce::Slider::rotarySliderFillColourId, isB ? kEmber : kYellow);
            s.setColour (juce::Slider::textBoxTextColourId, kAsh);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            //  a choice reads as its NAME and a frequency as a frequency
            s.textFromValueFunction = [&pd] (double v) { return paramText (pd, v); };
            s.updateText();
            s.onValueChange = [this, p, isB, &s]
            {
                auto& st = proc.rack().state (selected);
                (isB ? st.B[p] : st.A[p]) = (float) s.getValue();
                //  whether the slot can travel may have just changed
                repaint();
                timerCallback();
            };
            addAndMakeVisible (s);
        };
        setup (k->a, false);
        setup (k->b, true);
        k->name.setText (pd.name, juce::dontSendNotification);
        k->name.setJustificationType (juce::Justification::centred);
        k->name.setColour (juce::Label::textColourId, kSmoke);
        k->name.setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (k->name);
        ab.push_back (std::move (k));
    }
    resized();
}

// ---------------------------------------------------------------------------
juce::Rectangle<int> RiteEditor::laneBounds (int slot) const
{
    return { kInset, kHeadH + kGlobH + kColH + slot * (kLaneH + kLaneGap),
             getWidth() - 2 * kInset, kLaneH };
}

RiteEditor::LaneCells RiteEditor::cellsFor (juce::Rectangle<int> lane)
{
    LaneCells c;
    auto r = lane.reduced (8, 5);
    c.on     = r.removeFromLeft (22);
    r.removeFromLeft (4);
    c.socket = r.removeFromLeft (r.getHeight() + 4);
    r.removeFromLeft (4);
    c.fx     = r.removeFromLeft (108);
    r.removeFromLeft (10);
    c.curve  = r.removeFromRight (74);
    r.removeFromRight (4);
    c.tail   = r.removeFromRight (70);
    r.removeFromRight (4);
    c.place  = r.removeFromRight (58);
    r.removeFromRight (10);
    const int w = r.getWidth() / 3;
    c.enter  = r.removeFromLeft (w);
    c.exitS  = r.removeFromLeft (w);
    c.depth  = r;
    return c;
}

void RiteEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kSlots; ++i)
        if (laneBounds (i).contains (e.getPosition()))
        {
            if (selected != i) { selected = i; rebuildSlotEditor(); repaint(); }
            return;
        }
}

// ---------------------------------------------------------------------------
void RiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    tile (g, dGround, getLocalBounds(), 0.55f);
    //  whoever is driving it: the parameter, or AUTO
    const float t = proc.effectivePosition();

    // ---- the threshold: two uprights and a lintel -------------------------
    auto head = getLocalBounds().removeFromTop (kHeadH);
    if (dClay.isValid()) tile (g, dClay, head.withTrimmedTop (30), 0.45f);
    else { g.setColour (kClay); g.fillRect (head.withTrimmedTop (30)); }

    //  the posts run the full height of the panel: it IS a doorway
    auto drawPost = [&] (int x, bool mirrored)
    {
        auto box = juce::Rectangle<float> ((float) x, 30.0f, (float) kPostW,
                                           (float) getHeight() - 44.0f);
        if (dPost.isValid())
        {
            //  the post decal is a single charred stake; repeat it down the
            //  jamb rather than stretching one into a smear
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (box.toNearestInt());
            if (mirrored)
                g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f)
                                  .translated (2.0f * box.getCentreX(), 0.0f));
            const int pw = (int) box.getWidth();
            const int ph = juce::jmax (8, pw * dPost.getHeight() / dPost.getWidth());
            for (int y = (int) box.getY(); y < (int) box.getBottom(); y += ph)
                g.drawImage (dPost, (int) box.getX(), y, pw, ph,
                             0, 0, dPost.getWidth(), dPost.getHeight());
        }
        else
        {
            g.setColour (kOchre.withAlpha (0.55f));
            g.fillRect (box);
        }
    };
    drawPost (kMargin, false);
    drawPost (getWidth() - kMargin - kPostW, true);

    //  the lintel across the top
    auto lint = juce::Rectangle<float> ((float) kMargin, 22.0f,
                                        (float) (getWidth() - 2 * kMargin), 30.0f);
    if (dLintel.isValid())
    {
        g.drawImage (dLintel, lint.getX(), lint.getY(), lint.getWidth(), lint.getHeight(),
                     0, 0, dLintel.getWidth(), dLintel.getHeight());
    }
    else
    {
        g.setColour (kOchre.withAlpha (0.55f));
        g.fillRect (lint.withHeight (8.0f));
    }

    // ---- the column headings ----------------------------------------------
    {
        auto hdr = juce::Rectangle<int> (kInset, kHeadH + kGlobH, getWidth() - 2 * kInset, kColH);
        const auto c = cellsFor (hdr.withHeight (kLaneH).withY (hdr.getY() - 5));
        g.setColour (kSmoke.withAlpha (0.85f));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        auto lab = [&] (juce::Rectangle<int> box, const char* s)
        { g.drawText (s, box.withY (hdr.getY()).withHeight (kColH), juce::Justification::centred, false); };
        lab (c.fx,    "EFFECT");
        lab (c.enter, "ENTER");
        lab (c.exitS, "EXIT");
        lab (c.depth, "DEPTH");
        lab (c.place, "PLACE");
        lab (c.tail,  "ON ARRIVAL");
        lab (c.curve, "CURVE");
    }

    // ---- the lanes, which take on heat as the marker passes ---------------
    for (int i = 0; i < kSlots; ++i)
    {
        auto r = laneBounds (i);
        const auto& s = proc.rack().state (i);
        const int type = proc.rack().slotEffect (i);
        const bool live = type >= 0 && s.on;
        const bool travels = slotTravels (i);

        g.setColour (i == selected ? kClay.brighter (0.16f) : kClay);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        if (dClay.isValid())
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (r.toFloat(), 4.0f);
            g.reduceClipRegion (clip);
            tile (g, dClay, r, i == selected ? 0.55f : 0.38f);
        }
        if (i == selected)
        {
            g.setColour (kOchre.withAlpha (0.7f));
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.2f);
        }

        //  the lane's span on the score, and how far the heat has reached
        const auto cells = cellsFor (r);
        const int bx = cells.enter.getX(), bw = cells.depth.getRight() - bx;
        const int x0 = bx + (int) (s.enter * bw);
        const int x1 = bx + (int) (s.exit  * bw);
        g.setColour ((live && travels ? kOchre : kSmoke).withAlpha (0.30f));
        g.fillRect (x0, r.getY() + 4, juce::jmax (2, x1 - x0), r.getHeight() - 8);

        if (live && travels && t > s.enter)
        {
            const int xt = bx + (int) (juce::jmin (t, s.exit) * bw);
            g.setColour (kEmber.withAlpha (0.55f));
            g.fillRect (x0, r.getY() + 4, juce::jmax (1, xt - x0), r.getHeight() - 8);
        }

        /*  THE THING THAT WAS MISSING. A slot holding an effect whose A and B
            are identical cannot move, so ENTER, EXIT and DEPTH do nothing and
            the lane looked broken. The lane is 44 px tall and every pixel of
            it is under a slider, so it says so WITHOUT WORDS: the span it
            would have travelled is hatched out, the way a drawing marks a
            dimension that is not yet fixed. The sentence lives once, under
            the A/B editor, where there is room to be a sentence. */
        if (type >= 0 && ! travels)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (juce::Rectangle<int> (x0, r.getY() + 4,
                                                      juce::jmax (2, x1 - x0), r.getHeight() - 8));
            g.setColour (kYellow.withAlpha (0.20f));
            for (int d = -r.getHeight(); d < x1 - x0 + r.getHeight(); d += 7)
                g.drawLine ((float) (x0 + d), (float) r.getBottom(),
                            (float) (x0 + d + r.getHeight()), (float) r.getY(), 1.4f);
        }

        //  the socket, and the slot's mark struck into it
        if (dSocket.isValid()) fitImage (g, dSocket, cells.socket.toFloat(), live ? 1.0f : 0.72f);
        else { g.setColour (kGround.withAlpha (0.8f)); g.fillRoundedRectangle (cells.socket.toFloat().reduced (2.0f), 3.0f); }

        const int cell = type >= 0 ? glyphForEffect (effectDescriptor (type).id) : 11;
        const juce::Image& gl = (cell >= 0 && cell < 12) ? dGlyph[cell] : juce::Image();
        auto inner = cells.socket.toFloat().reduced (cells.socket.getWidth() * 0.26f);
        if (gl.isValid())
        {
            //  ash white, warming to ember as its own lane catches
            const float heat = (live && travels && t > s.enter)
                             ? juce::jlimit (0.0f, 1.0f, (t - s.enter) / juce::jmax (0.01f, s.exit - s.enter))
                             : 0.0f;
            juce::DrawableImage di;
            di.setImage (gl);
            di.setOverlayColour (kAsh.interpolatedWith (kEmber, heat));
            di.drawWithin (g, inner, juce::RectanglePlacement::centred, live ? 1.0f : 0.55f);
        }
        else if (type >= 0)
        {
            //  no decal: the name, in ash — or in the BWFX teal when the
            //  lane holds a World module, which is the one place on this
            //  panel that says where a slot's effect came from
            const bool world = isWrappedBwfx (effectDescriptor (type).id);
            g.setColour ((world ? kTeal : kAsh).withAlpha (live ? 0.7f : 0.4f));
            g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
            g.drawText (juce::String (effectDescriptor (type).name).substring (0, 3),
                        cells.socket, juce::Justification::centred, false);
        }
    }

    // ---- the marker's position across everything --------------------------
    const int mx = kInset + (int) (t * (getWidth() - 2 * kInset));
    g.setColour (kAsh.withAlpha (0.8f));
    g.fillRect (mx - 1, kHeadH + kGlobH + kColH - 6, 2, kSlots * (kLaneH + kLaneGap) + 8);

    //  the ash that has settled on the whole thing
    tile (g, dAshWear, getLocalBounds(), 0.18f);

    //  the mark struck at the moment of arrival
    if (flash > 0.005f && dArrival.isValid())
    {
        const float d = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.75f;
        fitImage (g, dArrival,
                  juce::Rectangle<float> (0, 0, d, d).withCentre (getLocalBounds().getCentre().toFloat()),
                  flash * 0.55f);
    }
}

void RiteEditor::resized()
{
    if (overlay != nullptr) overlay->setBounds (getLocalBounds());

    auto head = getLocalBounds().removeFromTop (kHeadH).reduced (kInset, 0);
    title.setBounds (head.getX(), 54, 260, 26);
    build.setBounds (head.getX() + 2, 78, 260, 14);
    readout.setBounds (head.getRight() - 320, 60, 320, 20);
    position.setBounds (head.getX(), 90, head.getWidth() - 230, 30);
    arrival.setBounds (head.getRight() - 110, 90, 110, 30);
    bwfxButton.setBounds (head.getRight() - 110 - 86, 90, 78, 30);

    //  the AUTO strip: one label line, one control line
    {
        auto lab = juce::Rectangle<int> (head.getX(), 126, head.getWidth(), 13);
        autoHead.setBounds (lab.removeFromLeft (150));
        autoReadout.setBounds (lab);

        auto rowA = juce::Rectangle<int> (head.getX(), 140, head.getWidth(), 28);
        autoOn.setBounds (rowA.removeFromLeft (66));
        rowA.removeFromLeft (4);
        autoBars.setBounds (rowA.removeFromLeft (84).reduced (0, 2));
        rowA.removeFromLeft (10);
        autoArrive.setBounds (rowA.removeFromRight (86));
        rowA.removeFromRight (6);
        autoAfter.setBounds (rowA.removeFromRight (100).reduced (0, 2));
        rowA.removeFromRight (6);
        autoDir.setBounds (rowA.removeFromRight (92).reduced (0, 2));
        rowA.removeFromRight (10);
        const int half = rowA.getWidth() / 2;
        autoStart.setBounds (rowA.removeFromLeft (half).reduced (2, 2));
        autoEnd.setBounds (rowA.reduced (2, 2));

        //  the MIX GATE, one row under it
        auto rowB = juce::Rectangle<int> (head.getX(), 172, head.getWidth(), 28);
        gateHead.setBounds (rowB.removeFromLeft (76));
        fadeInOn.setBounds (rowB.removeFromLeft (86));
        fadeInLen.setBounds (rowB.removeFromLeft (180).reduced (2, 2));
        rowB.removeFromLeft (14);
        fadeOutOn.setBounds (rowB.removeFromLeft (96));
        fadeOutLen.setBounds (rowB.removeFromLeft (180).reduced (2, 2));
        rowB.removeFromLeft (10);
        gateReadout.setBounds (rowB);
    }

    auto row = juce::Rectangle<int> (kInset, kHeadH, getWidth() - 2 * kInset, kGlobH).reduced (2, 4);
    for (auto& k : globals)
    {
        auto cell = row.removeFromLeft (96);
        k->label.setBounds (cell.removeFromTop (12));
        k->slider.setBounds (cell);
        row.removeFromLeft (6);
    }

    //  the six presets take the room left over beside the globals
    row.removeFromLeft (10);
    presetHead.setBounds (row.removeFromTop (12));
    {
        const int bw = row.getWidth() / 3;
        const int bh = (row.getHeight() - 4) / 2;
        for (int i = 0; i < RiteProcessor::kQuickPresets; ++i)
        {
            const int cx = row.getX() + (i % 3) * bw;
            const int cy = row.getY() + (i / 3) * (bh + 4);
            presetBtn[i].setBounds (juce::Rectangle<int> (cx, cy, bw, bh).reduced (2, 0));
        }
    }

    for (int i = 0; i < kSlots; ++i)
    {
        const auto c = cellsFor (laneBounds (i));
        lanes[i].on.setBounds (c.on);
        lanes[i].fx.setBounds (c.fx.reduced (1, 2));
        lanes[i].enter.setBounds (c.enter.reduced (3, 6));
        lanes[i].exitS.setBounds (c.exitS.reduced (3, 6));
        lanes[i].depth.setBounds (c.depth.reduced (3, 6));
        lanes[i].place.setBounds (c.place.reduced (1, 2));
        lanes[i].tail.setBounds  (c.tail.reduced (1, 2));
        lanes[i].curve.setBounds (c.curve.reduced (1, 2));
    }

    auto ed = getLocalBounds();
    ed.removeFromTop (kHeadH + kGlobH + kColH + kSlots * (kLaneH + kLaneGap) + 8);
    ed = ed.reduced (kInset, 0);
    abHeading.setBounds (ed.removeFromTop (22));
    abHint.setBounds (ed.removeFromTop (18));
    if (! ab.empty())
    {
        auto gutter = ed.removeFromLeft (20);
        abA.setBounds (gutter.getX(), ed.getY() + 34, 16, 16);
        abB.setBounds (gutter.getX(), ed.getY() + kAbRow + 34, 16, 16);
        const int cw = juce::jmin (96, ed.getWidth() / (int) ab.size());
        auto aRow = ed.removeFromTop (kAbRow);
        auto bRow = ed.removeFromTop (kAbRow);
        for (size_t i = 0; i < ab.size(); ++i)
        {
            auto ca = aRow.removeFromLeft (cw);
            ab[i]->name.setBounds (ca.removeFromTop (12));
            ab[i]->a.setBounds (ca.reduced (3, 0));
            ab[i]->b.setBounds (bRow.removeFromLeft (cw).reduced (3, 0).withTrimmedTop (12));
        }
    }
    else
    {
        abA.setBounds (0, 0, 0, 0);
        abB.setBounds (0, 0, 0, 0);
    }
}

void RiteEditor::timerCallback()
{
    const double l = proc.loudness();
    readout.setText ((l > -100.0 ? juce::String (l, 1) + " LUFS" : juce::String ("-inf"))
                     + (proc.rack().arrivalPending() ? "   ARMED"
                        : (proc.rack().arrived() ? "   ARRIVED" : juce::String())),
                     juce::dontSendNotification);

    //  the arrival mark is struck once and fades; it is an event, not a state
    const bool nowArrived = proc.rack().arrived();
    if (nowArrived && ! wasArrived) flash = 1.0f;
    if (proc.autoCycle().on)
    {
        syncAutoUi();
        //  display only: the parameter is not moving, so nothing fights this
        position.setValue (proc.effectivePosition() * 100.0, juce::dontSendNotification);
    }

    wasArrived = nowArrived;
    flash *= 0.86f;

    markInertLanes();


    //  say, in the editor heading, whether this slot can travel at all
    const int type = proc.rack().slotEffect (selected);
    if (type >= 0)
    {
        if (slotTravels (selected))
        {
            abHint.setText ("Top row is A, where the slot sits before ENTER. Bottom row is B, "
                            "where it arrives at EXIT. Only the knobs whose A and B differ move.",
                            juce::dontSendNotification);
            abHint.setColour (juce::Label::textColourId, kSmoke);
        }
        else
        {
            abHint.setText ("A and B are identical on every knob, so this slot cannot move and "
                            "ENTER, EXIT, DEPTH and CURVE do nothing. Change a knob in the "
                            "bottom (B) row to give it somewhere to travel to.",
                            juce::dontSendNotification);
            abHint.setColour (juce::Label::textColourId, kYellow);
        }
    }
    repaint();
}
