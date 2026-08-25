#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CloneWarsEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit CloneWarsEditor (CloneWarsProcessor&);
    ~CloneWarsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CloneWarsProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CloneWarsEditor)
};
