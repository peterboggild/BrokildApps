#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HighTideAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit HighTideAudioProcessorEditor (HighTideAudioProcessor&);
    ~HighTideAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    HighTideAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HighTideAudioProcessorEditor)
};
