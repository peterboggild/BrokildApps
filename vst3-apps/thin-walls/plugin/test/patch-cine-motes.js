"use strict";
/* cinematic: the dust's shadow test moves from the vertex to the fragment
   shader. A shadow-sampler lookup in a vertex shader measured 33 ms a frame
   under ANGLE/D3D11 for 1500 points; in the fragment shader it costs nothing. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const NL = String.fromCharCode(92) + "n";
const miss = [], ed = [];
const r = (a, b) => { const n = s.split(a).length - 1; if (n !== 1) miss.push("x" + n + " " + a.slice(0, 70)); else ed.push([a, b]); };
r('"out float vB;' + NL + '" +\n"float h1(float n)', '"out float vB; out vec3 vW;' + NL + '" +\n"float h1(float n)');
r('"  float g=goboAt(uSunPos,p); float s= g>0.001 ? sunShadow(p,1) : 0.0;' + NL + '" +\n"  vB=g*s*(0.35+0.65*h1(i*5.1));' + NL + '" +',
  '"  float g=goboAt(uSunPos,p); vW=p;' + NL + '" +\n"  vB=g*(0.35+0.65*h1(i*5.1));' + NL + '" +');
r('const MOTE_FS = "#version 300 es' + NL + 'precision highp float;' + NL + 'in float vB;' + NL + 'uniform float uK;' + NL + '" +\n"layout(location=0) out vec4 o0;' + NL + '" +\n"void main(){ vec2 q=gl_PointCoord*2.0-1.0; float r=dot(q,q); if(r>1.0) discard;' + NL + '" +\n"  o0=vec4(vec3(1.0,0.93,0.80)*vB*uK*(1.0-r)*(1.0-r), 1.0); }' + NL + '";',
  'const MOTE_FS = "#version 300 es' + NL + 'precision highp float;' + NL + 'in float vB; in vec3 vW;' + NL + 'uniform float uK;' + NL + '" + CINE_FUNCS +\n"layout(location=0) out vec4 o0;' + NL + '" +\n"void main(){ vec2 q=gl_PointCoord*2.0-1.0; float r=dot(q,q); if(r>1.0) discard;' + NL + '" +\n"  float s=sunShadow(vW,1); if(s<=0.0) discard;' + NL + '" +\n"  o0=vec4(vec3(1.0,0.93,0.80)*vB*s*uK*(1.0-r)*(1.0-r), 1.0); }' + NL + '";');
if (miss.length){ console.error("missing:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(f, s); console.log("ok " + ed.length);
