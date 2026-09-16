#pragma once

// LEGION — the panel. Deliberately plain for now: this build exists to find
// out whether the harmony sounds like a second singer, and a face gets in
// the way of that question. Everything is built from the same tables the
// parameters are declared in (Params.h) and from BWFX's own descriptors, so
// when the aesthetics do arrive, only the drawing changes.

#include <JuceHeader.h>

#include "PluginProcessor.h"

class LegionEditor  : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit LegionEditor (LegionProcessor&);
    ~LegionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildRackPanel();
    void layoutRackPanel (juce::Rectangle<int> area);

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using BoxAttach    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttach> attach;
    };

    void addKnob (const juce::String& paramId, const juce::String& text,
                  juce::Component& parent, std::vector<std::unique_ptr<Knob>>& into);

    LegionProcessor& proc;

    juce::Label title, latencyLabel;

    //  globals
    std::vector<std::unique_ptr<Knob>> globalKnobs;
    juce::ComboBox detailBox, rackPosBox;
    juce::Label    detailLabel, rackPosLabel;
    std::unique_ptr<BoxAttach> detailAttach, rackPosAttach;

    //  voices
    struct VoiceStrip
    {
        juce::ToggleButton on;
        std::unique_ptr<ButtonAttach> onAttach;
        juce::Label heading;
        std::vector<std::unique_ptr<Knob>> knobs;
    };
    VoiceStrip strips[legion::kVoices];

    //  the BWFX rack, generated from bwfx::moduleDescriptor()
    juce::TextButton rackButton { "BWFX" };

    //  The rack lives on an OPAQUE, full-bleed overlay. It has to: a Viewport
    //  and a plain Component both paint nothing, so without this the rack's
    //  knobs were drawn straight over the voice strips and both were legible
    //  at once — which is to say neither was.
    struct RackOverlay  : juce::Component
    {
        RackOverlay();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void()> onDismiss;
        juce::Rectangle<int>  card;      //  where the rack itself sits
    };

    RackOverlay      rackOverlay;
    juce::TextButton rackClose { "CLOSE" };
    juce::Label      rackTitle;
    juce::Component  rackPanel;
    juce::Viewport   rackView;
    juce::Slider     rackMix;
    juce::Label      rackMixLabel;

    struct RackModule
    {
        int type = 0;
        juce::ToggleButton power;
        juce::Label name;
        std::vector<std::unique_ptr<juce::Slider>> knobs;
        std::vector<std::unique_ptr<juce::Label>>  knobLabels;
    };
    std::vector<std::unique_ptr<RackModule>> rackModules;
    bool rackOpen = false;

    std::vector<std::unique_ptr<Knob>> macroKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LegionEditor)
};
