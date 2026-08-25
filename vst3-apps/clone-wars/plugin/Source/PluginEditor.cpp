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
    browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
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
    browser->setBounds (getLocalBounds());
}

void CloneWarsEditor::timerCallback()
{
    processor.timerService();
}
