/*  B2311.67 260902.2, part two — the wheel spills past the slider, and the
    void gets a pulse so it is plainly still running.
*/
const fs = require('fs');
const files = {};
function load (p) {
  const s = fs.readFileSync(p, 'utf8');
  files[p] = { s, nl: s.indexOf(String.fromCharCode(13,10)) >= 0
                      ? String.fromCharCode(13,10) : String.fromCharCode(10) };
}
const miss = [];
function rep (p, find, into) {
  const f = files[p];
  const n = f.s.split(find).length - 1;
  if (n !== 1) { miss.push(p.split('/').pop() + ' [' + n + 'x] ' + find.split(f.nl)[0].slice(0, 56)); return; }
  f.s = f.s.replace(find, into);
}
const J = (p, a) => a.join(files[p].nl);

const UI  = 'C:/Users/peter/b/ArtefactB2311_67/Source/ui/ui.html';
const PRC = 'C:/Users/peter/b/ArtefactB2311_67/Source/PluginProcessor.cpp';
load(UI); load(PRC);

//  ---------------------------------------------------- presence on the wire
rep(PRC,
    '    emitToUi ("body", juce::var (obj));',
    J(PRC, [
'    /*  How much of the fragment the cut is passing through. The page draws the',
'        body by it, so the picture and the sound cannot disagree about whether',
'        there is anything here. */',
'    obj->setProperty ("pres", (double) engine.presencePub.load (std::memory_order_relaxed));',
'    emitToUi ("body", juce::var (obj));']));

//  ------------------------------------------------- the wheel with no end
rep(UI,
    J(UI, [
'window.addEventListener("wheel", ev => {',
'  ev.preventDefault();',
'  const lo = LO.travel !== undefined ? LO.travel : -12, hi = FHI.travel !== undefined ? FHI.travel : 12;',
'  const span = hi - lo;',
'  const d = -ev.deltaY / 1400 * (ev.shiftKey ? 0.22 : 1.0) * (24 / span);',
'  setParam("travel", clamp((V.travel !== undefined ? V.travel : .5) + d, 0, 1), true);',
'  say("TRAVERSE", paramToTau().toFixed(3), "depth of the cut in the fourth dimension");',
'}, { passive: false });']),
    J(UI, [
'/*  THE WHEEL HAS NO END. TRAVERSE is a host parameter and therefore lives on',
'    nought to one, and the slider keeps that limit — but the wheel does not.',
'    Once the slider is against a stop the rest of the turn accumulates in the',
'    engine`s own offset, which is unbounded, and unwinds first on the way back',
'    so that returning is not a long scroll against a pinned control.',
'',
'    Nothing happens out there. Almost nothing. */',
'S.tauOff = 0;',
'window.addEventListener("wheel", ev => {',
'  if (ev.target && ev.target.closest && ev.target.closest(".bwfx-veil")) return;',
'  ev.preventDefault();',
'  const lo = LO.travel !== undefined ? LO.travel : -12, hi = FHI.travel !== undefined ? FHI.travel : 12;',
'  const span = hi - lo;',
'  const d = -ev.deltaY / 1400 * (ev.shiftKey ? 0.22 : 1.0) * (24 / span);',
'  let dTau = d * 24;',
'',
'  //  unwind whatever the wheel has already banked, before moving the slider',
'  if (S.tauOff !== 0 && (dTau > 0) !== (S.tauOff > 0)) {',
'    const take = Math.sign(dTau) * Math.min(Math.abs(dTau), Math.abs(S.tauOff));',
'    S.tauOff += take; dTau -= take;',
'    NB.send({ k: "twheel", d: take });',
'  }',
'  if (dTau !== 0) {',
'    const cur = V.travel !== undefined ? V.travel : .5;',
'    const want = cur + dTau / 24;',
'    const got = clamp(want, 0, 1);',
'    if (got !== cur) setParam("travel", got, true);',
'    const spill = (want - got) * 24;',
'    if (spill !== 0) { S.tauOff += spill; NB.send({ k: "twheel", d: spill }); }',
'  }',
'  const total = paramToTau() + S.tauOff;',
'  say("TRAVERSE", total.toFixed(2),',
'      Math.abs(total) > 10.4 ? "the plane has left the fragment"',
'                             : "depth of the cut in the fourth dimension");',
'}, { passive: false });']));

//  ------------------------------------------------------ the pulse in the void
rep(UI,
    J(UI, [
'function drawUI(t) {',
'  const W = uic.width, H = uic.height;',
'  cx2.setTransform(DPR, 0, 0, DPR, 0, 0);',
'  cx2.clearRect(0, 0, W/DPR, H/DPR);',
'  roseLayout();']),
    J(UI, [
'function drawUI(t) {',
'  const W = uic.width, H = uic.height;',
'  cx2.setTransform(DPR, 0, 0, DPR, 0, 0);',
'  cx2.clearRect(0, 0, W/DPR, H/DPR);',
'  roseLayout();',
'',
'  /*  OUT OF RANGE: the body has come apart and there is nothing to hear, so',
'      the only thing on the plate is a slow pulse at the centre of the section',
'      — the apparatus still running with nothing in front of it. It is drawn',
'      from the same presence the engine silenced the object with, so it can',
'      only appear where there is genuinely no cross-section. */',
'  if (S.pres !== undefined && S.pres < 0.02) {',
'    const gone = 1 - S.pres / 0.02;',
'    const ph = 0.5 - 0.5 * Math.cos(t * 1.9);',
'    const r = 2.2 + 2.6 * ph;',
'    cx2.save();',
'    cx2.globalAlpha = (0.30 + 0.55 * ph) * gone;',
'    const g = cx2.createRadialGradient(VIEW.cx, VIEW.cy, 0, VIEW.cx, VIEW.cy, r * 7);',
'    g.addColorStop(0, "rgba(79,214,196,.95)");',
'    g.addColorStop(1, "rgba(79,214,196,0)");',
'    cx2.fillStyle = g;',
'    cx2.beginPath(); cx2.arc(VIEW.cx, VIEW.cy, r * 7, 0, 6.2831853); cx2.fill();',
'    cx2.globalAlpha = (0.55 + 0.45 * ph) * gone;',
'    cx2.fillStyle = "#9ff0e4";',
'    cx2.beginPath(); cx2.arc(VIEW.cx, VIEW.cy, r, 0, 6.2831853); cx2.fill();',
'    cx2.restore();',
'  }']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
for (const p of Object.keys(files)) fs.writeFileSync(p, files[p].s);
console.log('patched: presence on the wire, unbounded wheel, the pulse in the void');
