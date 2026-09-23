// LAST STEP, settable to anything.
//
// The value was always there — a lane's length is any number from 1 to 32 —
// but the only way to reach it was clicking through a list, so it read as a
// fixed set of choices. Three ways in now, and the label says what it is:
//
//   * DRAG the LAST field: every value in the range, one step per few pixels.
//   * CLICK it: jumps through the musical lengths, which is what you want
//     nine times in ten.
//   * RIGHT-CLICK A STEP: sets the last step to that step. The most direct of
//     the three — you point at where the loop should end.
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

// ---- the option field learns to be dragged -------------------------------
s = rep(s, `    const mk = (label, get, next) => {
      const b = el("div", "o", o);
      b.innerHTML = "<b></b>" + label;
      b.addEventListener("click", ev => {
        const L = laneOf(ci);
        next(L, ev.shiftKey ? -1 : 1);
        NB.send({ k: "lane", p: curPat, c: ci, len: L.len, div: L.div, dir: L.dir, sw: L.sw });
        drawSeq();
      });
      b._get = () => get(laneOf(ci));
      return b;
    };`,
`    const sendLane = () => {
      const L = laneOf(ci);
      NB.send({ k: "lane", p: curPat, c: ci, len: L.len, div: L.div, dir: L.dir, sw: L.sw });
    };
    /*  drag, when given, makes the field continuous — click still jumps
        through the sensible values, which is what you want nine times in ten. */
    const mk = (label, get, next, drag) => {
      const b = el("div", "o", o);
      b.innerHTML = "<b></b>" + label;
      b.addEventListener("click", ev => {
        const L = laneOf(ci);
        next(L, ev.shiftKey ? -1 : 1);
        sendLane(); drawSeq();
      });
      if (drag) {
        let d = null;
        b.style.cursor = "ns-resize";
        b.addEventListener("pointerdown", ev => {
          ev.preventDefault(); b.setPointerCapture(ev.pointerId);
          d = { y: ev.clientY, v: drag.get(laneOf(ci)), moved: false };
        });
        b.addEventListener("pointermove", ev => {
          if (!d) return;
          const step = Math.round((d.y - ev.clientY) / 7);
          if (step === 0) return;
          d.moved = true;
          drag.set(laneOf(ci), d.v + step);
          sendLane(); drawSeq();
        });
        const end = ev => {
          if (!d) return;
          //  a drag that moved must not also fire the click
          if (d.moved) { const stop = e2 => { e2.stopPropagation(); b.removeEventListener("click", stop, true); };
                         b.addEventListener("click", stop, true); }
          d = null;
          try { b.releasePointerCapture(ev.pointerId); } catch (x) {}
        };
        b.addEventListener("pointerup", end);
        b.addEventListener("pointercancel", end);
      }
      b._get = () => get(laneOf(ci));
      return b;
    };`, "mk drag");

// ---- LEN becomes LAST, and gains the drag --------------------------------
s = rep(s, `      mk("LEN",  L => L.len, (L, d) => {
        let i = LENS.indexOf(L.len);
        if (i < 0) {   // an odd length set by hand: step to the nearest musical one
          i = 0;
          for (let k = 0; k < LENS.length; k++) if (Math.abs(LENS[k] - L.len) < Math.abs(LENS[i] - L.len)) i = k;
        } else i = clamp(i + d, 0, LENS.length - 1);
        L.len = LENS[i];
      }),`,
`      mk("LAST", L => L.len, (L, d) => {
        let i = LENS.indexOf(L.len);
        if (i < 0) {   // an odd length set by hand: step to the nearest musical one
          i = 0;
          for (let k = 0; k < LENS.length; k++) if (Math.abs(LENS[k] - L.len) < Math.abs(LENS[i] - L.len)) i = k;
        } else i = clamp(i + d, 0, LENS.length - 1);
        L.len = LENS[i];
      }, { get: L => L.len, set: (L, v) => { L.len = clamp(Math.round(v), 1, NSTEP); } }),`, "last drag");

// ---- right-click a step to end the loop there ----------------------------
s = rep(s, `      cell.addEventListener("pointerdown", ev => {`,
`      /*  The most direct way to say where a loop ends: point at the step it
          should end on. Right-click, so it cannot be confused with painting. */
      cell.addEventListener("contextmenu", ev => {
        ev.preventDefault();
        const L = laneOf(ci);
        L.len = k + 1;
        NB.send({ k: "lane", p: curPat, c: ci, len: L.len, div: L.div, dir: L.dir, sw: L.sw });
        drawSeq();
        say(CHANS[ci].name + " LAST STEP " + L.len);
      });
      cell.addEventListener("pointerdown", ev => {`, "cell contextmenu");

// ---- and say so ----------------------------------------------------------
s = rep(s, `Click a step to toggle &middot; shift-click to select it for editing`,
`Click a step to toggle &middot; shift-click to select it &middot; right-click to set the last step`, "hint");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("LAST step: drag it, click it, or right-click a step");
