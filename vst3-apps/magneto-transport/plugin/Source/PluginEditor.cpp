#include "PluginEditor.h"
#include "BinaryData.h"
#if JUCE_WINDOWS
 #include <WebView2.h>
#endif

namespace
{
    /*  The page and its art are embedded; looked up through BinaryData's own
        filename table, so adding a file to CMakeLists is enough. */
    std::optional<juce::WebBrowserComponent::Resource> namedResource (const juce::String& path)
    {
        juce::String want = path.startsWith ("/") ? path.substring (1) : path;
        if (want.isEmpty() || want == "index.html") want = "ui.html";
        want = want.fromLastOccurrenceOf ("/", false, false);

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String original (BinaryData::originalFilenames[i]);
            if (original != want) continue;
            int size = 0;
            const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            if (data == nullptr || size <= 0) break;
            juce::WebBrowserComponent::Resource r;
            r.data.resize ((size_t) size);
            std::memcpy (r.data.data(), data, (size_t) size);
            if (original.endsWithIgnoreCase (".html"))      r.mimeType = "text/html";
            else if (original.endsWithIgnoreCase (".png"))  r.mimeType = "image/png";
            else if (original.endsWithIgnoreCase (".jpg"))  r.mimeType = "image/jpeg";
            else                                            r.mimeType = "application/octet-stream";
            return r;
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (MagnetoTransportAudioProcessor& p)
    {
        using WBO = juce::WebBrowserComponent::Options;
        auto options = WBO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) { return namedResource (path); })
            .withEventListener ("mt", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        // Its own profile: a WebView2 joins a RUNNING browser process per user-data
        // folder and refuses one started with different arguments. The standalone
        // takes a profile per launch, so a stale msedgewebview2 from a CDP run can
        // never blank it (Thin Walls' field failure).
        auto userData = juce::JUCEApplicationBase::isStandaloneApp()
            ? juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("MagnetoTransport-WebView2-" + juce::String ((juce::int64) GetCurrentProcessId()))
            : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                  .getChildFile ("Brokild").getChildFile ("MagnetoTransport").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (WBO::Backend::webview2)
                         .withWinWebView2Options (WBO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff0d0f0e))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
MagnetoTransportAudioProcessorEditor::MagnetoTransportAudioProcessorEditor (MagnetoTransportAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
   #if JUCE_WINDOWS
    {
        // Is the WebView2 runtime here at all? Without it JUCE falls back to the
        // old Internet Explorer control, whose error page says nothing useful.
        LPWSTR ver = nullptr;
        const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString (nullptr, &ver);
        if (SUCCEEDED (hr) && ver != nullptr) CoTaskMemFree (ver);
        else webviewMissing = true;
    }
   #endif
    if (! webviewMissing)
    {
        browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
        addAndMakeVisible (*browser);
        browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }

    processor.uiHasState = false;
    processor.uiReady = false;
    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr) browser->emitEventIfBrowserIsVisible (name, payload);
    };

    // 16:9, the shape of the deck.
    setResizable (true, true);
    setResizeLimits (960, 540, 3840, 2160);
    setSize (1600, 900);
    startTimerHz (30);
}

MagnetoTransportAudioProcessorEditor::~MagnetoTransportAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor: WebView2 honours its background colour only once the
    controller exists, a second or so in. */
void MagnetoTransportAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0d0f0e));
    // two reels, still
    const float rr = r.getHeight() * 0.16f;
    g.setColour (juce::Colour (0xff2a2d2b));
    for (float cx : { r.getCentreX() - rr * 1.6f, r.getCentreX() + rr * 1.6f })
    {
        g.drawEllipse (cx - rr, r.getCentreY() - rr * 1.3f, rr * 2, rr * 2, 3.0f);
        g.fillEllipse (cx - rr * 0.25f, r.getCentreY() - rr * 0.55f, rr * 0.5f, rr * 0.5f);
    }
    g.setColour (juce::Colour (0xffc9b27a));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (18.0f, getHeight() * 0.05f)).withStyle ("Bold")));
    g.drawText ("MAGNETO TRANSPORT", getLocalBounds().withTrimmedTop ((int) (r.getCentreY() + rr * 0.9f)),
                juce::Justification::centredTop, false);
    g.setColour (juce::Colour (0x99a89a72));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, getHeight() * 0.02f))));
    g.drawText (juce::String ("threading tape  ") + JucePlugin_VersionString,
                getLocalBounds().withTrimmedTop ((int) (r.getCentreY() + rr * 0.9f + getHeight() * 0.07f)),
                juce::Justification::centredTop, false);
}

void MagnetoTransportAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (webviewMissing)
    {
        g.fillAll (juce::Colour (0xff0d0f0e));
        auto r = getLocalBounds().reduced (40);
        g.setColour (juce::Colour (0xffc9b27a));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        g.drawText ("MAGNETO TRANSPORT needs the Microsoft Edge WebView2 Runtime", r.removeFromTop (60),
                    juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawFittedText ("Install the Evergreen runtime from https://developer.microsoft.com/microsoft-edge/webview2/ "
                          "and reopen the plug-in. The tape is running regardless; only the panel needs it.",
                          r, juce::Justification::centredTop, 6);
        return;
    }
    if (! uiRevealed) paintSplash (g);
    else              g.fillAll (juce::Colour (0xff0d0f0e));
}

void MagnetoTransportAudioProcessorEditor::resized()
{
    if (browser == nullptr) return;
    if (uiRevealed)
        browser->setBounds (getLocalBounds());
    else
        // parked off-screen, NOT hidden: emitEventIfBrowserIsVisible tests isVisible()
        browser->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));
}

void MagnetoTransportAudioProcessorEditor::timerCallback()
{
    if (webviewMissing || uiRevealed) return;
    if (processor.uiReady.load() || ++ticks > 30 * 8)
    {
        uiRevealed = true;
        resized();
        repaint();
    }
}
