/* Journey to the Moon, chapter 5: the descent.
 *
 * GET 110:04:53 to the landing at 110:21:58, standing at the Commander's station of the LM (lm-cabin.js). Five
 * minutes of coast at 15 km, then powered descent: braking with the windows up (black sky and the Earth), the pitch-over
 * at high gate (the valley swings into the windows), the approach, and from 150 m P66: the autopilot lands you
 * safely, or press A and fly the last of it yourself (right stick: tilt; left stick flicked up or down: the descent
 * rate, 1 ft/s a flick; A again: back to the computer). A contact too hard, or the fuel gone, by hand: back to high
 * gate. Dust from 30 m, the LM's shadow running in towards you over the ground, contact light, engine stop.
 *
 * The flight: descent-space.js (stage 0's site frame); the Moon: stage 0's renderer as on the Moon page, in one pass
 * from high up and in its two surface passes below 3.5 km, with the roaming ground and the rocks near the site.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=) */
import * as THREE from 'three';
import { loadMoon } from './moon.js?v=3';
import { makeSky, makeRocks } from './extras.js?v=4';
import { makeGround } from './ground.js?v=4';
import { GETL } from './lunar-space.js?v=13';
import { makeDescent, GETD, PAD_DROP } from './descent-space.js?v=13';
import { makeLMCabin, LMC } from './lm-cabin.js?v=13';
import { makeLaunchSound } from './launch-sound.js?v=13';
import { makeRadio } from './radio.js?v=13';
import { makeVehicle } from './vehicle.js?v=13';
import { createHand } from './hands.js?v=13';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;

const START = GETD.pdi - 300;
const W_EARTH = 7.2921159e-5;
const RATE_AUTO = 2.5 * Math.PI / 180, RATE_HAND = 8 * Math.PI / 180;
const FT = 0.3048;
const FUEL_AT_LG = 215;                    /* seconds of hover at low gate: about 2 min left at the autopilot's contact */
const SAFE_V = 3.0, SAFE_H = 1.5;          /* the gear's limits at contact, m/s */

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;
renderer.xr.setReferenceSpaceType('local-floor');
$('stage').appendChild(renderer.domElement);
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, innerWidth / innerHeight, 0.02, 100);
const rig = new THREE.Group(), head = new THREE.Group();
rig.add(head); head.add(camera); scene.add(rig);
addEventListener('resize', () => { if (renderer.xr.isPresenting) return; camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); });

const S = { t: num('t', START), running: false, warp: 1, still: Q.has('still'), yaw: num('yaw', 0), pitch: num('pitch', 0),
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, ended: false,
  mode: 'table', live: null, rod: -1, tilt: [0, 0], fuel: FUEL_AT_LG, landed: -1, cues: {}, took: false, contactV: null, restarts: 0 };

/* ---------------------------------------------------------------- the LM, inside and out */
const vehicle = makeVehicle(renderer);
for (const k of ['sic', 'skirt', 'sii', 'sivb', 'csm', 'les', 'cm']) if (vehicle.parts[k]) vehicle.parts[k].visible = false;
vehicle.lm.userData.setLegs(true);
const vSun = vehicle.lights.children.find(l => l.isDirectionalLight), vHemi = vehicle.lights.children.find(l => l.isHemisphereLight);
const lmc = makeLMCabin(); lmc.setHatchOpen(false);
lmc.group.matrixAutoUpdate = false; scene.add(lmc.group);
const nearG = new THREE.Group(); scene.add(nearG);
nearG.add(vehicle.group, vehicle.lights);
const OUT = { want: false, on: false, az: 0.9, el: 0.3, dist: 30 };
const sound = makeLaunchSound();
let radio = null, moon = null, sky = null, D = null, ground = null, rocks = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0, then: null };

/* ---------------------------------------------------------------- the LM's shadow on the ground, and the dust */
/* the shadow: the LM's silhouette seen from the low Sun, laid along the ground away from it; a grid draped over the
 * terrain each frame. Texture: legs and body as the Sun sees them (u across, v from the feet up) */
const shadowTex = (() => {
  const c = document.createElement('canvas'); c.width = 256; c.height = 256; const x = c.getContext('2d');
  x.clearRect(0, 0, 256, 256); x.fillStyle = '#fff'; x.strokeStyle = '#fff';
  const X = u => 128 + u * 26, Y = v => 256 - v * 34;                    /* metres: u across (-4.7..4.7), v up (0..7) */
  x.fillRect(X(-2.1), Y(2.45), X(2.1) - X(-2.1), Y(0.8) - Y(2.45));       /* descent stage */
  x.beginPath(); x.moveTo(X(-1.25), Y(2.45)); x.lineTo(X(-1.4), Y(5.2)); x.lineTo(X(-0.6), Y(5.9)); x.lineTo(X(0.6), Y(5.9)); x.lineTo(X(1.4), Y(5.2)); x.lineTo(X(1.25), Y(2.45)); x.fill();
  x.fillRect(X(-0.4), Y(6.3), X(0.4) - X(-0.4), Y(5.85) - Y(6.3));        /* tunnel */
  x.lineWidth = 6; for (const s of [-1, 1]) { x.beginPath(); x.moveTo(X(s * 2.0), Y(2.3)); x.lineTo(X(s * 4.15), Y(0.15)); x.stroke(); x.beginPath(); x.moveTo(X(s * 1.6), Y(0.9)); x.lineTo(X(s * 3.2), Y(1.1)); x.stroke(); }
  x.lineWidth = 4; x.beginPath(); x.moveTo(X(0), Y(0.8)); x.lineTo(X(0), Y(0.15)); x.stroke();
  for (const u of [-4.15, 4.15, 0]) { x.beginPath(); x.ellipse(X(u), Y(0.1), 12, 5, 0, 0, Math.PI * 2); x.fill(); }
  const t = new THREE.CanvasTexture(c); return t;
})();
const SH = { nx: 8, ny: 24 };
const shadowGeo = new THREE.PlaneGeometry(1, 1, SH.nx, SH.ny);
const shadow = new THREE.Mesh(shadowGeo, new THREE.MeshBasicMaterial({ color: 0x000000, alphaMap: shadowTex, transparent: true, opacity: 0.9, depthWrite: false,
  polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -8 }));
shadow.frustumCulled = false; shadow.renderOrder = 20; shadow.visible = false; nearG.add(shadow);
/* the dust: sheets racing outwards over the ground from under the engine */
const dustU = { uTime: { value: 0 }, uK: { value: 0 }, uR: { value: 70 } };
const dust = new THREE.Mesh(new THREE.CircleGeometry(1, 64, 0, Math.PI * 2), new THREE.ShaderMaterial({
  uniforms: dustU, transparent: true, depthWrite: false,
  vertexShader: `varying vec2 vP; void main(){ vP = position.xy; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
  fragmentShader: `uniform float uTime; uniform float uK; varying vec2 vP;
    float h(float n){ return fract(sin(n) * 43758.5453); }
    void main(){ float r = length(vP), a = atan(vP.y, vP.x);
      float s = floor(a * 40.0 / 6.2831853); float ray = 0.55 + 0.45 * h(s) ;
      float flow = fract(r * 3.0 - uTime * 2.2 * (0.7 + 0.6 * h(s + 7.0)));
      float streak = smoothstep(0.0, 0.15, flow) * smoothstep(1.0, 0.4, flow) * ray;
      float fall = smoothstep(1.0, 0.25, r) * smoothstep(0.0, 0.06, r);
      gl_FragColor = vec4(vec3(0.62, 0.6, 0.56), uK * fall * (0.25 + 0.55 * streak)); }` }));
dust.frustumCulled = false; dust.renderOrder = 21; dust.visible = false; nearG.add(dust);

/* ---------------------------------------------------------------- HUD */
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
function prog(t) {
  if (S.landed >= 0) return 'landed';
  if (S.mode === 'hand') return 'P66 by hand';
  if (t < GETD.pdi) return 'coasting to PDI';
  if (t < GETD.hg) return 'P63 braking';
  if (t < GETD.lg) return 'P64 approach';
  return 'P66 auto';
}
let cur = null, padH = 15000;
const hud = (() => {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.08), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false }));
  mesh.renderOrder = 999;
  let last = 0;
  return { mesh, on: true, draw(now) {
    if (now - last < 250 || !cur) return; last = now;
    const up = D.upAt(cur.r), vu = cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2], vh = Math.sqrt(Math.max(0, cur.v[0] ** 2 + cur.v[1] ** 2 + cur.v[2] ** 2 - vu * vu));
    g.clearRect(0, 0, 512, 256); g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(0, 0, 512, 256);
    g.fillStyle = S.fps > 68 ? '#9fe6a0' : S.fps > 55 ? '#f0d070' : '#ff8a70'; g.font = 'bold 40px monospace'; g.fillText(S.fps.toFixed(0) + ' fps', 20, 46);
    g.fillStyle = '#d8d8d8'; g.font = '26px monospace';
    g.fillText('GET ' + fmt(S.t) + '  ' + prog(S.t), 20, 86);
    g.fillText('ALT ' + Math.max(0, padH / FT).toFixed(0) + ' ft  DOWN ' + (-vu / FT).toFixed(1), 20, 122);
    g.fillText('FWD ' + (vh / FT).toFixed(0) + ' ft/s' + (S.t > GETD.lg && S.landed < 0 ? '  FUEL ' + Math.max(0, S.fuel).toFixed(0) + ' s' : ''), 20, 158);
    if (S.ended) { g.fillStyle = '#f0d070'; g.fillText('End of chapter 5.  A: walk outside', 20, 196); }
    else if (S.mode === 'hand') { g.fillStyle = '#f0d070'; g.fillText('rate ' + (-S.rod / FT).toFixed(0) + ' ft/s  A: computer', 20, 196); }
    else if (S.landed < 0 && padH < 150 && S.t > GETD.lg - 20) { g.fillStyle = '#f0d070'; g.fillText('A: take over (P66)', 20, 196); }
    g.fillStyle = '#8a8a8a'; g.font = '18px monospace';
    g.fillText(S.mode === 'hand' ? 'R stick: tilt   L stick up/down: rate' : 'A/X time (x10 before high gate)  hold B outside', 12, 236);
    tex.needsUpdate = true;
  } };
})();

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyT') toggleHand();
  if (e.code === 'KeyN' && S.ended) nextChapter('index.html');
  if (e.code === 'BracketLeft' && S.mode === 'hand') S.rod -= FT;
  if (e.code === 'BracketRight' && S.mode === 'hand') S.rod = Math.min(0.5, S.rod + FT);
  const jump = { Digit1: START, Digit2: GETD.pdi - 20, Digit3: GETD.hg - 30, Digit4: GETD.lg - 30 }[e.code];
  if (jump !== undefined) restart(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const warpOk = t => t < GETD.hg - 60 && S.mode === 'table';
function cycleWarp() { if (!warpOk(S.t)) { S.warp = 1; return; } S.warp = S.warp === 1 ? 2 : S.warp === 2 ? 4 : S.warp === 4 ? 10 : 1; }
function nextChapter(href) { const s = renderer.xr.getSession(), go = () => { location.href = href; }; if (s) s.end().then(go, go); else go(); }
/* back to a point on the autopilot's flight: everything reset */
function restart(t) {
  S.t = t; S.warp = 1; S.ended = false; S.mode = 'table'; S.live = null; S.landed = -1; S.cues = {}; S.took = false; S.contactV = null;
  S.fuel = FUEL_AT_LG; S.rod = -1; S.tilt = [0, 0]; snap = true;
}
function toggleHand() {
  if (S.landed >= 0 || !cur) return;
  if (S.mode === 'hand') { S.mode = 'live'; if (radio) radio.cue('auto'); return; }
  if (padH > 160 || S.t < GETD.lg - 30) return;                        /* offered below about 150 m */
  S.mode = 'hand'; S.took = true; S.live = S.live || { ...cur };
  const up = D.upAt(cur.r); S.rod = Math.max(-3, Math.min(-0.3, cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2])); S.tilt = [0, 0];
  if (radio) radio.cue('manual');
}
const STICK = { lx: 0, ly: 0, rx: 0, ry: 0 };
const pad = { prev: {} };
function readControllers() {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed;
    if (a && !pad.prev[k + 'a']) { if (S.ended) nextChapter('index.html'); else if (S.t > GETD.lg - 30 && S.landed < 0) toggleHand(); else cycleWarp(); }
    if (k === 'right') OUT.want = !!b;
    if (st && !pad.prev[k + 's']) { if (k === 'left') hud.on = !hud.on; else SEAT.pending = 1; }
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') {
      /* the rate-of-descent switch: a flick up or down is one click, 1 ft/s */
      const was = pad.prev.ly || 0; STICK.lx = ax[0]; STICK.ly = ax[1];
      if (S.mode === 'hand') { if (ax[1] < -0.7 && was >= -0.7) S.rod = Math.min(0.5, S.rod + FT); if (ax[1] > 0.7 && was <= 0.7) S.rod -= FT; }
      pad.prev.ly = ax[1];
    } else { STICK.rx = ax[0]; STICK.ry = ax[1]; }
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b; pad.prev[k + 's'] = st;
  }
}
/* standing at your station (only turned and moved over the floor; your own height stays) */
const SEAT = { pending: 0, done: false };
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), 0, -(-s * p.x + c * p.z));
  SEAT.done = true;
}

/* ---------------------------------------------------------------- the flight */
const V3 = a => new THREE.Vector3().fromArray(a);
const _y = new THREE.Vector3(), _z = new THREE.Vector3(), _x = new THREE.Vector3(), _u = new THREE.Vector3(), _f = new THREE.Vector3(), _n = new THREE.Vector3();
const _m = new THREE.Matrix4(), _m2 = new THREE.Matrix4(), _s = new THREE.Vector3(), _q = new THREE.Quaternion();
const lmAtt = new THREE.Quaternion(), attWant = new THREE.Quaternion(), LMworld = new THREE.Matrix4();
let snap = true, thrustY = new THREE.Vector3(0, 1, 0);
function step(dt) {
  if (S.mode === 'table') { cur = D.state(S.t); return; }
  /* live: by the computer's guidance from here, or by hand */
  const n = Math.max(1, Math.ceil(dt / 0.02)), h = dt / n;
  for (let i = 0; i < n; i++) {
    let s = S.live;
    if (S.mode === 'hand') {
      _u.fromArray(D.upAt(s.r)); _n.fromArray(D.U0).cross(_f.fromArray(D.F0)); _f.crossVectors(_n, _u).normalize();
      s = D.manual(s, h, S.rod, S.tilt, _f.toArray());
    } else {
      const a = D.guide(s.r, s.v, s.t, null), up = D.upAt(s.r), gm = 1.62;
      const acc = [a[0] - up[0] * gm, a[1] - up[1] * gm, a[2] - up[2] * gm];
      const v2 = [s.v[0] + acc[0] * h, s.v[1] + acc[1] * h, s.v[2] + acc[2] * h];
      s = { t: s.t + h, r: [s.r[0] + v2[0] * h, s.r[1] + v2[1] * h, s.r[2] + v2[2] * h], v: v2, a };
    }
    S.live = s;
  }
  cur = S.live;
}
/* the LM's attitude: its +y along the thrust; windows up while braking (towards the sky), turning ahead as the
 * thrust comes up to vertical, which is the pitch-over */
function attitude(dt) {
  const am = Math.hypot(cur.a[0], cur.a[1], cur.a[2]);
  _u.fromArray(D.upAt(cur.r));
  _n.fromArray(D.U0).cross(_f.fromArray(D.F0)).normalize(); _f.crossVectors(_n, _u).normalize();      /* ahead along the track */
  if (S.t < GETD.pdi) thrustY.fromArray(cur.v).normalize().negate();                                    /* ignition attitude */
  else if (am > 0.3) thrustY.fromArray(cur.a).normalize();
  _y.copy(thrustY);
  _z.copy(_u).addScaledVector(_y, -_u.dot(_y)).add(_x.copy(_f).addScaledVector(_y, -_f.dot(_y))).normalize();
  _x.crossVectors(_y, _z).normalize(); _z.crossVectors(_x, _y);
  attWant.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
  if (snap) { lmAtt.copy(attWant); snap = false; } else lmAtt.rotateTowards(attWant, (S.mode === 'hand' ? RATE_HAND : RATE_AUTO) * dt);
  LMworld.compose(_s.fromArray(cur.r), lmAtt, _x.set(1, 1, 1));
  lmc.group.matrix.copy(LMworld);
  vehicle.group.position.fromArray(cur.r); vehicle.group.quaternion.copy(lmAtt);
  vehicle.lights.position.fromArray(cur.r);
}
const standM = new THREE.Matrix4();
function placeRig() {
  if (OUT.on) return placeOutside();
  standM.copy(LMworld).multiply(_m.makeTranslation(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z)).multiply(_m2.makeRotationY(Math.PI));
  standM.decompose(rig.position, rig.quaternion, _s);
}
const _tg = new THREE.Vector3(), _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3();
function placeOutside() {
  _tg.set(0, 3, 0).applyMatrix4(LMworld);
  _u.fromArray(D.upAt(cur.r)); _h1.fromArray(D.F0).addScaledVector(_u, -_h1.fromArray(D.F0).dot(_u)).normalize(); _h2.crossVectors(_u, _h1);
  rig.position.copy(_h1).multiplyScalar(Math.cos(OUT.az)).addScaledVector(_h2, Math.sin(OUT.az)).multiplyScalar(Math.cos(OUT.el)).addScaledVector(_u, Math.sin(OUT.el)).multiplyScalar(OUT.dist).add(_tg);
  _z.copy(rig.position).sub(_tg).normalize(); _x.crossVectors(_u, _z).normalize(); _y.crossVectors(_z, _x);
  rig.quaternion.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
function updateOutside(dt) {
  if (OUT.want !== OUT.on) { FADE.target = 1; if (FADE.k > 0.9) { OUT.on = OUT.want; FADE.target = 0; } }
  else if (FADE.target === 1 && FADE.k > 0.9 && !FADE.then) FADE.target = 0;
  if (!OUT.on) return;
  let ax = 0, ay = 0, ey = 0;
  if (renderer.xr.isPresenting) { ax = STICK.lx; ay = STICK.ly; ey = STICK.ry; }
  else { ax = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); ey = (keys.KeyI ? -1 : 0) + (keys.KeyK ? 1 : 0); ay = (keys.KeyM ? 1 : 0) - (keys.KeyU ? 1 : 0); }
  const dz = v => Math.abs(v) < 0.15 ? 0 : v;
  OUT.az += dz(ax) * 1.1 * dt; OUT.dist = Math.min(600, Math.max(9, OUT.dist * Math.exp(dz(ay) * 1.4 * dt))); OUT.el = Math.min(1.45, Math.max(-0.2, OUT.el - dz(ey) * 0.9 * dt));
}
/* the shadow and the dust under the LM */
const _sun = new THREE.Vector3(), _sh = new THREE.Vector3(), _sx = new THREE.Vector3(), _p = new THREE.Vector3();
function ground3(x, z, out) { return out.set(x, moon.heightAt(x, z), z); }
function updateShadow(now) {
  const near = padH < 900 && Math.hypot(cur.r[0], cur.r[2]) < 20000;
  shadow.visible = near && !!moon; dust.visible = near && padH < 32 && S.landed < 0;
  if (!near) return;
  const gx = cur.r[0], gz = cur.r[2], gy = moon.heightAt(gx, gz);
  const tanEl = Math.max(0.05, _sun.y / Math.hypot(_sun.x, _sun.z));
  _sh.set(-_sun.x, 0, -_sun.z).normalize();                       /* along the ground, away from the Sun */
  _sx.set(-_sh.z, 0, _sh.x);
  const base = (cur.r[1] - PAD_DROP - gy) / tanEl;                 /* the feet's shadow, this far out */
  const len = 7.0 / tanEl, half = 4.9;
  const pos = shadowGeo.attributes.position;
  for (let j = 0; j <= SH.ny; j++) for (let i = 0; i <= SH.nx; i++) {
    const k = j * (SH.nx + 1) + i, u = (i / SH.nx - 0.5) * 2 * half, v = base + (1 - j / SH.ny) * len;
    const x = gx + _sh.x * v + _sx.x * u, z = gz + _sh.z * v + _sx.z * u;
    pos.setXYZ(k, x, moon.heightAt(x, z) + 0.05, z);
  }
  pos.needsUpdate = true; shadowGeo.computeBoundingSphere();
  shadow.material.opacity = 0.88 * Math.min(1, (900 - padH) / 300);
  if (dust.visible) {
    dust.position.set(gx, gy + 0.3, gz); dust.rotation.set(-Math.PI / 2, 0, 0); dust.scale.setScalar(70);
    dustU.uTime.value = now / 1000; dustU.uK.value = Math.min(1, (32 - padH) / 22);
  }
}

/* ---------------------------------------------------------------- hands */
const hands = { left: createHand('left'), right: createHand('right') };
function updateHands(dt) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (let i = 0; i < 2; i++) {
    const src = s.inputSources[i]; if (!src || !src.gamepad) continue;
    const m = hands[src.handedness]; if (!m) continue;
    const grip = renderer.xr.getControllerGrip(i); if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
  }
}

/* ---------------------------------------------------------------- rendering */
function setPlanes(n, f) {
  const fix = c => { const e = c.projectionMatrix.elements; e[10] = -(f + n) / (f - n); e[14] = -2 * f * n / (f - n); c.projectionMatrixInverse.copy(c.projectionMatrix).invert(); };
  if (renderer.xr.isPresenting) { const xc = renderer.xr.getCamera(); fix(xc); xc.cameras.forEach(fix); }
  else { camera.near = n; camera.far = f; camera.updateProjectionMatrix(); }
}
const _cam = new THREE.Vector3(), _cc = new THREE.Vector3();
function render() {
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  _cam.setFromMatrixPosition(camera.matrixWorld);
  const low = padH < 3500 && Math.hypot(_cam.x, _cam.z) < 60000;
  moon.setDetail(_cam, false);
  if (low) { rocks.update(_cam); }
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, s, mf, mn, n) => {
    lmc.group.visible = c && !OUT.on; hud.mesh.visible = c && hud.on;
    sky.visible = s; moon.far.visible = mf; moon.near.visible = mn;
    ground.group.visible = n && low && padH < 2000; rocks.group.visible = n && low && padH < 1200;
    nearG.visible = n; vehicle.lm.visible = OUT.on;
  };
  show(false, true, false, false, false); setPlanes(1e5, 3e6); renderer.render(scene, camera);
  renderer.clearDepth();
  const d = _cam.distanceTo(_cc.fromArray(D.C));
  if (!low) {
    show(false, false, true, true, false); setPlanes(Math.max(50, (d - D.R - 6000) * 0.25), d + D.R); renderer.render(scene, camera);
    renderer.clearDepth();
    show(false, false, false, false, true); setPlanes(0.3, 40000); renderer.render(scene, camera);
  } else {
    /* as standing on the Moon: the distant terrain, then everything within 40 km over a fresh depth buffer */
    show(false, false, true, false, false); setPlanes(Math.max(100, padH * 0.5), 3e6); renderer.render(scene, camera);
    renderer.clearDepth();
    show(false, false, false, true, true); setPlanes(0.3, 40000); renderer.render(scene, camera);
  }
  renderer.clearDepth();
  show(true, false, false, false, false); setPlanes(0.02, 14); renderer.render(scene, camera);
  if (FADE.k > 0.001) { veil.material.opacity = FADE.k; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

/* ---------------------------------------------------------------- the frame */
const CALLS = [['h500', 500], ['h300', 300], ['h200', 200], ['h100', 100], ['h50', 50], ['h30', 30], ['h10', 10]];
const cue = k => { if (S.cues[k]) return; S.cues[k] = true; if (radio) radio.cue(k); };
let lastT = 0, acc = 0, accN = 0, lastDsky = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  if (xr) readControllers();
  /* by hand: the right stick tilts the LM (up to 20 degrees) */
  if (S.mode === 'hand') {
    let fx = 0, sx = 0;
    if (xr) { fx = -STICK.ry; sx = STICK.rx; } else { fx = (keys.ArrowUp ? 1 : 0) - (keys.ArrowDown ? 1 : 0); sx = (keys.ArrowRight ? 1 : 0) - (keys.ArrowLeft ? 1 : 0); }
    const dz = v => Math.abs(v) < 0.12 ? 0 : v;
    S.tilt = [dz(fx) * 0.35, dz(sx) * 0.35];
  }
  /* the clock */
  if (S.running && !S.still && !S.ended && S.landed < 0) {
    if (!warpOk(S.t)) S.warp = 1;
    const ddt = dt * S.warp;
    if (S.mode === 'table') { S.t += ddt; if (D.contact() && S.t >= D.contact()) S.t = D.contact(); }
    else { step(ddt); S.t = cur.t; }
  } else if (S.landed >= 0 && S.running && !S.still) S.t += dt;
  if (S.mode === 'table' && S.landed < 0) step(0);                      /* on the ground: stay put */
  padH = D.padHeight(cur);
  /* fuel, from low gate: hover seconds, used in proportion to the thrust */
  if (S.t > GETD.lg && S.landed < 0 && S.running) { S.fuel -= dt * Math.hypot(cur.a[0], cur.a[1], cur.a[2]) / 1.62; if (S.fuel < 60) cue('fuel60'); if (S.fuel < 30) cue('fuel30'); }
  /* the calls by altitude */
  if (S.t > GETD.lg - 40 && S.landed < 0) { for (const [k, ft] of CALLS) if (padH < ft * FT) cue(k); if (padH < 150) cue('take'); }
  /* contact */
  if (S.landed < 0 && S.t > GETD.lg && padH <= 0.03) {
    const up = D.upAt(cur.r), vu = cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2], vh = Math.sqrt(Math.max(0, cur.v[0] ** 2 + cur.v[1] ** 2 + cur.v[2] ** 2 - vu * vu));
    S.contactV = [-vu, vh];
    if (S.took && (-vu > SAFE_V || vh > SAFE_H)) { FADE.target = 1; FADE.then = 'again'; }
    else { S.landed = S.t; cue('contact'); sound.thump(0.35, 0.4, 300); cur = { ...cur, v: [0, 0, 0], a: [0, 0, 0] }; if (S.live) S.live = cur; S.mode = S.mode === 'hand' ? 'hand' : S.mode; }
  }
  if (S.took && S.fuel <= 0 && S.landed < 0 && !FADE.then) { FADE.target = 1; FADE.then = 'again'; }
  if (FADE.then === 'again' && FADE.k > 0.92) { FADE.then = null; restart(GETD.hg - 5); S.restarts++; FADE.target = 0; if (radio) radio.cue('again'); }
  if (S.landed >= 0 && S.t > S.landed + 40 && !S.ended) { S.ended = true; hud.on = true; }
  moon.setSunOffset(GETL ? (S.t - GETL.land) * 0.508 / 3600 : 0);
  _sun.copy(moon.shared.uSun.value);
  sky.userData.setSpin(W_EARTH * (S.t - GETL.land) + 2.1);
  attitude(dt);
  vehicle.update(-1e9, 'outside', { alt: 1e9, up: D.upAt(cur.r) });
  vehicle.space({ sivb: null, lm: _m.identity(), open: 1, gone: 1e6, sps: 0, j2: 0 });
  for (const k of ['sic', 'skirt', 'sii', 'sivb', 'csm', 'les', 'cm']) if (vehicle.parts[k]) vehicle.parts[k].visible = false;
  placeRig();
  if (low()) { ground.update(cur.r[0], cur.r[2]); }
  updateShadow(now);
  /* light: the Sun (the Moon's shadow does not reach here: it is morning at the site); the ground's grey light from below */
  vSun.position.copy(_sun).multiplyScalar(100); vSun.intensity = 2.6;
  vHemi.position.fromArray(D.upAt(cur.r)); vHemi.color.setRGB(0.05, 0.05, 0.06); vHemi.groundColor.setRGB(0.55, 0.53, 0.5); vHemi.intensity = 0.55;
  lmc.setDaylight(1, _x.copy(_sun).applyQuaternion(_q.copy(lmAtt).invert()));
  /* the engine: the cabin trembles; the sound */
  const thrust = S.landed >= 0 ? 0 : Math.hypot(cur.a[0], cur.a[1], cur.a[2]) / 3.0;
  lmc.group.matrix.multiply(_m.makeTranslation((Math.random() - 0.5) * 0.0006 * thrust, 0, (Math.random() - 0.5) * 0.0006 * thrust));
  sound.drive(Math.min(1, thrust) * 0.4, S.warp);
  /* instruments */
  for (const b of lmc.balls) b.quaternion.copy(lmAtt).invert();
  if (now - lastDsky > 400) {
    const d = lmc.dsky, up = D.upAt(cur.r), vu = cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2];
    const p = S.landed >= 0 ? '68' : S.t < GETD.hg ? '63' : S.t < GETD.lg ? '64' : '66';
    d.draw(S.t < GETD.pdi ? '63' : p, '06', '60', d.f(Math.hypot(cur.v[0], cur.v[1], cur.v[2]) / FT), d.f(vu / FT * 10), d.f(Math.max(0, padH) / FT));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  updateOutside(dt);
  updateHands(dt);
  lmc.update(now / 1000);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, OUT.on ? 0 : 1.62, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 5. A (N on this screen): out onto the surface.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + prog(S.t) + '  ' + Math.max(0, padH).toFixed(0) + ' m' + (S.warp > 1 ? '  ×' + S.warp : '');
  hud.draw(now);
  render();
  S.frames++;
  if (window.__descent.afterFrame) { const f = window.__descent.afterFrame; window.__descent.afterFrame = null; f(); }
}
const low = () => padH < 2000 && cur && Math.hypot(cur.r[0], cur.r[2]) < 60000;

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx && !radio) radio = await makeRadio(ctx, sound.out, new URL('../assets/script/descent.json', import.meta.url).href);
}
function readOptions() { if ($('start')) restart(+$('start').value); }
async function enterVR() {
  if (!navigator.xr) return;
  readOptions(); await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(0.5);
  const hz = +($('hz')?.value || 72);
  if (session.updateTargetFrameRate && session.supportedFrameRates?.includes(hz)) session.updateTargetFrameRate(hz).catch(() => {});
  for (const i of [0, 1]) {
    const grip = renderer.xr.getControllerGrip(i); head.add(grip);
    grip.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') grip.add(hud.mesh); });
  }
  hud.mesh.position.set(0, 0.09, -0.06); hud.mesh.rotation.set(-0.9, 0, 0);
  S.yaw = 0; S.pitch = 0; head.position.set(0, 0, 0); head.rotation.set(0, 0, 0);
  SEAT.pending = 1; SEAT.done = false;
  document.body.classList.add('invr');
  setTimeout(() => { S.running = true; }, 1500);
  session.addEventListener('end', () => { document.body.classList.remove('invr'); S.running = false; renderer.setSize(innerWidth, innerHeight); });
}
$('vr')?.addEventListener('click', enterVR);
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 30); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.shared.uExposure.value = 3.4;
  sky = makeSky(moon);
  rocks = await makeRocks(moon, BASE);
  ground = makeGround(moon, BASE);
  scene.add(sky, moon.far, moon.near, ground.group, rocks.group);
  D = makeDescent(moon.manifest, (x, z) => moon.heightAt(x, z));
  cur = D.state(S.t); padH = D.padHeight(cur);
  if (ground.ok) await ground.request(0, 0).catch(() => {});
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__descent = { S, OUT, GETD, renderer, camera, rig, head, lmc, vehicle, hands, SEAT, afterFrame: null, restart, toggleHand,
  get D() { return D; }, get cur() { return cur; }, get padH() { return padH; }, get radio() { return radio; }, get moon() { return moon; } };
