// FOUR THINGS.
//
// 1. THE FONT. Peter is right — the rail buttons read better than the display
//    face. They were Segoe UI at 700; the display face was Haettenschweiler
//    falling back to Impact, which is loud rather than confident. One family
//    throughout now. Segoe is much wider than Impact at the same size, so
//    every display size comes down with it or the words stop fitting.
//
// 2. THE LAST-STEP CONTROL WAS INVISIBLE. It was there — LAST/DIV/DIR/SWNG at
//    the right of every lane — but drawn as dim text on a dark block, so it
//    did not read as something you could touch. Given a control that exists
//    and a user who cannot find it, the control does not exist.
//
// 3. LOCK THE ASPECT. JUCE will do it: the constrainer takes a fixed ratio,
//    so the window cannot be dragged into a shape the deck has to letterbox.
//
// 4. THE PLAYHEAD IGNORED LANE LENGTH. It was a global sixteenth counter mod
//    32 — so with LAST at 11 the red column carried on to 32 while the lane
//    had already looped. Each lane reports its OWN position now, which is
//    also the only honest way to show polymeter.
"use strict";
const fs = require("fs");
const miss = [];
function edit(p, subs) {
  let s = fs.readFileSync(p, "utf8");
  for (const [a, b, t] of subs) {
    const n = s.split(a).length - 1;
    if (n !== 1) { miss.push(p.split("/").pop() + ": " + t + " x" + n); continue; }
    s = s.split(a).join(b);
  }
  return () => fs.writeFileSync(p, s);
}
const R = "C:/Users/peter/b/FullMetalRacket/";

// ── 4 · per-lane playhead, engine side ─────────────────────────────────────
const wh = edit(R + "Source/Engine.h", [
[`    int  seqStep() const { return uiStep; }          // for the panel's running light`,
 `    int  seqStep() const { return uiStep; }          // for the panel's running light
    /*  Each lane's OWN position. A single global counter cannot describe a
        machine whose lanes have different lengths and divisions — with LAST
        at 11 the light ran on to 32 while the lane had already looped. */
    int  seqStepFor (int c) const { return uiLaneStep[(size_t) (c < 0 ? 0 : (c >= NCH ? NCH - 1 : c))]; }`, "api"],
[`    int    uiStep = -1;`,
 `    int    uiStep = -1;
    std::array<int, NCH> uiLaneStep { };`, "member"]
]);

const wc = edit(R + "Source/Engine.cpp", [
[`void Engine::runSequencer (int n)
{
    if (p.g[GP_SEQ] < 0.5f) { uiStep = -1; return; }`,
`void Engine::runSequencer (int n)
{
    if (p.g[GP_SEQ] < 0.5f) { uiStep = -1; uiLaneStep.fill (-1); return; }`, "off"],
[`        const double sl = STEPLEN[L.div & 3];
        const double sa = a * 4.0 / sl, sb = b * 4.0 / sl;
        const double stepSamples = sl / 4.0 * 60.0 / bpm * fs;`,
`        const double sl = STEPLEN[L.div & 3];
        const double sa = a * 4.0 / sl, sb = b * 4.0 / sl;
        const double stepSamples = sl / 4.0 * 60.0 / bpm * fs;

        //  where THIS lane is, in its own length and its own division
        {
            const long long kNow = (long long) std::floor (sa);
            uiLaneStep[(size_t) c] = kNow < 0 ? -1 : laneIndex (L, kNow);
        }`, "per lane"],
[`        const Lane& L = P.lane[c];
        if (L.mute || p.ch[c][CP_MUTE] >= 0.5f) continue;`,
 `        const Lane& L = P.lane[c];
        if (L.mute || p.ch[c][CP_MUTE] >= 0.5f) { uiLaneStep[(size_t) c] = -1; continue; }`, "muted lane"],
[`    rr.fill (0); meter.fill (0.0f); sympExc.fill (0.0f);
    nHits = 0; uiStep = -1;`,
 `    rr.fill (0); meter.fill (0.0f); sympExc.fill (0.0f);
    nHits = 0; uiStep = -1; uiLaneStep.fill (-1);`, "reset"]
]);

const wp = edit(R + "Source/PluginProcessor.cpp", [
[`        obj->setProperty ("step", engine.seqStep());`,
`        juce::Array<juce::var> ls;
        for (int c = 0; c < fmr::NCH; ++c) ls.add (engine.seqStepFor (c));
        obj->setProperty ("step", engine.seqStep());
        obj->setProperty ("steps", ls);`, "emit lane steps"]
]);

// ── 3 · lock the aspect ────────────────────────────────────────────────────
const we = edit(R + "Source/PluginEditor.cpp", [
[`    setResizeLimits (1000, 462, 3800, 1900);
    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open`,
`    setResizeLimits (1000, 462, 3800, 1900);
    /*  Locked to the deck's own ratio, so the window cannot be dragged into a
        shape the panel then has to letterbox inside. */
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1800.0 / 830.0);
    setSize (1800, 830);           // exactly the deck's canvas: no letterbox at open`, "aspect"]
]);

// ── 1 and 2 · the panel ────────────────────────────────────────────────────
const wu = edit(R + "Source/ui/ui.html", [
[`  --f-disp:"Haettenschweiler","Impact","Arial Narrow","Segoe UI",sans-serif;
  --f-lab:"Segoe UI",system-ui,sans-serif;`,
`  /*  One family throughout. The condensed display face was loud where this is
      confident, and Peter is right that the buttons always read better than
      the headings. Segoe runs much wider than Impact at the same size, so
      every display size below came down with it. */
  --f-disp:"Segoe UI",system-ui,-apple-system,sans-serif;
  --f-lab:"Segoe UI",system-ui,sans-serif;`, "font stack"],

//  display sizes, all reduced for the wider face, and given a real weight
[`#plate b{font:400 34px/.86 var(--f-disp);letter-spacing:.045em;color:#191d22;text-transform:uppercase}`,
 `#plate b{font:700 24px/1 var(--f-disp);letter-spacing:.015em;color:#191d22;text-transform:uppercase}`, "plate"],
[`#kitname{font:400 30px/1 var(--f-disp);letter-spacing:.05em;text-transform:uppercase;`,
 `#kitname{font:700 23px/1 var(--f-disp);letter-spacing:.03em;text-transform:uppercase;`, "kitname"],
[`#webtag b{font:400 27px/.9 var(--f-disp);letter-spacing:.07em;color:var(--cream);text-transform:uppercase}`,
 `#webtag b{font:700 20px/1.05 var(--f-disp);letter-spacing:.03em;color:var(--cream);text-transform:uppercase}`, "webtag"],
[`.np .nm{font:400 24px/1 var(--f-disp);letter-spacing:.05em;color:var(--cream);text-transform:uppercase}`,
 `.np .nm{font:700 17px/1 var(--f-disp);letter-spacing:.03em;color:var(--cream);text-transform:uppercase}`, "channel name"],
[`#mstrip .np .nm{font-size:22px}`, `#mstrip .np .nm{font-size:16px}`, "mstrip name"],
[`.lname b{flex:1;font:400 17px/1 var(--f-disp);letter-spacing:.05em;color:var(--cream);text-transform:uppercase}`,
 `.lname b{flex:1;font:700 13px/1 var(--f-disp);letter-spacing:.04em;color:var(--cream);text-transform:uppercase}`, "lane name"],
[`.stepedit .who{font:400 19px/1 var(--f-disp);letter-spacing:.05em;color:var(--amber);`,
 `.stepedit .who{font:700 15px/1 var(--f-disp);letter-spacing:.04em;color:var(--amber);`, "step who"],
[`#menu .mh b{flex:1;font:400 22px/1 var(--f-disp);letter-spacing:.06em;`,
 `#menu .mh b{flex:1;font:700 17px/1 var(--f-disp);letter-spacing:.04em;`, "menu head"],

// ---- 2 · the lane controls look like controls -----------------------------
[`.lopts{flex:none;width:150px;display:flex;align-items:center;gap:2px;padding:0 4px;border-radius:2px;
  background:linear-gradient(180deg,#2b2213,var(--ink));border:1px solid #0a0703}
.lopts .o{flex:1;text-align:center;cursor:pointer;border-radius:2px;padding:3px 0;
  font:700 9px/1.25 var(--f-lab);letter-spacing:.05em;color:#c9b894;background:#00000030}
.lopts .o:hover{background:#ffffff18}
.lopts .o b{display:block;font:700 11px/1.1 var(--f-lab);color:var(--cream)}`,
`.lopts{flex:none;width:176px;display:flex;align-items:stretch;gap:3px;padding:3px;border-radius:2px;
  background:linear-gradient(180deg,#2b2213,var(--ink));border:1px solid #0a0703}
/*  These read as buttons now. They always were controls; drawn as dim text on
    a dark block they looked like a readout, and a control nobody can find is
    a control that does not exist. */
.lopts .o{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:1px;
  cursor:pointer;border-radius:2px;padding:2px 0;
  background:linear-gradient(180deg,#4a3a22,#2a2013);border:1px solid #0f0b06;
  box-shadow:inset 0 1px 0 #ffffff22,0 1px 2px #0006;
  font:700 7.5px/1 var(--f-lab);letter-spacing:.12em;color:#a8946f}
.lopts .o:hover{background:linear-gradient(180deg,#5f4b2c,#372a19);color:#d8c5a0}
.lopts .o:active{background:linear-gradient(180deg,#c95a2e,#a32d12);color:#fff3e4}
.lopts .o b{display:block;font:700 13px/1.15 var(--f-lab);letter-spacing:.02em;color:var(--cream)}
.lopts .o.wide{flex:1.25}`, "lopts style"],

//  LAST gets the extra width, and the whole block a heading
[`      }, { get: L => L.len, set: (L, v) => { L.len = clamp(Math.round(v), 1, NSTEP); } }),`,
 `      }, { get: L => L.len, set: (L, v) => { L.len = clamp(Math.round(v), 1, NSTEP); } }),`, "noop"],
[`    ln._opts = [
      mk("LAST",`,
 `    ln._opts = [
      mk("LAST",`, "noop2"],

// ---- 4 · the playhead follows each lane -----------------------------------
[`  if (typeof p.step === "number" && p.step !== playStep) { playStep = p.step; if ($("#seqpage").classList.contains("on")) drawSeq(); }`,
`  /*  Per lane, so a lane with its own LAST step or its own division shows its
      own position — a single counter could only ever be right for one lane. */
  if (p.steps && p.steps.length) {
    let moved = false;
    for (let i = 0; i < p.steps.length; i++) if (p.steps[i] !== laneStep[i]) { laneStep[i] = p.steps[i]; moved = true; }
    if (moved && $("#seqpage").classList.contains("on")) drawSeq();
  }`, "meter steps"],

[`let NSTEP = 32, NPAT = 16, curPat = 0, playStep = -1;`,
 `let NSTEP = 32, NPAT = 16, curPat = 0, playStep = -1;
const laneStep = new Array(12).fill(-1);`, "lanestep state"],

[`      cell.classList.toggle("past", playStep === k && VAL.seq >= 0.5);`,
 `      cell.classList.toggle("past", laneStep[c] === k && VAL.seq >= 0.5);`, "draw playhead"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wh(); wc(); wp(); we(); wu();
console.log("one font, visible LAST control, locked aspect, per-lane playhead");
