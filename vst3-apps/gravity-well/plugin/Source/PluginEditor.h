#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class GravityWellAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit GravityWellAudioProcessorEditor (GravityWellAudioProcessor&);
    ~GravityWellAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    GravityWellAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    bool retried = false;
    bool webviewMissing = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GravityWellAudioProcessorEditor)
};
