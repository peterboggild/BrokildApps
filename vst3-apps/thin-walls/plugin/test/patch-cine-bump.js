"use strict";
/* cinematic: the bump's three height samples in ONE loop with a uniform bound.
   Inlined three times, hgt() cost the D3D compiler ~6 s per program; in a loop
   it is compiled once. The heights, and so the picture, are the same. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const miss = [], ed = [];
const r = (a, b) => { const n = s.split(a).length - 1; if (n !== 1) miss.push("x" + n + " " + a.slice(0, 70)); else ed.push([a, b]); };
r('const MAIN_HEAD = [\n"  int m=int(vInfo.x+0.5), k=int(vInfo.y+0.5), rm=int(vInfo.z+0.5);",\n"  float alpha = vInfo.w;",\n"  vec3 gn=normalize(vNrm), T=normalize(vTan); vec3 B=cross(gn,T);",\n"  float e=0.012;",\n"  float h0=hgt(vUV,m,k), hx=hgt(vUV+vec2(e,0.0),m,k), hy=hgt(vUV+vec2(0.0,e),m,k);",',
  'const MAIN_HEAD = [\n"  int m=int(vInfo.x+0.5), k=int(vInfo.y+0.5), rm=int(vInfo.z+0.5);",\n"  float alpha = vInfo.w;",\n"  vec3 gn=normalize(vNrm), T=normalize(vTan); vec3 B=cross(gn,T);",\n"  float e=0.012;",\n' +
  '"  float hs[3]; hs[0]=0.5; hs[1]=0.5; hs[2]=0.5;",\n' +
  '"  for(int j=0;j<uHN;j++){ vec2 o= j==1 ? vec2(e,0.0) : (j==2 ? vec2(0.0,e) : vec2(0.0)); hs[j]=hgt(vUV+o,m,k); }",\n' +
  '"  float h0=hs[0], hx=hs[1], hy=hs[2];",');
r('"precision highp sampler2D; precision highp samplerCube;",\n"const float PI=3.14159265;",',
  '"precision highp sampler2D; precision highp samplerCube;",\n"const float PI=3.14159265;",\n"uniform int uHN;",');
r('  g.uniform1i(pr.u("uGhost"), 0);\n', '  g.uniform1i(pr.u("uGhost"), 0);\n  g.uniform1i(pr.u("uHN"), 3);\n');
r('  g.uniform1f(pr.u("uGlow"), CX.glowFreeze);\n', '  g.uniform1f(pr.u("uGlow"), CX.glowFreeze);\n  g.uniform1i(pr.u("uHN"), 3);\n');
if (miss.length){ console.error("missing:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(f, s); console.log("ok " + ed.length);
