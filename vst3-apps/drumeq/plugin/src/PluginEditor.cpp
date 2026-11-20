#include "PluginEditor.h"
#include "MachineArt.h"      // the drum family's shared machine parts

namespace
{
    const juce::Colour kBack   { 0xff0d0e10 };
    const juce::Colour kPanel2 { 0xff1f2227 };
    const juce::Colour kInk    { 0xffece8df };
    const juce::Colour kDim    { 0xff858c98 };
    const juce::Colour kFaint  { 0xff2c3037 };
    const juce::Colour kHot    { 0xffffa21f };   // the house accent: amber
    const juce::Colour kMaster { 0xff3d8bff };   // the MASTER panel: blue

    const juce::Colour kBand[4] = { juce::Colour (0xffff6a2a), juce::Colour (0xffffd23a),
                                    juce::Colour (0xff3fd6a6), juce::Colour (0xff6aa8ff) };
    const juce::Colour kFilt  { 0xff36c9bd };
    const juce::Colour kGate  { 0xffb58cff };
    const juce::Colour kComp  { 0xff4aa8ff };
    const juce::Colour kIo    { 0xffcfd3da };

    constexpr int kW = 1240, kH = 744, kCol = 70;

    juce::Font mono (float h)    { return machineart::mono (h); }
    juce::Font stencil (float h) { return machineart::stencil (h); }

    juce::String hzText (double v)
    {
        return v >= 1000.0 ? juce::String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    }

    juce::String noteName (double hz)
    {
        static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int m = juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0));
        return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    }

    //  what a band IS on the drum it is named for: the tooltip
    juce::String bandTip (const juce::String& label)
    {
        if (label == "THUMP")    return "THUMP - the kick's weight, its fundamental. FIND DRUM puts this on the drum's own note.";
        if (label == "BOX")      return "BOX - the cardboard, boxy ring of the shell. Cutting it is the most common move on a close mic.";
        if (label == "BEATER")   return "BEATER - the click of the beater on the head: what lets the kick cut through guitars.";
        if (label == "CLICK")    return "CLICK - the top of the attack. A shelf for air, a bell for a pointed metal click.";
        if (label == "SUB")      return "SUB - the low end the outside mic catches and the inside mic cannot.";
        if (label == "MUD")      return "MUD - low mids that blur the kit when every mic has them. Cut, do not boost.";
        if (label == "PRESENCE") return "PRESENCE - where the outside kick's beater lives, usually tamed in favour of the inside mic.";
        if (label == "TOP")      return "TOP - cymbal spill on a kick or room mic. Shelve it down.";
        if (label == "BODY")     return "BODY - the drum's fundamental and weight. FIND DRUM puts this on the drum's own note.";
        if (label == "CRACK")    return "CRACK - the stick on the snare head. The attack that cuts through.";
        if (label == "AIR")      return "AIR - the very top: shimmer and space. A gentle shelf.";
        if (label == "HONK")     return "HONK - the snare-bottom mic's nasal mids. Usually cut.";
        if (label == "WIRES")    return "WIRES - the snare wires' sizzle, what the bottom mic is there for.";
        if (label == "ATTACK")   return "ATTACK - the stick on the tom head. Boost to bring the toms forward in a dense mix.";
        if (label == "BLEED")    return "BLEED - spill from the cymbals (tom mics) or the kick and snare (cymbal mics). Shelve it away.";
        if (label == "CLANG")    return "CLANG - the hat's metallic clank. Small cuts tame a gong-y hat.";
        if (label == "HARSH")    return "HARSH - where cymbals and rooms bite the ear. Cut to calm, carefully.";
        if (label == "WASH")     return "WASH - the ride's build-up between strokes.";
        if (label == "PING")     return "PING - the stick on the ride's bow: the definition.";
        if (label == "LOW")      return "LOW - the kit's weight in the room and overhead mics.";
        return label;
    }

    struct KnobDef { const char* id; const char* label; const char* tip; };
}

//==============================================================================
DeLook::DeLook()
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

void DeLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float size = (float) juce::jmin (w, h) - 4.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;

    const bool bip = (bool) s.getProperties()["bipolar"];
    const float aVal = a0 + pos * (a1 - a0);
    //  a bipolar knob's arc starts at ZERO, wherever zero is on its range
    const double zp = (s.getMinimum() < 0.0 && s.getMaximum() > 0.0) ? s.valueToProportionOfLength (0.0) : 0.5;
    const float aFrom = bip ? a0 + (float) zp * (a1 - a0) : a0;
    juce::Path track, arc;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
    g.setColour (kFaint);
    g.strokePath (track, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    arc.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (aFrom, aVal), juce::jmax (aFrom, aVal), true);
    g.setColour (s.isEnabled() ? accent : accent.withAlpha (0.35f));
    g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kd = (r - 5.0f) * 2.0f;
    if (machineart::drawKnob (g, juce::Rectangle<float> (kd, kd).withCentre (c), aVal, "knob.png", s.isEnabled()))
        return;
    const float cr = r - 7.0f;
    juce::ColourGradient cap (juce::Colour (0xff3a3f47), c.x - cr * 0.5f, c.y - cr * 0.7f,
                              juce::Colour (0xff121417), c.x + cr * 0.4f, c.y + cr * 0.8f, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    const float px = c.x + std::sin (aVal) * (cr - 3.0f), py = c.y - std::cos (aVal) * (cr - 3.0f);
    g.setColour (kInk);
    g.drawLine (c.x + std::sin (aVal) * cr * 0.35f, c.y - std::cos (aVal) * cr * 0.35f, px, py, 2.2f);
}

void DeLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),
                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());
}

void DeLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    machineart::drawTape (g, r);
    juce::Path tri;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (kHot);
    g.fillPath (tri);
}

juce::Font DeLook::getComboBoxFont (juce::ComboBox&) { return mono (14.0f); }

void DeLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font DeLook::getTextButtonFont (juce::TextButton&, int h) { return mono (juce::jmin ((float) h * 0.55f, 12.5f)); }
juce::Font DeLook::getPopupMenuFont()                          { return mono (14.5f); }

juce::Font DeLook::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr) return mono (12.0f);
    return juce::LookAndFeel_V4::getLabelFont (l);
}

void DeLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (kPanel2);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (kHot.withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    g.setColour (kInk);
    g.setFont (mono (13.0f));
    g.drawFittedText (text, 10, 6, w - 20, h - 12, juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> DeLook::getTooltipBounds (const juce::String& text, juce::Point<int> at, juce::Rectangle<int> parent)
{
    const int w = 340;
    const int lines = juce::jmax (1, (int) std::ceil (text.length() / 42.0));
    const int h = 14 + lines * 17;
    return juce::Rectangle<int> (at.x + 14, at.y + 18, w, h).constrainedWithin (parent);
}

//==============================================================================
//  THE EQ DISPLAY
juce::Rectangle<float> EqDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 10.0f).withTrimmedTop (24.0f).withTrimmedBottom (16.0f).withTrimmedRight (26.0f);
}

float EqDisplay::xOf (double hz) const
{
    const auto p = plot();
    return p.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * p.getWidth();
}

double EqDisplay::hzOf (float x) const
{
    const auto p = plot();
    return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth())));
}

float EqDisplay::yOf (double db) const
{
    const auto p = plot();
    return p.getCentreY() - (float) (db / 20.0) * p.getHeight() * 0.5f;
}

double EqDisplay::dbOf (float y) const
{
    const auto p = plot();
    return -(double) (y - p.getCentreY()) / (p.getHeight() * 0.5) * 20.0;
}

namespace
{
    //  the response the engine runs, from the same designs it runs, at its rate
    struct Curve
    {
        double fs;
        const de::Params& p;
        double band (int b, double f) const
        {
            const float fr[4] = { p.b1f, p.b2f, p.b3f, p.b4f }, gn[4] = { p.b1g, p.b2g, p.b3g, p.b4g }, q[4] = { p.b1q, p.b2q, p.b3q, p.b4q };
            de::FType t = de::F_PEAK;
            if (b == 0 && p.b1t < 0.5f) t = de::F_LSHELF;
            if (b == 3 && p.b4t < 0.5f) t = de::F_HSHELF;
            return de::magDb (de::design (t, fr[b], fs, gn[b], q[b]), f, fs);
        }
        double filters (double f) const
        {
            double d = 0;
            if (p.hpfs > 1.5f) d += de::magDb (de::design (de::F_HP, p.hpf, fs, 0, 0.5411961), f, fs) + de::magDb (de::design (de::F_HP, p.hpf, fs, 0, 1.3065630), f, fs);
            else if (p.hpfs > 0.5f) d += de::magDb (de::design (de::F_HP, p.hpf, fs, 0, 0.7071068), f, fs);
            if (p.lpfs > 1.5f) d += de::magDb (de::design (de::F_LP, p.lpf, fs, 0, 0.5411961), f, fs) + de::magDb (de::design (de::F_LP, p.lpf, fs, 0, 1.3065630), f, fs);
            else if (p.lpfs > 0.5f) d += de::magDb (de::design (de::F_LP, p.lpf, fs, 0, 0.7071068), f, fs);
            return d;
        }
        double total (double f) const
        {
            double d = filters (f);
            if (p.eq > 0.5f) for (int b = 0; b < 4; ++b) d += band (b, f);
            return d;
        }
    };
}

juce::Point<float> EqDisplay::nodePos (int i, const de::Params& p) const
{
    const double fs = juce::jmax (8000.0, proc->getEngine().sampleRate());
    const Curve cv { fs, p };
    const float fr[4] = { p.b1f, p.b2f, p.b3f, p.b4f }, gn[4] = { p.b1g, p.b2g, p.b3g, p.b4g };
    if (i < 4) return { xOf (fr[i]), yOf (gn[i]) };
    const double f = i == 4 ? p.hpf : p.lpf;
    return { xOf (f), yOf (juce::jlimit (-19.0, 19.0, cv.total (f))) };
}

int EqDisplay::hit (juce::Point<float> at) const
{
    const auto p = proc->currentParams();
    int best = -1; float bd = 15.0f;
    for (int i = 0; i < 6; ++i)
    {
        if (i == 4 && p.hpfs < 0.5f) continue;
        if (i == 5 && p.lpfs < 0.5f) continue;
        const float d = nodePos (i, p).getDistanceFrom (at);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void EqDisplay::setParam (const char* id, float v)
{
    if (auto* prm = proc->apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void EqDisplay::setLogSpectra (const float* pre, const float* post, int n, double rate)
{
    logBins = true;
    specRate = rate;
    auto fall = [n] (std::vector<float>& held, const float* now)
    {
        if ((int) held.size() != n) { held.assign (now, now + n); return; }
        for (int i = 0; i < n; ++i) held[(size_t) i] = std::max (now[i], held[(size_t) i] - 1.2f);
    };
    if (n <= 0) { preDb.clear(); postDb.clear(); }
    else { fall (preDb, pre); fall (postDb, post); }
    repaint();
}

void EqDisplay::updateSpectra()
{
    logBins = false;
    auto& e = proc->getEngine();
    std::vector<float> a, b;
    const int pos = e.ringPos();
    de::spectrumDb (e.ringPre(), de::Engine::kRing, pos, 4096, a);
    de::spectrumDb (e.ringPost(), de::Engine::kRing, pos, 4096, b);
    auto fall = [] (std::vector<float>& held, const std::vector<float>& now)
    {
        if (held.size() != now.size()) { held = now; return; }
        for (size_t i = 0; i < now.size(); ++i) held[i] = std::max (now[i], held[i] - 1.2f);
    };
    fall (preDb, a);
    fall (postDb, b);
    repaint();
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kBack);
    g.fillRoundedRectangle (b, 8.0f);
    machineart::drawBezel (g, b, 8.0f);
    const auto pl = plot();
    const auto p = proc->currentParams();
    const int src = proc->currentSource();
    const auto& sd = de::source (src);
    const double fs = juce::jmax (8000.0, proc->getEngine().sampleRate());
    const Curve cv { fs, p };

    //  ---- the grid --------------------------------------------------------
    g.setFont (mono (11.0f));
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xOf (f);
        g.setColour (kFaint.withAlpha (0.9f));
        g.drawVerticalLine ((int) x, pl.getY(), pl.getBottom());
        g.setColour (kDim);
        const juce::String t = f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f);
        g.drawText (t, (int) x - 20, (int) pl.getBottom() + 2, 40, 13, juce::Justification::centred);
    }
    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yOf (db);
        g.setColour (db == 0 ? kDim.withAlpha (0.55f) : kFaint.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, pl.getX(), pl.getRight());
        g.setColour (kDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), (int) pl.getRight() + 2, (int) y - 7, 26, 14, juce::Justification::left);
    }

    //  ---- the sound: before (grey line), after (lit) ---------------------------
    auto spec = [&] (const std::vector<float>& d, bool filled, juce::Colour c)
    {
        if (d.empty()) return;
        const int bins = (int) d.size();
        juce::Path path;
        bool any = false;
        for (float x = pl.getX(); x <= pl.getRight(); x += 2.0f)
        {
            const double f0 = hzOf (x), f1 = hzOf (x + 2.0f);
            int i0 = logBins ? (int) (std::log (f0 / 20.0) / std::log (1000.0) * bins) : (int) (f0 / (fs * 0.5) * bins);
            int i1 = juce::jmax (i0 + 1, logBins ? (int) (std::log (f1 / 20.0) / std::log (1000.0) * bins) : (int) (f1 / (fs * 0.5) * bins));
            i0 = juce::jlimit (1, bins - 1, i0); i1 = juce::jlimit (i0 + 1, bins, i1);
            float m = -200.0f;
            for (int i = i0; i < i1; ++i) m = juce::jmax (m, d[(size_t) i]);
            //  the spectrum on its own scale: -84 dBFS at the floor, 0 at the top
            const float y = pl.getBottom() - juce::jlimit (0.0f, 1.0f, (m + 84.0f) / 84.0f) * pl.getHeight();
            if (! any) { path.startNewSubPath (x, filled ? pl.getBottom() : y); if (filled) path.lineTo (x, y); any = true; }
            else path.lineTo (x, y);
        }
        if (! any) return;
        if (filled)
        {
            path.lineTo (pl.getRight(), pl.getBottom());
            path.closeSubPath();
            g.setColour (c.withAlpha (0.22f));
            g.fillPath (path);
        }
        else
        {
            g.setColour (c);
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
    };
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pl.toNearestInt());
        spec (preDb, false, kDim.withAlpha (0.55f));
        spec (postDb, true, kHot);
    }
    if (note.isNotEmpty())
    {
        g.setFont (mono (13.0f));
        g.setColour (kDim);
        g.drawText (note, pl.toNearestInt().withTrimmedBottom ((int) (pl.getHeight() * 0.62f)), juce::Justification::centred);
    }

    //  ---- each band's own shape, then the whole curve ------------------------------
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pl.toNearestInt());
        const float y0 = yOf (0.0);
        for (int bnd = 0; bnd < 4; ++bnd)
        {
            juce::Path fill;
            fill.startNewSubPath (pl.getX(), y0);
            for (float x = pl.getX(); x <= pl.getRight(); x += 3.0f)
                fill.lineTo (x, yOf (juce::jlimit (-22.0, 22.0, cv.band (bnd, hzOf (x)))));
            fill.lineTo (pl.getRight(), y0);
            fill.closeSubPath();
            g.setColour (kBand[bnd].withAlpha (p.eq > 0.5f ? 0.16f : 0.05f));
            g.fillPath (fill);
        }
        juce::Path tot;
        int pts = 0;
        for (float x = pl.getX(); x <= pl.getRight(); x += 1.5f)
        {
            const float y = yOf (juce::jlimit (-22.0, 22.0, cv.total (hzOf (x))));
            if (pts++ == 0) tot.startNewSubPath (x, y); else tot.lineTo (x, y);
        }
        lastCurvePixels = pts;
        g.setColour (kInk.withAlpha (0.25f));
        g.strokePath (tot, juce::PathStrokeType (5.0f));
        g.setColour (kInk);
        g.strokePath (tot, juce::PathStrokeType (2.0f));
    }

    //  ---- the nodes ------------------------------------------------------------------
    for (int i = 5; i >= 0; --i)
    {
        if (i == 4 && p.hpfs < 0.5f) continue;
        if (i == 5 && p.lpfs < 0.5f) continue;
        const auto np = nodePos (i, p);
        const bool on = (i == drag || i == hover);
        if (i < 4)
        {
            const auto c = kBand[i].withMultipliedAlpha (p.eq > 0.5f ? 1.0f : 0.45f);
            if (on)
            {
                //  where this band can go on THIS drum
                g.setColour (c.withAlpha (0.10f));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xOf (sd.band[i].lo), pl.getY(), xOf (sd.band[i].hi), pl.getBottom()));
            }
            g.setColour (c);
            g.fillEllipse (juce::Rectangle<float> (on ? 20.0f : 17.0f, on ? 20.0f : 17.0f).withCentre (np));
            g.setColour (juce::Colours::black);
            g.setFont (mono (12.0f));
            g.drawText (juce::String (i + 1), juce::Rectangle<float> (20, 20).withCentre (np).toNearestInt(), juce::Justification::centred);
            g.setColour (c);
            g.setFont (mono (11.0f));
            const float ty = np.y < pl.getY() + 26 ? np.y + 12 : np.y - 25;
            g.drawText (sd.band[i].label, juce::Rectangle<float> (np.x - 50, ty, 100, 13).toNearestInt(), juce::Justification::centred);
        }
        else
        {
            g.setColour (kFilt);
            g.fillRect (juce::Rectangle<float> (on ? 14.0f : 11.0f, on ? 14.0f : 11.0f).withCentre (np));
            g.setFont (mono (11.0f));
            g.drawText (i == 4 ? "HPF" : "LPF", juce::Rectangle<float> (np.x - 30, np.y - 22, 60, 13).toNearestInt(), juce::Justification::centred);
        }
    }

    //  ---- the readout --------------------------------------------------------------------
    g.setFont (mono (13.0f));
    g.setColour (kInk);
    g.drawText (juce::String (sd.name) + "   " + proc->currentName(), (int) b.getX() + 16, (int) b.getY() + 8, 520, 18, juce::Justification::left);
    g.setFont (mono (11.0f));
    juce::String info;
    if (hover >= 0 && hover < 4)
    {
        const float fr[4] = { p.b1f, p.b2f, p.b3f, p.b4f }, gn[4] = { p.b1g, p.b2g, p.b3g, p.b4g }, q[4] = { p.b1q, p.b2q, p.b3q, p.b4q };
        info = juce::String (sd.band[hover].label) + "  " + hzText (fr[hover]) + "  " + (gn[hover] > 0 ? "+" : "") + juce::String (gn[hover], 1)
             + " dB  Q " + juce::String (q[hover], 2) + "   (drag, wheel = width, double-click = flat)";
    }
    else
        info = "IN   OUT";
    g.setColour (kDim);
    g.drawText (info, (int) b.getRight() - 520, (int) b.getY() + 9, 500, 16, juce::Justification::right);
    if (hover < 0)
    {
        //  the legend's two swatches
        g.setColour (kDim.withAlpha (0.7f));
        g.fillRect ((float) b.getRight() - 84.0f, b.getY() + 16.0f, 12.0f, 2.0f);
        g.setColour (kHot.withAlpha (0.6f));
        g.fillRect ((float) b.getRight() - 46.0f, b.getY() + 13.0f, 12.0f, 7.0f);
    }
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    drag = hit (e.position);
    if (drag < 0) return;
    static const char* f[4] = { "b1f", "b2f", "b3f", "b4f" }, *gn[4] = { "b1g", "b2g", "b3g", "b4g" };
    dragId[0] = drag < 4 ? f[drag] : (drag == 4 ? "hpf" : "lpf");
    dragId[1] = drag < 4 ? gn[drag] : "";
    for (auto& id : dragId)
        if (id.isNotEmpty()) if (auto* prm = proc->apvts.getParameter (id)) prm->beginChangeGesture();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const auto& sd = de::source (proc->currentSource());
    double f = hzOf (e.position.x);
    if (drag < 4)
    {
        f = juce::jlimit ((double) sd.band[drag].lo, (double) sd.band[drag].hi, f);
        setParam (dragId[0].toRawUTF8(), (float) f);
        setParam (dragId[1].toRawUTF8(), (float) juce::jlimit (-18.0, 18.0, dbOf (e.position.y)));
    }
    else setParam (dragId[0].toRawUTF8(), (float) f);
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    for (auto& id : dragId)
        if (id.isNotEmpty()) if (auto* prm = proc->apvts.getParameter (id)) prm->endChangeGesture();
    dragId[0] = dragId[1] = {};
    drag = -1;
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h < 0 || h >= 4) return;
    static const char* gn[4] = { "b1g", "b2g", "b3g", "b4g" };
    setParam (gn[h], 0.0f);
    repaint();
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hit (e.position);
    if (h < 0 || h >= 4) return;
    static const char* q[4] = { "b1q", "b2q", "b3q", "b4q" };
    const auto p = proc->currentParams();
    const float cur[4] = { p.b1q, p.b2q, p.b3q, p.b4q };
    setParam (q[h], juce::jlimit (0.3f, 8.0f, cur[h] * std::pow (2.0f, w.deltaY * 1.5f)));
    repaint();
}

//==============================================================================
void Meters::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    machineart::drawPlate (g, b);
    auto area = b.reduced (14.0f, 14.0f);
    const float w = (area.getWidth() - 3 * 10.0f) / 4.0f;
    auto bar = [&] (int i, float frac, juce::Colour c, bool down, const char* label, const juce::String& val)
    {
        auto r = juce::Rectangle<float> (area.getX() + i * (w + 10.0f), area.getY(), w, area.getHeight());
        auto well = r.withTrimmedBottom (34.0f);
        g.setColour (kBack);
        g.fillRoundedRectangle (well, 3.0f);
        const float hgt = juce::jlimit (0.0f, 1.0f, frac) * well.getHeight();
        g.setColour (c);
        if (down) g.fillRoundedRectangle (well.withHeight (hgt), 3.0f);
        else      g.fillRoundedRectangle (well.withTop (well.getBottom() - hgt), 3.0f);
        g.setColour (kDim);
        g.setFont (mono (11.0f));
        g.drawText (label, r.withTop (well.getBottom() + 3).withHeight (13).toNearestInt(), juce::Justification::centred);
        g.setColour (kInk);
        g.drawText (val, r.withTop (well.getBottom() + 17).withHeight (13).toNearestInt(), juce::Justification::centred);
    };
    auto pk = [] (float lin) { return 20.0f * std::log10 (juce::jmax (1.0e-5f, lin)); };
    const float inDb = pk (in), outDb = pk (out);
    bar (0, (inDb + 60.0f) / 66.0f, kIo, false, "IN", in > 1.0e-4f ? juce::String (inDb, 0) : juce::String ("-"));
    bar (1, -gate / 40.0f, kGate, true, "GATE", gate < -0.5f ? juce::String (gate, 0) : juce::String ("open"));
    bar (2, -gr / 24.0f, kComp, true, "GR", gr < -0.05f ? juce::String (gr, 1) : juce::String ("0"));
    bar (3, (outDb + 60.0f) / 66.0f, out > 1.0f ? juce::Colour (0xffff4a3a) : kHot, false, "OUT",
         out > 1.0e-4f ? juce::String (outDb, 0) : juce::String ("-"));
}

//==============================================================================
DrumEQEditor::DrumEQEditor (DrumEQProcessor& p)
    : AudioProcessorEditor (&p), host (p), v (&p), eq (p)
{
    setLookAndFeel (&look);
    setSize (kW, kH);

    //  ---- the source row -------------------------------------------------------
    for (int s = 0; s < de::kNumSources; ++s)
    {
        auto& b = srcBtn[s];
        b.setButtonText (de::source (s).chip);
        b.setColour (juce::TextButton::buttonOnColourId, kHot);
        b.setTooltip (juce::String ("This track is the ") + de::source (s).name
                      + ". The bands, their ranges and the presets follow the drum. A lamp above a drum means a track"
                        " in this session is on it. On a MASTER panel this row picks the drum to EDIT; HOME is this track's own.");
        b.onClick = [this, s]
        {
            if (host.isMaster()) host.setTarget (s);
            else host.selectSource (s);
            checkView();
            syncSource();
        };
        addAndMakeVisible (b);
    }

    //  ---- presets and kit ------------------------------------------------------------
    presetBox.setTooltip ("This drum's presets: the factory ones for the drum, then your own.");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) pv().loadFactoryPreset (id - 1);
        else
        {
            const auto files = pv().userPresets();
            if (id - 1001 < files.size()) pv().loadUserPreset (files[id - 1001]);
        }
        shownPreset.clear();
    };
    addAndMakeVisible (presetBox);
    for (auto* b : { &prevBtn, &nextBtn, &savePresetBtn, &kitBtn, &saveKitBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, kHot);
        addAndMakeVisible (*b);
    }
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    savePresetBtn.setTooltip ("Save this drum's settings as a preset of your own (Documents\\Brokild patches\\DrumEQ\\<drum>).");
    savePresetBtn.onClick = [this] { savePreset(); };
    kitBtn.setTooltip ("Load a whole kit onto every linked DrumEQ in the session: a factory kit, or one you saved.");
    kitBtn.onClick = [this] { showKitMenu(); };
    saveKitBtn.setTooltip ("Save EVERY drum in the session - every track's DrumEQ - as one kit file. Load it later onto all of them at once.");
    saveKitBtn.onClick = [this] { saveKit(); };
    linkBtn.setClickingTogglesState (true);
    linkBtn.setColour (juce::TextButton::buttonOnColourId, kHot);
    linkBtn.setToggleState (host.linked(), juce::dontSendNotification);
    linkBtn.setTooltip ("LINK on: this track follows kit loads. Off: it keeps its own settings, whatever kit is loaded.");
    linkBtn.onClick = [this] { host.setLinked (linkBtn.getToggleState()); checkView(); refreshKit(); repaint(); };
    addAndMakeVisible (linkBtn);
    masterBtn.setClickingTogglesState (true);
    masterBtn.setColour (juce::TextButton::buttonOnColourId, kMaster);
    masterBtn.setTooltip ("MASTER: tune the WHOLE KIT from this one panel. The source row then picks the drum to edit; every LINKED"
                          " track on that drum follows. This track keeps playing its own drum (HOME). Needs LINK. One master per session.");
    masterBtn.onClick = [this]
    {
        const bool want = masterBtn.getToggleState();
        if (! host.setMaster (want))
            setStatus ("MASTER needs LINK on (and the shared kit)", juce::Colour (0xffff6a5a));
        else if (want)
            setStatus ("MASTER - pick a drum on the source row to edit it; HOME is this track", kMaster);
        masterBtn.setToggleState (host.isMaster(), juce::dontSendNotification);
        checkView();
        syncSource();
        refreshKit();
        repaint();
    };
    addAndMakeVisible (masterBtn);
    keepBtn.setClickingTogglesState (true);
    keepBtn.setColour (juce::TextButton::buttonOnColourId, kBand[0]);
    keepBtn.setTooltip ("KEEP TRACK LEVEL: when FIND sets the input, OUT gives the same amount back, so the track's fader"
                        " balance does not move. Off: every track comes out at the same level and you balance on the faders.");
    keepBtn.onClick = [this] { pv().setKeepLevel (keepBtn.getToggleState()); };
    addAndMakeVisible (keepBtn);
    kitLabel.setFont (mono (13.0f));
    kitLabel.setColour (juce::Label::textColourId, kInk);
    kitLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (kitLabel);

    //  ---- the display and the meters ------------------------------------------------
    addAndMakeVisible (eq);
    addAndMakeVisible (meters);
    findBtn.setColour (juce::TextButton::buttonOnColourId, kBand[0]);
    findBtn.setTooltip ("FIND DRUM: play the drum a few times. DrumEQ finds its fundamental and puts the body band on it.");
    findBtn.onClick = [this]
    {
        if (host.editingRemote())
        {
            //  a track on the drum being edited listens; the answer comes back through the kit
            if (remoteFinding) return;
            host.requestRemoteFind (keepBtn.getToggleState());
            remoteFinding = true;
            setStatus (juce::String ("LISTENING on the ") + de::source (host.target()).name + " track - play the song or the drum", kBand[0]);
            return;
        }
        if (host.remoteFindActive()) return;       // the master asked this track to listen; it finishes itself
        if (! pv().finding()) { pv().startFind(); setStatus ("LISTENING - play the song or the drum a few times", kBand[0]); }
        else
        {
            const auto r = pv().finishFind();
            const auto& sd = de::source (pv().currentSource());
            if (! r.heard())
                setStatus ("No drum heard - play a few clear hits, then press again", juce::Colour (0xffff6a5a));
            else
            {
                juce::String t;
                if (r.hz > 0.0) t << "FOUND " << juce::String (r.hz, 1) << " Hz (" << noteName (r.hz) << ") - " << sd.band[sd.findBand].label << " moved there.  ";
                t << "IN " << (r.inDelta >= 0 ? "+" : "") << juce::String (r.inDelta, 1) << " dB";
                if (r.clamped) t << " (at the end of its range)";
                if (r.outDelta != 0.0f) t << ", OUT " << (r.outDelta >= 0 ? "+" : "") << juce::String (r.outDelta, 1) << " dB";
                t << ".  Gate " << juce::String (r.gthr, 0) << " dB" << (r.spillHeard ? " (above the spill)" : "");
                t << "  - " << juce::String (r.hits) << (r.hits == 1 ? " hit" : " hits");
                setStatus (t, kBand[0]);
            }
        }
    };
    addAndMakeVisible (findBtn);
    statusLabel.setFont (mono (12.0f));
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    //  ---- the knobs -------------------------------------------------------------------
    addKnob ("in", "TRIM", kIo, true);
    addKnob ("hpf", "HPF", kFilt);
    addKnob ("lpf", "LPF", kFilt);
    const char* bandIds[4][3] = { { "b1f", "b1g", "b1q" }, { "b2f", "b2g", "b2q" }, { "b3f", "b3g", "b3q" }, { "b4f", "b4g", "b4q" } };
    for (int b = 0; b < 4; ++b)
    {
        addKnob (bandIds[b][0], "FREQ", kBand[b]);
        addKnob (bandIds[b][1], "GAIN", kBand[b], true);
        addKnob (bandIds[b][2], "Q", kBand[b]);
    }
    static const KnobDef gateKnobs[] = { { "gthr", "THRESH", nullptr }, { "grange", "RANGE", nullptr }, { "gatk", "ATTACK", nullptr },
                                         { "ghold", "HOLD", nullptr }, { "grel", "RELEASE", nullptr }, { "gkeylo", "KEY LO", nullptr },
                                         { "gkeyhi", "KEY HI", nullptr } };
    for (const auto& d : gateKnobs) addKnob (d.id, d.label, kGate);
    static const KnobDef compKnobs[] = { { "cthr", "THRESH", nullptr }, { "crat", "RATIO", nullptr }, { "catk", "ATTACK", nullptr },
                                         { "crel", "RELEASE", nullptr }, { "cknee", "KNEE", nullptr }, { "cmake", "MAKEUP", nullptr },
                                         { "cmix", "MIX", nullptr }, { "csc", "KEY HPF", nullptr } };
    for (const auto& d : compKnobs) addKnob (d.id, d.label, kComp, std::strcmp (d.id, "cmake") == 0);
    addKnob ("out", "TRIM", kIo, true);

    //  the tooltips for everything that is not a band
    const std::pair<const char*, const char*> tips[] = {
        { "in", "INPUT trim, before everything. Gain-stage the mic so the gate and the compressor see a sensible level." },
        { "hpf", "HIGH-PASS: everything below is removed. On close mics it takes the rumble and the kick's spill away." },
        { "lpf", "LOW-PASS: everything above is removed. Tames cymbal spill on a kick or tom mic." },
        { "gthr", "Gate THRESHOLD: the drum opens the gate above this level (measured through the KEY band)." },
        { "grange", "Gate RANGE: how far it turns down between hits. 10-20 dB sounds natural; 40+ is a hard gate." },
        { "gatk", "Gate ATTACK: the time to open fully. Keep it short or the stick is softened." },
        { "ghold", "Gate HOLD: how long it stays open after the hit falls under the threshold." },
        { "grel", "Gate RELEASE: the time to close fully. Long enough to keep the drum's ring." },
        { "gkeylo", "Gate KEY LO: the gate listens only above this. Set it under the drum's body, over the kick's spill." },
        { "gkeyhi", "Gate KEY HI: and only below this, so cymbal spill cannot open it." },
        { "cthr", "Compressor THRESHOLD: level above which it turns the drum down." },
        { "crat", "Compressor RATIO: 3-4:1 for punch, 8:1 and up to crush a room." },
        { "catk", "Compressor ATTACK: SLOW (10-30 ms) lets the stick through for punch; FAST flattens the hit." },
        { "crel", "Compressor RELEASE: how fast it lets go. With AUTO REL it lengthens when the drum keeps it working." },
        { "cknee", "Compressor KNEE: a soft knee eases into compression around the threshold." },
        { "cmake", "Compressor MAKEUP: gain after compression, on the compressed path." },
        { "cmix", "Compressor MIX: parallel ('New York') compression - blend the crushed drum under the dry one." },
        { "csc", "Compressor KEY HPF: the compressor ignores the lows below this, so the kick's boom does not pump it. Fully left = OFF." },
        { "out", "OUTPUT trim, after everything." },
    };
    for (const auto& t : tips) if (auto* k = knobFor (t.first)) { k->slider.setTooltip (t.second); k->label.setTooltip (t.second); }

    addToggle (polBtn, "pol", kIo, "POLARITY: flip the mic. Snare bottom against snare top, kick in against kick out: listen for the fuller one.");
    addToggle (eqBtn, "eq", kHot, "EQ in or out, to compare.");
    addToggle (gateBtn, "gate", kGate, "The gate on or off. On toms and kicks it takes the spill out between hits.");
    addToggle (compBtn, "comp", kComp, "The compressor on or off.");
    addToggle (autoBtn, "cauto", kComp, "AUTO RELEASE: release follows the drum - fast after single hits, slower under a roll.");

    for (auto* b : { &hpSlope, &lpSlope, &b1Type, &b4Type })
    {
        b->setColour (juce::TextButton::buttonOnColourId, kFilt);
        addAndMakeVisible (*b);
    }
    hpSlope.setTooltip ("High-pass slope: OFF, 12 or 24 dB per octave.");
    lpSlope.setTooltip ("Low-pass slope: OFF, 12 or 24 dB per octave.");
    b1Type.setTooltip ("Band 1 as a SHELF (everything below) or a BELL (around the frequency).");
    b4Type.setTooltip ("Band 4 as a SHELF (everything above) or a BELL (around the frequency).");
    hpSlope.onClick = [this] { cycleSlope ("hpfs"); };
    lpSlope.onClick = [this] { cycleSlope ("lpfs"); };
    b1Type.onClick = [this] { toggleType ("b1t"); };
    b4Type.onClick = [this] { toggleType ("b4t"); };

    for (int i = 0; i < de::kNumParams; ++i) pv().apvts.addParameterListener (de::specs()[i].id, this);
    keepBtn.setToggleState (pv().keepLevel(), juce::dontSendNotification);
    masterBtn.setToggleState (host.isMaster(), juce::dontSendNotification);

    checkView();
    syncSource();
    refreshPresetBox();
    refreshKit();
    resized();
    startTimerHz (30);
}

DrumEQEditor::~DrumEQEditor()
{
    for (int i = 0; i < de::kNumParams; ++i) pv().apvts.removeParameterListener (de::specs()[i].id, this);
    for (auto& k : knobs) { k->attach.reset(); k->rangeAttach.reset(); }
    btnAttach.clear();
    setLookAndFeel (nullptr);
}

DrumEQEditor::Knob& DrumEQEditor::addKnob (const char* id, const char* label, juce::Colour c, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    k->id = id;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 16);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    k->slider.getProperties().set ("bipolar", bipolar);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setFont (mono (11.5f));
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, c.interpolatedWith (kInk, 0.35f));
    k->label.setInterceptsMouseClicks (false, false);

    const juce::String sid (id);
    if (sid.length() == 3 && sid[0] == 'b' && sid[2] == 'f')
    {
        //  a band frequency: its knob covers only this drum's range for it
        //  (syncSource sets it), so it is bound by hand rather than by the
        //  host parameter's full 20 Hz - 20 kHz
        auto* kp = k.get();
        k->slider.textFromValueFunction = [] (double v) { return hzText (v); };
        k->slider.onValueChange = [kp] { if (kp->rangeAttach) kp->rangeAttach->setValueAsPartOfGesture ((float) kp->slider.getValue()); };
        k->slider.onDragStart = [kp] { if (kp->rangeAttach) kp->rangeAttach->beginGesture(); };
        k->slider.onDragEnd = [kp] { if (kp->rangeAttach) kp->rangeAttach->endGesture(); };
        k->slider.setNormalisableRange (juce::NormalisableRange<double> (20.0, 20000.0,
            [] (double s, double e, double n) { return s * std::pow (e / s, n); },
            [] (double s, double e, double v) { return std::log (v / s) / std::log (e / s); }));
        k->slider.setDoubleClickReturnValue (false, 0.0);
    }
    attachKnob (*k);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void DrumEQEditor::attachKnob (Knob& k)
{
    //  bound to whichever processor the panel shows now
    k.attach.reset();
    k.rangeAttach.reset();
    const juce::String sid (k.id);
    if (sid.length() == 3 && sid[0] == 'b' && sid[2] == 'f')
    {
        auto* kp = &k;
        k.rangeAttach = std::make_unique<juce::ParameterAttachment> (*pv().apvts.getParameter (sid),
            [kp] (float val) { kp->slider.setValue (val, juce::dontSendNotification); }, nullptr);
        k.rangeAttach->sendInitialUpdate();
    }
    else
    {
        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (pv().apvts, sid, k.slider);
        const int si = de::indexOf (sid.toRawUTF8());
        if (si >= 0) k.slider.setDoubleClickReturnValue (true, de::specs()[si].def);
    }
}

void DrumEQEditor::attachButtons()
{
    btnAttach.clear();
    const std::pair<juce::TextButton*, const char*> bs[] = { { &polBtn, "pol" }, { &eqBtn, "eq" }, { &gateBtn, "gate" },
                                                             { &compBtn, "comp" }, { &autoBtn, "cauto" } };
    for (const auto& b : bs)
        btnAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (pv().apvts, b.second, *b.first));
}

void DrumEQEditor::checkView()
{
    auto* want = &host.view();
    masterLook = host.isMaster();
    masterBtn.setToggleState (masterLook, juce::dontSendNotification);
    masterBtn.setEnabled (host.linked() && host.kitOpen());
    if (want == v) return;
    for (int i = 0; i < de::kNumParams; ++i) v->apvts.removeParameterListener (de::specs()[i].id, this);
    v = want;
    for (int i = 0; i < de::kNumParams; ++i) v->apvts.addParameterListener (de::specs()[i].id, this);
    for (auto& k : knobs) attachKnob (*k);
    attachButtons();
    eq.setProcessor (*v);
    keepBtn.setToggleState (v->keepLevel(), juce::dontSendNotification);
    shownSource = -1;
    paramsMoved = true;
    syncSource();
    updateSwitches();
    repaint();
}

void DrumEQEditor::updateSpectra()
{
    if (! host.editingRemote()) { eq.note.clear(); eq.updateSpectra(); return; }
    float pre[drumkit::kSpec], post[drumkit::kSpec], m[4], rate = 48000.0f;
    if (host.remoteView (pre, post, m, rate))
    {
        eq.note.clear();
        eq.setLogSpectra (pre, post, drumkit::kSpec, rate);
    }
    else
    {
        eq.note = juce::String ("no track on the ") + de::source (host.target()).name + " right now - the settings wait in the kit";
        eq.setLogSpectra (nullptr, nullptr, 0, rate);
    }
}

DrumEQEditor::Knob* DrumEQEditor::knobFor (const juce::String& id)
{
    for (auto& k : knobs) if (k->id == id) return k.get();
    return nullptr;
}

juce::TextButton& DrumEQEditor::addToggle (juce::TextButton& b, const char* id, juce::Colour c, const juce::String& tip)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonOnColourId, c);
    b.setTooltip (tip);
    addAndMakeVisible (b);
    btnAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (pv().apvts, id, b));
    return b;
}

//==============================================================================
void DrumEQEditor::syncSource()
{
    const int s = pv().currentSource();
    const auto& sd = de::source (s);
    for (int i = 0; i < de::kNumSources; ++i)
    {
        srcBtn[i].setToggleState (i == s, juce::dontSendNotification);
        srcBtn[i].setColour (juce::TextButton::buttonOnColourId, masterLook ? kMaster : kHot);
    }
    static const char* ids[4] = { "b1f", "b2f", "b3f", "b4f" }, *gids[4] = { "b1g", "b2g", "b3g", "b4g" }, *qids[4] = { "b1q", "b2q", "b3q", "b4q" };
    for (int b = 0; b < 4; ++b)
    {
        if (auto* k = knobFor (ids[b]))
        {
            const double lo = sd.band[b].lo, hi = sd.band[b].hi;
            k->slider.setNormalisableRange (juce::NormalisableRange<double> (lo, hi,
                [] (double a, double e, double n) { return a * std::pow (e / a, n); },
                [] (double a, double e, double v) { return std::log (v / a) / std::log (e / a); }));
            k->rangeAttach->sendInitialUpdate();
            const juce::String tip = bandTip (sd.band[b].label) + "  (" + hzText (lo) + " - " + hzText (hi) + " on the " + sd.name + ")";
            k->slider.setTooltip (tip);
        }
        if (auto* k = knobFor (gids[b])) k->slider.setTooltip (bandTip (sd.band[b].label));
        if (auto* k = knobFor (qids[b])) k->slider.setTooltip ("Q: the band's width. Higher is narrower - narrow to cut a ring, wide to shape.");
    }
    findBtn.setButtonText (sd.findHi > 0.0f ? "FIND DRUM" : "SET LEVEL");
    const juce::String levels = " It also sets the INPUT so the hits arrive at "
                                + juce::String ((int) DrumEQProcessor::kTargetPeakDb) + " dBFS (the level the presets are"
                                " written for) and the gate threshold between the spill and the quietest hit.";
    findBtn.setTooltip (sd.findHi > 0.0f
        ? juce::String ("FIND DRUM: play the song or the drum, press again (it stops itself after 6 s). DrumEQ finds the drum's"
                        " fundamental (") + hzText (sd.findLo) + " - " + hzText (sd.findHi) + ") and puts " + sd.band[sd.findBand].label + " on it." + levels
        : juce::String ("SET LEVEL: play the song or the drum, press again (it stops itself after 6 s).") + levels);
    shownSource = s;
    refreshPresetBox();
    repaint();
}

void DrumEQEditor::refreshPresetBox()
{
    const int s = pv().currentSource();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading (juce::String ("FACTORY - ") + de::source (s).name);
    for (int i = 0; i < de::numPresets (s); ++i) presetBox.addItem (de::preset (s, i).name, i + 1);
    const auto files = pv().userPresets();
    if (! files.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("YOURS");
        for (int i = 0; i < files.size(); ++i) presetBox.addItem (files[i].getFileNameWithoutExtension(), 1001 + i);
    }
    presetBox.setText (pv().currentName(), juce::dontSendNotification);
    shownPreset = pv().currentName();
}

void DrumEQEditor::stepPreset (int delta)
{
    const int s = pv().currentSource();
    const int nf = de::numPresets (s);
    const auto files = pv().userPresets();
    const int total = nf + files.size();
    int cur = -1;
    for (int i = 0; i < nf; ++i) if (pv().currentName() == de::preset (s, i).name) cur = i;
    for (int i = 0; i < files.size(); ++i) if (pv().currentName() == files[i].getFileNameWithoutExtension()) cur = nf + i;
    const int next = ((cur < 0 ? (delta > 0 ? -1 : 0) : cur) + delta + total) % total;
    if (next < nf) pv().loadFactoryPreset (next);
    else pv().loadUserPreset (files[next - nf]);
    refreshPresetBox();
}

void DrumEQEditor::refreshKit()
{
    host.kitPresence (presence);
    int live = 0; for (int c : presence) if (c > 0) ++live;
    const auto name = host.kitName().isEmpty() ? juce::String ("no kit loaded yet") : host.kitName();
    juce::String t = "KIT  " + name + "   " + juce::String (live) + " of " + juce::String (de::kNumSources) + " drums on tracks";
    if (! host.linked()) t = "KIT  " + name + "   UNLINKED: this track keeps its own";
    int ms = -1, mt = -1;
    if (host.isMaster())
    {
        const int tg = host.target(), n = presence[tg];
        t = host.editingRemote()
              ? juce::String ("MASTER  editing ") + de::source (tg).name + "  -  " + (n == 0 ? juce::String ("no track yet") : juce::String (n) + (n == 1 ? " track" : " tracks"))
              : juce::String ("MASTER  HOME: this track's ") + de::source (tg).name;
    }
    else if (host.linked() && host.masterElsewhere (ms, mt) && ms >= 0 && ms < de::kNumSources)
        t = "KIT  " + name + "   MASTER: the " + de::source (ms).name + " track"
            + ((mt == pv().currentSource()) ? juce::String (" (editing this drum)") : juce::String());
    if (! host.kitOpen()) t = "KIT  this drum only (no shared kit on this system)";
    kitLabel.setText (t, juce::dontSendNotification);
    shownKit = name;
    linkBtn.setToggleState (host.linked(), juce::dontSendNotification);
}

void DrumEQEditor::setStatus (const juce::String& s, juce::Colour c)
{
    statusLabel.setText (s, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, c);
    statusUntil = juce::Time::getMillisecondCounter() + 9000;
}

void DrumEQEditor::cycleSlope (const char* id)
{
    if (auto* prm = pv().apvts.getParameter (id))
    {
        const int cur = juce::roundToInt (prm->convertFrom0to1 (prm->getValue()));
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((cur + 1) % 3)));
        prm->endChangeGesture();
    }
}

void DrumEQEditor::toggleType (const char* id)
{
    if (auto* prm = pv().apvts.getParameter (id))
    {
        const int cur = juce::roundToInt (prm->convertFrom0to1 (prm->getValue()));
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (1 - cur)));
        prm->endChangeGesture();
    }
}

void DrumEQEditor::savePreset()
{
    const auto dir = pv().presetFolder();
    chooser = std::make_unique<juce::FileChooser> (juce::String ("Save a ") + de::source (pv().currentSource()).name + " preset",
                                                   dir.getChildFile ("My " + juce::String (de::source (pv().currentSource()).name).toLowerCase() + ".json"),
                                                   "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("json")) f = f.withFileExtension ("json");
            if (pv().saveUserPreset (f, f.getFileNameWithoutExtension()))
                setStatus ("Saved preset " + f.getFileNameWithoutExtension(), kHot);
            refreshPresetBox();
        });
}

void DrumEQEditor::saveKit()
{
    const auto dir = host.kitFolder();
    const juce::String base = host.kitName().isNotEmpty() ? host.kitName() : juce::String ("My kit");
    chooser = std::make_unique<juce::FileChooser> ("Save the whole kit - every drum in the session",
                                                   dir.getChildFile (juce::File::createLegalFileName (base) + ".drumkit"), "*.drumkit");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("drumkit")) f = f.withFileExtension ("drumkit");
            juce::String report;
            if (host.saveKit (f, f.getFileNameWithoutExtension(), report))
                setStatus ("Saved kit " + f.getFileNameWithoutExtension() + " - " + report, kHot);
            else
                setStatus (report, juce::Colour (0xffff6a5a));
            refreshKit();
        });
}

void DrumEQEditor::showKitMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("FACTORY KITS");
    for (int i = 0; i < de::numKits(); ++i) m.addItem (1 + i, de::kit (i).name);
    const auto files = host.userKits();
    m.addSectionHeader ("YOUR KITS");
    if (files.isEmpty()) m.addItem (999, "(none saved yet - use SAVE KIT)", false);
    for (int i = 0; i < files.size(); ++i) m.addItem (1001 + i, files[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addItem (2000, "Open a kit file...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&kitBtn),
        [this, files] (int r)
        {
            juce::String report;
            if (r >= 1 && r <= de::numKits())
            {
                host.loadFactoryKit (r - 1, report);
                setStatus ("Loaded " + juce::String (de::kit (r - 1).name) + " onto every linked track (" + report + ")", kHot);
            }
            else if (r >= 1001 && r - 1001 < files.size())
            {
                const bool ok = host.loadKitFile (files[r - 1001], report);
                setStatus (ok ? "Loaded " + files[r - 1001].getFileNameWithoutExtension() + " - " + report : report,
                           ok ? kHot : juce::Colour (0xffff6a5a));
            }
            else if (r == 2000)
            {
                chooser = std::make_unique<juce::FileChooser> ("Open a DrumEQ kit", host.kitFolder(), "*.drumkit");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc)
                    {
                        const auto f = fc.getResult();
                        if (f == juce::File()) return;
                        juce::String rep;
                        const bool ok = host.loadKitFile (f, rep);
                        setStatus (ok ? "Loaded " + f.getFileNameWithoutExtension() + " - " + rep : rep, ok ? kHot : juce::Colour (0xffff6a5a));
                        refreshKit();
                    });
            }
            refreshKit();
            refreshPresetBox();
        });
}

void DrumEQEditor::updateSwitches()
{
    const auto p = pv().currentParams();
    //  FIND on a drum the master is editing needs a track on it to listen
    findBtn.setEnabled (! host.editingRemote() || presence[host.target()] > 0);
    static const char* sl[3] = { "OFF", "12 dB", "24 dB" };
    hpSlope.setButtonText (sl[juce::jlimit (0, 2, juce::roundToInt (p.hpfs))]);
    lpSlope.setButtonText (sl[juce::jlimit (0, 2, juce::roundToInt (p.lpfs))]);
    hpSlope.setToggleState (p.hpfs > 0.5f, juce::dontSendNotification);
    lpSlope.setToggleState (p.lpfs > 0.5f, juce::dontSendNotification);
    b1Type.setButtonText (p.b1t > 0.5f ? "BELL" : "SHELF");
    b4Type.setButtonText (p.b4t > 0.5f ? "BELL" : "SHELF");
    if (auto* k = knobFor ("hpf")) k->slider.setEnabled (p.hpfs > 0.5f);
    if (auto* k = knobFor ("lpf")) k->slider.setEnabled (p.lpfs > 0.5f);
    for (const char* id : { "gthr", "grange", "gatk", "ghold", "grel", "gkeylo", "gkeyhi" })
        if (auto* k = knobFor (id)) k->slider.setEnabled (p.gate > 0.5f);
    for (const char* id : { "cthr", "crat", "catk", "crel", "cknee", "cmake", "cmix", "csc" })
        if (auto* k = knobFor (id)) k->slider.setEnabled (p.comp > 0.5f);
}

//==============================================================================
void DrumEQEditor::parameterChanged (const juce::String&, float) { paramsMoved = true; }

void DrumEQEditor::timerCallback()
{
    checkView();
    float mi = 0, mo = 0, mg = 0, mt = 0;
    if (host.editingRemote())
    {
        float pre[drumkit::kSpec], post[drumkit::kSpec], m[4] = { 0, 0, 0, 0 }, rate = 0;
        if (host.remoteView (pre, post, m, rate)) { mi = m[0]; mo = m[1]; mg = m[2]; mt = m[3]; }
    }
    else
    {
        auto& e = host.getEngine();
        mi = e.meterIn(); mo = e.meterOut(); mg = e.meterGrDb(); mt = e.meterGateDb();
    }
    meters.in = juce::jmax (mi, meters.in * 0.86f);
    meters.out = juce::jmax (mo, meters.out * 0.86f);
    meters.gr = juce::jmin (mg, meters.gr * 0.8f);
    meters.gate = mt;
    meters.repaint();
    updateSpectra();

    if (pv().currentSource() != shownSource) syncSource();
    if (pv().currentName() != shownPreset) refreshPresetBox();
    if (paramsMoved.exchange (false))
    {
        updateSwitches();
        eq.repaint();
    }

    //  FIND: listening shows the time, and it stops itself after six seconds
    const juce::String findIdle = de::source (pv().currentSource()).findHi > 0.0f ? "FIND DRUM" : "SET LEVEL";
    if (remoteFinding)
    {
        float sec = 0, hz = 0, din = 0; int hits = 0;
        const int st = host.remoteFindState (sec, hz, hits, din);
        if (st == 1 || st == 0)
        {
            findBtn.setToggleState (true, juce::dontSendNotification);
            findBtn.setButtonText ("LISTENING " + juce::String (sec, 1) + " s");
        }
        else
        {
            remoteFinding = false;
            const auto& sd = de::source (host.target());
            if (st == 3) setStatus (juce::String ("Nothing heard on the ") + sd.name + " track - play it, then press again", juce::Colour (0xffff6a5a));
            else
            {
                juce::String t;
                if (hz > 0.0f) t << "FOUND " << juce::String (hz, 1) << " Hz (" << noteName (hz) << ") - " << sd.band[sd.findBand].label << " moved there.  ";
                t << "IN " << (din >= 0 ? "+" : "") << juce::String (din, 1) << " dB on the " << sd.name << " track, gate set  - "
                  << juce::String (hits) << (hits == 1 ? " hit" : " hits");
                setStatus (t, kBand[0]);
            }
        }
    }
    else if (pv().finding() && ! host.editingRemote())
    {
        findBtn.setToggleState (true, juce::dontSendNotification);
        findBtn.setButtonText ((host.remoteFindActive() ? "MASTER: " : "LISTENING ") + juce::String (pv().findSeconds(), 1) + " s");
        if (pv().findSeconds() >= 6.0 && ! host.remoteFindActive()) findBtn.onClick();
    }
    else if (findBtn.getToggleState() || findBtn.getButtonText() != findIdle)
    {
        findBtn.setToggleState (false, juce::dontSendNotification);
        findBtn.setButtonText (findIdle);
    }

    static int tick = 0;
    if (++tick % 6 == 0)
    {
        int before[de::kNumSources]; std::memcpy (before, presence, sizeof (before));
        refreshKit();
        if (std::memcmp (before, presence, sizeof (before)) != 0) { repaint (0, 0, kW, 60); updateSwitches(); }
    }
    if (statusUntil != 0 && juce::Time::getMillisecondCounter() > statusUntil)
    {
        statusUntil = 0;
        statusLabel.setText ({}, juce::dontSendNotification);
    }
}

//==============================================================================
void DrumEQEditor::paint (juce::Graphics& g)
{
    machineart::drawGround (g, getLocalBounds().toFloat(), "ground-beet.jpg", 0.75f, 0.55f);
    if (masterLook)
    {
        //  the MASTER panel is blue: a wash over the ground and a frame round it
        g.setColour (kMaster.withAlpha (0.16f));
        g.fillAll();
        g.setColour (kMaster);
        g.drawRect (getLocalBounds(), 3);
    }

    //  the header: the name, and the lamps over the drums
    {
        auto head = juce::Rectangle<float> (0, 0, (float) kW, 58.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (head);
        g.setFont (stencil (32.0f));
        g.setColour (masterLook ? kMaster : kHot);
        g.drawText ("DRUMEQ", 18, 2, 170, 38, juce::Justification::left);
        g.setFont (mono (10.0f));
        g.setColour (kDim);
        g.drawText (masterLook ? juce::String ("MASTER  ") + DE_BUILD_ID : juce::String ("ACOUSTIC DRUMS  ") + DE_BUILD_ID,
                    18, 40, 176, 12, juce::Justification::left);
        if (masterLook)
        {
            //  HOME: the drum this track plays, whatever the panel is editing
            const auto r = srcBtn[host.currentSource()].getBounds().toFloat();
            g.setColour (kInk);
            g.setFont (mono (9.5f));
            g.drawText ("HOME", r.withY (r.getBottom() + 1.0f).withHeight (10.0f).toNearestInt(), juce::Justification::centred);
        }
        for (int s = 0; s < de::kNumSources; ++s)
        {
            const auto r = srcBtn[s].getBounds().toFloat();
            const int n = presence[s];
            const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getCentreX(), r.getY() - 7.0f });
            g.setColour (n > 0 ? kHot : kFaint);
            g.fillEllipse (lamp);
            if (n > 1)
            {
                g.setColour (kHot);
                g.setFont (mono (10.0f));
                g.drawText (juce::String (n) + "x", (int) lamp.getRight() + 2, (int) lamp.getY() - 3, 22, 12, juce::Justification::left);
            }
        }
    }

    //  the bar
    g.setFont (mono (12.0f));
    g.setColour (kDim);
    g.drawText ("PRESET", 18, 66, 60, 26, juce::Justification::left);

    //  the sections
    for (const auto& s : sections)
    {
        machineart::drawPlate (g, s.r.toFloat());
        g.setColour (s.colour);
        g.setFont (mono (12.5f));
        g.drawText (s.title, s.r.getX() + 12, s.r.getY() + 6, 120, 16, juce::Justification::left);
    }

    //  the band names over their three knobs, in the drum's own words
    const auto& sd = de::source (pv().currentSource());
    static const char* fids[4] = { "b1f", "b2f", "b3f", "b4f" };
    for (int b = 0; b < 4; ++b)
        if (auto* k = knobFor (fids[b]))
        {
            const int x0 = k->slider.getX() - 2, y0 = k->label.getY() - 21;
            const bool typed = (b == 0 || b == 3);
            g.setColour (kBand[b]);
            g.setFont (mono (12.5f));
            g.drawText (juce::String (b + 1) + "  " + sd.band[b].label, x0 + 4, y0, typed ? 120 : 200, 16, juce::Justification::left);
            g.setColour (kBand[b].withAlpha (0.35f));
            g.fillRect (x0 + 4, y0 + 17, 3 * kCol - 10, 1);
        }
}

void DrumEQEditor::resized()
{
    sections.clear();

    //  ---- the source row ------------------------------------------------------
    {
        const int bw = 82, gap = 4, x0 = 200;
        for (int s = 0; s < de::kNumSources; ++s) srcBtn[s].setBounds (x0 + s * (bw + gap), 18, bw, 28);
    }

    //  ---- the bar ------------------------------------------------------------------
    presetBox.setBounds (78, 64, 230, 28);
    prevBtn.setBounds (312, 64, 28, 28);
    nextBtn.setBounds (342, 64, 28, 28);
    savePresetBtn.setBounds (374, 64, 64, 28);
    kitLabel.setBounds (452, 64, 444, 28);
    masterBtn.setBounds (900, 64, 84, 28);
    kitBtn.setBounds (988, 64, 84, 28);
    saveKitBtn.setBounds (1076, 64, 84, 28);
    linkBtn.setBounds (1164, 64, 60, 28);

    //  ---- the display, the meters --------------------------------------------------
    eq.setBounds (16, 100, 990, 286);
    meters.setBounds (1014, 100, 210, 228);
    findBtn.setBounds (1022, 334, 194, 28);
    keepBtn.setBounds (1022, 365, 194, 19);
    statusLabel.setBounds (16, 388, 1208, 16);

    //  ---- the knobs --------------------------------------------------------------------
    const int kh = 98;                         // the knob with its value box
    auto place = [&] (const char* id, int x, int y)
    {
        if (auto* k = knobFor (id))
        {
            k->label.setBounds (x, y, kCol, 14);
            k->slider.setBounds (x, y + 14, kCol, kh);
        }
    };
    auto section = [&] (const juce::String& title, juce::Colour c, int x, int y, int cols, int extraW = 0)
    {
        const juce::Rectangle<int> r (x, y, cols * kCol + 20 + extraW, 30 + 14 + kh + 8);
        sections.push_back ({ title, c, r });
        return r;
    };

    //  row 1: INPUT | FILTERS | EQ
    {
        const int y = 410;
        auto rIn = section ("IN", kIo, 16, y, 1);
        place ("in", rIn.getX() + 10, y + 30);
        polBtn.setBounds (rIn.getX() + 40, y + 5, 42, 20);

        auto rF = section ("FILTERS", kFilt, rIn.getRight() + 8, y, 2);
        place ("hpf", rF.getX() + 10, y + 30);
        place ("lpf", rF.getX() + 10 + kCol, y + 30);
        //  the slope buttons ride the knobs' label row, right of the name
        if (auto* k = knobFor ("hpf")) { k->label.setBounds (rF.getX() + 10, y + 30, 30, 14); hpSlope.setBounds (rF.getX() + 40, y + 28, 38, 16); }
        if (auto* k = knobFor ("lpf")) { k->label.setBounds (rF.getX() + 10 + kCol, y + 30, 30, 14); lpSlope.setBounds (rF.getX() + 40 + kCol, y + 28, 38, 16); }

        auto rE = section ("EQ", kHot, rF.getRight() + 8, y, 12, 4);
        eqBtn.setBounds (rE.getX() + 40, y + 5, 40, 20);
        const char* bandIds[4][3] = { { "b1f", "b1g", "b1q" }, { "b2f", "b2g", "b2q" }, { "b3f", "b3g", "b3q" }, { "b4f", "b4g", "b4q" } };
        for (int b = 0; b < 4; ++b)
            for (int k = 0; k < 3; ++k)
                place (bandIds[b][k], rE.getX() + 10 + (b * 3 + k) * kCol + (b > 0 ? 1 : 0) * 0, y + 44);
        //  the band names sit in the row above FREQ GAIN Q (painted); the two
        //  shapeable bands carry their SHELF/BELL switch there
        b1Type.setBounds (rE.getX() + 10 + 3 * kCol - 58, y + 25, 52, 17);
        b4Type.setBounds (rE.getX() + 10 + 12 * kCol - 58, y + 25, 52, 17);
        //  the EQ section is a row taller for the band names: shift everything below
        sections.back().r.setHeight (sections.back().r.getHeight() + 14);
        rIn.setHeight (rIn.getHeight() + 14); sections[0].r = rIn;
        rF.setHeight (rF.getHeight() + 14); sections[1].r = rF;
    }

    //  row 2: GATE | COMP | OUTPUT
    {
        const int y = 582;
        auto rG = section ("GATE", kGate, 16, y, 7);
        gateBtn.setBounds (rG.getX() + 60, y + 5, 40, 20);
        const char* g[7] = { "gthr", "grange", "gatk", "ghold", "grel", "gkeylo", "gkeyhi" };
        for (int i = 0; i < 7; ++i) place (g[i], rG.getX() + 10 + i * kCol, y + 30);

        auto rC = section ("COMPRESSOR", kComp, rG.getRight() + 8, y, 8);
        compBtn.setBounds (rC.getX() + 112, y + 5, 40, 20);
        autoBtn.setBounds (rC.getX() + 158, y + 5, 78, 20);
        const char* c[8] = { "cthr", "crat", "catk", "crel", "cknee", "cmake", "cmix", "csc" };
        for (int i = 0; i < 8; ++i) place (c[i], rC.getX() + 10 + i * kCol, y + 30);

        auto rO = section ("OUT", kIo, rC.getRight() + 8, y, 1, 0);
        place ("out", rO.getX() + 10, y + 30);
    }
}
