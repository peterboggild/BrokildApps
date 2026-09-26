#include "BeetEditor.h"
#include "BeetWebData.h"
#include "MachineArtData.h"
#if JUCE_WINDOWS
 #include <WebView2.h>
#endif

#ifndef BEET_BUILD_ID
 #define BEET_BUILD_ID "0.0.0"
#endif

using namespace beetcol;
using namespace beetui;

namespace
{
    juce::String pid (int s, const char* what) { return "s" + juce::String (s + 1) + "_" + what; }

    void tiny (juce::TextButton& b) { b.getProperties().set ("role", "tiny"); }
}

//==============================================================================
//  SLOT CARD
//==============================================================================
SlotCard::SlotCard (BeetProcessor& proc, int slot) : p (proc), s (slot)
{
    type.addItemList ({ "EMPTY", "KICKSTART", "SNARE TACTICS", "HATS OFF" }, 1);
    type.setTooltip ("What this slot is: empty (costs nothing), Kickstart, Snare Tactics or Hats Off.");
    type.onChange = [this] {
        const int t = type.getSelectedId() - 1;
        if (t >= 0 && t != p.slotType (s))
        {
            p.setSlotType (s, t);
            if (onTypeChanged) onTypeChanged (s);
            refresh();
        }
    };
    addAndMakeVisible (type);

    preset.setTooltip ("This slot's sound: the drum's own factory presets. Edit it fully in the panel below.");
    preset.onChange = [this] {
        const int i = preset.getSelectedId() - 1;
        if (i >= 0 && i != p.slotPreset (s)) p.setSlotPreset (s, i);
    };
    addAndMakeVisible (preset);

    for (auto* b : { &prev, &next })
    {
        tiny (*b);
        addAndMakeVisible (*b);
    }
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    prev.onClick = [this] { if (auto* d = p.slotProcessor (s)) p.setSlotPreset (s, juce::jmax (1, d->getCurrentProgram() - 1)); refresh(); };
    next.onClick = [this] { if (auto* d = p.slotProcessor (s)) p.setSlotPreset (s, juce::jmin (d->getNumPrograms() - 1, d->getCurrentProgram() + 1)); refresh(); };

    hit.getProperties().set ("role", "hit");
    hit.setTooltip ("Play this slot. Chokes act exactly as they do from MIDI.");
    hit.onClick = [this] { p.audition (s, 0.85f); if (onSelect) onSelect (s); };
    addAndMakeVisible (hit);

    for (auto* b : { &noteDown, &noteUp, &learn }) { tiny (*b); addAndMakeVisible (*b); }
    noteDown.setTooltip ("Note down a semitone");
    noteUp.setTooltip ("Note up a semitone");
    learn.setTooltip ("Learn: press, then play a key. That key triggers this slot.");
    noteDown.onClick = [this] { p.setSlotNote (s, p.slotNote (s) - 1); repaint(); };
    noteUp.onClick   = [this] { p.setSlotNote (s, p.slotNote (s) + 1); repaint(); };
    learn.onClick    = [this] { p.learnNote (p.learningSlot() == s ? -1 : s); };

    for (auto* k : { &level, &pan })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
        k->setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (*k);
    }
    level.setTooltip ("Slot level. Double-click for 0 dB.");
    pan.setTooltip ("Slot pan. Double-click to centre.");
    levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid (s, "level"), level);
    panAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid (s, "pan"), pan);
    level.setDoubleClickReturnValue (true, 0.0);
    pan.setDoubleClickReturnValue (true, 0.0);

    for (auto* b : { &mute, &solo }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    mute.setTooltip ("Mute this slot");
    solo.setTooltip ("Solo: while any slot is soloed, only soloed slots sound");
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid (s, "mute"), mute);
    solo.onClick = [this] { p.setSlotSolo (s, solo.getToggleState()); };

    for (int j = 0; j < beet::NUM_SLOTS; ++j)
    {
        auto& b = choke[(size_t) j];
        b.setButtonText (juce::String (j + 1));
        tiny (b);
        b.setClickingTogglesState (true);
        b.setTooltip (j == s ? "A slot cannot choke itself"
                             : "Lit: a hit on slot " + juce::String (j + 1) + " silences this slot (like a closed hat cutting an open one).");
        b.onClick = [this, j] {
            unsigned m = p.slotChokedBy (s);
            m = choke[(size_t) j].getToggleState() ? (m | (1u << j)) : (m & ~(1u << j));
            p.setSlotChokedBy (s, m);
        };
        addAndMakeVisible (b);
    }

    const char* outNames[] = { "MIX", "OWN", "BOTH" };
    const char* outTips[]  = { "Into the main stereo mix",
                               "Only to this slot's own output pair (taken out of the mix). Falls back to the mix while the host has that pair switched off.",
                               "To the mix AND this slot's own output pair" };
    for (int m = 0; m < beet::NUM_OUT_MODES; ++m)
    {
        auto& b = out[(size_t) m];
        b.setButtonText (outNames[m]);
        tiny (b);
        b.setTooltip (outTips[m]);
        b.onClick = [this, m] { p.setSlotOut (s, m); refreshOuts(); };
        addAndMakeVisible (b);
    }

    addMouseListener (this, true);     // a click anywhere on the card selects it
    refresh();
}

SlotCard::~SlotCard()
{
    removeMouseListener (this);
}

void SlotCard::mouseDown (const juce::MouseEvent&)
{
    if (onSelect) onSelect (s);
}

void SlotCard::setSelected (bool on)
{
    if (selected != on) { selected = on; repaint(); }
}

void SlotCard::rebuildPresets()
{
    preset.clear (juce::dontSendNotification);
    if (auto* d = p.slotProcessor (s))
        for (int i = 0; i < d->getNumPrograms(); ++i)
            preset.addItem (d->getProgramName (i), i + 1);
}

void SlotCard::refreshChokes()
{
    const unsigned m = p.slotChokedBy (s);
    for (int j = 0; j < beet::NUM_SLOTS; ++j)
    {
        choke[(size_t) j].setToggleState ((m >> j) & 1u, juce::dontSendNotification);
        choke[(size_t) j].setEnabled (j != s && p.slotType (s) != beet::EMPTY);
    }
}

void SlotCard::refreshOuts()
{
    const int mode = p.slotOut (s);
    for (int m = 0; m < beet::NUM_OUT_MODES; ++m)
    {
        out[(size_t) m].setToggleState (m == mode, juce::dontSendNotification);
        out[(size_t) m].setEnabled (p.slotType (s) != beet::EMPTY);
    }
    repaint();
}

void SlotCard::refresh()
{
    const int t = p.slotType (s);
    type.setSelectedId (t + 1, juce::dontSendNotification);
    hit.getProperties().set ("drum", t);                   // the pad is drawn in this drum's colours
    if (t != shownType) { rebuildPresets(); shownType = t; }

    const bool live = t != beet::EMPTY;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &preset, &prev, &next, &hit, &level, &pan, &mute, &solo, &noteDown, &noteUp, &learn })
        c->setEnabled (live);
    //  an empty bay shows nothing it cannot use: no preset, pad, knobs or M/S
    for (juce::Component* c : std::initializer_list<juce::Component*> { &preset, &prev, &next, &hit, &level, &pan, &mute, &solo })
        c->setVisible (live);
    solo.setToggleState (p.slotSolo (s), juce::dontSendNotification);
    refreshChokes();
    refreshOuts();
    tick();
    repaint();
}

void SlotCard::tick()
{
    //  the preset name as the drum sees it (a user patch loaded in the drum's
    //  own panel shows up here too)
    if (auto* d = p.slotProcessor (s))
    {
        const int cur = d->getCurrentProgram();
        const auto name = p.slotPresetName (s);
        if (preset.getSelectedId() != cur + 1 || preset.getText() != name)
        {
            preset.setSelectedId (cur + 1, juce::dontSendNotification);
            if (preset.getText() != name) preset.setText (name, juce::dontSendNotification);
        }
    }

    const int h = p.slotHits (s);
    if (h != lastHits) { lastHits = h; lamp = 1.0f; }
    else lamp *= 0.82f;

    const bool nowLearning = p.learningSlot() == s;
    learn.setToggleState (nowLearning, juce::dontSendNotification);
    learning = nowLearning;

    const int mode = p.slotOut (s);
    warnOut = mode != beet::OUT_MIX && ! p.slotOutputConnected (s) && p.slotType (s) != beet::EMPTY;
    repaint();
}

void SlotCard::resized()
{
    type.setBounds (38, 10, CARD_W - 46, 22);
    prev.setBounds (8, 38, 20, 22);
    preset.setBounds (31, 38, CARD_W - 62, 22);
    next.setBounds (CARD_W - 28, 38, 20, 22);

    hit.setBounds (6, 66, 62, 62);
    noteDown.setBounds (74, 104, 19, 20);
    noteUp.setBounds (95, 104, 19, 20);
    learn.setBounds (116, 104, 44, 20);

    level.setBounds (6, 130, 54, 54);
    pan.setBounds (62, 130, 54, 54);
    mute.setBounds (124, 136, 34, 22);
    solo.setBounds (124, 164, 34, 22);

    for (int j = 0; j < beet::NUM_SLOTS; ++j)
        choke[(size_t) j].setBounds (8 + j * 19, 210, 18, 16);
    for (int m = 0; m < beet::NUM_OUT_MODES; ++m)
        out[(size_t) m].setBounds (8 + m * 50, 230, 48, 16);
}

void SlotCard::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    BeetLook::drawBay (g, r, selected);

    //  the slot number, stencilled
    g.setFont (BeetLook::stencil (22.0f));
    g.setColour (selected ? amber : ink.withAlpha (0.85f));
    g.drawText (juce::String (s + 1), 8, 8, 28, 26, juce::Justification::centred, false);

    const bool live = p.slotType (s) != beet::EMPTY;

    //  the note, and the lamp that lights on every hit
    BeetLook::drawLamp (g, { 76.0f, 72.0f, 12.0f, 12.0f }, amber, live ? lamp : 0.0f);
    g.setFont (BeetLook::mono (20.0f));
    g.setColour (learning && (juce::Time::getMillisecondCounter() / 250) % 2 == 0 ? amber : ink);
    g.drawText (learning ? juce::String ("KEY?") : beet::noteName (p.slotNote (s)), 94, 68, CARD_W - 100, 26, juce::Justification::centredLeft, false);

    g.setFont (BeetLook::mono (10.0f));
    g.setColour (faint);
    if (live)
    {
        g.drawText ("LEVEL", 6, 182, 54, 12, juce::Justification::centred, false);
        g.drawText ("PAN", 62, 182, 54, 12, juce::Justification::centred, false);
    }
    g.drawText ("CHOKED BY", 8, 197, 120, 12, juce::Justification::centredLeft, false);

    if (warnOut)
    {
        g.setColour (amber);
        g.drawText ("OFF IN HOST", CARD_W - 76, 197, 70, 12, juce::Justification::centredRight, false);
    }

    if (! live)
    {
        g.setColour (ink.withAlpha (0.45f));
        g.setFont (BeetLook::mono (11.0f));
        g.drawFittedText ("empty bay\ncosts nothing", 8, 134, CARD_W - 16, 48, juce::Justification::centred, 2);
    }
}

//==============================================================================
//  EDITOR
//==============================================================================
BeetEditor::BeetEditor (BeetProcessor& proc)
    : juce::AudioProcessorEditor (&proc), p (proc)
{
    setLookAndFeel (&look);
    content.setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);

    for (int i = 0; i < beet::numKits(); ++i) kit.addItem (beet::kit (i).name, i + 1);
    kit.setTooltip ("Themed kits. Loading one changes the sounds, levels, pans and chokes, never your notes or output routing.");
    kit.onChange = [this] {
        const int k = kit.getSelectedId() - 1;
        if (k >= 0 && k != p.kitIndex()) p.loadKit (k);
    };
    content.addAndMakeVisible (kit);
    for (auto* b : { &kitPrev, &kitNext }) { b->getProperties().set ("role", "tiny"); content.addAndMakeVisible (*b); }
    kitPrev.setTooltip ("Previous kit");
    kitNext.setTooltip ("Next kit");
    kitPrev.onClick = [this] { p.loadKit ((p.kitIndex() + beet::numKits() - 1) % beet::numKits()); };
    kitNext.onClick = [this] { p.loadKit ((p.kitIndex() + 1) % beet::numKits()); };

    for (auto* b : { &mapC3, &mapGM }) { b->setClickingTogglesState (false); content.addAndMakeVisible (*b); }
    mapC3.setTooltip ("Notes C3 upwards: slot 1 on C3, slot 2 on C#3 ... slot 8 on G3");
    mapGM.setTooltip ("General MIDI drum notes: kicks 36/35, snares 38/40, closed hat 42, open hat 46, ride 51, crash 49");
    mapC3.onClick = [this] { p.setNoteMap (BeetProcessor::MAP_C3); refreshHeader(); };
    mapGM.onClick = [this] { p.setNoteMap (BeetProcessor::MAP_GM); refreshHeader(); };

    master.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    master.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    master.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    master.setPopupDisplayEnabled (true, true, nullptr);
    master.setDoubleClickReturnValue (true, 0.0);
    master.setTooltip ("Master level");
    master.getProperties().set ("red", true);        // the one dangerous knob is the red one
    masterAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "master", master);
    content.addAndMakeVisible (master);

    rackButton.getProperties().set ("role", "bwfx");
    rackButton.setClickingTogglesState (true);
    rackButton.setTooltip ("Brokild World FX: the effects rack on the main mix. Slots sent to their OWN outputs go round it.");
    rackButton.onClick = [this] { showRack (rackButton.getToggleState()); };
    content.addAndMakeVisible (rackButton);

    panic.getProperties().set ("role", "panic");
    panic.setTooltip ("Emergency stop: every slot fades out at once");
    panic.onClick = [this] { p.panic(); };
    content.addAndMakeVisible (panic);

    for (int s = 0; s < beet::NUM_SLOTS; ++s)
    {
        auto c = std::make_unique<SlotCard> (p, s);
        c->onSelect = [this] (int i) { selectSlot (i); };
        c->onTypeChanged = [this] (int i) { selectSlot (i); syncChild(); };
        c->setBounds (CARDS_X + s * (CARD_W + CARD_GAP), CARD_Y, CARD_W, CARD_H);
        content.addAndMakeVisible (*c);
        cards[(size_t) s] = std::move (c);
    }

    //  one logical size, scaled to the window; the aspect is fixed so the
    //  drum panels never distort
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) W / (double) H);
    setResizeLimits (W / 2, H / 2, W * 3 / 2, H * 3 / 2);
    setSize (W * 9 / 10, H * 9 / 10);

    selectSlot (0);
    refreshHeader();

    //  the web rack, if this machine has the WebView2 runtime (without it JUCE
    //  would fall back to the old Internet Explorer control - Thin Walls' lesson)
    bool haveWebView = useWebRack;
   #if JUCE_WINDOWS
    if (haveWebView)
    {
        LPWSTR ver = nullptr;
        const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString (nullptr, &ver);
        haveWebView = SUCCEEDED (hr) && ver != nullptr;
        if (ver != nullptr) CoTaskMemFree (ver);
    }
   #endif
    if (haveWebView)
    {
        rackWeb = std::make_unique<juce::WebBrowserComponent> (rackOptions());
        addAndMakeVisible (*rackWeb);
        parkRackWeb();
        rackWeb->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }
    startTimerHz (30);
}

//  THE BWFX RACK is the standard web rack - BrokildWorldFX/ui/bwfx-rack.js,
//  what Black Rider and every WebView synth show - in a WebView2 over the
//  whole window. Created with the editor and PARKED off to the side until
//  the button is pressed: no start-up wait on the first press, and a parked
//  browser still takes events (a hidden one would not - emitEventIfBrowser-
//  IsVisible tests isVisible()).
juce::WebBrowserComponent::Options BeetEditor::rackOptions()
{
    using BO = juce::WebBrowserComponent::Options;
    auto options = BO{}
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            auto make = [] (const char* data, int size, const char* mime)
            {
                juce::WebBrowserComponent::Resource r;
                r.data.resize ((size_t) size);
                std::memcpy (r.data.data(), data, (size_t) size);
                r.mimeType = mime;
                return r;
            };
            if (path == "/" || path == "/index.html") return make (BeetWebData::bwfxhost_html, BeetWebData::bwfxhost_htmlSize, "text/html");
            if (path == "/bwfx-rack.js")             return make (BeetWebData::bwfxrack_js, BeetWebData::bwfxrack_jsSize, "application/javascript");
            if (path == "/ground.jpg")
            {
                int n = 0;
                if (auto* d = MachineArtData::getNamedResource ("groundbeet_jpg", n)) return make (d, n, "image/jpeg");
            }
            return std::nullopt;
        })
        .withEventListener ("beet", [this] (juce::var m) { onRackMessage (m); });
   #if JUCE_WINDOWS
    //  its own profile, and the standalone another: WebView2 joins a RUNNING
    //  browser process per user-data folder (Black Rider's lesson)
    const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
    auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("Brokild").getChildFile ("Beetmachine")
                        .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
    userData.createDirectory();
    options = options.withBackend (BO::Backend::webview2)
                     .withWinWebView2Options (BO::WinWebView2{}
                         .withStatusBarDisabled()
                         .withBuiltInErrorPageDisabled()
                         .withBackgroundColour (juce::Colour (0xff1c2329))
                         .withUserDataFolder (userData));
   #endif
    return options;
}

void BeetEditor::onRackMessage (const juce::var& m)
{
    const auto k = m.getProperty ("k", {}).toString();
    if (k == "bwfx")
    {
        if (bwfx_juce::handleMessage (p.rack(), p.apvts, m) && rackWeb != nullptr)
            rackWeb->emitEventIfBrowserIsVisible ("bwfx", bwfx_juce::stateVar (p.rack()));
    }
    else if (k == "close")
    {
        juce::Component::SafePointer<BeetEditor> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->showRack (false); });
    }
}

void BeetEditor::parkRackWeb()
{
    if (rackWeb != nullptr) rackWeb->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));
}

void BeetEditor::showRack (bool show)
{
    if (rackWeb != nullptr)
    {
        rackShown = show;
        if (show)
        {
            rackWeb->setBounds (getLocalBounds());
            rackWeb->toFront (true);
            rackWeb->emitEventIfBrowserIsVisible ("open", juce::var());
            rackWeb->emitEventIfBrowserIsVisible ("bwfx", bwfx_juce::stateVar (p.rack()));
        }
        else parkRackWeb();
        if (rackButton.getToggleState() != show) rackButton.setToggleState (show, juce::dontSendNotification);
        return;
    }
    if (show && overlay == nullptr)
    {
        overlay = std::make_unique<BwfxPanel> (p.rack(), p.apvts);
        overlay->ground = BeetLook::art ("ground.jpg");         // the machine's own metal behind it
        overlay->onClose = [this] { rackButton.setToggleState (false, juce::sendNotificationSync); };
        addAndMakeVisible (*overlay);
        overlay->setBounds (getLocalBounds());
    }
    else if (show) overlay->refreshFromRack();
    if (overlay != nullptr)
    {
        overlay->setVisible (show);
        if (show) overlay->toFront (true);
    }
    if (rackButton.getToggleState() != show) rackButton.setToggleState (show, juce::dontSendNotification);
}

BeetEditor::~BeetEditor()
{
    stopTimer();
    rackWeb.reset();
    overlay.reset();
    releaseChild();                // the drum's editor goes before anything it points at
    p.collectGarbage();
    for (auto& c : cards) c.reset();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void BeetEditor::resized()
{
    const float k = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (k));

    kitPrev.setBounds (310, 16, 22, 26);
    kit.setBounds (334, 16, 190, 26);
    kitNext.setBounds (526, 16, 22, 26);
    mapC3.setBounds (622, 16, 44, 26);
    mapGM.setBounds (670, 16, 44, 26);
    master.setBounds (830, 4, 48, 48);
    rackButton.setBounds (920, 15, 96, 28);
    panic.setBounds (1310, 3, 50, 50);
    if (overlay != nullptr) overlay->setBounds (getLocalBounds());
    if (rackWeb != nullptr) { if (rackShown) rackWeb->setBounds (getLocalBounds()); else parkRackWeb(); }
}

void BeetEditor::paint (juce::Graphics& g)
{
    g.fillAll (cabinet2);
}

void BeetEditor::Content::paint (juce::Graphics& g)
{
    //  hammer-finish cabinet: the ground decal, tiled; the drawn version if absent
    static const juce::Image ground = BeetLook::art ("ground.jpg");
    if (ground.isValid())
    {
        BeetLook::drawTiled (g, ground, getLocalBounds().toFloat(), 0.75f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));        // a touch darker, so the bays read
        g.fillAll();
    }
    else
    {
        juce::ColourGradient bg (cabinet.brighter (0.06f), 0, 0, cabinet2, 0, (float) H, false);
        g.setGradientFill (bg);
        g.fillAll();
        juce::Random rnd (7);
        for (int i = 0; i < 2600; ++i)     // the hammered texture, fixed seed so it never crawls
        {
            const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
            g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.fillEllipse (x, y, 3.0f + rnd.nextFloat() * 4.0f, 3.0f + rnd.nextFloat() * 4.0f);
        }
    }

    //  header: the enamel nameplate. The delivered plate reads BEET (ordered
    //  before the name became Beetmachine), so the BLANK enamel plate is used
    //  and lettered here - worn cream capitals like the delivered one.
    auto plate = juce::Rectangle<float> (10.0f, 4.0f, 250.0f, 50.0f);
    static const juce::Image plateImg = BeetLook::art ("plate.png");
    if (plateImg.isValid())
        BeetLook::drawNineSlice (g, plateImg, plate, 52, 13.0f);   // rivets keep their shape
    else
    {
        g.setColour (juce::Colour (0xff0c0d0e));
        g.fillRoundedRectangle (plate.reduced (0, 6), 4.0f);
    }
    auto words = plate.reduced (26.0f, 10.0f);
    g.setFont (BeetLook::stencil (25.0f).withHorizontalScale (0.94f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText ("BEETMACHINE", words.translated (1.0f, 1.2f), juce::Justification::centred, false);
    g.setColour (juce::Colour (0xffe8e0cf));
    g.drawText ("BEETMACHINE", words, juce::Justification::centred, false);
    {
        //  wear: a few chips knocked out of the lettering, seeded so they never move
        juce::Random chips (19);
        g.setColour (juce::Colour (0xff15171a).withAlpha (0.75f));
        for (int i = 0; i < 70; ++i)
            g.fillEllipse (words.getX() + chips.nextFloat() * words.getWidth(),
                           words.getY() + chips.nextFloat() * words.getHeight(),
                           0.8f + chips.nextFloat() * 1.8f, 0.8f + chips.nextFloat() * 1.4f);
    }

    g.setFont (BeetLook::mono (12.0f));
    g.setColour (ink.withAlpha (0.78f));
    g.drawText ("KIT", 272, 22, 34, 14, juce::Justification::centredRight, false);
    g.drawText ("NOTES", 558, 22, 60, 14, juce::Justification::centredRight, false);
    if (ed.p.noteMap() == BeetProcessor::MAP_CUSTOM)
    {
        g.setColour (amber);
        g.drawText ("CUSTOM", 720, 22, 60, 14, juce::Justification::centredLeft, false);
        g.setColour (ink.withAlpha (0.78f));
    }
    g.drawText ("MASTER", 770, 22, 56, 14, juce::Justification::centredRight, false);
    g.drawText ("STOP", 1262, 22, 44, 14, juce::Justification::centredRight, false);
    g.setColour (ink.withAlpha (0.4f));
    g.drawText ("build " BEET_BUILD_ID, 1000, 22, 250, 14, juce::Justification::centredRight, false);

    //  the drum panel's bay
    auto area = juce::Rectangle<float> (4.0f, (float) PANEL_Y - 4.0f, (float) W - 8.0f, (float) PANEL_H + 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 6.0f);

    if (ed.child == nullptr)
    {
        g.setColour (ink.withAlpha (0.7f));
        g.setFont (BeetLook::mono (20.0f));
        g.drawFittedText ("SLOT " + juce::String (ed.selected + 1) + " IS EMPTY\n\n"
                          "choose KICKSTART, SNARE TACTICS or HATS OFF on its card above,\n"
                          "and that drum's own panel opens here.",
                          area.toNearestInt().reduced (40), juce::Justification::centred, 5);
    }
}

void BeetEditor::refreshHeader()
{
    kit.setSelectedId (p.kitIndex() + 1, juce::dontSendNotification);
    mapC3.setToggleState (p.noteMap() == BeetProcessor::MAP_C3, juce::dontSendNotification);
    mapGM.setToggleState (p.noteMap() == BeetProcessor::MAP_GM, juce::dontSendNotification);
    content.repaint();
}

void BeetEditor::selectSlot (int s)
{
    selected = juce::jlimit (0, beet::NUM_SLOTS - 1, s);
    if (const int v = p.layoutVersion.load(); v != seenLayout)   // apply a pending layout now, not a timer tick later
    {
        seenLayout = v;
        for (auto& c : cards) c->refresh();
        refreshHeader();
    }
    for (int i = 0; i < beet::NUM_SLOTS; ++i) cards[(size_t) i]->setSelected (i == selected);
    syncChild();
}

//  Closing a drum's panel the way a host must. JUCE's editor destructor does
//  NOT tell its processor it is gone - the HOST calls editorBeingDeleted()
//  first, and here Beetmachine is the host. Without it the drum keeps a
//  dangling "active editor", refuses to create another one, and the slot shows
//  as empty the second time it is selected (Peter, 2026-09-26).
void BeetEditor::releaseChild()
{
    if (child != nullptr && childOwner != nullptr)
        childOwner->editorBeingDeleted (child.get());
    child.reset();
    childOwner = nullptr;
}

void BeetEditor::syncChild()
{
    auto* want = p.slotProcessor (selected);
    if (want == childOwner && (want == nullptr) == (child == nullptr)) return;

    releaseChild();                           // let go of the old drum's editor FIRST
    if (want != nullptr)
    {
        child.reset (want->createEditorAndMakeActive());
        childOwner = want;
        if (child != nullptr)
        {
            const int cw = child->getWidth(), ch = child->getHeight();
            child->setTopLeftPosition ((W - cw) / 2, PANEL_Y + juce::jmax (0, (PANEL_H - ch) / 2));
            content.addAndMakeVisible (*child);
        }
    }
    p.collectGarbage();                       // nothing can point at a replaced drum now
    content.repaint();
}

void BeetEditor::timerCallback()
{
    if (const int v = p.layoutVersion.load(); v != seenLayout)
    {
        seenLayout = v;
        for (auto& c : cards) c->refresh();
        refreshHeader();
        //  a kit sets the rack, so the rack's page must hear about it
        if (rackWeb != nullptr) rackWeb->emitEventIfBrowserIsVisible ("bwfx", bwfx_juce::stateVar (p.rack()));
        if (overlay != nullptr && overlay->isVisible()) overlay->refreshFromRack();
    }
    syncChild();
    for (auto& c : cards) c->tick();
    const bool gmOn = p.noteMap() == BeetProcessor::MAP_GM, c3On = p.noteMap() == BeetProcessor::MAP_C3;
    if (mapGM.getToggleState() != gmOn || mapC3.getToggleState() != c3On) refreshHeader();
}
