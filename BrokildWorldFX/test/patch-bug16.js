/*  BWFX BUGLIST 16 — SPECTRA characters must not move the pitch.

    Peter: "tape seances dull, and dark drones drift, changes the pitch —
    that is rarely helpful. Please see if you can avoid the FX changing the
    pitch, unless its a pitch shifter as such."

    Measured, there are two mechanisms and they are not equally guilty:

    * pitchSag is the real offender. Hosts key it to the smoothed GATE, so it
      scoops EVERY note in and droops it out — a single held note included,
      whichever voice it lands on. It is never heard as an effect; it is heard
      as an instrument that will not stay in tune. TAPE shipped at 0.315
      semitones of it and DARK DRONE at 0.25. Both now default to ZERO. The
      knob stays: a user who wants a dying-tape droop can still have one.

    * detuneCents is fanned per voice by the hosts
      (fan = fmod(vi*0.618+0.5,1)*2-1, so voice 0 gets exactly 0), which makes
      it a SPREAD, not an offset. DARK DRONE's CLUSTER is therefore a named
      ensemble width and stays. But its DRIFT wrote detune as well as filter —
      a slow wander of the tuning itself, which is exactly "dark drone's drift
      changes the pitch". DRIFT now wanders the COLOUR only, which is what a
      drifting drone should be.

    TAPE's WOW stays: it is zero-mean (the note comes back), it lives on its
    own WOBBLE knob, and a tape emulation without it is not one. The bench now
    proves it is zero-mean rather than assuming it.

    KEMPER RULE, satisfied for free: Rack::toJson writes every character
    parameter explicitly, so a rack already saved in a patch or a project
    carries its own sag value and is untouched by a change of default. Only a
    fresh rack, and our own built-in presets, follow the new defaults — and
    the two presets that baked a sag (SEANCE, THE SWARM) are the ones that
    demonstrated the complaint, so they lose it too.
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

const wS = edit("C:/Users/peter/b/BrokildWorldFX/modules/bwfx_spectra.cpp", [

//  DARK DRONE: drift wanders the colour, not the tuning
[`        const float c = wc.step (dt, tau, amp);
        const float d = wd.step (dt, tau, amp);

        add.detuneCents += cluster + d * drift * 10.0f;
        add.pitchSag    += sag;                     // hosts key this to the gate
        add.filterMul   *= (float) std::pow (2.0, (double) (c * drift * 1.2f));`,
`        const float c = wc.step (dt, tau, amp);

        /*  CLUSTER is an ensemble WIDTH — hosts fan detuneCents across voices
            (voice 0 gets exactly 0), so it spreads the stack without moving
            the note. DRIFT used to wander detune as well, which is precisely
            "the drone drifts out of tune"; it now wanders the COLOUR only. */
        add.detuneCents += cluster;
        add.pitchSag    += sag;                     // hosts key this to the gate
        add.filterMul   *= (float) std::pow (2.0, (double) (c * drift * 1.2f));`, "dark tick"],

[`    Walk wc { 0xD44Cu }, wd { 0x0DD1u };`,
 `    Walk wc { 0xD44Cu };`, "dark walk"],

//  the defaults: no character bends the note unless asked to
[`        { "sag",     "SAG",        25, 0, 100, 0, "%", nullptr },`,
 `        { "sag",     "SAG",         0, 0, 100, 0, "%", nullptr },   // bends pitch: opt-in`, "dark sag"],

//  the built-in presets that demonstrated the complaint
[`          "\\"spectra\\":{\\"tape\\":{\\"on\\":1,\\"p\\":{\\"wobble\\":60,\\"sag\\":45,\\"dull\\":55}}}}" },`,
 `          "\\"spectra\\":{\\"tape\\":{\\"on\\":1,\\"p\\":{\\"wobble\\":60,\\"sag\\":0,\\"dull\\":55}}}}" },`, "seance"],

[`          "\\"darkdrone\\":{\\"on\\":1,\\"p\\":{\\"cluster\\":30,\\"sag\\":20,\\"drift\\":50,\\"dtime\\":60}}}}" },`,
 `          "\\"darkdrone\\":{\\"on\\":1,\\"p\\":{\\"cluster\\":30,\\"sag\\":0,\\"drift\\":50,\\"dtime\\":60}}}}" },`, "swarm"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wS();

//  TAPE's sag default lives in its own table; it shares the literal shape
//  with other characters, so it is patched by position rather than by text.
{
  const P = "C:/Users/peter/b/BrokildWorldFX/modules/bwfx_spectra.cpp";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
  const i = s.indexOf("TAPE_PARAMS[]");
  if (i < 0) { console.error("ABORT: TAPE_PARAMS not found"); process.exit(1); }
  const j = s.indexOf("};", i);
  const block = s.slice(i, j);
  const A = `{ "sag",     "SAG",        35, 0, 100, 0, "%", nullptr },`;
  if (block.split(A).length - 1 !== 1) {
    console.error("ABORT: tape sag default not found verbatim in TAPE_PARAMS:" + NL + block);
    process.exit(1);
  }
  const B = `{ "sag",     "SAG",         0, 0, 100, 0, "%", nullptr },   // bends pitch: opt-in`;
  s = s.slice(0, i) + block.split(A).join(B) + s.slice(j);
  fs.writeFileSync(P, s);
}
console.log("16: sag defaults to 0 on both characters; drift wanders colour, not tuning");
