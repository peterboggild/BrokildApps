#include "BwfxPanel.h"

#include <bwfx_juce.h>

namespace
{
    //  Rite's palette, and BWFX's one mandated accent
    const juce::Colour kGround { 0xff141010 };
    const juce::Colour kClay   { 0xff2a211c };
    const juce::Colour kAsh    { 0xffe6ddcd };
    const juce::Colour kSmoke  { 0xff6b625a };
    const juce::Colour kTeal   { 0xff35c9c0 };

    constexpr int kCardH   = 30;    // a collapsed pedal: its header only
    constexpr int kRowH    = 74;    // a row of knobs
    constexpr int kGap     = 6;

    /*  The fragment's own value formatter, mirrored exactly. BWFX stores a
        rate as hundredths of a hertz and a time as hundredths of a second, so
        without this a knob reads "400.0000" where the original reads
        "4.00 Hz" — which is how Legion's rack shipped before it was noticed. */
    juce::String bwfxText (const bwfx::ParamDesc& pd, double v)
    {
        const juce::String u (pd.unit != nullptr ? pd.unit : "");

        if (pd.choices != nullptr && *pd.choices != 0)
        {
            auto c = juce::StringArray::fromTokens (juce::String (pd.choices), "|", "");
            if (c.size() > 0)
                return c[juce::jlimit (0, c.size() - 1, juce::roundToInt (v))];
        }
        if (u == "%")   return juce::String (juce::roundToInt (v)) + " %";
        if (u == "dB")  return juce::String (v, 1) + " dB";
        if (u == "ms")  return juce::String (juce::roundToInt (v)) + " ms";
        if (u == "cHz") return juce::String (v / 100.0, 2) + " Hz";
        if (u == "dHz") return juce::String (v / 10.0,  1) + " Hz";
        if (u == "cs")  return juce::String (v / 100.0, 2) + " s";
        return juce::String (juce::roundToInt (v * 100.0) / 100.0, 2);
    }

    void styleKnob (juce::Slider& s, const bwfx::ParamDesc& pd, double value)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 14);
        s.setRange (pd.lo, pd.hi, pd.step);
        //  BY VALUE: a ParamDesc is a POD of literals, so the lambda must not
        //  depend on the descriptor table outliving this control
        s.textFromValueFunction = [pd] (double v) { return bwfxText (pd, v); };
        s.setValue (value, juce::dontSendNotification);
        s.updateText();
        s.setColour (juce::Slider::rotarySliderFillColourId, kTeal);
        s.setColour (juce::Slider::textBoxTextColourId, kAsh);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void stylePresence (juce::Slider& s, float value)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 14);
        s.setRange (0.0, 1.0, 0.001);
        s.textFromValueFunction = [] (double v)
            { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
        s.setValue (value, juce::dontSendNotification);
        s.updateText();
        s.setColour (juce::Slider::thumbColourId, kTeal);
        s.setColour (juce::Slider::trackColourId, kTeal.withAlpha (0.55f));
        s.setColour (juce::Slider::backgroundColourId, kGround);
        s.setColour (juce::Slider::textBoxTextColourId, kTeal);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleHead (juce::Label& l, const juce::String& t, float size, juce::Colour c,
                    bool bold = false)
    {
        l.setText (t, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, c);
        l.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }
}

// ---------------------------------------------------------------------------
BwfxPanel::BwfxPanel (bwfx::Rack& r, juce::AudioProcessorValueTreeState& s)
    : rack (r), apvts (s)
{
    setOpaque (true);                       // it HIDES the panel; that is the job
    setInterceptsMouseClicks (true, true);

    styleHead (title,    "BROKILD WORLD FX", 17.0f, kTeal, true);
    styleHead (subtitle, "THE WORLD RACK  -  EVERY BROKILD SYNTH, ONE RACK", 10.0f, kSmoke);
    styleHead (fxHead,   "FX RACK",      12.0f, kTeal, true);
    styleHead (specHead, "SPECTRA RACK", 12.0f, kTeal, true);
    styleHead (foot, juce::String ("BWFX ") + bwfx::Rack::version()
                     + "   -   the rack runs AFTER the harmony, so it colours the"
                       " whole choir and the dry voice together", 10.0f, kSmoke);
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (fxHead);
    addAndMakeVisible (specHead);
    addAndMakeVisible (foot);

    /*  SPECTRA writes a modulation BUS that the host engine has to consume,
        and Legion does not consume it: its voices are pitch-shifted copies of
        your own, not synth voices with a detune to bend. Saying so is better
        than showing six characters that quietly do nothing, which is what the
        fragment's own "arriving" plate exists to avoid. */
    styleHead (busNote,
               rack.isWorldModConsumed()
                   ? "armed characters reach this instrument"
                   : "the characters below that carry AUDIO work here; the ones that only"
                     " write the modulation bus do not, because Legion does not map"
                     " that bus onto its harmony voices",
               10.0f, kSmoke);
    busNote.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (busNote);

    styleHead (presetLabel, "PRESETS", 10.0f, kSmoke);
    presets.addItem ("-", 1);
    for (int i = 0; i < bwfx::numPresets(); ++i)
        presets.addItem (juce::String (bwfx::presetName (i)), i + 2);
    presets.setSelectedId (1, juce::dontSendNotification);
    presets.setColour (juce::ComboBox::backgroundColourId, kClay);
    presets.setColour (juce::ComboBox::textColourId, kAsh);
    presets.onChange = [this]
    {
        const int i = presets.getSelectedId() - 2;
        if (i < 0 || i >= bwfx::numPresets()) return;
        rack.fromJson (bwfx::presetBlob (i));
        rebuild();
        resized();
    };
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presets);

    styleHead (mixLabel, "RACK MIX", 10.0f, kSmoke);
    mix.setSliderStyle (juce::Slider::LinearHorizontal);
    mix.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    mix.setRange (0.0, 1.0, 0.001);
    mix.textFromValueFunction = [] (double v)
        { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
    mix.setValue (rack.getMix(), juce::dontSendNotification);
    mix.updateText();
    mix.setColour (juce::Slider::thumbColourId, kTeal);
    mix.setColour (juce::Slider::trackColourId, kTeal.withAlpha (0.55f));
    mix.setColour (juce::Slider::backgroundColourId, kGround);
    mix.setColour (juce::Slider::textBoxTextColourId, kAsh);
    mix.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    mix.onValueChange = [this] { rack.setMix ((float) mix.getValue()); };
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (mix);

    close.setColour (juce::TextButton::textColourOffId, kAsh);
    close.setColour (juce::TextButton::buttonColourId, kClay);
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);

    view.setViewedComponent (&board, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);

    for (int i = 0; i < 5; ++i)
    {
        auto m = std::make_unique<Macro>();
        m->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        m->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
        m->slider.setColour (juce::Slider::rotarySliderFillColourId, kTeal);
        m->slider.setColour (juce::Slider::textBoxTextColourId, kAsh);
        m->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        styleHead (m->label, "MACRO " + juce::String (i + 1), 10.0f, kSmoke);
        m->label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (m->slider);
        addAndMakeVisible (m->label);
        m->attach = std::make_unique<SliderAttach> (apvts, bwfx_juce::macroParamId (i), m->slider);
        macros.push_back (std::move (m));
    }

    rebuild();
}

BwfxPanel::~BwfxPanel() = default;

// ---------------------------------------------------------------------------
// Generated from the descriptors, never typed. The FX column is walked in
// CHAIN ORDER (rack.getOrder), because the order is the thing this column
// exists to show and to change.
void BwfxPanel::rebuild()
{
    modules.clear();
    chars.clear();
    board.removeAllChildren();

    std::vector<int> order ((size_t) juce::jmax (1, bwfx::numModuleTypes()), 0);
    rack.getOrder (order.data());

    for (int slot = 0; slot < bwfx::numModuleTypes(); ++slot)
    {
        const int t = order[(size_t) slot];
        const auto& d = bwfx::moduleDescriptor (t);
        auto c = std::make_unique<ModuleCard>();
        c->type = t;

        styleHead (c->index, juce::String (slot + 1).paddedLeft ('0', 2), 11.0f, kSmoke, true);
        styleHead (c->name,  d.name, 12.0f, kAsh, true);
        styleHead (c->sub,   d.sub,  10.0f, kSmoke);
        board.addAndMakeVisible (c->index);
        board.addAndMakeVisible (c->name);
        board.addAndMakeVisible (c->sub);

        c->power.setColour (juce::ToggleButton::tickColourId, kTeal);
        c->power.setToggleState (rack.getEnabled (t), juce::dontSendNotification);
        {
            auto* pw = &c->power;
            const int type = t;
            c->power.onClick = [this, type, pw]
            {
                rack.setEnabled (type, pw->getToggleState());
                rebuild();            // a pedal that is off folds to its header
                resized();
            };
        }
        board.addAndMakeVisible (c->power);

        for (auto* b : { &c->up, &c->down })
        {
            b->setColour (juce::TextButton::textColourOffId, kTeal);
            b->setColour (juce::TextButton::buttonColourId, kClay);
            board.addAndMakeVisible (*b);
        }
        {
            const int type = t;
            c->up.onClick   = [this, type] { moveModule (type, -1); };
            c->down.onClick = [this, type] { moveModule (type, +1); };
        }

        if (rack.getEnabled (t))
        {
            styleHead (c->presenceLabel, "PRESENCE", 9.5f, kTeal);
            stylePresence (c->presence, rack.getPresence (t));
            {
                auto* pr = &c->presence;
                const int type = t;
                c->presence.onValueChange = [this, type, pr]
                    { rack.setPresence (type, (float) pr->getValue()); };
            }
            board.addAndMakeVisible (c->presenceLabel);
            board.addAndMakeVisible (c->presence);

            for (int pi = 0; pi < d.numParams; ++pi)
            {
                auto sl = std::make_unique<juce::Slider>();
                styleKnob (*sl, d.params[pi], rack.getParam (t, pi));
                auto* raw = sl.get();
                const int type = t, p = pi;
                sl->onValueChange = [this, type, p, raw]
                    { rack.setParam (type, p, (float) raw->getValue()); };
                board.addAndMakeVisible (*sl);

                auto lb = std::make_unique<juce::Label>();
                styleHead (*lb, d.params[pi].name, 9.5f, kSmoke);
                lb->setJustificationType (juce::Justification::centred);
                board.addAndMakeVisible (*lb);

                c->knobs.push_back (std::move (sl));
                c->knobLabels.push_back (std::move (lb));
            }
        }
        modules.push_back (std::move (c));
    }

    for (int ci = 0; ci < bwfx::numCharacters(); ++ci)
    {
        const auto& d = bwfx::characterDescriptor (ci);
        auto c = std::make_unique<CharCard>();
        c->idx = ci;

        styleHead (c->name, d.name, 12.0f, kAsh, true);
        styleHead (c->sub,  d.sub,  10.0f, kSmoke);
        board.addAndMakeVisible (c->name);
        board.addAndMakeVisible (c->sub);

        c->arm.setColour (juce::ToggleButton::tickColourId, kTeal);
        c->arm.setToggleState (rack.getCharArmed (ci), juce::dontSendNotification);
        {
            auto* ar = &c->arm;
            const int idx = ci;
            c->arm.onClick = [this, idx, ar]
            {
                rack.setCharArmed (idx, ar->getToggleState());
                rebuild();
                resized();
            };
        }
        board.addAndMakeVisible (c->arm);

        if (rack.getCharArmed (ci))
        {
            styleHead (c->presenceLabel, "PRESENCE", 9.5f, kTeal);
            stylePresence (c->presence, rack.getCharPresence (ci));
            {
                auto* pr = &c->presence;
                const int idx = ci;
                c->presence.onValueChange = [this, idx, pr]
                    { rack.setCharPresence (idx, (float) pr->getValue()); };
            }
            board.addAndMakeVisible (c->presenceLabel);
            board.addAndMakeVisible (c->presence);

            for (int pi = 0; pi < d.numParams; ++pi)
            {
                auto sl = std::make_unique<juce::Slider>();
                styleKnob (*sl, d.params[pi], rack.getCharParam (ci, pi));
                auto* raw = sl.get();
                const int idx = ci, p = pi;
                sl->onValueChange = [this, idx, p, raw]
                    { rack.setCharParam (idx, p, (float) raw->getValue()); };
                board.addAndMakeVisible (*sl);

                auto lb = std::make_unique<juce::Label>();
                styleHead (*lb, d.params[pi].name, 9.5f, kSmoke);
                lb->setJustificationType (juce::Justification::centred);
                board.addAndMakeVisible (*lb);

                c->knobs.push_back (std::move (sl));
                c->knobLabels.push_back (std::move (lb));
            }
        }
        chars.push_back (std::move (c));
    }
}

// ---------------------------------------------------------------------------
// The chain order IS the sound, so moving a pedal has to move it in the rack
// and not merely on the screen. setOrder takes a permutation of type ids.
void BwfxPanel::moveModule (int type, int delta)
{
    const int n = bwfx::numModuleTypes();
    std::vector<int> order ((size_t) juce::jmax (1, n), 0);
    rack.getOrder (order.data());

    int at = -1;
    for (int i = 0; i < n; ++i) if (order[(size_t) i] == type) { at = i; break; }
    const int to = at + delta;
    if (at < 0 || to < 0 || to >= n) return;

    std::swap (order[(size_t) at], order[(size_t) to]);
    rack.setOrder (order.data(), n);
    rebuild();
    resized();
}

void BwfxPanel::refreshFromRack()
{
    mix.setValue (rack.getMix(), juce::dontSendNotification);
    rebuild();
    resized();
}

// ---------------------------------------------------------------------------
void BwfxPanel::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    if (ground.isValid())
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (0.30f);
        for (int y = 0; y < getHeight(); y += ground.getHeight())
            for (int x = 0; x < getWidth(); x += ground.getWidth())
                g.drawImageAt (ground, x, y);
        g.setOpacity (1.0f);
    }

    //  the teal rule under the header, as the fragment has
    g.setColour (kTeal.withAlpha (0.30f));
    g.fillRect (24, 86, getWidth() - 48, 1);

    //  and the divider between the two racks
    const int mid = getWidth() / 2;
    g.setColour (kTeal.withAlpha (0.14f));
    g.fillRect (mid - 1, 96, 1, getHeight() - 96 - 118);

    g.setColour (kTeal.withAlpha (0.22f));
    g.drawRect (getLocalBounds().reduced (8), 1);
}

void BwfxPanel::resized()
{
    auto r = getLocalBounds().reduced (24, 14);

    auto head = r.removeFromTop (60);
    close.setBounds (head.removeFromRight (78).withHeight (24));
    head.removeFromRight (16);
    {
        auto mixBox = head.removeFromRight (230);
        mixLabel.setBounds (mixBox.removeFromTop (14));
        mix.setBounds (mixBox.removeFromTop (22));
    }
    head.removeFromRight (16);
    {
        auto pBox = head.removeFromRight (190);
        presetLabel.setBounds (pBox.removeFromTop (14));
        presets.setBounds (pBox.removeFromTop (24));
    }
    title.setBounds (head.removeFromTop (24));
    subtitle.setBounds (head.removeFromTop (14));

    r.removeFromTop (14);

    //  the macros sit along the foot, under both racks
    auto footArea = r.removeFromBottom (24);
    foot.setBounds (footArea);
    auto macroRow = r.removeFromBottom (78);
    {
        auto lab = macroRow.removeFromLeft (64);
        //  reuse fxHead's font for the word MACROS without another label
        juce::ignoreUnused (lab);
        const int cw = macroRow.getWidth() / 5;
        for (auto& m : macros)
        {
            auto cell = macroRow.removeFromLeft (cw).reduced (6, 0);
            m->label.setBounds (cell.removeFromTop (13));
            m->slider.setBounds (cell);
        }
    }
    r.removeFromBottom (8);

    /*  The column headings belong to the PANEL, not to the scrolling board,
        so they need their own strip in PANEL coordinates — laying them out
        in the board's put them at 0,0, behind the title. */
    auto heads = r.removeFromTop (48);
    const int colW = (heads.getWidth() - 26) / 2;
    fxHead.setBounds (heads.getX(), heads.getY(), colW, 16);
    specHead.setBounds (heads.getX() + colW + 26, heads.getY(), colW, 16);
    busNote.setBounds (heads.getX() + colW + 26, heads.getY() + 18, colW, 30);

    view.setBounds (r);

    //  --- the two columns, in the board's own coordinates --------------------
    int leftY = 0, rightY = 0;
    const int leftX = 0, rightX = colW + 26;

    auto layCard = [&] (int x, int& y, int w,
                        juce::Label& name, juce::Label& sub, juce::Component& toggle,
                        juce::Label* index, juce::Component* up, juce::Component* down,
                        juce::Label* presLab, juce::Component* pres,
                        std::vector<std::unique_ptr<juce::Slider>>& knobs,
                        std::vector<std::unique_ptr<juce::Label>>& labels)
    {
        auto hdr = juce::Rectangle<int> (x, y, w, kCardH).reduced (6, 3);
        toggle.setBounds (hdr.removeFromLeft (26));
        if (index != nullptr) index->setBounds (hdr.removeFromLeft (22));
        if (down != nullptr)  down->setBounds (hdr.removeFromRight (34).reduced (1, 1));
        if (up != nullptr)    up->setBounds (hdr.removeFromRight (34).reduced (1, 1));
        name.setBounds (hdr.removeFromLeft (juce::jmin (108, hdr.getWidth())));
        sub.setBounds (hdr);
        y += kCardH;

        if (pres != nullptr)
        {
            auto pr = juce::Rectangle<int> (x, y, w, 20).reduced (8, 1);
            presLab->setBounds (pr.removeFromLeft (64));
            pres->setBounds (pr);
            y += 22;
        }
        if (! knobs.empty())
        {
            const int perRow = juce::jmax (1, (w - 12) / 84);
            for (size_t i = 0; i < knobs.size(); i += (size_t) perRow)
            {
                auto row = juce::Rectangle<int> (x, y, w, kRowH).reduced (6, 2);
                const int cw = row.getWidth() / perRow;
                for (int k = 0; k < perRow && (i + (size_t) k) < knobs.size(); ++k)
                {
                    auto cell = row.removeFromLeft (cw);
                    labels[i + (size_t) k]->setBounds (cell.removeFromTop (12));
                    knobs[i + (size_t) k]->setBounds (cell);
                }
                y += kRowH;
            }
        }
        y += kGap;
    };

    for (auto& m : modules)
        layCard (leftX, leftY, colW, m->name, m->sub, m->power, &m->index, &m->up, &m->down,
                 m->knobs.empty() ? nullptr : &m->presenceLabel,
                 m->knobs.empty() ? nullptr : &m->presence,
                 m->knobs, m->knobLabels);

    for (auto& c : chars)
        layCard (rightX, rightY, colW, c->name, c->sub, c->arm, nullptr, nullptr, nullptr,
                 c->knobs.empty() ? nullptr : &c->presenceLabel,
                 c->knobs.empty() ? nullptr : &c->presence,
                 c->knobs, c->knobLabels);

    board.setSize (r.getWidth() - 14, juce::jmax (leftY, rightY) + 12);
}
