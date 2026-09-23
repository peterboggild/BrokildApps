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

    juce::WebBrowserComponent::Options buildOptions (HairfryerAudioProcessor& p)
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
            .withEventListener ("hf", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("Hairfryer").getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff17120c))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
HairfryerAudioProcessorEditor::HairfryerAudioProcessorEditor (HairfryerAudioProcessor& p)
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
    setSize (1340, 860);
    startTimerHz (30);
}

HairfryerAudioProcessorEditor::~HairfryerAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours its background colour
    once its controller exists — after the second or so this covers. */
void HairfryerAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff241b10), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff120d07), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // a heating coil, warming up
    const float warm = juce::jlimit (0.0f, 1.0f, (float) ticks / 90.0f);
    const float cx = b.getCentreX(), cy = b.getCentreY() + b.getHeight() * 0.08f;
    for (int ring = 0; ring < 3; ++ring)
    {
        const float rr = 46.0f + 26.0f * (float) ring;
        const float glow = warm * (0.8f - 0.2f * (float) ring)
                         * (0.75f + 0.25f * std::sin ((float) ticks * 0.09f + (float) ring));
        g.setColour (juce::Colour (0xffff7a2a).withAlpha (0.10f + 0.55f * glow));
        g.drawEllipse (cx - rr, cy - rr * 0.34f, rr * 2.0f, rr * 0.68f, 5.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (18.0f, 44.0f, b.getWidth() * 0.030f))
                               .withStyle ("Bold")));
    g.setColour (juce::Colour (0xfff3e3c8));
    g.drawText ("H A I R F R Y E R",
                getLocalBounds().withTrimmedBottom (proportionOfHeight (0.34f)),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.72f));
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff9b7d55));
    g.drawText (juce::String ("B R O K I L D   K I T C H E N   A P P L I A N C E S   ")
                + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   P R E H E A T I N G",
                under.removeFromTop (22), juce::Justification::centred, false);

   #ifdef HF_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.setColour (juce::Colour (0xff6b5638));
    g.drawText ("BUILD " + juce::String (HF_BUILD_ID), under.removeFromTop (20),
                juce::Justification::centred, false);
   #endif
}

void HairfryerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17120c));
    if (! uiRevealed) paintSplash (g);
}

void HairfryerAudioProcessorEditor::resized()
{
    // parked, not hidden: emitEventIfBrowserIsVisible tests isVisible(), so a
    // hidden browser would be cut off from the state it is waiting for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void HairfryerAudioProcessorEditor::timerCallback()
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
