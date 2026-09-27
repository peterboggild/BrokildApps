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

    /*  The delivered decals. The artefact's own geometry stays computed — it
        has to, because it is a live cut through a four-dimensional lattice and
        no bitmap could keep being correct as the player travels. What these
        supply is what the facets REFLECT, which until now was nothing. */
    juce::WebBrowserComponent::Resource pngResource (const void* data, int size)
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) size);
        std::memcpy (r.data.data(), data, (size_t) size);
        r.mimeType = "image/png";
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

    juce::WebBrowserComponent::Options buildOptions (ArtefactAudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                if (path == "/img/mc-anodised.png")  return pngResource (BinaryData::mcanodised_png,  BinaryData::mcanodised_pngSize);
                if (path == "/img/mc-mineral.png")   return pngResource (BinaryData::mcmineral_png,   BinaryData::mcmineral_pngSize);
                if (path == "/img/mc-glass.png")     return pngResource (BinaryData::mcglass_png,     BinaryData::mcglass_pngSize);
                if (path == "/img/mc-coldfire.png")  return pngResource (BinaryData::mccoldfire_png,  BinaryData::mccoldfire_pngSize);
                if (path == "/img/tx-striations.png")return pngResource (BinaryData::txstriations_png,BinaryData::txstriations_pngSize);
                if (path == "/img/tx-alu.png")       return pngResource (BinaryData::txalu_png,       BinaryData::txalu_pngSize);
                if (path == "/img/tx-wear.png")      return pngResource (BinaryData::txwear_png,      BinaryData::txwear_pngSize);
                if (path == "/img/tx-corner.png")    return pngResource (BinaryData::txcorner_png,    BinaryData::txcorner_pngSize);
                if (path == "/img/tx-planet.png")    return pngResource (BinaryData::txplanet_png,    BinaryData::txplanet_pngSize);
                return std::nullopt;
            })
            .withEventListener ("ab", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        /*  The standalone gets its own WebView2 profile: WebView2 joins a
            running browser process per user-data folder and refuses one
            started with different arguments, so a standalone left open would
            leave the plugin in a host staring at a white rectangle. */
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("ArtefactB2311_67")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff07080a))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
ArtefactAudioProcessorEditor::ArtefactAudioProcessorEditor (ArtefactAudioProcessor& p)
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
    setResizeLimits (860, 640, 3600, 2400);
    setSize (1280, 880);
    startTimerHz (30);
}

ArtefactAudioProcessorEditor::~ArtefactAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The crate, opening. WebView2 only honours its own background colour once
    its controller exists — after the second or so this covers — so the editor
    paints the wait itself. */
void ArtefactAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f12), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff050607), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    // an octagon drawing itself, one edge at a time
    const float cx = b.getCentreX(), cy = b.getCentreY() - b.getHeight() * 0.04f;
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.18f;
    const float t = juce::jlimit (0.0f, 1.0f, (float) ticks / 70.0f);
    juce::Path oct;
    for (int i = 0; i <= 8; ++i)
    {
        const float a = (float) (i * juce::MathConstants<double>::pi * 0.25 - juce::MathConstants<double>::pi * 0.125);
        const float px = cx + rad * std::cos (a), py = cy + rad * std::sin (a);
        if (i == 0) oct.startNewSubPath (px, py); else oct.lineTo (px, py);
    }
    juce::Path drawn;
    juce::PathFlatteningIterator it (oct);
    oct.createPathWithRoundedCorners (0.0f);
    g.setColour (juce::Colour (0xff2a6f78).withAlpha (0.20f + 0.55f * t));
    g.strokePath (oct, juce::PathStrokeType (1.4f));
    (void) it;

    // the eight ⊥ directions, faint
    g.setColour (juce::Colour (0xff1d4a52).withAlpha (0.35f * t));
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) (i * juce::MathConstants<double>::pi * 0.25);
        g.drawLine (cx, cy, cx + rad * 1.55f * std::cos (a), cy + rad * 1.55f * std::sin (a), 0.6f);
    }

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (13.0f, 22.0f, b.getWidth() * 0.014f))));
    g.setColour (juce::Colour (0xff7fb8bf).withAlpha (0.45f + 0.5f * t));
    g.drawText ("P R O X I M S   C E N T A U R I   B",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.70f)).withHeight (26),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (17.0f, 34.0f, b.getWidth() * 0.023f)).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffd9e6e8));
    g.drawText ("A R T E F A C T   B 2 3 1 1 . 6 7",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.745f)).withHeight (40),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.83f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colour (0xff56666a));
    g.drawText (juce::String ("C O L L E C T I O N   ") + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"))
                + "   U N C R A T I N G", under.removeFromTop (20), juce::Justification::centred, false);
   #ifdef AB_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff3d4b4e));
    g.drawText ("BUILD " + juce::String (AB_BUILD_ID), under.removeFromTop (18), juce::Justification::centred, false);
   #endif
}

void ArtefactAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff07080a));
    if (! uiRevealed) paintSplash (g);
}

void ArtefactAudioProcessorEditor::resized()
{
    // parked off-screen, not hidden: emitEventIfBrowserIsVisible tests
    // isVisible(), so hiding it would cut the page off from the state it waits for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void ArtefactAudioProcessorEditor::timerCallback()
{
    processor.timerService();

    if (! uiRevealed)
    {
        ++ticks;
        if (! processor.uiReady.load() && ticks == 240 && ! retried)
        {
            retried = true;
            browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        }
        if (processor.uiReady.load() || ticks > (retried ? 480 : 240))
        {
            uiRevealed = true;
            resized();
        }
        repaint();
    }
}
