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

    juce::WebBrowserComponent::Resource bwfxResource()      // the shared BWFX overlay
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
        std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
        r.mimeType = "application/javascript";
        return r;
    }

    /*  A panel decal, looked up by its ORIGINAL filename. JUCE mangles
        filenames into C identifiers, so the generated table is the only
        honest way back — and a part that was never delivered simply is not in
        it, which is exactly the answer the page wants. */
    std::optional<juce::WebBrowserComponent::Resource> artResource (const juce::String& file)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (file != juce::String (BinaryData::originalFilenames[i])) continue;
            int size = 0;
            if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
            {
                juce::WebBrowserComponent::Resource r;
                r.data.resize ((size_t) size);
                std::memcpy (r.data.data(), data, (size_t) size);
                r.mimeType = "image/png";
                return r;
            }
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (FmrAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path.startsWith ("/art/")) return artResource (path.fromLastOccurrenceOf ("/", false, false));
                return std::nullopt;
            })
            .withEventListener ("fm", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        /*  The standalone gets its OWN WebView2 profile. WebView2 joins a
            RUNNING browser process per user-data folder and refuses to join one
            started with different arguments — so a standalone left open with a
            debugging port would leave the plugin blank white inside a host. */
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("FullMetalRacket")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff1a140c))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
FmrAudioProcessorEditor::FmrAudioProcessorEditor (FmrAudioProcessor& p)
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
    setResizeLimits (1000, 462, 3800, 1900);

    /*  The aspect is locked ONLY in the standalone, where we own the window.
        In a plugin the host owns it: Ableton resizes the editor, the
        constrainer overrides the height it asked for, and Ableton's frame
        does not grow to match — so the component ends up taller than the
        visible area and the bottom is outside it. The page cannot fix that
        from inside, because it never sees the missing pixels.

        It costs nothing to drop: the panel fits whatever size it is handed,
        measured across five deliberately wrong aspects, so a host that gives
        an odd shape gets a smaller panel on the machine's own dark ground
        rather than a cut one. */
    if (juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* c = getConstrainer())
            c->setFixedAspectRatio (1800.0 / 830.0);

    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open
    startTimerHz (30);
}

FmrAudioProcessorEditor::~FmrAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor: WebView2 only honours its background colour once its
    controller exists, which is after the second or so this covers. */
void FmrAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff241b10), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff120d07), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // walnut cheeks, as on the panel
    g.setColour (juce::Colour (0xff4a2d18));
    g.fillRect (0.0f, 0.0f, 24.0f, b.getHeight());
    g.fillRect (b.getWidth() - 24.0f, 0.0f, 24.0f, b.getHeight());

    // the twelve lamps come up one at a time, left to right
    const float cy = b.getCentreY() + 26.0f;
    const float span = juce::jmin (620.0f, b.getWidth() * 0.55f);
    for (int i = 0; i < 12; ++i)
    {
        const float x = b.getCentreX() - span * 0.5f + span * (i / 11.0f);
        const float lit = juce::jlimit (0.0f, 1.0f, ((float) ticks - 8.0f * i) / 26.0f);
        const auto col = juce::Colour (0xff3a2f1e).interpolatedWith (juce::Colour (0xffff8f1f), lit);
        g.setColour (col.withAlpha (0.16f + 0.5f * lit));
        g.fillEllipse (x - 11.0f, cy - 11.0f, 22.0f, 22.0f);
        g.setColour (col);
        g.fillEllipse (x - 4.5f, cy - 4.5f, 9.0f, 9.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (20.0f, 46.0f, b.getWidth() * 0.026f)).withStyle ("Bold")));
    g.setColour (juce::Colour (0xfff0e6d2));
    g.drawText ("F U L L   M E T A L   R A C K E T",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.36f)).withTrimmedBottom (proportionOfHeight (0.50f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.62f));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff9a8f78));
    const auto dot = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));
    g.drawText ("B R O K I L D   " + dot + "   P O L Y R H Y T H M I C   B E A T   C O M P O S E R",
                under.removeFromTop (22), juce::Justification::centred, false);
   #ifdef FM_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff6a6152));
    g.drawText ("BUILD " + juce::String (FM_BUILD_ID), under.removeFromTop (20), juce::Justification::centred, false);
   #endif
}

void FmrAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a140c));
    if (! uiRevealed) paintSplash (g);
}

void FmrAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible tests isVisible(), so a
    // hidden browser is cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void FmrAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        if (! processor.uiReady.load() && ticks == 240 && ! retried)
        {
            retried = true;                 // a transient WebView2 hiccup: navigate once more
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
