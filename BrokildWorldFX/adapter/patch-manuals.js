/*  Four manuals still describe a preset scheme that no longer exists — a
    folder beside the installed .vst3. That is not a wording nit: it tells a
    reader to keep their work in the one place an installer can delete, which
    is exactly how two of Peter's kits were lost. Each gets the real story,
    in its own manual's voice.

    Written as a file rather than passed to bash: it is full of backslashes.
*/
"use strict";
const fs = require("fs");
const miss = [];

function edit(path, was, now, tag) {
  let s;
  try { s = fs.readFileSync(path, "utf8"); }
  catch (e) { miss.push(tag + ": unreadable"); return; }
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = was.split(String.fromCharCode(10)).join(NL);
  const B = now.split(String.fromCharCode(10)).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return; }
  fs.writeFileSync(path, s.split(A).join(B));
  console.log("  " + tag);
}

const HOUSE = "Documents\\Brokild patches";

// ── Photo-Synth 2 ──────────────────────────────────────────────────────────
edit("C:/Users/peter/b/PhotoSynth/docs/manual/manual.html",
  "Until you pick one, the plugin uses a folder called <b>User presets</b> sitting beside the installed <code>.vst3</code>, creating it on first use; where that cannot be written to, <code>Documents\\Photo-Synth2 Presets</code> is used instead.",
  "Until you pick one, the plugin uses <code>" + HOUSE + "\\Photo-Synth 2</code>, creating it on first use. Every Brokild plugin keeps its own folder under that same roof. Earlier versions kept presets beside the installed <code>.vst3</code>, which an installer can reach and replace \u2014 anything saved that way is copied across the first time this version runs, and the originals are left where they were.",
  "Photo-Synth 2");

// ── Escape Room ────────────────────────────────────────────────────────────
edit("C:/Users/peter/b/EscapeRoom/docs/manual/manual.html",
  "    <tr><td>Where they go</td><td>A folder called <b>User presets</b> beside the installed\n"
+ "      <code>.vst3</code>, so the library travels with the plugin. Where that cannot be written\n"
+ "      to, <code>Documents\\Escape Room Patches</code> is used instead. Encoding a patch somewhere\n"
+ "      else moves the library there from then on \u2014 though not when you simply file one in a\n"
+ "      sub-folder of the library it already has.</td></tr>",
  "    <tr><td>Where they go</td><td><code>" + HOUSE + "\\Escape Room</code>, where every\n"
+ "      Brokild plugin keeps a folder of its own. Earlier versions kept rooms beside the installed\n"
+ "      <code>.vst3</code>, which an installer can reach and replace; anything saved that way is\n"
+ "      copied across the first time this version runs, and the originals are left where they\n"
+ "      were. Encoding a patch somewhere else moves the library there from then on \u2014 though not\n"
+ "      when you simply file one in a sub-folder of the library it already has.</td></tr>",
  "Escape Room");

// ── Blade Ruiner ───────────────────────────────────────────────────────────
edit("C:/Users/peter/b/BladeRuiner/docs/manual/manual.html",
  "  <p>The folder starts as <code>User presets</code> beside the installed <code>.vst3</code>, so a\n"
+ "  library travels with the plugin. If that folder cannot be written to,\n"
+ "  <code>Documents\\Blade Ruiner Presets</code> is used instead. Saving somewhere else once makes\n"
+ "  that the new folder. The choice is remembered in your own settings rather than in the project, so\n"
+ "  it survives across hosts.</p>",
  "  <p>The folder starts as <code>" + HOUSE + "\\Blade Ruiner</code>, where every Brokild\n"
+ "  plugin keeps one of its own. Earlier versions kept presets beside the installed\n"
+ "  <code>.vst3</code> \u2014 somewhere an installer can reach and replace \u2014 so anything saved that way\n"
+ "  is copied across the first time this version runs, and the originals are left where they were.\n"
+ "  Saving somewhere else once makes that the new folder. The choice is remembered in your own\n"
+ "  settings rather than in the project, so it survives across hosts.</p>",
  "Blade Ruiner");

// ── Hairfryer ──────────────────────────────────────────────────────────────
edit("C:/Users/peter/b/Hairfryer/docs/manual/manual.html",
  "    <p>SAVE / LOAD write ordinary <code>.json</code> files; the PRESETS menu lists the\n"
+ "    preset folder (by default <code>User presets</code> beside the installed plugin,\n"
+ "    falling back to Documents). Presets carry all 62 parameters.</p>",
  "    <p>SAVE / LOAD write ordinary <code>.json</code> files; the PRESETS menu lists the\n"
+ "    preset folder, which is <code>" + HOUSE + "\\Hairfryer</code> \u2014 every Brokild\n"
+ "    plugin keeps one of its own under that roof, out of reach of an installer.\n"
+ "    Presets carry all 62 parameters.</p>",
  "Hairfryer");

if (miss.length) { console.error("PROBLEMS:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("four manuals now describe the folder that actually exists");
