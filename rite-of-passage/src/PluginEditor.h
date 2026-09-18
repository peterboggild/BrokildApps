#pragma once

// RITE OF PASSAGE — the panel.
//
// The threshold, six lanes, and the marker travelling through the gate. The
// decals of assets/rite-decals/BRIEF.md are not delivered yet, so everything
// here is drawn in code against the palette of §12 — which is how it should
// start anyway: the layout has to be right before a material is laid over it.
//
// The one idea: THE PANEL TAKES ON HEAT. At rest it is cold ash and smoke; as
// the marker crosses, each lane catches in turn at its own enter point, so
// the score visibly ignites left to right in the order it was written.

#include <JuceHeader.h>

#include "PluginProcessor.h"

class RiteEditor  : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit RiteEditor (RiteProcessor&);
    ~RiteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildSlotEditor();
    void pushSlotUi (int slot);
    juce::Rectangle<int> laneBounds (int slot) const;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttach> attach;
    };
    std::unique_ptr<Knob> makeKnob (const juce::String& paramId, const juce::String& text);

    RiteProcessor& proc;

    juce::Slider position;
    std::unique_ptr<SliderAttach> positionAttach;
    juce::TextButton arrival { "ARRIVAL" };
    std::unique_ptr<ButtonAttach> arrivalAttach;
    juce::Label title, readout;

    std::vector<std::unique_ptr<Knob>> globals;

    //  one strip per slot
    struct Lane
    {
        juce::ComboBox fx, curve, place, tail;
        juce::ToggleButton on;
        juce::Slider enter, exitS, depth;
    };
    Lane lanes[rop::kSlots];
    int selected = 0;

    //  the A/B editor for the selected slot
    struct ABKnob
    {
        juce::Slider a, b;
        juce::Label name;
    };
    std::vector<std::unique_ptr<ABKnob>> ab;
    juce::Label abHeading;

    bool building = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiteEditor)
};
