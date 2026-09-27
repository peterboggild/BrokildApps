// 260905.1 — the panel: six new knobs, fifteen specimens, HU window presets,
// a linear CT window on the slice, the texel GRID, SPLIT lines, FLATTEN /
// FLATTEN ALL / REVERT / UNDO, and control points you can drag on the gantry.
// Exact-count anchors; nothing written on a miss.
const fs = require("fs");
const path = require("path");
const p = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(p, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const misses = [];
const rep = (from, to, count = 1) => {
  const f = from.split("\n").join(NL), t = to.split("\n").join(NL);
  const n = s.split(f).length - 1;
  if (n !== count) { misses.push(`expected ${count} of [${from.slice(0, 70).replace(/\n/g, "\\n")}...], found ${n}`); return; }
  s = s.split(f).join(t);
};

// ---- kinds, names, the fallback table --------------------------------------
rep(String.raw`const KP = { PCT:0, INT:1, LIST:2, BIPOL:3, HZ:4, VOL:5, SEMI:6, SEC:7, CENT:8 };`,
    String.raw`const KP = { PCT:0, INT:1, LIST:2, BIPOL:3, HZ:4, VOL:5, SEMI:6, SEC:7, CENT:8, RATIO:9 };`);
rep(String.raw`    case KP.SEMI:  return ((v*2-1)*12).toFixed(1) + " st";`,
    String.raw`    case KP.SEMI:  return ((v*2-1)*12).toFixed(1) + " st";
    case KP.RATIO: return "×" + Math.pow(2, (v-0.5)*4).toFixed(2);`);
rep(String.raw`      L_SPEC  = ["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","LUNG","SKULL","CORTEX","SUTURE","ENAMEL","TENDON"];`,
    String.raw`      L_SPEC  = ["SINUS","SPINE","PULSE","MARROW","NERVE","RETINA","THORAX","SKULL","CORTEX","SUTURE","ENAMEL","TENDON","VERTEBRA","FEMUR","JAW"];`);
rep(String.raw`  ["specimen","SPECIMEN",KP.LIST,0.0909,"the volume the lines read",0,11,L_SPEC],`,
    String.raw`  ["specimen","SPECIMEN",KP.LIST,0.0714,"the volume the lines read",0,14,L_SPEC],`);
rep(String.raw`  ["fold","FOLD",KP.PCT,0.00,"what the window does past its edges: clip, or fold back in",0,0],`,
    String.raw`  ["fold","FOLD",KP.PCT,0.00,"what the window does past its edges: clip, or fold back in",0,0],
  ["head2","2ND HEAD",KP.PCT,0.00,"a second reader on the same line, mixed in",0,0],
  ["head2Ratio","HEAD RATIO",KP.RATIO,0.50,"the second head's speed: an interval, or a partial that is not a harmonic",0,0],
  ["head2Phase","HEAD PHASE",KP.PCT,0.00,"where along the cycle the second head starts",0,0],
  ["uniScan","SCAN SPREAD",KP.PCT,0.00,"unison readers spread through the scan, each a little off the others",0,0],
  ["modContrast","MOD>WINDOW",KP.BIPOL,0.50,"how much the MOD line narrows the window",0,0],
  ["modGrain","MOD>GRAIN",KP.BIPOL,0.50,"how much the MOD line sharpens the read",0,0],`);

// ---- the console ------------------------------------------------------------
rep(String.raw`  mkKnob(r1b, "fold", { hint:"what happens to the waveform past the window's edges: 0 clips it flat, 1 folds it back into the window — the folds add harmonics the tissue never had" });

  const m2 = mkMod(1.30, "FILTER");`,
    String.raw`  mkKnob(r1b, "fold", { hint:"what happens to the waveform past the window's edges: 0 clips it flat, 1 folds it back into the window — the folds add harmonics the tissue never had" });
  /*  THE SECOND HEAD: a second reader on the same blended line. At ×1.00 with
      a phase offset it is a fixed-interval double; off the integers it is a
      partial that is not a harmonic — the one thing a single-cycle read
      cannot be on its own. */
  const r1c = mkRow(m1b);
  mkKnob(r1c, "head2", { hint:"a second reader on the same line, mixed in with the first — at ×1.00 and HEAD PHASE 50 % it cancels the odd harmonics (a hollow double); at ×1.50 it is a partial no harmonic series contains" });
  mkKnob(r1c, "head2Ratio", { hint:"the second head's speed against the note: ×0.25 to ×4, exactly ×1.00 in the middle.  Whole numbers give harmonics, anything else a beating partial — real inharmonicity, which the tissue alone can never give" });
  mkKnob(r1c, "head2Phase", { hint:"where along the cycle the second head starts — at ×1.00 this is the interval between the two reads, and it stays put" });

  const m2 = mkMod(1.30, "FILTER");`);
rep(String.raw`  const r6 = mkRow(m3); mkKnob(r6, "mscan"); mkKnob(r6, "mpitch"); mkKnob(r6, "mpan");`,
    String.raw`  const r6 = mkRow(m3); mkKnob(r6, "mscan"); mkKnob(r6, "mpitch"); mkKnob(r6, "mpan");
  const r6b = mkRow(m3);
  mkKnob(r6b, "modContrast", { hint:"how much the MOD line narrows the CONTRAST window — so the tissue under the MOD line decides how hard the WAVE line is read; centred is none" });
  mkKnob(r6b, "modGrain", { hint:"how much the MOD line moves GRAIN — the read sharpening and softening as the MOD line travels; centred is none" });`);
rep(String.raw`  const r10 = mkRow(m5); mkKnob(r10, "detune"); mkKnob(r10, "spread"); mkKnob(r10, "glide");`,
    String.raw`  const r10 = mkRow(m5); mkKnob(r10, "detune"); mkKnob(r10, "spread"); mkKnob(r10, "glide");
  mkKnob(r10, "uniScan", { hint:"unison readers spread through the SCAN as well as in pitch: each reads the body a little further along than the last, so a chord of readers widens without detuning" });`);

// ---- the volume: HU and a default window -----------------------------------
rep(String.raw`const VOL = { n:64, data:new Uint8Array(64*64*64), spec:-1, name:"", dirty:true };`,
    String.raw`const VOL = { n:64, data:new Uint8Array(64*64*64), spec:-1, name:"", dirty:true, hu:null, win:"" };`);
rep(String.raw`const GX = { density:0.50, win:0.42, lev:0.55, band:false };`,
    String.raw`const GX = { density:0.50, win:0.42, lev:0.55, band:false, preset:"" };`);
rep(String.raw`  VOL.spec = p.spec; VOL.name = p.name || "";
  VOL.dirty = true; TOMO.dirty = true;
  autoWindow();`,
    String.raw`  VOL.spec = p.spec; VOL.name = p.name || "";
  VOL.hu = (p.hu && p.hu.length === 2 && +p.hu[1] > +p.hu[0]) ? [+p.hu[0], +p.hu[1]] : null;
  VOL.win = p.win || "";
  VOL.dirty = true; TOMO.dirty = true;
  /*  a body opens on its own radiographer's window; a phantom, or an import,
      on the window its histogram suggests */
  if (!(VOL.hu && VOL.win && applyWindowPreset(VOL.win))){ GX.preset = ""; autoWindow(); }
  drawPresets();`);
rep(String.raw`function bakeGlow(){`,
    String.raw`/*  A radiographer's windows, in Hounsfield units: width and level. They mean
    something on the bodies (built in HU, -1000..2000 across the cube) and on an
    import (whose cube spans its own window); the phantoms have no HU and the
    buttons dim. */
const WIN_PRESETS = { BRAIN:[80,40], SOFT:[400,40], LUNG:[1500,-600], BONE:[2000,500] };
let presetBtns = [];
function huSpan(){ return VOL.hu ? (VOL.hu[1] - VOL.hu[0]) : 0; }
function applyWindowPreset(name){
  const pr = WIN_PRESETS[name]; if (!pr || !VOL.hu) return false;
  const lo = VOL.hu[0], span = huSpan(); if (span <= 0) return false;
  const W = pr[0], L = pr[1];
  GX.lev = clamp((L - lo) / span, 0.02, 0.98);
  GX.win = clamp((W / span - 0.06) / 1.1, 0, 1);
  GX.band = false;
  if (bandBtn){ bandBtn.textContent = "SOLID"; bandBtn.classList.remove("on"); }
  GX.preset = name;
  TOMO.dirty = true; winSld && winSld.redraw(); levSld && levSld.redraw();
  drawPresets();
  return true;
}
function drawPresets(){
  presetBtns.forEach(b => { b.classList.toggle("dim", !VOL.hu); b.classList.toggle("on", !!VOL.hu && GX.preset === b.textContent); });
}

function bakeGlow(){`);
rep(String.raw`    hint:"window LEVEL — which density the window is centred on.  In the gantry it chooses WHICH surface of the specimen you are looking at; sweep it and you travel out through the tissue." });
}`,
    String.raw`    hint:"window LEVEL — which density the window is centred on.  In the gantry it chooses WHICH surface of the specimen you are looking at; sweep it and you travel out through the tissue.  On a body it reads in Hounsfield units." });
  const prow = el("div", "row tight", $("#gantryWrap"));
  presetBtns = Object.keys(WIN_PRESETS).map(n => {
    const b = el("button", "tinybtn", prow); b.textContent = n;
    b.addEventListener("click", () => { if (applyWindowPreset(n)) say("window: " + n.toLowerCase()); else say("the presets need a volume in Hounsfield units - a body, or an import"); });
    return b;
  });
  hint(prow, "WINDOW PRESETS", "a radiographer's windows, in Hounsfield units: BRAIN W80 L40, SOFT W400 L40, LUNG W1500 L−600, BONE W2000 L500.  They light on the bodies and on an import; the phantoms have no HU and the sliders still work everywhere.");
  drawPresets();
}`);
rep(String.raw`    fmt: v => (0.06 + v*1.1).toFixed(2),`,
    String.raw`    fmt: v => VOL.hu ? "W " + Math.round((0.06 + v*1.1) * huSpan()) + " HU" : (0.06 + v*1.1).toFixed(2),`);
rep(String.raw`    fmt: v => v.toFixed(2),`,
    String.raw`    fmt: v => VOL.hu ? "L " + Math.round(VOL.hu[0] + v * huSpan()) + " HU" : v.toFixed(2),`);
rep(String.raw`    get:() => GX.win, set:v => { GX.win = v; TOMO.dirty = true; },`,
    String.raw`    get:() => GX.win, set:v => { GX.win = v; GX.preset = ""; TOMO.dirty = true; drawPresets(); },`);
rep(String.raw`    get:() => GX.lev, set:v => { GX.lev = v; TOMO.dirty = true; },`,
    String.raw`    get:() => GX.lev, set:v => { GX.lev = v; GX.preset = ""; TOMO.dirty = true; drawPresets(); },`);

// ---- the slice: a linear window, and the texel grid -----------------------
rep(String.raw`      let t = clamp((s - lo)/span, 0, 1); t = t*t*(3-2*t);`,
    String.raw`      const t = clamp((s - lo)/span, 0, 1);             /* a CT window is linear */`);
rep(String.raw`const TOMO = { plane:0, table:0.5, dirty:true, drag:null, hover:-1 };`,
    String.raw`const TOMO = { plane:0, table:0.5, dirty:true, drag:null, hover:-1, grid:false };`);
rep(String.raw`  ctx.strokeStyle = "rgba(120,190,230,.42)"; ctx.strokeRect(B.x+.5, B.y+.5, B.s-1, B.s-1);`,
    String.raw`  ctx.strokeStyle = "rgba(120,190,230,.42)"; ctx.strokeRect(B.x+.5, B.y+.5, B.s-1, B.s-1);
  if (TOMO.grid){
    /* the 128 texels the engine reads across this plane */
    ctx.strokeStyle = "rgba(255,220,160,.13)"; ctx.lineWidth = 1; ctx.beginPath();
    for (let i = 1; i < 128; i++){ const t = i/128;
      ctx.moveTo(px(t), B.y); ctx.lineTo(px(t), B.y + B.s); ctx.moveTo(B.x, py(t)); ctx.lineTo(B.x + B.s, py(t)); }
    ctx.stroke();
  }`);
rep(String.raw`    hint(b, b.textContent, "cut the volume on " + ({AXIAL:"XY, moving through z", CORONAL:"XZ, moving through y", SAGITTAL:"YZ, moving through x"})[b.textContent] + ".");
  });
}`,
    String.raw`    hint(b, b.textContent, "cut the volume on " + ({AXIAL:"XY, moving through z", CORONAL:"XZ, moving through y", SAGITTAL:"YZ, moving through x"})[b.textContent] + ".");
  });
  const gridBtn = el("button", "tinybtn", $("#tomo .scrhead")); gridBtn.textContent = "GRID";
  gridBtn.addEventListener("click", () => { TOMO.grid = !TOMO.grid; gridBtn.classList.toggle("on", TOMO.grid); });
  hint(gridBtn, "GRID", "show the 128 texels the engine reads across this plane.  With GRAIN up, what you hear as grain is this grid.");
  cv.addEventListener("pointerdown", () => pushUndo(), true);
}`);

// ---- lines: split, flatten, undo -------------------------------------------
rep(String.raw`for (let i = 0; i < NL; i++) LINES.push({ pts:[{x:0.04,y:0.5,z:0.5},{x:0.96,y:0.5,z:0.5}], closed:false, start:0, warp:0 });`,
    String.raw`for (let i = 0; i < NL; i++) LINES.push({ pts:[{x:0.04,y:0.5,z:0.5},{x:0.96,y:0.5,z:0.5}], closed:false, start:0, warp:0, split:0 });`);
rep(String.raw`function lineAt(L, sIn, out){
  const p = L.pts, n = p.length;
  out = out || {x:0,y:0,z:0};
  if (n <= 1){ out.x = p[0].x; out.y = p[0].y; out.z = p[0].z; return out; }
  let s = sIn - Math.floor(sIn);
  s = warpPhase(s, L.warp);
  if (L.closed){ s += L.start; s -= Math.floor(s); }
  const nseg = L.closed ? n : n-1;
  let f = s * nseg, i = Math.floor(f);
  if (i >= nseg){ i = nseg-1; f = nseg; }
  const t = f - i, t2 = t*t, t3 = t2*t;
  const idx = k => {
    if (L.closed){ k %= n; if (k < 0) k += n; return p[k]; }`,
    String.raw`/*  the same curve the engine evaluates (Engine::Line::at): Catmull-Rom with
    phantom ends, or closed; a SPLIT line is two open segments, one per half
    cycle — the panel probe checks the two agree to five decimals */
function isSplit(L){ return (L.split|0) >= 2 && (L.split|0) <= L.pts.length - 2; }
function lineAt(L, sIn, out){
  out = out || {x:0,y:0,z:0};
  const n = L.pts.length;
  if (n <= 1){ out.x = L.pts[0].x; out.y = L.pts[0].y; out.z = L.pts[0].z; return out; }
  let s = sIn - Math.floor(sIn);
  s = warpPhase(s, L.warp);
  if (isSplit(L)){
    const second = s >= 0.5, ss = second ? (s-0.5)*2 : s*2;
    return crEval(second ? L.pts.slice(L.split) : L.pts.slice(0, L.split), false, ss, out);
  }
  if (L.closed){ s += L.start; s -= Math.floor(s); }
  return crEval(L.pts, L.closed, s, out);
}
function crEval(p, closed, s, out){
  const n = p.length;
  const nseg = closed ? n : n-1;
  let f = s * nseg, i = Math.floor(f);
  if (i >= nseg){ i = nseg-1; f = nseg; }
  const t = f - i, t2 = t*t, t3 = t2*t;
  const idx = k => {
    if (closed){ k %= n; if (k < 0) k += n; return p[k]; }`);
rep(String.raw`    LINES[i].warp = src.warp || 0;
  }
  GLOW.dirty = true; TOMO.dirty = true; drawLineBench();
});`,
    String.raw`    LINES[i].warp = src.warp || 0;
    LINES[i].split = src.split || 0;
  }
  GLOW.dirty = true; TOMO.dirty = true; drawLineBench();
});`);
rep(String.raw`  NB.send({ k:"line", i, pts:flat, closed:L.closed, start:L.start, warp:L.warp });
  GLOW.dirty = true; TOMO.dirty = true;
}`,
    String.raw`  NB.send({ k:"line", i, pts:flat, closed:L.closed, start:L.start, warp:L.warp, split:L.split|0 });
  GLOW.dirty = true; TOMO.dirty = true;
}
/*  starting over: straight lines through the middle, A a little below B so
    SCAN still has somewhere to go */
function flattenLine(i){
  const L = LINES[i], y = (i % 2 === 0) ? 0.45 : 0.60;
  L.pts = [{x:0.04,y,z:0.5},{x:0.96,y,z:0.5}]; L.closed = false; L.start = 0; L.warp = 0; L.split = 0;
  sendLine(i); drawLineBench();
}
/*  one step of undo for the six lines: a snapshot is taken at the start of
    every gesture on the slice, the gantry or the line bench */
const UNDO = { snap:null };
function snapLines(){ return LINES.map(L => ({ pts:L.pts.map(q => ({x:q.x,y:q.y,z:q.z})), closed:L.closed, start:L.start, warp:L.warp, split:L.split|0 })); }
function pushUndo(){ UNDO.snap = snapLines(); }
function undoLines(){
  if (!UNDO.snap){ say("nothing to undo"); return; }
  const cur = snapLines();
  for (let i = 0; i < NL; i++) Object.assign(LINES[i], UNDO.snap[i]);
  UNDO.snap = cur;
  const l = LINES.map(L => { const flat = []; for (const q of L.pts) flat.push(q.x, q.y, q.z); return { pts:flat, closed:L.closed, start:L.start, warp:L.warp, split:L.split|0 }; });
  NB.send({ k:"lines", j:{ ver:1, l } });
  GLOW.dirty = true; TOMO.dirty = true; drawLineBench(); say("lines: undone");
}`);
rep(String.raw`  const proj = (L, blend) => {
    const N = 160, out = [];
    for (let i = 0; i <= N; i++){
      const s = i/N;
      if (blend) blendAt(blend[0], blend[1], scan, s, q); else lineAt(L, s, q);
      out.push([px(uOf(q)), py(vOf(q)), Math.abs(dOf(q) - TOMO.table)]);
    }
    return out;
  };
  const stroke = (pts, css, base, wdt) => {
    for (let i = 1; i < pts.length; i++){
      const a = pts[i-1], b = pts[i];`,
    String.raw`  const proj = (L, blend) => {
    const N = 160, out = [];
    const brk = blend ? (isSplit(LINES[blend[0]]) || isSplit(LINES[blend[1]])) : isSplit(L);
    for (let i = 0; i <= N; i++){
      const s = i/N;
      if (brk && i === N/2) out.push(null);            /* a split line is drawn as two */
      if (blend) blendAt(blend[0], blend[1], scan, s, q); else lineAt(L, s, q);
      out.push([px(uOf(q)), py(vOf(q)), Math.abs(dOf(q) - TOMO.table)]);
    }
    return out;
  };
  const stroke = (pts, css, base, wdt) => {
    for (let i = 1; i < pts.length; i++){
      const a = pts[i-1], b = pts[i];
      if (!a || !b) continue;`);
rep(String.raw`let LNBTN = [], startSld = null, warpSld = null, ptsLbl = null, closeBtn = null;`,
    String.raw`let LNBTN = [], startSld = null, warpSld = null, ptsLbl = null, closeBtn = null, splitLbl = null;`);
rep(String.raw`  hint(bPlus, "POINTS", "add a control point at the end of the line; up to sixteen.");`,
    String.raw`  hint(bPlus, "POINTS", "add a control point at the end of the line; up to sixteen.");
  const bSplitM = el("button", "tinybtn", r2); bSplitM.textContent = "◂";
  splitLbl = el("span", "mono", r2); splitLbl.style.cssText = "font-size:10px;color:#3b3f45;min-width:60px;text-align:center;line-height:21px";
  const bSplitP = el("button", "tinybtn", r2); bSplitP.textContent = "▸";
  const stepSplit = d => {
    const L = LINES[SEL.i], n = L.pts.length; let sp = L.split|0;
    if (n < 4){ say("a split needs four points or more"); return; }
    if (d > 0){ sp = sp === 0 ? 2 : sp + 1; if (sp > n - 2) sp = 0; }
    else { sp = sp === 0 ? n - 2 : sp - 1; if (sp < 2) sp = 0; }
    L.split = sp; sendLine(SEL.i); drawLineBench(); say(sp ? "split after point " + sp : "one curve again");
  };
  bSplitM.addEventListener("click", () => stepSplit(-1)); bSplitP.addEventListener("click", () => stepSplit(1));
  hint(bSplitM, "SPLIT", "make the line two segments instead of one curve: the first half of the cycle reads the points before the split, the second half the points after it.  Two edges a cycle — a family of timbre one curve cannot make.  Needs four points or more.");
  hint(bSplitP, "SPLIT", "move the split one point along, or back to one curve.");`);
rep(String.raw`  hint(bCopy, "A → B", "copy this scanner's A anchor onto its B anchor, so you can nudge B away from a shape you like.");
  drawLineBench();
}`,
    String.raw`  hint(bCopy, "A → B", "copy this scanner's A anchor onto its B anchor, so you can nudge B away from a shape you like.");

  /*  starting over, and taking it back */
  const r5 = el("div", "row tight", m);
  const bFlat = el("button", "tinybtn", r5); bFlat.textContent = "FLATTEN";
  bFlat.addEventListener("click", () => { const base = SEL.i - (SEL.i % 2); flattenLine(base); flattenLine(base + 1); say(LNAME[base].split(" ")[0].toLowerCase() + " scanner flattened"); });
  hint(bFlat, "FLATTEN", "start this scanner over: two straight lines along x through the middle of the body, A a little below B so SCAN still has somewhere to go.");
  const bFlatAll = el("button", "tinybtn", r5); bFlatAll.textContent = "FLATTEN ALL";
  bFlatAll.addEventListener("click", () => { for (let i = 0; i < NL; i++) flattenLine(i); say("all six lines flattened"); });
  hint(bFlatAll, "FLATTEN ALL", "every line back to a straight read through the middle — the simplest state the instrument has, to build up from again.");
  const bRevert = el("button", "tinybtn", r5); bRevert.textContent = "REVERT";
  bRevert.addEventListener("click", () => { NB.send({ k:"revertLines" }); });
  hint(bRevert, "REVERT", "put back the six lines the current patch, study or project was loaded with.");
  const bUndo = el("button", "tinybtn", r5); bUndo.textContent = "UNDO"; bUndo.id = "btnUndoLines";
  bUndo.addEventListener("click", undoLines);
  hint(bUndo, "UNDO", "take back the last change to the lines — one step, from any gesture on the slice, the gantry or this bench.");
  if (!m.dataset.undoHook){ m.dataset.undoHook = "1";
    m.addEventListener("pointerdown", ev => { if (!(ev.target && ev.target.id === "btnUndoLines")) pushUndo(); }, true); }
  drawLineBench();
}`);
rep(String.raw`  ptsLbl.textContent = L.pts.length + " PTS";`,
    String.raw`  ptsLbl.textContent = L.pts.length + " PTS";
  if (splitLbl) splitLbl.textContent = isSplit(L) ? "SPLIT " + L.split : "ONE CURVE";`);

// ---- the gantry: handles you can drag, and along the view ray -------------
rep(String.raw`  gl.drawArrays(gl.POINTS, 0, pts.length/6);
  gl.disable(gl.BLEND);
}`,
    String.raw`  gl.drawArrays(gl.POINTS, 0, pts.length/6);
  gl.disable(gl.BLEND);
  drawGantryHandles();
}
/*  the selected line's control points, projected with the same matrix the
    lines are drawn with, on a 2-D overlay — so they can be grabbed */
const GANTRY_HANDLES = [], GDRAG = { hot:-1 };
function projectPoint(q, w, h, M){
  const cx = M[0]*q.x + M[4]*q.y + M[8]*q.z + M[12], cy = M[1]*q.x + M[5]*q.y + M[9]*q.z + M[13],
        cw = M[3]*q.x + M[7]*q.y + M[11]*q.z + M[15];
  if (cw <= 1e-6) return null;
  return { x:(cx/cw*0.5+0.5)*w, y:(1-(cy/cw*0.5+0.5))*h };
}
function drawGantryHandles(){
  const ov = $("#gantryOv"), cv = GL.cv; if (!ov || !cv) return;
  if (ov.width !== cv.width || ov.height !== cv.height){ ov.width = cv.width; ov.height = cv.height; }
  const ctx = ov.getContext("2d"); ctx.clearRect(0, 0, ov.width, ov.height);
  const M = mvpMatrix(cv.width/cv.height), L = LINES[SEL.i];
  GANTRY_HANDLES.length = 0;
  L.pts.forEach((q, k) => {
    const pr = projectPoint(q, ov.width, ov.height, M); if (!pr) return;
    GANTRY_HANDLES.push({ k, x:pr.x, y:pr.y });
    ctx.beginPath(); ctx.arc(pr.x, pr.y, k === GDRAG.hot ? 7 : 4.5, 0, Math.PI*2);
    ctx.fillStyle = "rgba(255,255,255,.82)"; ctx.fill();
    ctx.strokeStyle = LCSS[SEL.i]; ctx.lineWidth = 1.6; ctx.stroke();
  });
}`);
rep(String.raw`function initGantryInput(){
  const cv = $("#gantryCv");
  let drag = null;
  cv.addEventListener("pointerdown", ev => { ev.preventDefault();
    drag = { x:ev.clientX, y:ev.clientY, yaw:CAM.yaw, pitch:CAM.pitch };
    try { cv.setPointerCapture(ev.pointerId); } catch(e){} });
  cv.addEventListener("pointermove", ev => {
    if (!drag) return;
    CAM.yaw = drag.yaw - (ev.clientX - drag.x) * 0.008;
    CAM.pitch = clamp(drag.pitch + (ev.clientY - drag.y) * 0.006, -1.45, 1.45);
  });
  const end = () => { drag = null; };`,
    String.raw`function initGantryInput(){
  const cv = $("#gantryCv");
  const scr = $("#gantryScreen"); scr.style.position = "relative";
  const ov = el("canvas", null, scr); ov.id = "gantryOv";
  ov.style.cssText = "position:absolute;left:0;top:0;width:100%;height:100%;pointer-events:none";
  let drag = null;
  cv.addEventListener("pointerdown", ev => { ev.preventDefault();
    const r = cv.getBoundingClientRect(), sx = cv.width / Math.max(1, r.width), sy = cv.height / Math.max(1, r.height);
    const mx = (ev.clientX - r.left) * sx, my = (ev.clientY - r.top) * sy;
    let hit = -1, best = 12 * sx;
    for (const hnd of GANTRY_HANDLES){ const d = Math.hypot(hnd.x - mx, hnd.y - my); if (d < best){ best = d; hit = hnd.k; } }
    if (hit >= 0){
      /*  a control point: drag it across the view, or with shift along the
          line of sight — the one direction the slice cannot reach */
      pushUndo();
      const q = LINES[SEL.i].pts[hit], b = camBasis();
      const depth = (q.x-b.eye[0])*b.f[0] + (q.y-b.eye[1])*b.f[1] + (q.z-b.eye[2])*b.f[2];
      drag = { pt:hit, x:ev.clientX, y:ev.clientY, px:q.x, py:q.y, pz:q.z, b, kpx: depth * 0.62 * 2 / Math.max(1, r.height) };
      GDRAG.hot = hit;
    } else drag = { x:ev.clientX, y:ev.clientY, yaw:CAM.yaw, pitch:CAM.pitch };
    try { cv.setPointerCapture(ev.pointerId); } catch(e){} });
  cv.addEventListener("pointermove", ev => {
    if (!drag) return;
    if (drag.pt !== undefined){
      const dx = ev.clientX - drag.x, dy = ev.clientY - drag.y, b = drag.b, k = drag.kpx, q = LINES[SEL.i].pts[drag.pt];
      let X = drag.px, Y = drag.py, Z = drag.pz;
      if (ev.shiftKey){ X += b.f[0]*(-dy)*k; Y += b.f[1]*(-dy)*k; Z += b.f[2]*(-dy)*k; }
      else { X += (b.r[0]*dx - b.u[0]*dy)*k; Y += (b.r[1]*dx - b.u[1]*dy)*k; Z += (b.r[2]*dx - b.u[2]*dy)*k; }
      q.x = clamp(X, 0, 1); q.y = clamp(Y, 0, 1); q.z = clamp(Z, 0, 1);
      sendLine(SEL.i);
      return;
    }
    CAM.yaw = drag.yaw - (ev.clientX - drag.x) * 0.008;
    CAM.pitch = clamp(drag.pitch + (ev.clientY - drag.y) * 0.006, -1.45, 1.45);
  });
  const end = () => { drag = null; GDRAG.hot = -1; };`);
rep(String.raw`    "the specimen seen through the scanner.  Drag to turn it, wheel to come closer.  The bright line is what is being read; the dim pair are its anchors.  Raise DENSITY and the tissue closes over them.");`,
    String.raw`    "the specimen seen through the scanner.  Drag to turn it, wheel to come closer.  The bright line is what is being read; the dim pair are its anchors.  The white dots are the selected line's control points: drag one to move it across the view, hold shift to push it along the line of sight — the one direction the slice cannot reach.  Raise DENSITY and the tissue closes over them.");`);

// ---- the debug surface -------------------------------------------------------
rep(String.raw`  IMP, drawImport,`,
    String.raw`  IMP, drawImport,
  applyWindowPreset, WIN_PRESETS, drawPresets, flattenLine, pushUndo, undoLines, isSplit, GANTRY_HANDLES, drawGantryHandles,`);

if (misses.length) { console.error("NOT WRITTEN:\n  " + misses.join("\n  ")); process.exit(1); }
fs.writeFileSync(p, s, "utf8");
console.log("ui.html patched for 260905.1");
