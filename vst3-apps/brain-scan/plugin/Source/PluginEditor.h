#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class BrainScanAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit BrainScanAudioProcessorEditor (BrainScanAudioProcessor&);
    ~BrainScanAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    BrainScanAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainScanAudioProcessorEditor)
};
