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

    juce::WebBrowserComponent::Options buildOptions (MarsWarsAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                return std::nullopt;
            })
            .withEventListener ("mw", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("MarsWars").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff140807))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
MarsWarsAudioProcessorEditor::MarsWarsAudioProcessorEditor (MarsWarsAudioProcessor& p)
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

    setResizable (true, true);
    setResizeLimits (860, 560, 3400, 2400);
    setSize (1440, 940);
    startTimerHz (30);
}

MarsWarsAudioProcessorEditor::~MarsWarsAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours the background colour
    its options carry once its controller exists — which is after the second
    or so this is here to cover. */
void MarsWarsAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0f0b), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff0d0504), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    const float pulse = 0.55f + 0.45f * std::sin ((float) ticks * 0.09f);
    juce::ColourGradient glow (juce::Colour (0xffff3b1f).withAlpha (0.16f * pulse),
                               b.getCentreX(), b.getCentreY(),
                               juce::Colours::transparentBlack,
                               b.getCentreX(), b.getCentreY() + b.getHeight() * 0.5f, true);
    g.setGradientFill (glow);
    g.fillRect (b);

    const float size = juce::jlimit (18.0f, 48.0f, b.getWidth() * 0.036f);
    g.setFont (juce::Font (juce::FontOptions (size).withStyle ("Bold")));
    g.setColour (juce::Colour (0xfff2d7c8));
    g.drawText ("T H E   M A R S   W A R S",
                getLocalBounds().withTrimmedBottom (proportionOfHeight (0.13f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.53f));

    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff8a5a4c));
    g.drawText ("B R I N G I N G   T H E   R E A C T O R   U P", under.removeFromTop (24),
                juce::Justification::centred, false);

    auto row = under.removeFromTop (26);
    const int lit = (ticks / 8) % 6;
    for (int i = 0; i < 5; ++i)
    {
        juce::Rectangle<float> dot ((float) row.getCentreX() - 38.0f + (float) i * 19.0f,
                                    (float) row.getY() + 9.0f, 9.0f, 9.0f);
        g.setColour (i < lit ? juce::Colour (0xffff4a24) : juce::Colour (0xff34211c));
        g.fillEllipse (dot);
    }

   #ifdef MW_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff5c3a31));
    g.drawText ("BROKILD  \xc2\xb7  BUILD " + juce::String (MW_BUILD_ID), under.removeFromTop (22),
                juce::Justification::centred, false);
   #endif
}

void MarsWarsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff140807));
    if (! uiRevealed) paintSplash (g);
}

void MarsWarsAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible tests isVisible(), so a
    // hidden browser would be cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void MarsWarsAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        if (processor.uiReady.load() || ticks > 240)
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
