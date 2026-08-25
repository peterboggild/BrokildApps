#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // The panel page is compiled into the binary; the plugin runs with no
    // external files (devkit convention).
    juce::WebBrowserComponent::Resource makeUiResource()
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::ui_htmlSize);
        std::memcpy (r.data.data(), BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize);
        r.mimeType = "text/html";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (CloneWarsProcessor& processor)
    {
        using BrowserOptions = juce::WebBrowserComponent::Options;

        auto options = juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path)
                                   -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html")
                    return makeUiResource();
                return std::nullopt;
            })
            .withEventListener ("cw", [&processor] (juce::var payload)
            {
                processor.handleUiMessage (payload);
            });

    #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("CloneWars").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BrowserOptions::Backend::webview2)
                         .withWinWebView2Options (BrowserOptions::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withUserDataFolder (userData));
    #endif

        return options;
    }
}

CloneWarsEditor::CloneWarsEditor (CloneWarsProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    const auto options = buildOptions (p);

   #if JUCE_WINDOWS
    // If WebView2 genuinely cannot start (runtime missing), say so plainly
    // instead of letting JUCE fall back to the IE backend's cryptic
    // "Navigation to the webpage was canceled" page.
    if (! juce::WebBrowserComponent::areOptionsSupported (options))
    {
        fallbackNote = std::make_unique<juce::Label>();
        fallbackNote->setJustificationType (juce::Justification::centred);
        fallbackNote->setColour (juce::Label::textColourId, juce::Colour (0xffe3e9e4));
        fallbackNote->setText ("CLONE WARS\n\n"
                               "The interface needs the Microsoft Edge WebView2 runtime,\n"
                               "which was not found on this machine.\n\n"
                               "Install it from:\n"
                               "developer.microsoft.com/microsoft-edge/webview2\n\n"
                               "then reopen this window. The audio engine is running either way.",
                               juce::dontSendNotification);
        addAndMakeVisible (*fallbackNote);
        setSize (640, 360);
        return;
    }
   #endif

    browser = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr)
            browser->emitEventIfBrowserIsVisible (name, payload);
    };

    setResizable (true, true);
    setResizeLimits (900, 560, 3400, 2200);
    setSize (1760, 1080);
    startTimerHz (15);
}

CloneWarsEditor::~CloneWarsEditor()
{
    processor.emitToUi = nullptr;
}

void CloneWarsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1319));   // the page's stage colour
}

void CloneWarsEditor::resized()
{
    if (browser != nullptr)
        browser->setBounds (getLocalBounds());
    if (fallbackNote != nullptr)
        fallbackNote->setBounds (getLocalBounds());
}

void CloneWarsEditor::timerCallback()
{
    processor.timerService();
}
