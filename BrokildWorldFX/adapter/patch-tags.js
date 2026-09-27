/*  patchFolder() now takes a LIST of app tags, because a renamed plugin has
    files saved under both names. Update the seven call sites, and give Mars
    Wars its new identity: MARTIAN GAIN.
*/
"use strict";
const fs = require("fs");
const miss = [];
const Q = String.fromCharCode(34);

const CALLS = [
  { root: "PhotoSynth",      was: '"Photo-Synth 2", "\\"photo-synth\\""',
                             now: '"Photo-Synth 2", { "\\"photo-synth\\"" }' },
  { root: "EscapeRoom",      was: '"Escape Room", "\\"escape-room\\""',
                             now: '"Escape Room", { "\\"escape-room\\"" }' },
  { root: "BladeRuiner",     was: '"Blade Ruiner", "\\"blade-ruiner\\""',
                             now: '"Blade Ruiner", { "\\"blade-ruiner\\"" }' },
  { root: "BlackRider",      was: '"Black Rider", "\\"blackrider\\""',
                             now: '"Black Rider", { "\\"blackrider\\"" }' },
  { root: "Hairfryer",       was: '"Hairfryer", "\\"hairfryer\\""',
                             now: '"Hairfryer", { "\\"hairfryer\\"" }' },
  { root: "FullMetalRacket", was: '"Full Metal Racket", "", "*.fmrkit"',
                             now: '"Full Metal Racket", {}, "*.fmrkit"' },
  //  the rename: new folder, both tags, and the old folder named so nothing
  //  saved as "The Mars Wars" is stranded
  { root: "MarsWars",        was: '"The Mars Wars", "\\"mars-wars\\""',
                             now: '"Martian Gain", { "\\"martian-gain\\"", "\\"mars-wars\\"" },\n'
                                + '                                   "*.json", { "The Mars Wars" }' }
];

for (const c of CALLS) {
  const P = "C:/Users/peter/b/" + c.root + "/Source/PluginProcessor.cpp";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const was = c.was, now = c.now.split(String.fromCharCode(10)).join(NL);
  const n = s.split(was).length - 1;
  if (n !== 1) { miss.push(c.root + ": call site x" + n); continue; }
  fs.writeFileSync(P, s.split(was).join(now));
  console.log("  " + c.root);
}

//  Clone Wars, in the website repo
{
  const P = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/clone-wars/plugin/Source/PluginProcessor.cpp";
  let s = fs.readFileSync(P, "utf8");
  const was = '"Clone Wars", "", "slot-*.json"';
  const now = '"Clone Wars", {}, "slot-*.json"';
  const n = s.split(was).length - 1;
  if (n !== 1) miss.push("CloneWars: call site x" + n);
  else { fs.writeFileSync(P, s.split(was).join(now)); console.log("  CloneWars"); }
}

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("call sites updated; Mars Wars folder is now Martian Gain");
