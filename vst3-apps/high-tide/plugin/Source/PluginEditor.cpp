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

    /*  The panel's photographic parts, looked up by their original filename so
        renaming one does not need a matching change to a mangled symbol. */
    std::optional<juce::WebBrowserComponent::Resource> decalResource (const juce::String& file)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (file == juce::String (BinaryData::originalFilenames[i]))
            {
                int sz = 0;
                if (const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], sz))
                {
                    juce::WebBrowserComponent::Resource r;
                    r.data.resize ((size_t) sz);
                    std::memcpy (r.data.data(), d, (size_t) sz);
                    r.mimeType = "image/png";
                    return r;
                }
            }
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (HighTideAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;
        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path.startsWith ("/decals/")) return decalResource (path.fromLastOccurrenceOf ("/", false, false));
                return std::nullopt;
            })
            .withEventListener ("hightide", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("HighTide")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff0a141c))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
HighTideAudioProcessorEditor::HighTideAudioProcessorEditor (HighTideAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    processor.uiHasState = false;
    processor.emitToUi = [this] (const juce::String& name, const juce::var& payload)
    {
        if (browser != nullptr) browser->emitEventIfBrowserIsVisible (name, payload);
    };

    setResizable (true, true);
    setResizeLimits (900, 620, 3600, 2400);
    setSize (1240, 820);
    startTimerHz (30);
}

HighTideAudioProcessorEditor::~HighTideAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The chart table before the lamp is lit. WebView2 paints its own background
    only once its controller exists, so the editor paints the wait itself: a
    terrain profile with the water rising across it, in brass and teal. */
void HighTideAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1c28), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff070e15), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    const float t = juce::jlimit (0.0f, 1.0f, (float) ticks / 80.0f);
    const float w = b.getWidth(), h = b.getHeight();
    const float base = h * 0.58f, amp = h * 0.10f;

    //  the relief: valleys and ridges
    juce::Path relief;
    relief.startNewSubPath (0, base);
    for (int i = 0; i <= 160; ++i)
    {
        const float u = (float) i / 160.0f, x = u * w;
        const float y = base - amp * (0.55f + 0.45f * std::sin (u * 6.9f + 0.4f) * std::cos (u * 2.3f)) + amp * 0.6f * std::sin (u * 21.0f) * 0.15f;
        relief.lineTo (x, y);
    }
    relief.lineTo (w, h); relief.lineTo (0, h); relief.closeSubPath();
    g.setColour (juce::Colour (0xff2a3a48));
    g.fillPath (relief);
    g.setColour (juce::Colour (0xffd9c7a0).withAlpha (0.55f));
    g.strokePath (relief, juce::PathStrokeType (1.2f));

    //  the tide, rising
    const float water = base + amp * 0.9f - t * amp * 1.5f;
    g.setColour (juce::Colour (0xff1fb5c4).withAlpha (0.28f));
    g.fillRect (0.0f, water, w, h - water);
    g.setColour (juce::Colour (0xff6fe3ee).withAlpha (0.6f));
    g.drawLine (0, water, w, water, 1.0f);

    //  the pearl, in the deepest valley
    const float px = w * 0.42f, py = base - amp * 0.12f - 6.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff4dc), px - 3, py - 3, juce::Colour (0xffb9a98a), px + 4, py + 5, true));
    g.fillEllipse (px - 6, py - 6, 12, 12);

    g.setFont (juce::Font (juce::FontOptions ("Georgia", juce::jlimit (26.0f, 44.0f, w * 0.03f), juce::Font::plain)));
    g.setColour (juce::Colour (0xffc9a24a));
    g.drawText ("H I G H   T I D E", getLocalBounds().withTrimmedTop (proportionOfHeight (0.22f)).withHeight (56),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions ("Georgia", 12.0f, juce::Font::italic)));
    g.setColour (juce::Colour (0xff8a6a2a));
    g.drawText ("the chart table is being laid", getLocalBounds().withTrimmedTop (proportionOfHeight (0.22f) + 58).withHeight (20),
                juce::Justification::centred, false);
   #ifdef HT_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff4a5560));
    g.drawText ("BROKILD  -  BUILD " + juce::String (HT_BUILD_ID), getLocalBounds().withTrimmedTop (getHeight() - 26).withHeight (18),
                juce::Justification::centred, false);
   #endif
}

void HighTideAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a141c));
    if (! uiRevealed) paintSplash (g);
}

void HighTideAudioProcessorEditor::resized()
{
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void HighTideAudioProcessorEditor::timerCallback()
{
    if (! uiRevealed)
    {
        ++ticks;
        if (! processor.uiHasState.load() && ticks == 240 && ! retried)
        {
            retried = true;
            browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        }
        if (processor.uiHasState.load() || ticks > (retried ? 480 : 240))
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
