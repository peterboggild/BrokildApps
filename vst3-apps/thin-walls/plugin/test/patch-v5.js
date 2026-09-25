"use strict";
/*  Thin Walls v5, page side: light sync (protocol 9) and wall pictures (10).
    node test/patch-v5.js <v5-block.js>. Every anchor is checked for its exact
    count before anything is written. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const BLOCK = fs.readFileSync(process.argv[2], "utf8");
const miss = [], ed = [];
const r = (a, b, n) => { const c = s.split(a).length - 1; if (c !== (n || 1)) miss.push("x" + c + " " + a.slice(0, 80)); else ed.push([a, b]); };

/* the shader: per-room lamp factor, window factor, picture texture */
r('"uniform int uHN;",', '"uniform int uHN;",\n"uniform float uLampK[3];",\n"uniform float uWinK;",\n"uniform sampler2D uPicTex;",');
r('"  Mtl mt=surf(vUV,m,k,h0,vPos);",',
  '"  Mtl mt=surf(vUV,m,k,h0,vPos);",\n' +
  '"  vec2 dux=dFdx(vUV), duy=dFdy(vUV);",\n' +
  '"  if(k==7||k==33) mt.emis*=uLampK[rm];",\n' +
  '"  if(k==8) mt.emis*=uWinK;",\n' +
  '"  if(k==60){ vec3 tc=textureGrad(uPicTex, vUV, dux, duy).rgb; mt.alb=pow(tc,vec3(2.2))*0.92; mt.sp= m==1 ? 0.02 : 0.07; mt.shin= m==1 ? 8.0 : 40.0; }",', 2);
r('  const names = ["uHN","uMVP",', '  const names = ["uLampK","uWinK","uPicTex","uHN","uMVP",');
r('  gl.uniform1i(U.uHN, 3);\n', '  gl.uniform1i(U.uHN, 3);\n  syncUniforms({ k:U.uLampK, w:U.uWinK });\n  gl.uniform1i(U.uPicTex, PIC_UNIT);\n');
r('gl.drawArrays(gl.TRIANGLES, 0, Math.min(MESH_OPAQUE, vertsUploaded));', 'drawOpaque(U.uPicTex);');
/* the ordinary view's lamps */
r('  ];\n  /*  Only a source that is actually fed casts the radiation cue. A ghosted',
  '  ];\n  /*  Light sync: an exact multiply by 1 when it is off. */\n' +
  '  for (const l of lamps){ if (l.sh) l.i *= lampK(l.room); else l.i *= winK(); }\n' +
  '  /*  Only a source that is actually fed casts the radiation cue. A ghosted');
/* mesh, plan, loop */
r('  buildFurnitureMesh();                 /* opaque', '  buildFurnitureMesh();\n  buildPictureMesh();                   /* opaque');
r('  drawFurniturePlan(c);\n', '  drawFurniturePlan(c);\n  drawPicturesPlan(c);\n');
r('  const now = (typeof t === "number") ? t : nowMs();\n  stepWalk(now);\n',
  '  const now = (typeof t === "number") ? t : nowMs();\n  stepWalk(now);\n  if (SYNC.s > 0) syncStep(now);\n');
r('  else if (cineLoop()){ dirtyPov = true; schedule(); }\n}',
  '  else if (cineLoop() || syncActive()){ dirtyPov = true; schedule(); }\n}');
/* controls */
r('  hintPos.textContent = "set in the views";\n', '  hintPos.textContent = "set in the views";\n  buildSyncControls(gv);\n');
r('  buildFurnGroup();\n  buildRecGroup();\n', '  buildFurnGroup();\n  buildPicsSection(document.getElementById("grpFurn"));\n  buildRecGroup();\n');
r('.row.xrow{', '.pgrid{ display:grid; grid-template-columns:minmax(0,1fr) minmax(0,1fr); gap:0 12px; }\n.row.xrow{');
/* native traffic */
r('NB.on("scene", d => {\n  if (!d) return;\n  scene = d;\n', 'NB.on("scene", d => {\n  if (!d) return;\n  scene = d;\n  syncOnScene(d);\n');
r('  if (Array.isArray(d.furn)) applyFurnNative(d.furn);\n',
  '  if (Array.isArray(d.furn)) applyFurnNative(d.furn);\n  if (Array.isArray(d.pics)) applyPicsNative(d.pics, d.picImages);\n');
r('NB.on("wav", d => { if (d) applyWav(d); });', 'NB.on("wav", d => { if (d) applyWav(d); });\nNB.on("pics", d => { if (d && Array.isArray(d.items)) applyPicsNative(d.items, d.images); });');
/* cinematic */
r('g.uniform1i(pr.u("uHN"), 3);', 'g.uniform1i(pr.u("uHN"), 3);\n  syncUniforms({ k:pr.u("uLampK"), w:pr.u("uWinK") });\n  g.uniform1i(pr.u("uPicTex"), PIC_UNIT);', 2);
r('  const opq = Math.min(MESH_OPAQUE, vertsUploaded);\n  g.drawArrays(g.TRIANGLES, 0, opq);\n  if (vertsUploaded > MESH_OPAQUE){',
  '  drawOpaque(pr.u("uPicTex"));\n  if (vertsUploaded > MESH_OPAQUE){');
r('g.drawArrays(g.TRIANGLES, 0, Math.min(MESH_OPAQUE, vertsUploaded));', 'drawOpaque(pr.u("uPicTex"));');
r('c:mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i * f), room:-1, ty:1', 'c:mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i * f * lampK(lampRoom(i))), room:-1, ty:1');
r('c:mul3([0.34,0.46,0.78], 4.2*0.45), room:2', 'c:mul3([0.34,0.46,0.78], 4.2*0.45*winK()), room:2');
r('L.push({ p:SUN_POS, c:SUN_COL, room:-1', 'L.push({ p:SUN_POS, c:mul3(SUN_COL, winK()), room:-1');
r('lc.set(mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i * (life ? cineFlicker(i, t) : 1)), i*3);',
  'lc.set(mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i * (life ? cineFlicker(i, t) : 1) * lampK(lampRoom(i))), i*3);');
r('g.uniform3f(pr.u("uSunCol"), SUN_COL[0], SUN_COL[1], SUN_COL[2]);',
  'g.uniform3f(pr.u("uSunCol"), SUN_COL[0]*winK(), SUN_COL[1]*winK(), SUN_COL[2]*winK());', 2);
r('lc.set(mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i), i*3); }', 'lc.set(mul3(LAMP_LIGHT[i].c, LAMP_LIGHT[i].i * lampK(lampRoom(i))), i*3); }');
r('      if (key !== CX.pkey){ CX.pkey = key; CX.still = now; CX.photo = false; CX.pn = 0; CX.converged = false; }\n',
  '      if (key !== CX.pkey){ CX.pkey = key; CX.still = now; CX.photo = false; CX.pn = 0; CX.converged = false; }\n' +
  '      /*  Moving light cannot be averaged: while light sync is modulating the\n' +
  '          lamps the view stays real-time, and PHOTO waits for it to settle. */\n' +
  '      if (syncActive()){ CX.still = now; if (CX.photo){ CX.photo = false; CX.pn = 0; CX.converged = false; } }\n');
r('    else add([SXn(n), SYn(n), zc], 0, 0.06, 0.06, 0.06, [0.68,0.67,0.65]);\n  }\n  return B;',
  '    else add([SXn(n), SYn(n), zc], 0, 0.06, 0.06, 0.06, [0.68,0.67,0.65]);\n  }\n' +
  '  for (const it of pics){\n' +
  '    const pf = picPlace(it), rec = picImg[it.id];\n' +
  '    add([pf.c[0] + pf.n[0]*pf.d/2, pf.c[1] + pf.n[1]*pf.d/2, pf.zc], Math.atan2(pf.t[1], pf.t[0]), pf.w/2, pf.d/2, pf.h/2,\n' +
  '        rec && rec.avg ? rec.avg : [0.4, 0.4, 0.4]);\n' +
  '  }\n  return B;');
/* export: the take's own light and clock, never the live ones */
r('  EXP.t = i / plan.fps;\n}',
  '  EXP.t = i / plan.fps;\n' +
  '  const tg = expSyncTarget(plan, i);\n' +
  '  if (i === 0 || !SYNC.exp) SYNC.exp = tg.slice(); else syncSmooth(SYNC.exp, tg, 1 / plan.fps);\n' +
  '}\n' +
  'function expSyncTarget(plan, i){\n' +
  '  const b = plan.beat && plan.beat[i];\n' +
  '  if (SYNC.mode === "beat" && b && b[2] && b[1] > 0){ const p = beatPulseAt(+b[0] || 0); return [p, p, p]; }\n' +
  '  const l = plan.light && plan.light[i];\n' +
  '  return [0, 1, 2].map(k => l && isFinite(+l[k]) ? clamp01(+l[k]) : 0);\n' +
  '}');
r('  EXP.active = false;\n', '  EXP.active = false;\n  SYNC.exp = null;\n');
/* the block, and the hooks */
r("/* ==================================================================== */\n/*  native traffic", BLOCK + "\n/* ==================================================================== */\n/*  native traffic");
r('  get verts(){ return M.n; },\n',
  '  get verts(){ return M.n; },\n' +
  '  sync(){ return { s:SYNC.s, mode:SYNC.mode, win:SYNC.win, L:SYNC.L.slice(), tgt:SYNC.tgt.slice(), k:[lampK(0), lampK(1), lampK(2)],\n' +
  '                   active:syncActive(), stored:(function(){ try { return localStorage.getItem(SYNC_KEY); } catch(e){ return null; } })() }; },\n' +
  '  setSync(o){ if (o){ if (typeof o.s === "number") SYNC.s = clamp01(o.s); if (o.mode) SYNC.mode = o.mode === "beat" ? "beat" : "follow"; if (o.win !== undefined) SYNC.win = !!o.win; } syncSave(); updateSyncUI(); invalidate("pov"); },\n' +
  '  syncSettle(){ const t = syncTargets(nowMs()); for (let r = 0; r < 3; r++) SYNC.L[r] = t[r]; },\n' +
  '  pics(){ return pics.map(p => Object.assign({}, p)); },\n' +
  '  setPics(items, images){ applyPicsNative(items || [], images || null); },\n' +
  '  get picSel(){ return picSel; },\n' +
  '  get picArmed(){ return picArm; },\n' +
  '  selectPic(i){ picSel = (i >= 0 && i < pics.length) ? i : -1; updatePicUI(); invalidate("both"); },\n' +
  '  picPlace(i){ const it = pics[i]; if (!it) return null; const p = picPlace(it); return { c:p.c, t:p.t, n:p.n, zc:p.zc, w:p.w, h:p.h, d:p.d }; },\n' +
  '  picReady(id){ const r = picImg[id]; return !!(r && r.ok); },\n' +
  '  planPics(){ return planPicsDrawn.slice(); },\n');

if (miss.length){ console.error("missing, nothing written:\n  " + miss.join("\n  ")); process.exit(1); }
for (const [a, b] of ed) s = s.split(a).join(b);
fs.writeFileSync(f, s);
console.log("patched " + ed.length + " places");
