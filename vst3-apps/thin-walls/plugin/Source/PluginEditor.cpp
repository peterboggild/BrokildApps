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
            r.mimeType = original.endsWithIgnoreCase (".html") ? "text/html" : "application/octet-stream";
            return r;
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (ThinWallsAudioProcessor& p)
    {
        using WBO = juce::WebBrowserComponent::Options;
        auto options = WBO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) { return namedResource (path); })
            .withEventListener ("tw", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        // Its own profile folder: a WebView2 joins a RUNNING browser process per
        // user-data folder and refuses one started with different arguments.
        // The standalone takes a profile of its own PER LAUNCH (in the temp folder,
        // named by its process id): a stale msedgewebview2 left over from an
        // automated run holds a shared profile open with different arguments,
        // and a new launch then silently falls back to the old browser control -
        // which is what Peter saw the first time he ran the downloaded exe.
        auto userData = juce::JUCEApplicationBase::isStandaloneApp()
            ? juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("ThinWalls-WebView2-" + juce::String ((juce::int64) GetCurrentProcessId()))
            : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                  .getChildFile ("Brokild").getChildFile ("ThinWalls").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (WBO::Backend::webview2)
                         .withWinWebView2Options (WBO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff17130f))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
ThinWallsAudioProcessorEditor::ThinWallsAudioProcessorEditor (ThinWallsAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
   #if JUCE_WINDOWS
    {
        // Is the Microsoft Edge WebView2 runtime here at all? Without it JUCE
        // falls back to the old Internet Explorer control and shows its
        // "navigation was cancelled" page, which says nothing useful.
        LPWSTR ver = nullptr;
        const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString (nullptr, &ver);
        if (SUCCEEDED (hr) && ver != nullptr) { webviewVersion = juce::String (ver); CoTaskMemFree (ver); }
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
    setResizeLimits (900, 600, 3840, 2400);
    setSize (1400, 900);
    startTimerHz (30);
}

ThinWallsAudioProcessorEditor::~ThinWallsAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours the background colour
    its options carry once its controller exists - a second or so in. */
void ThinWallsAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2119), r.getCentreX(), r.getY(),
                                             juce::Colour (0xff100d0a), r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
    // a doorway with light behind it
    const float w = r.getHeight() * 0.22f, h = r.getHeight() * 0.5f;
    juce::Rectangle<float> door (r.getCentreX() - w * 0.5f, r.getCentreY() - h * 0.55f, w, h);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd9a0), door.getCentreX(), door.getY(),
                                             juce::Colour (0xff8a5a2a), door.getCentreX(), door.getBottom(), false));
    g.fillRect (door);
    g.setColour (juce::Colour (0xff3b2e22));
    g.drawRect (door.expanded (w * 0.08f), w * 0.08f);

    g.setColour (juce::Colour (0xffe8c98f));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (18.0f, getHeight() * 0.05f)).withStyle ("Bold")));
    g.drawText ("THIN WALLS", getLocalBounds().withTrimmedTop ((int) door.getBottom() + 12),
                juce::Justification::centredTop, false);
    g.setColour (juce::Colour (0x99c8a878));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, getHeight() * 0.02f))));
    g.drawText (juce::String ("a Brokild apartment  -  ") + JucePlugin_VersionString,
                getLocalBounds().withTrimmedTop ((int) door.getBottom() + 12 + (int) (getHeight() * 0.07f)),
                juce::Justification::centredTop, false);
}

void ThinWallsAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (webviewMissing)
    {
        g.fillAll (juce::Colour (0xff17130f));
        g.setColour (juce::Colour (0xffe8c98f));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
        auto r = getLocalBounds().reduced (40);
        g.drawText ("THIN WALLS needs the Microsoft Edge WebView2 Runtime", r.removeFromTop (60), juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.setColour (juce::Colour (0xffc8a878));
        const juce::String msg = juce::String ("This PC does not have it (or it is blocked). It is a free Microsoft component that ships with Windows 11 and Edge.")
                                + juce::newLine + juce::newLine
                                + "Install the Evergreen runtime from:" + juce::newLine
                                + "  https://developer.microsoft.com/microsoft-edge/webview2/" + juce::newLine + juce::newLine
                                + "then reopen Thin Walls. The audio engine is running regardless; only the panel needs it.";
        g.drawFittedText (msg, r, juce::Justification::centredTop, 8);
        return;
    }
    if (! uiRevealed) paintSplash (g);
    else              g.fillAll (juce::Colour (0xff17130f));
}

void ThinWallsAudioProcessorEditor::resized()
{
    if (browser == nullptr) return;
    if (uiRevealed)
        browser->setBounds (getLocalBounds());
    else
        // parked off-screen, NOT hidden: emitEventIfBrowserIsVisible tests isVisible()
        browser->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));
}

void ThinWallsAudioProcessorEditor::timerCallback()
{
    if (webviewMissing) return;
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
