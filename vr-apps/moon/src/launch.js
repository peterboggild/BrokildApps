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
import { state, EVENTS, phase, SUN } from './ascent.js?v=31';
import { makeOutside } from './earth.js?v=31';
import { makeCabin } from './cabin.js?v=31';
import { makeLaunchSound, makeHaptics, intensity } from './launch-sound.js?v=31';
import { makeRadio, combineRadios } from './radio.js?v=31';
import { makeClock } from './clock.js?v=31';
import { makeChatter } from './chatter.js?v=31';
import { handOnCrew } from './crew-life.js?v=31';
import { makePad, ARM, WR } from './pad.js?v=31';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=31';
import { createHand, makeControllers } from './hands.js?v=31';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=31';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=31';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=31';

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
const OPT = { get body() { return settings.get('body'); } };
/* a black veil for the moment of climbing in */
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };
const sound = makeLaunchSound(), haptics = makeHaptics();
let chat = null, radio = null;

/* ---------------------------------------------------------------- the wrist panel (X) and messages */
const ctl = makeControllers(renderer, head);
const common = applyCommon(renderer, sound);
const wrist = makeWrist(['ff', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
const LEVENTS = [[EVENTS.liftoff, 'liftoff'], [EVENTS.maxQ, 'max Q'], [EVENTS.sicCutoff, 'first staging'], [EVENTS.towerJett, 'tower jettison'],
  [EVENTS.siiCutoff, 'second staging'], [EVENTS.seco, 'orbit insertion'], [END, 'end of the chapter']];
function info() {
  const boarding = S.mode !== 'seated';
  return { chapter: 'Chapter 1 \u00b7 Launch', now: boarding ? 'boarding: the White Room and the hatch' : S.running || S.t > 0 ? phase(S.t) : 'the count is holding',
    next: S.t >= END ? null : nextOf(LEVENTS, S.t), after: 'chapter 2, to the Moon', get: S.t, warp: S.warp,
    rows: [['Altitude', fmtDist(st.alt)], ['Speed', fmtSpeed(st.speed)], ['Downrange', fmtDist(st.downrange)], ['Fast forward', settings.text('ff')]],
    alert: S.t >= END ? 'End of chapter 1.  A: on to chapter 2' : null,
    keys: boarding ? [['L stick', 'walk'], ['trigger', 'board now'], ['L stick', 'press: panel, settings']]
      : [['A', 'slower (hold: real time)'], ['B', 'faster'], ['X', 'cheat window'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['R stick', 'press: re-seat']], fps: S.fps };
}

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startCount(); S.running = !S.running || !S.started; S.started = true; }
  if (e.code === 'KeyW' && S.mode === 'seated') cycleWarp();
  if (e.code === 'KeyS' && S.mode === 'seated') clock.slower(S);
  if (e.code === 'KeyP') wrist.press();
  if (e.code === 'KeyE' && S.mode === 'boarding') boardNow();
  if (e.code === 'KeyC' && S.mode === 'seated') cabin.setCheat(!cabin.cheat);
  if (e.code === 'KeyO' && S.mode === 'seated') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.t >= END) nextChapter('orbit.html');
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
/* fast forward (clock.js): x4 at most from fifteen seconds before liftoff to the tower jettison, through the second
 * staging (every event) and at orbit insertion */
const LHOLD_ALL = [[-15, EVENTS.towerJett + 20, 'liftoff and the first stage'], [EVENTS.siiCutoff - 15, EVENTS.sivbIgnition + 8, 'the second staging'], [EVENTS.seco - 12, END + 1, 'orbit insertion']];
const clock = makeClock({ holds: () => holdsFor(LHOLD_ALL, [LHOLD_ALL[0], LHOLD_ALL[2]]), lines: () => radio ? radio.lines : [], warps: [1, 4, 16], cap: 4, toast });
function cycleWarp() { clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
function setTime(t) { S.t = t; S.warp = 1; clock.reset(); }
/* the chapter's end: A (or N on the screen) goes on; a page change must leave VR first */
/* on to the next chapter: the page changes while still in VR, and the next page enters VR by itself where the browser
 * allows it (progress.js autoEnter); a break: out of VR, to this page (Continue will find the place) */
function nextChapter(href) { location.href = href; }
function takeBreak() { const s = renderer.xr.getSession(); if (s) s.end(); card.hide(); }
const pad = { prev: {}, aDown: -1 };
/* B (right): one step faster; A (right): one step slower, held: real time at once; X (left): the cheat window;
 * Y (left): outside (both stay till pressed again); left stick press: the panel, then the settings; right stick
 * press: re-seat; boarding: the trigger boards */
function readControllers(now) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed, tr = gp.buttons[0]?.pressed;
    if (k === 'right') {
      if (a && !pad.prev.ra && S.mode === 'seated') { pad.aDown = now; if (S.t >= END) { nextChapter('orbit.html'); pad.aDown = -1; } }
      if (a && pad.aDown >= 0 && now - pad.aDown > 600) { realTime(); pad.aDown = -1; }
      if (!a && pad.prev.ra && pad.aDown >= 0) { clock.slower(S); pad.aDown = -1; }
      if (b && !pad.prev.rb && S.t >= END) takeBreak();
      if (b && !pad.prev.rb && S.mode === 'seated' && S.t < END) cycleWarp();
      if (st && !pad.prev.rs && S.mode === 'seated') SEAT.pending = 1;
    } else if (k === 'left') {
      if (a && !pad.prev.la && S.mode === 'seated') cabin.setCheat(!cabin.cheat);       /* X: the cheat window, Y: outside (both stay) */
      if (b && !pad.prev.lb && S.mode === 'seated') OUT.want = !OUT.want;
      if (st && !pad.prev.ls) wrist.press();
    }
    if (tr && !pad.prev[k[0] + 't'] && S.mode === 'boarding') boardNow();
    pad.prev[k[0] + 'a'] = a; pad.prev[k[0] + 'b'] = b; pad.prev[k[0] + 's'] = st; pad.prev[k[0] + 't'] = tr;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') { wrist.stick(ax[0], ax[1]); if (wrist.capturesStick) { STICK.x = 0; STICK.y = 0; } else { STICK.x = ax[0]; STICK.y = ax[1]; } } else { STICK.turn = ax[0]; STICK.ry = ax[1]; }
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
const SEAT = { pending: 0, done: false }, recentred = watchRecentre(renderer, camera);
const _e = new THREE.Euler();
function seat() {
  /* after renderer.xr.updateCamera, camera.position and .quaternion are the headset's pose in `head`'s frame */
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true; SEAT.n = (SEAT.n || 0) + 1;
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
    if (recentred() && SEAT.done && !SEAT.pending && S.mode === 'seated') SEAT.pending = 1;     /* the headset's own "reset view" */
    if (SEAT.pending && camera.position.lengthSq() + Math.abs(camera.quaternion.w - 1) > 0) {
      /* wait a few frames for a real pose, then seat and apply at once */
      if (++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
    }
  }
  outside.dome.position.setFromMatrixPosition(camera.matrixWorld);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, f, n) => { cabin.group.visible = c && !NOCABIN && !OUT.on; wrist.mesh.visible = (c || S.mode !== 'seated') && wrist.on; toast.mesh.visible = (c || S.mode !== 'seated') && toast.k > 0.01; badge.mesh.visible = (c || S.mode !== 'seated') && badge.on; card.mesh.visible = (c || S.mode !== 'seated') && card.k > 0.01; outside.far.visible = f; outside.near.visible = n; sign.visible = c && SIGN.k > 0.01; };
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
  for (const h of ['left', 'right']) {
    const src = ctl.src[h]; if (!src || !src.gamepad) continue;
    const m = hands[h], grip = ctl.grips[h];
    if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    const thumb = [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed));
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, thumb, false, dt);
    const onCrew = S.mode === 'seated' && !OUT.on && handOnCrew(m, [cabin.life], gp, who => chat && chat.touch(who));
    m.userData.tip.getWorldPosition(_tip);
    const hit = S.mode === 'seated' && !OUT.on && touchingPanel(_tip);
    m.userData.warn = hit || onCrew ? 0.4 : Math.max(0, (m.userData.warn || 0) - dt);
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
  ctl.update();
  if (xr) readControllers(now);
  if (S.mode === 'boarding') walk(dt);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (S.mode === 'seating' && FADE.k > 0.97) seatAfterBoarding();
  if (S.resumeAt && performance.now() > S.resumeAt) { S.resumeAt = 0; S.running = true; S.started = true; }
  if (S.running && !S.still) {
    S.t = Math.min(END, clock.advance(S, dt));
    if (S.t >= END && !S.endShown) { S.endShown = true; wrist.show(true); card.show({ title: 'End of chapter 1', sub: 'Launch', a: 'Continue: chapter 2, To the Moon', b: 'Take a break' }); saveProgress('orbit.html', null, 0, true); }      /* the way on is on the wrist */
  }
  st = state(S.t, st);
  placeRig();
  outside.update(st, S.t, EVENTS);
  padScene.update(now / 1000, dt, S.mode, S.t);
  vehicle.update(S.t, S.mode !== 'seated' ? 'boarding' : OUT.on ? 'outside' : 'inside', st);
  if (S.mode !== 'seated') cabin.setHatch(1);
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio) });
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
  if (chat) chat.update(dt, { crew: ['CMP', 'LMP'], where: S.t < -20 ? ['pad'] : ['any', 'earth'], quiet: S.mode === 'seated' && !OUT.on && (S.t < -60 || S.t > EVENTS.seco + 20) && !(radio && radio.current(S.t)) });
  if (xr) haptics(renderer.xr.getSession(), S.t, S.shake, now / 1000);
  if (!xr && EYE_TEST) { head.position.fromArray(EYE_TEST); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  else if (!xr) { head.position.set(0, S.mode === 'seated' ? 0 : 1.65, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : ''; }
  if ($('clock')) $('clock').textContent = (S.t < 0 ? 'T−' : 'T+') + fmt(Math.abs(S.t)) + '  ' + (S.running ? phase(S.t) : 'holding') + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && S.mode === 'seated' && S.t < END) saveProgress('launch.html', S.t, now);
  render();
  S.frames++;
  if (window.__launch.afterFrame) { const f = window.__launch.afterFrame; window.__launch.afterFrame = null; f(); }
}
const fmt = s => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/launch.json', import.meta.url).href);
  if (ctx && !chat) chat = await makeChatter(ctx, sound.voice);
}
function startCount() { startSound(); }
async function enterVR() {
  if (!navigator.xr) return;
  readOptions();
  await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  if ($('hz')) settings.set('hz', +$('hz').value);
  renderer.xr.setFoveation(settings.get('fov'));
  common.all();
  card.show({ title: 'Chapter 1', sub: 'Launch' }, 6);
  session.addEventListener('selectstart', () => sound.init());          /* sound needs a press when VR was entered by itself */
  setTimeout(() => toast.say('Press the left stick for the status and the settings', 6, true), 7500);
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
  if ($('opt-body')) settings.set('body', $('opt-body').checked);
  if ($('start')) { const v = $('start').value; if (v === 'board') { S.mode = 'boarding'; S.t = -210; } else { S.mode = 'seated'; S.t = +v; } }
  if ($('shake')) S.shake = +$('shake').value;
}
$('vr')?.addEventListener('click', enterVR);
autoEnter(() => S.ready, enterVR);
offerContinue('launch.html', $('start'));          /* after a crash or a break: on from where you were */
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

window.__launch = { S, W, OUT, OPT, ctl, wrist, toast, clock, get chat() { return chat; }, settings, info, renderer, camera, rig, head, cabin, outside, padScene, vehicle, hands, afterFrame: null, setTime, SEAT, boardNow, beginBoarding, get radio() { return radio; },
  walkTo(x, z) { W.x = x; W.z = z; },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; } };
