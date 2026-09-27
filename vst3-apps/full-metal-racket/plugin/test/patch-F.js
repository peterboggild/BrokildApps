// CHUNK F — the panel gains a second page.
//
// The twelve strips stay exactly as they are; the sequencer is a PAGE, not a
// band squeezed underneath them. Sound design and pattern writing are
// different jobs and you do not need both on screen at once — which is also
// how machines with a finite front panel have always solved this.
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

// ─────────────────────────────────────────────────────────────────── CSS ───
s = rep(s, `#tip{position:fixed;`,
`/* ── the sequencer page ─────────────────────────────────────────────────── */
#seqpage{flex:1;display:none;flex-direction:column;gap:6px;min-width:0}
#seqpage.on{display:flex}
#strips.off{display:none}
.seqhead{flex:none;display:flex;align-items:center;gap:10px;padding:7px 10px;border-radius:3px}
.pats{display:flex;gap:3px}
.pats .k{width:26px;height:30px;font-size:11px;letter-spacing:0;border-radius:2px}
.seqhead .gk{display:flex;flex-direction:column;align-items:center;gap:4px;width:62px}
.seqhead .k{height:30px;padding:0 12px}
.seqhead .sp{flex:1}
.lanes{flex:1;display:flex;flex-direction:column;gap:3px;min-height:0}
.lane{flex:1;display:flex;align-items:stretch;gap:5px;min-height:0}
.lname{flex:none;width:96px;display:flex;align-items:center;gap:5px;padding:0 7px;border-radius:2px;
  background:linear-gradient(180deg,#2b2213,var(--ink));border:1px solid #0a0703}
.lname b{flex:1;font:400 17px/1 var(--f-disp);letter-spacing:.05em;color:var(--cream);text-transform:uppercase}
.lname .lm{width:20px;height:20px;border-radius:2px;cursor:pointer;flex:none;
  border:1px solid #0a0703;background:#3a2e1b;color:#c0ae8a;font:700 9px/18px var(--f-lab);text-align:center}
.lname .lm.on{background:linear-gradient(180deg,#d8552f,#a32d12);color:#fff}
.cells{flex:1;display:grid;grid-template-columns:repeat(32,1fr);gap:2px;min-width:0}
.cell{border-radius:2px;cursor:pointer;position:relative;
  background:linear-gradient(180deg,#cfc2a3,#b3a482);border:1px solid #9a8a68;
  box-shadow:inset 0 1px 0 #ffffff70}
.cell:nth-child(4n+1){background:linear-gradient(180deg,#e2d7bb,#c2b492)}
.cell.on{background:linear-gradient(180deg,#ffb35c,#d8721a);border-color:#95470c;
  box-shadow:inset 0 1px 0 #ffffff88,0 0 8px #ff8f1f55}
.cell.acc{background:linear-gradient(180deg,#ef8a5e,#c5411f);border-color:#8d2413}
.cell.past{outline:2px solid var(--red);outline-offset:-2px}
.cell.sel{outline:2px solid #2f8a74;outline-offset:-2px}
.cell.dim{opacity:.28}
.cell i{position:absolute;left:2px;right:2px;bottom:1px;height:2px;border-radius:1px;
  background:#00000055;display:block}
.lopts{flex:none;width:150px;display:flex;align-items:center;gap:2px;padding:0 4px;border-radius:2px;
  background:linear-gradient(180deg,#2b2213,var(--ink));border:1px solid #0a0703}
.lopts .o{flex:1;text-align:center;cursor:pointer;border-radius:2px;padding:3px 0;
  font:700 9px/1.25 var(--f-lab);letter-spacing:.05em;color:#c9b894;background:#00000030}
.lopts .o:hover{background:#ffffff18}
.lopts .o b{display:block;font:700 11px/1.1 var(--f-lab);color:var(--cream)}
.stepedit{flex:none;display:flex;align-items:center;gap:14px;padding:7px 12px;border-radius:3px}
.stepedit .gk{display:flex;flex-direction:column;align-items:center;gap:4px;width:64px}
.stepedit .who{font:400 19px/1 var(--f-disp);letter-spacing:.05em;color:var(--amber);
  text-transform:uppercase;min-width:150px}
.stepedit .hintx{font:700 9px/1.5 var(--f-lab);letter-spacing:.12em;text-transform:uppercase;
  color:#8d7d5e;text-align:right;flex:1}

#tip{position:fixed;`, "seq css");

// ────────────────────────────────────────────────────────────── markup ─────
s = rep(s, `      <div id="strips"></div>
      <div id="mstrip" class="strip">`,
`      <div id="strips"></div>
      <div id="seqpage">
        <div class="seqhead blk">
          <span class="lab" style="color:#a08e6c">Pattern</span>
          <div class="pats" id="pats"></div>
          <button class="k dark" id="patclear">Clear</button>
          <button class="k dark" id="patcopy">Copy &#9654;</button>
          <div class="sp"></div>
          <button class="k dark" id="seqrun">Run</button>
          <div class="gk"><div class="knob big" data-g="tempo"></div><div class="lab" style="color:#a08e6c">Tempo</div></div>
          <div class="gk"><div class="knob big" data-g="swing"></div><div class="lab" style="color:#a08e6c">Swing</div></div>
          <div class="gk"><div class="knob big" data-g="feel"></div><div class="lab" style="color:#a08e6c">Feel</div></div>
          <div class="gk"><div class="knob big" data-g="grip"></div><div class="lab" style="color:#a08e6c">Grip</div></div>
        </div>
        <div class="lanes" id="lanes"></div>
        <div class="stepedit blk">
          <div class="who" id="stepwho">No step selected</div>
          <div class="gk"><div class="knob big" data-s="vel"></div><div class="lab" style="color:#a08e6c">Velocity</div></div>
          <div class="gk"><div class="knob big" data-s="prob"></div><div class="lab" style="color:#a08e6c">Chance</div></div>
          <div class="gk"><div class="knob big" data-s="rt"></div><div class="lab" style="color:#a08e6c">Ratchet</div></div>
          <div class="gk"><div class="knob big" data-s="mic"></div><div class="lab" style="color:#a08e6c">Micro</div></div>
          <div class="gk"><div class="knob big" data-s="cond"></div><div class="lab" style="color:#a08e6c">Condition</div></div>
          <div class="hintx">Click a step to toggle &middot; shift-click to select it for editing</div>
        </div>
      </div>
      <div id="mstrip" class="strip">`, "seq markup");

// morph and the patch buttons go in the WEB band, which had room
s = rep(s, `  el("div", "sep", wk);
  el("div", "note", wk).textContent =
    "Rail sag — one supply, so a hard hit dips the whole kit in level and in pitch. " +
    "Kit body — one shell they all sit in.";`,
`  el("div", "sep", wk);

  /*  MORPH lives here rather than on a strip because it belongs to the whole
      machine. It is inert until BOTH kits are captured — a fader with nothing
      to morph toward should do nothing, not sweep to silence. */
  { const w = el("div", "gk", wk); w.style.width = "86px";
    makeKnob(el("div", "knob big", w), "morph");
    el("div", "lab", w).textContent = "Morph"; }
  { const w = el("div", "gk", wk); w.style.width = "104px";
    const row = el("div", "", w); row.style.cssText = "display:flex;gap:4px";
    const ba = el("button", "k", row); ba.textContent = "A"; ba.style.cssText = "width:48px;height:30px";
    const bb = el("button", "k", row); bb.textContent = "B"; bb.style.cssText = "width:48px;height:30px";
    ba.addEventListener("click", () => NB.send({ k: "capture", slot: "a" }));
    bb.addEventListener("click", () => NB.send({ k: "capture", slot: "b" }));
    CAPBTN.a = ba; CAPBTN.b = bb;
    el("div", "lab", w).textContent = "Capture"; }
  el("div", "note", wk).textContent =
    "Rail sag \\u2014 one supply, so a hard hit dips the whole kit. " +
    "Kit body \\u2014 one shell they all sit in.";`, "morph in web");

// the rail gains pages and the patch buttons
s = rep(s, `  rbtn("Random", () => NB.send({ k: "random" }));
  rbtn("Panic", () => NB.send({ k: "panic" }));`,
`  /*  Two pages. The strips are the machine's identity so they are page one;
      the sequencer gets the same room rather than a strip along the bottom. */
  { const v = rbtn("Voices", () => showPage(0));
    const q = rbtn("Steps",  () => showPage(1));
    PAGEBTN.push(v, q); }
  el("div", "rl", rail).textContent = "Page";
  rbtn("Random", () => NB.send({ k: "random" }));
  rbtn("Panic", () => NB.send({ k: "panic" }));
  rbtn("Save", () => NB.send({ k: "save" }));
  rbtn("Load", () => NB.send({ k: "open" }));`, "rail pages");

// ────────────────────────────────────────────────────────────────── JS ─────
s = rep(s, `let built = false;
const VUCOL = [null, null];
let vuHold = [0, 0];

function build() {`,
`let built = false;
const VUCOL = [null, null];
let vuHold = [0, 0];
const PAGEBTN = [], CAPBTN = {};
let NSTEP = 32, NPAT = 16, curPat = 0, playStep = -1;
let SEQ = null;                       // the native sequencer state, mirrored
let sel = null;                       // {c, s} — the step being edited

function showPage(i) {
  PAGEBTN.forEach((b, j) => b.classList.toggle("on", j === i));
  $("#strips").classList.toggle("off", i === 1);
  $("#seqpage").classList.toggle("on", i === 1);
  if (i === 1) NB.send({ k: "seqget" });
}

/*  A blank lane, so the page can be drawn before the native side has said
    anything — the alternative is a page that is empty until a round trip
    completes, which reads as broken. */
function blankLane() {
  const st = [];
  for (let k = 0; k < NSTEP; k++) st.push({ on: 0, vel: 100, prob: 100, rt: 1, cond: 0, mic: 0 });
  return { len: 16, div: 1, dir: 0, sw: 0, m: 0, st };
}
function blankSeq() {
  const p = [];
  for (let i = 0; i < NPAT; i++) { const lanes = []; for (let c = 0; c < 12; c++) lanes.push(blankLane()); p.push(lanes); }
  return p;
}

const DIVNAME = ["1/32", "1/16", "1/8", "1/4"];
const DIRNAME = ["FWD", "REV", "PEND", "RAND"];
const CONDNAME = ["--", "1:2", "1:3", "1:4", "PREV", "!PREV"];

function build() {`, "seq state");

// build the sequencer page at the end of build()
s = rep(s, `  $("#mapnote").textContent = "MIDI · GM map, or C3–B3 for the twelve in panel order";`,
`  buildSeq();
  showPage(0);
  $("#mapnote").textContent = "MIDI \\u00b7 GM map, or C3\\u2013B3 for the twelve in panel order";`, "build seq call");

// the sequencer page builder and its wiring
s = rep(s, `/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`,
`/* ─── 3b · the sequencer page ────────────────────────────────────────────── */
const CELL = [];         // CELL[c][s] -> element

function laneOf(c) { return (SEQ || (SEQ = blankSeq()))[curPat][c]; }

function buildSeq() {
  //  pattern keys
  const pats = $("#pats");
  pats.innerHTML = "";
  for (let i = 0; i < NPAT; i++) {
    const b = el("button", "k dark", pats);
    b.textContent = String(i + 1);
    b.addEventListener("click", () => { curPat = i; NB.send({ k: "pat", i }); drawSeq(); });
    b._pat = i;
  }
  $("#patclear").addEventListener("click", () => NB.send({ k: "clearpat" }));
  $("#patcopy").addEventListener("click", () => NB.send({ k: "copypat", to: (curPat + 1) % NPAT }));
  { const b = $("#seqrun");
    b.addEventListener("click", () => setVal("seq", VAL.seq >= 0.5 ? 0 : 1));
    reg("seq", { draw() { b.classList.toggle("on", VAL.seq >= 0.5); b.textContent = VAL.seq >= 0.5 ? "Running" : "Run"; } }); }

  document.querySelectorAll("#seqpage [data-g]").forEach(n => makeKnob(n, n.dataset.g));

  //  twelve lanes
  const box = $("#lanes");
  box.innerHTML = "";
  CELL.length = 0;
  CHANS.forEach((c, ci) => {
    const ln = el("div", "lane", box);

    const nm = el("div", "lname", ln);
    el("b", "", nm).textContent = c.name;
    const lm = el("div", "lm", nm); lm.textContent = "M";
    lm.addEventListener("click", () => {
      const L = laneOf(ci); L.m = L.m ? 0 : 1;
      NB.send({ k: "lane", p: curPat, c: ci, m: L.m }); drawSeq();
    });
    nm._mute = lm;

    const cells = el("div", "cells", ln);
    const row = [];
    for (let k = 0; k < NSTEP; k++) {
      const cell = el("div", "cell", cells);
      el("i", "", cell);
      cell.addEventListener("pointerdown", ev => {
        ev.preventDefault();
        const L = laneOf(ci);
        if (ev.shiftKey) { sel = { c: ci, s: k }; drawStepEdit(); drawSeq(); return; }
        L.st[k].on = L.st[k].on ? 0 : 1;
        //  a fresh step takes the velocity of the one being edited, so a run
        //  of ghost notes is painted rather than typed
        if (L.st[k].on && sel) L.st[k].vel = laneOf(sel.c).st[sel.s].vel;
        NB.send({ k: "step", p: curPat, c: ci, s: k, on: L.st[k].on, vel: L.st[k].vel });
        sel = { c: ci, s: k };
        drawSeq(); drawStepEdit();
      });
      row.push(cell);
    }
    CELL.push(row);

    const o = el("div", "lopts", ln);
    const mk = (label, get, next) => {
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
    };
    ln._opts = [
      mk("LEN",  L => L.len, (L, d) => { L.len = clamp(L.len + d, 1, NSTEP); }),
      mk("DIV",  L => DIVNAME[L.div], (L, d) => { L.div = (L.div + d + 4) % 4; }),
      mk("DIR",  L => DIRNAME[L.dir], (L, d) => { L.dir = (L.dir + d + 4) % 4; }),
      mk("SWNG", L => L.sw, (L, d) => { L.sw = clamp(L.sw + d * 5, -50, 50); })
    ];
    ln._nm = nm;
  });

  //  the step editor's five knobs are not APVTS parameters, so they get their
  //  own tiny widget rather than being forced through the parameter system
  document.querySelectorAll("#seqpage [data-s]").forEach(n => makeStepKnob(n, n.dataset.s));
  drawSeq(); drawStepEdit();
}

const SFIELD = {
  vel:  { lo: 1, hi: 127, fmt: v => String(v) },
  prob: { lo: 0, hi: 100, fmt: v => v + " %" },
  rt:   { lo: 1, hi: 8,   fmt: v => "x" + v },
  mic:  { lo: -50, hi: 50, fmt: v => (v > 0 ? "+" : "") + v + " %" },
  cond: { lo: 0, hi: 5,   fmt: v => CONDNAME[v] }
};
const SKNOB = {};

function makeStepKnob(node, field) {
  const f = SFIELD[field];
  let drag = null;
  const cur = () => (sel ? laneOf(sel.c).st[sel.s][field] : f.lo);
  const put = v => {
    if (!sel) return;
    const L = laneOf(sel.c);
    L.st[sel.s][field] = Math.round(clamp(v, f.lo, f.hi));
    const m = { k: "step", p: curPat, c: sel.c, s: sel.s };
    m[field] = L.st[sel.s][field];
    NB.send(m);
    drawStepEdit(); drawSeq();
  };
  node.addEventListener("pointerdown", e => {
    if (!sel) return;
    e.preventDefault(); node.setPointerCapture(e.pointerId);
    drag = { y: e.clientY, v: cur() };
  });
  node.addEventListener("pointermove", e => {
    if (!drag) return;
    put(drag.v + (drag.y - e.clientY) / 150 * (f.hi - f.lo));
  });
  const end = e => { if (drag) { drag = null; try { node.releasePointerCapture(e.pointerId); } catch (x) {} } };
  node.addEventListener("pointerup", end);
  node.addEventListener("pointercancel", end);
  SKNOB[field] = node;
}

function drawStepEdit() {
  const on = !!sel;
  $("#stepwho").textContent = on
    ? (CHANS[sel.c] ? CHANS[sel.c].name : "?") + "  step " + (sel.s + 1)
    : "No step selected";
  Object.keys(SFIELD).forEach(f => {
    const node = SKNOB[f]; if (!node) return;
    const spec = SFIELD[f];
    const v = on ? laneOf(sel.c).st[sel.s][f] : spec.lo;
    node.style.setProperty("--a", (-135 + 270 * clamp((v - spec.lo) / (spec.hi - spec.lo), 0, 1)) + "deg");
    node.style.opacity = on ? "1" : "0.35";
    node.title = f.toUpperCase() + "  " + spec.fmt(v);
  });
}

function drawSeq() {
  if (!SEQ) SEQ = blankSeq();
  $("#pats").childNodes.forEach && null;
  Array.prototype.forEach.call($("#pats").children, b => b.classList.toggle("on", b._pat === curPat));
  const lanes = $("#lanes").children;
  for (let c = 0; c < CELL.length; c++) {
    const L = laneOf(c);
    const ln = lanes[c];
    if (ln && ln._nm && ln._nm._mute) ln._nm._mute.classList.toggle("on", !!L.m);
    if (ln && ln._opts) ln._opts.forEach(b => { b.querySelector("b").textContent = b._get(); });
    for (let k = 0; k < NSTEP; k++) {
      const cell = CELL[c][k];
      const st = L.st[k];
      cell.classList.toggle("on", !!st.on);
      cell.classList.toggle("acc", !!st.on && st.vel > 110);
      cell.classList.toggle("dim", k >= L.len);
      cell.classList.toggle("sel", !!sel && sel.c === c && sel.s === k);
      //  the little bar under a step shows its velocity at a glance
      cell.firstChild.style.width = st.on ? (6 + 88 * (st.vel / 127)) + "%" : "0";
      cell.classList.toggle("past", playStep === k && VAL.seq >= 0.5);
    }
  }
}

/* ─── 4 · kit stepper ────────────────────────────────────────────────────── */`, "seq page js");

// running light + kit + patches
s = rep(s, `NB.on("hostParam", p => { (p.p || []).forEach(e => { if (SPEC[e.id]) setVal(e.id, e.v, false); }); });`,
`NB.on("hostParam", p => { (p.p || []).forEach(e => { if (SPEC[e.id]) setVal(e.id, e.v, false); }); });

NB.on("seq", p => {
  //  the native side is the truth; rebuild the mirror from it
  SEQ = blankSeq();
  const v = p.seq || {};
  curPat = clamp(v.cur | 0, 0, NPAT - 1);
  (v.pats || []).forEach(po => {
    const pi = clamp(po.i | 0, 0, NPAT - 1);
    (po.lanes || []).forEach(lo => {
      const c = clamp(lo.c | 0, 0, 11);
      const L = SEQ[pi][c];
      L.len = lo.len; L.div = lo.div; L.dir = lo.dir; L.sw = lo.sw; L.m = lo.m;
      (lo.st || []).forEach(e => {
        const st = L.st[clamp(e[0] | 0, 0, NSTEP - 1)];
        st.on = 1; st.vel = e[1]; st.prob = e[2]; st.rt = e[3]; st.cond = e[4]; st.mic = e[5];
      });
    });
  });
  drawSeq(); drawStepEdit();
});

NB.on("kit", p => {
  kitIdx = clamp(p.i | 0, 0, Math.max(0, KITS.length - 1));
  showKit(kitIdx);
  if (p.cat) $("#kitno").textContent = "KIT " + String(kitIdx + 1).padStart(3, "0") + "  \\u00b7  " + p.cat;
  if (CAPBTN.a) CAPBTN.a.classList.toggle("on", !!p.a);
  if (CAPBTN.b) CAPBTN.b.classList.toggle("on", !!p.b);
});

NB.on("patches", p => { say((p.files || []).length + " KITS IN " + (p.dir || "")); });`, "seq events");

s = rep(s, `NB.on("meter", p => {
  const m = p.m || [];`,
`NB.on("meter", p => {
  const m = p.m || [];
  if (typeof p.step === "number" && p.step !== playStep) { playStep = p.step; if ($("#seqpage").classList.contains("on")) drawSeq(); }`, "running light");

s = rep(s, `  CHANS = p.chans || []; KITS = p.kits || [];`,
`  CHANS = p.chans || [];
  KITS = (p.kits || []).map(x => (typeof x === "string" ? x : x.n));
  if (p.nstep) NSTEP = p.nstep;
  if (p.npat)  NPAT = p.npat;
  if (typeof p.kit === "number") kitIdx = p.kit;`, "initial kits");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("chunk F patched OK — sequencer page, morph, patches");
