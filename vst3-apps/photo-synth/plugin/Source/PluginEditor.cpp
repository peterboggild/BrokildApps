#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    juce::WebBrowserComponent::Resource makeUiResource()
    {
        juce::WebBrowserComponent::Resource resource;
        resource.data.resize ((size_t) BinaryData::ui_htmlSize);
        std::memcpy (resource.data.data(), BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize);
        resource.mimeType = "text/html";
        return resource;
    }

    juce::WebBrowserComponent::Options buildOptions (PhotoSynthAudioProcessor& processor)
    {
        using BrowserOptions = juce::WebBrowserComponent::Options;

        auto options = juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html")
                    return makeUiResource();
                if (path == "/bwfx-rack.js")
                {
                    juce::WebBrowserComponent::Resource r;
                    r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
                    std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
                    r.mimeType = "application/javascript";
                    return r;
                }
                return std::nullopt;
            })
            .withEventListener ("ps", [&processor] (juce::var payload)
            {
                processor.handleUiMessage (payload);
            });

    #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("Photo-Synth2").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BrowserOptions::Backend::webview2)
                         .withWinWebView2Options (BrowserOptions::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             // WebView2 paints white until the page puts up its
                             // first frame. Match the editor so the window is
                             // never anything but dark.
                             .withBackgroundColour (juce::Colour (0xff0c0a07))
                             .withUserDataFolder (userData));
    #endif

        return options;
    }
}

PhotoSynthAudioProcessorEditor::PhotoSynthAudioProcessorEditor (PhotoSynthAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    browser = std::make_unique<juce::WebBrowserComponent> (buildOptions (p));
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // A new page starts with nothing: make the processor offer its state again.
    processor.uiHasState = false;
    processor.uiReady = false;
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

PhotoSynthAudioProcessorEditor::~PhotoSynthAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/* The loading screen, drawn by the editor itself.

   It has to be native. WebView2 needs on the order of a second to bring up its
   browser process before a single line of the page can run, and the background
   colour the options carry is only applied once its controller exists — that
   is, only after the wait it was meant to cover. So the editor paints this
   from its very first frame, and the page is parked off to the side until it
   reports that the patch is on screen. */
void PhotoSynthAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff100c08), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff070504), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // warm pool of light behind the wordmark
    juce::ColourGradient glow (juce::Colour (0x1af0a848), b.getCentreX(), b.getCentreY(),
                               juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY() + b.getHeight() * 0.42f, true);
    g.setGradientFill (glow);
    g.fillRect (b);

    // letter-spaced wordmark — JUCE has no tracking, so space it by hand
    const juce::String mark = "P H O T O   " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"))
                            + "   S Y N T H   " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   2";
    const float size = juce::jlimit (18.0f, 46.0f, b.getWidth() * 0.038f);
    g.setFont (juce::Font (juce::FontOptions (size).withStyle ("Bold")));
    g.setColour (juce::Colour (0xfff6d9a8));
    g.drawText (mark, getLocalBounds().withTrimmedBottom (proportionOfHeight (0.10f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.52f));

    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff9c8a6c));
    g.drawText ("R E L O A D I N G", under.removeFromTop (26), juce::Justification::centred, false);

    // a light sweeping along a hairline, so it reads as working rather than stuck
    const int barW = juce::jmin (240, proportionOfWidth (0.34f));
    auto bar = under.removeFromTop (22).withSizeKeepingCentre (barW, 2);
    g.setColour (juce::Colour (0x14ffffff));
    g.fillRoundedRectangle (bar.toFloat(), 1.0f);

    const float phase = std::fmod ((float) revealTicks / 34.0f, 1.0f);
    const float segW = barW * 0.34f;
    const float x = bar.getX() + (barW + segW) * phase - segW;
    juce::ColourGradient seg (juce::Colours::transparentBlack, x, 0.0f,
                              juce::Colours::transparentBlack, x + segW, 0.0f, false);
    seg.addColour (0.5, juce::Colour (0xfff0a848));
    g.setGradientFill (seg);
    g.fillRoundedRectangle (juce::Rectangle<float> (x, (float) bar.getY(), segW, 2.0f)
                                .getIntersection (bar.toFloat()), 1.0f);

   #ifdef PS_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colour (0xff5d523f));
    g.drawText ("BUILD " + juce::String (PS_BUILD_ID), under.removeFromTop (24),
                juce::Justification::centred, false);
   #endif
}

void PhotoSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0a07));
    if (! uiRevealed)
        paintSplash (g);
}

void PhotoSynthAudioProcessorEditor::resized()
{
    // Parked off to the side, not hidden: emitEventIfBrowserIsVisible tests
    // isVisible(), so hiding the component would cut the page off from the
    // very state it is waiting for.
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void PhotoSynthAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++revealTicks;
        // Show the page once it says the patch is on screen — or after eight
        // seconds regardless, so a page that never reports in cannot leave the
        // window stuck on the splash.
        if (processor.uiReady.load() || revealTicks > 240)
        {
            uiRevealed = true;
            resized();
        }
        repaint();          // keeps the sweep moving while we wait
    }
}
