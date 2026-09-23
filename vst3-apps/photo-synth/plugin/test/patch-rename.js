/*  Photo-Synth2 -> "Photo Synth", everywhere a user can see it.

    Peter: "I prefer Photo Synth throughout, without the '2'."

    Following the Martian Gain precedent exactly, because that rename is the
    one that went well:

    * PLUGIN_CODE stays `Psy2`. JUCE builds the VST3 class id from the
      manufacturer and plugin codes, never from the display name, so the id is
      unchanged and every existing project keeps working while showing the new
      name. NEVER change PLUGIN_CODE on a rename.
    * Deliberately NOT renamed: the C++ class, the CMake target, the folder
      `b/PhotoSynth`, and the settings file (`%APPDATA%\Brokild\
      Photo-Synth2.settings`, via PropertiesFile::applicationName) — that last
      one holds the remembered preset folder and the WebView2 profile path, so
      renaming it silently resets the user's UI for no visible gain. Churn
      with real cost, invisible to anyone.
    * The patch folder follows PRODUCT_NAME (tools/check-names.js enforces
      that), with BOTH old spellings kept as former names so anything already
      saved migrates instead of being orphaned.

    No collision: the ChatGPT-built "Photo Synth.vst3" that used to sit at the
    VST3 root is already in Quarantine, and carries plugin code `Psyn`, so
    even its class id differs from ours.
*/
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
const Q = String.fromCharCode(92) + '"';

const wC = edit("C:/Users/peter/b/PhotoSynth/CMakeLists.txt", [
[`    PRODUCT_NAME "Photo-Synth2"`,
 `    PRODUCT_NAME "Photo Synth"`, "product"]
]);

const wP = edit("C:/Users/peter/b/PhotoSynth/Source/PluginProcessor.cpp", [
[`    /*  Documents/Brokild patches/Photo-Synth 2/ — see brokild_paths.h.`,
 `    /*  Documents/Brokild patches/Photo Synth/ — see brokild_paths.h.`, "comment"],
[`    return brokild::patchFolder ("Photo-Synth2", { "` + Q + `photo-synth` + Q + `" },
                                 "*.json", { "Photo-Synth 2" });`,
 `    return brokild::patchFolder ("Photo Synth", { "` + Q + `photo-synth` + Q + `" },
                                 "*.json", { "Photo-Synth2", "Photo-Synth 2" });`, "patchfolder"],
[`                                   .getChildFile ("Photo-Synth2 Presets");`,
 `                                   .getChildFile ("Photo Synth Presets");`, "fallback"]
]);

const wU = edit("C:/Users/peter/b/PhotoSynth/Source/ui/ui.html", [
[`<title>Photo-Synth2</title>`, `<title>Photo Synth</title>`, "title"],
[`  <h1>Photo-Synth2</h1>`, `  <h1>Photo Synth</h1>`, "h1"],
[`title="About Photo-Synth2"`, `title="About Photo Synth"`, "about btn"],
[`<footer>Photo-Synth2 — native plugin edition; no photo or sound leaves the device.</footer>`,
 `<footer>Photo Synth — native plugin edition; no photo or sound leaves the device.</footer>`, "footer"],
[`>Photo-Synth 2</h2>`, `>Photo Synth</h2>`, "about h2"],
[`    if (h) h.title = "Photo-Synth2 · build " + id;`,
 `    if (h) h.title = "Photo Synth · build " + id;`, "tooltip"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wC(); wP(); wU();
console.log("plugin renamed to Photo Synth (PLUGIN_CODE Psy2 untouched)");
