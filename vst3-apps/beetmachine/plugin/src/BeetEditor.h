#pragma once

// BEETMACHINE - the panel. Eight slot cards across the top; below them, the
// SELECTED slot's own drum panel, exactly the editor that drum shows on its
// own. The whole thing is laid out at one logical size and scaled to the
// window, because the drum panels are big and laptops are not.

#include <JuceHeader.h>
#include "BeetProcessor.h"
#include "BeetLook.h"
#include "BwfxPanel.h"

namespace beetui
{
    constexpr int W = 1400, H = 1044;
    constexpr int HEADER_H = 56;
    constexpr int CARD_Y = 62, CARD_H = 250, CARD_W = 165, CARD_GAP = 8, CARDS_X = 12;
    constexpr int PANEL_Y = 316, PANEL_H = 724;
}

class SlotCard  : public juce::Component
{
public:
    SlotCard (BeetProcessor&, int slot);
    ~SlotCard() override;

    void refresh();          // pull everything from the processor
    void tick();             // 30 Hz: lamp, learn, preset name
    void setSelected (bool);

    std::function<void (int)> onSelect;
    std::function<void (int)> onTypeChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void rebuildPresets();
    void refreshChokes();
    void refreshOuts();

    BeetProcessor& p;
    const int s;
    bool selected = false;

    juce::ComboBox type, preset;
    juce::TextButton prev { "<" }, next { ">" }, hit { "HIT" }, learn { "LEARN" },
                     noteDown { "-" }, noteUp { "+" }, mute { "M" }, solo { "S" };
    juce::Slider level, pan;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt, panAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt;
    std::array<juce::TextButton, beet::NUM_SLOTS> choke;
    std::array<juce::TextButton, beet::NUM_OUT_MODES> out;

    int shownType = -2;
    int lastHits = 0;
    float lamp = 0.0f;
    bool learning = false, warnOut = false;
};

class BeetEditor  : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit BeetEditor (BeetProcessor&);
    ~BeetEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    //  for the snapshot harness
    void selectSlot (int s);
    int  selectedSlot() const { return selected; }
    juce::Component& contentComponent() { return content; }
    void openRack (bool show)            { showRack (show); }
    juce::Component* rackPanel()         { return overlay.get(); }
    juce::Component& rackToggle()        { return rackButton; }

private:
    class Content  : public juce::Component
    {
    public:
        explicit Content (BeetEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        BeetEditor& ed;
    };

    void timerCallback() override;
    void syncChild();
    void releaseChild();
    void refreshHeader();

    BeetProcessor& p;
    BeetLook look;
    Content content { *this };
    juce::TooltipWindow tips { this, 500 };

    juce::ComboBox kit;
    juce::TextButton kitPrev { "<" }, kitNext { ">" }, mapC3 { "C3" }, mapGM { "GM" }, panic { "STOP" };
    juce::Slider master;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAtt;

    //  the BWFX rack on the main mix: ONE button on the machine, and the
    //  STANDARD rack panel (FX left, SPECTRA right, UP/DN, macros) over the
    //  whole window. It is opaque and eats every click, so it carries its
    //  own CLOSE; the button is underneath it.
    juce::TextButton rackButton { "BWFX" };
    std::unique_ptr<BwfxPanel> overlay;
    void showRack (bool);

    std::array<std::unique_ptr<SlotCard>, beet::NUM_SLOTS> cards;
    std::unique_ptr<juce::AudioProcessorEditor> child;
    juce::AudioProcessor* childOwner = nullptr;
    int selected = 0;
    int seenLayout = -1;

    friend class Content;
};
