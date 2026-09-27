/*  The CSS block is an array of complete string literals, one per physical
    line — a rule that wraps is two adjacent entries. My insertion wrote
    multi-line strings, which is a syntax error. Rewrite those entries.
*/
"use strict";
const fs = require("fs");
const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
let s = fs.readFileSync(P, "utf8");
const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
  ? String.fromCharCode(13, 10) : String.fromCharCode(10);

const startMark = "    /* --- the macro rail: five faders, always visible --- */";
const endMark = '    ".bwfx-foot{padding:8px 18px 2px;';
const i = s.indexOf(startMark);
const j = s.indexOf(endMark, i);
if (i < 0 || j < 0) { console.error("ABORT: block not found"); process.exit(1); }

const GOOD = [
'    /* --- the macro rail: five faders, always visible --- */',
'    ".bwfx-macros{display:flex;align-items:stretch;gap:10px;padding:10px 16px 12px;",',
'    "border-top:1px solid #1d2830;background:linear-gradient(180deg,#0d151a,#0a1116)}",',
'    ".bwfx-macros>.lab{font:600 9.5px ui-monospace,Menlo,monospace;letter-spacing:.2em;",',
'    "color:#5d7a74;align-self:center;flex:none}",',
'    ".bwfx-mac{flex:1 1 0;min-width:0;display:grid;gap:3px}",',
'    ".bwfx-mac .mn{display:flex;align-items:center;gap:5px;cursor:pointer;",',
'    "font:600 10px ui-monospace,Menlo,monospace;letter-spacing:.12em;color:#8ea6a1;",',
'    "border:1px solid transparent;border-radius:4px;padding:1px 4px}",',
'    ".bwfx-mac .mn:hover{color:#cfe4df}",',
'    ".bwfx-mac.armed .mn{color:#0a0d10;background:var(--bwfx-acc);border-color:transparent}",',
'    ".bwfx-mac .cnt{margin-left:auto;font-size:9px;opacity:.8}",',
'    ".bwfx-mac input[type=range]{width:100%;margin:0}",',
'    ".bwfx-mac output{font:600 9.5px ui-monospace,Menlo,monospace;color:#6d8a85}",',
'    /* an assignable control while a macro is armed, and one already wired */',
'    ".bwfx-assignable{outline:1px dashed rgba(63,224,216,.55);outline-offset:2px;",',
'    "border-radius:4px;cursor:crosshair}",',
'    ".bwfx-assigned{outline:1px solid var(--bwfx-acc);outline-offset:2px;border-radius:4px}",',
''].join(NL);

s = s.slice(0, i) + GOOD + s.slice(j);
fs.writeFileSync(P, s);
console.log("css entries repaired");
