// ABLETON WAS CUTTING THE BOTTOM, AND THE ASPECT LOCK WAS WHY.
//
// Peter asked for the ratio to be locked, so I put a fixed aspect on the
// editor's constrainer. In a plugin that is the wrong place for it: the host
// owns the window. Ableton resizes the editor, JUCE's constrainer overrides
// the height it asked for, and Ableton's frame does not grow to match — so
// the component is taller than the visible area and the bottom is simply
// outside it. That is the cut, and it is not something the page can fix from
// inside, because the page never sees the missing pixels.
//
// The lock is only safe where WE own the window, which is the standalone. In
// a host it comes off: the panel already fits whatever size it is handed —
// measured across five deliberately wrong aspects — so the ratio no longer
// needs enforcing, and enforcing it was the only thing breaking it.
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

const we = edit(R + "Source/PluginEditor.cpp", [
[`    setResizeLimits (1000, 462, 3800, 1900);
    /*  Locked to the deck's own ratio, so the window cannot be dragged into a
        shape the panel then has to letterbox inside. */
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1800.0 / 830.0);
    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open`,
`    setResizeLimits (1000, 462, 3800, 1900);

    /*  The aspect is locked ONLY in the standalone, where we own the window.
        In a plugin the host owns it: Ableton resizes the editor, the
        constrainer overrides the height it asked for, and Ableton's frame
        does not grow to match — so the component ends up taller than the
        visible area and the bottom is outside it. The page cannot fix that
        from inside, because it never sees the missing pixels.

        It costs nothing to drop: the panel fits whatever size it is handed,
        measured across five deliberately wrong aspects, so a host that gives
        an odd shape gets a smaller panel on the machine's own dark ground
        rather than a cut one. */
    if (juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* c = getConstrainer())
            c->setFixedAspectRatio (1800.0 / 830.0);

    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open`, "aspect standalone only"]
]);

//  and make a letterbox look deliberate rather than like a mistake
const wu = edit(R + "Source/ui/ui.html", [
[`html,body{margin:0;height:100%;overflow:hidden;font-family:var(--f-lab);color:var(--ink);
  background:radial-gradient(120% 100% at 50% 0%,#241a10,#0d0905 70%)}`,
`html,body{margin:0;height:100%;overflow:hidden;font-family:var(--f-lab);color:var(--ink);
  /*  The ground a letterbox shows. Deliberately the colour of the bench the
      machine sits on, so an odd window shape reads as space around it rather
      than as something having gone wrong. */
  background:radial-gradient(120% 100% at 50% 40%,#2a1f13,#0b0805 72%)}`, "surround"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
we(); wu();
console.log("aspect locked in the standalone only; the host sizes its own window");
