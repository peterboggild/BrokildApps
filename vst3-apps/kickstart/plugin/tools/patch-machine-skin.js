// Kickstart: the machine-style reskin (DRUMS-BUGLIST item 1, 2026-09-26).
// Layout and geometry are untouched; only the drawing changes. Every anchor
// is checked before anything is written.
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const miss = [];
function patch(rel, pairs) {
  const f = path.join(root, rel);
  let s = fs.readFileSync(f, "utf8");
  const crlf = (s.match(/\r\n/g) || []).length > (s.match(/\n/g) || []).length / 2;
  s = s.replace(/\r\n/g, "\n");
  for (const [a, b] of pairs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(rel + ": " + n + "x " + a.slice(0, 60)); continue; }
    s = s.replace(a, () => b);
  }
  return () => fs.writeFileSync(f, crlf ? s.replace(/\n/g, "\r\n") : s, "utf8");
}

const w1 = patch("src/PluginEditor.cpp", [
  ['#include "PluginEditor.h"\n', '#include "PluginEditor.h"\n#include "MachineArt.h"      // the drum family\'s shared machine parts\n'],

  // knobs: keep the value arc (the setting must stay readable), the decal replaces the cap
  ['    //  the cap: a dark machined disc\n    const float cr = r - 8.0f;',
   '    //  the cap: the bakelite knob decal, turned to the value; the drawn cap\n' +
   '    //  below is the fallback if the art is missing\n' +
   '    {\n' +
   '        const juce::String file = s.getProperties().getWithDefault ("knob", "knob.png").toString();\n' +
   '        const float kd = (r - 5.0f) * 2.0f;\n' +
   '        if (machineart::drawKnob (g, juce::Rectangle<float> (kd, kd).withCentre (c), aVal, file.toRawUTF8(), s.isEnabled()))\n' +
   '            return;\n' +
   '    }\n' +
   '    const float cr = r - 8.0f;'],

  ['    auto r = b.getLocalBounds().toFloat().reduced (0.5f);\n    const bool on = b.getToggleState();\n    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);\n    g.setColour (on ? accent : (down ? kFaint.brighter (0.2f) : (over ? kFaint.brighter (0.1f) : kPanel2)));\n    g.fillRoundedRectangle (r, 4.0f);\n    g.setColour (on ? accent.brighter (0.3f) : kFaint);\n    g.drawRoundedRectangle (r, 4.0f, 1.0f);',
   '    //  a steel-bezel push button, lit in its section colour when on\n' +
   '    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),\n' +
   '                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());'],

  ['    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);\n    g.setColour (kPanel2);\n    g.fillRoundedRectangle (r, 4.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (r, 4.0f, 1.0f);\n    juce::Path tri;',
   '    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);\n' +
   '    machineart::drawTape (g, r);                  // embossed label tape\n' +
   '    juce::Path tri;'],

  // the scope gets a steel bezel
  ['    auto b = getLocalBounds().toFloat();\n    g.setColour (kBack);\n    g.fillRoundedRectangle (b, 8.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);\n\n    auto area',
   '    auto b = getLocalBounds().toFloat();\n' +
   '    g.setColour (kBack);\n' +
   '    g.fillRoundedRectangle (b, 8.0f);\n' +
   '    machineart::drawBezel (g, b, 8.0f);\n\n' +
   '    auto area'],

  // the pad: a riveted plate, and the palm push-button
  ['    auto b = getLocalBounds().toFloat();\n    g.setColour (kPanel);\n    g.fillRoundedRectangle (b, 8.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);\n\n    //  meters on the right',
   '    auto b = getLocalBounds().toFloat();\n' +
   '    machineart::drawPlate (g, b);\n\n' +
   '    //  meters on the right'],
  ['    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.40f;\n    const auto c = b.getCentre().translated (0.0f, -6.0f);\n    juce::ColourGradient face',
   '    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.40f;\n' +
   '    const auto c = b.getCentre().translated (0.0f, -6.0f);\n' +
   '    if (machineart::drawPalmButton (g, juce::Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c), glow > 0.85f, glow, kHot))\n' +
   '    {\n' +
   '        g.setColour (kDim);\n' +
   '        g.setFont (sans (11.5f));\n' +
   '        g.drawText ("click, space or MIDI", juce::Rectangle<float> (b.getX(), c.y + r + 6, b.getWidth(), 16).toNearestInt(),\n' +
   '                    juce::Justification::centred);\n' +
   '        return;\n' +
   '    }\n' +
   '    juce::ColourGradient face'],

  // DRIVE wears the red knob
  ['        if (juce::String (s.unit) == "bi") k->slider.getProperties().set ("bipolar", true);',
   '        if (juce::String (s.unit) == "bi") k->slider.getProperties().set ("bipolar", true);\n' +
   '        if (juce::String (s.id) == "drive") k->slider.getProperties().set ("knob", "knob-red.png");'],

  // the machine: red-lead cast iron, the cast KICKSTART plate, riveted sections
  ['    g.fillAll (kBack);\n\n    //  header\n    auto head = getLocalBounds().removeFromTop (64);\n    juce::ColourGradient hg (kPanel2, 0, 0, kBack, 0, 64, false);\n    g.setGradientFill (hg);\n    g.fillRect (head);\n    g.setColour (kHot);\n    g.fillRect (0, 62, getWidth(), 2);\n',
   '    //  the machine: a steam pile-driver in red-lead primer on cast iron\n' +
   '    machineart::drawGround (g, getLocalBounds().toFloat(), "ground-kick.jpg", 0.75f, 0.32f);\n\n' +
   '    //  header\n' +
   '    g.setColour (juce::Colours::black.withAlpha (0.35f));\n' +
   '    g.fillRect (0, 0, getWidth(), 64);\n' +
   '    g.setColour (kHot);\n' +
   '    g.fillRect (0, 62, getWidth(), 2);\n' +
   '    const auto plate = machineart::drawNameplate (g, { 14.0f, 5.0f, 300.0f, 54.0f }, "plate-kick.png");\n' +
   '    if (! plate.isEmpty())\n' +
   '    {\n' +
   '        g.setColour (kInk.withAlpha (0.7f));\n' +
   '        g.setFont (mono (11.0f));\n' +
   '        g.drawText (juce::String ("BROKILD  ") + KS_BUILD_ID, (int) plate.getRight() + 10, 38, 150, 14, juce::Justification::left);\n' +
   '    }\n' +
   '    else\n' +
   '    {\n'],
  ['    g.drawText (juce::String ("BROKILD  ") + KS_BUILD_ID, 66, 44, 200, 14, juce::Justification::left);\n\n    //  sections\n    for (const auto& s : sections)\n    {\n        g.setColour (kPanel);\n        g.fillRoundedRectangle (s.r.toFloat(), 8.0f);',
   '    g.drawText (juce::String ("BROKILD  ") + KS_BUILD_ID, 66, 44, 200, 14, juce::Justification::left);\n' +
   '    }\n\n' +
   '    //  sections: riveted plates on the machine\n' +
   '    for (const auto& s : sections)\n' +
   '    {\n' +
   '        machineart::drawPlate (g, s.r.toFloat());'],
]);

const w2 = patch("CMakeLists.txt", [
  ['juce_generate_juce_header(Kickstart)\n',
   'juce_generate_juce_header(Kickstart)\n\n' +
   '# the drum family\'s shared machine-style parts (vst3-apps/machine-art)\n' +
   'include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")\n' +
   'brokild_add_machine_art(Kickstart)\n'],
]);

if (miss.length) { console.log("ABORTED, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
w1(); w2();
console.log("patched PluginEditor.cpp and CMakeLists.txt");
