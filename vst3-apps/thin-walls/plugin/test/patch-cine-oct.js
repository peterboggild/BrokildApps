"use strict";
/* cinematic: four cube samplers behind a branch -> one octahedral atlas, and
   loops bounded by uniforms, so the D3D compiler cannot unroll them. */
const fs = require("fs"), path = require("path");
const f = path.join(__dirname, "..", "Source", "ui", "ui.html");
let s = fs.readFileSync(f, "utf8");
const miss = [], ed = [];
const r = (a, b) => { const n = s.split(a).length - 1; if (n !== 1) miss.push("x" + n + " " + a.slice(0, 70)); else ed.push([a, b]); };
r('"uniform samplerCube uCube0; uniform samplerCube uCube1; uniform samplerCube uCube2; uniform samplerCube uCube3;",',
  '"uniform sampler2D uOct;",');
r('"float cubeFetch(int c, vec3 d){",\n"  if(c==0) return textureLod(uCube0,d,0.0).r; if(c==1) return textureLod(uCube1,d,0.0).r;",\n"  if(c==2) return textureLod(uCube2,d,0.0).r; return textureLod(uCube3,d,0.0).r; }",',
  '"vec2 octEnc(vec3 n){ n/=(abs(n.x)+abs(n.y)+abs(n.z)); vec2 e=n.xy;",\n' +
  '"  if(n.z<0.0) e=(1.0-abs(n.yx))*vec2(n.x>=0.0?1.0:-1.0, n.y>=0.0?1.0:-1.0); return e*0.5+0.5; }",\n' +
  '"float cubeFetch(int c, vec3 d){ vec2 e=clamp(octEnc(normalize(d)), 0.5/1024.0, 1.0-0.5/1024.0);",\n' +
  '"  vec2 tile=vec2(float(c-2*(c/2)), float(c/2)); return textureLod(uOct,(tile+e)*0.5,0.0).r; }",');
r('"  for(int i=0;i<12;i++){",\n"    if(i>=uLightN) break;",\n"    int ty=int(uLType[i]+0.5);",',
  '"  for(int i=0;i<uLightN;i++){",\n"    int ty=int(uLType[i]+0.5);",');
const BS = String.fromCharCode(92);          /* a literal backslash, never typed through a shell */
const LNL = BS + "n";                         /* the two characters backslash-n, as the source holds them */
r('"  for(int i=0;i<160;i++){ if(i>=uBoxN) break; if(boxHit(i,ro,rd,t,n) && t<tmax) return true; } return false; }' + LNL + '" +',
  '"  for(int i=0;i<uBoxN;i++){ if(boxHit(i,ro,rd,t,n) && t<tmax) return true; } return false; }' + LNL + '" +');
r('"  for(int i=0;i<160;i++){ if(i>=uBoxN) break; if(boxHit(i,ro,rd,t,n) && t<tb){ tb=t; nb=n; ib=i; } } return ib>=0; }' + LNL + '" +',
  '"  for(int i=0;i<uBoxN;i++){ if(boxHit(i,ro,rd,t,n) && t<tb){ tb=t; nb=n; ib=i; } } return ib>=0; }' + LNL + '" +');
/* the conversion pass: a cube face set -> one octahedral tile */
r('const SHADOW_SUN_FS = ',
  'const OCT_FS = "#version 300 es' + LNL + 'precision highp float;' + LNL + 'precision highp samplerCube;' + LNL +
  'uniform samplerCube uCube; uniform vec2 uOrg;' + LNL + 'out vec4 o;' + LNL + '" +\n' +
  '"vec3 octDec(vec2 e){ e=e*2.0-1.0; vec3 n=vec3(e.x,e.y,1.0-abs(e.x)-abs(e.y)); float t=max(-n.z,0.0);' + LNL + '" +\n' +
  '"  n.x+= n.x>=0.0 ? -t : t; n.y+= n.y>=0.0 ? -t : t; return normalize(n); }' + LNL + '" +\n' +
  '"void main(){ vec2 e=(gl_FragCoord.xy-uOrg)/1024.0; o=vec4(textureLod(uCube, octDec(e), 0.0).r, 0.0, 0.0, 1.0); }' + LNL + '";\n' +
  'const SHADOW_SUN_FS = ');
r('    CX.P.shSun  = cineProg("sun shadow", SHADOW_VS, SHADOW_SUN_FS, true);',
  '    CX.P.shSun  = cineProg("sun shadow", SHADOW_VS, SHADOW_SUN_FS, true);\n    CX.P.oct    = cineProg("octahedral", FSQ_VS, OCT_FS, false);');
r('    const SS = 2048;\n',
  '    CX.tx.oct = g.createTexture();\n' +
  '    g.bindTexture(g.TEXTURE_2D, CX.tx.oct);\n' +
  '    g.texImage2D(g.TEXTURE_2D, 0, g.R16F, 2048, 2048, 0, g.RED, g.HALF_FLOAT, null);\n' +
  '    g.texParameteri(g.TEXTURE_2D, g.TEXTURE_MIN_FILTER, g.NEAREST); g.texParameteri(g.TEXTURE_2D, g.TEXTURE_MAG_FILTER, g.NEAREST);\n' +
  '    g.texParameteri(g.TEXTURE_2D, g.TEXTURE_WRAP_S, g.CLAMP_TO_EDGE); g.texParameteri(g.TEXTURE_2D, g.TEXTURE_WRAP_T, g.CLAMP_TO_EDGE);\n' +
  '    CX.fb.oct = cineFbo([CX.tx.oct]);\n' +
  '    const SS = 2048;\n');
r("for (const k in CX.tx) if (CX.tx[k] && !/^(cube|sun|box)/.test(k)) g.deleteTexture(CX.tx[k]);",
  "for (const k in CX.tx) if (CX.tx[k] && !/^(cube|sun|box|oct)/.test(k)) g.deleteTexture(CX.tx[k]);");
r("for (const k in CX.fb) if (CX.fb[k]) g.deleteFramebuffer(CX.fb[k]);",
  "for (const k in CX.fb) if (CX.fb[k] && !/^(cube|sun|oct)/.test(k)) g.deleteFramebuffer(CX.fb[k]);");
r("for (const k in CX.fb) if (/^cube|^sun/.test(k)) keepFb[k] = CX.fb[k];",
  "for (const k in CX.fb) if (/^(cube|sun|oct)/.test(k)) keepFb[k] = CX.fb[k];");
r("for (const k in CX.tx) if (/^(cube|sun|box)/.test(k)) keepTx[k] = CX.tx[k];",
  "for (const k in CX.tx) if (/^(cube|sun|box|oct)/.test(k)) keepTx[k] = CX.tx[k];");
r("  /* the window */\n  g.useProgram(CX.P.shSun.p);",
  "  /* each cube folded into its octahedral tile: one sampler serves all four lamps */\n" +
  "  g.disable(g.DEPTH_TEST);\n" +
  "  g.useProgram(CX.P.oct.p);\n" +
  "  g.bindVertexArray(CX.emptyVao);\n" +
  "  for (let i = 0; i < 4; i++){\n" +
  "    const ox = (i & 1) * 1024, oy = (i >> 1) * 1024;\n" +
  "    g.activeTexture(g.TEXTURE0); g.bindTexture(g.TEXTURE_CUBE_MAP, CX.tx[\"cube\" + i]);\n" +
  "    g.uniform1i(CX.P.oct.u(\"uCube\"), 0);\n" +
  "    g.uniform2f(CX.P.oct.u(\"uOrg\"), ox, oy);\n" +
  "    g.bindFramebuffer(g.FRAMEBUFFER, CX.fb.oct);\n" +
  "    g.viewport(ox, oy, 1024, 1024);\n" +
  "    g.drawArrays(g.TRIANGLES, 0, 3);\n" +
  "  }\n" +
  "  g.bindTexture(g.TEXTURE_CUBE_MAP, null);\n" +
  "  g.enable(g.DEPTH_TEST);\n" +
  "  g.bindVertexArray(vao);\n" +
  "  /* the window */\n  g.useProgram(CX.P.shSun.p);");
r('  for (let i = 0; i < 4; i++){\n    g.activeTexture(g.TEXTURE0 + unit0 + i);\n    g.bindTexture(g.TEXTURE_CUBE_MAP, CX.tx["cube" + i]);\n    g.uniform1i(pr.u("uCube" + i), unit0 + i);\n  }\n',
  '  g.activeTexture(g.TEXTURE0 + unit0);\n  g.bindTexture(g.TEXTURE_2D, CX.tx.oct);\n  g.uniform1i(pr.u("uOct"), unit0);\n');
if (miss.length){ console.error("missing:\n" + miss.join("\n")); process.exit(1); }
for (const [a, b] of ed) s = s.replace(a, () => b);
fs.writeFileSync(f, s); console.log("ok " + ed.length);
