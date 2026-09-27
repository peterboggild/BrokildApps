/*  Every synth routes its bwfx messages through the APVTS-aware overload, so
    the rail's macro fader moves the HOST PARAMETER rather than the rack.

    Without this the rail stored a value in the rack and the very next audio
    block overwrote it from the parameter — the fader moved and nothing
    happened, which is exactly what Peter reported.
*/
"use strict";
const fs = require("fs");
const miss = [];

const FILES = [
  ["C:/Users/peter/b/PhotoSynth/Source/PluginProcessor.cpp", "PhotoSynth"],
  ["C:/Users/peter/b/EscapeRoom/Source/PluginProcessor.cpp", "EscapeRoom"],
  ["C:/Users/peter/b/BladeRuiner/Source/PluginProcessor.cpp", "BladeRuiner"],
  ["C:/Users/peter/b/BlackRider/Source/PluginProcessor.cpp", "BlackRider"],
  ["C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp", "FullMetalRacket"],
  ["C:/Users/peter/b/Hairfryer/Source/PluginProcessor.cpp", "Hairfryer"],
  ["c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/clone-wars/plugin/Source/PluginProcessor.cpp", "CloneWars"]
];

const OLD = "bwfx_juce::handleMessage (bwfxRack, m)";
const NEW = "bwfx_juce::handleMessage (bwfxRack, apvts, m)";

for (const [p, label] of FILES) {
  let s;
  try { s = fs.readFileSync(p, "utf8"); } catch (e) { miss.push(label + ": unreadable"); continue; }
  if (s.indexOf(NEW) >= 0) { console.log("  " + label + " (already)"); continue; }
  const n = s.split(OLD).length - 1;
  if (n !== 1) { miss.push(label + ": handleMessage x" + n); continue; }
  fs.writeFileSync(p, s.split(OLD).join(NEW));
  console.log("  " + label);
}

if (miss.length) { console.error("PROBLEMS:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("the rail moves the parameter, not the rack");
