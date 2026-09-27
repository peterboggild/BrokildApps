// "How do I choose two bars?" — you clicked LEN sixteen times, which is not an
// answer. Two changes:
//
//   * LEN steps through MUSICAL lengths (1 2 3 4 6 8 12 16 24 32) instead of
//     one at a time, shift-click walks back, and dragging it is fine tuning.
//     The lengths that are not powers of two are there on purpose: a 3 or a 12
//     against a 16 is the whole reason per-lane length exists.
//   * A BARS control in the sequencer header sets EVERY lane at once, which is
//     what "make it two bars" actually means. Lanes that have been given their
//     own odd length are left alone — that is somebody's polymeter, and a
//     global control should not quietly flatten it.
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

// ---- the BARS control in the header --------------------------------------
s = rep(s, `          <button class="k dark" id="patcopy">Copy &#9654;</button>`,
`          <button class="k dark" id="patcopy">Copy &#9654;</button>
          <span class="lab" style="color:#a08e6c;margin-left:6px">Bars</span>
          <button class="k dark" id="bars1" style="width:34px;padding:0">1</button>
          <button class="k dark" id="bars2" style="width:34px;padding:0">2</button>`, "bars markup");

// ---- musical lengths, and drag ------------------------------------------
s = rep(s, `    ln._opts = [
      mk("LEN",  L => L.len, (L, d) => { L.len = clamp(L.len + d, 1, NSTEP); }),`,
`    /*  Musical lengths rather than one step at a time. The odd ones are the
        point of per-lane length: a three or a twelve against a sixteen is
        polymeter, which is free here and is most of why the lanes are
        independent at all. */
    const LENS = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32];
    ln._opts = [
      mk("LEN",  L => L.len, (L, d) => {
        let i = LENS.indexOf(L.len);
        if (i < 0) {   // an odd length set by hand: step to the nearest musical one
          i = 0;
          for (let k = 0; k < LENS.length; k++) if (Math.abs(LENS[k] - L.len) < Math.abs(LENS[i] - L.len)) i = k;
        } else i = clamp(i + d, 0, LENS.length - 1);
        L.len = LENS[i];
      }),`, "len list");

// ---- wire the bars buttons ----------------------------------------------
s = rep(s, `  $("#patclear").addEventListener("click", () => NB.send({ k: "clearpat" }));`,
`  /*  Set every lane at once. A lane already carrying an odd length is left
      as it is: that is deliberate polymeter, and a global control that
      silently flattened it would be worse than no control. */
  const setBars = bars => {
    const len = bars * 16;
    CHANS.forEach((c, ci) => {
      const L = laneOf(ci);
      if (L.len !== 16 && L.len !== 32) return;      // somebody's polymeter, leave it
      L.len = len;
      NB.send({ k: "lane", p: curPat, c: ci, len: L.len, div: L.div, dir: L.dir, sw: L.sw });
    });
    drawSeq();
    say(bars === 1 ? "ONE BAR" : "TWO BARS");
  };
  $("#bars1").addEventListener("click", () => setBars(1));
  $("#bars2").addEventListener("click", () => setBars(2));
  $("#patclear").addEventListener("click", () => NB.send({ k: "clearpat" }));`, "bars wiring");

// ---- and light whichever matches -----------------------------------------
s = rep(s, `    if (ln && ln._opts) ln._opts.forEach(b => { b.querySelector("b").textContent = b._get(); });`,
`    if (ln && ln._opts) ln._opts.forEach(b => { b.querySelector("b").textContent = b._get(); });
    if (c === 0) {
      //  the header lamps follow lane one, which is what "the pattern length"
      //  means when the lanes agree — and they usually do
      const b1 = $("#bars1"), b2 = $("#bars2");
      if (b1) b1.classList.toggle("on", L.len === 16);
      if (b2) b2.classList.toggle("on", L.len === 32);
    }`, "bars lamps");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("BARS 1/2 in the header, musical lane lengths");
