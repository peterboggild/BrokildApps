/* Journey to the Moon, chapter 3: lunar orbit.
 *
 * From the edge of lunar space (GET 71:52, where chapter 2 ends) to the first low pass of the descent orbit
 * (GET 94:20): the long fall towards the Moon, loss of signal, lunar orbit insertion behind the Moon (the main engine
 * for 6 min 33 s), Earthrise as you come round, two orbits with Taurus-Littrow passing below, descent orbit
 * insertion behind the Moon, and the pass at 15 km over the landing site.
 *
 * The flight is lunar-space.js, in the Moon's site frame of stage 0, so the Moon is stage 0's (moon.js) as it is,
 * with the Sun turned back from where it stands at the landing. The Earth is extras.js's sky, with real imagery. The
 * cabin, the vehicle (CSM with the LM on its nose), the hands, the cheat window and the outside view are chapter 2's.
 *
 * Comfort: the spacecraft's attitude never turns faster than 1.5 degrees a second as you see it, whatever the time
 * warp; at high warp it holds still in space while the Moon turns below, and catches up when time slows.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)  ?cheat */
import * as THREE from 'three';
import { state as ascentState } from './ascent.js?v=31';
import { loadMoon } from './moon.js?v=4';
import { makeSky } from './extras.js?v=5';
import { makeLunarFlight, GETL } from './lunar-space.js?v=31';
import { makeCabin } from './cabin.js?v=31';
import { makeLaunchSound } from './launch-sound.js?v=31';
import { makeRadio, combineRadios } from './radio.js?v=31';
import { makeClock } from './clock.js?v=31';
import { makeChatter } from './chatter.js?v=31';
import { handOnCrew } from './crew-life.js?v=31';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=31';
import { createHand, makeControllers } from './hands.js?v=31';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=31';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=31';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=31';
import { makeFloaters } from './floaters.js?v=31';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;

const START = 258760, END = 339600;
const WARPS = [1, 10, 100, 1000, 10000];
const W_EARTH = 7.2921159e-5;
/* stretches run at real time (or x10 at most): the burns with the turns round them, Earthrise, the low pass */
const HOLD_ALL = [[319650, 320520, 'the LOI burn'], [321860, 322120, 'Earthrise'], [334900, 335420, 'the DOI burn'], [338560, 338950, 'the low pass']];
const HOLD_MAJOR = HOLD_ALL.slice(0, 2);
const holds = () => holdsFor(HOLD_ALL, HOLD_MAJOR);
const RATE = () => settings.get('turn') * Math.PI / 180;     /* the fastest the attitude turns as you see it, rad/s */

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
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, ended: false, snap: true };

const cabin = makeCabin();
rig.add(cabin.group);
cabin.setHelmets(false); cabin.setCovers(false);
const vehicle = makeVehicle(renderer);
const near = new THREE.Group(); scene.add(near);
near.add(vehicle.group, vehicle.lights);
const vSun = vehicle.lights.children.find(l => l.isDirectionalLight), vHemi = vehicle.lights.children.find(l => l.isHemisphereLight);
const attPad = new THREE.Quaternion(), attRel = new THREE.Quaternion(), att = new THREE.Quaternion(), attWant = new THREE.Quaternion();
const OUT = { want: false, on: false, az: -1.25, el: 0.25, dist: 26 };
const OPT = { get body() { return settings.get('body'); } };
const sound = makeLaunchSound();
let chat = null, radio = null, moon = null, sky = null, L = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };

/* things floating free in the cabin, yours to take (floaters.js) */
const floaters = makeFloaters(cabin.group);
const ctl = makeControllers(renderer, head);
const common = applyCommon(renderer, sound);

/* ---------------------------------------------------------------- helpers for the panel and the page */
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
const km = m => m > 1e6 ? (m / 1e6).toFixed(1) + 'k km' : (m / 1000).toFixed(0) + ' km';
function phase(t) {
  if (!L) return '';
  if (t < L.EV.loi) return 'approaching the Moon';
  if (t < L.EV.loiEnd) return 'LOI burn';
  if (t < L.EV.doi) return 'lunar orbit';
  if (t < L.EV.doiEnd) return 'DOI burn';
  return 'descent orbit';
}
/* ---------------------------------------------------------------- the wrist panel (X) and messages */
const wrist = makeWrist(['ff', 'turn', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
function info() {
  if (!L || !w) return null;
  const ev = [[L.EV.loi, 'LOI burn, behind the Moon'], [321935, 'Earthrise'], [L.EV.doi, 'DOI burn'], [338719, 'the low pass, 15 km'], [END, 'end of the chapter']];
  return { chapter: 'Chapter 3 \u00b7 Lunar orbit', now: phase(S.t) + (w.hidden ? ', out of touch' : ''), next: S.ended ? null : nextOf(ev, S.t), after: 'chapter 4, into the LM', get: S.t, warp: S.warp,
    rows: [['Above the Moon', fmtDist(w.alt)], ['Speed', fmtSpeed(w.speed)], ['Radio', w.hidden ? 'behind the Moon (LOS)' : 'in touch with Houston'], ['Fast forward', settings.text('ff')]],
    alert: S.ended ? 'End of chapter 3.  A: on to chapter 4' : null,
    keys: [['A', 'slower (hold: real time)'], ['B', 'faster'], ['X', 'cheat window'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['grip', 'take things'], ['R stick', 'press: re-seat']], fps: S.fps };
}

/* ---------------------------------------------------------------- input */
const keys = {};
const JUMPS = { Digit1: START, Digit2: 300000, Digit3: 319560, Digit4: 321840, Digit5: 323760, Digit6: 334700, Digit7: 338480 };
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyS') clock.slower(S);
  if (e.code === 'KeyP') wrist.press();
  if (e.code === 'KeyC') cabin.setCheat(!cabin.cheat);
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter('lm.html');
  if (JUMPS[e.code] !== undefined) setTime(JUMPS[e.code]);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const clock = makeClock({ holds, lines: () => radio ? radio.lines : [], warps: WARPS, cap: 10, toast });
const held = t => !!clock.inHold(t);
function cycleWarp() { clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
function setTime(t) { S.t = t; S.warp = 1; S.ended = false; S.snap = true; clock.reset(); }
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
      if (a && !pad.prev.ra) { pad.aDown = now; if (S.ended) nextChapter('lm.html'); }
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
const SEAT = { pending: 0, done: false }, recentred = watchRecentre(renderer, camera);
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true;
}

/* ---------------------------------------------------------------- attitude (as the rig: -Z the nose, +Y your head) */
const _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion();
const _a = new THREE.Vector3(), _b = new THREE.Vector3(), _c = new THREE.Vector3();
const V = a => new THREE.Vector3().fromArray(a);
function fromNoseHead(nose, headV, out) {
  _y.copy(headV).addScaledVector(nose, -headV.dot(nose)).normalize(); _z.copy(nose).negate(); _x.crossVectors(_y, _z).normalize();
  return out.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
/* the left-hand side window beside you: the line from your eye through its centre, in the cabin (cabin.js: the
 * window at azimuth 180, z -0.25 on the cone round the axis at x 0.6, y -0.15) */
const LEFT_WIN = new THREE.Vector3(-0.863, -0.15, -0.25).normalize();
const _ma = new THREE.Matrix4(), _mb = new THREE.Matrix4(), _p = new THREE.Vector3(), _r = new THREE.Vector3();
/* the attitude that puts cabin direction `c` on world direction `d`, with your head as near `h` as that allows */
function aim(c, d, h, out) {
  _p.set(0, 1, 0).addScaledVector(c, -c.y).normalize(); _r.crossVectors(c, _p);
  _ma.makeBasis(c, _p, _r);
  _y.copy(h).addScaledVector(d, -h.dot(d)).normalize(); _x.crossVectors(d, _y);
  _mb.makeBasis(d, _y, _x);
  return out.setFromRotationMatrix(_mb.multiply(_ma.transpose()));
}
const OFF = 21 * Math.PI / 180, DIP = 4 * Math.PI / 180;
function attitudeAt(t, w, out) {
  const up = V(w.up), vel = V(w.v).normalize(), nrm = V(L.NORMAL);
  /* the burns: the engine forward, against the motion; from 4 min before to 2 min after */
  if ((t > L.EV.loi - 240 && t < L.EV.loiEnd + 120) || (t > L.EV.doi - 240 && t < L.EV.doiEnd + 120)) return fromNoseHead(vel.negate(), up, out);
  /* the approach: the Moon in the window beside you */
  if (t < L.EV.loi) return aim(LEFT_WIN, up.clone().negate(), nrm, out);
  /* in orbit: the Moon below; your window looks ahead along the track, turned towards the Earth's side of the orbit
   * (where the Earth rises) and down to the horizon */
  _a.copy(vel).addScaledVector(up, -vel.dot(up)).normalize().multiplyScalar(Math.cos(OFF)).addScaledVector(nrm, Math.sin(OFF)).normalize();
  const dip = Math.acos(Math.min(1, L.R / w.rC)) + DIP;               /* the horizon's dip, and a little more */
  _a.multiplyScalar(Math.cos(dip)).addScaledVector(up, -Math.sin(dip));
  return aim(LEFT_WIN, _a, up, out);
}
const DOCKED_LM = new THREE.Matrix4();

/* ---------------------------------------------------------------- outside view, hands */
const _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3(), _up = new THREE.Vector3(), _tg = new THREE.Vector3();
function placeOutside(w) {
  _up.fromArray(w.up); _h1.fromArray(w.v).addScaledVector(_up, -_h1.dot(_up)).normalize(); _h2.crossVectors(_up, _h1).normalize();
  _tg.set(0, -2.5, 0).applyQuaternion(attRel).add(vehicle.group.position);
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
  OUT.az += dz(ax) * 1.1 * dt; OUT.dist = Math.min(600, Math.max(9, OUT.dist * Math.exp(dz(ay) * 1.4 * dt))); OUT.el = Math.min(1.45, Math.max(-1.3, OUT.el - dz(ey) * 0.9 * dt));
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
let w = null;
const _cam = new THREE.Vector3();
function render() {
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (recentred() && SEAT.done && !SEAT.pending && true) SEAT.pending = 1;     /* the headset's own "reset view": seated again where you belong */
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  _cam.setFromMatrixPosition(camera.matrixWorld);
  moon.setDetail(_cam, false);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, s, m, n) => { cabin.group.visible = c && !OUT.on; wrist.mesh.visible = c && wrist.on; toast.mesh.visible = c && toast.k > 0.01; badge.mesh.visible = c && badge.on; card.mesh.visible = c && card.k > 0.01; sign.visible = c && SIGN.k > 0.01;
    sky.visible = s; moon.far.visible = m; moon.near.visible = m; near.visible = n; };
  /* the Sun and the Earth (drawn 2,000 km out, at their true sizes), then the Moon over them, the spacecraft, the cabin */
  show(false, true, false, false); setPlanes(1e5, 3e6); renderer.render(scene, camera);
  renderer.clearDepth();
  const d = _cam.distanceTo(_c.fromArray(L.C));
  show(false, false, true, false); setPlanes(Math.max(50, (d - L.R - 6000) * 0.25), d + L.R); renderer.render(scene, camera);
  renderer.clearDepth();
  show(false, false, false, true); setPlanes(0.5, 20000); renderer.render(scene, camera);
  renderer.clearDepth();
  show(true, false, false, false); setPlanes(0.02, 12); renderer.render(scene, camera);
  if (FADE.k > 0.001) { veil.material.opacity = FADE.k; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t;
const _sun = new THREE.Vector3();
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock: warp stops at a held stretch, where it runs at real time or x10, and says so */
  if (S.running && !S.still && !S.ended) {
    S.t = Math.min(END, clock.advance(S, dt));
    if (S.t >= END) { S.ended = true; S.running = false; wrist.show(true); card.show({ title: 'End of chapter 3', sub: 'Lunar orbit', a: 'Continue: chapter 4, Into the LM', b: 'Take a break' }); saveProgress('lm.html', null, 0, true); }
  }
  w = L.where(S.t);
  moon.setSunOffset(L.sunOffset(S.t));
  _sun.copy(moon.shared.uSun.value);
  sky.userData.setSpin(W_EARTH * (S.t - GETL.land) + 2.1);
  /* attitude, never turning faster than RATE as you see it */
  attitudeAt(S.t, w, attWant);
  if (S.snap) { att.copy(attWant); S.snap = false; } else att.rotateTowards(attWant, RATE() * dt);
  attRel.copy(att).multiply(_q.copy(attPad).invert());
  vehicle.group.position.fromArray(w.pos); vehicle.group.quaternion.copy(attRel);
  vehicle.lights.position.copy(vehicle.group.position);
  if (OUT.on) placeOutside(w); else { rig.quaternion.copy(att); rig.position.copy(EYE_OFF).applyQuaternion(attRel).add(vehicle.group.position); }
  vehicle.update(-1e9, OUT.on ? 'outside' : 'inside', { alt: 1e9, up: w.up });
  vehicle.space({ sivb: null, lm: DOCKED_LM, open: 1, gone: 1e6, sps: w.engine, j2: 0 });
  /* sunlight, or the Moon's shadow; the Moon's grey light from below */
  const along = _a.fromArray(w.r).dot(_sun), perp = Math.sqrt(Math.max(0, w.rC * w.rC - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - L.R) / 30000 + 0.5));
  vSun.position.copy(_sun).multiplyScalar(100); vSun.intensity = 2.6 * lit;
  const fill = Math.min(1, Math.pow(L.R / w.rC, 2)) * Math.max(0, -_a.fromArray(w.up).dot(_sun) * -1 + 0.2);
  vHemi.position.fromArray(w.up); vHemi.color.setRGB(0.05, 0.05, 0.06); vHemi.groundColor.setRGB(0.55, 0.53, 0.5); vHemi.intensity = 0.15 + 0.6 * fill;
  _y.copy(_sun).applyQuaternion(_q.copy(rig.quaternion).invert());
  cabin.setDaylight(lit, _y);
  sky.userData.stars.setDark(1 - lit);
  /* the engine: the cabin trembles, the sound */
  cabin.group.position.set((Math.random() - 0.5) * 0.0005 * w.engine, (Math.random() - 0.5) * 0.0005 * w.engine, 0);
  sound.drive(w.engine * 0.55, S.warp);
  for (const at of [L.EV.loi, L.EV.loiEnd, L.EV.doi, L.EV.doiEnd]) if (prevT < at && S.t >= at && S.warp <= 10) sound.thump(0.45, 0.3, 500);
  prevT = S.t;
  floaters.update(dt, { ctl, head, hands, enabled: xr && !OUT.on });
  /* instruments: the main engine program round the burns, else orbit numbers (nautical miles, feet per second) */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  if (now - lastDsky > 500) {
    const d = cabin.dsky, burnNear = Math.abs(S.t - L.EV.loi) < 900 || Math.abs(S.t - L.EV.doi) < 900;
    if (burnNear) d.draw('40', '06', '40', d.f(Math.max(0, (S.t < L.EV.doi ? L.EV.loi : L.EV.doi) - S.t)), d.f(w.speed * 3.281), d.f(0));
    else d.draw('00', '16', '44', d.f(Math.min(99999, w.alt / 1852 * 10)), d.f(0), d.f(w.speed * 3.281));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: ['CMP', 'LMP'], where: ['any', 'space', 'moon'], quiet: S.running && !S.ended && !OUT.on && !clock.inHold(S.t) && !(radio && radio.current(S.t)) });
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio) });
  cabin.setBody(OPT.body);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 3. A (N on this screen) goes on to chapter 4, into the LM.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (w.hidden ? '  · behind the Moon' : '') + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress('lunar.html', S.t, now);
  render();
  S.frames++;
  if (window.__lunar.afterFrame) { const f = window.__lunar.afterFrame; window.__lunar.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/lunar.json', import.meta.url).href);
  if (ctx && !chat) chat = await makeChatter(ctx, sound.voice);
}
function readOptions() {
  if ($('start')) setTime(+$('start').value);
  if ($('opt-body')) settings.set('body', $('opt-body').checked);
  if ($('hz')) settings.set('hz', +$('hz').value);
}
async function enterVR() {
  if (!navigator.xr) return;
  readOptions(); await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(settings.get('fov'));
  common.all();
  card.show({ title: 'Chapter 3', sub: 'Lunar orbit' }, 6);
  session.addEventListener('selectstart', () => sound.init());          /* sound needs a press when VR was entered by itself */
  setTimeout(() => toast.say('Press the left stick for the status and the settings', 6, true), 7500);
  S.yaw = 0; S.pitch = 0; head.position.set(0, 0, 0); head.rotation.set(0, 0, 0);
  SEAT.pending = 1; SEAT.done = false;
  document.body.classList.add('invr');
  setTimeout(() => { S.running = true; }, 1500);
  session.addEventListener('end', () => { document.body.classList.remove('invr'); S.running = false; renderer.setSize(innerWidth, innerHeight); });
}
$('vr')?.addEventListener('click', enterVR);
autoEnter(() => S.ready, enterVR);
offerContinue('lunar.html', $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = ascentState(-60); fromNoseHead(V(s0.nose), V(s0.head), attPad); }
  { const md = new THREE.Matrix4(); vehicle.csmInSivb(vehicle.HARD_DIST, Math.PI, md); vehicle.setProbe(0); DOCKED_LM.copy(md).invert().multiply(vehicle.LM_REST); }
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 26); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  if (Q.has('cheat')) cabin.setCheat(true);
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.shared.uExposure.value = 3.4;
  sky = makeSky(moon);
  scene.add(sky, moon.far, moon.near);
  L = makeLunarFlight(moon.manifest);
  w = L.where(S.t);
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__lunar = { S, OUT, END, HOLD: HOLD_ALL, ctl, wrist, toast, clock, get chat() { return chat; }, floaters, settings, info, renderer, camera, rig, head, cabin, vehicle, hands, SEAT, afterFrame: null, setTime,
  get radio() { return radio; }, get L() { return L; }, get moon() { return moon; }, get sky() { return sky; }, att, attWant };
