#pragma once

// RITE OF PASSAGE — the panel.
//
// The threshold, six lanes, and the marker travelling through the gate. The
// twelve decals of assets/rite-decals/BRIEF.md are in as of 260918.2 and are
// embedded through juce_add_binary_data; EVERY ONE IS OPTIONAL. A decal that
// fails to load leaves its procedural drawing in place, so the panel that was
// rendered before they arrived is still the panel underneath, and a bad
// delivery can never take the plugin with it.
//
// The one idea: THE PANEL TAKES ON HEAT. At rest it is cold ash and smoke; as
// the marker crosses, each lane catches in turn at its own enter point, so
// the score visibly ignites left to right in the order it was written.
//
// THE FAULT THIS ROUND EXISTS TO FIX. Peter: "sure the punch in-punch out
// sliders work? dont seem to make so much difference when i change them?"
// They worked. A slot whose A equals its B — which is every slot the moment
// an effect is assigned, deliberately, so that loading one never changes the
// sound — resolves to A at every position, so ENTER, EXIT, DEPTH and CURVE
// are all inert and nothing said so. A lane that cannot travel now says it,
// on the lane and in the editor heading. Working and looking broken is the
// oldest bug in this workspace and it is a PANEL bug, not a DSP one.

#include <JuceHeader.h>

#include "BwfxPanel.h"
#include "PluginProcessor.h"

//  ---------------------------------------------------------------------------
/*  The knobs and the marker are photographs, so JUCE's vector knob has to go.
    A LookAndFeel is the only place that can be done once for every rotary on
    the panel rather than per control. */
class RiteLook : public juce::LookAndFeel_V4
{
public:
    RiteLook();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Image knobLarge, knobSmall, marker;
};

//  ---------------------------------------------------------------------------
/*  A button that can be right-clicked. JUCE's TextButton reports a click and
    nothing about which button did it, and this panel needs the distinction:
    left loads a preset, right stores one over it. */
class PresetButton : public juce::TextButton
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
        juce::TextButton::mouseDown (e);
    }
};

//  ---------------------------------------------------------------------------
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
    void markInertLanes();
    juce::Rectangle<int> laneBounds (int slot) const;

    //  where everything in a lane sits. ONE function, used by the layout, by
    //  the column headings and by the painter, so the three cannot drift.
    struct LaneCells
    {
        juce::Rectangle<int> on, socket, fx, enter, exitS, depth, place, tail, curve;
    };
    static LaneCells cellsFor (juce::Rectangle<int> lane);

    //  a slot with an effect but with A == B on every parameter cannot move,
    //  whatever ENTER, EXIT, DEPTH or CURVE say
    bool slotTravels (int slot) const;

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
    RiteLook look;

    juce::Slider position;
    std::unique_ptr<SliderAttach> positionAttach;
    juce::TextButton arrival { "ARRIVAL" };
    std::unique_ptr<ButtonAttach> arrivalAttach;
    juce::TextButton bwfxButton { "BWFX" };

    //  AUTO TRANSITION: the strip that drives POSITION from the transport
    juce::ToggleButton autoOn, autoArrive;
    juce::ComboBox     autoBars, autoDir;
    juce::Slider       autoStart, autoEnd;
    juce::Label        autoHead, autoReadout;
    void syncAutoUi();
    juce::Label title, readout, build;

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
    juce::Label abHeading, abHint, abA, abB;

    //  the six quick presets: left loads, right stores
    PresetButton presetBtn[RiteProcessor::kQuickPresets];
    juce::Label  presetHead;
    void refreshPresetNames();

    std::unique_ptr<BwfxPanel> overlay;

    //  the decals. An empty Image means "not delivered" and the code draws
    //  what it drew before — never a hole.
    juce::Image dGround, dClay, dAshWear, dLintel, dPost, dSocket, dArrival;
    juce::Image dGlyph[12];

    bool building = false;
    float flash = 0.0f;          // the arrival mark, fading after it fires
    bool  wasArrived = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiteEditor)
};
