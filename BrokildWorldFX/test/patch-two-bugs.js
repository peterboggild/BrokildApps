/*  TWO BUGS, both found by reading the overlay's own state instead of
    guessing at it.

    1. modRows was 0, so nothing ever repainted. The `MODROWS = []` reset
       landed in renderSpectra() rather than renderList() — the anchor I used
       appears in both, matched once, and I assumed it was the right one.
       renderSpectra runs AFTER renderList, so it wiped the registry that had
       just been filled.

    2. Assigning anything to any macro silently DROPPED macro 5's dry/wet.
       The default is implicit — no "m" key means "macro 5 holds the mix" —
       and the first edit left that state by clearing everything. Leaving the
       default has to MATERIALISE it first, not discard it.
*/
"use strict";
const fs = require("fs");
const miss = [];

// ── 1 · the reset belongs in renderList ────────────────────────────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/ui/bwfx-rack.js";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);

  const WRONG = "    MODROWS = [];      // renderList rebuilds every row" + NL;
  if (s.split(WRONG).length - 1 !== 1) miss.push("stray reset x" + (s.split(WRONG).length - 1));
  else s = s.split(WRONG).join("");

  const A = "  function renderList() {";
  if (s.split(A).length - 1 !== 1) miss.push("renderList x" + (s.split(A).length - 1));
  else s = s.split(A).join(
    A + NL
    + "    //  every row is rebuilt here, so the modulation registry starts" + NL
    + "    //  empty. It lived in renderSpectra() by mistake, which runs AFTER" + NL
    + "    //  this and wiped what this had just filled." + NL
    + "    MODROWS = [];");

  if (! miss.length) fs.writeFileSync(P, s);
}

// ── 2 · leaving the default materialises it ────────────────────────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/src/bwfx_macros.cpp";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const J = (a) => a.join(NL);
  function rep(a, b, tag) {
    const A = a.split(String.fromCharCode(10)).join(NL);
    const B = b.split(String.fromCharCode(10)).join(NL);
    const n = s.split(A).length - 1;
    if (n !== 1) { miss.push(tag + " x" + n); return; }
    s = s.split(A).join(B);
  }

  rep(
"void Rack::setMacroAssign (int macro, const std::string& dest, float depth)\n"
+ "{\n"
+ "    if (macro < 0 || macro >= kMacros || dest.empty()) return;\n"
+ "    macroDefaulted = false;                 // the rack now states its own wiring",

"/*  The default wiring is IMPLICIT — a blob with no \"m\" key means macro 5\n"
+ "    holds the dry/wet, which is what keeps pre-macro blobs byte-identical.\n"
+ "    The moment anything is edited the rack has to state its wiring in full,\n"
+ "    and that means writing the default down rather than discarding it.\n"
+ "    Discarding it is what the first version did, so assigning anything to\n"
+ "    any macro silently took the dry/wet off macro 5. */\n"
+ "void Rack::materialiseDefault()\n"
+ "{\n"
+ "    if (! macroDefaulted) return;\n"
+ "    macroDefaulted = false;\n"
+ "    for (auto& v : macroAssign) v.clear();\n"
+ "    macroAssign[(size_t) kDefaultMacro].push_back ({ kDefaultDest, kDefaultDepth });\n"
+ "}\n"
+ "\n"
+ "void Rack::setMacroAssign (int macro, const std::string& dest, float depth)\n"
+ "{\n"
+ "    if (macro < 0 || macro >= kMacros || dest.empty()) return;\n"
+ "    materialiseDefault();", "setMacroAssign");

  rep(
"void Rack::clearMacroAssigns (int macro)\n"
+ "{\n"
+ "    if (macro < 0 || macro >= kMacros) return;\n"
+ "    macroDefaulted = false;\n"
+ "    macroAssign[(size_t) macro].clear();",

"void Rack::clearMacroAssigns (int macro)\n"
+ "{\n"
+ "    if (macro < 0 || macro >= kMacros) return;\n"
+ "    materialiseDefault();          // ...then take this one away, default or not\n"
+ "    macroAssign[(size_t) macro].clear();", "clearMacroAssigns");

  //  macroAssignJson no longer needs the defaulted special case for reading,
  //  but keep it: the default is still implicit until something is edited.
  if (! miss.length) fs.writeFileSync(P, s);
}

// ── the declaration ────────────────────────────────────────────────────────
{
  const P = "C:/Users/peter/b/BrokildWorldFX/src/bwfx.h";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = "    void republishMacros();          // message thread: resolve names -> indices";
  if (s.split(A).length - 1 !== 1) miss.push("decl x" + (s.split(A).length - 1));
  else s = s.split(A).join(
    "    void materialiseDefault();      // write the implicit wiring down before editing" + NL + A);
  if (! miss.length) fs.writeFileSync(P, s);
}

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("registry reset moved to renderList; the default wiring survives the first edit");
