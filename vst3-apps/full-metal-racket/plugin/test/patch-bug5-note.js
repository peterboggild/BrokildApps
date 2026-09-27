/*  The spec's caveat, honoured: "It overlaps with SNAP, and the panel must
    not pretend otherwise. SNAP shapes the excitation — it changes what the
    drum IS. PUNCH shapes the envelope after the fact — it changes how the
    drum SITS. Related, not the same, and if the note under them does not say
    so people will assume one of them is broken."

    Both knobs now say which is which, in the place a hand is already
    hovering. */
"use strict";
const fs = require("fs");
const NLo = String.fromCharCode(10);
const P = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0 ? String.fromCharCode(13, 10) : NLo;

const A = [
"    node.title = s.n + \"  \" + fmt(id);"].join(NL);
const B = [
"    node.title = s.n + \"  \" + fmt(id) + (KNOBNOTE[id.replace(/^.*_/, \"\")] || \"\");"].join(NL);
if (s.split(A).length - 1 !== 1) { console.error("ABORT: title anchor x" + (s.split(A).length - 1)); process.exit(1); }
s = s.split(A).join(B);

//  the note table, beside the knob lists it belongs to
const A2 = "const FAMCOL = {";
const B2 = [
"/*  PUNCH and SNAP are cousins and people will assume one of them is broken",
"    unless the panel says which is which. SNAP changes what the drum IS;",
"    PUNCH changes how it SITS. */",
"const KNOBNOTE = {",
"  snap:  \"\\n\\nShapes the excitation — what the drum IS.\\nPunch (in the kit row) shapes how it sits.\",",
"  punch: \"\\n\\nLifts every strike and leans on every body, keyed to each\\nvoice's own trigger, so it cannot mis-fire. Shapes how the\\ndrum SITS; Snap shapes what it IS.\"",
"};",
"const FAMCOL = {"].join(NL);
if (s.split(A2).length - 1 !== 1) { console.error("ABORT: famcol anchor"); process.exit(1); }
s = s.split(A2).join(B2);
fs.writeFileSync(P, s);
console.log("PUNCH and SNAP say which is which");
