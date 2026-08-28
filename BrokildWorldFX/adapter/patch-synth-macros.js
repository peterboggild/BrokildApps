/*  Declare and push the five macro parameters in every synth that carries the
    rack. Two lines each — addMacroParameters() in the layout, pushMacros()
    in processBlock — because the shared adapter does the rest.

    Martian Gain is absent on purpose: it lost the rack when it stopped being
    a synth. Hairfryer is included so it is ready whenever it is next built.
*/
"use strict";
const fs = require("fs");
const miss = [];

/*  Each synth's layout function and its processBlock differ in wording, so
    the anchors are per synth. The bwfxRack member name is the same
    everywhere ("bwfxRack") and so is the APVTS ("apvts"), which is what
    makes the shared helpers possible at all. */
const SYNTHS = [
  { root: "PhotoSynth",       cpp: "Source/PluginProcessor.cpp" },
  { root: "EscapeRoom",       cpp: "Source/PluginProcessor.cpp" },
  { root: "BladeRuiner",      cpp: "Source/PluginProcessor.cpp" },
  { root: "BlackRider",       cpp: "Source/PluginProcessor.cpp" },
  { root: "FullMetalRacket",  cpp: "Source/PluginProcessor.cpp" },
  { root: "Hairfryer",        cpp: "Source/PluginProcessor.cpp" }
];
const CW = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/clone-wars/plugin/Source/PluginProcessor.cpp";

function work(path, label) {
  let s;
  try { s = fs.readFileSync(path, "utf8"); } catch (e) { miss.push(label + ": unreadable"); return; }
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  if (s.indexOf("addMacroParameters") >= 0) { console.log("  " + label + " (already)"); return; }

  //  1 · the layout. Every synth's layout builder ends with "return layout;".
  const A1 = "    return layout;";
  const n1 = s.split(A1).length - 1;
  if (n1 !== 1) { miss.push(label + ": return layout x" + n1); return; }
  s = s.split(A1).join(
    [ "    //  the rack's five automatable macros, declared by shared code so",
      "    //  every synth carries the identical five (see bwfx_juce.h)",
      "    bwfx_juce::addMacroParameters (layout);",
      "    return layout;" ].join(NL));

  //  2 · processBlock. Anchor on the rack call every synth already makes.
  const cands = [
    "    bwfxRack.process (",
    "        bwfxRack.process (",
    "    bwfxRack.setBpm ("
  ];
  let done = false;
  for (const c of cands) {
    const n = s.split(c).length - 1;
    if (n < 1) continue;
    const at = s.indexOf(c);
    const lineStart = s.lastIndexOf(NL, at - 1) + 1;
    const indent = s.slice(lineStart, at);
    const ins = [ indent + "bwfx_juce::pushMacros (bwfxRack, apvts);   // the five host macros",
                  "" ].join(NL) + indent;
    s = s.slice(0, lineStart) + ins + s.slice(lineStart);
    done = true;
    break;
  }
  if (! done) { miss.push(label + ": no rack call in processBlock"); return; }

  fs.writeFileSync(path, s);
  console.log("  " + label);
}

for (const x of SYNTHS) work("C:/Users/peter/b/" + x.root + "/" + x.cpp, x.root);
work(CW, "CloneWars");

if (miss.length) { console.error("PROBLEMS:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("every synth declares and pushes the macros");
