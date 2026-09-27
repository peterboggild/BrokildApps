/*  "Once A and B are pressed, they cannot be unpressed. That I think means it
    does not work."

    He is right about the panel, if not about the code. A and B are not
    toggles — they are slots, and a lit lamp means "this slot holds a kit".
    But a lit button that will not release reads as a switch that has jammed,
    and nothing on the panel ever said otherwise. Three fixes:

      * the row TELLS you what to do, and the text changes as you go: press A,
        change the machine, press B, then morph between them;
      * MORPH is visibly dead until both slots hold something. It was already
        inert — a fader with nothing to morph toward should do nothing — but
        an inert fader that looks live is the same bug in a different place;
      * shift-click CLEARS a slot, so the lamp can go out. Plain click
        re-captures, which is what you actually want nine times in ten.
*/
"use strict";
const fs = require("fs");
const miss = [];
const J = (a, nl) => a.join(nl);

// ── the panel ──────────────────────────────────────────────────────────────
{
  const P = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  function rep(a, b, tag) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push("ui: " + tag + " x" + n); return; }
    s = s.split(a).join(b);
  }

  rep(J([
    '  { const w = el("div", "gk", wk); w.style.width = "104px";',
    '    const row = el("div", "", w); row.style.cssText = "display:flex;gap:4px";',
    '    const ba = el("button", "k", row); ba.textContent = "A"; ba.style.cssText = "width:48px;height:30px";',
    '    const bb = el("button", "k", row); bb.textContent = "B"; bb.style.cssText = "width:48px;height:30px";',
    '    ba.addEventListener("click", () => NB.send({ k: "capture", slot: "a" }));',
    '    bb.addEventListener("click", () => NB.send({ k: "capture", slot: "b" }));',
    "    CAPBTN.a = ba; CAPBTN.b = bb;",
    '    el("div", "lab", w).textContent = "Capture"; }'
  ], NL), J([
    '  { const w = el("div", "gk", wk); w.style.width = "104px";',
    '    const row = el("div", "", w); row.style.cssText = "display:flex;gap:4px";',
    '    const ba = el("button", "k", row); ba.textContent = "A"; ba.style.cssText = "width:48px;height:30px";',
    '    const bb = el("button", "k", row); bb.textContent = "B"; bb.style.cssText = "width:48px;height:30px";',
    "    /*  A and B are SLOTS, not switches: a plain press fills one with what",
    "        the machine is doing right now, and pressing a full one fills it",
    "        again. Shift-clears, so the lamp can go out — without that the",
    "        buttons read as toggles that have jammed. */",
    '    const cap = (slot, ev) => NB.send({ k: "capture", slot, clear: ev.shiftKey ? 1 : 0 });',
    '    ba.addEventListener("click", ev => cap("a", ev));',
    '    bb.addEventListener("click", ev => cap("b", ev));',
    '    ba.title = bb.title = "Click: hold what the machine is doing now  \\u00b7  Shift-click: empty the slot";',
    "    CAPBTN.a = ba; CAPBTN.b = bb;",
    '    el("div", "lab", w).textContent = "Capture"; }'
  ], NL), "capture buttons");

  rep(J([
    '  el("div", "note", wk).textContent =',
    '    "Rail sag \\u2014 one supply, so a hard hit dips the whole kit. " +',
    '    "Kit body \\u2014 one shell they all sit in.";'
  ], NL), J([
    '  MORPHNOTE = el("div", "note", wk);',
    '  el("div", "note", wk).textContent =',
    '    "Rail sag \\u2014 one supply, so a hard hit dips the whole kit. " +',
    '    "Kit body \\u2014 one shell they all sit in.";',
    "  drawMorphHint();"
  ], NL), "morph note");

  rep("const PAGEBTN = [], CAPBTN = {};",
    J([
      "const PAGEBTN = [], CAPBTN = {};",
      "let MORPHNOTE = null, HAVE_A = false, HAVE_B = false;",
      "",
      "/*  The row says what to do next, and what it says changes as you go.",
      "    MORPH goes dim until there is something at both ends: it was already",
      "    inert then, and an inert fader that looks live is its own bug. */",
      "function drawMorphHint() {",
      "  if (!MORPHNOTE) return;",
      "  const both = HAVE_A && HAVE_B;",
      "  MORPHNOTE.textContent = both",
      '    ? "Morph blends A into B. Press A or B again to re-capture, shift-click to empty a slot."',
      "    : (HAVE_A || HAVE_B)",
      '      ? ("Slot " + (HAVE_A ? "A" : "B") + " is holding a kit. Now change the machine and press "',
      '         + (HAVE_A ? "B" : "A") + ".")',
      '      : "Press A to hold this kit, change the machine, then press B \\u2014 Morph blends between them.";',
      '  const mk = document.querySelector(".knob.big");',
      '  if (mk) mk.classList.toggle("inert", !both);',
      "}"
    ], NL), "hint fn");

  rep(J([
    '  if (CAPBTN.a) CAPBTN.a.classList.toggle("on", !!p.a);',
    '  if (CAPBTN.b) CAPBTN.b.classList.toggle("on", !!p.b);'
  ], NL), J([
    '  if (CAPBTN.a) CAPBTN.a.classList.toggle("on", !!p.a);',
    '  if (CAPBTN.b) CAPBTN.b.classList.toggle("on", !!p.b);',
    "  HAVE_A = !!p.a; HAVE_B = !!p.b;",
    "  drawMorphHint();"
  ], NL), "hint update");

  // a dimmed knob, so "nothing to morph toward" is visible
  rep(".knob.big{", ".knob.inert{opacity:.42;filter:saturate(.25)}" + NL + ".knob.big{", "inert css");

  if (!miss.length) fs.writeFileSync(P, s);
}

// ── the native side: shift-click empties a slot ────────────────────────────
{
  const P = "C:/Users/peter/b/FullMetalRacket/Source/PluginProcessor.cpp";
  let s = fs.readFileSync(P, "utf8");
  const NL = s.indexOf(String.fromCharCode(13, 10)) >= 0
    ? String.fromCharCode(13, 10) : String.fromCharCode(10);
  const A = J([
    "        const bool toB = o->getProperty (" + String.fromCharCode(34) + "slot" + String.fromCharCode(34) + ").toString() == " + String.fromCharCode(34) + "b" + String.fromCharCode(34) + ";",
    "        if (toB) { engine.kitB = q; engine.haveB = true; }",
    "        else     { engine.kitA = q; engine.haveA = true; }",
    "        emitKit();",
    '        notice (toB ? "CAPTURED INTO B" : "CAPTURED INTO A");'
  ], NL);
  const B = J([
    "        const bool toB = o->getProperty (" + String.fromCharCode(34) + "slot" + String.fromCharCode(34) + ").toString() == " + String.fromCharCode(34) + "b" + String.fromCharCode(34) + ";",
    "        const bool clear = (int) o->getProperty (" + String.fromCharCode(34) + "clear" + String.fromCharCode(34) + ") != 0;",
    "        if (clear)",
    "        {",
    "            //  emptying a slot must also stand the morph down, or the",
    "            //  fader would keep blending toward a kit that is no longer there",
    "            if (toB) engine.haveB = false; else engine.haveA = false;",
    "            emitKit();",
    '            notice (toB ? "B EMPTIED" : "A EMPTIED");',
    "        }",
    "        else",
    "        {",
    "            if (toB) { engine.kitB = q; engine.haveB = true; }",
    "            else     { engine.kitA = q; engine.haveA = true; }",
    "            emitKit();",
    '            notice (toB ? "CAPTURED INTO B" : "CAPTURED INTO A");',
    "        }"
  ], NL);
  const n = s.split(A).length - 1;
  if (n !== 1) miss.push("cpp: capture x" + n);
  else if (!miss.length) fs.writeFileSync(P, s.split(A).join(B));
}

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
console.log("A/B says what it is for, MORPH dims when it has nothing to do, shift-click empties");
