/*  Record the round in the design doc and CLAUDE.md. */
"use strict";
const fs = require("fs");
const misses = [];
function edit(file, pairs) {
  let s = fs.readFileSync(file, "utf8");
  const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
  for (const [a0, b0] of pairs) {
    const a = a0.split("\n").join(NL), b = b0.split("\n").join(NL);
    const c = s.split(a).length - 1;
    if (c !== 1) { misses.push(file.split(/[\\/]/).pop() + ": expected 1 of " + JSON.stringify(a0.slice(0, 60)) + ", found " + c); continue; }
    s = s.split(a).join(b);
  }
  return [file, s];
}

const D = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/HIGH-TIDE-DESIGN.md";
const C = "c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/CLAUDE.md";

const d = edit(D, [
  [`*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.1,`,
   `*STATUS 2026-09-04: BUILT AND SHIPPED — build 260904.2,`],
  [`- Not built, deliberately: the 2-D bowl (§3.2), the bifurcation view (§3.4),
  decals (ordered, slot in through the panel's \`--decal-*\` variables), the
  MPE mapping. The mod wheel and channel pressure lift the TIDE.`,
   `- Not built, deliberately: the 2-D bowl (§3.2), the bifurcation view (§3.4),
  decals (ordered, slot in through the panel's \`--decal-*\` variables), the
  MPE mapping. The mod wheel and channel pressure lift the TIDE.

## 13. THE CENTRE LINE — the discovery of the second round (260904.2)

Peter asked for "less chaotic starter sounds ... a safe starting point". The
design as written offered one isochronous family, the linear shear, which is
thin ground for seventeen patches. The way out was already in §2.4 and had not
been read properly. Landau's result is about the WIDTH function alone, so write
a bowl as

    x±(h) = c(h) ± sqrt(2h)

and the width is the reference parabola's **whatever c does**. The bowl's
CENTRE LINE may wander with height however it likes and the pitch does not
move. c is the timbre; the pitch is exact by construction.

- A straight c is the sheared bowl (even harmonics). A **wavy** c gives a wave
  nothing like a sine and still exactly in tune — that is what the starters are
  made of. The only constraint is that the walls stay monotone,
  |c'(h)| < 1/sqrt(2h), which at full scale means |c'| < 1.
- **Morphing along z must blend the CENTRE LINES, not the heights.** The
  average of two isochronous bowls' heights has neither one's width and is not
  isochronous; blending c keeps the whole sweep in tune. That is what makes
  SWEEP LEAD (a filter sweep with no filter) and MORPH PAD possible.
- The construction generalises the design's own claim: TUNE LOCK is the
  c-preserving edit, and the red tension band is |c'| running out of room.

Four measured lessons from the same round:

- **The balls' mutual push is what makes a unison patch drift with the
  strike.** A patch at SPREAD 0.45 drifted 6.8 cents between a brushed and a
  hammered key; at SPREAD 0.20, 0.76 cents. The push was there to stop the
  balls sitting on top of each other, which is DETUNE's job now, so it was
  halved and the drift went to 1.2 cents.
- **A box bowl under heavy friction cannot be held in tune by the servo.** As
  the ball sinks onto the flat floor the period grows without bound and the
  servo runs out of range — CLAV measured 35 cents flat within a second. It is
  built from a wavy centre line instead, which cannot go out of tune at all.
- **ballPeriod is measured in BALL time, so it cannot see DETUNE** — a detuned
  ball's clock scales that time too and its period reads 2π whatever the
  detune. The first version of the detune check measured exactly nothing.
  Detune is heard as BEATING, so the check measures beating: 1.00× envelope
  swing at zero, 21.9× at 40 %.
- **Where a block is placed is part of whether it works.** The hints block was
  appended near the end of the page and \`initHints()\` called from the boot
  line above it; function declarations hoist but \`const\` does not, so the call
  threw on the temporal dead zone and took the whole script with it. Moving the
  block above its first use was the fix. Same family as the missing
  \`</script>\`: only a live page shows it.`]
]);

const c = edit(C, [
  [`- **Not built (deliberately):** the 2-D bowl (design §3.2), the bifurcation view (§3.4), MPE. The mod wheel and channel pressure lift the TIDE.`,
   `- 2026-09-04 **260904.2 — hints on everything, seventeen starters, DETUNE** (Peter: "make a tooltip for each control and button ... a set of less chaotic starter sounds"). 89 hints cover every control, button, tool, stamp, timeline lane and the view; each says what it is AND how it is used; placed BESIDE the control (right of the left rail, left of the right rail, below the header, above the timeline) so it never covers the control or the pointer; HINTS button in the header, remembered. The panel probe fails if one control lacks a hint and checks the popup does not overlap what it explains. STARTERS group (17: SOFT KEYS SUB PLUCK/WIDE/SQUARE BASS, SAW/SQUARE/SWEEP/GLIDE LEAD, ORGAN, ELECTRIC PIANO, BELL, CLAV, SLOW/MORPH PAD, GLASS, DRONE) ahead of the 12 TERRAINS; a fresh instance opens on SOFT KEYS. Bench 283 checks ALL CLEAR.
- **THE CENTRE-LINE DISCOVERY, and it is the reusable idea of this instrument** (design doc §13): Landau's result is about the WIDTH function only, so writing a bowl as \`x± = c(h) ± sqrt(2h)\` means the bowl's CENTRE LINE can wander with height however it likes and THE PITCH DOES NOT MOVE. A wavy c gives a rich wave exactly in tune — that is what all seventeen starters are made of. Morphing along z must blend the CENTRE LINES, not the heights (an average of two isochronous bowls is not isochronous), which is what makes a "filter sweep with no filter" possible.
- **Four measured lessons from the same round:** the balls' mutual push is what makes a unison patch drift with the strike (6.8 c at SPREAD 0.45, 0.76 c at 0.20 — halved once DETUNE existed); a box under heavy friction cannot be servo'd in tune (the flat floor's period grows without bound, 35 c flat within a second); **\`ballPeriod\` is in BALL time so it cannot see DETUNE** (a detuned ball's clock scales that time too — measure the BEATING instead, 1.00x vs 21.9x); and **where a block is placed is part of whether it works** — \`initHints()\` called from the boot line above its own \`const\` threw on the temporal dead zone and killed the whole script (functions hoist, consts do not; only a live page showed it).
- **Two Edit calls were refused mid-round** ("file has not been read yet") and I did not notice, so DETUNE existed in Params, was set by the factory, and never reached the audio — the build succeeded and the bench measured a detune that was not applied. **Read the tool results.** A node patch script is the honest alternative for a file the editor will not touch.
- **Not built (deliberately):** the 2-D bowl (design §3.2), the bifurcation view (§3.4), MPE. The mod wheel and channel pressure lift the TIDE.`]
]);

if (misses.length) { console.log("MISS:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(d[0], d[1], "utf8");
fs.writeFileSync(c[0], c[1], "utf8");
console.log("design doc and CLAUDE.md updated");
