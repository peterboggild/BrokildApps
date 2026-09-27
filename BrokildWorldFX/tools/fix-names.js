/*  Make the one plugin that disagreed with itself agree: Photo-Synth ships as
    PRODUCT_NAME "Photo-Synth2" but kept its patches in "Photo-Synth 2". The
    patch folder follows the product name, not the other way round, because
    PRODUCT_NAME is the name the DAW shows and the bundle carries — renaming
    that would rename the installed bundle and orphan it.

    "Photo-Synth 2" is kept as a FORMER name, so anything already in the old
    folder is migrated on first run rather than abandoned.

    Also teaches tools/check-names.js about Clone Wars, whose canonical source
    lives in the website repo (the local tree at b/CloneWars is a build copy).  */
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];

function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  for (const [a, b, tag] of subs) {
    const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); continue; }
    s = s.split(A).join(B);
  }
  return () => fs.writeFileSync(path, s);
}

const Q = String.fromCharCode(92) + '"';       // an escaped quote in C++ source

const wPS = edit("C:/Users/peter/b/PhotoSynth/Source/PluginProcessor.cpp", [
[`    return brokild::patchFolder ("Photo-Synth 2", { "` + Q + `photo-synth` + Q + `" });`,
 `    //  the patch folder is PRODUCT_NAME verbatim; the old spelling is kept as
    //  a former name so existing patches migrate rather than disappear
    return brokild::patchFolder ("Photo-Synth2", { "` + Q + `photo-synth` + Q + `" },
                                 "*.json", { "Photo-Synth 2" });`, "photosynth"]
]);

const wChk = edit("C:/Users/peter/b/BrokildWorldFX/tools/check-names.js", [
[`const DIRS = ["ArtefactB2311", "ArtefactB2311_67", "BlackRider", "BladeRuiner",
              "EscapeRoom", "FullMetalRacket", "Hairfryer", "MarsWars", "PhotoSynth"];`,
 `const DIRS = ["ArtefactB2311", "ArtefactB2311_67", "BlackRider", "BladeRuiner",
              "CloneWars", "EscapeRoom", "FullMetalRacket", "Hairfryer",
              "MarsWars", "PhotoSynth"];`, "dirs"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wPS(); wChk();
console.log("Photo-Synth's patch folder now matches its product name; Clone Wars added to the check");
