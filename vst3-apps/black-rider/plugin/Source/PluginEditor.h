#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class BlackRiderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit BlackRiderAudioProcessorEditor (BlackRiderAudioProcessor&);
    ~BlackRiderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    BlackRiderAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackRiderAudioProcessorEditor)
};
