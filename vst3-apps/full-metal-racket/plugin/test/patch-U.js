// TWO BUGS.
//
// 1. THE PANEL RESTYLED ITSELF WHEN THE WINDOW GOT SMALL. The deck is a flex
//    ITEM inside #fit, and a flex item shrinks below its stated width unless
//    told not to. So below 1800px the deck did not just scale — it also got
//    NARROWER, and the whole layout reflowed: strips squeezed, knobs shrank,
//    labels ran into each other. transform:scale() does not change layout
//    size, but flex-shrink does, and the two were fighting. One line:
//    flex:0 0 auto. Everything else in the fit work was necessary too, but
//    this is what Peter's screenshot was actually showing.
//
// 2. OH's TUNE DID NOTHING. HAT LINK is on by default and makes the open hat
//    take the closed hat's tuning — correct, and the whole point of the pair
//    being one instrument — but it made a knob on the panel dead, with no
//    sign of why. Same fault as REBOUND having no control and KIT TUNE moving
//    a quarter of the machine: a control that lies about what it does.
//
//    Not fixed by unlinking them. While linked, OH's TUNE and MODEL show the
//    pair's value, are marked as slaved, and DRIVE THE PAIR — so the knob
//    does the obvious thing instead of nothing.
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

// ── 1 · the deck must not shrink, only scale ───────────────────────────────
s = rep(s, `#deck{width:1800px;height:830px;transform-origin:center center;display:grid;`,
`/*  flex:0 0 auto is load-bearing. #deck is a flex item, and a flex item
    shrinks below its width unless it is told not to — so below 1800px the
    deck got NARROWER as well as scaled, and the layout reflowed instead of
    shrinking: strips squeezed, labels ran together. transform does not
    change layout size; flex-shrink does. */
#deck{width:1800px;height:830px;flex:0 0 auto;transform-origin:center center;display:grid;`, "deck flex");

// ── 2 · the linked pair ────────────────────────────────────────────────────
s = rep(s, `function setVal(id, v, push) {
  const s = SPEC[id]; if (!s) return;`,
`/*  While HAT LINK is on the open hat takes the closed hat's tuning and model.
    Rather than leave OH's knobs dead, they drive the PAIR — which is what a
    player reaching for them means. Only for edits made here: a value arriving
    from the host is never redirected, or automation on ch_tune would bounce
    between the two. */
const LINKED = { oh_tune: "ch_tune", oh_model: "ch_model" };
function linkTarget(id) {
  return (VAL.hatlink >= 0.5 && LINKED[id]) ? LINKED[id] : id;
}

function setVal(id, v, push) {
  if (push !== false && LINKED[id] && VAL.hatlink >= 0.5) return setVal(LINKED[id], v);
  const s = SPEC[id]; if (!s) return;`, "setVal redirect");

// the knob reads the value it is actually controlling
s = rep(s, `function makeKnob(node, id) {
  const s = SPEC[id]; if (!s) return;
  const hi = s.hi || 1;
  dragify(node, id, 170, false);
  reg(id, { draw() {
    const t = clamp((VAL[id] || 0) / hi, 0, 1);
    node.style.setProperty("--a", (-135 + 270 * t) + "deg");
    node.title = s.n + "  " + fmt(id);
  } });
}`,
`function makeKnob(node, id) {
  const s = SPEC[id]; if (!s) return;
  const hi = s.hi || 1;
  dragify(node, id, 170, false);
  const w = { draw() {
    const eff = linkTarget(id);
    const t = clamp((VAL[eff] || 0) / hi, 0, 1);
    node.style.setProperty("--a", (-135 + 270 * t) + "deg");
    node.classList.toggle("slaved", eff !== id);
    node.title = eff !== id
      ? s.n + "  " + fmt(eff) + "   (linked to the closed hat)"
      : s.n + "  " + fmt(id);
  } };
  reg(id, w);
  //  redraw when the LINK switch moves, or the slaved mark goes stale
  if (LINKED[id]) reg("hatlink", { draw: () => w.draw() });
}`, "makeKnob link");

// the model keys light from whichever channel is actually in charge
s = rep(s, `  reg(id, { draw() {
    const v = Math.round(VAL[id] || 0);
    keys.forEach((b, i) => b.classList.toggle("on", i === v));
  } });
}`,
`  const w = { draw() {
    const eff = linkTarget(id);
    const v = Math.round(VAL[eff] || 0);
    keys.forEach((b, i) => {
      b.classList.toggle("on", i === v);
      b.classList.toggle("slaved", eff !== id);
    });
  } };
  reg(id, w);
  if (LINKED[id]) reg("hatlink", { draw: () => w.draw() });
}`, "makeSegs link");

// and it has to look slaved
s = rep(s, `.knob.big{max-width:50px}`,
`.knob.big{max-width:50px}
/*  A slaved control is not disabled — it still works, it just belongs to the
    pair rather than to this channel. Marked with the link colour rather than
    greyed out, because greyed-out reads as broken. */
.knob.slaved{box-shadow:0 0 0 2px #2c7a6688,0 2px 3px #00000055,inset 0 -3px 5px #00000048!important}
.k.slaved{box-shadow:inset 0 0 0 1px #2c7a6699!important}`, "slaved css");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("deck no longer reflows; OH tune and model drive the linked pair");
