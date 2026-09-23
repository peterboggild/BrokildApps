// Panel round 3: the master becomes a CHANNEL on the right, not a slider across
// the bottom. Three wins at once — it reads as a mixer, the output meter goes
// where an output meter belongs (on the master strip), and the whole bottom
// band is freed for the sequencer, which is where the next wave has to live.
// The deck goes wide and shallow (1800 x 830), which is also what a real drum
// machine is shaped like.
"use strict";
const fs = require("fs");
const miss = [];
function edit(path, subs) {
  let s = fs.readFileSync(path, "utf8");
  for (const [a, b, tag] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(path.split("/").pop() + ": " + tag + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(path, s);
}
const U = "C:/Users/peter/b/FullMetalRacket/Source/ui/ui.html";
const E = "C:/Users/peter/b/FullMetalRacket/Source/PluginEditor.cpp";

const writeU = edit(U, [

// ---- canvas: wide and shallow, like the machines this descends from -------
[`#deck{width:1720px;height:980px;transform-origin:center center;display:grid;`,
 `#deck{width:1800px;height:830px;transform-origin:center center;display:grid;`, "deck size"],
[`  const s = Math.min(window.innerWidth / 1720, window.innerHeight / 980);`,
 `  const s = Math.min(window.innerWidth / 1800, window.innerHeight / 830);`, "fit numbers"],

// ---- the rail loses the meter; it gets the machine's name down its side ---
[`#rail .fill{flex:1;display:flex;flex-direction:column;gap:6px;justify-content:flex-end;padding:6px 0}
#vu{flex:1;min-height:60px;display:flex;gap:4px;align-items:stretch;justify-content:center}
#vu .col{width:16px;border-radius:2px;position:relative;overflow:hidden;
  background:linear-gradient(180deg,#0c0803,#221a0e);
  box-shadow:inset 0 0 4px #000,0 1px 0 #ffffff18}
#vu .col b{position:absolute;left:0;right:0;bottom:0;height:0;display:block;
  background:linear-gradient(180deg,#ff5a2a,#ffb35c 38%,#c2600f);box-shadow:0 0 8px #ff8f1f88}
#vulab{font:700 7.5px/1 var(--f-lab);letter-spacing:.2em;text-transform:uppercase;
  color:#8d7d5e;text-align:center;flex:none}`,
 `#rail .fill{flex:1;display:flex;align-items:center;justify-content:center;overflow:hidden}
#rail .mark{transform:rotate(-90deg);white-space:nowrap;
  font:400 27px/1 var(--f-disp);letter-spacing:.22em;text-transform:uppercase;
  color:#4a3c26;text-shadow:0 1px 0 #ffffff10}

/* ── the master channel, thirteenth strip on the right ──────────────────── */
#mstrip{flex:none;width:132px}
#mstrip .np .nm{font-size:22px}
#vu{flex:none;height:126px;margin:0 -5px;padding:8px 26px;display:flex;gap:6px;
  align-items:stretch;justify-content:center;
  background:linear-gradient(180deg,#241c10,var(--ink));border-bottom:1px solid #0a0703}
#vu .col{flex:1;border-radius:2px;position:relative;overflow:hidden;
  background:linear-gradient(180deg,#0c0803,#1d160c);box-shadow:inset 0 0 5px #000}
#vu .col b{position:absolute;left:0;right:0;bottom:0;height:0;display:block;
  background:linear-gradient(180deg,#ff4a1c,#ffb35c 42%,#c2600f);box-shadow:0 0 9px #ff8f1f99}
#mwrap{flex:1;display:flex;align-items:stretch;justify-content:center;min-height:0;padding:8px 0}
.fader.big{width:46px;height:auto}
.fader.big .trk{width:11px;margin-left:-5.5px;border-radius:6px}
.fader.big .cap{left:0;width:46px;height:34px;border-radius:4px;
  background:linear-gradient(180deg,#f6efe0 0 40%,#8e8064 40% 60%,#c6b795 60%)}
.fader.big .cap::after{left:4px;right:4px;height:3px}
#mval{flex:none;margin:0 -5px;padding:9px 0;text-align:center;
  background:linear-gradient(180deg,#241c10,var(--ink));border-top:1px solid #0a0703;
  font:700 17px/1 var(--f-mono);color:var(--amber);text-shadow:0 0 12px #ff8f1f55}`,
 "rail + master css"],

// ---- the old bottom band goes ---------------------------------------------
[`/* ── master band ────────────────────────────────────────────────────────── */
#master{flex:none;height:98px;display:flex;align-items:center;gap:20px;padding:0 20px;border-radius:3px}
#master .tag{flex:none;display:flex;flex-direction:column;gap:3px}
#master .tag b{font:400 31px/.9 var(--f-disp);letter-spacing:.07em;color:var(--cream);text-transform:uppercase}
#master .tag i{font:700 8px/1 var(--f-lab);letter-spacing:.22em;color:#a08e6c;font-style:normal}
#mfader{flex:1;position:relative;height:44px;cursor:ew-resize;touch-action:none;min-width:0}
#mfader .trk{position:absolute;left:0;right:0;top:50%;height:10px;margin-top:-5px;border-radius:5px;
  background:linear-gradient(180deg,#0d0904,#2e2415);box-shadow:inset 0 1px 4px #000,0 1px 0 #ffffff22}
#mfader .fillbar{position:absolute;left:0;top:50%;height:10px;margin-top:-5px;border-radius:5px;
  background:linear-gradient(180deg,#ffb35c,#c2600f);box-shadow:0 0 14px #ff8f1f66}
#mfader .cap{position:absolute;top:0;width:30px;height:44px;border-radius:3px;margin-left:-15px;
  background:linear-gradient(90deg,#f2eada 0 42%,#8e8064 42% 58%,#c6b795 58%);
  border:1px solid #6f6247;box-shadow:0 3px 6px #000a,inset 0 1px 0 #fff9;pointer-events:none}
#mfader .cap::after{content:"";position:absolute;top:3px;bottom:3px;left:50%;width:2px;margin-left:-1px;
  background:var(--red);box-shadow:0 0 5px #d8412a90}
#mval{flex:none;width:132px;text-align:right;font:700 23px/1 var(--f-mono);color:var(--amber);
  text-shadow:0 0 14px #ff8f1f55}

`, ``, "old master css"],

[`    <div id="master" class="blk">
      <div class="tag"><b>Master</b><i>Output</i></div>
      <div id="mfader"><div class="trk"></div><div class="fillbar"></div><div class="cap"></div></div>
      <div id="mval">&mdash;</div>
    </div>

`, ``, "old master markup"],

// ---- the master strip joins the row --------------------------------------
[`      <div id="rail" class="blk"></div>
      <div id="strips"></div>`,
 `      <div id="rail" class="blk"></div>
      <div id="strips"></div>
      <div id="mstrip" class="strip">
        <div class="stripe" style="background:linear-gradient(90deg,#7d8184,#c9cccf,#7d8184)"></div>
        <div class="np"><div class="nm">Master</div><div class="fm">Output</div></div>
        <div id="vu"><div class="col"><b></b></div><div class="col"><b></b></div></div>
        <div id="mwrap"><div id="mfader" class="fader big"><div class="trk"></div><div class="cap"></div></div></div>
        <div id="mval">&mdash;</div>
      </div>`, "master strip markup"],

// ---- the rail's fill is now the wordmark ---------------------------------
[`  { const f = el("div", "fill", rail);
    const vu = el("div", "", f); vu.id = "vu";
    VUCOL[0] = el("b", "", el("div", "col", vu));
    VUCOL[1] = el("b", "", el("div", "col", vu));
    const l = el("div", "", f); l.id = "vulab"; l.textContent = "Out"; }`,
 `  { const f = el("div", "fill", rail);
    el("div", "mark", f).textContent = "Full Metal Racket"; }`, "rail fill"],

// ---- the master fader is vertical now ------------------------------------
[`  /* ---- the master fader */
  const mf = $("#mfader");
  dragify(mf, "volume", 520, true);
  reg("volume", { draw() {
    const t = clamp((VAL.volume || 0) / (SPEC.volume.hi || 1), 0, 1);
    const w = mf.clientWidth;
    mf.querySelector(".cap").style.left = (t * w) + "px";
    mf.querySelector(".fillbar").style.width = (t * w) + "px";
    $("#mval").textContent = fmt("volume");
  } });`,
 `  /* ---- the master channel */
  const mf = $("#mfader");
  const mcap = mf.querySelector(".cap");
  dragify(mf, "volume", 300, false);
  reg("volume", { draw() {
    const t = clamp((VAL.volume || 0) / (SPEC.volume.hi || 1), 0, 1);
    mcap.style.top = ((1 - t) * Math.max(0, mf.clientHeight - 34)) + "px";
    $("#mval").textContent = fmt("volume");
  } });
  VUCOL[0] = $("#vu").children[0].firstElementChild;
  VUCOL[1] = $("#vu").children[1].firstElementChild;`, "master wiring"],

// ---- redraw the master on resize too -------------------------------------
[`  if (WIDG.volume) WIDG.volume.forEach(x => x.draw());`,
 `  requestAnimationFrame(() => { if (WIDG.volume) WIDG.volume.forEach(x => x.draw()); });`, "resize redraw"]
]);

// the editor opens at the deck's own aspect, so there is no letterbox at all
const writeE = edit(E, [
  [`    setResizeLimits (980, 558, 3800, 2200);
    setSize (1720, 980);           // exactly the deck's canvas: no letterbox at open`,
   `    setResizeLimits (1000, 462, 3800, 1900);
    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open`,
   "editor size"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
writeU(); writeE();
console.log("panel round 3 patched OK");
