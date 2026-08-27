/*  A remembered patch folder that points inside an installed bundle, or into
    Program Files, is not a choice the user made — it is the old default that
    the plugin wrote down for itself. Obeying it would defeat the move to the
    house folder, and it is the exact path that lost Peter two kits.

    Six synths share one wording, Full Metal Racket has its own.
*/
"use strict";
const fs = require("fs");
const miss = [];

const SIX = ["PhotoSynth", "EscapeRoom", "BladeRuiner", "MarsWars", "BlackRider", "Hairfryer"];

for (const name of SIX) {
  const P = "C:/Users/peter/b/" + name + "/Source/PluginProcessor.cpp";
  let t = fs.readFileSync(P, "utf8");
  const NL = t.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);

  const A = [
    "        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved))",
    "            presetFolder = juce::File (saved);"
  ].join(NL);
  const B = [
    "        //  ...unless it is one the plugin wrote down for itself, in a",
    "        //  place an installer replaces. See brokild_paths.h.",
    "        if (saved.isNotEmpty() && juce::File::isAbsolutePath (saved)",
    "            && ! brokild::isUnsafePatchFolder (juce::File (saved)))",
    "            presetFolder = juce::File (saved);"
  ].join(NL);

  const n = t.split(A).length - 1;
  if (n !== 1) { miss.push(name + ": saved-folder branch x" + n); continue; }
  fs.writeFileSync(P, t.split(A).join(B));
  console.log("  " + name);
}

//  Full Metal Racket words it differently
{
  const P = "C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp";
  let t = fs.readFileSync(P, "utf8");
  const NL = t.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = [
    "    const auto remembered = userSettings().getValue (" + '"patchFolder"' + ");",
    "    if (remembered.isNotEmpty() && juce::File (remembered).isDirectory())",
    "        return (presetFolder = juce::File (remembered));"
  ].join(NL);
  const B = [
    "    //  ...unless it is one the plugin wrote down for itself, in a place an",
    "    //  installer replaces — which is how the first two kits were lost.",
    "    const auto remembered = userSettings().getValue (" + '"patchFolder"' + ");",
    "    if (remembered.isNotEmpty() && juce::File (remembered).isDirectory()",
    "        && ! brokild::isUnsafePatchFolder (juce::File (remembered)))",
    "        return (presetFolder = juce::File (remembered));"
  ].join(NL);
  const n = t.split(A).length - 1;
  if (n !== 1) miss.push("FullMetalRacket: remembered branch x" + n);
  else { fs.writeFileSync(P, t.split(A).join(B)); console.log("  FullMetalRacket"); }
}

if (miss.length) { console.error("PROBLEMS:" + String.fromCharCode(10) + "  " + miss.join(String.fromCharCode(10) + "  ")); process.exit(1); }
console.log("stale remembered folders are refused");
