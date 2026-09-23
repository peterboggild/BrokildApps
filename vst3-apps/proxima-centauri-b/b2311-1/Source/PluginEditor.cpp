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

    juce::WebBrowserComponent::Options buildOptions (Artefact1AudioProcessor& p)
    {
        using BO = juce::WebBrowserComponent::Options;

        auto options = BO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path)
                                   -> std::optional<juce::WebBrowserComponent::Resource>
            {
                if (path == "/" || path == "/index.html" || path == "/ui.html") return uiResource();
                if (path == "/bwfx-rack.js") return bwfxResource();
                return std::nullopt;
            })
            .withEventListener ("ab1", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        /*  The standalone gets its own WebView2 profile. WebView2 joins a
            running browser process per user-data folder and refuses one started
            with different arguments, so a standalone left open would otherwise
            leave the plug-in in a host staring at a white rectangle. */
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("ArtefactB2311_1")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff08090b))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
Artefact1AudioProcessorEditor::Artefact1AudioProcessorEditor (Artefact1AudioProcessor& p)
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
    setResizeLimits (820, 600, 3600, 2400);
    setSize (1180, 800);
    startTimerHz (30);
}

Artefact1AudioProcessorEditor::~Artefact1AudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The crate, opening. WebView2 only honours its own background colour once its
    controller exists — after the second or so this covers — so the editor paints
    the wait itself. What it paints is a count: this object was found because it
    would not stop counting. */
void Artefact1AudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101112), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff050505), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    const float cx = b.getCentreX(), cy = b.getCentreY() - b.getHeight() * 0.05f;
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.16f;
    const float t = juce::jlimit (0.0f, 1.0f, (float) ticks / 70.0f);

    //  a ring of marks, filling one at a time — the object counting itself in
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) (i * juce::MathConstants<double>::twoPi / 48.0
                                 - juce::MathConstants<double>::halfPi);
        const bool lit = (float) i / 48.0f <= t;
        const float r0 = rad * (lit ? 0.86f : 0.93f), r1 = rad * 1.0f;
        g.setColour (juce::Colour (lit ? 0xffc98a3a : 0xff2a2c2e)
                        .withAlpha (lit ? 0.35f + 0.55f * t : 0.5f));
        g.drawLine (cx + r0 * std::cos (a), cy + r0 * std::sin (a),
                    cx + r1 * std::cos (a), cy + r1 * std::sin (a), lit ? 1.6f : 0.9f);
    }

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (12.0f, 20.0f, b.getWidth() * 0.013f))));
    g.setColour (juce::Colour (0xffb08a5a).withAlpha (0.40f + 0.5f * t));
    g.drawText ("P R O X I M S   C E N T A U R I   B",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.70f)).withHeight (26),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (17.0f, 32.0f, b.getWidth() * 0.022f)).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffe6ded2));
    g.drawText ("A R T E F A C T   B 2 3 1 1 . 1",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.745f)).withHeight (40),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.83f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colour (0xff6a6055));
    g.drawText (juce::String ("A C C E S S I O N   1   ")
                + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   S T I L L   C O U N T I N G",
                under.removeFromTop (20), juce::Justification::centred, false);
   #ifdef AB_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff4b443c));
    g.drawText ("BUILD " + juce::String (AB_BUILD_ID), under.removeFromTop (18),
                juce::Justification::centred, false);
   #endif
}

void Artefact1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff08090b));
    if (! uiRevealed) paintSplash (g);
}

void Artefact1AudioProcessorEditor::resized()
{
    //  parked off-screen, not hidden: emitEventIfBrowserIsVisible tests
    //  isVisible(), so hiding it would cut the page off from the state it waits for
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void Artefact1AudioProcessorEditor::timerCallback()
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
