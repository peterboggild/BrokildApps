#pragma once

// DRUMEQ - the panel. Native JUCE in the drum family's machine-art parts, so it
// cannot fail to load and the snapshot harness renders the real thing.
//
//   the SOURCE row     which drum this track is; a lamp over each drum shows
//                      where the rest of the session's kit is
//   PRESET / KIT       this drum's presets; the whole kit saved and loaded
//   the EQ display     the curve the engine runs (from the same designs), the
//                      sound before (grey) and after (lit); drag a band's
//                      node, wheel for its width, double-click to flatten
//   the meters         in, gate, gain reduction, out; FIND DRUM
//   the knobs          INPUT, FILTERS, EQ (named for THIS drum), GATE, COMP, OUT

#include <JuceHeader.h>

#include "PluginProcessor.h"

//==============================================================================
class DeLook  : public juce::LookAndFeel_V4
{
public:
    DeLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
};

//==============================================================================
class EqDisplay  : public juce::Component
{
public:
    explicit EqDisplay (DrumEQProcessor& p) : proc (&p) {}
    void setProcessor (DrumEQProcessor& p) { proc = &p; drag = hover = -1; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void updateSpectra();           // from the engine's rings; the editor's timer calls it
    //  MASTER, editing another track: that track's spectra, on log bins
    //  (n = 0: no track on that drum)
    void setLogSpectra (const float* pre, const float* post, int n, double rate);
    juce::String note;              // a line across the plot when there is nothing to hear
    int  curvePixels() const { return lastCurvePixels; }

private:
    juce::Rectangle<float> plot() const;
    float xOf (double hz) const;
    double hzOf (float x) const;
    float yOf (double db) const;
    double dbOf (float y) const;
    //  the nodes: 0..3 bands, 4 the high-pass, 5 the low-pass
    juce::Point<float> nodePos (int i, const de::Params&) const;
    int hit (juce::Point<float>) const;
    void setParam (const char* id, float v);

    DrumEQProcessor* proc;
    std::vector<float> preDb, postDb;
    bool logBins = false;
    double specRate = 0.0;
    int drag = -1, hover = -1;
    juce::String dragId[2];
    int lastCurvePixels = 0;
};

//==============================================================================
class Meters  : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float in = 0, out = 0, gr = 0, gate = 0;           // in/out linear peaks (held), gr/gate dB
};

//==============================================================================
class DrumEQEditor  : public juce::AudioProcessorEditor,
                      private juce::Timer,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrumEQEditor (DrumEQProcessor&);
    ~DrumEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    //  for the snapshot harness
    void refreshNow() { checkView(); syncSource(); refreshPresetBox(); refreshKit(); updateSwitches(); updateSpectra(); repaint(); }
    void clickSource (int s) { srcBtn[s].onClick(); }
    void clickMaster()       { masterBtn.setToggleState (! masterBtn.getToggleState(), juce::dontSendNotification); masterBtn.onClick(); }
    bool showsMaster() const { return masterLook; }
    int  numKnobs() const { return (int) knobs.size(); }
    int  eqCurvePixels() const { return eq.curvePixels(); }

private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
        std::unique_ptr<juce::ParameterAttachment> rangeAttach;       // the band frequencies
        juce::String id;
    };
    struct Section { juce::String title; juce::Colour colour; juce::Rectangle<int> r; };

    Knob& addKnob (const char* id, const char* label, juce::Colour c, bool bipolar = false);
    void attachKnob (Knob&);
    void attachButtons();
    void checkView();               // the master switched drums: bind to the new one
    void updateSpectra();
    DrumEQProcessor& pv() const { return *v; }
    Knob* knobFor (const juce::String& id);
    juce::TextButton& addToggle (juce::TextButton& b, const char* id, juce::Colour c, const juce::String& tip);

    void syncSource();              // band names and ranges follow the drum
    void updateSwitches();          // slope and shape captions, dimmed sections
    void refreshPresetBox();
    void refreshKit();
    void stepPreset (int delta);
    void showKitMenu();
    void saveKit();
    void savePreset();
    void cycleSlope (const char* id);
    void toggleType (const char* id);
    void setStatus (const juce::String& s, juce::Colour c);

    DrumEQProcessor& host;          // this track
    DrumEQProcessor* v;             // what the panel shows: this track, or the drum the MASTER edits
    bool masterLook = false;
    DeLook look;
    juce::TooltipWindow tips { this, 600 };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::TextButton srcBtn[de::kNumSources];
    juce::TextButton polBtn { "POL" }, gateBtn { "ON" }, compBtn { "ON" }, autoBtn { "AUTO REL" }, eqBtn { "IN" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAttach;
    juce::TextButton masterBtn { "MASTER" }, keepBtn { "KEEP TRACK LEVEL" };
    bool remoteFinding = false;
    juce::TextButton hpSlope, lpSlope, b1Type, b4Type;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, savePresetBtn { "SAVE" };
    juce::TextButton kitBtn { "LOAD KIT" }, saveKitBtn { "SAVE KIT" }, linkBtn { "LINK" };
    juce::Label kitLabel, statusLabel;
    juce::TextButton findBtn { "FIND DRUM" };
    std::unique_ptr<juce::FileChooser> chooser;

    EqDisplay eq;
    Meters meters;

    int shownSource = -1;
    juce::String shownPreset, shownKit;
    int presence[de::kNumSources] {};
    std::atomic<bool> paramsMoved { true };
    juce::uint32 statusUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEQEditor)
};
