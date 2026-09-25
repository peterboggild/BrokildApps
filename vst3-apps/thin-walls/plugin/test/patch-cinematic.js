/*  Thin Walls - cinematic mode and video export, page side (protocol 7 + 8).
    node test/patch-cinematic.js <cine-block.js>
    Every anchor is checked to occur exactly once before anything is written. */
"use strict";
const fs = require("fs");
const path = require("path");
const UI = path.join(__dirname, "..", "Source", "ui", "ui.html");
const BLOCK = fs.readFileSync(process.argv[2], "utf8");
let s = fs.readFileSync(UI, "utf8");
const miss = [];
const edits = [];
function rep(a, b){
  const n = s.split(a).length - 1;
  if (n !== 1){ miss.push("x" + n + ": " + a.slice(0, 70).replace(/\n/g, "\\n")); return; }
  edits.push([a, b]);
}

/* 1 css */
rep("</style>", [
"/* ------------------------------------------------------------ cinematic */",
"#cineBox{ display:flex; gap:5px; align-items:center; align-self:center; }",
"#cineBox .selwrap{ width:98px; }",
"#cineHud{",
"  position:absolute; right:10px; bottom:9px; pointer-events:none;",
"  font-family:Consolas,\"SF Mono\",monospace; font-size:10px; color:#b39a78; letter-spacing:.08em;",
"  text-shadow:0 1px 3px #000; background:rgba(12,9,8,.45); padding:2px 6px; border-radius:2px;",
"}",
"/* ---------------------------------------------------- record and export */",
".row.xrow{ grid-template-columns:60px minmax(0,1fr) 66px; }",
".xbar{ height:4px; margin:3px 0 3px 0; background:#0d0b0a; border:1px solid #2b2522; border-radius:2px; overflow:hidden; }",
".xbar i{ display:block; height:100%; width:0; background:linear-gradient(90deg,#8a6636,#e2b170); }",
"#recTime{ margin-left:4px; }",
"#expNote{ margin-top:1px; }",
"#expModal{ position:fixed; inset:0; z-index:90; background:rgba(8,6,5,.72); display:flex; align-items:center; justify-content:center; }",
"#expModal .xbox{ width:min(420px,86vw); padding:14px 16px 12px 16px; border:1px solid #4a3a26; border-radius:3px;",
"  background:linear-gradient(180deg,#221d19,#171311); box-shadow:0 10px 30px rgba(0,0,0,.8); }",
"#expModal h3{ margin:0 0 8px 0; font-size:10px; letter-spacing:.24em; text-transform:uppercase; color:#d8a45c; }",
"#expModal .xstage{ font-family:Consolas,monospace; font-size:11px; color:#e8cd9f; margin-bottom:6px; }",
"#expModal .xbar{ height:6px; margin-bottom:10px; }",
"</style>"].join("\n"));

/* 2 header */
rep('    <div id="topright">\n      <span>BUILD',
    '    <div id="cineBox">\n' +
    '      <button class="b" id="bCine" type="button">Cinematic</button>\n' +
    '      <div class="selwrap"><select class="sel" id="cineQ"><option value="high">HIGH 1.5x</option><option value="ultra">ULTRA 2x</option></select></div>\n' +
    '    </div>\n' +
    '    <div id="topright">\n      <span>BUILD');

/* 3 hud in the 3D view */
rep('      <div id="glName"></div>\n', '      <div id="glName"></div>\n      <div id="cineHud" hidden></div>\n');

/* 4 the frame loop */
rep("  rafId = 0;\n  const now = (typeof t === \"number\") ? t : nowMs();\n  stepWalk(now);",
    "  rafId = 0;\n  if (EXP.active) return;            /* an export draws its own frames; nothing else may */\n  const now = (typeof t === \"number\") ? t : nowMs();\n  stepWalk(now);");
rep("  if (walkActive() || doorAnimActive()){ dirtyPov = true; dirtyPlan = true; schedule(); }\n}",
    "  if (walkActive() || doorAnimActive()){ dirtyPov = true; dirtyPlan = true; schedule(); }\n" +
    "  /*  Cinematic keeps the 3D view alive - dust, flicker, the photo converging -\n" +
    "      until the photo is done. Off, cineLoop() is false and nothing changes. */\n" +
    "  else if (cineLoop()){ dirtyPov = true; schedule(); }\n}");

/* 5 native values during an export go to the saved live state */
rep("function setNative(id, v){             /* it arrived from native: do NOT send */\n  if (!(id in V)) return;",
    "function setNative(id, v){             /* it arrived from native: do NOT send */\n  if (!(id in V)) return;\n" +
    "  if (EXP.active){ EXP.liveV[id] = clamp01(v); return; }   /* the live state is parked while frames are drawn */");
rep("function applyFurnNative(items){\n",
    "function applyFurnNative(items){\n  if (EXP.active){ EXP.liveFurn = sanitizeFurn(items); return; }\n");

/* 6 the group */
rep("  buildWavGroup();\n  buildFurnGroup();\n}", "  buildWavGroup();\n  buildFurnGroup();\n  buildRecGroup();\n}");

/* 7 the mesh generation the shadow maps follow */
rep("  buildSources(true);\n  meshDirty = false;\n}", "  buildSources(true);\n  meshDirty = false;\n  meshGen++;\n}");

/* 8 drawPov */
rep("  drawCount++;\n  sizeCanvas(povCv);\n  if (glMode === \"pending\") initGL();",
    "  drawCount++;\n  if (!EXP.active) sizeCanvas(povCv);    /* an export has set the frame size itself */\n  if (glMode === \"pending\") initGL();");
rep("  if (meshDirty || vertsUploaded === 0){ buildMesh(); uploadMesh(); }\n  const w = povCv.width, h = povCv.height;\n  gl.viewport(0,0,w,h);",
    "  if (meshDirty || vertsUploaded === 0){ buildMesh(); uploadMesh(); }\n  if (cineWanted() && cineDraw()) return;\n  const w = povCv.width, h = povCv.height;\n  gl.viewport(0,0,w,h);");

/* 9 no walking while frames are drawn */
rep("  const k = movementKey(e);\n  if (!k || textEntry(e)) return;\n",
    "  const k = movementKey(e);\n  if (!k || textEntry(e)) return;\n  if (EXP.active){ e.preventDefault(); e.stopPropagation(); return; }\n");

/* 10 the block */
rep("/* ==================================================================== */\n/*  native traffic",
    BLOCK + "\n/* ==================================================================== */\n/*  native traffic");

/* 11 debug hooks */
rep("  get verts(){ return M.n; },\n",
    "  get verts(){ return M.n; },\n" +
    "  /*  Cinematic and export: the state that is otherwise locked in this\n" +
    "      closure, and a bench that times a mode frame by frame. */\n" +
    "  cine(){ return { on:CX.on, q:CX.q, ok:CX.ok, why:CX.why, mode:CX.mode || null, pn:CX.pn, converged:CX.converged,\n" +
    "                   ms:CX.ftMs, samples:CX.samples, target:CX.rw + \"x\" + CX.rh, flags:CX.flags.slice(),\n" +
    "                   stored:(function(){ try { return localStorage.getItem(CINE_KEY); } catch(e){ return null; } })() }; },\n" +
    "  setCine(on, q){ cineSet(on, q); },\n" +
    "  cineBench(mode, n){ return cineBench(mode, n); },\n" +
    "  rec(){ return Object.assign({}, recSt); },\n" +
    "  get exporting(){ return EXP.active || EXP.waitingPlan; },\n" +
    "  expOpt(o){ if (o) Object.assign(expOpt, o); return Object.assign({}, expOpt); },\n" +
    "  get pitchFov(){ return [pitchDeg, fovDeg]; },\n");

if (miss.length){ console.error("anchors missing, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
for (const [a, b] of edits) s = s.replace(a, () => b);
fs.writeFileSync(UI, s, "utf8");
console.log("patched " + edits.length + " places; " + s.length + " bytes");
