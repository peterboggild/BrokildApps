#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class TTYAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit TTYAudioProcessorEditor (TTYAudioProcessor&);
    ~TTYAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    TTYAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool webviewMissing = false;
    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TTYAudioProcessorEditor)
};
