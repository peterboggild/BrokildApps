/* WebGL2 renderer with a hard bound on GPU work per draw call.

   Why this shape (stability):
   - "Live" frames (playing, dragging) render once at an adaptive fraction of the canvas size,
     capped at LIVE_PIXEL_CAP pixels per draw.
   - When the frame stops changing, a "refine" pass renders full resolution in 128 px tiles,
     a few tiles per animation frame, and accumulates jittered samples for antialiasing.
     No single draw call can run long enough to trigger the Windows GPU watchdog (TDR), which
     is what takes down the browser (and VS Code webviews sharing the GPU process).
   - Once refined, nothing is drawn at all: an idle app costs no GPU.
   - Shader programs are compiled per formula, asynchronously when KHR_parallel_shader_compile exists.
   - Context loss is handled: first loss resumes in Draft quality, a second one stops until the user restarts. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const Formulas = window.FVFormulas;

  const QUALITY = {
    draft: { label: 'Draft', steps: 110, shadow: 0, samples: 1, liveMax: 0.5, refineMax: 0.75 },
    balanced: { label: 'Balanced', steps: 180, shadow: 32, samples: 6, liveMax: 0.75, refineMax: 1 },
    high: { label: 'High', steps: 300, shadow: 64, samples: 24, liveMax: 1, refineMax: 1 }
  };
  const LIVE_PIXEL_CAP = 900000;
  const REFINE_PIXEL_CAP = 4200000;
  const TILE = 128;
  const REFINE_DELAY_MS = 180;
  const MAX_DPR = 2;

  const VERT = `#version 300 es
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

  const FRAG_HEAD = `#version 300 es
precision highp float;
precision highp int;
out vec4 outColor;
uniform vec2 uRes, uCenter, uJitter;
uniform float uFrameH;
uniform vec3 uCamPos, uCamRight, uCamUp, uCamFwd;
uniform float uFovT;
uniform int uVR, uVRLayout;
uniform float uVRSpan, uIpd;
uniform float uP[8];
uniform int uIter;
uniform mat3 uRotA, uRotB;
uniform float uRepeat, uTwist, uBound, uStep, uDetail, uSize, uMaxDist, uAO;
uniform int uMaxSteps, uShadowSteps;
uniform vec3 uPalC[8];
uniform float uPalP[8];
uniform int uPalN, uPalInterp, uColorSrc, uPalWrap, uFogMatch, uEncode;
uniform float uPalScale, uPalOffset, uFog, uAmbient, uSpec;
uniform vec3 uBgTop, uBgBot, uFogCol, uGlowCol, uSunDir, uSunCol, uFillCol;
uniform sampler2D uBg0, uBg1;
uniform vec2 uBgS0, uBgO0, uBgS1, uBgO1, uBgAmt, uBgGain, uBgSat, uBgLod, uBgFront, uBgMode;
uniform float uBgFogFade;
#define MAX_ITER 32
#define MAX_STEPS 400
`;

  const FRAG_MAIN = `
float mapT(vec3 p, out vec4 trap, out float it) {
  vec3 q = p;
  if (uTwist != 0.0) {
    float a = q.y * uTwist;
    float c = cos(a), s = sin(a);
    q.xz = mat2(c, s, -s, c) * q.xz;
  }
  if (uRepeat > 0.0) q -= uRepeat * floor(q / uRepeat + 0.5);
  float d = fde(q, trap, it);
  if (uBound > 0.0) d = max(d, length(p) - uBound);
  return d * uStep;
}

float mapD(vec3 p) { vec4 tr; float it; return mapT(p, tr, it); }

vec3 calcNormal(vec3 p, float e) {
  const vec2 k = vec2(1.0, -1.0);
  return normalize(k.xyy * mapD(p + k.xyy * e) + k.yyx * mapD(p + k.yyx * e) +
                   k.yxy * mapD(p + k.yxy * e) + k.xxx * mapD(p + k.xxx * e));
}

float calcAO(vec3 p, vec3 n) {
  if (uAO <= 0.0) return 1.0;
  float occ = 0.0, norm = 0.0, w = 1.0;
  for (int i = 1; i <= 5; i++) {
    float h = 0.01 * uSize * float(i * i);
    occ += (h - mapD(p + n * h)) * w;
    norm += h * w;
    w *= 0.65;
  }
  return mix(1.0, clamp(1.0 - occ / norm, 0.0, 1.0), uAO);
}

float softShadow(vec3 ro, vec3 rd) {
  float res = 1.0;
  float t = 0.01 * uSize;
  for (int i = 0; i < 128; i++) {
    if (i >= uShadowSteps) break;
    float h = mapD(ro + rd * t);
    res = min(res, 10.0 * h / t);
    t += clamp(h, 0.004 * uSize, 0.25 * uSize);
    if (res < 0.002 || t > uMaxDist) break;
  }
  return clamp(res, 0.0, 1.0);
}

vec3 oklabToLinear(vec3 c) {
  float l_ = c.x + 0.3963377774 * c.y + 0.2158037573 * c.z;
  float m_ = c.x - 0.1055613458 * c.y - 0.0638541728 * c.z;
  float s_ = c.x - 0.0894841775 * c.y - 1.2914855480 * c.z;
  float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
  return vec3(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
             -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
             -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s);
}

vec3 palette(float u) {
  vec3 c = uPalC[0];
  for (int i = 1; i < 8; i++) {
    if (i >= uPalN) break;
    float p0 = uPalP[i - 1];
    float p1 = uPalP[i];
    if (u >= p1) { c = uPalC[i]; continue; }
    if (u > p0) {
      float a = clamp((u - p0) / max(p1 - p0, 1e-5), 0.0, 1.0);
      if (uPalInterp == 2) a = 0.0;
      else if (uPalInterp == 3) a = a * a * (3.0 - 2.0 * a);
      c = mix(uPalC[i - 1], uPalC[i], a);
    }
    break;
  }
  if (uPalInterp == 1 || uPalInterp == 3) c = oklabToLinear(c);
  return max(c, vec3(0.0));
}

vec3 background(vec3 rd) {
  return mix(uBgBot, uBgTop, smoothstep(-0.65, 0.85, rd.y));
}

// Backdrop/overlay picture or movie frame, pinned to the output frame.
// Returns linear colour + coverage (inside the picture x its own transparency).
vec4 backdropLayer(sampler2D tex, vec2 fuv, vec2 s, vec2 o, float gain, float sat, float lod) {
  vec2 tuv = fuv * s + o;
  vec4 t = texture(tex, tuv, lod);
  vec3 c = pow(t.rgb, vec3(2.2)) * gain;
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = max(mix(vec3(l), c, sat), 0.0);
  vec2 ins = step(vec2(0.0), tuv) * step(tuv, vec2(1.0));
  return vec4(c, ins.x * ins.y * t.a);
}

// Overlay in front of the fractal: 0 normal, 1 screen, 2 add, 3 multiply, 4 soft light.
vec3 overlayBlend(vec3 col, vec3 o, float a, float mode) {
  if (a <= 0.0) return col;
  vec3 b = clamp(col, 0.0, 1.0);
  vec3 r;
  if (mode < 0.5) r = o;
  else if (mode < 1.5) r = col + o * (1.0 - b);
  else if (mode < 2.5) r = col + o;
  else if (mode < 3.5) r = col * o;
  else {
    vec3 oc = clamp(o, 0.0, 1.0);
    vec3 soft = mix(2.0 * b * oc + b * b * (1.0 - 2.0 * oc), sqrt(b) * (2.0 * oc - 1.0) + 2.0 * b * (1.0 - oc), step(0.5, oc));
    r = soft + max(col - 1.0, 0.0);
  }
  return mix(col, r, a);
}

void main() {
  vec2 frag = gl_FragCoord.xy + uJitter;
  vec2 uv = (frag - uCenter) / (0.5 * uFrameH);
  vec3 rd = normalize(uCamFwd + uFovT * (uv.x * uCamRight + uv.y * uCamUp));
  vec3 ro = uCamPos;
  float pix = uFovT / (0.5 * uFrameH);
  float bgMask = 1.0;
  if (uVR == 1) {
    // Equirectangular VR frame. uVRLayout: 0 mono, 1 top/bottom (left eye on top), 2 side by side (left eye left).
    vec2 eye = uRes;
    float side = 0.0;
    vec2 q = frag;
    if (uVRLayout == 1) { eye.y *= 0.5; side = q.y >= eye.y ? -1.0 : 1.0; if (q.y >= eye.y) q.y -= eye.y; }
    else if (uVRLayout == 2) { eye.x *= 0.5; side = q.x < eye.x ? -1.0 : 1.0; if (q.x >= eye.x) q.x -= eye.x; }
    float lon = (q.x / eye.x - 0.5) * uVRSpan;
    float lat = (q.y / eye.y - 0.5) * 3.14159265;
    vec3 hz = sin(lon) * uCamRight + cos(lon) * uCamFwd;
    rd = normalize(cos(lat) * hz + sin(lat) * uCamUp);
    // omni-directional stereo: each eye sits on a circle, offset at right angles to the horizontal view
    // direction; the offset fades out towards the poles, where the two eyes would otherwise swap
    ro = uCamPos + side * 0.5 * uIpd * cos(lat) * (cos(lon) * uCamRight - sin(lon) * uCamFwd);
    pix = 3.14159265 / eye.y;
    // backdrops are flat pictures: place them in front, where the normal camera shows them
    float fz = dot(rd, uCamFwd);
    bgMask = smoothstep(0.05, 0.2, fz);
    uv = vec2(dot(rd, uCamRight), dot(rd, uCamUp)) / max(fz, 0.05) / uFovT;
  }
  // sampled up front (uniform control flow) so mip selection works
  vec4 bl0 = uBgAmt.x > 0.0 ? backdropLayer(uBg0, uv * 0.5, uBgS0, uBgO0, uBgGain.x, uBgSat.x, uBgLod.x) : vec4(0.0);
  vec4 bl1 = uBgAmt.y > 0.0 ? backdropLayer(uBg1, uv * 0.5, uBgS1, uBgO1, uBgGain.y, uBgSat.y, uBgLod.y) : vec4(0.0);
  bl0.a *= bgMask;
  bl1.a *= bgMask;
  float minEps = 1e-5 * uSize;
  float t = 0.0;
  float d = 1e9;
  float glow = 0.0;
  bool hit = false;
  for (int i = 0; i < MAX_STEPS; i++) {
    if (i >= uMaxSteps) break;
    d = mapD(ro + rd * t);
    float eps = max(pix * t * uDetail, minEps);
    glow += exp(-max(d, 0.0) / (0.03 * uSize));
    if (d < eps) { hit = true; break; }
    t += d;
    if (t > uMaxDist) break;
  }
  if (!hit && t < uMaxDist && d < pix * t * uDetail * 12.0) hit = true;

  vec3 sky = background(rd);
  float a0 = uBgAmt.x * bl0.a * (1.0 - uBgFront.x);
  float a1 = uBgAmt.y * bl1.a * (1.0 - uBgFront.y);
  float cover = a0 + a1;
  vec3 bg = sky * (1.0 - cover) + bl0.rgb * a0 + bl1.rgb * a1;
  // fog fades the fractal into the backdrop (uBgFogFade = 1) or into the fog/sky colour (0)
  vec3 fogC = mix(uFogMatch == 1 ? sky : uFogCol, bg, uBgFogFade);
  vec3 col;
  if (hit) {
    vec3 p = ro + rd * t;
    vec4 trap;
    float it;
    mapT(p, trap, it);
    float e = max(pix * t * uDetail, minEps) * 0.5;
    vec3 n = calcNormal(p, e);
    float ao = calcAO(p, n);
    float v;
    if (uColorSrc == 0) v = sqrt(trap.w);
    else if (uColorSrc == 1) v = min(min(trap.x, trap.y), trap.z) * 2.0;
    else if (uColorSrc == 2) v = it;
    else if (uColorSrc == 3) v = 1.0 - ao;
    else if (uColorSrc == 4) v = n.y * 0.5 + 0.5;
    else if (uColorSrc == 5) v = length(p) / uSize * 0.5;
    else v = t / uMaxDist;
    float u = v * uPalScale + uPalOffset;
    if (uPalWrap == 0) u = fract(u);
    else if (uPalWrap == 1) u = 1.0 - abs(1.0 - 2.0 * fract(u * 0.5));
    else u = clamp(u, 0.0, 1.0);
    vec3 alb = palette(u);

    float dif = max(dot(n, uSunDir), 0.0);
    float sh = 1.0;
    if (uShadowSteps > 0 && dif > 0.001) sh = softShadow(p + n * e * 4.0, uSunDir);
    vec3 fillDir = normalize(vec3(-uSunDir.x, 0.35, -uSunDir.z));
    float fil = max(dot(n, fillDir), 0.0);
    vec3 hv = normalize(uSunDir - rd);
    float spe = pow(max(dot(n, hv), 0.0), 48.0) * dif * sh;
    float fre = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    vec3 sky = mix(uBgBot, uBgTop, 0.5 + 0.5 * n.y);
    vec3 amb = uAmbient * (0.55 + 0.45 * n.y) * (vec3(0.6) + 0.8 * sky);
    col = alb * (amb * ao + uSunCol * dif * sh + uFillCol * fil * ao);
    col += uSunCol * spe * uSpec * 0.5;
    col += fre * ao * 0.25 * sky * uAmbient;
    col = mix(col, fogC, 1.0 - exp(-uFog * t));
  } else {
    col = bg;
    if (uFogMatch == 0) col = mix(col, uFogCol, (1.0 - exp(-uFog * uMaxDist)) * (1.0 - uBgFogFade * cover));
  }
  col += uGlowCol * glow * 0.012;
  col = max(col, vec3(0.0));
  col = overlayBlend(col, bl0.rgb, uBgAmt.x * bl0.a * uBgFront.x, uBgMode.x);
  col = overlayBlend(col, bl1.rgb, uBgAmt.y * bl1.a * uBgFront.y, uBgMode.y);
  if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
  col = min(col, vec3(256.0));
  if (uEncode == 1) col = col / (1.0 + col);
  outColor = vec4(col, 1.0);
}
`;

  const DISPLAY_FRAG = `#version 300 es
precision highp float;
out vec4 outColor;
uniform sampler2D uLive, uAccum;
uniform int uUseAccum, uTonemap, uEncode;
uniform vec2 uOut, uCenter, uOrigin;
uniform float uFrameH, uExposure, uContrast, uSat, uHue, uVignette, uSeed, uGrain, uGrainSeed;
vec3 decode(vec3 c) { return uEncode == 1 ? c / max(1.0 - c, vec3(1e-3)) : c; }
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
vec3 hueRotate(vec3 c, float a) {
  const vec3 k = vec3(0.57735);
  float ca = cos(a);
  return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - ca);
}
vec3 toSrgb(vec3 c) { return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c)); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233)) + uSeed) * 43758.5453); }
void main() {
  vec2 uv = (gl_FragCoord.xy - uOrigin) / uOut;
  vec3 c = decode(texture(uLive, uv).rgb);
  if (uUseAccum == 1) {
    vec4 a = texture(uAccum, uv);
    if (a.a > 0.5) c = decode(a.rgb / a.a);
  }
  c *= exp2(uExposure);
  c = 0.18 * pow(max(c, vec3(1e-6)) / 0.18, vec3(uContrast));
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = max(mix(vec3(l), c, uSat), 0.0);
  if (uHue != 0.0) c = max(hueRotate(c, uHue), 0.0);
  if (uTonemap == 0) c = aces(c);
  else if (uTonemap == 1) c = c / (1.0 + c);
  else c = clamp(c, 0.0, 1.0);
  vec2 v = (gl_FragCoord.xy - uCenter) / (0.5 * uFrameH);
  float vig = smoothstep(2.2, 0.4, length(v * vec2(0.8, 1.0)));
  c *= mix(1.0, vig, uVignette);
  c = toSrgb(clamp(c, 0.0, 1.0));
  if (uGrain > 0.0) {
    // film grain: frame-seeded, roughly Gaussian, strongest in the midtones, sized to the frame
    vec2 gp = floor((gl_FragCoord.xy - uCenter) / max(uFrameH / 720.0, 1.0));
    float n = 0.0;
    for (int i = 0; i < 3; i++) n += fract(sin(dot(gp + float(i) * 17.17 + uGrainSeed * 3.71, vec2(12.9898, 78.233))) * 43758.5453);
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c += (n / 3.0 - 0.5) * uGrain * 0.32 * (0.35 + lum * (1.0 - lum) * 2.6);
  }
  c += (hash(gl_FragCoord.xy) - 0.5) / 255.0;
  outColor = vec4(c, 1.0);
}`;

  // Auto-flight probe: the fractal's own distance function (mapT/mapD, same space ops as the render),
  // evaluated for ONE camera position. Pixel 0 = distance, 1..6 = distance at +-eps on x/y/z (gradient),
  // 7.. = how far each look-ahead ray travels before it hits the surface (capped at uMaxRay).
  const PROBE_N = 32;
  const MAP_SRC = FRAG_MAIN.slice(0, FRAG_MAIN.indexOf('vec3 calcNormal'));
  const PROBE_MAIN = `
uniform vec3 uPPos;
uniform vec3 uDirs[25];
uniform float uMaxRay, uEps;
void main() {
  int x = int(gl_FragCoord.x);
  float v;
  if (x == 0) v = mapD(uPPos);
  else if (x < 7) {
    int a = (x - 1) / 2;
    vec3 e = vec3(0.0);
    e[a] = ((x - 1) % 2 == 0) ? uEps : -uEps;
    v = mapD(uPPos + e);
  } else if (x < 32) {
    vec3 d = uDirs[x - 7];
    float t = 0.0;
    for (int i = 0; i < 160; i++) {
      float h = mapD(uPPos + d * t);
      if (h < uEps * 0.5) break;
      t += h;
      if (t >= uMaxRay) { t = uMaxRay; break; }
    }
    v = min(t, uMaxRay);
  } else v = 0.0;
  outColor = vec4(v, 0.0, 0.0, 1.0);
}`;

  const FRACTAL_UNIFORMS = ['uRes', 'uCenter', 'uFrameH', 'uJitter', 'uCamPos', 'uCamRight', 'uCamUp', 'uCamFwd', 'uFovT', 'uVR', 'uVRLayout', 'uVRSpan', 'uIpd', 'uP', 'uIter',
    'uRotA', 'uRotB', 'uRepeat', 'uTwist', 'uBound', 'uStep', 'uDetail', 'uSize', 'uMaxSteps', 'uShadowSteps', 'uMaxDist', 'uAO',
    'uPalC', 'uPalP', 'uPalN', 'uPalInterp', 'uColorSrc', 'uPalWrap', 'uPalScale', 'uPalOffset', 'uBgTop', 'uBgBot', 'uFogCol',
    'uFogMatch', 'uFog', 'uGlowCol', 'uSunDir', 'uSunCol', 'uFillCol', 'uAmbient', 'uSpec', 'uEncode',
    'uBg0', 'uBg1', 'uBgS0', 'uBgO0', 'uBgS1', 'uBgO1', 'uBgAmt', 'uBgGain', 'uBgSat', 'uBgLod', 'uBgFront', 'uBgMode', 'uBgFogFade'];
  const DISPLAY_UNIFORMS = ['uLive', 'uAccum', 'uUseAccum', 'uTonemap', 'uEncode', 'uOut', 'uCenter', 'uFrameH', 'uExposure',
    'uContrast', 'uSat', 'uHue', 'uOrigin', 'uVignette', 'uSeed', 'uGrain', 'uGrainSeed'];

  // R2 low-discrepancy sequence for sub-pixel jitter.
  function jitter(n) {
    if (n === 0) return [0, 0];
    const g = 1.32471795724474602596;
    return [((0.5 + n / g) % 1) - 0.5, ((0.5 + n / (g * g)) % 1) - 0.5];
  }

  class Renderer {
    constructor(canvas) {
      this.canvas = canvas;
      this.listeners = {};
      this.quality = 'balanced';
      this.enabled = true;
      this.frame = null;
      this.gfxKey = '';
      this.aspect = 16 / 9;
      this.lost = false;
      this.lossCount = 0;
      this.liveScale = 0.45;
      this.tilesPerTick = 4;
      this.lastTick = 0;
      this.prevWork = null;
      this.dtAvg = 16;
      this.liveDirty = false;
      this.displayDirty = false;
      this.refineAt = 0;
      this.samples = 0;
      this.tileCursor = 0;
      this.tiles = [];
      this.accumFresh = true;
      this.job = null;
      this.precompileQueue = [];
      this.lastStats = 0;
      this.mode = 'idle';
      this.sizeDirty = true;
      this.error = null;
      this.media = null;
      this.bgTex = new Map();
      canvas.addEventListener('webglcontextlost', (e) => { e.preventDefault(); this._onLost(); }, false);
      canvas.addEventListener('webglcontextrestored', () => this._onRestored(), false);
      if (typeof ResizeObserver !== 'undefined') {
        this.resizeObserver = new ResizeObserver(() => { this.sizeDirty = true; });
        this.resizeObserver.observe(canvas);
      }
      this._initGL();
    }

    on(name, fn) { (this.listeners[name] = this.listeners[name] || []).push(fn); }
    emit(name, data) { (this.listeners[name] || []).forEach((fn) => { try { fn(data); } catch (err) { console.error(err); } }); }

    _initGL() {
      const gl = this.canvas.getContext('webgl2', {
        alpha: false, antialias: false, depth: false, stencil: false,
        premultipliedAlpha: false, preserveDrawingBuffer: false, powerPreference: 'high-performance'
      });
      if (!gl) throw new Error('WebGL2 is not available in this browser.');
      this.gl = gl;
      this.extFloat = gl.getExtension('EXT_color_buffer_float');
      gl.getExtension('OES_texture_float_linear');
      this.extParallel = gl.getExtension('KHR_parallel_shader_compile');
      this.hdr = !!this.extFloat;
      this.programs = new Map();
      this.vao = gl.createVertexArray();
      gl.bindVertexArray(this.vao);
      gl.disable(gl.DEPTH_TEST);
      gl.disable(gl.CULL_FACE);
      this.targets = {};
      this.black = this._texture(1, 1, false);
      this.displayProg = this._buildProgram(DISPLAY_FRAG, DISPLAY_UNIFORMS);
      this._checkProgram(this.displayProg, true);
      this.sizeDirty = true;
      this.liveDirty = true;
    }

    // ---------- resources ----------
    _texture(w, h, hdr) {
      const gl = this.gl;
      const tex = gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, tex);
      if (hdr && this.hdr) gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA16F, w, h, 0, gl.RGBA, gl.HALF_FLOAT, null);
      else gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      return tex;
    }

    _target(name, w, h, hdr) {
      const gl = this.gl;
      const cur = this.targets[name];
      if (cur && cur.w === w && cur.h === h) return cur;
      if (cur) {
        gl.deleteFramebuffer(cur.fbo);
        gl.deleteTexture(cur.tex);
      }
      const tex = this._texture(w, h, hdr);
      const fbo = gl.createFramebuffer();
      gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
      gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT);
      const t = { tex, fbo, w, h, fresh: true };
      this.targets[name] = t;
      return t;
    }

    _freeTarget(name) {
      const t = this.targets[name];
      if (!t || !this.gl) return;
      this.gl.deleteFramebuffer(t.fbo);
      this.gl.deleteTexture(t.tex);
      delete this.targets[name];
    }

    _buildProgram(fsSource, uniformNames) {
      const gl = this.gl;
      const vs = gl.createShader(gl.VERTEX_SHADER);
      gl.shaderSource(vs, VERT);
      gl.compileShader(vs);
      const fs = gl.createShader(gl.FRAGMENT_SHADER);
      gl.shaderSource(fs, fsSource);
      gl.compileShader(fs);
      const prog = gl.createProgram();
      gl.attachShader(prog, vs);
      gl.attachShader(prog, fs);
      gl.linkProgram(prog);
      return { prog, vs, fs, uniformNames, ready: false, failed: false, u: null, started: performance.now() };
    }

    // Non-blocking readiness check; with `force` it waits (used for the tiny display shader).
    _checkProgram(entry, force) {
      if (entry.ready || entry.failed) return entry.ready;
      const gl = this.gl;
      if (!force && this.extParallel && !gl.getProgramParameter(entry.prog, this.extParallel.COMPLETION_STATUS_KHR)) return false;
      if (!gl.getProgramParameter(entry.prog, gl.LINK_STATUS)) {
        const log = gl.getShaderInfoLog(entry.fs) || gl.getProgramInfoLog(entry.prog) || 'unknown shader error';
        console.error('Shader build failed:', log);
        entry.failed = true;
        entry.log = log;
        return false;
      }
      entry.u = {};
      entry.uniformNames.forEach((n) => { entry.u[n] = gl.getUniformLocation(entry.prog, n); });
      gl.deleteShader(entry.vs);
      gl.deleteShader(entry.fs);
      entry.ready = true;
      return true;
    }

    _formulaProgram(id) {
      let entry = this.programs.get(id);
      if (!entry) {
        const f = Formulas.get(id);
        entry = this._buildProgram(FRAG_HEAD + f.glsl + FRAG_MAIN, FRACTAL_UNIFORMS);
        this.programs.set(id, entry);
      }
      return entry;
    }

    // ---------- auto-flight probe ----------
    canProbe() { return !!(this.gl && !this.lost && this.extFloat); }

    _probeProgram(id) {
      const key = `probe:${id}`;
      let entry = this.programs.get(key);
      if (!entry) {
        const f = Formulas.get(id);
        entry = this._buildProgram(FRAG_HEAD + f.glsl + MAP_SRC + PROBE_MAIN, FRACTAL_UNIFORMS.concat(['uPPos', 'uDirs', 'uMaxRay', 'uEps']));
        this.programs.set(key, entry);
      }
      return entry;
    }

    // Synchronous: draws a 32x1 float target and reads it back.
    probe(frame, pos, dirs, maxRay, eps) {
      const gl = this.gl;
      if (!this.canProbe()) throw new Error('This browser cannot read float results from the GPU');
      const entry = this._probeProgram(frame.formula);
      if (!this._checkProgram(entry, true)) throw new Error(entry.log || 'probe shader failed');
      if (!this.probeT) {
        const tex = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, tex);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, PROBE_N, 1, 0, gl.RGBA, gl.FLOAT, null);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        const fbo = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
        this.probeT = { tex, fbo, buf: new Float32Array(PROBE_N * 4), dirs: new Float32Array(75) };
      }
      const P = this.probeT;
      gl.bindFramebuffer(gl.FRAMEBUFFER, P.fbo);
      gl.viewport(0, 0, PROBE_N, 1);
      gl.disable(gl.BLEND);
      gl.useProgram(entry.prog);
      this._setFractalUniforms(entry, frame, PROBE_N, 1, [0, 0]);
      P.dirs.fill(0);
      dirs.slice(0, 25).forEach((d, i) => { P.dirs[i * 3] = d[0]; P.dirs[i * 3 + 1] = d[1]; P.dirs[i * 3 + 2] = d[2]; });
      gl.uniform3fv(entry.u.uPPos, pos);
      gl.uniform3fv(entry.u.uDirs, P.dirs);
      gl.uniform1f(entry.u.uMaxRay, maxRay);
      gl.uniform1f(entry.u.uEps, eps);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      gl.readPixels(0, 0, PROBE_N, 1, gl.RGBA, gl.FLOAT, P.buf);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      const b = P.buf;
      const at = (i) => b[i * 4];
      return {
        d: at(0),
        grad: [(at(1) - at(2)) / (2 * eps), (at(3) - at(4)) / (2 * eps), (at(5) - at(6)) / (2 * eps)],
        rays: dirs.slice(0, 25).map((_, k) => at(7 + k))
      };
    }

    // Queue formulas to compile in the background while idle.
    precompile(ids) {
      ids.forEach((id) => { if (!this.programs.has(id) && !this.precompileQueue.includes(id)) this.precompileQueue.push(id); });
    }

    // ---------- state from the app ----------
    setQuality(q) {
      if (!QUALITY[q] || q === this.quality) return;
      this.quality = q;
      this.liveScale = Math.min(this.liveScale, QUALITY[q].liveMax);
      this._invalidate();
    }

    setAspect(aspect) {
      if (!(aspect > 0) || Math.abs(aspect - this.aspect) < 1e-6) return;
      this.aspect = aspect;
      this._invalidate();
    }

    setEnabled(on) {
      this.enabled = !!on;
      if (on) this._invalidate();
    }

    setFrame(frame) {
      this.frame = frame;
      const key = frame.formula + JSON.stringify(frame.gpu, (k, v) => (v instanceof Float32Array ? Array.from(v) : v));
      if (key !== this.gfxKey) {
        this.gfxKey = key;
        this._invalidate();
      } else {
        this.displayDirty = true;
      }
    }

    _invalidate() {
      this.liveDirty = true;
      this.accumFresh = true;
      this.samples = 0;
      this.tileCursor = 0;
      this.refineAt = performance.now() + REFINE_DELAY_MS;
    }

    // Frame-guide geometry (the render aspect inside the canvas), in pixels of a target of size w x h.
    _frameGeom(w, h) {
      const a = this.aspect;
      const gh = w / h > a ? h : w / a;
      return { cx: w / 2, cy: h / 2, fh: gh };
    }

    guideRect() {
      const w = this.canvas.clientWidth;
      const h = this.canvas.clientHeight;
      const g = this._frameGeom(w, h);
      const gw = g.fh * this.aspect;
      return { x: (w - gw) / 2, y: (h - g.fh) / 2, w: gw, h: g.fh };
    }

    _syncSize() {
      if (!this.sizeDirty) return;
      this.sizeDirty = false;
      const dpr = Math.min(window.devicePixelRatio || 1, MAX_DPR);
      const w = Math.max(16, Math.round(this.canvas.clientWidth * dpr));
      const h = Math.max(16, Math.round(this.canvas.clientHeight * dpr));
      if (w !== this.canvas.width || h !== this.canvas.height) {
        this.canvas.width = w;
        this.canvas.height = h;
        this.tiles = [];
        this._invalidate();
      }
    }

    // ---------- uniforms ----------
    _setFractalUniforms(entry, frame, w, h, jit, overrides) {
      const gl = this.gl;
      const u = entry.u;
      const g = frame.gpu;
      const q = QUALITY[(overrides && overrides.quality) || this.quality];
      const geom = (overrides && overrides.geom) || this._frameGeom(w, h);
      gl.uniform2f(u.uRes, w, h);
      gl.uniform2f(u.uCenter, geom.cx, geom.cy);
      gl.uniform1f(u.uFrameH, geom.fh);
      gl.uniform2f(u.uJitter, jit[0], jit[1]);
      const vr = overrides && overrides.vr;
      const oc = overrides && overrides.cam;
      let cr = oc ? oc.right : g.camRight;
      let cu = oc ? oc.up : g.camUp;
      if (vr && vr.level) {
        // keep the horizon level: drop the camera roll (pitch and heading stay)
        const f = g.camFwd;
        const rl = Math.hypot(f[2], f[0]) || 1;
        cr = [-f[2] / rl, 0, f[0] / rl];
        cu = [cr[1] * f[2] - cr[2] * f[1], cr[2] * f[0] - cr[0] * f[2], cr[0] * f[1] - cr[1] * f[0]];
      }
      gl.uniform3fv(u.uCamPos, oc ? oc.pos : g.camPos);
      gl.uniform3fv(u.uCamRight, cr);
      gl.uniform3fv(u.uCamUp, cu);
      gl.uniform3fv(u.uCamFwd, oc ? oc.fwd : g.camFwd);
      gl.uniform1f(u.uFovT, oc ? oc.fovT : g.fovT);
      gl.uniform1i(u.uVR, vr ? 1 : 0);
      gl.uniform1i(u.uVRLayout, vr ? vr.layout : 0);
      gl.uniform1f(u.uVRSpan, vr ? vr.span : Math.PI * 2);
      gl.uniform1f(u.uIpd, vr ? vr.ipd : 0);
      gl.uniform1fv(u.uP, g.p);
      gl.uniform1i(u.uIter, g.iter);
      gl.uniformMatrix3fv(u.uRotA, false, g.rotA);
      gl.uniformMatrix3fv(u.uRotB, false, g.rotB);
      gl.uniform1f(u.uRepeat, g.repeat);
      gl.uniform1f(u.uTwist, g.twist);
      gl.uniform1f(u.uBound, g.bound);
      gl.uniform1f(u.uStep, g.step);
      gl.uniform1f(u.uDetail, g.detail);
      gl.uniform1f(u.uSize, g.size);
      gl.uniform1i(u.uMaxSteps, q.steps);
      gl.uniform1i(u.uShadowSteps, g.shadows ? q.shadow : 0);
      gl.uniform1f(u.uMaxDist, g.maxDist);
      gl.uniform1f(u.uAO, g.ao);
      gl.uniform3fv(u.uPalC, g.palCol);
      gl.uniform1fv(u.uPalP, g.palPos);
      gl.uniform1i(u.uPalN, g.palN);
      gl.uniform1i(u.uPalInterp, g.palInterp);
      gl.uniform1i(u.uColorSrc, g.colorSrc);
      gl.uniform1i(u.uPalWrap, g.palWrap);
      gl.uniform1f(u.uPalScale, g.palScale);
      gl.uniform1f(u.uPalOffset, g.palOffset);
      gl.uniform3fv(u.uBgTop, g.bgTop);
      gl.uniform3fv(u.uBgBot, g.bgBottom);
      gl.uniform3fv(u.uFogCol, g.fogColor || g.bgBottom);
      gl.uniform1i(u.uFogMatch, g.fogColor ? 0 : 1);
      gl.uniform1f(u.uFog, g.fog);
      gl.uniform3fv(u.uGlowCol, g.glowColor);
      gl.uniform3fv(u.uSunDir, g.sunDir);
      gl.uniform3fv(u.uSunCol, g.sunColor);
      gl.uniform3fv(u.uFillCol, g.fillColor);
      gl.uniform1f(u.uAmbient, g.ambient);
      gl.uniform1f(u.uSpec, g.specular);
      gl.uniform1i(u.uEncode, this.hdr ? 0 : 1);
      // backdrop layers on texture units 2 and 3; a layer whose media is not loaded yet shows the sky
      const layers = (g.backdrop && g.backdrop.layers) || [];
      const amt = [0, 0];
      const gain = [1, 1];
      const sat = [1, 1];
      const lod = [0, 0];
      const front = [0, 0];
      const mode = [0, 0];
      ['0', '1'].forEach((n, i) => {
        const L = layers[i];
        const tex = L && this.bgTex.get(L.key);
        gl.activeTexture(gl.TEXTURE2 + i);
        gl.bindTexture(gl.TEXTURE_2D, tex ? tex.tex : this.black);
        gl.uniform1i(u[`uBg${n}`], 2 + i);
        if (!tex) return;
        amt[i] = L.amount;
        gain[i] = L.gain;
        sat[i] = L.sat;
        lod[i] = L.lod;
        front[i] = L.front || 0;
        mode[i] = L.mode || 0;
        gl.uniform2fv(u[`uBgS${n}`], L.scale);
        gl.uniform2fv(u[`uBgO${n}`], L.offset);
      });
      gl.uniform2fv(u.uBgAmt, amt);
      gl.uniform2fv(u.uBgGain, gain);
      gl.uniform2fv(u.uBgSat, sat);
      gl.uniform2fv(u.uBgLod, lod);
      gl.uniform2fv(u.uBgFront, front);
      gl.uniform2fv(u.uBgMode, mode);
      gl.uniform1f(u.uBgFogFade, g.backdrop && (amt[0] || amt[1]) ? g.backdrop.fogFade : 0);
    }

    // ---------- backdrop textures ----------
    // The media source (app side) owns the <video>/ImageBitmap elements; here they are only uploaded
    // when their version changes (new movie frame, finished load, seek).
    setMediaSource(src) { this.media = src; }

    forgetBackdrop(key) {
      const e = this.bgTex.get(key);
      if (e && this.gl) this.gl.deleteTexture(e.tex);
      this.bgTex.delete(key);
    }

    _syncBackdrops(frame) {
      const layers = frame && frame.gpu.backdrop ? frame.gpu.backdrop.layers : null;
      if (!layers || !this.media) return false;
      const gl = this.gl;
      let changed = false;
      layers.forEach((L) => {
        const m = this.media.get(L.key);
        if (!m || !m.ready) return;
        let e = this.bgTex.get(L.key);
        if (e && e.version === m.version) return;
        if (!e) {
          const tex = gl.createTexture();
          gl.bindTexture(gl.TEXTURE_2D, tex);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
          e = { tex, version: -1 };
        }
        try {
          gl.activeTexture(gl.TEXTURE4);
          gl.bindTexture(gl.TEXTURE_2D, e.tex);
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, m.el);
          gl.generateMipmap(gl.TEXTURE_2D);
          e.version = m.version;
          this.bgTex.set(L.key, e);
          changed = true;
        } catch (err) {
          // a movie with no decoded frame yet: try again on the next version
          if (!this.bgTex.has(L.key)) gl.deleteTexture(e.tex);
        }
      });
      return changed;
    }

    _drawDisplay(fbo, w, h, liveTex, accumTex, useAccum, post, geom, origin) {
      const gl = this.gl;
      const e = this.displayProg;
      const u = e.u;
      const ox = origin ? origin[0] : 0;
      const oy = origin ? origin[1] : 0;
      gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
      gl.viewport(ox, oy, w, h);
      gl.disable(gl.BLEND);
      gl.disable(gl.SCISSOR_TEST);
      gl.useProgram(e.prog);
      gl.uniform2f(u.uOrigin, ox, oy);
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, liveTex || this.black);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, accumTex || this.black);
      gl.uniform1i(u.uLive, 0);
      gl.uniform1i(u.uAccum, 1);
      gl.uniform1i(u.uUseAccum, useAccum ? 1 : 0);
      gl.uniform1i(u.uEncode, this.hdr ? 0 : 1);
      gl.uniform2f(u.uOut, w, h);
      gl.uniform2f(u.uCenter, geom.cx + ox, geom.cy + oy);
      gl.uniform1f(u.uFrameH, geom.fh);
      gl.uniform1f(u.uExposure, post.exposure);
      gl.uniform1f(u.uContrast, post.contrast);
      gl.uniform1f(u.uSat, post.saturation);
      gl.uniform1f(u.uHue, post.hue);
      gl.uniform1f(u.uVignette, post.vignette);
      gl.uniform1i(u.uTonemap, post.tonemap);
      gl.uniform1f(u.uSeed, (this.samples % 64) * 0.618);
      gl.uniform1f(u.uGrain, post.grain || 0);
      gl.uniform1f(u.uGrainSeed, post.grainSeed || 0);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
    }

    // ---------- per-frame work ----------
    tick(now) {
      if (this.lost || !this.gl) return;
      const dt = this.lastTick ? now - this.lastTick : 16;
      this.lastTick = now;
      if (dt < 500) {
        this.dtAvg = this.dtAvg * 0.85 + dt * 0.15;
        this._adapt(dt);
      }
      this.prevWork = null;
      if (!this.enabled) return;
      this._syncSize();
      if (this.job) { this._syncBackdrops(this.job.frame); this._runJob(); return; }
      if (this.hold || !this.frame) return;
      // a backdrop finished loading, seeked, or advanced a movie frame: draw again
      if (this._syncBackdrops(this.frame) && !this.liveDirty) this._invalidate();

      const entry = this._formulaProgram(this.frame.formula);
      if (!this._checkProgram(entry)) {
        this._setMode(entry.failed ? 'error' : 'compiling', entry.failed ? entry.log : null);
        return;
      }
      const q = QUALITY[this.quality];
      const cw = this.canvas.width;
      const ch = this.canvas.height;

      if (this.liveDirty) {
        let s = Math.min(this.liveScale, q.liveMax);
        if (cw * ch * s * s > LIVE_PIXEL_CAP) s = Math.sqrt(LIVE_PIXEL_CAP / (cw * ch));
        const lw = Math.max(8, Math.round(cw * s));
        const lh = Math.max(8, Math.round(ch * s));
        const live = this._target('live', lw, lh, true);
        const gl = this.gl;
        gl.bindFramebuffer(gl.FRAMEBUFFER, live.fbo);
        gl.viewport(0, 0, lw, lh);
        gl.disable(gl.BLEND);
        gl.disable(gl.SCISSOR_TEST);
        gl.useProgram(entry.prog);
        this._setFractalUniforms(entry, this.frame, lw, lh, [0, 0]);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
        live.fresh = false;
        this.liveDirty = false;
        this.prevWork = 'live';
        this.liveRes = [lw, lh];
        this._display(false);
        this._setMode('live');
        return;
      }

      const target = this.hdr ? q.samples : 1;
      if (this.samples < target && now >= this.refineAt) {
        this._refineStep(entry, q, cw, ch);
        return;
      }

      if (this.displayDirty) {
        this._display(this.samples > 0 || this.tileCursor > 0);
        this.displayDirty = false;
      }
      if (this.samples >= target) this._setMode('idle');

      if (this.precompileQueue.length && this.mode === 'idle') {
        const id = this.precompileQueue.shift();
        this._formulaProgram(id);
      }
    }

    _adapt(dt) {
      if (this.prevWork === 'live') {
        const q = QUALITY[this.quality];
        if (dt > 50) this.liveScale *= 0.8;
        else if (dt > 34) this.liveScale *= 0.93;
        else if (dt < 21) this.liveScale *= 1.04;
        this.liveScale = Math.min(q.liveMax, Math.max(0.18, this.liveScale));
      } else if (this.prevWork === 'tiles') {
        if (dt > 45) this.tilesPerTick = Math.max(1, Math.floor(this.tilesPerTick * 0.6));
        else if (dt > 30) this.tilesPerTick = Math.max(1, this.tilesPerTick - 1);
        else if (dt < 21) this.tilesPerTick = Math.min(48, this.tilesPerTick + 1);
      }
    }

    _refineSize(cw, ch, q) {
      let s = q.refineMax;
      if (cw * ch * s * s > REFINE_PIXEL_CAP) s = Math.sqrt(REFINE_PIXEL_CAP / (cw * ch));
      return [Math.max(8, Math.round(cw * s)), Math.max(8, Math.round(ch * s))];
    }

    _tileList(w, h) {
      const list = [];
      for (let y = 0; y < h; y += TILE) for (let x = 0; x < w; x += TILE) list.push([x, y, Math.min(TILE, w - x), Math.min(TILE, h - y)]);
      const cx = w / 2;
      const cy = h / 2;
      list.sort((a, b) => Math.hypot(a[0] + a[2] / 2 - cx, a[1] + a[3] / 2 - cy) - Math.hypot(b[0] + b[2] / 2 - cx, b[1] + b[3] / 2 - cy));
      return list;
    }

    _refineStep(entry, q, cw, ch) {
      const gl = this.gl;
      const [rw, rh] = this._refineSize(cw, ch, q);
      const accum = this._target('accum', rw, rh, true);
      if (!this.tiles.length || this.tilesW !== rw || this.tilesH !== rh) {
        this.tiles = this._tileList(rw, rh);
        this.tilesW = rw;
        this.tilesH = rh;
      }
      gl.bindFramebuffer(gl.FRAMEBUFFER, accum.fbo);
      gl.viewport(0, 0, rw, rh);
      gl.disable(gl.SCISSOR_TEST);
      if (this.accumFresh) {
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        this.accumFresh = false;
        this.samples = 0;
        this.tileCursor = 0;
      }
      gl.useProgram(entry.prog);
      this._setFractalUniforms(entry, this.frame, rw, rh, jitter(this.samples));
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.ONE, gl.ONE);
      gl.enable(gl.SCISSOR_TEST);
      const n = Math.min(this.tilesPerTick, this.tiles.length - this.tileCursor);
      for (let i = 0; i < n; i += 1) {
        const t = this.tiles[this.tileCursor + i];
        gl.scissor(t[0], t[1], t[2], t[3]);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
      }
      gl.disable(gl.SCISSOR_TEST);
      gl.disable(gl.BLEND);
      this.tileCursor += n;
      this.prevWork = 'tiles';
      let passDone = false;
      if (this.tileCursor >= this.tiles.length) {
        this.tileCursor = 0;
        this.samples += 1;
        passDone = true;
      }
      this._display(true);
      const target = this.hdr ? QUALITY[this.quality].samples : 1;
      this._setMode('refine', null, this.samples + (passDone ? 0 : this.tileCursor / this.tiles.length), target);
      if (passDone && this.samples === 1) this.emit('refined', this.frame);
    }

    _display(useAccum) {
      if (!this.frame) return;
      const cw = this.canvas.width;
      const ch = this.canvas.height;
      const live = this.targets.live;
      const accum = this.targets.accum;
      this._drawDisplay(null, cw, ch, live && live.tex, accum && accum.tex, useAccum && !!accum, this.frame.post, this._frameGeom(cw, ch));
    }

    _setMode(mode, info, progress, target) {
      this.mode = mode;
      const now = performance.now();
      if (now - this.lastStats < 200 && mode !== 'idle' && mode !== 'error') return;
      if (mode === 'idle' && this._idleReported) return;
      this._idleReported = mode === 'idle';
      this.lastStats = now;
      this.emit('stats', {
        mode, info, progress: progress || 0, target: target || 0,
        fps: 1000 / Math.max(this.dtAvg, 1),
        liveScale: this.liveScale,
        live: this.liveRes,
        size: [this.canvas.width, this.canvas.height],
        hdr: this.hdr
      });
    }

    // ---------- still render (full-resolution PNG of one frame) ----------
    renderStill(frame, width, height, samples, onProgress, raw, opts) {
      const gl = this.gl;
      if (!gl || this.lost) return Promise.reject(new Error('GPU context lost'));
      const max = gl.getParameter(gl.MAX_TEXTURE_SIZE);
      const w = Math.min(max, Math.max(16, Math.round(width)));
      const h = Math.min(max, Math.max(16, Math.round(height)));
      const total = this.hdr ? Math.max(1, Math.round(samples)) : 1;
      return new Promise((resolve, reject) => {
        this.job = {
          frame, w, h, total, onProgress, resolve, reject, raw: !!raw, march: QUALITY[(opts || {}).march] ? opts.march : 'high', vr: (opts && opts.vr) || null,
          tiles: this._tileList(w, h), cursor: 0, sample: 0, perTick: 4, started: false, cancelled: false
        };
      });
    }

    // One movie frame: resolves to the graded RGBA8 pixels, bottom row first (as read from WebGL).
    // opts.march: ray-march quality ('high' unless a movie preset asks for less).
    renderFrame(frame, width, height, samples, onProgress, opts) {
      return this.renderStill(frame, width, height, samples, onProgress, true, opts);
    }

    // ---------- live VR (WebXR) ----------
    // One eye of a WebXR frame: the scene camera becomes the origin of the room, the headset's pose moves and
    // turns the eye inside it. view = XRView (or any {projectionMatrix, transform: {matrix}}), vp = its viewport
    // in fbo (the XR layer's framebuffer). opts: { scale (metres -> world units), level, quality }.
    // The eye ray for NDC (x, y) is C + x DX + y DY; the fractal shader's camera has exactly that form.
    renderXR(frame, view, vp, fbo, opts) {
      const gl = this.gl;
      if (!gl || this.lost) return false;
      const entry = this._formulaProgram(frame.formula);
      if (!this._checkProgram(entry)) return false;
      this._syncBackdrops(frame);
      const g = frame.gpu;
      const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
      const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
      const len = (a) => Math.hypot(a[0], a[1], a[2]) || 1;
      let F = g.camFwd;
      let R = g.camRight;
      let U = g.camUp;
      let base = g.camPos;
      let scale = opts.scale;
      if (opts.rig) {
        // VR mode: an explicit room -> world rig (world = O + S * R * room), R = [right, up, back]
        R = opts.rig.R[0];
        U = opts.rig.R[1];
        F = mul(opts.rig.R[2], -1);
        base = opts.rig.O;
        scale = opts.rig.S;
      } else if (opts.level) {
        // the room stands upright: heading only; the head does the looking up and down
        let fh = [F[0], 0, F[2]];
        if (Math.hypot(fh[0], fh[2]) < 1e-3) fh = [U[0], 0, U[2]];
        F = mul(fh, 1 / len(fh));
        R = [-F[2], 0, F[0]];
        U = [0, 1, 0];
      }
      const W = (v) => add(add(mul(R, v[0]), mul(U, v[1])), mul(F, -v[2]));
      const M = view.transform.matrix;
      const rot = (v) => [M[0] * v[0] + M[4] * v[1] + M[8] * v[2], M[1] * v[0] + M[5] * v[1] + M[9] * v[2], M[2] * v[0] + M[6] * v[1] + M[10] * v[2]];
      const P = view.projectionMatrix;
      const C = W(rot([P[8] / P[0], P[9] / P[5], -1]));
      const DX = W(rot([1 / P[0], 0, 0]));
      const DY = W(rot([0, 1 / P[5], 0]));
      const pos = add(base, mul(W([M[12], M[13], M[14]]), scale));
      const w = vp.width;
      const h = vp.height;
      const ft = len(DY);
      const cam = { pos, fwd: C, right: mul(DX, h / w / ft), up: mul(DY, 1 / ft), fovT: ft };
      // opts.res: the fractal is marched at this share of the eye's size and scaled up by the display pass
      const res = opts.res > 0 ? Math.min(1, opts.res) : 1;
      const rw = Math.max(8, Math.round(w * res));
      const rh = Math.max(8, Math.round(h * res));
      const live = this._target('xr', rw, rh, true);
      const geom = { cx: rw / 2, cy: rh / 2, fh: rh };
      let fr = frame;
      if (opts.rig && frame.camera) {
        // flying away from the target must not run into the far limit
        const c = frame.camera;
        const away = len([pos[0] - (c.tx || 0), pos[1] - (c.ty || 0), pos[2] - (c.tz || 0)]);
        if (away * 3 > g.maxDist) fr = Object.assign({}, frame, { gpu: Object.assign({}, g, { maxDist: away * 3 }) });
      }
      gl.bindFramebuffer(gl.FRAMEBUFFER, live.fbo);
      gl.viewport(0, 0, rw, rh);
      gl.disable(gl.BLEND);
      gl.disable(gl.SCISSOR_TEST);
      gl.useProgram(entry.prog);
      this._setFractalUniforms(entry, fr, rw, rh, [0, 0], { quality: opts.quality || 'draft', geom, cam });
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      this._drawDisplay(fbo, w, h, live.tex, null, false, Object.assign({}, frame.post, { vignette: 0 }), { cx: w / 2, cy: h / 2, fh: h }, [vp.x, vp.y]);
      return true;
    }

    // While a movie renders, the preview does no work of its own (jobs still run).
    setHold(on) {
      this.hold = !!on;
      if (!on) {
        this._freeTarget('still');
        this._freeTarget('stillOut');
        this._invalidate();
      }
    }

    cancelStill() {
      if (this.job) this.job.cancelled = true;
    }

    _runJob() {
      const job = this.job;
      const gl = this.gl;
      if (job.cancelled) {
        this._endJob();
        job.reject(new Error('cancelled'));
        return;
      }
      const entry = this._formulaProgram(job.frame.formula);
      if (!this._checkProgram(entry)) {
        if (entry.failed) { this._endJob(); job.reject(new Error(entry.log)); }
        return;
      }
      const acc = this._target('still', job.w, job.h, true);
      gl.bindFramebuffer(gl.FRAMEBUFFER, acc.fbo);
      gl.viewport(0, 0, job.w, job.h);
      if (!job.started) {
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        job.started = true;
      }
      // Stills use High ray-march settings; movie presets may ask for less.
      const geom = { cx: job.w / 2, cy: job.h / 2, fh: job.h };
      gl.useProgram(entry.prog);
      this._setFractalUniforms(entry, job.frame, job.w, job.h, jitter(job.sample), { quality: job.march, geom, vr: job.vr });
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.ONE, gl.ONE);
      gl.enable(gl.SCISSOR_TEST);
      const n = Math.min(job.perTick, job.tiles.length - job.cursor);
      for (let i = 0; i < n; i += 1) {
        const t = job.tiles[job.cursor + i];
        gl.scissor(t[0], t[1], t[2], t[3]);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
      }
      gl.disable(gl.SCISSOR_TEST);
      gl.disable(gl.BLEND);
      job.cursor += n;
      this.prevWork = 'tiles';
      job.perTick = Math.max(1, Math.min(48, this.tilesPerTick));
      if (job.cursor >= job.tiles.length) {
        job.cursor = 0;
        job.sample += 1;
      }
      const progress = (job.sample + job.cursor / job.tiles.length) / job.total;
      if (job.onProgress) job.onProgress(progress);
      if (job.sample < job.total) return;

      const out = this._target('stillOut', job.w, job.h, false);
      this._drawDisplay(out.fbo, job.w, job.h, null, acc.tex, true, job.vr ? Object.assign({}, job.frame.post, { vignette: 0 }) : job.frame.post, geom);
      const pixels = new Uint8Array(job.w * job.h * 4);
      gl.readPixels(0, 0, job.w, job.h, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
      this._endJob();
      if (job.raw) { job.resolve(pixels); return; }
      const c = document.createElement('canvas');
      c.width = job.w;
      c.height = job.h;
      const ctx = c.getContext('2d');
      const img = ctx.createImageData(job.w, job.h);
      const row = job.w * 4;
      for (let y = 0; y < job.h; y += 1) img.data.set(pixels.subarray((job.h - 1 - y) * row, (job.h - y) * row), y * row);
      ctx.putImageData(img, 0, 0);
      c.toBlob((blob) => (blob ? job.resolve(blob) : job.reject(new Error('PNG encoding failed'))), 'image/png');
    }

    _endJob() {
      this.job = null;
      if (this.hold) return; // movie render: keep the targets for the next frame
      this._freeTarget('still');
      this._freeTarget('stillOut');
      this._invalidate();
    }

    // ---------- context loss ----------
    _onLost() {
      this.lost = true;
      this.lossCount += 1;
      this.programs = new Map();
      this.targets = {};
      this.probeT = null;
      this.bgTex = new Map();
      if (this.job) {
        const job = this.job;
        this.job = null;
        job.reject(new Error('GPU context lost'));
      }
      this.emit('lost', { count: this.lossCount });
    }

    _onRestored() {
      this.lost = false;
      this._initGL();
      if (this.lossCount >= 1) this.quality = 'draft';
      if (this.lossCount >= 2) this.enabled = false;
      this._invalidate();
      this.emit('restored', { count: this.lossCount, enabled: this.enabled });
    }

    // Manual restart after repeated losses.
    restart() {
      this.enabled = true;
      if (this.lost) {
        const ext = this.gl && this.gl.getExtension('WEBGL_lose_context');
        if (ext) ext.restoreContext();
        return;
      }
      this._invalidate();
    }
  }

  Renderer.QUALITY = QUALITY;
  FV.Renderer = Renderer;
})();
