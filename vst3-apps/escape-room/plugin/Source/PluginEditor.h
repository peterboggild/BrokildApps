#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EscapeRoomAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit EscapeRoomAudioProcessorEditor (EscapeRoomAudioProcessor&);
    ~EscapeRoomAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    EscapeRoomAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    // WebView2 needs about a second before the page can draw anything, so the
    // editor paints its own door until the page reports that it is up.
    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EscapeRoomAudioProcessorEditor)
};
