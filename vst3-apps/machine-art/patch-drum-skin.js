// The machine-style reskin for Snare Tactics and Hats Off (DRUMS-BUGLIST
// item 1, 2026-09-26). Kickstart has its own script (its pad became the palm
// button; these two keep their pads, because WHERE you hit them matters:
// head / hoop on the snare, bell / bow / edge on the cymbal).
//
//   node patch-drum-skin.js snare|hats
//
// Layout and geometry are untouched. Every anchor is checked before anything
// is written.
const fs = require("fs");
const path = require("path");

const CFG = {
  snare: { dir: "snare-tactics", target: "SnareTactics", build: "ST_BUILD_ID", ground: "ground-snare.jpg", plate: "plate-snare.png",
           machine: "military field equipment, olive drab" },
  hats:  { dir: "hats-off",      target: "HatsOff",      build: "HO_BUILD_ID", ground: "ground-hats.jpg",  plate: "plate-hats.png",
           machine: "a lathe in a cymbal foundry, machine-tool green" },
}[process.argv[2]];
if (!CFG) { console.log("usage: node patch-drum-skin.js snare|hats"); process.exit(1); }

const root = path.join(__dirname, "..", CFG.dir, "plugin");
const miss = [];
const writers = [];
function patch(rel, fn) {
  const f = path.join(root, rel);
  let s = fs.readFileSync(f, "utf8");
  const crlf = (s.match(/\r\n/g) || []).length > (s.match(/\n/g) || []).length / 2;
  s = s.replace(/\r\n/g, "\n");
  const once = (a, b) => {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(rel + ": " + n + "x " + a.slice(0, 70).replace(/\n/g, "\\n")); return; }
    s = s.replace(a, () => b);
  };
  fn(once, () => s, (v) => { s = v; });
  writers.push(() => fs.writeFileSync(f, crlf ? s.replace(/\n/g, "\r\n") : s, "utf8"));
}

patch("src/PluginEditor.cpp", (once, get, set) => {
  once('#include "PluginEditor.h"\n', '#include "PluginEditor.h"\n#include "MachineArt.h"      // the drum family\'s shared machine parts\n');

  // knobs: the value arc stays (the setting must stay readable); the decal replaces the cap
  once('    const float cr = r - 8.0f;\n    juce::ColourGradient cap (',
       '    {\n' +
       '        //  the cap: the bakelite knob decal, turned to the value; the drawn cap\n' +
       '        //  below is the fallback if the art is missing\n' +
       '        const juce::String file = s.getProperties().getWithDefault ("knob", "knob.png").toString();\n' +
       '        const float kd = (r - 5.0f) * 2.0f;\n' +
       '        if (machineart::drawKnob (g, juce::Rectangle<float> (kd, kd).withCentre (c), aVal, file.toRawUTF8(), s.isEnabled()))\n' +
       '            return;\n' +
       '    }\n' +
       '    const float cr = r - 8.0f;\n    juce::ColourGradient cap (');

  once('    auto r = b.getLocalBounds().toFloat().reduced (0.5f);\n    const bool on = b.getToggleState();\n    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);\n    g.setColour (on ? accent : (down ? kFaint.brighter (0.2f) : (over ? kFaint.brighter (0.1f) : kPanel2)));\n    g.fillRoundedRectangle (r, 4.0f);\n    g.setColour (on ? accent.brighter (0.3f) : kFaint);\n    g.drawRoundedRectangle (r, 4.0f, 1.0f);',
       '    //  a steel-bezel push button, lit in its section colour when on\n' +
       '    machineart::drawSteelButton (g, b.getLocalBounds().toFloat(), b.getToggleState(),\n' +
       '                                 b.findColour (juce::TextButton::buttonOnColourId), over, down, b.isEnabled());');

  once('    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);\n    g.setColour (kPanel2);\n    g.fillRoundedRectangle (r, 4.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (r, 4.0f, 1.0f);\n    juce::Path tri;',
       '    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);\n' +
       '    machineart::drawTape (g, r);                  // embossed label tape\n' +
       '    juce::Path tri;');

  // the scope in a steel bezel
  once('void HitDisplay::paint (juce::Graphics& g)\n{\n    auto b = getLocalBounds().toFloat();\n    g.setColour (kBack);\n    g.fillRoundedRectangle (b, 8.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);',
       'void HitDisplay::paint (juce::Graphics& g)\n{\n    auto b = getLocalBounds().toFloat();\n    g.setColour (kBack);\n    g.fillRoundedRectangle (b, 8.0f);\n    machineart::drawBezel (g, b, 8.0f);');

  // the pad keeps its drawing (its zones mean something); it sits on a riveted plate
  once('void HitPad::paint (juce::Graphics& g)\n{\n    auto b = getLocalBounds().toFloat();\n    g.setColour (kPanel);\n    g.fillRoundedRectangle (b, 8.0f);\n    g.setColour (kFaint);\n    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);',
       'void HitPad::paint (juce::Graphics& g)\n{\n    auto b = getLocalBounds().toFloat();\n    machineart::drawPlate (g, b);');

  // the machine's paint, its nameplate, riveted sections
  once('    g.fillAll (kBack);\n',
       '    //  the machine: ' + CFG.machine + '\n' +
       '    machineart::drawGround (g, getLocalBounds().toFloat(), "' + CFG.ground + '", 0.75f, 0.30f);\n');
  once('    juce::ColourGradient hg (kPanel2, 0, 0, kBack, 0, 64, false);\n    g.setGradientFill (hg);\n    g.fillRect (head);\n',
       '    g.setColour (juce::Colours::black.withAlpha (0.35f));\n    g.fillRect (head);\n');

  // wrap the drawn wordmark: it is now only the fallback for a missing nameplate
  let s = get();
  const startMark = '    g.fillRect (0, 62, getWidth(), 2);\n';
  const endLineStart = '    g.drawText (juce::String ("BROKILD  ") + ' + CFG.build + ', 66, 44, 200, 14, juce::Justification::left);\n';
  const a = s.indexOf(startMark), b = s.indexOf(endLineStart);
  if (a < 0 || b < 0 || b < a || s.indexOf(startMark, a + 1) >= 0) { miss.push("wordmark region not found uniquely"); }
  else {
    const inner = s.slice(a + startMark.length, b + endLineStart.length);
    const wrapped =
      startMark +
      '    const auto plate = machineart::drawNameplate (g, { 14.0f, 4.0f, 320.0f, 56.0f }, "' + CFG.plate + '");\n' +
      '    if (! plate.isEmpty())\n' +
      '    {\n' +
      '        g.setColour (kInk.withAlpha (0.7f));\n' +
      '        g.setFont (mono (11.0f));\n' +
      '        g.drawText (juce::String ("BROKILD  ") + ' + CFG.build + ', (int) plate.getRight() + 10, 38, 150, 14, juce::Justification::left);\n' +
      '    }\n' +
      '    else\n' +
      '    {\n' + inner + '    }\n';
    s = s.slice(0, a) + wrapped + s.slice(b + endLineStart.length);
    set(s);
  }
  once('        g.setColour (kPanel);\n        g.fillRoundedRectangle (s.r.toFloat(), 8.0f);\n        g.setColour (s.colour);',
       '        machineart::drawPlate (g, s.r.toFloat());\n        g.setColour (s.colour);');

  // Hats Off: the METAL section wears bronze
  if (process.argv[2] === "hats")
    once('        if (juce::String (s.unit) == "bi" || juce::String (s.id) == "pitch") k->slider.getProperties().set ("bipolar", true);',
         '        if (juce::String (s.unit) == "bi" || juce::String (s.id) == "pitch") k->slider.getProperties().set ("bipolar", true);\n' +
         '        if (juce::String (s.section) == "METAL") k->slider.getProperties().set ("knob", "knob-bronze.png");\n' +
         '        if (juce::String (s.id) == "drive")      k->slider.getProperties().set ("knob", "knob-red.png");');
  else
    once('        if (juce::String (s.unit) == "bi") k->slider.getProperties().set ("bipolar", true);',
         '        if (juce::String (s.unit) == "bi") k->slider.getProperties().set ("bipolar", true);\n' +
         '        if (juce::String (s.id) == "drive") k->slider.getProperties().set ("knob", "knob-red.png");');
});

patch("CMakeLists.txt", (once) => {
  once('juce_generate_juce_header(' + CFG.target + ')\n',
       'juce_generate_juce_header(' + CFG.target + ')\n\n' +
       '# the drum family\'s shared machine-style parts (vst3-apps/machine-art)\n' +
       'include("${CMAKE_CURRENT_SOURCE_DIR}/../../machine-art/machine-art.cmake")\n' +
       'brokild_add_machine_art(' + CFG.target + ')\n');
});

if (miss.length) { console.log("ABORTED, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
writers.forEach(w => w());
console.log("patched " + CFG.dir);
