// Renders one PNG per formula with the real GLSL from js/formulas.js (needs playwright). Convert to JPG into docs/equations/img/.
const { chromium } = require('playwright');
const F = require('../js/formulas.js');
const fs = require('fs');
const W = 800, H = 500;
const HEAD = `#version 300 es
precision highp float; precision highp int;
#define MAX_ITER 32
uniform float uP[8]; uniform int uIter; uniform mat3 uRotA, uRotB; uniform float uSize, uStep;
uniform vec3 uCam, uFwd, uRight, uUp; uniform float uFov; uniform vec2 uRes; uniform float uUnb;
out vec4 outc;
`;
const MAIN = `
float mapT(vec3 p, out vec4 trap, out float it){ float d = fde(p, trap, it); return d*uStep; }
float mapD(vec3 p){ vec4 t; float i; return mapT(p,t,i); }
vec3 nrm(vec3 p, float e){ const vec2 k=vec2(1,-1); return normalize(k.xyy*mapD(p+k.xyy*e)+k.yyx*mapD(p+k.yyx*e)+k.yxy*mapD(p+k.yxy*e)+k.xxx*mapD(p+k.xxx*e)); }
vec3 pal(float t){ return 0.5+0.5*cos(6.2831*(vec3(1.0,1.0,1.0)*t+vec3(0.0,0.12,0.30))); }
void main(){
  vec2 uv=(gl_FragCoord.xy-0.5*uRes)/uRes.y;
  vec3 rd=normalize(uFwd+uv.x*uRight*uFov+uv.y*uUp*uFov);
  vec3 ro=uCam; float t=0.0; bool hit=false; vec4 trap; float it; float steps=0.0;
  float maxd = uSize*14.0;
  for(int i=0;i<500;i++){
    float d=mapT(ro+rd*t,trap,it);
    float eps = 0.0007*t*uSize*0.6 + 1e-5;
    if(abs(d)<eps){hit=true;break;}
    t+=d; steps+=1.0;
    if(t>maxd) break;
  }
  vec3 bg = mix(vec3(0.02,0.03,0.06), vec3(0.10,0.12,0.20), gl_FragCoord.y/uRes.y);
  vec3 col=bg;
  if(hit){
    vec3 p=ro+rd*t; vec3 n=nrm(p,0.0012*t*uSize+1e-5);
    vec4 tr; float it2; mapT(p,tr,it2);
    float u = clamp(0.35*length(tr.xyz)+0.5*it2,0.0,3.0);
    vec3 base = pal(0.15+0.6*u);
    vec3 sun=normalize(vec3(0.6,0.8,0.4));
    float dif=clamp(dot(n,sun),0.0,1.0);
    // cheap soft shadow
    float sh=1.0; float tt=0.02*uSize;
    for(int i=0;i<48;i++){ float h=mapD(p+n*0.002*uSize+sun*tt); sh=min(sh,8.0*h/tt); tt+=clamp(h,0.003*uSize,0.3*uSize); if(sh<0.01||tt>uSize*4.0)break; }
    sh=clamp(sh,0.0,1.0);
    float ao=1.0-clamp(steps/220.0,0.0,1.0)*0.9;
    float fill=0.5+0.5*n.y;
    col=base*(0.18*fill*ao + 0.95*dif*sh*(0.4+0.6*ao)) + 0.12*pow(clamp(dot(reflect(rd,n),sun),0.0,1.0),24.0)*sh;
    col=mix(col,bg,1.0-exp(-t/(uSize*9.0)));
  } else { col = bg + 0.25*pal(0.1+steps/200.0)*clamp(steps/120.0,0.0,1.0); }
  col = pow(col/(1.0+col*0.35), vec3(0.4545));
  outc=vec4(col,1.0);
}`;
(async () => {
  const only = process.argv[2];
  const browser = await chromium.launch({ args: ['--use-gl=angle','--use-angle=swiftshader','--enable-unsafe-swiftshader','--ignore-gpu-blocklist'] });
  const page = await browser.newPage({ viewport: { width: W, height: H } });
  await page.setContent(`<canvas id=c width=${W} height=${H}></canvas>`);
  for (const f of F.FORMULAS) {
    if (only && f.id !== only) continue;
    const pk = F.pack(f.id, F.defaults(f.id));
    const cam = f.camera || {};
    const dist = cam.dist || 3, pitch = (cam.pitch||0)*Math.PI/180, yaw = (cam.yaw||0)*Math.PI/180;
    const tgt = [cam.tx||0, cam.ty||0, cam.tz||0];
    const pos = [tgt[0]+dist*Math.cos(pitch)*Math.sin(yaw), tgt[1]+dist*Math.sin(pitch), tgt[2]+dist*Math.cos(pitch)*Math.cos(yaw)];
    const fwd = tgt.map((t,i)=>t-pos[i]); const l=Math.hypot(...fwd); fwd.forEach((_,i)=>fwd[i]/=l);
    const cr=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
    let right = cr(fwd,[0,1,0]); const rl=Math.hypot(...right); right=right.map(x=>x/rl);
    const up = cr(right,fwd);
    const fov = Math.tan(((cam.fov||50)*Math.PI/180)/2)*2.0;
    const src = HEAD + f.glsl + MAIN;
    const ok = await page.evaluate(({src,pk,pos,fwd,right,up,fov,iter,size,step,W,H}) => {
      const c=document.getElementById('c'); const gl=c.getContext('webgl2',{preserveDrawingBuffer:true});
      if(!gl) return 'nogl';
      const mk=(t,s)=>{const sh=gl.createShader(t);gl.shaderSource(sh,s);gl.compileShader(sh);if(!gl.getShaderParameter(sh,gl.COMPILE_STATUS))throw new Error(gl.getShaderInfoLog(sh));return sh;};
      const vs=mk(gl.VERTEX_SHADER,`#version 300 es\nin vec2 a; void main(){gl_Position=vec4(a,0,1);}`);
      const fs=mk(gl.FRAGMENT_SHADER,src);
      const pr=gl.createProgram();gl.attachShader(pr,vs);gl.attachShader(pr,fs);gl.linkProgram(pr);
      if(!gl.getProgramParameter(pr,gl.LINK_STATUS))throw new Error(gl.getProgramInfoLog(pr));
      gl.useProgram(pr);
      const b=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,b);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,3,-1,-1,3]),gl.STATIC_DRAW);
      const loc=gl.getAttribLocation(pr,'a');gl.enableVertexAttribArray(loc);gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);
      const U=n=>gl.getUniformLocation(pr,n);
      gl.uniform1fv(U('uP'),pk.p);gl.uniform1i(U('uIter'),iter);
      gl.uniformMatrix3fv(U('uRotA'),false,pk.rotA);gl.uniformMatrix3fv(U('uRotB'),false,pk.rotB);
      gl.uniform1f(U('uSize'),size);gl.uniform1f(U('uStep'),step);
      gl.uniform3fv(U('uCam'),pos);gl.uniform3fv(U('uFwd'),fwd);gl.uniform3fv(U('uRight'),right);gl.uniform3fv(U('uUp'),up);
      gl.uniform1f(U('uFov'),fov);gl.uniform2f(U('uRes'),W,H);
      gl.viewport(0,0,W,H);gl.drawArrays(gl.TRIANGLES,0,3);
      return 'ok';
    }, { src, pk, pos, fwd, right, up, fov, iter: f.iter.def, size: f.size, step: f.step, W, H }).catch(e=>'ERR '+e.message.slice(0,300));
    if (ok !== 'ok') { console.log(f.id, ok); continue; }
    await page.locator('#c').screenshot({ path: `docs/equations/img/${f.id}.png` });
    console.log(f.id, 'done');
  }
  await browser.close();
})();
