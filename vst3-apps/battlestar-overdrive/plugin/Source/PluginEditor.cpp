#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    /*  The art is embedded, so the page asks for it by name and this hands it
        back. Looked up through BinaryData's own filename table rather than a
        hand-written if-chain, so adding a part to CMakeLists is enough. */
    std::optional<juce::WebBrowserComponent::Resource> namedResource (const juce::String& path)
    {
        juce::String want = path.startsWith ("/") ? path.substring (1) : path;
        if (want.isEmpty() || want == "index.html") want = "ui.html";
        if (want.startsWith ("art/")) want = want.substring (4);

        // BinaryData mangles names: "knobs-chrome.png" -> "knobschrome_png".
        const juce::String mangled = want.replaceCharacter ('-', '_')
                                         .replaceCharacter ('.', '_')
                                         .removeCharacters (" ");

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String original (BinaryData::originalFilenames[i]);
            if (original != want) continue;

            int size = 0;
            const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            if (data == nullptr || size <= 0) break;

            juce::WebBrowserComponent::Resource r;
            r.data.resize ((size_t) size);
            std::memcpy (r.data.data(), data, (size_t) size);

            if (original.endsWithIgnoreCase (".html"))      r.mimeType = "text/html";
            else if (original.endsWithIgnoreCase (".jpg"))  r.mimeType = "image/jpeg";
            else if (original.endsWithIgnoreCase (".png"))  r.mimeType = "image/png";
            else                                            r.mimeType = "application/octet-stream";
            return r;
        }
        juce::ignoreUnused (mangled);
        return std::nullopt;
    }

    juce::WebBrowserComponent::Options buildOptions (BattlestarOverdriveAudioProcessor& p)
    {
        using WBO = juce::WebBrowserComponent::Options;

        auto options = WBO{}
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([] (const juce::String& path) { return namedResource (path); })
            .withEventListener ("bo", [&p] (juce::var payload) { p.handleUiMessage (payload); });

       #if JUCE_WINDOWS
        // Its own profile folder. A WebView2 joins a RUNNING browser process per
        // user-data folder and refuses one started with different arguments, so
        // a dev standalone holding a shared profile open can blank the VST3's
        // panel in a host - which is what happened to Black Rider.
        auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("Brokild")
                            .getChildFile ("BattlestarOverdrive")
                            .getChildFile ("WebView2");
        userData.createDirectory();
        options = options.withBackend (WBO::Backend::webview2)
                         .withWinWebView2Options (WBO::WinWebView2{}
                             .withStatusBarDisabled()
                             .withBuiltInErrorPageDisabled()
                             .withBackgroundColour (juce::Colour (0xff07050a))
                             .withUserDataFolder (userData));
       #endif

        return options;
    }
}

//==============================================================================
BattlestarOverdriveAudioProcessorEditor::BattlestarOverdriveAudioProcessorEditor (
        BattlestarOverdriveAudioProcessor& p)
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

    // 3:2, because the panel is.
    setResizable (true, true);
    setResizeLimits (720, 480, 3072, 2048);
    setSize (1200, 800);
    startTimerHz (30);
}

BattlestarOverdriveAudioProcessorEditor::~BattlestarOverdriveAudioProcessorEditor()
{
    processor.emitToUi = nullptr;
}

/*  Painted by the editor, because WebView2 only honours the background colour
    its options carry once its controller exists - which is after the second or
    so this is here to cover. */
void BattlestarOverdriveAudioProcessorEditor::paintSplash (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120a16), r.getCentreX(), r.getCentreY(),
                                             juce::Colour (0xff04030a), r.getX(), r.getBottom(), true));
    g.fillRect (r);

    // a few stars, so the wait looks like the outside of a spacecraft
    juce::Random rnd (0x5ca1ab1e);
    for (int i = 0; i < 160; ++i)
    {
        const float x = rnd.nextFloat() * r.getWidth();
        const float y = rnd.nextFloat() * r.getHeight();
        const float a = 0.15f + rnd.nextFloat() * 0.75f;
        const float s = 0.7f + rnd.nextFloat() * 1.6f;
        g.setColour (juce::Colours::white.withAlpha (a * 0.7f));
        g.fillEllipse (x, y, s, s);
    }

    g.setColour (juce::Colour (0xffffb020));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (16.0f, getHeight() * 0.045f)).withStyle ("Bold")));
    g.drawText ("BATTLESTAR OVERDRIVE", getLocalBounds().reduced (24),
                juce::Justification::centred, false);

    g.setColour (juce::Colour (0x88ff7020));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, getHeight() * 0.020f))));
    g.drawText (juce::String ("warming up  ") + JucePlugin_VersionString,
                getLocalBounds().reduced (24).withTrimmedTop (getHeight() * 0.10f),
                juce::Justification::centred, false);
}

void BattlestarOverdriveAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (! uiRevealed) paintSplash (g);
    else              g.fillAll (juce::Colour (0xff07050a));
}

void BattlestarOverdriveAudioProcessorEditor::resized()
{
    if (browser == nullptr) return;

    if (uiRevealed)
        browser->setBounds (getLocalBounds());
    else
        // Parked off-screen, NOT hidden: emitEventIfBrowserIsVisible tests
        // isVisible(), so hiding it would cut the page off from the very state
        // it is waiting for.
        browser->setBounds (getLocalBounds().withPosition (getWidth() + 32, 0));
}

void BattlestarOverdriveAudioProcessorEditor::timerCallback()
{
    if (! uiRevealed)
    {
        ++ticks;
        // revealed when the page says it is up, or after 8 s regardless
        if (processor.uiReady.load() || ticks > 30 * 8)
        {
            uiRevealed = true;
            resized();
            repaint();
        }
    }
}
