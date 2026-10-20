#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Marmalade50AudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit Marmalade50AudioProcessorEditor (Marmalade50AudioProcessor&);
    ~Marmalade50AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Marmalade50AudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Marmalade50AudioProcessorEditor)
};
