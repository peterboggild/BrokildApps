/*  THE MARS WARS becomes MARTIAN GAIN, and loses the world rack.

    Two of Peter's calls, both right. "The Mars Wars" reads as a sibling of
    "Clone Wars" and they have nothing to do with each other; and the plugin
    has no war theme in it — it is a multiband distortion, so a name about
    gain says what it is. And BWFX is a rack of effects: putting it inside an
    effect plugin was never the point, since anyone using this already has a
    chain to put things in.

    WHAT DOES NOT CHANGE, deliberately:

      * PLUGIN_CODE stays MrsW. JUCE builds the VST3 class id from the
        manufacturer and plugin codes alone (VST3Interface::jucePluginId) —
        NOT from the display name — so every project that already loads this
        plugin keeps working and simply shows the new name. Changing the code
        would orphan them for no gain at all.
      * the C++ class, the CMake target and the folder in b/ stay MarsWars.
        Renaming those is churn with real risk (build paths, artefact names,
        git history) and not one letter of it is visible to anyone.
      * the settings file keeps its name, so nothing remembered is lost.

    Presets: new files are written with "app": "martian-gain", and the loader
    accepts BOTH tags, so everything saved as a Mars Wars preset still opens.
*/
"use strict";
const fs = require("fs");
const miss = [];
const R = "C:/Users/peter/b/MarsWars/";
const Q = String.fromCharCode(34);

function edit(rel, subs) {
  const P = R + rel;
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  for (const [a, b, tag] of subs) {
    const A = a.split(String.fromCharCode(10)).join(NL);
    const B = b.split(String.fromCharCode(10)).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(rel + ": " + tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(P, s);
}

// ── CMakeLists ─────────────────────────────────────────────────────────────
const wCM = edit("CMakeLists.txt", [
  ['add_subdirectory("${BWFX_DIR}" bwfx)',
   '# NOT built here any more: this is an effect, and BWFX is a rack of effects.\n'
   + '# The adapter folder is still on the include path for brokild_paths.h,\n'
   + '# which is header-only and depends on nothing but juce_core.',
   "no bwfx subdir"],
  ['        Source/ui/ui.html\n        "${BWFX_DIR}/ui/bwfx-rack.js")',
   "        Source/ui/ui.html)", "no rack js"],
  ["        MarsWarsAssets\n        bwfx\n", "        MarsWarsAssets\n", "no bwfx link"],
  ['target_compile_definitions(MarsWars',
   'target_include_directories(MarsWars PRIVATE "${BWFX_DIR}/adapter")\n\n'
   + 'target_compile_definitions(MarsWars', "adapter include"]
]);

// ── the processor header ───────────────────────────────────────────────────
const wPH = edit("Source/PluginProcessor.h", [
  ['#include "Engine.h"\n#include "bwfx.h"', '#include "Engine.h"', "no bwfx.h"],
  ["class MarsWarsAudioProcessor : public juce::AudioProcessor,\n                               private juce::Timer\n{",
   "class MarsWarsAudioProcessor : public juce::AudioProcessor\n{", "no Timer"],
  ["\n    // Brokild World FX - the shared rack, additive and default empty.\n    bwfx::Rack bwfxRack;\n", "\n", "no rack member"],
  ["    void timerCallback() override { bwfxRack.service(); }   // editor open or not\n    void emitBwfx();\n", "", "no timer callback"]
]);

// ── the processor ──────────────────────────────────────────────────────────
const wPC = edit("Source/PluginProcessor.cpp", [
  ['#include "bwfx_juce.h"\n', "", "no bwfx_juce"],
  ["\n    startTimerHz (15);        // bwfxRack.service() - editor open or not\n", "\n", "no startTimer"],
  ["\n    bwfxRack.prepare (sampleRate, juce::jmax (64, samplesPerBlock));", "", "no rack prepare"],
  ["    // host tempo for the world rack's synced modules\n"
   + "    if (auto* ph = getPlayHead())\n"
   + "        if (auto pos = ph->getPosition())\n"
   + "            if (auto bpm = pos->getBpm())\n"
   + "                bwfxRack.setBpm (*bpm);\n\n", "", "no tempo read"],
  ["        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);\n"
   + "        // the world rack: one extra stage after the engine (empty = untouched)\n"
   + "        bwfxRack.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);",
   "        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);", "no rack process a"],
  ["        engine.process (l, tmp.get(), n);\n        bwfxRack.process (l, tmp.get(), n);",
   "        engine.process (l, tmp.get(), n);", "no rack process b"],
  ['    xml->setAttribute ("bwfx", juce::String (bwfxRack.toJson()));\n', "", "no state write"],
  ['            bwfxRack.fromJson (xml->getStringAttribute ("bwfx").toStdString());\n', "", "no state read"],
  ['            xml->removeAttribute ("bwfx");\n', "", "no state strip"],
  ['    else if (k == "bwfx")   { if (bwfx_juce::handleMessage (bwfxRack, m)) emitBwfx(); }\n', "", "no message"],
  ['        emitToUi ("bwfx", bwfx_juce::stateVar (bwfxRack));\n', "", "no emit"],
  ['    o->setProperty ("bwfx", juce::String (bwfxRack.toJson()));   // a patch stores its own rack\n', "", "no patch write"],
  ['    bwfxRack.fromJson (v.getProperty ("bwfx", juce::var ("")).toString().toStdString());\n', "", "no patch read"],
  //  the new identity in the preset files, with the old tag still accepted
  ['    o->setProperty ("app", "mars-wars");',
   '    o->setProperty ("app", "martian-gain");', "app tag out"],
  ['    if (v.getProperty ("app", "").toString() != "mars-wars")',
   '    /*  Renamed from THE MARS WARS: everything saved under the old name\n'
   + '        must still open, so both tags are accepted forever. */\n'
   + '    const auto app = v.getProperty ("app", "").toString();\n'
   + '    if (app != "martian-gain" && app != "mars-wars")', "app tag in"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wCM(); wPH(); wPC();
console.log("BWFX removed; preset tag is martian-gain (mars-wars still accepted)");
