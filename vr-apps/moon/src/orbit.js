/* Journey to the Moon, chapter 2: to the Moon.
 *
 * From orbit insertion (GET 0:12) through one and a half... in fact two Earth orbits, the trans-lunar injection burn
 * (GET 3:12:36, the S-IVB's J-2 for 5 min 51 s), transposition and docking (the CSM separates, turns round and docks
 * with the LM on top of the S-IVB, then pulls it free), and the three-day coast until the Moon's gravity takes over.
 * Time can run up to 10,000 times faster, never across the burn or the docking.
 *
 * The flight is space.js (an inertial frame: the launch's pad frame frozen at liftoff). The cabin is the launch's,
 * helmets off; the outside (earth.js), the vehicle (vehicle.js), the hands, the cheat window and the outside view are
 * shared with chapter 1.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)  ?cheat */
import * as THREE from 'three';
import { state as ascentState, SUN, RE } from './ascent.js?v=26';
import { where, GET, NORMAL, W_EARTH, T_SOI, R_MOON } from './space.js?v=26';
import { makeOutside } from './earth.js?v=26';
import { makeCabin } from './cabin.js?v=26';
import { makeLaunchSound } from './launch-sound.js?v=26';
import { makeRadio, combineRadios } from './radio.js?v=26';
import { makeClock } from './clock.js?v=26';
import { makeChatter } from './chatter.js?v=26';
import { handOnCrew } from './crew-life.js?v=26';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=26';
import { createHand, makeControllers } from './hands.js?v=26';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=26';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=26';
import { settings, applyCommon, holdsFor } from './settings.js?v=26';
import { makeFloaters } from './floaters.js?v=26';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);

/* the timeline of transposition and docking (GET, seconds) */
const TD = { sep: 13320, sepEnd: 13380, pitch0: 13385, pitch1: 13505, app0: 13510, dock: 13838, hard: 13860, extract: 17100, ptc: 21600, view: 17400 };
const END = T_SOI + 90;
const WARPS = [1, 10, 100, 1000, 10000];
/* never warp across these (by the fast-forward setting: all of them, the major ones, or none) */
const HOLD_ALL = [[GET.tli - 150, GET.tliEnd + 40, 'the TLI burn'], [TD.sep - 60, TD.hard + 40, 'transposition and docking'], [TD.extract - 60, TD.extract + 240, 'LM extraction']];
const HOLD_MAJOR = HOLD_ALL.slice(0, 2);
const holds = () => holdsFor(HOLD_ALL, HOLD_MAJOR);

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

const S = { t: num('t', 760), running: false, warp: 1, still: Q.has('still'), yaw: num('yaw', 0), pitch: num('pitch', 0),
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, roll: 0, ended: false, snap: true };

const outside = makeOutside(renderer);
const cabin = makeCabin();
rig.add(cabin.group);
cabin.setHelmets(false); cabin.setCovers(false);
scene.add(outside.far, outside.near);
const vehicle = makeVehicle(renderer);
outside.near.add(vehicle.group, vehicle.lights);
const attPad = new THREE.Quaternion(), attRel = new THREE.Quaternion(), att = new THREE.Quaternion(), attWant = new THREE.Quaternion();
const RATE = () => settings.get('turn') * Math.PI / 180;     /* the fastest the attitude turns as you see it, outside docking, rad/s */
const OUT = { want: false, on: false, az: -1.25, el: 0.2, dist: 22 };
const OPT = { get body() { return settings.get('body'); } };
const sound = makeLaunchSound();
let chat = null, radio = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };

/* things floating free in the cabin: weightlessness made visible, and yours to take (floaters.js) */
const floaters = makeFloaters(cabin.group, { tether: true });
const ctl = makeControllers(renderer, head);
const common = applyCommon(renderer, sound);

/* ---------------------------------------------------------------- the wrist panel (X) and messages */
const wrist = makeWrist(['ff', 'turn', 'hz', 'fov', 'voices', 'sound', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
const EVENTS = [[GET.tli, 'TLI burn'], [TD.sep, 'transposition and docking'], [TD.extract, 'LM extraction'], [TD.ptc, 'passive thermal control'], [T_SOI, "the Moon's sphere of influence"]];
function info() {
  return { chapter: 'Chapter 2 \u00b7 To the Moon', now: phase(S.t), next: S.ended ? null : nextOf(EVENTS, S.t), after: 'chapter 3, lunar orbit', get: S.t, warp: S.warp,
    rows: [['Above the Earth', fmtDist(w.alt)], ['To the Moon', fmtDist(w.dMoon)], ['Speed', fmtSpeed(w.speed)], ['Fast forward', settings.text('ff')]],
    alert: S.ended ? 'End of chapter 2.  A: on to chapter 3' : null,
    keys: [['A', 'slower (hold: real time)'], ['B', 'faster'], ['X', 'cheat window'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['grip', 'take things'], ['R stick', 'press: re-seat']], fps: S.fps };
}
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
const km = m => m > 1e6 ? (m / 1e6).toFixed(m > 1e8 ? 0 : 1) + 'k km' : (m / 1000).toFixed(0) + ' km';
function phase(t) {
  if (t < GET.tli) return 'Earth orbit';
  if (t < GET.tliEnd) return 'TLI burn';
  if (t < TD.sep) return 'leaving Earth';
  if (t < TD.hard) return 'transposition and docking';
  if (t < TD.extract + 60) return 'docked to the LM';
  if (t < T_SOI) return 'coast to the Moon';
  return 'in lunar space';
}

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyS') clock.slower(S);
  if (e.code === 'KeyP') wrist.press();
  if (e.code === 'KeyC') cabin.setCheat(!cabin.cheat);
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter('lunar.html');
  const jump = { Digit1: 760, Digit2: GET.tli - 160, Digit3: TD.sep - 70, Digit4: TD.extract - 70, Digit5: 24 * 3600, Digit6: T_SOI - 600 }[e.code];
  if (jump !== undefined) setTime(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const clock = makeClock({ holds, lines: () => radio ? radio.lines : [], warps: WARPS, cap: 10, toast });
const held = t => !!clock.inHold(t);
function cycleWarp() { clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5); } }
function setTime(t) { S.t = t; S.warp = 1; S.ended = false; S.snap = true; clock.reset(); }
/* the chapter's end: A (or N on the screen) goes on; a page change must leave VR first */
/* on to the next chapter: the page changes while still in VR, and the next page enters VR by itself where the browser
 * allows it (progress.js autoEnter); a break: out of VR, to this page (Continue will find the place) */
function nextChapter(href) { location.href = href; }
function takeBreak() { const s = renderer.xr.getSession(); if (s) s.end(); card.hide(); }
const STICK = { x: 0, y: 0, ry: 0 };
const pad = { prev: {}, aDown: -1 };
/* B (right): one step faster; A (right): one step slower, held: real time at once; X (left): the cheat window;
 * Y (left): outside (both stay till pressed again); left stick press: the panel, then the settings; right stick
 * press: re-seat */
function readControllers(now) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed;
    if (k === 'right') {
      if (a && !pad.prev.ra) { pad.aDown = now; if (S.ended) nextChapter('lunar.html'); }
      if (a && pad.aDown >= 0 && now - pad.aDown > 600) { realTime(); pad.aDown = -1; }
      if (!a && pad.prev.ra && pad.aDown >= 0) { if (!S.ended) clock.slower(S); pad.aDown = -1; }
      if (b && !pad.prev.rb && S.ended) takeBreak();
      if (b && !pad.prev.rb && !S.ended) cycleWarp();
      if (st && !pad.prev.rs) SEAT.pending = 1;
    } else if (k === 'left') {
      if (a && !pad.prev.la) cabin.setCheat(!cabin.cheat);       /* X: the cheat window, Y: outside (both stay) */
      if (b && !pad.prev.lb) OUT.want = !OUT.want;
      if (st && !pad.prev.ls) wrist.press();
    }
    pad.prev[k[0] + 'a'] = a; pad.prev[k[0] + 'b'] = b; pad.prev[k[0] + 's'] = st;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') { wrist.stick(ax[0], ax[1]); if (wrist.capturesStick) { STICK.x = 0; STICK.y = 0; } else { STICK.x = ax[0]; STICK.y = ax[1]; } } else STICK.ry = ax[1];
  }
}

/* seating: the couch brought to you (see launch.js) */
const SEAT = { pending: 0, done: false };
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true;
}

/* ---------------------------------------------------------------- attitude */
const _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion();
const V = a => new THREE.Vector3().fromArray(a);
function fromNoseHead(nose, headV, out) {
  _y.copy(headV).addScaledVector(nose, -headV.dot(nose)).normalize(); _z.copy(nose).negate(); _x.crossVectors(_y, _z).normalize();
  return out.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
const SUNV = V(SUN), NRM = V(NORMAL);
let tdBase = null;                  /* the attitude (vehicle to world) frozen at separation */
function attitudeAt(t, w, out) {
  const up = V(w.up), vel = V(w.v).normalize();
  if (t < TD.sep) return fromNoseHead(vel, up.clone().negate(), out);
  if (t < TD.view) {                                             /* T&D: the stack holds still in space; the CSM turns */
    if (!tdBase) { const wb = where(TD.sep); fromNoseHead(V(wb.v).normalize(), V(wb.up).negate(), _q2); tdBase = _q2.clone().multiply(_q.copy(attPad).invert()); }
    out.copy(tdBase).multiply(_q.setFromAxisAngle(_x.set(1, 0, 0), tdAngle(t))).multiply(attPad);
    return out;
  }
  if (t < TD.ptc) return fromNoseHead(new THREE.Vector3().crossVectors(up, NRM).normalize(), up.clone().negate(), out);   /* the hatch window on the Earth */
  /* passive thermal control: the nose across the sunlight, rolling slowly */
  const nose = new THREE.Vector3().crossVectors(SUNV, NRM).normalize();
  const h0 = new THREE.Vector3().crossVectors(NRM, nose).normalize().applyAxisAngle(nose, S.roll);
  return fromNoseHead(nose, h0, out);
}
const smooth = (a, b, x) => { const k = Math.min(1, Math.max(0, (x - a) / (b - a))); return k * k * (3 - 2 * k); };
function tdDist(t) {
  if (t < TD.sep) return 0;
  if (t < TD.app0) return 15 * smooth(TD.sep, TD.sepEnd + 20, t);
  if (t < TD.dock) return 15 + (vehicle.DOCK_DIST - 15) * smooth(TD.app0, TD.dock, t);
  return vehicle.DOCK_DIST + (vehicle.HARD_DIST - vehicle.DOCK_DIST) * probeIn(t);       /* the probe draws the two together */
}
/* the probe's retraction after capture: 0 at capture, 1 ring on ring, a second before the latches fire */
const probeIn = t => smooth(TD.dock + 4, TD.hard - 1, t);
function tdAngle(t) { return Math.PI * smooth(TD.pitch0, TD.pitch1, t); }

/* the vehicle's configuration for time t */
const _ms = new THREE.Matrix4(), _ml = new THREE.Matrix4(), _md = new THREE.Matrix4(), _mt = new THREE.Matrix4();
const DOCKED_LM = new THREE.Matrix4();
function configure(t) {
  const td = { j2: t > GET.tli && t < GET.tliEnd ? Math.min(1, (t - GET.tli) / 2) : 0 };
  if (t < TD.sep) { td.sivb = _ms.identity(); td.lm = vehicle.LM_REST; td.open = 0; td.gone = 0; }
  else if (t < TD.extract) {
    vehicle.csmInSivb(tdDist(t), tdAngle(t), _md); _ms.copy(_md).invert();
    td.sivb = _ms; td.lm = _ml.multiplyMatrices(_ms, vehicle.LM_REST); td.open = smooth(TD.sep, TD.sep + 3, t); td.gone = t - (TD.sep + 4);
  } else {
    vehicle.csmInSivb(vehicle.HARD_DIST, Math.PI, _md); _ms.copy(_md).invert();
    DOCKED_LM.multiplyMatrices(_ms, vehicle.LM_REST);
    const d = t - TD.extract, s = d < 30 ? 0.04 * d * d : 36 + 2.4 * (d - 30);          /* springs, then the S-IVB's own push away */
    td.sivb = s > 8000 ? null : _ms.multiply(_mt.makeTranslation(0, -s, 0));
    td.lm = DOCKED_LM; td.open = 1; td.gone = t - (TD.sep + 4);
  }
  vehicle.space(td);
  vehicle.setProbe(t < TD.dock ? 1 : 1 - probeIn(t));
  return td;
}

/* ---------------------------------------------------------------- outside view, hands (as in launch.js) */
const _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3(), _up = new THREE.Vector3(), _tg = new THREE.Vector3();
function placeOutside(w) {
  _up.fromArray(w.up); _h1.fromArray(w.v).addScaledVector(_up, -_h1.dot(_up)).normalize(); _h2.crossVectors(_up, _h1).normalize();
  _tg.set(0, 0.4, 0).applyQuaternion(attRel);
  rig.position.copy(_h1).multiplyScalar(Math.cos(OUT.az)).addScaledVector(_h2, Math.sin(OUT.az)).multiplyScalar(Math.cos(OUT.el)).addScaledVector(_up, Math.sin(OUT.el)).multiplyScalar(OUT.dist).add(_tg);
  _z.copy(rig.position).sub(_tg).normalize(); _x.crossVectors(_up, _z).normalize(); _y.crossVectors(_z, _x);
  rig.quaternion.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
function updateOutside(dt) {
  if (OUT.want !== OUT.on) { FADE.target = 1; if (FADE.k > 0.9) { OUT.on = OUT.want; FADE.target = 0; } }
  else if (FADE.target === 1 && FADE.k > 0.9) FADE.target = 0;
  if (!OUT.on) return;
  let ax = 0, ay = 0, ey = 0;
  if (renderer.xr.isPresenting) { ax = STICK.x; ay = STICK.y; ey = STICK.ry; }
  else { ax = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); ey = (keys.KeyI ? -1 : 0) + (keys.KeyK ? 1 : 0); ay = (keys.KeyM ? 1 : 0) - (keys.KeyU ? 1 : 0); }
  const dz = v => Math.abs(v) < 0.15 ? 0 : v;
  OUT.az += dz(ax) * 1.1 * dt; OUT.dist = Math.min(600, Math.max(7, OUT.dist * Math.exp(dz(ay) * 1.4 * dt))); OUT.el = Math.min(1.45, Math.max(-1.3, OUT.el - dz(ey) * 0.9 * dt));
}
const hands = { left: createHand('left'), right: createHand('right') };
const SIGN = { k: 0, t: 0 };
const sign = new THREE.Mesh(new THREE.PlaneGeometry(0.12, 0.045), new THREE.MeshBasicMaterial({ transparent: true, depthTest: false, map: (() => {
  const c = document.createElement('canvas'); c.width = 256; c.height = 96; const x = c.getContext('2d');
  x.fillStyle = 'rgba(150,20,14,0.92)'; x.beginPath(); x.roundRect(4, 4, 248, 88, 16); x.fill();
  x.fillStyle = '#fff'; x.font = 'bold 46px Arial'; x.textAlign = 'center'; x.fillText('Not now', 128, 64);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; })() }));
sign.renderOrder = 1000; sign.visible = false; scene.add(sign);
const _tip = new THREE.Vector3(), _loc = new THREE.Vector3();
function updateHands(dt) {
  const s = renderer.xr.getSession(); if (!s) return;
  let touched = null;
  for (const h of ['left', 'right']) {
    const src = ctl.src[h]; if (!src || !src.gamepad) continue;
    const m = hands[h], grip = ctl.grips[h]; if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
    const onCrew = !OUT.on && handOnCrew(m, [cabin.life], gp, who => chat && chat.touch(who));
    m.userData.tip.getWorldPosition(_tip);
    let hit = false;
    if (!OUT.on) for (const pnl of cabin.panels) { _loc.copy(_tip); pnl.mesh.worldToLocal(_loc); if (Math.abs(_loc.x) < pnl.w / 2 && Math.abs(_loc.y) < pnl.h / 2 && Math.abs(_loc.z) < 0.03) hit = true; }
    m.userData.warn = hit || onCrew ? 0.4 : Math.max(0, (m.userData.warn || 0) - dt); m.userData.setTint(m.userData.warn > 0);
    if (hit) { touched = _tip.clone(); if (!m.userData.buzzed) { gp.hapticActuators?.[0]?.pulse?.(0.5, 60).catch?.(() => {}); m.userData.buzzed = true; } } else m.userData.buzzed = false;
  }
  if (touched) { SIGN.k = 1; SIGN.t = 1.4; _loc.setFromMatrixPosition(camera.matrixWorld); sign.position.copy(touched).lerp(_loc, 0.25); sign.quaternion.copy(camera.getWorldQuaternion(_q)); sign.translateY(0.05); }
  else { SIGN.t -= dt; if (SIGN.t < 0) SIGN.k = Math.max(0, SIGN.k - dt * 3); }
  sign.material.opacity = SIGN.k;
}

/* ---------------------------------------------------------------- rendering */
function setPlanes(n, f) {
  const fix = c => { const e = c.projectionMatrix.elements; e[10] = -(f + n) / (f - n); e[14] = -2 * f * n / (f - n); c.projectionMatrixInverse.copy(c.projectionMatrix).invert(); };
  if (renderer.xr.isPresenting) { const xc = renderer.xr.getCamera(); fix(xc); xc.cameras.forEach(fix); }
  else { camera.near = n; camera.far = f; camera.updateProjectionMatrix(); }
}
let w = where(S.t);
function render() {
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  outside.dome.position.setFromMatrixPosition(camera.matrixWorld);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, f, n) => { cabin.group.visible = c && !OUT.on; wrist.mesh.visible = c && wrist.on; toast.mesh.visible = c && toast.k > 0.01; badge.mesh.visible = c && badge.on; card.mesh.visible = c && card.k > 0.01; outside.far.visible = f; outside.near.visible = n; sign.visible = c && SIGN.k > 0.01; };
  const nearest = Math.min(w.alt, w.dMoon - R_MOON), farthest = Math.max(w.alt + 2 * RE, w.dMoon + R_MOON);
  show(false, true, false); setPlanes(Math.max(20, Math.min(nearest * 0.2, OUT.on ? 2000 : 1e9)), Math.max(6e7, farthest * 1.5)); renderer.render(scene, camera);
  renderer.clearDepth();
  show(false, false, true); setPlanes(0.5, 80000); renderer.render(scene, camera);
  renderer.clearDepth();
  show(true, false, false); setPlanes(0.02, 12); renderer.render(scene, camera);
  if (FADE.k > 0.001) { veil.material.opacity = FADE.k; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t;
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock: warp stops short of a held stretch and drops to real time there, and says so */
  if (S.running && !S.still && !S.ended) {
    S.t = Math.min(END, clock.advance(S, dt));
    if (S.t >= END) { S.ended = true; S.running = false; wrist.show(true); card.show({ title: 'End of chapter 2', sub: 'To the Moon', a: 'Continue: chapter 3, Lunar orbit', b: 'Take a break' }); saveProgress('lunar.html', null, 0, true); }
  }
  w = where(S.t);
  /* the roll of passive thermal control: three turns an hour, but never faster than 1.4 deg/s to look at */
  if (S.t > TD.ptc) S.roll += Math.min(2 * Math.PI * 3 / 3600 * dt * S.warp, 0.025 * dt);
  /* comfort: the attitude as you see it never turns faster than RATE (in low orbit at high warp it would spin the
   * Earth past the windows); docking is exact, as its geometry depends on it */
  attitudeAt(S.t, w, attWant);
  if (S.snap || (S.t > TD.sep - 5 && S.t < TD.view)) { att.copy(attWant); S.snap = false; } else att.rotateTowards(attWant, RATE() * dt);
  attRel.copy(att).multiply(_q.copy(attPad).invert());
  vehicle.group.quaternion.copy(attRel);
  if (OUT.on) placeOutside(w); else { rig.quaternion.copy(att); rig.position.copy(EYE_OFF).applyQuaternion(attRel); }
  outside.update({ pos: w.pos, up: w.up, alt: w.alt, nose: [0, 1, 0], head: [0, 0, -1] }, S.t, { towerJett: -1e9 });
  outside.setSpin(W_EARTH * S.t);
  _x.fromArray(w.moon).sub(_y.fromArray(w.rE));
  outside.setMoon(_x, _y.fromArray(w.moon).negate().normalize(), NRM);
  const td = configure(S.t);
  vehicle.update(-1e9, OUT.on ? 'outside' : 'inside', { alt: w.alt, up: w.up });
  vehicle.space(td);                                   /* again: update() resets the launch configuration */
  /* orbital night: in the Earth's shadow the Sun is gone, outside and in the cabin (a soft edge for the penumbra) */
  _z.fromArray(w.rE);
  const along = _z.dot(SUNV), perp = Math.sqrt(Math.max(0, _z.lengthSq() - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - RE) / 60000 + 0.5));
  vehicle.lights.children.forEach(l => { if (l.isDirectionalLight) l.intensity = 2.6 * lit; });
  _y.copy(SUNV).applyQuaternion(_q.copy(rig.quaternion).invert());
  cabin.setDaylight(lit, _y);
  outside.stars.setDark(1 - lit);
  /* the cabin shakes a little while an engine runs; bangs at separation, capture, latching, the springs */
  const engine = td.j2 || 0;
  cabin.group.position.set((Math.random() - 0.5) * 0.0008 * engine, (Math.random() - 0.5) * 0.0008 * engine, 0);
  sound.drive(engine, S.warp);
  for (const [at, lvl] of [[TD.sep, 0.8], [TD.dock, 0.5], [TD.hard, 0.6], [TD.extract, 0.5]]) if (prevT < at && S.t >= at && S.warp === 1) {
    sound.thump(lvl, 0.4, 700);
    if (at === TD.hard) for (let k = 1; k < 12; k++) setTimeout(() => sound.thump(0.18, 0.08, 1800), k * 90);
  }
  prevT = S.t;
  /* floating things drift, and can be taken */
  floaters.update(dt, { ctl, head, hands, enabled: xr && !OUT.on });
  /* instruments */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  if (now - lastDsky > 500) {
    const d = cabin.dsky;
    if (S.t > GET.tli - 600 && S.t < GET.tliEnd + 60) d.draw('15', '16', '62', d.f(w.speed * 3.281), d.f(0), d.f(w.alt / 1852 * 10));
    else d.draw('00', '16', '44', d.f(Math.min(99999, w.alt / 1852 / 10)), d.f(0), d.f(w.speed * 3.281));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: ['CMP', 'LMP'], where: ['any', 'space', 'earth'], quiet: S.running && !S.ended && !OUT.on && !clock.inHold(S.t) && !(radio && radio.current(S.t)) });
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio) });
  cabin.setBody(OPT.body);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 2. A (N on this screen) goes on to chapter 3, lunar orbit.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress('orbit.html', S.t, now);
  render();
  S.frames++;
  if (window.__orbit.afterFrame) { const f = window.__orbit.afterFrame; window.__orbit.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/orbit.json', import.meta.url).href);
  if (ctx && !chat) chat = await makeChatter(ctx, sound.voice);
}
function readOptions() {
  if ($('start')) setTime(+$('start').value);
  if ($('opt-body')) settings.set('body', $('opt-body').checked);
}
async function enterVR() {
  if (!navigator.xr) return;
  readOptions(); await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(settings.get('fov'));
  common.all();
  card.show({ title: 'Chapter 2', sub: 'To the Moon' }, 6);
  session.addEventListener('selectstart', () => sound.init());          /* sound needs a press when VR was entered by itself */
  setTimeout(() => toast.say('Press the left stick for the status and the settings', 6), 7500);
  S.yaw = 0; S.pitch = 0; head.position.set(0, 0, 0); head.rotation.set(0, 0, 0);
  SEAT.pending = 1; SEAT.done = false;
  document.body.classList.add('invr');
  setTimeout(() => { S.running = true; }, 1500);
  session.addEventListener('end', () => { document.body.classList.remove('invr'); S.running = false; renderer.setSize(innerWidth, innerHeight); });
}
$('vr')?.addEventListener('click', enterVR);
autoEnter(() => S.ready, enterVR);
offerContinue('orbit.html', $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = ascentState(-60); fromNoseHead(V(s0.nose), V(s0.head), attPad); }
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 22); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  if (Q.has('cheat')) cabin.setCheat(true);
  w = where(S.t);
  outside.update({ pos: w.pos, up: w.up, alt: w.alt, nose: [0, 1, 0], head: [0, 0, -1] }, S.t, { towerJett: -1e9 });
  await outside.ready;
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__orbit = { S, OUT, END, renderer, camera, rig, head, cabin, outside, vehicle, hands, SEAT, TD, afterFrame: null, setTime, get radio() { return radio; }, where,
  ctl, wrist, toast, badge, clock, get chat() { return chat; }, floaters, settings, info };
