#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class FmrAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit FmrAudioProcessorEditor (FmrAudioProcessor&);
    ~FmrAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    FmrAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    bool uiRevealed = false, retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmrAudioProcessorEditor)
};
