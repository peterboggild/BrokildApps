#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PluginEditorTemplate : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit PluginEditorTemplate (PluginProcessorTemplate&);
    ~PluginEditorTemplate() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessorTemplate& processor;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorTemplate)
};
