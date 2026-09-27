"use strict";
/*  1. The ordinary (OFF) shader's bump heights in one loop with a uniform bound
       (measured 7 s -> 2 s compile under ANGLE/D3D11; the heights are the same).
    2. REC and EXPORT move to the header; EXPORT opens a popover with the options.
    Every anchor is checked before anything is written. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const miss = [], ed = [];
const r = (a, b) => { const n = s.split(a).length - 1; if (n !== 1) miss.push("x" + n + " " + a.slice(0, 80)); else ed.push([a, b]); };

/* ---- 1: the OFF shader ---- */
r('"uniform vec2 uRes;",', '"uniform vec2 uRes;",\n"uniform int uHN;",                       /* always 3: a uniform bound, so the compiler keeps ONE copy of hgt() */');
r('"  float h0=hgt(vUV,m,k), hx=hgt(vUV+vec2(e,0.0),m,k), hy=hgt(vUV+vec2(0.0,e),m,k);",',
  '"  float hs[3]; hs[0]=0.5; hs[1]=0.5; hs[2]=0.5;",\n' +
  '"  for(int j=0;j<uHN;j++){ vec2 o= j==1 ? vec2(e,0.0) : (j==2 ? vec2(0.0,e) : vec2(0.0)); hs[j]=hgt(vUV+o,m,k); }",\n' +
  '"  float h0=hs[0], hx=hs[1], hy=hs[2];",');
/* the cinematic library now inherits the declaration from FS's head */
r('"const float PI=3.14159265;",\n"uniform int uHN;",', '"const float PI=3.14159265;",');
r('  const names = ["uMVP","uCam","uLightN","uLightPos","uLightCol","uLightRoom","uLightShade",',
  '  const names = ["uHN","uMVP","uCam","uLightN","uLightPos","uLightCol","uLightRoom","uLightShade",');
r('  gl.uniform2f(U.uRes, w, h);\n', '  gl.uniform2f(U.uRes, w, h);\n  gl.uniform1i(U.uHN, 3);\n');
/* a hook that renders the OFF view with another fragment shader, so the probe can
   prove the legacy three-call form and the loop give the same pixels */
r('  get verts(){ return M.n; },\n',
  '  get verts(){ return M.n; },\n' +
  '  fsSource(){ return FS; },\n' +
  '  swapFS(src){\n' +
  '    if (!gl) return "no gl";\n' +
  '    const fs2 = compile(gl, gl.FRAGMENT_SHADER, src || FS), vs2 = compile(gl, gl.VERTEX_SHADER, VS);\n' +
  '    if (!fs2 || !vs2) return "compile failed";\n' +
  '    const p = gl.createProgram();\n' +
  '    gl.attachShader(p, vs2); gl.attachShader(p, fs2);\n' +
  '    ["aPos","aNrm","aTan","aUV","aInfo"].forEach((n, i) => gl.bindAttribLocation(p, i, n));\n' +
  '    gl.linkProgram(p);\n' +
  '    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) return "link failed";\n' +
  '    prog = p; gl.useProgram(prog);\n' +
  '    for (const n in U) U[n] = gl.getUniformLocation(prog, n);\n' +
  '    return "ok";\n' +
  '  },\n');

/* ---- 2: header controls and the popover ---- */
r('    <div id="topright">\n      <span>BUILD',
  '    <div id="recBox">\n' +
  '      <button class="b" id="bRec" type="button">Rec</button>\n' +
  '      <span id="recTime">0:00.0</span>\n' +
  '      <button class="b" id="bExpOpen" type="button">Export</button>\n' +
  '    </div>\n' +
  '    <div id="topright">\n      <span>BUILD');
r('#cineBox .selwrap{ width:98px; }',
  '#cineBox .selwrap{ width:92px; }\n' +
  '#recBox{ display:flex; gap:6px; align-items:center; align-self:center; }\n' +
  '#recTime{ font-family:Consolas,"SF Mono",monospace; font-size:10.5px; color:#c9b18b; white-space:nowrap; min-width:86px; }\n' +
  '#bRec.on{ color:#ffe0cc; border-color:#b0482a; background:linear-gradient(180deg,#8a2c16,#5a1a0c); }\n' +
  '#expPop{ position:fixed; z-index:80; width:300px; padding:8px 10px 8px 10px;\n' +
  '  border:1px solid #4a3a26; border-radius:3px; background:linear-gradient(180deg,#221d19,#171311);\n' +
  '  box-shadow:0 10px 26px rgba(0,0,0,.8); }\n' +
  '#expPop > h2{ margin:0 0 6px 0; font-size:9.5px; font-weight:700; letter-spacing:.24em; color:#9c7a45; text-transform:uppercase;\n' +
  '  border-bottom:1px solid #241f1c; padding-bottom:3px; }\n' +
  '@media (max-width:1100px){ #by{ display:none; } }');
r('function buildRecGroup(g){\n  const hd = mk("h2", "sub", g); hd.textContent = "Record and export";\n  const br = mk("div", "btnrow", g);\n  const bRec = mk("button", "b", br); bRec.type = "button"; bRec.id = "bRec"; bRec.textContent = "Record";\n  hintOn(bRec, "rec");',
  '/*  REC and EXPORT live in the HEADER, where they are always on screen; the\n' +
  '    export options open as a popover under EXPORT. */\n' +
  'let expPop = null;\n' +
  'function expPopPlace(){\n' +
  '  const b = document.getElementById("bExpOpen");\n' +
  '  if (!b || !expPop) return;\n' +
  '  const r = b.getBoundingClientRect();\n' +
  '  const w = expPop.offsetWidth || 300;\n' +
  '  expPop.style.top = Math.round(r.bottom + 6) + "px";\n' +
  '  expPop.style.left = Math.round(Math.max(6, Math.min(r.right - w, window.innerWidth - w - 6))) + "px";\n' +
  '}\n' +
  'function expPopShow(on){\n' +
  '  if (!expPop) return;\n' +
  '  expPop.hidden = !on;\n' +
  '  document.getElementById("bExpOpen").classList.toggle("on", !!on);\n' +
  '  if (on){ expPopPlace(); updateRecUI(); }\n' +
  '}\n' +
  'window.addEventListener("resize", () => { if (expPop && !expPop.hidden) expPopPlace(); });\n' +
  'document.addEventListener("pointerdown", e => {\n' +
  '  if (!expPop || expPop.hidden) return;\n' +
  '  if (expPop.contains(e.target) || e.target.closest && e.target.closest("#bExpOpen")) return;\n' +
  '  expPopShow(false);\n' +
  '});\n' +
  'window.addEventListener("keydown", e => { if (e.key === "Escape" && expPop && !expPop.hidden) expPopShow(false); });\n' +
  'function buildRecGroup(){\n' +
  '  expPop = mk("div", null, document.body);\n' +
  '  expPop.id = "expPop"; expPop.hidden = true;\n' +
  '  const g = expPop;\n' +
  '  const hd = mk("h2", null, g); hd.textContent = "Export the last take";\n' +
  '  const bOpen = document.getElementById("bExpOpen");\n' +
  '  hintOn(bOpen, "expopen");\n' +
  '  bOpen.addEventListener("click", () => expPopShow(expPop.hidden));\n' +
  '  const bRec = document.getElementById("bRec");\n' +
  '  hintOn(bRec, "rec");');
r('  const tm = mk("span", "ro", br); tm.id = "recTime"; tm.style.alignSelf = "center";\n  hintOn(tm, "rectime");',
  '  const tm = document.getElementById("recTime");\n  hintOn(tm, "rectime");');
r('  buildRecGroup(gv);\n', '');
r('  bRec.textContent = recording ? "Stop" : "Record";', '  bRec.textContent = recording ? "Stop" : "Rec";');
r('  expinset:["PLAN INSET",', '  expopen: ["EXPORT", "Open the export panel: video size, frame rate, picture quality, the plan inset, and the button that renders the last take to an MP4 with its sound. Record a take with REC first."],\n  expinset:["PLAN INSET",');

if (miss.length){ console.error("missing, nothing written:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(f, s); console.log("ok " + ed.length);
