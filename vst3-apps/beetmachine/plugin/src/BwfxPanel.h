#pragma once

// BEETMACHINE — the Brokild World FX rack, drawn natively. This file is Rite of
// Passage's panel (the one written from the standard fragment), copied whole;
// only the two sentences that name the host differ. Legion carries the same.
//
// Peter, 2026-09-18: "i dont think the BWFX are there - there is an empty
// panel apart from the macro buttons. Remember to use the original, look of
// the BWFX stack... for consistency", and then the correction that matters
// most: "LEGION also dont show the standard BWFX panel with FX to the left
// and SPECTRA to the right, and with FX that can be reordered".
//
// He is right twice. My first BWFX face showed the five macros and nothing
// else — the rack's own modules were never on it. And Legion's native rack,
// which I was about to copy, is a FLAT LIST of every module type with no
// order and no SPECTRA at all, so copying it would have shipped the same gap
// a second time.
//
// THE STANDARD PANEL is `BrokildWorldFX/ui/bwfx-rack.js`, and this is a
// native port of THAT, laid out from the fragment's own markup:
//
//     BROKILD WORLD FX          PRESETS [ ]   RACK MIX [====]      CLOSE
//     ----------------------------------------------------------------
//     FX RACK                        |  SPECTRA RACK
//       01  TUBE      warmth         |    [o] TAPE SEANCE
//       02  ECHO      repeats        |    [o] DARK DRONE
//       ...            up/down       |    ...
//     ----------------------------------------------------------------
//     MACROS   [1] [2] [3] [4] [5]
//
// Everything is GENERATED from `bwfx::moduleDescriptor()` and
// `bwfx::characterDescriptor()`, never typed: that is the whole point of
// self-describing modules, and a module added to BWFX appears here on the
// next rebuild with its own name, ranges and units.
//
// REORDERING IS UP/DOWN BUTTONS, and that is not a compromise — the fragment
// has `data-up` and `data-down` on every pedal beside the drag grip, so this
// is the original's own second mechanism rather than a substitute for it.

#include <JuceHeader.h>

#include <bwfx.h>

class BwfxPanel : public juce::Component
{
public:
    BwfxPanel (bwfx::Rack&, juce::AudioProcessorValueTreeState&);
    ~BwfxPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onClose;

    //  the panel is opaque and eats every click, so the button that opened it
    //  cannot be reached underneath — it carries its own way out
    void refreshFromRack();

    juce::Image ground;

private:
    void rebuild();
    void moveModule (int type, int delta);

    bwfx::Rack& rack;
    juce::AudioProcessorValueTreeState& apvts;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;

    //  one card per FX module, in CHAIN ORDER
    struct ModuleCard
    {
        int type = 0;
        juce::Label index, name, sub;
        juce::ToggleButton power;
        juce::TextButton up { "UP" }, down { "DN" };
        juce::Slider presence;
        juce::Label presenceLabel;
        std::vector<std::unique_ptr<juce::Slider>> knobs;
        std::vector<std::unique_ptr<juce::Label>>  knobLabels;
    };

    //  one card per SPECTRA character
    struct CharCard
    {
        int idx = 0;
        juce::Label name, sub;
        juce::ToggleButton arm;
        juce::Slider presence;
        juce::Label presenceLabel;
        std::vector<std::unique_ptr<juce::Slider>> knobs;
        std::vector<std::unique_ptr<juce::Label>>  knobLabels;
    };

    struct Macro
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttach> attach;
    };

    juce::Label   title, subtitle, fxHead, specHead, foot, busNote;
    juce::ComboBox presets;
    juce::Label   presetLabel;
    juce::Slider  mix;
    juce::Label   mixLabel;
    juce::TextButton close { "CLOSE" };

    juce::Viewport  view;
    juce::Component board;                 // the two columns live in here

    std::vector<std::unique_ptr<ModuleCard>> modules;
    std::vector<std::unique_ptr<CharCard>>   chars;
    std::vector<std::unique_ptr<Macro>>      macros;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BwfxPanel)
};
