/* Lysrytme: light that flickers at a chosen frequency, in a Quest 3 or on a
   flat screen, with Sleeper Agent's sound underneath and a pulse in step.

   Plain WebGL2 and WebXR, no three.js: the whole picture is one full-screen
   shader per eye, worked out from the direction each pixel looks in (view
   space), so every pattern is fixed to the head like a pair of light
   glasses, and the per-frame cost is a handful of uniforms.

   Timing. The display shows a new picture every 1/fr seconds (fr = 120, 90
   or 72 on a Quest 3). The light's phase is read from the audio clock (see
   sound.js) at each frame's display time. When fr/f is a whole number the
   frequency is exact, and the waveform is sampled at the middle of the
   frame, so a hard blink is a clean on/off. Otherwise it is averaged over
   the frame, which turns the unavoidable jitter into small steps in
   brightness instead of blinks of uneven length. */

import { createSound } from './sound.js?v=2';

const Q = new URLSearchParams(location.search);
const $ = (id) => document.getElementById(id);
const TAU = Math.PI * 2;
const frac = (x) => x - Math.floor(x);
const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
const num = (x, d = 1) => x.toFixed(d).replace('.', ',');

/* ---------------- what the light can do ---------------- */

const E5 = Math.exp(-5);
export const WAVES = [
  { id: 'hard', name: 'Hårdt blink', fn: (p) => (p < 0.5 ? 1 : 0) },
  { id: 'soft', name: 'Blød bølge', fn: (p) => 0.5 - 0.5 * Math.cos(TAU * p) },
  { id: 'glimt', name: 'Kort glimt', fn: (p) => (p < 0.25 ? 1 : 0) },
  { id: 'fade', name: 'Blink og fade', fn: (p) => (Math.exp(-5 * p) - E5) / (1 - E5) },
  { id: 'swell', name: 'Svulmende', fn: (p) => p * p * (3 - 2 * p) * (p < 0.85 ? 1 : (1 - p) / 0.15) },
];
export const PATTERNS = [
  { id: 'felt', name: 'Helt felt' },
  { id: 'gradient', name: 'Farvegradient' },
  { id: 'ringe', name: 'Ringe' },
  { id: 'skak', name: 'Skakternet der vender' },
  { id: 'spiral', name: 'Spiral' },
  { id: 'farve', name: 'Farveskift' },
];
export const COLORS = [
  { id: 'varm', name: 'Varm', a: '#ffb060', c: '#8a2050' },
  { id: 'hvid', name: 'Hvid', a: '#ffffff', c: '#3a4a66' },
  { id: 'blaa', name: 'Blå og violet', a: '#6a8cff', c: '#9030c8' },
  { id: 'groen', name: 'Grøn og turkis', a: '#60ffb0', c: '#1a60b0' },
  { id: 'regnbue', name: 'Regnbue (glider)', rainbow: true },
];
/* Frequency points in minutes; between them the frequency glides (in
   proportion, so 10 to 5 Hz takes as long as 6 to 3 Hz). */
export const PROGRAMS = [
  { id: 'fri', name: 'Fri', desc: 'Du styrer selv frekvensen. Ingen tidsgrænse.' },
  { id: 'fokus', name: 'Fokus · 30 Hz', min: 15, pts: [[0, 30]], desc: '15 minutter ved 30 Hz (lav gamma).' },
  { id: 'gamma', name: 'Gamma · 40 Hz', min: 20, pts: [[0, 40]], wave: 'hard', desc: '20 minutter ved 40 Hz, som i MIT-forsøgene (de brugte lys og lyd sammen: slå takt-lyd til).' },
  { id: 'ro', name: 'Ro · 10 Hz', min: 15, pts: [[0, 12], [3, 10]], desc: 'Fra 12 til 10 Hz (alfa) over 3 minutter, derefter 10 Hz.' },
  { id: 'meditation', name: 'Meditation · 10 → 6 Hz', min: 20, pts: [[0, 10], [8, 6]], desc: 'Glider fra alfa ned i theta over 8 minutter og bliver der.' },
  { id: 'sovn', name: 'Søvn · 8 → 3 Hz', min: 25, pts: [[0, 8], [15, 3]], dim: [18, 25], wave: 'soft', desc: 'Glider ned i delta over 15 minutter; lyset dæmpes langsomt til mørke fra minut 18.' },
];
const STEPS = [2, 2.5, 3, 3.5, 4, 4.5, 5, 6, 7, 7.5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22.5, 24, 25, 30, 36, 40];

export function isExact(f, fr) { const k = fr / f; return Math.abs(k - Math.round(k)) < 0.004 && Math.round(k) >= 2; }
export function band(f) {
  return f < 4 ? 'delta' : f < 8 ? 'theta' : f < 13 ? 'alfa' : f < 30 ? 'beta' : 'gamma';
}
function stepsFor(fr, exactOnly) { return exactOnly ? STEPS.filter((f) => isExact(f, fr)) : STEPS; }
function nearestExact(f, fr) {
  const k = Math.max(2, Math.round(fr / f));
  return fr / k;
}
function progFreq(pg, tMin) {
  const P = pg.pts;
  if (tMin <= P[0][0]) return P[0][1];
  for (let i = 1; i < P.length; i++) {
    if (tMin <= P[i][0]) {
      const u = (tMin - P[i - 1][0]) / (P[i][0] - P[i - 1][0]);
      return P[i - 1][1] * Math.pow(P[i][1] / P[i - 1][1], u);
    }
  }
  return P[P.length - 1][1];
}

/* The light level of one frame. Exact frequencies are point-sampled in the
   middle of the frame; others are averaged over it. Exported for tests. */
export function frameLevel(wave, ph0, ph1, exact) {
  if (exact) return wave(frac((ph0 + ph1) * 0.5));
  let s = 0;
  for (let i = 0; i < 8; i++) s += wave(frac(ph0 + (ph1 - ph0) * (i + 0.5) / 8));
  return s / 8;
}
export function levelsFor(f, fr, waveId, n, startPhase = 0) {
  const w = WAVES.find((x) => x.id === waveId).fn, ex = isExact(f, fr), out = [];
  for (let i = 0; i < n; i++) out.push(+frameLevel(w, startPhase + f * i / fr, startPhase + f * (i + 1) / fr, ex).toFixed(3));
  return out;
}

/* ---------------- settings ---------------- */

const KEY = 'brokild-lysrytme';
const DEF = {
  freq: 10, exact: true, wave: 'soft', pattern: 'felt', color: 'varm', eyes: 'ens', calm: false,
  level: 0.7, contrast: 1, program: 'fri', rate: 120, fadeIn: 10,
  sound: 'off', volume: 0.6, pulse: 'off', pulseLevel: 0.5, accepted: false,
};
const S = Object.assign({}, DEF);
try { Object.assign(S, JSON.parse(localStorage.getItem(KEY) || '{}')); } catch (_) { /* private window */ }
const save = () => { try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (_) { /* fine */ } };
const waveOf = () => (WAVES.find((w) => w.id === S.wave) || WAVES[1]);
const progOf = () => (PROGRAMS.find((p) => p.id === S.program) || PROGRAMS[0]);

/* ---------------- sound ---------------- */

const sound = createSound();
sound.setFreq(S.freq, true);
sound.setWave(waveOf().fn);

/* ---------------- WebGL ---------------- */

const VS = `#version 300 es
in vec2 aP; void main(){ gl_Position = vec4(aP, 0.0, 1.0); }`;
const FS = `#version 300 es
precision highp float;
uniform mat4 uInvProj;
uniform vec4 uVp;          /* viewport x, y, w, h in pixels */
uniform float uB;          /* this frame's light level, 0..1, contrast already applied */
uniform float uRaw;        /* the waveform itself, 0..1, for patterns that swap rather than dim */
uniform int uPat;
uniform vec3 uA, uC;       /* linear colours */
uniform float uLevel, uT, uCalm, uHudA, uPause;
uniform sampler2D uHud;
out vec4 o;
float stripe(float x, float w){ x = fract(x); return smoothstep(0.0, w, x) * smoothstep(0.5, 0.5 - w, x); }
void main(){
  vec2 ndc = (gl_FragCoord.xy - uVp.xy) / uVp.zw * 2.0 - 1.0;
  vec4 v = uInvProj * vec4(ndc, 1.0, 1.0);
  vec3 d = normalize(v.xyz / v.w);
  float th = acos(clamp(-d.z, -1.0, 1.0));      /* angle from straight ahead */
  float az = atan(d.y, d.x);
  vec3 col;
  if (uPause > 0.5) {
    col = uA * 0.035;
  } else {
    if (uPat == 0) col = uA * uB;
    else if (uPat == 1) {
      float r = uT * 0.08;
      float g = smoothstep(-0.9, 0.9, d.y * cos(r) + d.x * sin(r));
      col = mix(uC, uA, g) * uB;
    } else if (uPat == 2) {
      float s = stripe(th * 2.6 - uT * 0.12, 0.18);
      col = mix(uC * 0.12, uA, s) * uB;
    } else if (uPat == 3) {
      float u = floor(th * 7.0), w = floor((az / 6.2831853 + 0.5) * 18.0);
      float chk = mod(u + w, 2.0);
      float val = mix(chk, 1.0 - chk, uRaw);
      col = mix(uC * 0.25, uA, val);
    } else if (uPat == 4) {
      float s = stripe(az / 6.2831853 * 3.0 + th * 1.6 - uT * 0.05, 0.12);
      col = mix(uC * 0.12, uA, s) * uB;
    } else col = mix(uC, uA, uRaw);
    if (uCalm > 0.5) {
      float m = smoothstep(0.16, 0.24, th);
      col = mix(uA * 0.03, col, m);
      col += vec3(0.25) * (1.0 - smoothstep(0.004, 0.007, th));
    }
  }
  col *= uLevel;
  if (uHudA > 0.0 && d.z < 0.0) {
    vec2 p = d.xy / -d.z;
    vec2 h = (p - vec2(0.0, -0.36)) / vec2(0.44, 0.183) * 0.5 + 0.5;
    if (h.x > 0.0 && h.x < 1.0 && h.y > 0.0 && h.y < 1.0) {
      vec4 t = texture(uHud, vec2(h.x, 1.0 - h.y));
      col = mix(col, t.rgb, t.a * uHudA);
    }
  }
  o = vec4(pow(max(col, 0.0), vec3(1.0 / 2.2)), 1.0);
}`;

const canvas = $('gl');
const gl = canvas.getContext('webgl2', { xrCompatible: true, antialias: false, alpha: false, depth: false, stencil: false });
let prog, U = {}, hudTex, vao;
function initGL() {
  const sh = (t, s) => { const x = gl.createShader(t); gl.shaderSource(x, s); gl.compileShader(x); if (!gl.getShaderParameter(x, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(x)); return x; };
  prog = gl.createProgram();
  gl.attachShader(prog, sh(gl.VERTEX_SHADER, VS)); gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, FS));
  gl.bindAttribLocation(prog, 0, 'aP'); gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(prog));
  for (const n of ['uInvProj', 'uVp', 'uB', 'uRaw', 'uPat', 'uA', 'uC', 'uLevel', 'uT', 'uCalm', 'uHudA', 'uPause', 'uHud']) U[n] = gl.getUniformLocation(prog, n);
  vao = gl.createVertexArray(); gl.bindVertexArray(vao);
  const b = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, b);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0); gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  hudTex = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, hudTex);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(4));
}

function invert(m) {
  const a = m, o = new Float32Array(16);
  const [a00, a01, a02, a03, a10, a11, a12, a13, a20, a21, a22, a23, a30, a31, a32, a33] = a;
  const b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11;
  const b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12, b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30;
  const b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
  const det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
  if (!det) return o;
  const id = 1 / det;
  o[0] = (a11 * b11 - a12 * b10 + a13 * b09) * id; o[1] = (a02 * b10 - a01 * b11 - a03 * b09) * id;
  o[2] = (a31 * b05 - a32 * b04 + a33 * b03) * id; o[3] = (a22 * b04 - a21 * b05 - a23 * b03) * id;
  o[4] = (a12 * b08 - a10 * b11 - a13 * b07) * id; o[5] = (a00 * b11 - a02 * b08 + a03 * b07) * id;
  o[6] = (a32 * b02 - a30 * b05 - a33 * b01) * id; o[7] = (a20 * b05 - a22 * b02 + a23 * b01) * id;
  o[8] = (a10 * b10 - a11 * b08 + a13 * b06) * id; o[9] = (a01 * b08 - a00 * b10 - a03 * b06) * id;
  o[10] = (a30 * b04 - a31 * b02 + a33 * b00) * id; o[11] = (a21 * b02 - a20 * b04 - a23 * b00) * id;
  o[12] = (a11 * b07 - a10 * b09 - a12 * b06) * id; o[13] = (a00 * b09 - a01 * b07 + a02 * b06) * id;
  o[14] = (a31 * b01 - a30 * b03 - a32 * b00) * id; o[15] = (a20 * b03 - a21 * b01 + a22 * b00) * id;
  return o;
}
function perspective(fovY, aspect) {
  const f = 1 / Math.tan(fovY / 2), n = 0.1, far = 100;
  return new Float32Array([f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + n) / (n - far), -1, 0, 0, 2 * far * n / (n - far), 0]);
}
const lin = (hex) => [1, 3, 5].map((i) => Math.pow(parseInt(hex.slice(i, i + 2), 16) / 255, 2.2));
function hsv(h) {
  const f = (n) => { const k = (n + h * 6) % 6; return 1 - Math.max(0, Math.min(k, 4 - k, 1)); };
  return [f(5), f(3), f(1)].map((x) => Math.pow(x, 2.2));
}

/* ---------------- the info board (HUD) ---------------- */

const hudC = document.createElement('canvas'); hudC.width = 768; hudC.height = 320;
const hx = hudC.getContext('2d');
let hudKey = '', hudUntil = 0, hudPinned = false;
function showHud(sec = 4) { hudUntil = performance.now() + sec * 1000; }
function hudLines() {
  const R = run, f = R.f, fr = R.fr, k = fr / f, ex = isExact(f, fr), pg = progOf();
  const L = [];
  if (R.done) return ['Forløbet er slut', 'Tag brillerne af i dit eget tempo.', '', 'Begge greb i 1 sek. afslutter'];
  L.push(`${num(f, f < 10 ? 2 : 1)} Hz · ${band(f)}${R.paused ? '  ·  PAUSE' : ''}`);
  L.push(ex ? `Præcis: ${Math.round(k)} billeder pr. blink (skærm ${fr} Hz)` : `Ikke præcis: ${num(k, 2)} billeder pr. blink – lidt ujævn`);
  L.push(`${waveOf().name} · ${(PATTERNS.find((p) => p.id === S.pattern) || PATTERNS[0]).name}`);
  const col = (COLORS.find((c) => c.id === S.color) || COLORS[0]).name;
  L.push(`${col}${S.eyes === 'skift' ? ' · øjne på skift' : ''}${S.calm ? ' · rolig midte' : ''} · lys ${Math.round(S.level * 100)} %`);
  if (pg.min) { const t = R.progT, m = (x) => `${Math.floor(x / 60)}:${String(Math.floor(x % 60)).padStart(2, '0')}`; L.push(`${pg.name}  ${m(t)} / ${m(pg.min * 60)}`); }
  else L.push(`Takt-lyd: ${{ off: 'fra', tone: 'tone', sus: 'sus' }[S.pulse]} · tabte billeder: ${R.drops}`);
  if (f >= 15 && f <= 25 && !R.paused) L.push('! 15–25 Hz er mest risikabelt for lysfølsomme');
  else if (R.paused) L.push('Aftrækker / knib: fortsæt');
  return L;
}
function drawHud() {
  const L = hudLines(), key = L.join('|');
  if (key === hudKey) return;
  hudKey = key;
  hx.clearRect(0, 0, 768, 320);
  hx.fillStyle = 'rgba(6,8,14,0.86)';
  hx.beginPath(); hx.roundRect(4, 4, 760, 312, 26); hx.fill();
  hx.textAlign = 'center'; hx.textBaseline = 'middle';
  L.forEach((s, i) => {
    hx.font = i === 0 ? '600 52px system-ui, sans-serif' : '30px system-ui, sans-serif';
    hx.fillStyle = s.startsWith('!') ? '#ffb070' : i === 0 ? '#f4f1ea' : '#b9c4d6';
    hx.fillText(s, 384, i === 0 ? 54 : 70 + i * 40);
  });
  gl.bindTexture(gl.TEXTURE_2D, hudTex);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, hudC);
}

/* ---------------- the session ---------------- */

const run = {
  on: false, mode: null, paused: false, done: false, f: S.freq, fr: 60, progT: 0, lastT: 0,
  drops: 0, frames: 0, fade: 0, b: [0, 0], session: null, measured: [], endHold: 0,
};

function currentFreq() {
  const pg = progOf();
  if (!pg.min) return S.freq;
  return progFreq(pg, run.progT / 60);
}
function envelope() {
  const pg = progOf(), t = run.progT;
  let e = S.fadeIn > 0 ? clamp(t / S.fadeIn, 0, 1) : 1;
  e = e * e * (3 - 2 * e);
  if (pg.min) {
    const end = pg.min * 60;
    if (pg.dim) e *= clamp(1 - (t - pg.dim[0] * 60) / ((pg.dim[1] - pg.dim[0]) * 60), 0, 1);
    e *= clamp((end - t) / 20, 0, 1);
    if (t >= end && !run.done) { run.done = true; sound.stop(40); showHud(30); }
  }
  return e;
}

/* One frame for every view. views: [{ invProj, vp: [x, y, w, h], right }] */
function renderFrame(tMs, views, fb) {
  const dtMs = run.lastT ? tMs - run.lastT : 1000 / run.fr;
  run.lastT = tMs;
  run.frames++;
  if (run.frames > 30 && dtMs > 1500 / run.fr) run.drops += Math.max(1, Math.round(dtMs * run.fr / 1000) - 1);
  if (!run.paused && !run.done) run.progT += Math.min(dtMs, 100) / 1000;

  const f = currentFreq();
  run.f = f;
  sound.setFreq(f);
  const env = envelope();
  sound.setLevel(run.done ? 0 : env, 2);

  const tA = sound.audioTimeOf(tMs);
  const ph0 = sound.phaseAt(tA), ph1 = sound.phaseAt(tA + 1 / run.fr);
  const ex = isExact(f, run.fr), wf = waveOf().fn;
  const floor = 1 - S.contrast;

  const pal = COLORS.find((c) => c.id === S.color) || COLORS[0];
  let A, C;
  if (pal.rainbow) { const h = frac(run.progT * 0.01); A = hsv(h); C = hsv(frac(h + 0.45)); }
  else { A = pal._a || (pal._a = lin(pal.a)); C = pal._c || (pal._c = lin(pal.c)); }

  if (hudPinned || run.paused || run.done || tMs < hudUntil) drawHud();
  const hudA = hudPinned || run.paused || run.done ? 1 : clamp((hudUntil - tMs) / 600, 0, 1);

  gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
  gl.useProgram(prog); gl.bindVertexArray(vao);
  gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, hudTex); gl.uniform1i(U.uHud, 0);
  gl.uniform1i(U.uPat, Math.max(0, PATTERNS.findIndex((p) => p.id === S.pattern)));
  gl.uniform3fv(U.uA, A); gl.uniform3fv(U.uC, C);
  gl.uniform1f(U.uLevel, run.done ? 0 : S.level * (run.paused ? 1 : env));
  gl.uniform1f(U.uT, run.progT);
  gl.uniform1f(U.uCalm, S.calm ? 1 : 0);
  gl.uniform1f(U.uHudA, hudA);
  gl.uniform1f(U.uPause, run.paused ? 1 : 0);
  for (const v of views) {
    const off = S.eyes === 'skift' && v.right ? 0.5 : 0;
    const raw = frameLevel(wf, ph0 + off, ph1 + off, ex);
    run.b[v.right ? 1 : 0] = raw;
    gl.viewport(v.vp[0], v.vp[1], v.vp[2], v.vp[3]);
    gl.uniformMatrix4fv(U.uInvProj, false, v.invProj);
    gl.uniform4f(U.uVp, v.vp[0], v.vp[1], v.vp[2], v.vp[3]);
    gl.uniform1f(U.uB, floor + (1 - floor) * raw);
    gl.uniform1f(U.uRaw, floor * 0.5 + (1 - floor) * raw);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
  if (run.hook) run.hook();
}

function resetRun(mode) {
  Object.assign(run, { on: true, mode, paused: false, done: false, progT: 0, lastT: 0, drops: 0, frames: 0, b: [0, 0], endHold: 0 });
  const pg = progOf();
  if (pg.wave && S.wave !== pg.wave) { S.wave = pg.wave; sound.setWave(waveOf().fn); }
  sound.setFreq(currentFreq(), true);
  showHud(6);
}

/* ---------------- changes from controllers and keys ---------------- */

function cycle(list, id, dir) { const i = list.findIndex((x) => x.id === id); return list[(i + dir + list.length) % list.length].id; }
const act = {
  pause() { if (run.done) return; run.paused = !run.paused; sound.setPaused(run.paused); showHud(3); },
  freq(dir) {
    if (progOf().min) { S.freq = +currentFreq().toFixed(2); S.program = 'fri'; }
    const st = stepsFor(run.fr, S.exact);
    let i = st.findIndex((x) => x >= S.freq - 1e-6);
    if (i < 0) i = st.length - 1;
    if (dir > 0) i = st[i] > S.freq + 1e-6 ? i : i + 1; else i = i - 1;
    S.freq = st[clamp(i, 0, st.length - 1)];
    save(); showHud(); syncForm();
  },
  pattern(dir) { S.pattern = cycle(PATTERNS, S.pattern, dir); save(); showHud(); syncForm(); },
  wave() { S.wave = cycle(WAVES, S.wave, 1); sound.setWave(waveOf().fn); save(); showHud(); syncForm(); },
  color() { S.color = cycle(COLORS, S.color, 1); save(); showHud(); syncForm(); },
  eyes() { S.eyes = S.eyes === 'ens' ? 'skift' : 'ens'; save(); showHud(); syncForm(); },
  calm() { S.calm = !S.calm; save(); showHud(); syncForm(); },
  level(dir) { S.level = clamp(Math.round((S.level + dir * 0.1) * 10) / 10, 0.1, 1); save(); showHud(); syncForm(); },
  pulse() { S.pulse = { off: 'tone', tone: 'sus', sus: 'off' }[S.pulse]; sound.setPulse(S.pulse, S.pulseLevel); save(); showHud(); syncForm(); },
  hud() { hudPinned = !hudPinned; showHud(); },
};

/* xr-standard gamepad: 0 trigger, 1 grip, 3 stick press, 4 A/X, 5 B/Y; axes 2, 3 the stick. */
const pads = { left: { b: [], ax: 0, ay: 0, rx: 0, ry: 0 }, right: { b: [], ax: 0, ay: 0, rx: 0, ry: 0 } };
let bothGrip = 0;
function pollPads(session, tMs) {
  let grips = 0;
  for (const src of session.inputSources) {
    const g = src.gamepad, P = pads[src.handedness];
    if (!g || !P) continue;
    const pressed = (i) => !!(g.buttons[i] && g.buttons[i].pressed);
    const edge = (i) => { const p = pressed(i), was = P.b[i]; P.b[i] = p; return p && !was; };
    if (pressed(1)) grips++;
    const x = g.axes[2] || 0, y = g.axes[3] || 0;
    const rep = (val, key, fn) => {
      const on = Math.abs(val) > 0.7, k = key + 't';
      if (!on) { P[k] = 0; return; }
      if (!P[k] || tMs > P[k]) { fn(val > 0 ? 1 : -1); P[k] = tMs + (P[k] ? 220 : 420); }
    };
    if (src.handedness === 'right') {
      rep(-y, 'ry', (d) => act.freq(d));
      rep(x, 'rx', (d) => act.pattern(d));
      if (edge(4)) act.wave();
      if (edge(5)) act.color();
      if (edge(3)) act.eyes();
    } else {
      rep(-y, 'ly', (d) => act.level(d));
      rep(x, 'lx', () => act.eyes());
      if (edge(4)) act.pulse();
      if (edge(5)) act.hud();
      if (edge(3)) act.calm();
    }
  }
  if (grips >= 2) { if (!bothGrip) bothGrip = tMs; else if (tMs - bothGrip > 1000) { bothGrip = 0; session.end(); } }
  else bothGrip = 0;
}

/* ---------------- VR ---------------- */

function failVR(session, msg) {
  run.failed = true;
  setStatus(msg + ' Skriv beskeden til Claude.', true);
  try { session.end(); } catch (_) { /* already over */ }
}

async function enterVR() {
  run.failed = false;
  const sp = sound.start();
  sound.setPulse(S.pulse, S.pulseLevel);
  let session;
  try {
    session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local'] });
  } catch (e) { setStatus('Kunne ikke starte VR: ' + e.message, true); sound.stop(); return; }
  sp.catch(() => {});   /* sound may still be starting; never hold the first frame for it */
  /* If no frame arrives, or drawing fails, end the session and say why on
     the page instead of leaving the headset waiting. */
  let firstFrame = false, errors = 0;
  setTimeout(() => { if (!firstFrame && !run.failed) failVR(session, 'Brillerne sendte ingen billeder til appen inden for 8 sekunder.'); }, 8000);
  let layer, ref;
  try {
    await gl.makeXRCompatible();
    layer = new XRWebGLLayer(session, gl, { antialias: false, depth: false, framebufferScaleFactor: 0.8 });
    if ('fixedFoveation' in layer) layer.fixedFoveation = 0;
    session.updateRenderState({ baseLayer: layer });
    ref = await session.requestReferenceSpace('viewer');
  } catch (e) { failVR(session, 'VR kunne ikke sættes op: ' + e.message); return; }
  run.fr = session.frameRate || 72;
  run.session = session;
  resetRun('vr');
  session.addEventListener('select', () => act.pause());
  session.addEventListener('end', () => { run.on = false; run.session = null; sound.stop(); if (!run.failed) setStatus('VR er afsluttet.'); });
  session.addEventListener('frameratechange', () => { run.fr = session.frameRate || run.fr; showHud(); });
  /* Ask for 120 Hz without waiting for the answer: on a real Quest the
     promise only settles once frames are being drawn, so awaiting it before
     the first frame waits forever (the headset shows its loading stars). */
  if (session.supportedFrameRates && session.updateTargetFrameRate) {
    const rates = Array.from(session.supportedFrameRates);
    const want = rates.includes(S.rate) ? S.rate : Math.max(...rates.filter((r) => r <= 120));
    session.updateTargetFrameRate(want).then(() => { run.fr = session.frameRate || want; showHud(); }, () => {});
  }
  const views = [];
  const onXR = (t, frame) => {
    if (!run.session) return;
    session.requestAnimationFrame(onXR);
    firstFrame = true;
    try {
      pollPads(session, t);
      const pose = frame.getViewerPose(ref);
      if (!pose) return;
      views.length = 0;
      for (const v of pose.views) {
        const vp = layer.getViewport(v);
        views.push({ invProj: invert(v.projectionMatrix), vp: [vp.x, vp.y, vp.width, vp.height], right: v.eye === 'right' });
      }
      renderFrame(t, views, layer.framebuffer);
      errors = 0;
    } catch (e) {
      if (++errors >= 3) failVR(session, 'Fejl under tegningen: ' + e.message);
    }
  };
  session.requestAnimationFrame(onXR);
}

/* ---------------- flat screen ---------------- */

const flatKeys = {
  ' ': () => act.pause(), ArrowUp: () => act.freq(1), ArrowDown: () => act.freq(-1),
  ArrowRight: () => act.pattern(1), ArrowLeft: () => act.pattern(-1), w: () => act.wave(), c: () => act.color(),
  e: () => act.eyes(), m: () => act.calm(), '+': () => act.level(1), '-': () => act.level(-1), t: () => act.pulse(),
  i: () => act.hud(), Escape: () => stopFlat(),
};
function measureRate(dt) {
  const m = run.measured; m.push(dt); if (m.length > 90) m.shift();
  if (m.length < 30) return;
  const s = m.slice().sort((a, b) => a - b), med = s[s.length >> 1], hz = 1000 / med;
  const common = [30, 50, 60, 72, 75, 90, 100, 120, 144, 165, 240];
  run.fr = common.reduce((a, b) => (Math.abs(b - hz) < Math.abs(a - hz) ? b : a));
}
let flatRaf = 0;
function startFlat(silent) {
  if (!silent) { sound.start(); sound.setPulse(S.pulse, S.pulseLevel); }
  document.body.classList.add('flat');
  resetRun('flat');
  run.fr = 60; run.measured = [];
  let last = 0;
  const loop = (t) => {
    if (!run.on) return;
    flatRaf = requestAnimationFrame(loop);
    if (last) measureRate(t - last);
    last = t;
    const w = canvas.clientWidth * Math.min(devicePixelRatio, 2) | 0, h = canvas.clientHeight * Math.min(devicePixelRatio, 2) | 0;
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    renderFrame(t, [{ invProj: invert(perspective(1.6, w / h)), vp: [0, 0, w, h], right: false }], null);
  };
  flatRaf = requestAnimationFrame(loop);
}
function stopFlat() {
  if (run.mode !== 'flat') return;
  run.on = false; cancelAnimationFrame(flatRaf);
  document.body.classList.remove('flat');
  sound.stop();
  if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
}
addEventListener('keydown', (e) => {
  if (!run.on || run.mode !== 'flat') return;
  const fn = flatKeys[e.key] || flatKeys[e.key.toLowerCase()];
  if (fn) { fn(); e.preventDefault(); }
});

/* ---------------- the start page ---------------- */

function setStatus(msg, bad) { const s = $('status'); s.textContent = msg; s.classList.toggle('bad', !!bad); }
function chips(el, list, key, after) {
  el.innerHTML = '';
  for (const it of list) {
    const b = document.createElement('button'); b.type = 'button'; b.textContent = it.name; b.dataset.id = it.id;
    if (it.desc) b.title = it.desc;
    b.onclick = () => { S[key] = it.id; save(); after && after(it); syncForm(); };
    el.appendChild(b);
  }
}
function syncForm() {
  for (const [el, key] of [['waveChips', 'wave'], ['patChips', 'pattern'], ['colChips', 'color'], ['progChips', 'program'], ['eyeChips', 'eyes'], ['pulseChips', 'pulse'], ['soundChips', 'sound'], ['rateChips', 'rate']]) {
    for (const b of $(el).children) b.classList.toggle('on', b.dataset.id === String(S[key]));
  }
  $('freq').value = S.freq; $('freqNum').value = S.freq;
  $('exact').checked = S.exact; $('calm').checked = S.calm;
  $('level').value = Math.round(S.level * 100); $('levelVal').textContent = Math.round(S.level * 100) + ' %';
  $('contrast').value = Math.round(S.contrast * 100); $('contrastVal').textContent = Math.round(S.contrast * 100) + ' %';
  $('volume').value = Math.round(S.volume * 100); $('volumeVal').textContent = Math.round(S.volume * 100) + ' %';
  $('pulseLevel').value = Math.round(S.pulseLevel * 100); $('pulseLevelVal').textContent = Math.round(S.pulseLevel * 100) + ' %';
  $('accept').checked = !!S.accepted;
  const f = S.freq, r = S.rate, k = r / f;
  $('freqInfo').innerHTML = `<b>${num(f, f < 10 ? 2 : 1)} Hz · ${band(f)}</b> — ${isExact(f, r) ? `præcis ved ${r} Hz: ${Math.round(k)} billeder pr. blink` : `ikke præcis ved ${r} Hz (${num(k, 2)} billeder pr. blink), blinket bliver lidt ujævnt`}` +
    (f >= 15 && f <= 25 ? '<br><span class="warnTxt">15–25 Hz er det område, hvor lysfølsomme lettest får anfald.</span>' : '');
  const pg = progOf();
  $('progDesc').textContent = pg.desc;
  $('freqRow').classList.toggle('dim', !!pg.min);
  const ps = (sound.presets() || []).find((p) => p.id === S.sound);
  $('soundDesc').textContent = ps ? ps.desc : 'Ingen baggrundslyd.';
  $('vrBtn').disabled = !S.accepted; $('flatBtn').disabled = !S.accepted;
}
function prepSound() {
  if (S.sound === 'off') { sound.setPreset('off'); setStatus(''); return; }
  const id = S.sound;
  setStatus('Forbereder lyd …');
  sound.setPreset(id, (d, n) => { if (S.sound === id) setStatus(d < n ? `Forbereder lyd ${d + 1}/${n} …` : 'Lyden er klar.'); })
    .catch((e) => setStatus('Lyden kunne ikke laves: ' + e.message, true));
}

function initPage() {
  chips($('waveChips'), WAVES, 'wave', () => sound.setWave(waveOf().fn));
  chips($('patChips'), PATTERNS, 'pattern');
  chips($('colChips'), COLORS, 'color');
  chips($('progChips'), PROGRAMS, 'program');
  chips($('eyeChips'), [{ id: 'ens', name: 'Begge øjne ens' }, { id: 'skift', name: 'Øjne på skift' }], 'eyes');
  chips($('pulseChips'), [{ id: 'off', name: 'Fra' }, { id: 'tone', name: 'Tone' }, { id: 'sus', name: 'Sus' }], 'pulse');
  chips($('rateChips'), [{ id: '120', name: '120 Hz' }, { id: '90', name: '90 Hz' }, { id: '72', name: '72 Hz' }], 'rate');
  for (const b of $('rateChips').children) b.onclick = () => { S.rate = +b.dataset.id; if (S.exact) S.freq = +nearestExact(S.freq, S.rate).toFixed(3); save(); syncForm(); };
  const presets = [{ id: 'off', name: 'Ingen' }].concat((sound.presets() || []).map((p) => ({ id: p.id, name: p.name, desc: p.desc })));
  chips($('soundChips'), presets, 'sound', prepSound);
  $('freqPresets').querySelectorAll('button').forEach((b) => { b.onclick = () => { S.freq = +b.dataset.f; S.program = 'fri'; save(); syncForm(); }; });
  const setF = (v) => { v = clamp(+v || 10, 1, 60); if (S.exact) v = +nearestExact(v, S.rate).toFixed(3); S.freq = v; sound.setFreq(v, true); save(); syncForm(); };
  $('freq').oninput = (e) => { S.freq = +e.target.value; $('freqNum').value = S.freq; syncForm(); };
  $('freq').onchange = (e) => setF(e.target.value);
  $('freqNum').onchange = (e) => setF(e.target.value);
  $('exact').onchange = (e) => { S.exact = e.target.checked; setF(S.freq); };
  $('calm').onchange = (e) => { S.calm = e.target.checked; save(); };
  const slider = (id, key, fn) => { $(id).oninput = (e) => { S[key] = e.target.value / 100; fn && fn(); save(); syncForm(); }; };
  slider('level', 'level'); slider('contrast', 'contrast');
  slider('volume', 'volume', () => sound.setVolume(S.volume));
  slider('pulseLevel', 'pulseLevel', () => sound.setPulse(S.pulse, S.pulseLevel));
  $('pulseChips').addEventListener('click', () => sound.setPulse(S.pulse, S.pulseLevel));
  $('accept').onchange = (e) => { S.accepted = e.target.checked; save(); syncForm(); };
  $('flatBtn').onclick = () => { const r = canvas.requestFullscreen && canvas.requestFullscreen(); if (r && r.catch) r.catch(() => {}); startFlat(); };
  $('flatExit').onclick = () => stopFlat();
  sound.setVolume(S.volume);
  syncForm();
  prepSound();
  (async () => {
    let ok = false;
    try { ok = !!(navigator.xr && await navigator.xr.isSessionSupported('immersive-vr')); } catch (_) { /* no WebXR */ }
    $('vrBtn').hidden = !ok; $('noVr').hidden = ok;
    $('vrBtn').onclick = () => enterVR();
  })();
}

try { initGL(); } catch (e) { setStatus('WebGL2 virker ikke her: ' + e.message, true); }
initPage();
if (Q.has('flat')) startFlat(true);

/* Test hooks. */
window.__lys = { S, run, act, sound, levelsFor, isExact, frameLevel, WAVES, PATTERNS, COLORS, PROGRAMS, startFlat, stopFlat, syncForm, showHud };
