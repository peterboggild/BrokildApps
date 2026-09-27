// THE CUT IS ON THE STEPS PAGE, AND IT IS A LAYOUT BUG, NOT A WINDOW ONE.
//
// Measured: the voices page needs exactly 830px, the steps page needs 885 —
// so 55px hang below the deck and are clipped. It looked like a host problem
// because it looked like the bottom of the window was missing, and it was
// invisible on the page I kept screenshotting.
//
// The cause: #deck is a grid with a fixed height, but a grid row is `auto` by
// default, so #face grew past the deck instead of being held to it, and
// overflow:hidden cut the difference. The children CAN compress — the lanes
// are flex:1 with min-height:0 — they were simply never told there was
// anything to compress into.
//
// grid-template-rows:minmax(0,1fr) holds the face to the deck's height, and
// the min-height:0 chain lets that pressure reach the lanes. It fixes any
// future page too, rather than this one page today.
//
// Also: SPACE starts and stops the sequencer.
"use strict";
const fs = require("fs");
const miss = [];
function rep(s, a, b, tag) {
  const n = s.split(a).length - 1;
  if (n !== 1) { miss.push(tag + " x" + n); return s; }
  return s.split(a).join(b);
}
const P = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
let s = fs.readFileSync(P, "utf8");

// ── hold the face to the deck ──────────────────────────────────────────────
s = rep(s, `#deck{width:1800px;height:830px;flex:0 0 auto;transform-origin:center center;display:grid;
  grid-template-columns:30px 1fr 30px;border-radius:6px;overflow:hidden;`,
`#deck{width:1800px;height:830px;flex:0 0 auto;transform-origin:center center;display:grid;
  grid-template-columns:30px 1fr 30px;
  /*  A grid row is auto-sized by default, so the face grew PAST the deck's fixed
      height and overflow:hidden cut the difference — 885px of steps page
      inside an 830px deck. minmax(0,1fr) holds it to the deck instead, and
      the min-height:0 chain below lets that pressure reach the lanes, which
      were always able to compress and were simply never asked to. */
  grid-template-rows:minmax(0,1fr);
  border-radius:6px;overflow:hidden;`, "deck rows");

s = rep(s, `  padding:13px 14px;display:flex;flex-direction:column;gap:9px;min-width:0}`,
         `  padding:13px 14px;display:flex;flex-direction:column;gap:9px;min-width:0;min-height:0}`, "face min-height");

s = rep(s, `#seqpage{flex:1;display:none;flex-direction:column;gap:6px;min-width:0}`,
         `#seqpage{flex:1;display:none;flex-direction:column;gap:6px;min-width:0;min-height:0}`, "seqpage min-height");

// the two bands either side of the lanes hold their own size; the lanes give
s = rep(s, `.seqhead{flex:none;display:flex;align-items:center;gap:10px;padding:7px 10px;border-radius:3px}`,
         `.seqhead{flex:none;display:flex;align-items:center;gap:10px;padding:6px 10px;border-radius:3px}`, "seqhead pad");
s = rep(s, `.stepedit{flex:none;display:flex;align-items:center;gap:14px;padding:7px 12px;border-radius:3px}`,
         `.stepedit{flex:none;display:flex;align-items:center;gap:14px;padding:6px 12px;border-radius:3px}`, "stepedit pad");

//  and the lane's own contents must be able to get small enough to matter
s = rep(s, `.lopts{flex:none;width:176px;display:flex;align-items:stretch;gap:3px;padding:3px;border-radius:2px;`,
         `.lopts{flex:none;width:176px;display:flex;align-items:stretch;gap:3px;padding:2px;border-radius:2px;min-height:0;`, "lopts pad");
s = rep(s, `.lopts .o{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:1px;
  cursor:pointer;border-radius:2px;padding:2px 0;`,
         `.lopts .o{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:0;
  cursor:pointer;border-radius:2px;padding:1px 0;min-height:0;overflow:hidden;`, "lopts o pad");
s = rep(s, `.lane{flex:1;display:flex;align-items:stretch;gap:5px;min-height:0}`,
         `.lane{flex:1;display:flex;align-items:stretch;gap:5px;min-height:0;overflow:hidden}`, "lane overflow");

// ── SPACE starts and stops the sequencer ───────────────────────────────────
s = rep(s, `window.addEventListener("keydown", e => { if (e.key === "Escape") { closeMenu(); closeSettings(); closeAbout(); } });`,
`window.addEventListener("keydown", e => {
  if (e.key === "Escape") { closeMenu(); closeSettings(); closeAbout(); return; }
  /*  SPACE runs and stops it, the way it does on every machine with a
      transport. Not while typing in the kit filter, and not while a button
      has focus — otherwise space would "press" that button instead, which is
      the browser's own default and would be baffling here. */
  if (e.code === "Space" || e.key === " ") {
    const t = e.target;
    if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
    e.preventDefault();
    if (t && typeof t.blur === "function" && t.tagName === "BUTTON") t.blur();
    if (SPEC.seq) { setVal("seq", VAL.seq >= 0.5 ? 0 : 1); say(VAL.seq >= 0.5 ? "RUNNING" : "STOPPED"); }
  }
});`, "space");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("the face is held to the deck; SPACE runs and stops");
