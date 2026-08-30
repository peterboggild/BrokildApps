/*  Verify the rack fragment's stylesheet is well-formed: balanced braces, and
    no ordinary rule accidentally nested inside another. @media / @supports /
    @keyframes blocks are legitimately one level deep.

    This is the check that would have caught buglist 15 the day it landed —
    a `.bwfx-machint` rule spliced into the middle of an unclosed
    `.bwfx-dep.neg`, which under CSS nesting silently matched nothing. */
"use strict";
const fs = require("fs");
const src = fs.readFileSync("C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js", "utf8");

const m = src.match(/CSS\s*=\s*\[([\s\S]*?)\]\s*\.join\(/);
if (!m) { console.error("could not isolate the CSS array"); process.exit(1); }
const css = m[1].split(/\r?\n/).map(l => {
  const i = l.indexOf('"'), j = l.lastIndexOf('"');
  if (i < 0 || j <= i) return "";
  try { return JSON.parse(l.slice(i, j + 1)); } catch (e) { return ""; }
}).join("\n");

let open = 0, close = 0, depth = 0;
const stack = [], bad = [];
let selStart = 0;
for (let i = 0; i < css.length; i++) {
  const ch = css[i];
  if (ch === "{") {
    const sel = css.slice(selStart, i).replace(/[\s\S]*[};]/, "").trim().replace(/\s+/g, " ");
    if (depth > 0) {
      const outer = stack[stack.length - 1];
      //  at-rules may legitimately contain rules
      if (!/^@/.test(outer)) bad.push(outer + "  >>  " + sel);
    }
    stack.push(sel); open++; depth++;
  } else if (ch === "}") {
    stack.pop(); close++; depth--; selStart = i + 1;
  }
}
console.log("braces: " + open + " open, " + close + " close -> "
          + (open === close ? "BALANCED" : "*** UNBALANCED ***"));
if (bad.length) {
  console.log("*** " + bad.length + " rule(s) nested inside a NON at-rule:");
  bad.forEach(b => console.log("    " + b));
} else console.log("no ordinary rule is nested inside another");

for (const sel of [".bwfx-machint", ".bwfx-machint.armed", ".bwfx-dep.neg"]) {
  const i = css.indexOf(sel + "{");
  console.log(sel + " -> " + (i < 0 ? "NOT FOUND"
    : css.slice(i, css.indexOf("}", i) + 1).replace(/\n/g, " ")));
}
process.exit(open === close && bad.length === 0 ? 0 : 1);
