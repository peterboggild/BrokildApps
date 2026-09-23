#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class HairfryerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit HairfryerAudioProcessorEditor (HairfryerAudioProcessor&);
    ~HairfryerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    HairfryerAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairfryerAudioProcessorEditor)
};
