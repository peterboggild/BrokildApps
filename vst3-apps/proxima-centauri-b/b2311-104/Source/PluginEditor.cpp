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

    juce::WebBrowserComponent::Options buildOptions (Artefact104AudioProcessor& p)
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
            .withEventListener ("ab104", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("ArtefactB2311_104")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff05060a))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
Artefact104AudioProcessorEditor::Artefact104AudioProcessorEditor (Artefact104AudioProcessor& p)
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

Artefact104AudioProcessorEditor::~Artefact104AudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The crate, opening. WebView2 paints its own background only once its
    controller exists (a second or so), so the editor paints the wait itself.
    What it paints is a web on a curved ground, warming from cold: the newest
    finding, condensing into view. */
void Artefact104AudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a0c14), b.getCentreX(), b.getY(),
                                             juce::Colour (0xff030308), b.getCentreX(), b.getBottom(), false));
    g.fillRect (b);

    const float cx = b.getCentreX(), cy = b.getCentreY() - b.getHeight() * 0.05f;
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.19f;
    const float t = juce::jlimit (0.0f, 1.0f, (float) ticks / 70.0f);

    //  a ring of arcs, warming into violet — a web on the sphere, condensing
    juce::Random rng (0xB2311104u);
    for (int i = 0; i < 26; ++i)
    {
        const float a0 = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float a1 = a0 + 0.4f + rng.nextFloat() * 1.1f;
        const float rr = rad * (0.35f + 0.65f * rng.nextFloat());
        const bool lit = (float) i / 26.0f <= t;
        juce::Path arc;
        arc.addCentredArc (cx, cy, rr, rr, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (lit ? 0xff8a6bd8 : 0xff20233a)
                        .withAlpha (lit ? 0.30f + 0.50f * t : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (lit ? 1.5f : 0.8f));
    }
    //  the point at infinity
    g.setColour (juce::Colour (0xff05060a));
    g.fillEllipse (cx - 4, cy - 4, 8, 8);

    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (12.0f, 20.0f, b.getWidth() * 0.013f))));
    g.setColour (juce::Colour (0xff7d6fb0).withAlpha (0.40f + 0.5f * t));
    g.drawText ("P R O X I M S   C E N T A U R I   B",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.71f)).withHeight (26),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (17.0f, 32.0f, b.getWidth() * 0.022f)).withStyle ("Bold")));
    g.setColour (juce::Colour (0xffe6e0f2));
    g.drawText ("A R T E F A C T   B 2 3 1 1 . 1 0 4",
                getLocalBounds().withTrimmedTop (proportionOfHeight (0.755f)).withHeight (40),
                juce::Justification::centred, false);

    auto under = getLocalBounds().withTrimmedTop (proportionOfHeight (0.84f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (juce::Colour (0xff58527a));
    g.drawText (juce::String ("A C C E S S I O N   1 0 4   ")
                + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   S T I L L   C A R R Y I N G",
                under.removeFromTop (20), juce::Justification::centred, false);
   #ifdef AB104_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff3d3956));
    g.drawText ("BUILD " + juce::String (AB104_BUILD_ID), under.removeFromTop (18),
                juce::Justification::centred, false);
   #endif
}

void Artefact104AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05060a));
    if (! uiRevealed) paintSplash (g);
}

void Artefact104AudioProcessorEditor::resized()
{
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void Artefact104AudioProcessorEditor::timerCallback()
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
