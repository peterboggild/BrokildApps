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

    juce::WebBrowserComponent::Options buildOptions (BrainScanAudioProcessor& p)
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
            .withEventListener ("brainscan", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild").getChildFile ("BrainScan")
                            .getChildFile (standalone ? "WebView2-standalone" : "WebView2");
        userData.createDirectory();
        options = options.withBackend (BO::Backend::webview2)
                         .withWinWebView2Options (BO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xffe6e3da))
                             .withUserDataFolder (userData));
       #endif
        return options;
    }
}

//==============================================================================
BrainScanAudioProcessorEditor::BrainScanAudioProcessorEditor (BrainScanAudioProcessor& p)
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
    setResizeLimits (980, 660, 3600, 2400);
    setSize (1300, 860);
    startTimerHz (30);
}

BrainScanAudioProcessorEditor::~BrainScanAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  The console before the screens light. WebView2 paints its own background
    only once its controller exists, so the editor paints the wait itself: the
    off-white enclosure, a dark screen with a gantry ring turning in it and a
    trace running underneath. */
void BrainScanAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xffe6e3da));
    g.fillRect (b);

    const float t = (float) ticks / 30.0f;
    auto scr = b.reduced (b.getWidth() * 0.22f, b.getHeight() * 0.22f);
    g.setColour (juce::Colour (0xff9aa0a6));
    g.fillRoundedRectangle (scr.expanded (14.0f), 10.0f);
    g.setColour (juce::Colour (0xff0d1114));
    g.fillRoundedRectangle (scr, 6.0f);

    //  the gantry ring, turning
    const float cx = scr.getCentreX(), cy = scr.getCentreY() - scr.getHeight() * 0.08f;
    const float r = std::min (scr.getWidth(), scr.getHeight()) * 0.28f;
    g.setColour (juce::Colour (0xff2b6fb3).withAlpha (0.35f));
    g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 3.0f);
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.0f, t * 1.8f, t * 1.8f + 1.4f, true);
    g.setColour (juce::Colour (0xff6fb7ff));
    g.strokePath (arc, juce::PathStrokeType (3.0f));
    g.setColour (juce::Colour (0xffdfe9f2).withAlpha (0.85f));
    g.fillEllipse (cx - r * 0.42f, cy - r * 0.42f, r * 0.84f, r * 0.84f);
    g.setColour (juce::Colour (0xff0d1114));
    g.drawLine (cx, cy - r * 0.42f, cx, cy + r * 0.42f, 1.2f);

    //  the trace
    juce::Path tr;
    const float y0 = scr.getBottom() - scr.getHeight() * 0.18f;
    tr.startNewSubPath (scr.getX() + 16, y0);
    for (int i = 0; i <= 200; ++i)
    {
        const float u = (float) i / 200.0f;
        const float x = scr.getX() + 16 + u * (scr.getWidth() - 32);
        float y = y0;
        const float ph = std::fmod (u * 3.0f + t * 0.5f, 1.0f);
        if (ph > 0.42f && ph < 0.46f) y -= (ph - 0.42f) / 0.04f * 22.0f;
        else if (ph >= 0.46f && ph < 0.50f) y -= (0.50f - ph) / 0.04f * 22.0f;
        else if (ph >= 0.50f && ph < 0.53f) y += 6.0f;
        tr.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xff39ff88));
    g.strokePath (tr, juce::PathStrokeType (1.6f));

    g.setFont (juce::Font (juce::FontOptions ("Segoe UI", juce::jlimit (20.0f, 34.0f, b.getWidth() * 0.024f), juce::Font::bold)));
    g.setColour (juce::Colour (0xff2b2e33));
    g.drawText ("BRAIN SCAN", getLocalBounds().withTrimmedTop ((int) (scr.getY() - 52)).withHeight (40),
                juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions ("Segoe UI", 11.0f, juce::Font::plain)));
    g.setColour (juce::Colour (0xff6a6f75));
    g.drawText ("GANTRY INITIALISING", getLocalBounds().withTrimmedTop ((int) (scr.getBottom() + 14)).withHeight (20),
                juce::Justification::centred, false);
   #ifdef BS_BUILD_ID
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (juce::Colour (0xff9aa0a6));
    g.drawText ("BROKILD  -  BUILD " + juce::String (BS_BUILD_ID), getLocalBounds().withTrimmedTop (getHeight() - 26).withHeight (18),
                juce::Justification::centred, false);
   #endif
}

void BrainScanAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffe6e3da));
    if (! uiRevealed) paintSplash (g);
}

void BrainScanAudioProcessorEditor::resized()
{
    browser->setBounds (uiRevealed ? getLocalBounds()
                                   : getLocalBounds().withPosition (getWidth() + 32, 0));
}

void BrainScanAudioProcessorEditor::timerCallback()
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
