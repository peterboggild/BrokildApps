#pragma once

// MACHINE ART - the shared parts kit of the drum family: Kickstart, Snare
// Tactics, Hats Off and Beetmachine. Raw vintage machine controls - bakelite
// knobs, a palm push-button, riveted plates, label tape - cut from the
// delivery in assets/drum-decals by ingest.ps1 and embedded by
// machine-art.cmake.
//
// One kit, four machines: what differs between the plug-ins is their GROUND
// (the machine's paint) and their NAMEPLATE. Every function falls back to a
// drawn version if its image is missing, so a panel never fails to draw.
//
// Usability rules the kit keeps (each learned on a real panel):
//   - a knob's setting must be readable at panel size: the caller keeps its
//     value arc or scale around the decal knob; the decal only replaces the cap;
//   - pressing is drawn (the delivered "pressed" drawings are not registered
//     with the "up" ones and would make a button jump);
//   - text never sits directly on a textured ground: sections are dark plates.

#include <JuceHeader.h>

namespace machineart
{
    //  an embedded part by its file name (e.g. "knob.png"); invalid if absent
    juce::Image image (const char* file);

    //  the machine's paint, tiled over the area, then darkened by `darken`
    //  (0..1) so the parts on it read
    void drawGround (juce::Graphics&, juce::Rectangle<float>, const char* file, float scale = 0.75f, float darken = 0.15f);

    //  a nameplate at its own proportions, fitted inside `area` and aligned left;
    //  returns the rectangle it actually took (empty if the image is absent)
    juce::Rectangle<float> drawNameplate (juce::Graphics&, juce::Rectangle<float> area, const char* file);

    //  a section: a dark recessed plate with a steel edge and four rivets
    void drawPlate (juce::Graphics&, juce::Rectangle<float>, float corner = 8.0f, float alpha = 0.88f);

    //  a bakelite knob decal ("knob.png", "knob-red.png", "knob-bronze.png"),
    //  pointer drawn straight up, turned to `angle` (radians, 0 = up).
    //  Returns false if the image is absent, so the caller draws its own cap.
    bool drawKnob (juce::Graphics&, juce::Rectangle<float> box, float angle, const char* file = "knob.png", bool enabled = true);

    //  the palm push-button ("hit.png"): sinks and darkens while pressed, and
    //  glows in `glowColour` for `glow` (0..1) after a hit
    bool drawPalmButton (juce::Graphics&, juce::Rectangle<float> box, bool pressed, float glow, juce::Colour glowColour);

    //  a square steel-bezel push button, lit in `onColour` when on
    void drawSteelButton (juce::Graphics&, juce::Rectangle<float>, bool on, juce::Colour onColour,
                          bool over, bool down, bool enabled);

    //  embossed label tape behind a menu or a title
    void drawTape (juce::Graphics&, juce::Rectangle<float>);

    //  a steel bezel around a display (the scope, a meter)
    void drawBezel (juce::Graphics&, juce::Rectangle<float>, float corner = 8.0f);

    void drawRivet (juce::Graphics&, float cx, float cy, float r);

    void drawNineSlice (juce::Graphics&, const juce::Image&, juce::Rectangle<float> dest, int srcBorder, float destBorder);
    void drawTiled (juce::Graphics&, const juce::Image&, juce::Rectangle<float> dest, float scale);
}
