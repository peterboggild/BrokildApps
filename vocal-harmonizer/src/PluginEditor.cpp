#include "PluginEditor.h"

namespace
{
    const juce::Colour kBack   { 0xff14171c };
    const juce::Colour kPanel  { 0xff1c2128 };
    const juce::Colour kInk    { 0xffd8dee6 };
    const juce::Colour kDim    { 0xff7c8794 };
    const juce::Colour kAmber  { 0xffe0a33c };
    const juce::Colour kTeal   { 0xff35c9c0 };   // the BWFX accent, the same
                                                 // colour in every Brokild
                                                 // plugin (BWFX-DESIGN.md §5)

    //  The rack's own display rules, kept identical to fmt() in the shared
    //  ui/bwfx-rack.js so a module reads the same here as it does in every
    //  other Brokild plugin. Without it JUCE's default wins and a rate shows
    //  as "400.0000..." instead of "4.00 Hz".
    juce::String bwfxText (const bwfx::ParamDesc& pd, double v)
    {
        const juce::String u (pd.unit != nullptr ? pd.unit : "");

        if (u == "%")   return juce::String (juce::roundToInt (v)) + " %";
        if (u == "dB")  return juce::String (v, 1) + " dB";
        if (u == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
        if (u == "cHz") return juce::String (v / 100.0, 2) + " Hz";
        if (u == "dHz") return juce::String (v / 10.0,  1) + " Hz";
        if (u == "cs")  return juce::String (v / 100.0, 2) + " s";

        if (pd.choices != nullptr && *pd.choices != 0)
        {
            auto c = juce::StringArray::fromTokens (juce::String (pd.choices), "|", "");
            const int i = juce::jlimit (0, c.size() - 1, juce::roundToInt (v));
            return c[i];
        }
        return juce::String (juce::roundToInt (v * 100.0) / 100.0, 2);
    }
}

// ---------------------------------------------------------------------------
// The BWFX overlay. Opaque and full-bleed on purpose: it is the one thing
// standing between the rack and the panel underneath it.
LegionEditor::RackOverlay::RackOverlay()
{
    setOpaque (true);                         //  every pixel is ours
    setInterceptsMouseClicks (true, true);    //  and no click reaches the panel
}

void LegionEditor::RackOverlay::paint (juce::Graphics& g)
{
    //  the panel, darkened — it stays faintly readable so you can see WHAT the
    //  rack is sitting on, without competing with it
    g.fillAll (kBack.withAlpha (1.0f));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (getLocalBounds());

    //  the rack card
    g.setColour (kPanel);
    g.fillRoundedRectangle (card.toFloat(), 8.0f);
    g.setColour (kTeal.withAlpha (0.55f));
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.0f);

    //  its header rule
    g.setColour (kTeal.withAlpha (0.25f));
    g.fillRect (card.getX() + 12, card.getY() + 38, card.getWidth() - 24, 1);
}

void LegionEditor::RackOverlay::mouseDown (const juce::MouseEvent& e)
{
    //  clicking the darkened surround closes it, the usual modal idiom
    if (! card.contains (e.getPosition()) && onDismiss)
        onDismiss();
}

// ---------------------------------------------------------------------------
void LegionEditor::addKnob (const juce::String& paramId, const juce::String& text,
                            juce::Component& parent, std::vector<std::unique_ptr<Knob>>& into)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
    k->slider.setColour (juce::Slider::thumbColourId, kAmber);
    k->slider.setColour (juce::Slider::textBoxTextColourId, kInk);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, kDim);
    k->label.setFont (juce::FontOptions (11.0f));

    parent.addAndMakeVisible (k->slider);
    parent.addAndMakeVisible (k->label);
    k->attach = std::make_unique<SliderAttach> (proc.apvts, paramId, k->slider);

    into.push_back (std::move (k));
}

// ---------------------------------------------------------------------------
LegionEditor::LegionEditor (LegionProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    title.setText ("LEGION", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kInk);
    addAndMakeVisible (title);

    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setColour (juce::Label::textColourId, kDim);
    latencyLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (latencyLabel);

    addKnob (legion_ids::mix,      "MIX",      *this, globalKnobs);
    addKnob (legion_ids::humanize, "HUMANISE", *this, globalKnobs);
    addKnob (legion_ids::output,   "OUTPUT",   *this, globalKnobs);

    for (auto* box : { &detailBox, &rackPosBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, kPanel);
        box->setColour (juce::ComboBox::textColourId, kInk);
        addAndMakeVisible (*box);
    }
    detailBox.addItemList ({ "TIGHT", "NATURAL", "SMOOTH" }, 1);
    rackPosBox.addItemList ({ "HARMONY", "MASTER" }, 1);
    detailAttach  = std::make_unique<BoxAttach> (proc.apvts, legion_ids::detail,  detailBox);
    rackPosAttach = std::make_unique<BoxAttach> (proc.apvts, legion_ids::rackPos, rackPosBox);

    for (auto* l : { &detailLabel, &rackPosLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, kDim);
        l->setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (*l);
    }
    detailLabel .setText ("DETAIL",  juce::dontSendNotification);
    rackPosLabel.setText ("BWFX ON", juce::dontSendNotification);

    // ---- the four voices --------------------------------------------------
    for (int v = 0; v < legion::kVoices; ++v)
    {
        auto& st = strips[v];
        st.heading.setText ("VOICE " + juce::String (v + 1), juce::dontSendNotification);
        st.heading.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        st.heading.setColour (juce::Label::textColourId, kAmber);
        st.heading.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (st.heading);

        st.on.setColour (juce::ToggleButton::tickColourId, kAmber);
        addAndMakeVisible (st.on);
        st.onAttach = std::make_unique<ButtonAttach> (proc.apvts, legion_ids::voice (v, "on"), st.on);

        for (int s = 0; s < kNumVoiceSpecs; ++s)
        {
            auto k = std::make_unique<Knob>();
            k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 15);
            k->slider.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
            k->slider.setColour (juce::Slider::textBoxTextColourId, kInk);
            k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            k->label.setText (kVoiceSpecs[s].name, juce::dontSendNotification);
            k->label.setJustificationType (juce::Justification::centred);
            k->label.setColour (juce::Label::textColourId, kDim);
            k->label.setFont (juce::FontOptions (10.0f));
            addAndMakeVisible (k->slider);
            addAndMakeVisible (k->label);
            k->attach = std::make_unique<SliderAttach> (
                proc.apvts, legion_ids::voice (v, kVoiceSpecs[s].id), k->slider);
            st.knobs.push_back (std::move (k));
        }
    }

    // ---- BWFX -------------------------------------------------------------
    rackButton.setClickingTogglesState (true);
    rackButton.setColour (juce::TextButton::textColourOffId, kTeal);
    rackButton.setColour (juce::TextButton::textColourOnId, kBack);
    rackButton.setColour (juce::TextButton::buttonOnColourId, kTeal);
    rackButton.onClick = [this]
    {
        rackOpen = rackButton.getToggleState();
        rackOverlay.setVisible (rackOpen);
        if (rackOpen)
            rackOverlay.toFront (false);      //  always the last thing painted
        resized();
    };
    addAndMakeVisible (rackButton);

    buildRackPanel();
    rackView.setViewedComponent (&rackPanel, false);
    rackView.setScrollBarsShown (true, false);

    rackTitle.setText ("BWFX", juce::dontSendNotification);
    rackTitle.setColour (juce::Label::textColourId, kTeal);
    rackTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));

    rackClose.setColour (juce::TextButton::textColourOffId, kDim);
    rackClose.onClick = [this] { rackButton.setToggleState (false, juce::sendNotificationSync); };

    rackOverlay.onDismiss = [this] { rackButton.setToggleState (false, juce::sendNotificationSync); };
    rackOverlay.addAndMakeVisible (rackView);
    rackOverlay.addAndMakeVisible (rackTitle);
    rackOverlay.addAndMakeVisible (rackClose);
    addChildComponent (rackOverlay);          //  added last: it paints on top

    setSize (940, 620);
    startTimerHz (8);
}

LegionEditor::~LegionEditor() { stopTimer(); }

// ---------------------------------------------------------------------------
// The rack UI is GENERATED from the BWFX descriptors, never typed out: that
// is the whole point of self-describing modules. A module added to BWFX
// appears here on the next rebuild, with its own name, ranges and units,
// and no line of this file changes.
void LegionEditor::buildRackPanel()
{
    rackPanel.setName ("bwfx");

    rackMix.setSliderStyle (juce::Slider::LinearHorizontal);
    rackMix.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
    rackMix.setRange (0.0, 1.0, 0.001);
    rackMix.textFromValueFunction = [] (double v)
        { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    rackMix.setValue (proc.rack().getMix(), juce::dontSendNotification);
    rackMix.updateText();
    rackMix.setColour (juce::Slider::thumbColourId, kTeal);
    rackMix.setColour (juce::Slider::trackColourId, kTeal.withAlpha (0.5f));
    rackMix.onValueChange = [this] { proc.rack().setMix ((float) rackMix.getValue()); };
    rackMixLabel.setText ("RACK DRY / WET", juce::dontSendNotification);
    rackMixLabel.setColour (juce::Label::textColourId, kTeal);
    rackMixLabel.setFont (juce::FontOptions (11.0f));
    rackPanel.addAndMakeVisible (rackMix);
    rackPanel.addAndMakeVisible (rackMixLabel);

    for (int t = 0; t < bwfx::numModuleTypes(); ++t)
    {
        const auto& d = bwfx::moduleDescriptor (t);
        auto rm = std::make_unique<RackModule>();
        rm->type = t;

        rm->name.setText (juce::String (d.name) + "   " + juce::String (d.sub),
                          juce::dontSendNotification);
        rm->name.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        rm->name.setColour (juce::Label::textColourId, kTeal);
        rackPanel.addAndMakeVisible (rm->name);

        rm->power.setColour (juce::ToggleButton::tickColourId, kTeal);
        rm->power.setToggleState (proc.rack().getEnabled (t), juce::dontSendNotification);
        const int type = t;
        rm->power.onClick = [this, type, pw = &rm->power]
        {
            proc.rack().setEnabled (type, pw->getToggleState());
        };
        rackPanel.addAndMakeVisible (rm->power);

        for (int pi = 0; pi < d.numParams; ++pi)
        {
            const auto& ps = d.params[pi];
            auto sl = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                      juce::Slider::TextBoxBelow);
            sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
            sl->setRange (ps.lo, ps.hi, ps.step);
            //  by value: a ParamDesc is a POD of literals, so the lambda does
            //  not depend on the descriptor table outliving the editor
            sl->textFromValueFunction = [ps] (double v) { return bwfxText (ps, v); };
            sl->updateText();
            sl->setValue (proc.rack().getParam (t, pi), juce::dontSendNotification);
            sl->setColour (juce::Slider::rotarySliderFillColourId, kTeal);
            sl->setColour (juce::Slider::textBoxTextColourId, kInk);
            sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            auto* raw = sl.get();
            sl->onValueChange = [this, type, pi, raw]
            {
                proc.rack().setParam (type, pi, (float) raw->getValue());
            };

            auto lb = std::make_unique<juce::Label>();
            lb->setText (ps.name, juce::dontSendNotification);
            lb->setJustificationType (juce::Justification::centred);
            lb->setColour (juce::Label::textColourId, kDim);
            lb->setFont (juce::FontOptions (10.0f));

            rackPanel.addAndMakeVisible (*sl);
            rackPanel.addAndMakeVisible (*lb);
            rm->knobs.push_back (std::move (sl));
            rm->knobLabels.push_back (std::move (lb));
        }

        rackModules.push_back (std::move (rm));
    }

    //  the five macros are HOST parameters, so they attach the normal way —
    //  the rack never owns their values (bwfx_juce, "one owner per value")
    for (int i = 0; i < bwfx::kMacros; ++i)
        addKnob (bwfx_juce::macroParamId (i), "MACRO " + juce::String (i + 1), rackPanel, macroKnobs);
}

// ---------------------------------------------------------------------------
void LegionEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBack);

    auto r = getLocalBounds();
    g.setColour (kPanel);
    g.fillRect (r.removeFromTop (58));

    //  the voice strips
    const int stripTop = 118;
    const int stripH   = 148;
    for (int v = 0; v < legion::kVoices; ++v)
    {
        const bool on = proc.apvts.getRawParameterValue (legion_ids::voice (v, "on"))->load() > 0.5f;
        g.setColour (on ? kPanel : kPanel.withAlpha (0.45f));
        g.fillRoundedRectangle ((float) (14 + 0), (float) (stripTop + v * (stripH + 8)),
                                (float) (getWidth() - 28), (float) stripH, 6.0f);
    }
}

void LegionEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (58).reduced (14, 8);

    title.setBounds (head.removeFromLeft (110));
    latencyLabel.setBounds (head.removeFromRight (230));
    rackButton.setBounds (head.removeFromRight (90).reduced (2, 6));

    //  globals
    auto row = r.removeFromTop (60).reduced (14, 2);
    const int kw = 86;
    for (auto& k : globalKnobs)
    {
        auto cell = row.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (13));
        k->slider.setBounds (cell);
        row.removeFromLeft (6);
    }
    auto box1 = row.removeFromLeft (110);
    detailLabel.setBounds (box1.removeFromTop (13));
    detailBox.setBounds (box1.reduced (2, 8));
    row.removeFromLeft (8);
    auto box2 = row.removeFromLeft (110);
    rackPosLabel.setBounds (box2.removeFromTop (13));
    rackPosBox.setBounds (box2.reduced (2, 8));

    //  the voice strips
    const int stripH = 148;
    for (int v = 0; v < legion::kVoices; ++v)
    {
        auto strip = juce::Rectangle<int> (14, 118 + v * (stripH + 8), getWidth() - 28, stripH)
                        .reduced (10, 6);
        auto top = strip.removeFromTop (18);
        strips[v].on.setBounds (top.removeFromLeft (26));
        strips[v].heading.setBounds (top.removeFromLeft (90));

        const int cw = strip.getWidth() / kNumVoiceSpecs;
        for (int s = 0; s < kNumVoiceSpecs; ++s)
        {
            auto cell = strip.removeFromLeft (cw).reduced (4, 2);
            strips[v].knobs[(size_t) s]->label.setBounds (cell.removeFromTop (12));
            strips[v].knobs[(size_t) s]->slider.setBounds (cell);
        }
    }

    //  the overlay always spans the whole editor, so nothing can show past it
    rackOverlay.setBounds (getLocalBounds());

    if (rackOpen)
    {
        auto card = getLocalBounds().reduced (24);
        rackOverlay.card = card;

        auto cardHead = card.withHeight (38).reduced (12, 6);
        rackTitle.setBounds (cardHead.removeFromLeft (120));
        rackClose.setBounds (cardHead.removeFromRight (80));

        auto area = card.withTrimmedTop (40).reduced (10, 8);
        rackView.setBounds (area);
        layoutRackPanel (area);
    }
}

void LegionEditor::layoutRackPanel (juce::Rectangle<int> area)
{
    const int w = area.getWidth() - 20;
    int y = 8;

    rackMixLabel.setBounds (12, y, 140, 16);
    rackMix.setBounds (156, y, w - 170, 16);
    y += 28;

    for (auto& rm : rackModules)
    {
        const auto& d = bwfx::moduleDescriptor (rm->type);
        rm->power.setBounds (12, y, 26, 20);
        rm->name .setBounds (42, y, w - 60, 20);
        y += 22;

        const int per = juce::jmax (1, (w - 24) / 76);
        for (int i = 0; i < d.numParams; ++i)
        {
            const int col = i % per, rowN = i / per;
            const int x = 14 + col * 76;
            const int ky = y + rowN * 74;
            rm->knobLabels[(size_t) i]->setBounds (x, ky, 72, 12);
            rm->knobs[(size_t) i]->setBounds (x, ky + 12, 72, 58);
        }
        y += ((d.numParams + per - 1) / per) * 74 + 10;
    }

    y += 6;
    for (size_t i = 0; i < macroKnobs.size(); ++i)
    {
        const int x = 14 + (int) i * 92;
        macroKnobs[i]->label .setBounds (x, y, 88, 12);
        macroKnobs[i]->slider.setBounds (x, y + 12, 88, 62);
    }
    y += 84;

    rackPanel.setSize (area.getWidth() - 4, y);
}

void LegionEditor::timerCallback()
{
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    const int lat = proc.getLatencySamples();
    const float f0 = proc.detectedF0();

    latencyLabel.setText (juce::String (lat) + " smp / "
                          + juce::String (1000.0 * lat / fs, 1) + " ms"
                          + (f0 > 20.0f ? "   f0 " + juce::String ((int) std::lround (f0)) + " Hz"
                                        : juce::String()),
                          juce::dontSendNotification);
    //  the strips are behind the overlay while the rack is open — no point
    //  redrawing them, and it would drag the whole overlay with them
    if (! rackOpen)
        repaint (0, 118, getWidth(), getHeight() - 118);
}
