#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // The panel is one embedded page; anything else asked for does not exist.
    std::optional<juce::WebBrowserComponent::Resource> namedResource (const juce::String& path)
    {
        juce::String want = path.startsWith ("/") ? path.substring (1) : path;
        if (want.isEmpty() || want == "index.html") want = "ui.html";

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (juce::String (BinaryData::originalFilenames[i]) != want) continue;
            int size = 0;
            const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            if (data == nullptr || size <= 0) break;

            juce::WebBrowserComponent::Resource r;
            r.data.resize ((size_t) size);
            std::memcpy (r.data.data(), data, (size_t) size);
            r.mimeType = "text/html";
            return r;
        }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (Marmalade50AudioProcessor& p)
    {
        using WBO = juce::WebBrowserComponent::Options;

        auto options = WBO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) { return namedResource (path); })
            .withEventListener ("mm", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        // Its own WebView2 profile folder, so another Brokild plugin or a dev
        // standalone holding a different one open cannot blank this panel.
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild")
                            .getChildFile ("Marmalade50")
                            .getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (WBO::Backend::webview2)
                         .withWinWebView2Options (WBO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff1d1410))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
Marmalade50AudioProcessorEditor::Marmalade50AudioProcessorEditor (Marmalade50AudioProcessor& p)
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

    // 2:1, because the panel is
    setResizable (true, true);
    setResizeLimits (800, 400, 2800, 1400);
    setSize (1200, 600);
    startTimerHz (30);
}

Marmalade50AudioProcessorEditor::~Marmalade50AudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

void Marmalade50AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1d1410));
    if (uiRevealed) return;

    // covers the second or so before the page is up
    g.setColour (juce::Colour (0xffe8892a));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (16.0f, getHeight() * 0.06f)).withStyle ("Bold")));
    g.drawText ("MARMALADE 50", getLocalBounds().reduced (24), juce::Justification::centred, false);
    g.setColour (juce::Colour (0x88e8892a));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, getHeight() * 0.025f))));
    g.drawText ("warming the valves", getLocalBounds().reduced (24).withTrimmedTop (getHeight() / 6),
                juce::Justification::centred, false);
}

void Marmalade50AudioProcessorEditor::resized()
{
    if (browser == nullptr) return;
    if (uiRevealed)
        browser->setBounds (getLocalBounds());
    else
        // parked off-screen, NOT hidden: emitEventIfBrowserIsVisible tests
        // isVisible(), so hiding it would cut the page off from its state
        browser->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));
}

void Marmalade50AudioProcessorEditor::timerCallback()
{
    if (uiRevealed) return;
    ++ticks;
    if (processor.uiReady.load() || ticks > 30 * 8)
    {
        uiRevealed = true;
        resized();
        repaint();
    }
}
