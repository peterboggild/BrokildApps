/*  ONE PLACE FOR USER PATCHES, ACROSS THE WHOLE FLEET.

    Every synth resolved its own patch folder, and the three schemes had
    drifted apart badly enough to lose work:

      * six synths shared ONE "User presets" folder beside the installed
        bundles, so Escape Room, Black Rider and Mars Wars patches sat mixed
        together in a single list;
      * Clone Wars had its own folder next to it under another name;
      * Full Metal Racket kept its patches INSIDE the .vst3 bundle, which an
        installer deletes and replaces. Two of Peter's kits went that way.

    All of them now answer with brokild::patchFolder(), which is
    Documents/Brokild patches/<Plugin Name>/ and migrates the old locations
    once, by copying. The plugin-side function keeps its name and its
    contract, so nothing downstream changes.

    Written as a file rather than passed to bash -c: it contains backslashes
    and quotes, and bash eats those before node ever sees them.
*/
"use strict";
const fs = require("fs");

const miss = [];

const SYNTHS = [
  { root: "C:/Users/peter/b/PhotoSynth",       cls: "PhotoSynthAudioProcessor", folder: "Photo-Synth 2",     tag: '"photo-synth"' },
  { root: "C:/Users/peter/b/EscapeRoom",       cls: "EscapeRoomAudioProcessor", folder: "Escape Room",       tag: '"escape-room"' },
  { root: "C:/Users/peter/b/BladeRuiner",      cls: "BladeRuinerAudioProcessor",folder: "Blade Ruiner",      tag: '"blade-ruiner"' },
  { root: "C:/Users/peter/b/MarsWars",         cls: "MarsWarsAudioProcessor",   folder: "The Mars Wars",     tag: '"mars-wars"' },
  { root: "C:/Users/peter/b/BlackRider",       cls: "BlackRiderAudioProcessor", folder: "Black Rider",       tag: '"blackrider"' },
  { root: "C:/Users/peter/b/Hairfryer",        cls: "HairfryerAudioProcessor",  folder: "Hairfryer",         tag: '"hairfryer"' },
  { root: "C:/Users/peter/b/FullMetalRacket",  cls: "FmrAudioProcessor",        folder: "Full Metal Racket", tag: "", wild: "*.fmrkit" }
];

for (const s of SYNTHS) {
  const P = s.root + "/Source/PluginProcessor.cpp";
  let t;
  try { t = fs.readFileSync(P, "utf8"); }
  catch (e) { miss.push(s.folder + ": no PluginProcessor.cpp"); continue; }

  const NL = t.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);

  //  the include, once, next to the one it belongs beside
  if (t.indexOf('#include "brokild_paths.h"') < 0) {
    const anchor = '#include "bwfx_juce.h"';
    if (t.split(anchor).length - 1 !== 1) { miss.push(s.folder + ": bwfx_juce include x" + (t.split(anchor).length - 1)); continue; }
    t = t.split(anchor).join(anchor + NL + '#include "brokild_paths.h"');
  }

  //  replace the whole body of installedPresetFolder(). Nothing else in these
  //  files opens a brace in column zero, so the first NL + "}" after the
  //  signature is the end of the function — and juce::File{} inside would
  //  defeat a naive brace count.
  const sig = "juce::File " + s.cls + "::installedPresetFolder()";
  const at = t.indexOf(sig);
  if (at < 0) { miss.push(s.folder + ": no installedPresetFolder"); continue; }
  const open = t.indexOf("{", at);
  const close = t.indexOf(NL + "}", open);
  if (open < 0 || close < 0) { miss.push(s.folder + ": could not bound the body"); continue; }

  const wild = s.wild ? ', "' + s.wild + '"' : "";
  const body = [
    "{",
    "    /*  Documents/Brokild patches/" + s.folder + "/ — see brokild_paths.h.",
    "        It used to be a folder beside the installed bundle, shared with",
    "        every other Brokild plugin; the old contents are migrated once,",
    "        by copying, so nothing there is disturbed. */",
    '    return brokild::patchFolder ("' + s.folder + '", "' + s.tag.split('"').join('\\"') + '"' + wild + ");",
    "}"
  ].join(NL);

  t = t.slice(0, open) + body + t.slice(close + NL.length + 1);
  fs.writeFileSync(P, t);
  console.log("  " + s.folder + " -> Documents/Brokild patches/" + s.folder);
}

if (miss.length) { console.error("PROBLEMS:" + String.fromCharCode(10) + "  " + miss.join(String.fromCharCode(10) + "  ")); process.exit(1); }
console.log("all seven point at the house folder");
