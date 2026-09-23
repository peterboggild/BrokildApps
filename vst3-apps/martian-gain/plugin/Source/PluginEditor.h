#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MarsWarsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit MarsWarsAudioProcessorEditor (MarsWarsAudioProcessor&);
    ~MarsWarsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    MarsWarsAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarsWarsAudioProcessorEditor)
};
