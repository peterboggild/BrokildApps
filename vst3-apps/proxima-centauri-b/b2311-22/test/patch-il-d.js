/*  Three last things, all found by watching the running plugin:

    1. The readout never showed the conversation. updateTags() only runs when
       a parameter moves, and the second body arrives on its own stream — so
       the line said "SPECIMEN 000" while the engine was busy talking. It now
       refreshes when what it displays actually changes.

    2. The second body's glow saturated its own level meter, so the membrane
       filaments were either fully on or fully off. Scaled to breathe.

    3. AN ACCENT BELONGS TO A BODY, NOT TO THE VESSEL. Dialling a new
       specimen was handing the newcomer the scars of the last one — which is
       meaningless, since they are different graphs with different edges. A
       new being now arrives unmarked, while a body restored from a saved
       session keeps everything it earned.
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

const wU = edit("C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html", [
[`  if (s.kin !== undefined) KIN = s.kin;
  if (s.say !== undefined) SAY = s.say;
  if (s.accent !== undefined) ACCENT = s.accent;`,
`  if (s.kin !== undefined) KIN = s.kin;
  if (s.say !== undefined) SAY = s.say;
  if (s.accent !== undefined) ACCENT = s.accent;
  /*  the readout lives on a stream of its own, not on parameter moves —
      without this it reported "SPECIMEN 000" throughout a conversation */
  const sig = (SPEC2 ? SPEC2.cat : -1) + "|" + SAY + "|" + Math.round(KIN * 100)
            + "|" + ACCENT + "|" + (SPEC ? SPEC.cat : -1);
  if (sig !== lastReadout) { lastReadout = sig; updateTags(); }`, "readout refresh"],

[`let KIN = 0, SAY = 0, ACCENT = 0;           // the state of the conversation`,
 `let KIN = 0, SAY = 0, ACCENT = 0;           // the state of the conversation
let lastReadout = "";`, "sig var"],

[`  AUD2 = AUD2 * 0.88 + Math.min(1, __l2 / (LUM2.length * 0.22 + 1)) * 0.12;`,
 `  AUD2 = AUD2 * 0.88 + Math.min(1, __l2 / (LUM2.length * 0.62 + 1)) * 0.12;`, "aud2 scale"],

[`NB.on("other", o => {
  SPEC2 = o;
  LUM2 = new Array(o.nodes.length).fill(0);`,
`NB.on("other", o => {
  SPEC2 = o;
  LUM2 = new Array(o.nodes.length).fill(0);
  lastReadout = "";`, "other resets sig"]
]);

const wH = edit("C:/Users/peter/b/ArtefactB2311/Source/Engine.h", [
[`    void clearAccent();`,
`    void clearAccent();
    /*  a new being arrives unmarked: the scars of the last body mean nothing
        on a different graph. (Restoring a session is not "a new being" — the
        processor only forgets when the catalog number actually changes.) */
    void forgetAccent (bool self, bool other)
    {
        if (self)  { accentA.fill (1.0f); scarA.fill (0.0f); }
        if (other) { accentB.fill (1.0f); scarB.fill (0.0f); }
        accentCount = 0;
    }`, "forget"]
]);

const wP = edit("C:/Users/peter/b/ArtefactB2311/Source/PluginProcessor.cpp", [
[`    if (want != specLoadedUi)
    {
        engine.loadSpecimen (want);`,
`    if (want != specLoadedUi)
    {
        //  a body dialled in by hand is a fresh being; one restored from a
        //  session (specLoadedUi still -1) keeps what it earned
        if (specLoadedUi >= 0) engine.forgetAccent (true, false);
        engine.loadSpecimen (want);`, "forget self"],
[`        if (wantsConv && (wantO != otherLoadedUi || engine.otherLoadedCatalog() < 0))
        {
            engine.loadOther (wantO);`,
`        if (wantsConv && (wantO != otherLoadedUi || engine.otherLoadedCatalog() < 0))
        {
            if (otherLoadedUi >= 0) engine.forgetAccent (false, true);
            engine.loadOther (wantO);`, "forget other"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wU(); wH(); wP();
console.log("D: the readout speaks, the filaments breathe, an accent belongs to a body");
