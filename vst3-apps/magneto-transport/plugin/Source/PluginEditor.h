#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MagnetoTransportAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit MagnetoTransportAudioProcessorEditor (MagnetoTransportAudioProcessor&);
    ~MagnetoTransportAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    MagnetoTransportAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool webviewMissing = false;   // no WebView2 runtime: say so instead of showing IE's error page
    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagnetoTransportAudioProcessorEditor)
};
