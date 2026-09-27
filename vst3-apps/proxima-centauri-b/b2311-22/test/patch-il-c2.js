/*  You choose who to talk to from the survey field — which is already laid
    out by real spectral similarity, so a near neighbour is a fluent
    conversation and a far one is very nearly a stranger. Shift-click calls a
    body into the vessel instead of becoming it. Travel by conversation.
*/
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const miss = [];
const P = "C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;
for (const [a, b, tag] of [

[`function fieldClick(x, y) {
  if (fieldHover >= 0) {
    VAL.specimen = fieldHover;
    NB.send({ k: "p", id: "specimen", v: fieldHover });   // raw catalog number
    fieldOpen = false;
    document.getElementById("surveyBtn").classList.remove("on");
  }
}`,
`function fieldClick(x, y, shift) {
  if (fieldHover >= 0) {
    if (shift) {
      /*  call it in to talk to, rather than becoming it. The field's own
          geometry is spectral similarity, so where you click decides
          whether you will be understood. */
      VAL.other = fieldHover;
      NB.send({ k: "p", id: "other", v: fieldHover });
      if (!(VAL.converse > 0)) sendParam("converse", 0.45);
    } else {
      VAL.specimen = fieldHover;
      NB.send({ k: "p", id: "specimen", v: fieldHover });   // raw catalog number
    }
    fieldOpen = false;
    document.getElementById("surveyBtn").classList.remove("on");
  }
}`, "fieldClick"],

[`  if (fieldOpen) { fieldClick(e.clientX, e.clientY); return; }`,
 `  if (fieldOpen) { fieldClick(e.clientX, e.clientY, e.shiftKey); return; }`, "call site"]

]) {
  const A = a.split(NLo).join(NL), B = b.split(NLo).join(NL);
  const n = s.split(A).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); continue; }
  s = s.split(A).join(B);
}
if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("C2: shift-click the field to call a body");
