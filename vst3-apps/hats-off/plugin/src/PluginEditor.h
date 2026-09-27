#pragma once

// HATS OFF - the panel. Native JUCE, drawn entirely in code, the trilogy's
// house style (Kickstart, Snare Tactics).
//
// The top half is the cymbal: the hit is rendered offline by the SAME engine
// every time a control moves and drawn in brass, with its BRIGHTNESS over
// time (the spectral centroid) in steel over it - so BLOOM shows as a curve
// that rises after the stick, and OPEN as a hit that stops. Beside it, the
// pad is a cymbal seen from above: the bell in the middle, the edge outside,
// and where you click is where the stick lands. CLOSED, PEDAL and OPEN below.

#include <JuceHeader.h>

#include "PluginProcessor.h"

class HoLook  : public juce::LookAndFeel_V4
{
public:
    HoLook();
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

class HitDisplay  : public juce::Component
{
public:
    void setHit (std::vector<float> wave, std::vector<float> bright, double seconds, const ho::Params& p, double bpm);
    void paint (juce::Graphics&) override;
private:
    std::vector<float> wave, bright;
    double secs = 0.5, bpm = 120.0;
    ho::Params params;
};

class HitPad  : public juce::Component
{
public:
    //  velocity, articulation, strike (0 bell .. 1 edge)
    std::function<void (float, int, float)> onHit;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void flash() { glow = 1.0f; repaint(); }
    void decay() { if (glow > 0.0f) { glow = std::max (0.0f, glow - 0.08f); repaint(); } }
    float meterPeak = 0.0f, meterGr = 0.0f;
private:
    float glow = 0.0f;
};

class HatsOffEditor  : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit HatsOffEditor (HatsOffProcessor&);
    ~HatsOffEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

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

    HatsOffProcessor& proc;
    HoLook look;
    juce::TooltipWindow tips { this, 500 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::TextButton engineBtn[ho::NUM_DRIVE_ENGINES];
    std::unique_ptr<juce::ParameterAttachment> engineAttach;
    juce::Label engineLabel;

    juce::TextButton chokeBtn { "CHOKE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chokeAttach;

    juce::ComboBox timeBox, keysBox;
    juce::Label timeLabel, keysLabel, bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeAttach, keysAttach;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    std::unique_ptr<juce::FileChooser> chooser;

    HitDisplay display;
    HitPad pad;
    juce::TextButton closedBtn { "CLOSED" }, pedalBtn { "PEDAL" }, openBtn { "OPEN" };

    std::atomic<bool> dirty { true };
    int lastHits = 0;
    int shownProgram = -1;
    juce::String shownName;
    double shownBpm = 0.0;
    int ticks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HatsOffEditor)
};
