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

    juce::WebBrowserComponent::Resource bwfxResource()
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
        std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
        r.mimeType = "application/javascript";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (GravityWellAudioProcessor& p)
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
            .withEventListener ("gw", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        //  A stale msedgewebview2 holding the shared profile is one way to get
        //  Internet Explorer's error page instead of a panel, so the standalone
        //  keeps its own.
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("GravityWell")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff05060a))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
GravityWellAudioProcessorEditor::GravityWellAudioProcessorEditor (GravityWellAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
   #if JUCE_WINDOWS
    /*  Peter's work PC showed Internet Explorer's "navigation cancelled" page
        because the WebView2 runtime was not there.  Say so, with the link,
        instead of showing a page nobody can read.                          */
    webviewMissing = juce::WebBrowserComponent::areOptionsSupported (buildOptions (p)) == false;
   #endif

    if (! webviewMissing)
    {
        browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
        addAndMakeVisible (*browser);
        browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }

    processor.uiHasState = false;
    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr) browser->emitEventIfBrowserIsVisible (name, payload);
    };

    setResizable (true, true);
    //  the cockpit needs its desk: below this the consoles would clip
    setResizeLimits (1100, 760, 3600, 2400);
    setSize (1340, 880);
    startTimerHz (30);
}

GravityWellAudioProcessorEditor::~GravityWellAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The wait before the page arrives.  WebView2 paints its own background only
    once its controller exists - which is after this - so the editor paints
    the wait itself: a funnel drawn in wireframe, turning.                  */
void GravityWellAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff05060a));
    g.fillRect (b);

    const float t  = (float) ticks / 30.0f;
    const float cx = b.getCentreX(), cy = b.getCentreY() + b.getHeight() * 0.06f;
    const float R  = juce::jmin (b.getWidth(), b.getHeight()) * 0.34f;

    //  Flamm's paraboloid in wireframe: rings falling toward the throat
    for (int i = 1; i <= 9; ++i)
    {
        const float u  = (float) i / 9.0f;
        const float rr = R * u;
        const float dz = R * 0.42f * (1.0f - std::sqrt (u));      // the funnel
        const float a  = 0.10f + 0.30f * u;
        g.setColour (juce::Colour (0xff4fc3f7).withAlpha (a));
        g.drawEllipse (cx - rr, cy - rr * 0.34f + dz, 2 * rr, rr * 0.68f, 1.2f);
    }
    //  the photon ring, at the throat
    g.setColour (juce::Colour (0xffffb74d).withAlpha (0.55f + 0.25f * std::sin (t * 2.0f)));
    g.drawEllipse (cx - R * 0.14f, cy + R * 0.42f - R * 0.05f, R * 0.28f, R * 0.10f, 2.0f);

    g.setFont (juce::Font (juce::FontOptions ("Segoe UI", juce::jlimit (22.0f, 40.0f, b.getWidth() * 0.026f), juce::Font::bold)));
    g.setColour (juce::Colour (0xffe8eef6));
    g.drawText ("GRAVITY WELL", getLocalBounds().withTrimmedTop ((int) (cy - R * 1.30f)).withHeight (46),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions ("Segoe UI", 11.0f, juce::Font::plain)));
    g.setColour (juce::Colour (0xff7f8c9b));
    g.drawText ("COLLAPSING", getLocalBounds().withTrimmedTop ((int) (cy - R * 1.30f + 46)).withHeight (20),
                juce::Justification::centred, false);
   #ifdef GW_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff5c6773));
    g.drawText ("BROKILD  -  BUILD " + juce::String (GW_BUILD_ID),
                getLocalBounds().withTrimmedTop (getHeight() - 26).withHeight (18),
                juce::Justification::centred, false);
   #endif
}

void GravityWellAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05060a));

    if (webviewMissing)
    {
        g.setColour (juce::Colour (0xffe8eef6));
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText ("GRAVITY WELL needs the Microsoft WebView2 runtime",
                    getLocalBounds().withHeight (getHeight() / 2), juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.setColour (juce::Colour (0xff9fb0c2));
        g.drawText ("Install it from  https://go.microsoft.com/fwlink/p/?LinkId=2124703  and reopen the plug-in.",
                    getLocalBounds().withTrimmedTop (getHeight() / 2).withHeight (30),
                    juce::Justification::centred, false);
        return;
    }
    if (! uiRevealed) paintSplash (g);
}

void GravityWellAudioProcessorEditor::resized()
{
    //  OFF-SCREEN, never setVisible(false): emitEventIfBrowserIsVisible tests
    //  isVisible(), so hiding the browser cuts the page off from the very
    //  state it is waiting for.
    if (browser != nullptr)
        browser->setBounds (uiRevealed ? getLocalBounds()
                                       : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void GravityWellAudioProcessorEditor::timerCallback()
{
    if (webviewMissing) return;
    if (! uiRevealed)
    {
        ++ticks;
        if (! processor.uiHasState.load() && ticks == 240 && ! retried)
        {
            retried = true;
            if (browser != nullptr) browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        }
        if (processor.uiHasState.load() || ticks > (retried ? 480 : 240))
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
