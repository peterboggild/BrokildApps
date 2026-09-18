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

    constexpr int kHeadH = 116;
    constexpr int kGlobH = 62;
    constexpr int kLaneH = 40;
    constexpr int kMargin = 14;
}

std::unique_ptr<RiteEditor::Knob> RiteEditor::makeKnob (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
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
    title.setText ("RITE OF PASSAGE", juce::dontSendNotification);
    title.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAsh);
    addAndMakeVisible (title);

    readout.setJustificationType (juce::Justification::centredRight);
    readout.setColour (juce::Label::textColourId, kSmoke);
    readout.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (readout);

    //  the marker travelling through the gate
    position.setSliderStyle (juce::Slider::LinearHorizontal);
    position.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    position.setColour (juce::Slider::thumbColourId, kAsh);
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

    globals.push_back (makeKnob (rop_ids::mix,      "MIX"));
    globals.push_back (makeKnob (rop_ids::output,   "OUTPUT"));
    globals.push_back (makeKnob (rop_ids::spread,   "SPREAD"));
    globals.push_back (makeKnob (rop_ids::turn,     "TURN"));
    globals.push_back (makeKnob (rop_ids::monogate, "MONO GATE"));

    juce::StringArray fxNames { "—" };
    for (int t = 0; t < numEffects(); ++t) fxNames.add (effectDescriptor (t).name);

    for (int i = 0; i < kSlots; ++i)
    {
        auto& L = lanes[i];
        L.fx.addItemList (fxNames, 1);
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

        auto lane = [&] (juce::Slider& s, double lo, double hi, double v)
        {
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setRange (lo, hi, 0.001);
            s.setValue (v, juce::dontSendNotification);
            s.setColour (juce::Slider::thumbColourId, kYellow);
            s.setColour (juce::Slider::trackColourId, kOchre.withAlpha (0.5f));
            s.setColour (juce::Slider::backgroundColourId, kClay);
            addAndMakeVisible (s);
        };
        lane (L.enter, 0.0, 1.0, proc.rack().state (i).enter);
        lane (L.exitS, 0.0, 1.0, proc.rack().state (i).exit);
        lane (L.depth, 0.0, 1.0, proc.rack().state (i).depth);
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
        box (L.tail,  { "BYPASS", "SPILL", "CLEAR" }, (int) proc.rack().state (i).tail);
        L.curve.onChange = [this, i] { proc.rack().state (i).curve = lanes[i].curve.getSelectedId() - 1; };
        L.place.onChange = [this, i] { proc.rack().state (i).place = (Place) (lanes[i].place.getSelectedId() - 1); };
        L.tail.onChange  = [this, i] { proc.rack().state (i).tail  = (Tail)  (lanes[i].tail.getSelectedId() - 1); };
    }

    abHeading.setColour (juce::Label::textColourId, kOchre);
    abHeading.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (abHeading);

    rebuildSlotEditor();
    setSize (980, kHeadH + kGlobH + kSlots * (kLaneH + 6) + 210);
    startTimerHz (24);
}

RiteEditor::~RiteEditor() { stopTimer(); }

// ---------------------------------------------------------------------------
// The A and B of the selected slot, generated from its descriptor.
void RiteEditor::rebuildSlotEditor()
{
    ab.clear();
    const int type = proc.rack().slotEffect (selected);
    abHeading.setText (type < 0 ? "SLOT " + juce::String (selected + 1) + " — EMPTY"
                                : "SLOT " + juce::String (selected + 1) + " — "
                                  + effectDescriptor (type).name + "   "
                                  + effectDescriptor (type).sub,
                       juce::dontSendNotification);
    if (type < 0) { resized(); return; }

    const auto& d = effectDescriptor (type);
    for (int p = 0; p < d.numParams; ++p)
    {
        auto k = std::make_unique<ABKnob>();
        auto setup = [&] (juce::Slider& s, bool isB)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 13);
            s.setRange (d.params[p].lo, d.params[p].hi,
                        d.params[p].step > 0 ? d.params[p].step : 0.0);
            s.setValue (isB ? proc.rack().state (selected).B[p]
                            : proc.rack().state (selected).A[p], juce::dontSendNotification);
            s.setColour (juce::Slider::rotarySliderFillColourId, isB ? kEmber : kYellow);
            s.setColour (juce::Slider::textBoxTextColourId, kAsh);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s.onValueChange = [this, p, isB, &s]
            {
                auto& st = proc.rack().state (selected);
                (isB ? st.B[p] : st.A[p]) = (float) s.getValue();
            };
            addAndMakeVisible (s);
        };
        setup (k->a, false);
        setup (k->b, true);
        k->name.setText (d.params[p].name, juce::dontSendNotification);
        k->name.setJustificationType (juce::Justification::centred);
        k->name.setColour (juce::Label::textColourId, kSmoke);
        k->name.setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (k->name);
        ab.push_back (std::move (k));
    }
    resized();
}

juce::Rectangle<int> RiteEditor::laneBounds (int slot) const
{
    return { kMargin, kHeadH + kGlobH + slot * (kLaneH + 6), getWidth() - 2 * kMargin, kLaneH };
}

void RiteEditor::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kSlots; ++i)
        if (laneBounds (i).contains (e.getPosition()))
        {
            if (selected != i) { selected = i; rebuildSlotEditor(); }
            return;
        }
}

// ---------------------------------------------------------------------------
void RiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    const float t = proc.apvts.getRawParameterValue (rop_ids::position)->load() * 0.01f;

    // ---- the threshold: two uprights and a lintel -------------------------
    auto head = getLocalBounds().removeFromTop (kHeadH);
    g.setColour (kClay);
    g.fillRect (head.removeFromTop (kHeadH - 34));
    g.setColour (kOchre.withAlpha (0.55f));
    g.fillRect (kMargin, 40, 10, kHeadH - 56);                       // left upright
    g.fillRect (getWidth() - kMargin - 10, 40, 10, kHeadH - 56);     // right upright
    g.fillRect (kMargin, 40, getWidth() - 2 * kMargin, 8);           // the lintel

    //  three counted notches on each upright — marks, not ornament
    g.setColour (kAsh.withAlpha (0.35f));
    for (int k = 0; k < 3; ++k)
    {
        const int y = 60 + k * 13;
        g.fillRect (kMargin, y, 10, 2);
        g.fillRect (getWidth() - kMargin - 10, y, 10, 2);
    }

    // ---- the lanes, which take on heat as the marker passes ---------------
    for (int i = 0; i < kSlots; ++i)
    {
        auto r = laneBounds (i);
        const auto& s = proc.rack().state (i);
        const int type = proc.rack().slotEffect (i);
        const bool live = type >= 0 && s.on;

        g.setColour (i == selected ? kClay.brighter (0.16f) : kClay);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        //  the lane's span on the score, and how far the heat has reached
        const int x0 = r.getX() + (int) (s.enter * r.getWidth());
        const int x1 = r.getX() + (int) (s.exit  * r.getWidth());
        g.setColour ((live ? kOchre : kSmoke).withAlpha (0.30f));
        g.fillRect (x0, r.getY(), juce::jmax (2, x1 - x0), r.getHeight());

        if (live && t > s.enter)
        {
            const int xt = r.getX() + (int) (juce::jmin (t, s.exit) * r.getWidth());
            g.setColour (kEmber.withAlpha (0.55f));
            g.fillRect (x0, r.getY(), juce::jmax (1, xt - x0), r.getHeight());
        }

        //  tally notches along the lane
        g.setColour (kAsh.withAlpha (0.10f));
        for (int k = 1; k < 8; ++k)
        {
            const int x = r.getX() + k * r.getWidth() / 8;
            g.fillRect (x, r.getY() + 4, 1, r.getHeight() - 8);
        }
    }

    // ---- the marker's position across everything --------------------------
    const int mx = kMargin + (int) (t * (getWidth() - 2 * kMargin));
    g.setColour (kAsh.withAlpha (0.8f));
    g.fillRect (mx - 1, kHeadH + kGlobH - 6, 2, kSlots * (kLaneH + 6) + 8);

    g.setColour (kTeal.withAlpha (0.8f));
    g.drawText ("BWFX", getWidth() - 74, kHeadH - 30, 60, 18, juce::Justification::centredRight);
}

void RiteEditor::resized()
{
    auto head = getLocalBounds().removeFromTop (kHeadH).reduced (kMargin + 16, 0);
    title.setBounds (head.getX(), 8, 240, 26);
    readout.setBounds (head.getRight() - 300, 10, 300, 20);
    position.setBounds (head.getX(), 56, head.getWidth() - 120, 30);
    arrival.setBounds (head.getRight() - 104, 56, 104, 30);

    auto row = juce::Rectangle<int> (kMargin, kHeadH, getWidth() - 2 * kMargin, kGlobH).reduced (2, 2);
    for (auto& k : globals)
    {
        auto cell = row.removeFromLeft (84);
        k->label.setBounds (cell.removeFromTop (12));
        k->slider.setBounds (cell);
        row.removeFromLeft (4);
    }

    for (int i = 0; i < kSlots; ++i)
    {
        auto r = laneBounds (i).reduced (6, 5);
        lanes[i].on.setBounds (r.removeFromLeft (24));
        lanes[i].fx.setBounds (r.removeFromLeft (98).reduced (1, 2));
        r.removeFromLeft (6);
        lanes[i].curve.setBounds (r.removeFromRight (74).reduced (1, 2));
        lanes[i].tail.setBounds  (r.removeFromRight (76).reduced (1, 2));
        lanes[i].place.setBounds (r.removeFromRight (58).reduced (1, 2));
        r.removeFromRight (6);
        const int w = r.getWidth() / 3;
        lanes[i].enter.setBounds (r.removeFromLeft (w).reduced (2, 6));
        lanes[i].exitS.setBounds (r.removeFromLeft (w).reduced (2, 6));
        lanes[i].depth.setBounds (r.reduced (2, 6));
    }

    auto ed = getLocalBounds();
    ed.removeFromTop (kHeadH + kGlobH + kSlots * (kLaneH + 6) + 6);
    ed = ed.reduced (kMargin, 0);
    abHeading.setBounds (ed.removeFromTop (20));
    if (! ab.empty())
    {
        const int cw = juce::jmin (86, ed.getWidth() / (int) ab.size());
        auto aRow = ed.removeFromTop (86);
        auto bRow = ed.removeFromTop (86);
        for (size_t i = 0; i < ab.size(); ++i)
        {
            auto ca = aRow.removeFromLeft (cw);
            ab[i]->name.setBounds (ca.removeFromTop (12));
            ab[i]->a.setBounds (ca.reduced (2, 0));
            ab[i]->b.setBounds (bRow.removeFromLeft (cw).reduced (2, 0));
        }
    }
}

void RiteEditor::timerCallback()
{
    const double l = proc.loudness();
    readout.setText ((l > -100.0 ? juce::String (l, 1) + " LUFS" : juce::String ("-inf"))
                     + (proc.rack().arrivalPending() ? "   ARMED"
                        : (proc.rack().arrived() ? "   ARRIVED" : juce::String())),
                     juce::dontSendNotification);
    repaint();
}
