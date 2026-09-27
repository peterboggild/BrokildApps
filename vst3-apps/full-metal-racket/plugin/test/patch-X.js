// MORPH LOOKED BROKEN FOR TWO REASONS, AND BOTH ARE MINE.
//
// 1. IT MORPHED THE WRONG HALF OF THE KIT. applyMorph skipped every global,
//    with a comment claiming globals are not a kit's character — but the kit
//    GENERATOR sets RAIL SAG, BLEED, KIT BODY, AGE and KIT TUNE, and those
//    are a great deal of what makes two kits sound unalike. So a morph
//    between two seeds left the machine's character exactly where it was and
//    moved only the twelve voices. The two halves of the same idea disagreed
//    about what a kit is.
//
// 2. NOTHING ON THE PANEL MOVED. Morph is applied to the engine's copy of the
//    parameters and never written back — which is right, because a fader that
//    rewrote a hundred knobs would wreck its own automation — but it means
//    the panel shows the endpoints while the ears hear something between
//    them. With a weak morph and a still panel, "it does not work" is the
//    only reasonable conclusion to draw.
//
//    Fixed by sending the two captured kits to the panel and computing the
//    SAME blend there for display. Same arithmetic, so the two cannot
//    disagree, and the knobs now sweep as you move the fader.
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

// ── 1 · morph what a kit actually is ───────────────────────────────────────
const wc = edit(R + "Source/Engine.cpp", [
[`    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        if (s.chan < 0) continue;                       // globals are not a kit's character
        if (s.slot == CP_MUTE) continue;                // a morph must not silence a channel`,
`    for (int i = 0; i < numParams(); ++i)
    {
        const PSpec& s = paramSpec (i);
        if (! morphable (s)) continue;`, "morph loop"],

[`void Engine::applyMorph (Params& dst) const
{`,
`/*  What belongs to a KIT, and therefore morphs.
    Everything on a channel except its mute, plus the five globals the kit
    generator writes — RAIL SAG, BLEED, KIT BODY, AGE and KIT TUNE. Leaving
    those out was the bug: a morph between two seeds moved the twelve voices
    and left the machine's character exactly where it was, which is most of
    what makes two kits sound unalike.

    Deliberately NOT morphed: the master, the oversampling, and the whole
    transport — those belong to the performance, not to the sound. The same
    list applyKitIndex leaves alone, and it should stay the same list. */
bool morphable (const PSpec& s)
{
    if (s.chan >= 0) return s.slot != CP_MUTE;
    switch (s.slot)
    {
        case GP_SAG: case GP_BLEED: case GP_BODY: case GP_AGE: case GP_KITTUNE: return true;
        default: return false;
    }
}

void Engine::applyMorph (Params& dst) const
{`, "morphable fn"]
]);

const wh = edit(R + "Source/Engine.h", [
[`inline float& pvalue (Params& p, const PSpec& s)`,
 `/*  Whether a parameter is part of a KIT — so it morphs, as opposed to
    belonging to the performance. */
bool morphable (const PSpec& s);

inline float& pvalue (Params& p, const PSpec& s)`, "decl"]
]);

// ── 2 · the panel shows the blend ──────────────────────────────────────────
const wp = edit(R + "Source/PluginProcessor.cpp", [
[`    o->setProperty ("a", engine.haveA);
    o->setProperty ("b", engine.haveB);
    emitToUi ("kit", juce::var (o));`,
`    o->setProperty ("a", engine.haveA);
    o->setProperty ("b", engine.haveB);
    /*  The captured kits go to the panel so it can show the blend as the
        fader moves. Sent on capture rather than per frame — it is a few
        hundred numbers, and they only change when something is captured. */
    auto pack = [] (const fmr::Params& q)
    {
        auto* k = new juce::DynamicObject();
        for (int i = 0; i < fmr::numParams(); ++i)
        {
            const auto& sp = fmr::paramSpec (i);
            if (fmr::morphable (sp))
                k->setProperty (sp.id, (double) fmr::pvalue (const_cast<fmr::Params&> (q), sp));
        }
        return juce::var (k);
    };
    if (engine.haveA) o->setProperty ("kitA", pack (engine.kitA));
    if (engine.haveB) o->setProperty ("kitB", pack (engine.kitB));
    emitToUi ("kit", juce::var (o));`, "emit kits"]
]);

const wu = edit(R + "Source/ui/ui.html", [
[`const LINKED = { oh_tune: "ch_tune", oh_model: "ch_model" };`,
`const LINKED = { oh_tune: "ch_tune", oh_model: "ch_model" };

/*  The two captured kits, and the same blend the engine computes. Doing the
    arithmetic in both places sounds like a way to have them disagree; it is
    the opposite — the alternative was the panel showing the endpoints while
    the ears heard something between them, which is what made a working morph
    look broken. */
let KITA = null, KITB = null;
function morphView(id) {
  if (!KITA || !KITB) return null;
  const t = VAL.morph || 0;
  if (t <= 0) return null;
  const a = KITA[id], b = KITB[id];
  if (typeof a !== "number" || typeof b !== "number") return null;
  const sp = SPEC[id];
  if (sp && (sp.kind === KIND.LIST || sp.kind === KIND.SW)) {
    //  the same staggered thresholds the engine uses, so a switched value
    //  flips on the panel at the moment it flips in the sound
    const i = Object.keys(SPEC).indexOf(id);
    const th = 0.30 + 0.40 * ((i * 37) % 100) / 100;
    return t < th ? a : b;
  }
  return a + (b - a) * t;
}
function viewVal(id) {
  const m = morphView(id);
  return m === null ? (VAL[id] || 0) : m;
}`, "morphView"],

// knobs and faders read the blended value
[`  const w = { draw() {
    const eff = linkTarget(id);
    const t = clamp((VAL[eff] || 0) / hi, 0, 1);`,
 `  const w = { draw() {
    const eff = linkTarget(id);
    const t = clamp(viewVal(eff) / hi, 0, 1);`, "knob view"],

[`  reg(id, { draw() {
    const t = clamp((VAL[id] || 0) / (s.hi || 1), 0, 1);
    cap.style.top = ((1 - t) * Math.max(0, node.clientHeight - 26)) + "px";
    node.title = s.n + "  " + fmt(id);
  } });`,
 `  reg(id, { draw() {
    const t = clamp(viewVal(id) / (s.hi || 1), 0, 1);
    cap.style.top = ((1 - t) * Math.max(0, node.clientHeight - 26)) + "px";
    node.title = s.n + "  " + fmt(id);
  } });`, "fader view"],

[`  const w = { draw() {
    const eff = linkTarget(id);
    const v = Math.round(VAL[eff] || 0);`,
 `  const w = { draw() {
    const eff = linkTarget(id);
    const v = Math.round(viewVal(eff));`, "segs view"],

// moving MORPH redraws the panel
[`NB.on("kit", p => {`,
`function redrawAll() { Object.keys(WIDG).forEach(id => WIDG[id].forEach(w => w.draw())); }

NB.on("kit", p => {
  KITA = p.kitA || null;
  KITB = p.kitB || null;`, "kit event"],

[`  if (CAPBTN.a) CAPBTN.a.classList.toggle("on", !!p.a);
  if (CAPBTN.b) CAPBTN.b.classList.toggle("on", !!p.b);
});`,
`  if (CAPBTN.a) CAPBTN.a.classList.toggle("on", !!p.a);
  if (CAPBTN.b) CAPBTN.b.classList.toggle("on", !!p.b);
  redrawAll();
});`, "kit redraw"]
]);

if (miss.length) { console.error("ABORT:\n  " + miss.join("\n  ")); process.exit(1); }
wc(); wh(); wp(); wu();
console.log("morph covers the kit, and the panel shows the blend");
