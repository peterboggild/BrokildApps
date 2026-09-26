#pragma once

// SNARE TACTICS - the panel. Native JUCE, drawn entirely in code (no decals,
// no web view), Kickstart's house style, so it cannot fail to load and the
// snapshot harness can render the real thing.
//
// The top half is the snare itself: the hit is rendered offline by the SAME
// engine every time a control moves and drawn in two layers, the head (warm)
// under the wires (steel), with the head's pitch curve over it - so turning
// TENSION or DECAY shows what it did before anything is hit. Beside it, the
// pad: the head is the snare, the rim ring a rim shot, and three buttons for
// the rim shot, the cross-stick and the clap. The bottom half is the controls.

#include <JuceHeader.h>

#include "PluginProcessor.h"

//==============================================================================
class StLook  : public juce::LookAndFeel_V4
{
public:
    StLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
};

//==============================================================================
class HitDisplay  : public juce::Component
{
public:
    void setHit (std::vector<float> full, std::vector<float> head, double seconds, const st::Params& p, double bpm);
    void paint (juce::Graphics&) override;
private:
    std::vector<float> full, head;
    double secs = 0.5, bpm = 120.0;
    st::Params params;
};

class HitPad  : public juce::Component
{
public:
    //  velocity, articulation
    std::function<void (float, int)> onHit;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void flash() { glow = 1.0f; repaint(); }
    void decay() { if (glow > 0.0f) { glow = std::max (0.0f, glow - 0.12f); repaint(); } }
    float meterPeak = 0.0f, meterGr = 0.0f;
private:
    float glow = 0.0f;
};

//==============================================================================
class SnareTacticsEditor  : public juce::AudioProcessorEditor,
                            private juce::Timer,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SnareTacticsEditor (SnareTacticsProcessor&);
    ~SnareTacticsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    //  for the snapshot harness: render the display now, not on the timer
    void refreshDisplayNow() { rebuildDisplay(); refreshPresetBox(); }
    int  numKnobs() const { return (int) knobs.size(); }
    juce::Rectangle<int> displayBounds() const { return display.getBounds(); }

private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override;
    void rebuildDisplay();
    void refreshPresetBox();
    void stepPreset (int delta);

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        int spec = -1;
    };

    struct Section { juce::String title; juce::Colour colour; juce::Rectangle<int> r; };

    Knob* knobFor (const char* id);
    void  layoutKnobs (juce::Rectangle<int> area, std::initializer_list<const char*> ids);

    SnareTacticsProcessor& proc;
    StLook look;
    juce::TooltipWindow tips { this, 500 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    //  ENGINE: four lit buttons, bound to the choice parameter
    juce::TextButton engineBtn[st::NUM_DRIVE_ENGINES];
    std::unique_ptr<juce::ParameterAttachment> engineAttach;
    juce::Label engineLabel;

    //  the two other choices: the echo's TIME and the KEYS map
    juce::ComboBox timeBox, keysBox;
    juce::Label timeLabel, keysLabel, bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeAttach, keysAttach;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    std::unique_ptr<juce::FileChooser> chooser;

    HitDisplay display;
    HitPad pad;
    juce::TextButton rimBtn { "RIM" }, xstickBtn { "X-STICK" }, clapBtn { "CLAP" };

    std::atomic<bool> dirty { true };
    int lastHits = 0;
    int shownProgram = -1;
    juce::String shownName;
    double shownBpm = 0.0;
    int ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnareTacticsEditor)
};
