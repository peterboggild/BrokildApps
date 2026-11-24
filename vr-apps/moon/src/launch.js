/* Journey to the Moon, chapter 1: the launch.
 *
 * Boarding (optional): you walk along the crew access arm into the White Room, the count holding at T-3:30, and
 * climb in through the open hatch; the hatch closes, you are seated, the arm swings back, the count resumes.
 * Then from the Commander's couch to orbit insertion (T+11:53) and a minute beyond. The sky and the Earth are drawn
 * first, then the pad, then the cabin over them (its windows are openings). Until the escape tower is jettisoned
 * at T+3:18 the boost protective cover blinds every window except a small one over the hatch window.
 *
 * Curiosity, not authentic (owner's wish): Y opens a large window in the wall to your left; holding B takes you out
 * of the spacecraft to look at it from outside (sticks: round it, nearer or further, up or down).
 *
 * URL: ?board (start at the access arm)  ?t=-210 (start time, s from liftoff)  ?yaw= &pitch= (flat preview)
 *      ?still (clock stopped)  ?shot  ?scale= (VR resolution)  ?shake=0..1 */
import * as THREE from 'three';
import { state, EVENTS, phase, SUN } from './ascent.js?v=11';
import { makeOutside } from './earth.js?v=11';
import { makeCabin } from './cabin.js?v=11';
import { makeLaunchSound, makeHaptics, intensity } from './launch-sound.js?v=11';
import { makeRadio } from './radio.js?v=11';
import { makePad, ARM, WR } from './pad.js?v=11';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=11';
import { createHand } from './hands.js?v=11';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const END = 800;
const EYE_TEST = Q.has('eye') ? Q.get('eye').split(',').map(Number) : null;   /* test aid: the flat preview's eye, in the cabin frame */
const NOCABIN = Q.has('nocabin');                    /* test aid: the world outside without the spacecraft round it */

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;
renderer.xr.setReferenceSpaceType('local-floor');    /* standing to board; seated later (seat() brings the couch to you) */
$('stage').appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, innerWidth / innerHeight, 0.02, 100);
const rig = new THREE.Group(), head = new THREE.Group();
rig.add(head); head.add(camera); scene.add(rig);
addEventListener('resize', () => {
  if (renderer.xr.isPresenting) return;
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight);
});

const S = {
  t: num('t', -210), running: false, warp: 1, still: Q.has('still'), shake: num('shake', 1),
  yaw: num('yaw', 0), pitch: num('pitch', 0), fps: 0, calls: 0, tris: 0, frames: 0, ready: false, dsky: 0,
  mode: Q.has('board') ? 'boarding' : 'seated', resumeAt: 0
};
/* the walker while boarding: pad-frame position of the rig (the floor under you) and its facing */
const W = { x: num('wx', 0), z: num('wz', ARM.z0 + 0.9), yaw: Math.PI, dwell: 0, snap: 0 };   /* ?wx= &wz=: test aid */
const EYE_PAD = new THREE.Vector3(0.6, 0, -0.15);     /* the Commander's eye point relative to the vehicle's axis */

const outside = makeOutside(renderer);
const cabin = makeCabin();
rig.add(cabin.group);
scene.add(outside.far, outside.near);
const padScene = makePad();
outside.pad.add(padScene.group);
const vehicle = makeVehicle(renderer);
outside.near.add(vehicle.group, vehicle.lights);
/* the attitude on the pad, to turn the vehicle (built in the pad's frame) with the spacecraft */
const attPad = new THREE.Quaternion(), attRel = new THREE.Quaternion();
/* looking from outside (B held) */
const OUT = { want: false, on: false, az: -1.25, el: 0.12, dist: 16 };
const OPT = { body: true };
/* a black veil for the moment of climbing in */
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };
const sound = makeLaunchSound(), haptics = makeHaptics();
let radio = null;

/* ---------------------------------------------------------------- the HUD (a test instrument on the left controller) */
const hud = (() => {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.08), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false,
    }));
  mesh.renderOrder = 999;
  let last = 0;
  return { mesh, on: true, draw(now) {
    if (now - last < 500) return; last = now;
    g.clearRect(0, 0, 512, 256); g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(0, 0, 512, 256);
    g.fillStyle = S.fps > 68 ? '#9fe6a0' : S.fps > 55 ? '#f0d070' : '#ff8a70'; g.font = 'bold 70px monospace'; g.fillText(S.fps.toFixed(0) + ' fps', 20, 80);
    g.fillStyle = '#d8d8d8'; g.font = '28px monospace';
    g.fillText(S.calls + ' calls  ' + (S.tris / 1000).toFixed(0) + 'k tris', 20, 126);
    g.fillText(phase(S.t) + (S.warp > 1 ? '   x' + S.warp : ''), 20, 168);
    g.fillStyle = '#8a8a8a'; g.font = '19px monospace'; g.fillText('A/X time  Y window  hold B outside', 12, 214);
    g.fillText('left stick press: panel  right stick press: re-seat', 12, 240);
    tex.needsUpdate = true;
  } };
})();

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startCount(); S.running = !S.running || !S.started; S.started = true; }
  if (e.code === 'KeyW' && S.mode === 'seated') cycleWarp();
  if (e.code === 'KeyE' && S.mode === 'boarding') boardNow();
  if (e.code === 'KeyC' && S.mode === 'seated') cabin.setCheat(!cabin.cheat);
  if (e.code === 'KeyO' && S.mode === 'seated') OUT.want = !OUT.want;
  const jump = { Digit1: -210, Digit2: -30, Digit3: 185, Digit4: 540, Digit5: 700 }[e.code];
  if (jump !== undefined) setTime(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => {
  if (!drag) return;
  S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2));
  drag = [e.clientX, e.clientY];
});
function warpAllowed(t) { return t > EVENTS.towerJett + 20 && !(t > EVENTS.siiCutoff - 15 && t < EVENTS.sivbIgnition + 8) && t < EVENTS.seco - 12; }
function cycleWarp() { S.warp = !warpAllowed(S.t) ? 1 : S.warp === 1 ? 4 : S.warp === 4 ? 16 : 1; }
function setTime(t) { S.t = t; S.warp = 1; }
const pad = { prev: {} };
function readControllers() {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed, tr = gp.buttons[0]?.pressed;
    if (a && !pad.prev[k + 'a'] && S.mode === 'seated') cycleWarp();
    /* Y (left): the cheat window; B (right), held: outside; stick presses: left the panel, right re-seat */
    if (k === 'left' && b && !pad.prev[k + 'b'] && S.mode === 'seated') cabin.setCheat(!cabin.cheat);
    if (k === 'right') OUT.want = !!b && S.mode === 'seated';
    if (st && !pad.prev[k + 's']) { if (k === 'left') hud.on = !hud.on; else if (S.mode === 'seated') SEAT.pending = 1; }
    if (tr && !pad.prev[k + 't'] && S.mode === 'boarding') boardNow();
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b; pad.prev[k + 's'] = st; pad.prev[k + 't'] = tr;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') { STICK.x = ax[0]; STICK.y = ax[1]; } else { STICK.turn = ax[0]; STICK.ry = ax[1]; }
  }
}
const STICK = { x: 0, y: 0, turn: 0, ry: 0 };

/* ---------------------------------------------------------------- boarding */
const _hp = new THREE.Vector3(), _fw = new THREE.Vector3();
/* your head in the pad frame */
function headPad(out) { out.setFromMatrixPosition(camera.matrixWorld); out.x += st.pos[0]; out.y += st.pos[1]; out.z += st.pos[2]; return out; }
function walk(dt) {
  const xr = renderer.xr.isPresenting;
  let mx = 0, mz = 0;
  if (xr) { mx = STICK.x; mz = STICK.y; }
  else { mz = (keys.KeyS ? 1 : 0) - (keys.KeyW ? 1 : 0); mx = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0); }
  if (Math.abs(mx) > 0.15 || Math.abs(mz) > 0.15) {
    camera.getWorldDirection(_fw); _fw.y = 0; _fw.normalize();
    const sp = 1.0 * dt;
    W.x += (_fw.x * -mz - _fw.z * mx) * sp; W.z += (_fw.z * -mz + _fw.x * mx) * sp;
  }
  if (xr) {                                          /* snap turns, 30 degrees */
    if (Math.abs(STICK.turn) > 0.7 && !W.snap) { W.yaw -= Math.sign(STICK.turn) * Math.PI / 6; W.snap = 1; }
    if (Math.abs(STICK.turn) < 0.3) W.snap = 0;
  }
  /* stay inside the arm and the White Room (real walking included: the rig is pushed back) */
  headPad(_hp);
  let x = _hp.x, z = _hp.z;
  if (z < WR.z0 + 0.05) { x = Math.max(-ARM.half + 0.22, Math.min(ARM.half - 0.22, x)); z = Math.max(ARM.z0 + 0.35, z); }
  else { x = Math.max(-WR.half + 0.25, Math.min(WR.half - 0.25, x)); z = Math.min(-1.65, z); }
  W.x += x - _hp.x; W.z += z - _hp.z;
  /* at the hatch: climb in */
  const atHatch = z > -2.1 && Math.abs(x) < 0.95;
  W.dwell = atHatch ? W.dwell + dt : 0;
  if (W.dwell > 0.5) boardNow();
  if (z > WR.z0 + 0.3 && !W.inWR) { W.inWR = true; radio?.cue('whiteroom'); }
}
function boardNow() {
  if (S.mode !== 'boarding') return;
  headPad(_hp);
  if (_hp.z < WR.z0 - 0.2) return;                    /* only from the White Room */
  S.mode = 'seating'; FADE.target = 1;
}
function seatAfterBoarding() {
  rig.add(cabin.group); cabin.group.position.set(0, 0, 0); cabin.group.quaternion.identity();
  cabin.setHatch(0);
  rig.position.set(0, 0, 0);
  S.mode = 'seated';
  if (renderer.xr.isPresenting) { head.position.set(0, 0, 0); head.rotation.set(0, 0, 0); SEAT.pending = 1; SEAT.done = false; }
  else { S.yaw = 0; S.pitch = 0; }
  sound.hatch && sound.hatch();
  radio?.cue('seated');
  S.resumeAt = performance.now() + 16000;            /* the arm swings back, then the count resumes */
  FADE.target = 0;
}

/* Seating. The headset reports your head relative to its own origin, which is wherever the Quest was last
 * recentred: it can be metres from where you sit now, and at another height. So the couch is brought to you:
 * `head` (the parent of the camera and the controllers) is moved and turned so that your eyes, as they are now,
 * are at the Commander's eye point looking at the panel. Done when the session starts, and again whenever a
 * thumbstick is pressed. */
const SEAT = { pending: 0, done: false };
const _e = new THREE.Euler();
function seat() {
  /* after renderer.xr.updateCamera, camera.position and .quaternion are the headset's pose in `head`'s frame */
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true;
}

/* ---------------------------------------------------------------- rendering */
function setPlanes(n, f) {
  const fix = c => { const e = c.projectionMatrix.elements; e[10] = -(f + n) / (f - n); e[14] = -2 * f * n / (f - n); c.projectionMatrixInverse.copy(c.projectionMatrix).invert(); };
  if (renderer.xr.isPresenting) { const xc = renderer.xr.getCamera(); fix(xc); xc.cameras.forEach(fix); }
  else { camera.near = n; camera.far = f; camera.updateProjectionMatrix(); }
}
let st = state(S.t);
function render() {
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (SEAT.pending && camera.position.lengthSq() + Math.abs(camera.quaternion.w - 1) > 0) {
      /* wait a few frames for a real pose, then seat and apply at once */
      if (++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
    }
  }
  outside.dome.position.setFromMatrixPosition(camera.matrixWorld);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, f, n) => { cabin.group.visible = c && !NOCABIN && !OUT.on; hud.mesh.visible = (c || S.mode !== 'seated') && hud.on; outside.far.visible = f; outside.near.visible = n; sign.visible = c && SIGN.k > 0.01; };
  if (S.mode !== 'seated') {
    /* boarding: the sky, then the pad, the arm, the White Room and the spacecraft (its cabin seen through the
     * open hatch) together, in one depth range */
    show(false, true, false); setPlanes(20, 6e7); renderer.render(scene, camera);
    renderer.clearDepth();
    show(true, false, true); setPlanes(0.05, 80000); renderer.render(scene, camera);
    drawVeil();
    S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
    return;
  }
  /* The outside first, the cabin last over it: the cabin covers everything but its window openings. (Not a stencil
   * mask: the Quest's XR framebuffer may have no stencil buffer, and then the sky was drawn over the whole cabin.) */
  /* 1: sky and Earth */
  show(false, true, false); setPlanes(OUT.on ? Math.max(20, Math.min(st.alt * 0.2, 2000)) : Math.max(20, st.alt * 0.2), 6e7); renderer.render(scene, camera);
  /* 2: things near the vehicle: the pad, the tower, the escape tower leaving */
  renderer.clearDepth();
  show(false, false, true); setPlanes(0.5, 80000); renderer.render(scene, camera);
  /* 3: the cabin */
  renderer.clearDepth();
  show(true, false, false); setPlanes(0.02, 12); renderer.render(scene, camera);
  drawVeil();
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}
function drawVeil() {
  if (FADE.k <= 0.001) return;
  veil.material.opacity = FADE.k;
  camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale);
  veil.scale.set(1, 1, 1); veil.translateZ(-0.1);
  veil.updateMatrixWorld(true);
  renderer.render(fadeScene, camera);
}

const _m = new THREE.Matrix4(), _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _q0 = new THREE.Quaternion(), _sun = new THREE.Vector3();
let jolt = 0, prevT = S.t;
const _att = new THREE.Quaternion();
function placeRig() {
  _y.fromArray(st.head); _z.fromArray(st.nose).negate(); _x.crossVectors(_y, _z).normalize();
  _m.makeBasis(_x, _y, _z);
  _att.setFromRotationMatrix(_m);
  /* the vehicle turns with the spacecraft (it is built as it stands on the pad) */
  attRel.copy(_att).multiply(_q0.copy(attPad).invert());
  vehicle.group.quaternion.copy(attRel);
  if (S.mode === 'seated' && OUT.on) { placeOutside(); return; }
  if (S.mode === 'seated') { rig.quaternion.copy(_att); rig.position.copy(EYE_OFF).applyQuaternion(attRel); return; }
  /* boarding: you stand on the arm; the cabin stays where it is in the spacecraft, at the eye point */
  rig.quaternion.setFromAxisAngle(_y.set(0, 1, 0), W.yaw);
  rig.position.set(W.x - st.pos[0], ARM.floor - st.pos[1], W.z - st.pos[2]);
  if (cabin.group.parent !== scene) scene.add(cabin.group);
  cabin.group.quaternion.copy(_att);
  cabin.group.position.set(EYE_PAD.x, 0, EYE_PAD.z);
}

/* outside: a point on a sphere round the spacecraft, the horizon level, looking at it */
const _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3(), _up = new THREE.Vector3(), _tg = new THREE.Vector3();
function placeOutside() {
  _up.fromArray(st.up); _h1.fromArray(st.fwd); _h2.crossVectors(_up, _h1).normalize();
  _tg.set(0, 0.4, 0).applyQuaternion(attRel);
  rig.position.copy(_h1).multiplyScalar(Math.cos(OUT.az)).addScaledVector(_h2, Math.sin(OUT.az)).multiplyScalar(Math.cos(OUT.el))
    .addScaledVector(_up, Math.sin(OUT.el)).multiplyScalar(OUT.dist).add(_tg);
  _z.copy(rig.position).sub(_tg).normalize(); _x.crossVectors(_up, _z).normalize(); _y.crossVectors(_z, _x);
  rig.quaternion.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
function steerOutside(dt) {
  let ax = 0, ay = 0, ey = 0;
  if (renderer.xr.isPresenting) { ax = STICK.x; ay = STICK.y; ey = STICK.ry; }
  else { ax = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); ey = (keys.KeyI ? -1 : 0) + (keys.KeyK ? 1 : 0); ay = (keys.KeyM ? 1 : 0) - (keys.KeyU ? 1 : 0); }
  const dz = v => Math.abs(v) < 0.15 ? 0 : v;
  OUT.az += dz(ax) * 1.1 * dt;
  OUT.dist = Math.min(600, Math.max(7, OUT.dist * Math.exp(dz(ay) * 1.4 * dt)));
  OUT.el = Math.min(1.45, Math.max(-1.3, OUT.el - dz(ey) * 0.9 * dt));
}
/* going out and coming back: a short blink */
function updateOutside(dt) {
  if (S.mode !== 'seated') { OUT.want = false; }
  if (OUT.want !== OUT.on) { FADE.target = 1; if (FADE.k > 0.9) { OUT.on = OUT.want; FADE.target = 0; } }
  else if (S.mode === 'seated' && FADE.target === 1 && FADE.k > 0.9) FADE.target = 0;
  if (OUT.on) steerOutside(dt);
}

/* ---------------------------------------------------------------- hands, and touching the controls */
const hands = { left: createHand('left'), right: createHand('right') };
const SIGN = { k: 0, t: 0 };
const sign = new THREE.Mesh(new THREE.PlaneGeometry(0.12, 0.045), new THREE.MeshBasicMaterial({ transparent: true, depthTest: false, map: (() => {
  const c = document.createElement('canvas'); c.width = 256; c.height = 96; const x = c.getContext('2d');
  x.fillStyle = 'rgba(150,20,14,0.92)'; x.beginPath(); x.roundRect(4, 4, 248, 88, 16); x.fill();
  x.fillStyle = '#fff'; x.font = 'bold 46px Arial'; x.textAlign = 'center'; x.fillText('Not now', 128, 64);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; })() }));
sign.renderOrder = 1000; sign.visible = false; scene.add(sign);
const _tip = new THREE.Vector3(), _loc = new THREE.Vector3();
function touchingPanel(p) {
  for (const pnl of cabin.panels) {
    _loc.copy(p); pnl.mesh.worldToLocal(_loc);
    if (Math.abs(_loc.x) < pnl.w / 2 && Math.abs(_loc.y) < pnl.h / 2 && _loc.z < 0.03 && _loc.z > -0.03) return true;
  }
  return false;
}
function updateHands(dt) {
  const s = renderer.xr.getSession();
  if (!s) return;
  let touched = null;
  for (let i = 0; i < 2; i++) {
    const src = s.inputSources[i]; if (!src || !src.gamepad) continue;
    const m = hands[src.handedness]; if (!m) continue;
    const grip = renderer.xr.getControllerGrip(i);
    if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    const thumb = [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed));
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, thumb, false, dt);
    m.userData.tip.getWorldPosition(_tip);
    const hit = S.mode === 'seated' && !OUT.on && touchingPanel(_tip);
    m.userData.warn = hit ? 0.4 : Math.max(0, (m.userData.warn || 0) - dt);
    m.userData.setTint(m.userData.warn > 0);
    if (hit) {
      touched = _tip.clone();
      if (!m.userData.buzzed) { gp.hapticActuators?.[0]?.pulse?.(0.5, 60).catch?.(() => {}); m.userData.buzzed = true; }
    } else m.userData.buzzed = false;
  }
  if (touched) {
    SIGN.k = 1; SIGN.t = 1.4;
    _loc.setFromMatrixPosition(camera.matrixWorld);
    sign.position.copy(touched).lerp(_loc, 0.25); sign.position.y += 0.0;
    sign.quaternion.copy(camera.getWorldQuaternion(_q0));
    sign.translateY(0.05);
  } else { SIGN.t -= dt; if (SIGN.t < 0) SIGN.k = Math.max(0, SIGN.k - dt * 3); }
  sign.material.opacity = SIGN.k;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, lastAlt = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  if (xr) readControllers();
  if (S.mode === 'boarding') walk(dt);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (S.mode === 'seating' && FADE.k > 0.97) seatAfterBoarding();
  if (S.resumeAt && performance.now() > S.resumeAt) { S.resumeAt = 0; S.running = true; S.started = true; }
  if (S.running && !S.still) {
    if (!warpAllowed(S.t)) S.warp = 1;
    S.t = Math.min(END, S.t + dt * S.warp);
  }
  st = state(S.t, st);
  placeRig();
  outside.update(st, S.t, EVENTS);
  padScene.update(now / 1000, dt, S.mode, S.t);
  vehicle.update(S.t, S.mode !== 'seated' ? 'boarding' : OUT.on ? 'outside' : 'inside', st);
  if (S.mode !== 'seated') cabin.setHatch(1);
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000);
  cabin.setBody(OPT.body && S.mode === 'seated');
  /* the cover and the daylight */
  const open = S.t >= EVENTS.towerJett;
  cabin.setCovers(!open && S.mode === 'seated');
  _sun.fromArray(SUN).applyQuaternion(_q0.copy(rig.quaternion).invert());
  cabin.setDaylight(open ? 1 : 0.15, _sun);
  /* shaking: the cabin round you, never your view */
  const k = intensity(S.t);
  if (prevT < EVENTS.sicCutoff + 0.6 && S.t >= EVENTS.sicCutoff + 0.6) jolt = 1;
  if (prevT < EVENTS.siiCutoff + 0.8 && S.t >= EVENTS.siiCutoff + 0.8) jolt = 0.6;
  prevT = S.t;
  jolt = Math.max(0, jolt - dt * 3);
  const a = 0.0035 * k.shake * S.shake;
  if (S.mode === 'seated') cabin.group.position.set((Math.random() - 0.5) * a, (Math.random() - 0.5) * a, (Math.random() - 0.5) * a * 0.6 + 0.03 * jolt * Math.sin(jolt * Math.PI) * S.shake);
  /* instruments */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  if (now - lastDsky > 500) {
    const d = cabin.dsky, rate = (st.alt - lastAlt) / Math.max(0.05, (now - lastDsky) / 1000) / Math.max(1, S.warp);
    if (S.t < 0) d.draw('02', '16', '65', d.f(0), d.f(0), d.f(0));
    else d.draw('11', '06', '62', d.f(st.speed * 3.281 + 1340), d.f(rate * 3.281), d.f(st.alt / 1852 * 10));
    lastDsky = now; lastAlt = st.alt;
  }
  /* sound, radio, controllers */
  sound.update(S.t, S.warp);
  sound.ambience(S.mode !== 'seated');
  if (radio && S.running) radio.update(S.t, S.warp);
  if (xr) haptics(renderer.xr.getSession(), S.t, S.shake, now / 1000);
  if (!xr && EYE_TEST) { head.position.fromArray(EYE_TEST); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  else if (!xr) { head.position.set(0, S.mode === 'seated' ? 0 : 1.65, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : ''; }
  if ($('clock')) $('clock').textContent = (S.t < 0 ? 'T−' : 'T+') + fmt(Math.abs(S.t)) + '  ' + (S.running ? phase(S.t) : 'holding') + (S.warp > 1 ? '  ×' + S.warp : '');
  hud.draw(now);
  render();
  S.frames++;
  if (window.__launch.afterFrame) { const f = window.__launch.afterFrame; window.__launch.afterFrame = null; f(); }
}
const fmt = s => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx && !radio) radio = await makeRadio(ctx, sound.out, new URL('../assets/script/launch.json', import.meta.url).href);
}
function startCount() { startSound(); }
async function enterVR() {
  if (!navigator.xr) return;
  readOptions();
  await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(0.5);
  const hz = +($('hz')?.value || 72);
  if (session.updateTargetFrameRate && session.supportedFrameRates?.includes(hz)) session.updateTargetFrameRate(hz).catch(() => {});
  for (const i of [0, 1]) {
    const grip = renderer.xr.getControllerGrip(i); head.add(grip);       /* the same frame as the camera */
    grip.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') grip.add(hud.mesh); });
  }
  hud.mesh.position.set(0, 0.09, -0.06); hud.mesh.rotation.set(-0.9, 0, 0);
  S.yaw = 0; S.pitch = 0;
  head.position.set(0, 0, 0); head.rotation.set(0, 0, 0);
  document.body.classList.add('invr');
  if (S.mode === 'seated') {
    SEAT.pending = 1; SEAT.done = false;
    setTimeout(() => { S.running = true; S.started = true; }, 2500);   /* a moment to settle in before the count resumes */
  } else beginBoarding();
  session.addEventListener('end', () => { document.body.classList.remove('invr'); S.running = false; renderer.setSize(innerWidth, innerHeight); });
}
function beginBoarding() {
  S.mode = 'boarding'; S.running = false; S.t = -210; W.x = 0; W.z = ARM.z0 + 0.9; W.yaw = Math.PI; W.inWR = false;
  setTimeout(() => radio?.cue('board'), 1500);
}
function readOptions() {
  if ($('opt-body')) OPT.body = $('opt-body').checked;
  if ($('start')) { const v = $('start').value; if (v === 'board') { S.mode = 'boarding'; S.t = -210; } else { S.mode = 'seated'; S.t = +v; } }
  if ($('shake')) S.shake = +$('shake').value;
}
$('vr')?.addEventListener('click', enterVR);
$('flat')?.addEventListener('click', async () => {
  readOptions(); await startSound(); document.body.classList.add('running');
  if (S.mode === 'boarding') beginBoarding(); else { S.running = true; S.started = true; }
});
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = state(-60); _y.fromArray(s0.head); _z.fromArray(s0.nose).negate(); _x.crossVectors(_y, _z).normalize(); attPad.setFromRotationMatrix(_m.makeBasis(_x, _y, _z)); }
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 16); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  if (Q.has('cheat')) cabin.setCheat(true);
  outside.update(state(S.t), S.t, EVENTS);          /* asks for the first sky table */
  await outside.ready;
  st = state(S.t); placeRig();
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__launch = { S, W, OUT, OPT, renderer, camera, rig, head, cabin, outside, padScene, vehicle, hands, afterFrame: null, setTime, SEAT, boardNow, beginBoarding, get radio() { return radio; },
  walkTo(x, z) { W.x = x; W.z = z; },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; } };
