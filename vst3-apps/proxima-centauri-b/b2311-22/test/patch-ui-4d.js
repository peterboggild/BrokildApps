/*  B2311.22 260902.2 — the two big sliders on the face.

    Peter: the 4D control as a larger slider like TEMPERATURE on the other
    artefacts, labelled "4D PULL/PUSH?" with "(Try wheel)" so it can be
    guessed; and .22 should carry the same big kelvin slider its siblings do,
    down to 77 K.
*/
const fs = require('fs');
const P = 'C:/Users/peter/b/ArtefactB2311/Source/ui/ui.html';
let s = fs.readFileSync(P, 'utf8');
const NL = s.indexOf(String.fromCharCode(13,10)) >= 0
  ? String.fromCharCode(13,10) : String.fromCharCode(10);
const miss = [];
const J = a => a.join(NL);
function rep (find, into) {
  const n = s.split(find).length - 1;
  if (n !== 1) { miss.push('[' + n + 'x] ' + find.split(NL)[0].slice(0, 64)); return; }
  s = s.replace(find, into);
}

//  ------------------------------------------------------------------- style
rep('  #cat { color:#5d716d; margin-right:2px; }',
    J([
'  /* ── the two big instruments on the frame: the traverse, and the kelvin',
'        reading the frame has always carried ── */',
'  #bigs { position:absolute; right:14px; top:52px; z-index:5; width:190px;',
'          display:flex; flex-direction:column; gap:14px; user-select:none; }',
'  .big { background:rgba(8,13,18,.72); border:1px solid #22303a; border-radius:4px;',
'         padding:8px 10px 10px; }',
'  .big .lab { font-size:9.5px; letter-spacing:.14em; color:#9db3af; }',
'  .big .hint { font-size:9px; letter-spacing:.06em; color:#5f7views; }',
'  .big .rd { font-size:11px; letter-spacing:.06em; color:#69d2c4; float:right; }',
'  .big .trk { position:relative; height:12px; margin-top:9px; cursor:ew-resize;',
'              border:1px solid #2a3841; border-radius:6px; background:rgba(4,8,12,.7); }',
'  .big .fil { position:absolute; left:1px; top:1px; bottom:1px; border-radius:5px;',
'              background:linear-gradient(90deg,#1d4f4a,#69d2c4); pointer-events:none; }',
'  .big .kn  { position:absolute; top:-3px; width:6px; height:16px; border-radius:3px;',
'              background:#d6efe9; box-shadow:0 0 6px rgba(105,210,196,.7); pointer-events:none; }',
'  /*  the empty space either side of the tissue, drawn so the traverse shows',
'      you that there IS an outside */',
'  .big .void { position:absolute; top:0; bottom:0; background:rgba(90,20,30,.30);',
'               pointer-events:none; }',
'  #cat { color:#5d716d; margin-right:2px; }']));

//  fix the colour typo introduced above (kept deliberate: one source of truth)
rep('color:#5f7views;', 'color:#5f7a75;');

//  -------------------------------------------------------------------- markup
rep('<div id="clip" title="The harness',
    J([
'<div id="bigs">',
'  <div class="big" id="big4d">',
'    <div><span class="lab">4D PULL/PUSH?</span><span class="rd" id="rd4d">0</span></div>',
'    <div class="hint">(Try wheel)</div>',
'    <div class="trk" id="trk4d">',
'      <div class="void" style="left:1px;width:19%"></div>',
'      <div class="void" style="right:1px;width:19%"></div>',
'      <div class="fil" id="fil4d"></div><div class="kn" id="kn4d"></div>',
'    </div>',
'  </div>',
'  <div class="big" id="bigT">',
'    <div><span class="lab">TEMPERATURE</span><span class="rd" id="rdT">293 K</span></div>',
'    <div class="hint">77 K &mdash; 800 K</div>',
'    <div class="trk" id="trkT">',
'      <div class="fil" id="filT"></div><div class="kn" id="knT"></div>',
'    </div>',
'  </div>',
'</div>',
'<div id="clip" title="The harness']));

//  --------------------------------------------------------------- behaviour
rep('/* ── the harness rail ───────────────────────────────────────────────────── */',
    J([
'/* ── the two big instruments ────────────────────────────────────────────── */',
'/*  Both are the same shape: a track, a fill, a knob, and a readout. The 4D',
'    one is the WINCH parameter under its researchers\u0027 name — the id does not',
'    change, so every saved observation and every automation lane still finds',
'    it. */',
'function bindBig (id, track, fill, knob, readout, fmt) {',
'  const trk = document.getElementById(track);',
'  const fil = document.getElementById(fill);',
'  const kn  = document.getElementById(knob);',
'  const rd  = document.getElementById(readout);',
'  let drag = false;',
'  const setFromX = (clientX) => {',
'    const r = trk.getBoundingClientRect();',
'    sendParam(id, Math.max(0, Math.min(1, (clientX - r.left) / Math.max(1, r.width))));',
'  };',
'  trk.addEventListener("pointerdown", e => {',
'    drag = true; try { trk.setPointerCapture(e.pointerId); } catch (err) {}',
'    setFromX(e.clientX); e.preventDefault();',
'  });',
'  trk.addEventListener("pointermove", e => { if (drag) setFromX(e.clientX); });',
'  trk.addEventListener("pointerup",   () => { drag = false; });',
'  trk.addEventListener("pointercancel", () => { drag = false; });',
'  //  the wheel works on the instrument itself as well as on the face',
'  trk.addEventListener("wheel", e => {',
'    e.preventDefault();',
'    const d = (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? 0.008 : 0.075);',
'    sendParam(id, (VAL[id] !== undefined ? VAL[id] : 0.5) + d);',
'  }, { passive: false });',
'  return () => {',
'    const v = VAL[id] !== undefined ? VAL[id] : 0.5;',
'    const w = trk.clientWidth - 2;',
'    fil.style.width = Math.max(0, v * w) + "px";',
'    kn.style.left = Math.max(0, Math.min(w - 3, v * w)) + "px";',
'    rd.textContent = fmt(v);',
'  };',
'}',
'/*  The traverse reads as where the section stands, not as a percentage: the',
'    number the researchers wrote down was the depth, and past about +-1 the',
'    tissue is behind you. */',
'const draw4d = bindBig("winch", "trk4d", "fil4d", "kn4d", "rd4d", v => {',
'  const x = (v - 0.5) * 2;',
'  const w = (x < 0 ? -1 : 1) * Math.pow(Math.abs(x), 1.9) * 2.2;',
'  return (Math.abs(w) > 1.05 ? "OUT " : "") + w.toFixed(2);',
'});',
'const drawT = bindBig("temp", "trkT", "filT", "knT", "rdT",',
'                      v => Math.round(77 + 723 * v) + " K");',
'function updateBigs () { draw4d(); drawT(); }',
'',
'/* ── the harness rail ───────────────────────────────────────────────────── */']));

//  the rail no longer needs the winch tap; it has a instrument of its own
rep('"warmth","wake","transit","slab","retention","winch","volume",',
    '"warmth","wake","transit","slab","retention","volume",');

//  keep the big instruments live
rep('function updateTags() {',
    J([
'function updateTags() {',
'  updateBigs();']));

if (miss.length) {
  console.error('ABORTED, nothing written. Missed:');
  for (const m of miss) console.error('  ' + m);
  process.exit(1);
}
fs.writeFileSync(P, s);
console.log('ui.html: two big instruments added, winch tap retired from the rail');
