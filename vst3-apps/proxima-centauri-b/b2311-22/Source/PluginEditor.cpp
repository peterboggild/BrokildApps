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

    /*  The collection's own stationery: the globe, and the metal and wear of
        the crate the specimen travelled in. Shared with B2311.67, because the
        two came out of the same survey and the paperwork was printed on the
        same stock. */
    juce::WebBrowserComponent::Resource pngResource (const void* data, int size)
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) size);
        std::memcpy (r.data.data(), data, (size_t) size);
        r.mimeType = "image/png";
        return r;
    }

    juce::WebBrowserComponent::Resource bwfxResource()
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
        std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
        r.mimeType = "application/javascript";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (ArtefactAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path == "/img/tx-planet.png") return pngResource (BinaryData::txplanet_png, BinaryData::txplanet_pngSize);
                if (path == "/img/tx-alu.png")    return pngResource (BinaryData::txalu_png,    BinaryData::txalu_pngSize);
                if (path == "/img/tx-wear.png")   return pngResource (BinaryData::txwear_png,   BinaryData::txwear_pngSize);
                return std::nullopt;
            })
            .withEventListener ("ab", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        //  the standalone gets its OWN WebView2 profile (the Black Rider lesson)
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("ArtefactB2311")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff05070c))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
ArtefactAudioProcessorEditor::ArtefactAudioProcessorEditor (ArtefactAudioProcessor& p)
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
    setResizeLimits (900, 560, 3800, 2200);
    setSize (1240, 800);
    startTimerHz (30);
}

ArtefactAudioProcessorEditor::~ArtefactAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The splash: a point of light condensing out of nothing in a void — the
    artefact arriving in our slice. WebView2 paints white before its first
    frame; this covers it, diegetically.  */
void ArtefactAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff090d16), b.getCentreX(), b.getCentreY(),
                                             juce::Colour (0xff03040a), b.getX(), b.getY(), true));
    g.fillRect (b);

    const float t = juce::jlimit (0.0f, 1.0f, (float) ticks / 90.0f);
    const float cx = b.getCentreX(), cy = b.getCentreY() - 20.0f;

    //  the condensation
    for (int ring = 5; ring >= 0; --ring)
    {
        const float rr = (14.0f + 46.0f * ring) * (1.2f - 0.5f * t);
        const float a = t * 0.10f * (1.0f - ring / 6.0f);
        g.setColour (juce::Colour::fromHSV (0.52f + 0.05f * ring, 0.55f, 0.9f, a));
        g.fillEllipse (cx - rr, cy - rr * 0.8f, rr * 2.0f, rr * 1.6f);
    }
    g.setColour (juce::Colour (0xffd8f2ee).withAlpha (0.25f + 0.6f * t));
    const float core = 3.0f + 5.0f * t;
    g.fillEllipse (cx - core, cy - core, core * 2.0f, core * 2.0f);

    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff77837e).withAlpha (0.9f));
    const auto dot = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));
    g.drawText ("PROXIMS CENTAURI B  " + dot + "  ARTEFACT B2311.22",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.62f)).withHeight (22),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff4a5450));
    juce::String line = "CONTAINMENT STABLE  " + dot + "  SECTION ALIGNING";
   #ifdef AB_BUILD_ID
    line += "  " + dot + "  SURVEY BUILD " + juce::String (AB_BUILD_ID);
   #endif
    g.drawText (line, getLocalBounds().withTrimmedTop (proportionOfHeight (0.62f) + 24).withHeight (18),
                juce::Justification::centred, false);
}

void ArtefactAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05070c));
    if (! uiRevealed) paintSplash (g);
}

void ArtefactAudioProcessorEditor::resized()
{
    //  parked off-screen until ready: emitEventIfBrowserIsVisible tests
    //  isVisible(), so a hidden browser would be cut off from its state
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void ArtefactAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
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
