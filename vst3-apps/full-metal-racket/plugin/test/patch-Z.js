// The transport, on the panel: run, pause, stop, record — as symbols.
// And the kit display says when what you are hearing came from a file.
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

// ── CSS ────────────────────────────────────────────────────────────────────
s = rep(s, `.pats{display:flex;gap:3px}`,
`.pats{display:flex;gap:3px}
/*  A transport, in symbols. Sized so they are hit targets rather than
    decorations, and RECORD is red when armed because a record button that
    does not shout is how you lose a take. */
.tport{display:flex;gap:4px;align-items:center}
.tport .k{width:38px;height:32px;padding:0;font-size:13px;letter-spacing:0}
.tport .k.rec.on{background:linear-gradient(180deg,#e0473a,#a3120a)!important;
  border-color:#7a0d06!important;color:#fff!important;
  box-shadow:inset 0 1px 0 #ffffff44,0 0 14px #d8412a99!important}
.tport .k.run.on{background:linear-gradient(180deg,#4fbf6a,#1f7a3a)!important;
  border-color:#155c2a!important;color:#f0fff4!important;
  box-shadow:inset 0 1px 0 #ffffff44,0 0 14px #35b25a88!important}
#kitno.user{color:var(--red)}`, "transport css");

// ── markup: replace the lone RUN key with the transport ────────────────────
s = rep(s, `          <button class="k dark" id="seqrun">Run</button>`,
`          <div class="tport">
            <button class="k dark run" id="t-run" title="Run from step one">&#9654;</button>
            <button class="k dark" id="t-pause" title="Pause where it is">&#10074;&#10074;</button>
            <button class="k dark" id="t-stop" title="Stop and rewind to step one">&#9632;</button>
            <button class="k dark rec" id="t-rec" title="Record: play the pads or MIDI to write steps">&#9679;</button>
          </div>`, "transport markup");

// ── JS ─────────────────────────────────────────────────────────────────────
s = rep(s, `  { const b = $("#seqrun");
    b.addEventListener("click", () => setVal("seq", VAL.seq >= 0.5 ? 0 : 1));
    reg("seq", { draw() { b.classList.toggle("on", VAL.seq >= 0.5); b.textContent = VAL.seq >= 0.5 ? "Running" : "Run"; } }); }`,
`  /*  RUN restarts from step one; the pause key is what continues from where
      it stopped. Two keys rather than one toggle, because "start again" and
      "carry on" are different intentions and a single button has to guess. */
  $("#t-run").addEventListener("click", () => NB.send({ k: "transport", what: "run" }));
  $("#t-pause").addEventListener("click", () => NB.send({ k: "transport", what: VAL.seq >= 0.5 ? "pause" : "cont" }));
  $("#t-stop").addEventListener("click", () => NB.send({ k: "transport", what: "stop" }));
  $("#t-rec").addEventListener("click", () => { recArm = !recArm; NB.send({ k: "rec", on: recArm ? 1 : 0 }); drawTransport(); });
  reg("seq", { draw: drawTransport });`, "transport js");

s = rep(s, `let NSTEP = 32, NPAT = 16, curPat = 0, playStep = -1;`,
`let NSTEP = 32, NPAT = 16, curPat = 0, playStep = -1, recArm = false;
function drawTransport() {
  const run = $("#t-run"), rec = $("#t-rec"), pause = $("#t-pause");
  if (run) run.classList.toggle("on", VAL.seq >= 0.5);
  if (rec) rec.classList.toggle("on", recArm);
  if (pause) pause.title = VAL.seq >= 0.5 ? "Pause where it is" : "Continue from here";
}`, "transport state");

// SPACE keeps working, and now means run/pause rather than a raw toggle
s = rep(s, `    if (SPEC.seq) { setVal("seq", VAL.seq >= 0.5 ? 0 : 1); say(VAL.seq >= 0.5 ? "RUNNING" : "STOPPED"); }`,
`    if (SPEC.seq) NB.send({ k: "transport", what: VAL.seq >= 0.5 ? "pause" : "cont" });`, "space transport");

// ── the kit display names a loaded file ────────────────────────────────────
s = rep(s, `  if (p.cat) $("#kitno").textContent = "KIT " + String(kitIdx + 1).padStart(3, "0") + "  \\u00b7  " + p.cat;`,
`  /*  What you are hearing came from a file, and the panel should say so —
      it used to keep naming whichever seed was last dialled. */
  if (p.user) {
    $("#kitname").textContent = p.name || "\\u2014";
    $("#kitno").textContent = "USER PATCH";
    $("#kitno").classList.add("user");
  } else if (p.cat) {
    $("#kitno").textContent = "KIT " + String(kitIdx + 1).padStart(3, "0") + "  \\u00b7  " + p.cat;
    $("#kitno").classList.remove("user");
  }
  if (typeof p.rec === "boolean") { recArm = p.rec; drawTransport(); }`, "kit display");

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
fs.writeFileSync(P, s);
console.log("transport symbols, and the panel names a loaded patch");
