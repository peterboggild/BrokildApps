#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Marmalade120AudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit Marmalade120AudioProcessorEditor (Marmalade120AudioProcessor&);
    ~Marmalade120AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Marmalade120AudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Marmalade120AudioProcessorEditor)
};
