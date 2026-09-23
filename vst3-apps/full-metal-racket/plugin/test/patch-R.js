// The machine's descriptor becomes POLYRHYTHMIC BEAT COMPOSER.
//
// Spelled with the h — "polyrythmic" is a typo, and this is silkscreen on a
// nameplate rather than a comment. The subtitle is more than twice as long as
// "Rhythm Composer" was, so the tracking comes in or it runs off the badge.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

const wu = edit(R + "Source/ui/ui.html", [
  ["<div id=\"plate\"><b>Full Metal Racket</b><i>Rhythm Composer</i></div>",
   "<div id=\"plate\"><b>Full Metal Racket</b><i>Polyrhythmic Beat Composer</i></div>", "plate text"],
  // twice the characters, so the tracking has to come in to stay on the badge
  ["#plate i{font:700 8.5px/1 var(--f-lab);letter-spacing:.42em;color:#4d5359;font-style:normal;margin-top:5px}",
   "#plate i{display:block;font:700 8.5px/1 var(--f-lab);letter-spacing:.19em;color:#4d5359;font-style:normal;margin-top:5px;\n  white-space:nowrap}", "plate tracking"],
  // the status line at the top right says what the machine is, too
  ["<div class=\"lab\">Brokild &middot; Twelve Channels</div>",
   "<div class=\"lab\">Brokild &middot; Polyrhythmic Beat Composer</div>", "status line"]
]);

const we = edit(R + "Source/PluginEditor.cpp", [
  ["g.drawText (\"B R O K I L D   \" + dot + \"   R H Y T H M   C O M P O S E R   \" + dot + \"   W A R M I N G   U P\",",
   "g.drawText (\"B R O K I L D   \" + dot + \"   P O L Y R H Y T H M I C   B E A T   C O M P O S E R\",", "splash"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wu(); we();
console.log("Polyrhythmic Beat Composer");
