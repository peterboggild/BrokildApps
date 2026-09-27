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

    juce::WebBrowserComponent::Options buildOptions (BlackRiderAudioProcessor& p)
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
            .withEventListener ("bk", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        /*  The standalone gets its OWN WebView2 profile. WebView2 joins a
            RUNNING browser process per user-data folder, and refuses to join
            one that was started with different arguments — so a standalone
            left open (say, with a debugging port) would leave the plugin in a
            host with a blank white view. Separate folders, no collision. */
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("BlackRider")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff0b0b0c))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
BlackRiderAudioProcessorEditor::BlackRiderAudioProcessorEditor (BlackRiderAudioProcessor& p)
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
    setResizeLimits (900, 600, 3800, 2400);
    setSize (processor.uiWide.load() ? 2000 : 1440, 940);
    /*  The patch bay is a wing to the right of the panel: when it opens the
        page asks for a wider window, keeping the height, so the main panel
        stays the size it was. A host that refuses just gets a scaled deck. */
    processor.onWide = [this] (bool wide)
    {
        const int h = getHeight();
        const int w = juce::roundToInt (h * (wide ? 2000.0 : 1440.0) / 940.0);
        if (std::abs (getWidth() - w) > 2) setSize (w, h);
    };
    startTimerHz (30);
}

BlackRiderAudioProcessorEditor::~BlackRiderAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
    processor.onWide = nullptr;
}

/*  Painted by the editor, because WebView2 only honours its background colour
    once its controller exists — after the second or so this covers. */
void BlackRiderAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1c), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff08080a), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // walnut cheeks
    g.setColour (juce::Colour (0xff3b2418));
    g.fillRect (0.0f, 0.0f, 22.0f, b.getHeight());
    g.fillRect (b.getWidth() - 22.0f, 0.0f, 22.0f, b.getHeight());

    /*  The power lamp, warming slowly to red like a valve heater — the whole
        wait is the lamp coming up. A soft breathing on top so it reads as
        alive rather than stuck. */
    const float warm = juce::jlimit (0.0f, 1.0f, (float) ticks / 110.0f);
    const float breathe = 0.92f + 0.08f * std::sin ((float) ticks * 0.11f);
    const float cx = b.getCentreX(), cy = b.getCentreY() - b.getHeight() * 0.06f;
    const float glowR = (30.0f + 34.0f * warm) * breathe;
    // cold grey filament -> deep ember -> bright red
    const auto lamp = juce::Colour (0xff3a3a3e).interpolatedWith (juce::Colour (0xffff2e14), warm);
    g.setColour (lamp.withAlpha (0.05f + 0.40f * warm * breathe));
    g.fillEllipse (cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
    g.setColour (lamp.withAlpha (0.10f + 0.60f * warm));
    g.fillEllipse (cx - 15.0f, cy - 15.0f, 30.0f, 30.0f);
    g.setColour (juce::Colour (0xff55555a).interpolatedWith (juce::Colour (0xffffc9a8), warm).withAlpha (0.35f + 0.65f * warm));
    g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    // the bezel
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawEllipse (cx - 16.0f, cy - 16.0f, 32.0f, 32.0f, 2.5f);
    g.setColour (juce::Colour (0xff4a4a4e));
    g.drawEllipse (cx - 18.0f, cy - 18.0f, 36.0f, 36.0f, 1.2f);

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (20.0f, 48.0f, b.getWidth() * 0.032f)).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffe8e6df));
    g.drawText ("B L A C K   R I D E R", getLocalBounds().withTrimmedTop (proportionOfHeight (0.50f)).withTrimmedBottom (proportionOfHeight (0.36f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.70f));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff8d8a80));
    g.drawText (juce::String ("B R O K I L D   ") + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   A N A L O G U E   M O N O S Y N T H   "
                + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   W A R M I N G   U P",
                under.removeFromTop (22), juce::Justification::centred, false);
   #ifdef BK_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff5a5851));
    g.drawText ("BUILD " + juce::String (BK_BUILD_ID), under.removeFromTop (20), juce::Justification::centred, false);
   #endif
}

void BlackRiderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));
    if (! uiRevealed) paintSplash (g);
}

void BlackRiderAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible tests isVisible(), so a
    // hidden browser would be cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void BlackRiderAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        /*  If the page has not said ready by ~8 s, the WebView likely failed
            its first navigation (a transient WebView2 hiccup in a host).
            Navigate again and keep the splash up — once — before giving up
            and revealing whatever is there. */
        if (! processor.uiReady.load() && ticks == 240 && ! retried)
        {
            retried = true;
            browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        }
        if (processor.uiReady.load() || ticks > (retried ? 480 : 240))
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
