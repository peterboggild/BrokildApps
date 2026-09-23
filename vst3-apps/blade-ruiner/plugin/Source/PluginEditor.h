#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class BladeRuinerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit BladeRuinerAudioProcessorEditor (BladeRuinerAudioProcessor&);
    ~BladeRuinerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintSplash (juce::Graphics&);

    BladeRuinerAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    bool uiRevealed = false;
    int  ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BladeRuinerAudioProcessorEditor)
};
