#include "PluginEditor.h"
#include "BinaryData.h"
#if JUCE_WINDOWS
 #include <WebView2.h>
#endif

namespace
{
    std::optional<juce::WebBrowserComponent::Resource> namedResource (const juce::String& path)
    {
        juce::String want = path.startsWith ("/") ? path.substring (1) : path;
        if (want.isEmpty() || want == "index.html") want = "ui.html";
        // the page asks for assets/decals/<file>; BinaryData knows files by their basename
        if (want.containsChar ('/')) want = want.fromLastOccurrenceOf ("/", false, false);
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
            r.mimeType = original.endsWithIgnoreCase (".html") ? "text/html"
                       : original.endsWithIgnoreCase (".js")   ? "text/javascript"
                       : original.endsWithIgnoreCase (".png")  ? "image/png" : "application/octet-stream";
            return r;
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (Nineteen84AudioProcessor& p)
    {
        using WBO = juce::WebBrowserComponent::Options;
        auto options = WBO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) { return namedResource (path); })
            .withEventListener ("n84", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        // The standalone takes a profile of its own per launch: a stale
        // msedgewebview2 from an automated run holding a shared profile open
        // with different arguments silently falls a new launch back to IE.
        auto userData = juce::JUCEApplicationBase::isStandaloneApp()
            ? juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("Nineteen84-WebView2-" + juce::String ((juce::int64) GetCurrentProcessId()))
            : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                  .getChildFile ("Brokild").getChildFile ("Nineteen84").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (WBO::Backend::webview2)
                         .withWinWebView2Options (WBO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff0b0b0d))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
Nineteen84AudioProcessorEditor::Nineteen84AudioProcessorEditor (Nineteen84AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
   #if JUCE_WINDOWS
    {
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

    setResizable (true, true);
    setResizeLimits (1000, 620, 3840, 2400);
    setSize (1600, 980);
    startTimerHz (30);
}

Nineteen84AudioProcessorEditor::~Nineteen84AudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours the background colour
    its options carry once its controller exists — a second or so in. */
void Nineteen84AudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0b0b0d));
    // a horizon line and an amber glow, the VFD warming up
    const float cy = r.getCentreY();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x00ffa640), r.getCentreX(), cy - r.getHeight() * 0.25f,
                                             juce::Colour (0x33ffa640), r.getCentreX(), cy, false));
    g.fillRect (r.withHeight (r.getHeight() * 0.5f));
    g.setColour (juce::Colour (0xffffb050));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (40.0f, getHeight() * 0.16f)).withStyle ("Bold")));
    g.drawText ("1984", getLocalBounds().withTrimmedBottom (getHeight() / 8), juce::Justification::centred, false);
    g.setColour (juce::Colour (0x99e8d8b0));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (11.0f, getHeight() * 0.02f))));
    g.drawText (juce::String ("a Brokild polysynth  -  warming up  -  build ")
               #ifdef N84_BUILD_ID
                + N84_BUILD_ID,
               #else
                + "dev",
               #endif
                getLocalBounds().withTrimmedTop (getHeight() / 2 + (int) (getHeight() * 0.08f)),
                juce::Justification::centredTop, false);
}

void Nineteen84AudioProcessorEditor::paint (juce::Graphics& g)
{
    if (webviewMissing)
    {
        g.fillAll (juce::Colour (0xff0b0b0d));
        g.setColour (juce::Colour (0xffffb050));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        auto r = getLocalBounds().reduced (40);
        g.drawText ("1984 needs the Microsoft Edge WebView2 Runtime", r.removeFromTop (60), juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.setColour (juce::Colour (0xffe8d8b0));
        const juce::String msg = juce::String ("This PC does not have it (or it is blocked). It is a free Microsoft component that ships with Windows 11 and Edge.")
                                + juce::newLine + juce::newLine
                                + "Install the Evergreen runtime from:" + juce::newLine
                                + "  https://developer.microsoft.com/microsoft-edge/webview2/" + juce::newLine + juce::newLine
                                + "then reopen 1984. The synth is playing regardless; only the panel needs it.";
        g.drawFittedText (msg, r, juce::Justification::centredTop, 8);
        return;
    }
    if (! uiRevealed) paintSplash (g);
    else              g.fillAll (juce::Colour (0xff0b0b0d));
}

void Nineteen84AudioProcessorEditor::resized()
{
    if (browser == nullptr) return;
    if (uiRevealed) browser->setBounds (getLocalBounds());
    else            browser->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));   // parked, NOT hidden
}

void Nineteen84AudioProcessorEditor::timerCallback()
{
    if (webviewMissing) return;
    processor.timerService();
    if (! uiRevealed)
    {
        ++ticks;
        if (processor.uiReady.load() || ticks > 30 * 8)
        {
            uiRevealed = true;
            resized();
            repaint();
        }
    }
}
