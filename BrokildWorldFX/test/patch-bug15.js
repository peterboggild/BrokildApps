/*  BWFX BUGLIST 15 — the macro hint line.

    Peter: "the CLICK A MACRO... sentence in the BWFX macro window should be
    styled like the rest of the text... capital white letters. Make it
    brighter or yellow if you want it to pop, but not larger, it looks like a
    design error."

    It looked like a design error because it WAS one, and not a styling
    choice: the `.bwfx-machint` rule had been spliced into the middle of
    `.bwfx-dep.neg`, which was opened and never closed before it. Under CSS
    nesting that resolves to `.bwfx-dep.neg .bwfx-machint` and matches
    nothing at all — so the line inherited from `.bwfx-win`: system-ui, no
    font-size at all, i.e. the browser default 16px, sitting next to 9.5px
    monospace captions. No amount of restyling would have shown up until the
    braces were repaired. (The insertion-anchor bug, again — the third time
    in this codebase.)

    Fixed: the two rules are separated, and the hint now matches the panel —
    9.5px monospace, the same letter-spacing as its neighbours, never larger.
    It carries state rather than decoration: muted while nothing is armed,
    accent-bright while a macro is waiting for a control, which is the moment
    the sentence is actually addressed to you.
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

const w = edit("C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js", [

//  1. repair the braces and restyle
[`    ".bwfx-dep.neg{background:transparent;color:var(--bwfx-acc);",
    ".bwfx-machint{padding:0 16px 8px;font:9.5px ui-monospace,Menlo,monospace;",
    "letter-spacing:.13em;color:#4d625e;min-height:13px}",
    "box-shadow:inset 0 0 0 1px var(--bwfx-acc)}",`,
`    ".bwfx-dep.neg{background:transparent;color:var(--bwfx-acc);",
    "box-shadow:inset 0 0 0 1px var(--bwfx-acc)}",
    ".bwfx-machint{padding:0 16px 8px;font:9.5px ui-monospace,Menlo,monospace;",
    "letter-spacing:.14em;color:#7f9a94;min-height:13px}",
    ".bwfx-machint.armed{color:var(--bwfx-acc)}",`, "css"],

//  2. the hint carries state: bright only while a macro is waiting
[`    if (hint) hint.textContent = armed < 0`,
`    if (hint) hint.classList.toggle("armed", armed >= 0);
    if (hint) hint.textContent = armed < 0`, "armed class"]
]);

if (miss.length) { console.error("ABORT:" + NLo + "  " + miss.join(NLo + "  ")); process.exit(1); }
w();
console.log("15: braces repaired, hint sized and coloured like the panel");
