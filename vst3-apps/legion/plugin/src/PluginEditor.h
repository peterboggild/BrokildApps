#pragma once

// LEGION — the panel. Deliberately plain for now: this build exists to find
// out whether the harmony sounds like a second singer, and a face gets in
// the way of that question. Everything is built from the same tables the
// parameters are declared in (Params.h) and from BWFX's own descriptors, so
// when the aesthetics do arrive, only the drawing changes.

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "BwfxPanel.h"

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

    //  the build id, on the face of the plugin: the one question a version
    //  number exists to answer by looking
    juce::Label buildLabel;

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

    //  the LEVELLER: its own panel in the globals row, with a gain meter that
    //  shows reduction downward and lift upward from a 0 dB centre line
    juce::ToggleButton levOn { "LEVELLER" };
    std::unique_ptr<ButtonAttach> levOnAttach;
    juce::Label levHint;
    std::vector<std::unique_ptr<Knob>> levKnobs;
    juce::Rectangle<int> levPanel, levMeter;
    float levShown = 0.0f;

    //  the BWFX rack, generated from bwfx::moduleDescriptor()
    juce::TextButton rackButton { "BWFX" };

    /*  The rack is the STANDARD BWFX panel: the FX chain on the left in its
        own order with UP/DN, SPECTRA on the right, presets, rack mix and the
        five macros along the foot -- the same shape as BrokildWorldFX's
        ui/bwfx-rack.js, which is what every WebView synth in the fleet shows.
        Legion used to draw a flat list of every module instead, with no chain
        order and no SPECTRA at all, which is the thing Peter reported. */
    std::unique_ptr<BwfxPanel> overlay;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LegionEditor)
};
