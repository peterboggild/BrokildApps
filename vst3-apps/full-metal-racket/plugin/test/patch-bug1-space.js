/*  FMR BUGLIST 1, the SPACE half.

    The panel called preventDefault() on SPACE unconditionally and sent its
    own transport message — which is exactly why space did not start Ableton:
    the plugin ate the key and the host never saw it. Two transports were
    arguing and the plugin always won.

    With the gate in place the fix is to stop being clever. When the host has
    a transport, let the key through: Ableton starts, and FMR follows, because
    it now follows. Only the standalone — or a host offering no transport at
    all — handles SPACE itself, which is the case the spec says to keep.

    The typing guard and the button-blur stay exactly as they were.
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

const SP = "  ";      // the handler body sits one level in

const wP = edit("C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp", [
[`        obj->setProperty ("step", engine.seqStep());
        obj->setProperty ("steps", ls);`,
`        obj->setProperty ("step", engine.seqStep());
        obj->setProperty ("steps", ls);
        //  whether a host transport exists at all decides who owns SPACE
        obj->setProperty ("xport", engine.hostHasTransport() ? 1 : 0);
        obj->setProperty ("free", engine.isFreeRunning() ? 1 : 0);`, "meter"]
]);

const wU = edit("C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html", [

[`NB.on("meter", p => {
  const m = p.m || [];`,
`NB.on("meter", p => {
  const m = p.m || [];
  if (p.xport !== undefined) HOSTXPORT = p.xport ? 1 : 0;
  if (p.free !== undefined) FREERUN = p.free ? 1 : 0;`, "handler"],

[`let KITA = null, KITB = null;`,
`let KITA = null, KITB = null;
//  does the host have a transport at all? It decides who owns SPACE.
let HOSTXPORT = 0, FREERUN = 0;`, "flags"],

[SP + `if (e.code === "Space" || e.key === " ") {
` + SP + `  const t = e.target;
` + SP + `  if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
` + SP + `  e.preventDefault();
` + SP + `  if (t && typeof t.blur === "function" && t.tagName === "BUTTON") t.blur();
` + SP + `  if (SPEC.seq) NB.send({ k: "transport", what: VAL.seq >= 0.5 ? "pause" : "cont" });
` + SP + `}`,
 SP + `if (e.code === "Space" || e.key === " ") {
` + SP + `  const t = e.target;
` + SP + `  if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
` + SP + `  if (t && typeof t.blur === "function" && t.tagName === "BUTTON") t.blur();
` + SP + `  /*  When the host has a transport, SPACE belongs to the host. Eating it
` + SP + `      here is why space never started Ableton — and now that the machine
` + SP + `      follows the DAW, letting it through gives one transport instead of
` + SP + `      two arguing. With no host transport (the standalone) we still
` + SP + `      handle it ourselves. */
` + SP + `  if (HOSTXPORT) return;
` + SP + `  e.preventDefault();
` + SP + `  if (SPEC.seq) NB.send({ k: "transport", what: VAL.seq >= 0.5 ? "pause" : "cont" });
` + SP + `}`, "space"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
wP(); wU();
console.log("1: SPACE belongs to the host when there is one");
