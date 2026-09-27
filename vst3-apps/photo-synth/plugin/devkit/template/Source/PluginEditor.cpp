#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // The page is compiled into the binary by juce_add_binary_data, so the
    // plugin runs offline with no external files.
    juce::WebBrowserComponent::Resource makeUiResource()
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::ui_htmlSize);
        std::memcpy (r.data.data(), BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize);
        r.mimeType = "text/html";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (PluginProcessorTemplate& processor)
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
            .withEventListener ("ps", [&processor] (juce::var payload)
            {
                processor.handleUiMessage (payload);
            });

    #if JUCE_WINDOWS
        // The default user-data folder sits next to the host executable and is
        // often not writable; put it somewhere the user certainly owns.
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("YourVendor").getChildFile ("YourProduct").getChildFile ("WebView2");
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

PluginEditorTemplate::PluginEditorTemplate (PluginProcessorTemplate& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // How the processor reaches the page. Cleared in the destructor so the
    // processor knows when there is no UI.
    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr)
            browser->emitEventIfBrowserIsVisible (name, payload);
    };

    setResizable (true, true);
    setResizeLimits (700, 520, 3200, 2400);
    setSize (1300, 900);
    startTimerHz (30);
}

PluginEditorTemplate::~PluginEditorTemplate()
{
    processor.emitToUi = nullptr;
}

void PluginEditorTemplate::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101010));   // matches the page background
}

void PluginEditorTemplate::resized()
{
    browser->setBounds (getLocalBounds());
}

void PluginEditorTemplate::timerCallback()
{
    // Clock, MIDI relay, dirty automation, consumed payload collection.
    processor.timerService();
}
