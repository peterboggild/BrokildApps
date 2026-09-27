#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Nineteen84AudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit Nineteen84AudioProcessorEditor (Nineteen84AudioProcessor&);
    ~Nineteen84AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    Nineteen84AudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool webviewMissing = false;
    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Nineteen84AudioProcessorEditor)
};
