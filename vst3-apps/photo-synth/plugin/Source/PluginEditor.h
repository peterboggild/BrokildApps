#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhotoSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit PhotoSynthAudioProcessorEditor (PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void paintSplash (juce::Graphics&);

    PhotoSynthAudioProcessor& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    // The page is kept off to the side until it says it has the patch on
    // screen, so the window shows this editor's own splash rather than
    // WebView2 starting up.
    bool uiRevealed = false;
    int  revealTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioProcessorEditor)
};
