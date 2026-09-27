#include "PluginEditor.h"

namespace
{
    /*  The voice strips' geometry is read by BOTH paint() and resized(), and
        it used to be written out in each. Giving the globals row the height it
        needed moved one copy and not the other, so the first strip was painted
        over the global knobs. One source, so that cannot happen again. */
    constexpr int kHeadH    = 58;
    constexpr int kGlobH    = 132;   //  the globals row, which holds the LEVELLER
    constexpr int kStripTop = kHeadH + kGlobH;
    constexpr float kMeterDb = 18.0f;   //  the meter spans +/- this
    constexpr int kStripH   = 148;
    constexpr int kStripGap = 8;

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

    buildLabel.setText (LEGION_BUILD_ID, juce::dontSendNotification);
    buildLabel.setColour (juce::Label::textColourId, kDim);
    buildLabel.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (buildLabel);

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

    // ---- the LEVELLER ------------------------------------------------------
    levOn.setColour (juce::ToggleButton::tickColourId, kAmber);
    levOn.setColour (juce::ToggleButton::textColourId, kAmber);
    addAndMakeVisible (levOn);
    levOnAttach = std::make_unique<ButtonAttach> (proc.apvts, legion_ids::levOn, levOn);

    levHint.setText ("down from the top, up from the bottom", juce::dontSendNotification);
    levHint.setColour (juce::Label::textColourId, kDim);
    levHint.setFont (juce::FontOptions (10.5f));
    levHint.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (levHint);

    addKnob (legion_ids::levTop,   "TOP",   *this, levKnobs);
    addKnob (legion_ids::levRatio, "RATIO", *this, levKnobs);
    addKnob (legion_ids::levLift,  "LIFT",  *this, levKnobs);
    addKnob (legion_ids::levFloor, "FLOOR", *this, levKnobs);
    addKnob (legion_ids::levSpeed, "SPEED", *this, levKnobs);

    // ---- BWFX -------------------------------------------------------------
    rackButton.setClickingTogglesState (true);
    rackButton.setColour (juce::TextButton::textColourOffId, kTeal);
    rackButton.setColour (juce::TextButton::textColourOnId, kBack);
    rackButton.setColour (juce::TextButton::buttonOnColourId, kTeal);
    rackButton.onClick = [this]
    {
        /*  Built on first open, so an editor nobody opens the rack on costs
            nothing -- and refreshed from the rack on every later open, because
            a patch load or a macro can have moved the chain underneath it. */
        if (overlay == nullptr)
        {
            overlay = std::make_unique<BwfxPanel> (proc.rack(), proc.apvts);
            overlay->onClose = [this] { rackButton.setToggleState (false, juce::sendNotificationSync); };
            addAndMakeVisible (*overlay);
            overlay->setBounds (getLocalBounds());
        }
        else overlay->refreshFromRack();

        overlay->setVisible (rackButton.getToggleState());
        if (rackButton.getToggleState()) overlay->toFront (true);
    };
    addAndMakeVisible (rackButton);


    setSize (1060, kStripTop + legion::kVoices * (kStripH + kStripGap) + 12);
    startTimerHz (15);
}

LegionEditor::~LegionEditor() { stopTimer(); }

// ---------------------------------------------------------------------------
void LegionEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBack);

    auto r = getLocalBounds();
    g.setColour (kPanel);
    g.fillRect (r.removeFromTop (kHeadH));

    //  the LEVELLER panel, and its meter: 0 dB is the centre line, reduction
    //  runs down in amber, lift runs up in teal. Dim while the switch is off.
    if (! levPanel.isEmpty())
    {
        const bool on = levOn.getToggleState();
        g.setColour (on ? kPanel : kPanel.withAlpha (0.55f));
        g.fillRoundedRectangle (levPanel.toFloat(), 6.0f);

        const auto m = levMeter.toFloat();
        g.setColour (kBack);
        g.fillRoundedRectangle (m, 3.0f);
        const float mid = m.getCentreY();
        const float h = juce::jlimit (-1.0f, 1.0f, levShown / kMeterDb) * (m.getHeight() * 0.5f - 2.0f);
        if (on && std::abs (h) > 0.5f)
        {
            g.setColour (h > 0 ? kTeal : kAmber);
            if (h > 0) g.fillRect (m.getX() + 3.0f, mid - h, m.getWidth() - 6.0f, h);
            else       g.fillRect (m.getX() + 3.0f, mid, m.getWidth() - 6.0f, -h);
        }
        g.setColour (kDim);
        g.drawHorizontalLine ((int) mid, m.getX(), m.getRight());
        g.setFont (juce::FontOptions (9.5f));
        g.drawText (on ? juce::String (levShown, 1) + " dB" : juce::String ("off"),
                    levMeter.withY (levMeter.getBottom() + 1).withHeight (12).expanded (14, 0),
                    juce::Justification::centred);
    }

    //  the voice strips
    const int stripTop = kStripTop;
    const int stripH   = kStripH;
    for (int v = 0; v < legion::kVoices; ++v)
    {
        const bool on = proc.apvts.getRawParameterValue (legion_ids::voice (v, "on"))->load() > 0.5f;
        g.setColour (on ? kPanel : kPanel.withAlpha (0.45f));
        g.fillRoundedRectangle ((float) (14 + 0), (float) (stripTop + v * (stripH + kStripGap)),
                                (float) (getWidth() - 28), (float) stripH, 6.0f);
    }
}

void LegionEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (kHeadH).reduced (14, 8);

    title.setBounds (head.removeFromLeft (110));
    buildLabel.setBounds (head.removeFromLeft (70).withTrimmedTop (10));
    latencyLabel.setBounds (head.removeFromRight (230));
    rackButton.setBounds (head.removeFromRight (90).reduced (2, 6));

    //  globals
    auto row = r.removeFromTop (kGlobH).reduced (14, 4);
    const int kw = 86;
    for (auto& k : globalKnobs)
    {
        auto cell = row.removeFromLeft (kw).withSizeKeepingCentre (kw, 96);
        k->label.setBounds (cell.removeFromTop (13));
        k->slider.setBounds (cell);
        row.removeFromLeft (6);
    }
    auto box1 = row.removeFromLeft (110);
    detailLabel.setBounds (box1.removeFromTop (13));
    detailBox.setBounds (box1.withSizeKeepingCentre (box1.getWidth() - 4, 30));
    row.removeFromLeft (8);
    auto box2 = row.removeFromLeft (110);
    rackPosLabel.setBounds (box2.removeFromTop (13));
    rackPosBox.setBounds (box2.withSizeKeepingCentre (box2.getWidth() - 4, 30));

    //  the LEVELLER takes the rest of the row
    row.removeFromLeft (16);
    levPanel = row;
    {
        auto lp = row.reduced (10, 4);
        auto top = lp.removeFromTop (20);
        levOn.setBounds (top.removeFromLeft (110));
        levHint.setBounds (top);
        levMeter = lp.removeFromRight (30).withTrimmedBottom (14).withTrimmedTop (2);
        lp.removeFromRight (8);
        const int cw = lp.getWidth() / (int) levKnobs.size();
        for (auto& k : levKnobs)
        {
            auto cell = lp.removeFromLeft (cw);
            k->label.setBounds (cell.removeFromTop (13));
            k->slider.setBounds (cell);
        }
    }

    //  the voice strips
    const int stripH = kStripH;
    for (int v = 0; v < legion::kVoices; ++v)
    {
        auto strip = juce::Rectangle<int> (14, kStripTop + v * (stripH + kStripGap), getWidth() - 28, stripH)
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
    if (overlay == nullptr || ! overlay->isVisible())
        repaint (0, kStripTop, getWidth(), getHeight() - kStripTop);

    //  the meter at 15 Hz, a little ballistic so it reads rather than flickers
    const float gdb = proc.levellerGainDb();
    levShown += 0.5f * (gdb - levShown);
    if (overlay == nullptr || ! overlay->isVisible())
        repaint (levPanel);
}
