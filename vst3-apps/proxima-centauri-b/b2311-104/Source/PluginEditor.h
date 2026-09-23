#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class Artefact104AudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit Artefact104AudioProcessorEditor (Artefact104AudioProcessor&);
    ~Artefact104AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    Artefact104AudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Artefact104AudioProcessorEditor)
};
