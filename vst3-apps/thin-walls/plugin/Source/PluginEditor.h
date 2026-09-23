#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ThinWallsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit ThinWallsAudioProcessorEditor (ThinWallsAudioProcessor&);
    ~ThinWallsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    ThinWallsAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool webviewMissing = false;   // no WebView2 runtime on this PC: say so instead of showing IE
    juce::String webviewVersion;
    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThinWallsAudioProcessorEditor)
};
