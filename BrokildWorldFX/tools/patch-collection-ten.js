/*  The collection gains Battlestar Overdrive: nine plugins become ten.
 *
 *  A collection is a DERIVED artefact - it goes stale the moment the fleet
 *  gains or renames a plugin, and it has gone stale twice before without
 *  anyone noticing (it shipped Photo-Synth2 after the rename, and predated
 *  High Tide and Brain Scan).
 *
 *  "nine" is NOT safe to replace blindly. It means the plugin count in some
 *  places, but in others it is Brain Scan's NINE SPECIMENS and Blade Ruiner's
 *  NINE DETUNED SAWS - both of which must not move. And "Eight of the nine
 *  carry the rack" becomes "eight of the ten", because Battlestar is an
 *  effect and deliberately has no BWFX. Every replacement here is anchored on
 *  enough surrounding text to prove which sense it is.
 */
"use strict";
const fs = require("fs");

const miss = [];
function edit(path, pairs) {
  let s = fs.readFileSync(path, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [find, sub] of pairs) {
    const f = Array.isArray(find) ? find.join(NL) : find;
    const r = Array.isArray(sub) ? sub.join(NL) : sub;
    const n = s.split(f).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + n + " matches: " + f.split(NL)[0].trim().slice(0, 48)); continue; }
    s = s.replace(f, r);
  }
  return { path, s };
}

const T = "C:/Users/peter/b/BrokildWorldFX/tools/";

// ---------------------------------------------------------------- the zip ---
const zip = edit(T + "build-collection-zip.ps1", [
  // the plugin list: effects sit at the end, after Martian Gain
  ['  @{ slug = "martian-gain";      name = "Martian Gain" }\n)',
   '  @{ slug = "martian-gain";      name = "Martian Gain" },\n' +
   '  @{ slug = "battlestar-overdrive"; name = "Battlestar Overdrive" }\n)'],

  ["Nine Windows plugins: eight instruments and one effect.",
   "Ten Windows plugins: eight instruments and two effects."],

  // the entry, after Martian Gain's - which is the last one in the list
  [[
'  Martian Gain       a multiband distortion with a patch bay under the',
'                     front panel, and a gain match measured rather than',
'                     modelled - so DRIVE changes the sound without',
'                     changing the level'
  ], [
'  Martian Gain       a multiband distortion with a patch bay under the',
'                     front panel, and a gain match measured rather than',
'                     modelled - so DRIVE changes the sound without',
'                     changing the level',
'  Battlestar Overdrive',
'                     an overdrive, and a tribute to the Copenhagen solo',
'                     act of the same name, built with Max Christensen',
'                     blessing. Eight drive engines on one knob from a',
'                     polite tube to a fold-crush-chaos cascade, and a',
'                     CRT that names the one you are on, goes to warp on',
'                     the seventh and blows up a star on the eighth'
  ]],

  // the install note names which ones are effects
  [['  3. They appear under Brokild. Eight are instruments; Martian Gain is',
    '     an effect.'],
   ['  3. They appear under Brokild. Eight are instruments; Martian Gain and',
    '     Battlestar Overdrive are effects.']],

  // BWFX is in the instruments only, and now there are two effects without it
  ["  BROKILD WORLD FX. Eight of the nine carry the same rack of global",
   "  BROKILD WORLD FX. Eight of the ten carry the same rack of global"],
  ["  in.)",
   "  in, and so does Battlestar Overdrive.)"]
]);

// --------------------------------------------------------------- the page ---
const page = edit(T + "build-collection-page.js", [
  ["/*  The collection page: one download, nine plugins.",
   "/*  The collection page: one download, ten plugins."],

  ["    Moved here from FullMetalRacket/tools on 2026-09-04 and rewritten for nine:",
   "    Moved here from FullMetalRacket/tools on 2026-09-04, rewritten for nine and\n" +
   "    then for ten when Battlestar Overdrive landed:"],

  ['"<title>The Brokild Collection — nine free VST3 plugins | BrokildApps</title>")',
   '"<title>The Brokild Collection — ten free VST3 plugins | BrokildApps</title>")'],

  ['\'<meta name="description" content="Every Brokild plugin in one download: eight instruments and one effect for Windows. Brain Scan, High Tide, Photo Synth, Escape Room, Blade Ruiner, Black Rider, Clone Wars, Full Metal Racket and Martian Gain - with their manuals, their standalones, and the shared world-effects rack that runs inside all of them. Free, no installer, no account." />\')',
   '\'<meta name="description" content="Every Brokild plugin in one download: eight instruments and two effects for Windows. Brain Scan, High Tide, Photo Synth, Escape Room, Blade Ruiner, Black Rider, Clone Wars, Full Metal Racket, Martian Gain and Battlestar Overdrive - with their manuals, their standalones, and the shared world-effects rack that runs inside the instruments. Free, no installer, no account." />\')'],

  ['if (head.indexOf("nine free VST3") < 0) { console.error("ABORT: the title swap missed"); process.exit(1); }',
   'if (head.indexOf("ten free VST3") < 0) { console.error("ABORT: the title swap missed"); process.exit(1); }'],

  // the directory entry
  [[
'   "The front panel comes off. Underneath is a patch bay where any band\'s audio or envelope can drive any other band\'s knobs, or move the crossovers themselves."]',
'];'
  ], [
'   "The front panel comes off. Underneath is a patch bay where any band\'s audio or envelope can drive any other band\'s knobs, or move the crossovers themselves."],',
'  ["Battlestar Overdrive", "battlestar-overdrive", "Effect",',
'   "An overdrive, and a tribute: it is named after Max Christensen\'s Copenhagen solo project and built with his blessing. Eight drive engines on one knob, running from a polite tube to a fold-crush-chaos cascade with no musical justification whatsoever, every one of them level-matched by a table the plugin measures for itself at startup.",',
'   "The CRT is part of the instrument. It names the engine you landed on, speeds its starfield up as you climb the knob, goes to warp on the seventh and blows up a star on the eighth. And watch the fuel."]',
'];'
  ]],

  ["'    <div class=\"kicker\">Nine plugins · Windows · Free</div>',",
   "'    <div class=\"kicker\">Ten plugins · Windows · Free</div>',"],

  ["'        Download all nine (' + MB + ' MB)</a>',",
   "'        Download all ten (' + MB + ' MB)</a>',"],

  ['\'      <img src="img/collection.jpg" alt="The Full Metal Racket panel, one of the nine plugins in the collection">\',',
   '\'      <img src="img/collection.jpg" alt="The Full Metal Racket panel, one of the ten plugins in the collection">\','],

  ["'        Brokild — eight instruments and one effect.</li>',",
   "'        Brokild — eight instruments and two effects.</li>',"],

  ['console.log("collection page written - nine plugins, " + MB + " MB");',
   'console.log("collection page written - ten plugins, " + MB + " MB");']
]);

if (miss.length) {
  console.error("ABORTED - nothing written:");
  miss.forEach(m => console.error("  " + m));
  process.exit(1);
}
[zip, page].forEach(e => fs.writeFileSync(e.path, e.s));
console.log("collection builders now know about Battlestar Overdrive: nine -> ten");
