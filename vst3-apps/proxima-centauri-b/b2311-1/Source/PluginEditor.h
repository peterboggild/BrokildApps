#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Artefact1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit Artefact1AudioProcessorEditor (Artefact1AudioProcessor&);
    ~Artefact1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    Artefact1AudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Artefact1AudioProcessorEditor)
};
