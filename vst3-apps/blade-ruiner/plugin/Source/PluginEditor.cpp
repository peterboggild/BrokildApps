#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    juce::WebBrowserComponent::Resource uiResource()
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::ui_htmlSize);
        std::memcpy (r.data.data(), BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize);
        r.mimeType = "text/html";
        return r;
    }

    juce::WebBrowserComponent::Resource bwfxResource()   // the shared BWFX rack overlay
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
        std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
        r.mimeType = "application/javascript";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (BladeRuinerAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                return std::nullopt;
            })
            .withEventListener ("br", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("BladeRuiner").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff070a10))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
BladeRuinerAudioProcessorEditor::BladeRuinerAudioProcessorEditor (BladeRuinerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    processor.uiHasState = false;
    processor.uiReady = false;
    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr) browser->emitEventIfBrowserIsVisible (name, payload);
    };

    setResizable (true, true);
    setResizeLimits (840, 560, 3400, 2400);
    setSize (1380, 900);
    startTimerHz (30);
}

BladeRuinerAudioProcessorEditor::~BladeRuinerAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours the background colour
    its options carry once its controller exists — which is after the second
    or so this is here to cover. */
void BladeRuinerAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101a26), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff05070c), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // a searchlight, sweeping
    const float sweep = std::sin ((float) ticks * 0.021f);
    juce::ColourGradient beam (juce::Colour (0xffffb45a).withAlpha (0.11f),
                               b.getCentreX() + sweep * b.getWidth() * 0.34f, b.getY(),
                               juce::Colours::transparentBlack,
                               b.getCentreX() + sweep * b.getWidth() * 0.34f, b.getBottom(), false);
    g.setGradientFill (beam);
    g.fillRect (b);

    const float pulse = 0.55f + 0.45f * std::sin ((float) ticks * 0.075f);
    juce::ColourGradient glow (juce::Colour (0xff2ad4d4).withAlpha (0.13f * pulse),
                               b.getCentreX(), b.getCentreY(),
                               juce::Colours::transparentBlack,
                               b.getCentreX(), b.getCentreY() + b.getHeight() * 0.55f, true);
    g.setGradientFill (glow);
    g.fillRect (b);

    const float size = juce::jlimit (17.0f, 46.0f, b.getWidth() * 0.034f);
    g.setFont (juce::Font (juce::FontOptions (size).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffe8dcc8));
    g.drawText ("B L A D E   R U I N E R",
                getLocalBounds().withTrimmedBottom (proportionOfHeight (0.13f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.53f));

    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff5f7d8a));
    g.drawText ("R O S E N   A S S O C I A T I O N   " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"))
                + "   B R I N G I N G   T H E   C I T Y   U P",
                under.removeFromTop (24), juce::Justification::centred, false);

    auto row = under.removeFromTop (26);
    const int lit = (ticks / 8) % 4;
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<float> dot ((float) row.getCentreX() - 26.0f + (float) i * 26.0f,
                                    (float) row.getY() + 9.0f, 9.0f, 9.0f);
        static const juce::Colour cols[3] { juce::Colour (0xffffa23c), juce::Colour (0xffe8c88a),
                                            juce::Colour (0xff36e0e0) };
        g.setColour (i < lit ? cols[i] : juce::Colour (0xff1b2731));
        g.fillEllipse (dot);
    }

   #ifdef BR_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff405663));
    g.drawText ("BROKILD  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  BUILD "
                + juce::String (BR_BUILD_ID), under.removeFromTop (22),
                juce::Justification::centred, false);
   #endif
}

void BladeRuinerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff070a10));
    if (! uiRevealed) paintSplash (g);
}

void BladeRuinerAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible tests isVisible(), so a
    // hidden browser would be cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void BladeRuinerAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        if (processor.uiReady.load() || ticks > 240)
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
