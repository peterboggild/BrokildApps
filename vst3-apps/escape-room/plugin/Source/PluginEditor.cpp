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

    juce::WebBrowserComponent::Resource bwfxResource()   // the shared BWFX rack overlay
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::bwfxrack_jsSize);
        std::memcpy (r.data.data(), BinaryData::bwfxrack_js, (size_t) BinaryData::bwfxrack_jsSize);
        r.mimeType = "application/javascript";
        return r;
    }

    juce::WebBrowserComponent::Options buildOptions (EscapeRoomAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                return std::nullopt;
            })
            .withEventListener ("er", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("EscapeRoom").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff0b0b0c))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
EscapeRoomAudioProcessorEditor::EscapeRoomAudioProcessorEditor (EscapeRoomAudioProcessor& p)
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
    setResizeLimits (760, 540, 3000, 2200);
    setSize (1280, 880);
    startTimerHz (30);
}

EscapeRoomAudioProcessorEditor::~EscapeRoomAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The door, painted natively so the window is never blank. WebView2 only
    honours its background colour once its controller exists — which is after
    the very wait this covers — so this has to be JUCE's own drawing. */
void EscapeRoomAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131211), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff060606), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // one bare bulb, guttering
    const float flick = 0.55f + 0.45f * std::sin ((float) ticks * 0.31f)
                              * std::sin ((float) ticks * 0.073f + 1.3f);
    juce::ColourGradient pool (juce::Colour (0xff2a2118).withAlpha (0.55f * flick + 0.2f),
                               b.getCentreX(), b.getCentreY() - b.getHeight() * 0.18f,
                               juce::Colours::transparentBlack,
                               b.getCentreX(), b.getCentreY() + b.getHeight() * 0.5f, true);
    g.setGradientFill (pool);
    g.fillRect (b);

    // the wordmark, stencilled and spaced by hand (JUCE has no letter tracking)
    const float size = juce::jlimit (16.0f, 44.0f, b.getWidth() * 0.034f);
    g.setFont (juce::Font (juce::FontOptions (size).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffd8cdb8).withAlpha (0.55f + 0.45f * flick));
    g.drawText ("E S C A P E   R O O M", getLocalBounds().withTrimmedBottom (proportionOfHeight (0.13f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.53f));

    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff6b6257));
    g.drawText ("F I N D I N G   T H E   W A Y   I N", under.removeFromTop (24),
                juce::Justification::centred, false);

    // six lamps for the six bits of the lock, filling as we wait
    auto row = under.removeFromTop (26);
    const int lit = (ticks / 9) % 7;
    for (int i = 0; i < 6; ++i)
    {
        juce::Rectangle<float> dot ((float) row.getCentreX() - 46.0f + (float) i * 18.0f,
                                    (float) row.getY() + 9.0f, 8.0f, 8.0f);
        g.setColour (i < lit ? juce::Colour (0xffc8632a) : juce::Colour (0xff2a2724));
        g.fillEllipse (dot);
    }

   #ifdef ER_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff474038));
    g.drawText ("BROKILD  \xc2\xb7  BUILD " + juce::String (ER_BUILD_ID), under.removeFromTop (22),
                juce::Justification::centred, false);
   #endif
}

void EscapeRoomAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));
    if (! uiRevealed) paintSplash (g);
}

void EscapeRoomAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible checks isVisible(), so a
    // hidden browser would be cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void EscapeRoomAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        if (processor.uiReady.load() || ticks > 240)   // eight seconds, then show it regardless
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
