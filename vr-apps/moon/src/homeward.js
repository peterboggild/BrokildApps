/* Journey to the Moon, chapter 10: the coast home.
 *
 * GET 250:00 to 300:18, in your couch in the Command Module, alone with the Service Module behind it: the Moon
 * small astern, the Earth growing ahead, the ship turning slowly in the sunlight. Time can run up to 10,000 times
 * faster. At GET 254:54, 286,000 km from home, the walk outside (Apollo 17's was made by the Command Module Pilot;
 * here it is yours): the cabin is let down, the hatch opened, and you go hand over hand along the rail to the
 * instrument bay in the Service Module's side, take the film cassette out of the mapping camera and bring it back.
 * The chapter ends 25,000 km above the Earth, 80 minutes before the entry.
 *
 * The flight is home-space.js (chapter 2's ellipse flown backwards). The cabin, the outside (earth.js), the vehicle,
 * the hands, the cheat window and the outside view are chapter 2's, which this was made from.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)  ?cheat  ?eva=s (outside, s metres
 *      along the rail, for pictures) */
import * as THREE from 'three';
import { state as ascentState, SUN, RE } from './ascent.js?v=36';
import { NORMAL, W_EARTH, R_MOON } from './space.js?v=36';
import { where, GETH } from './home-space.js?v=36';
import { makeOutside } from './earth.js?v=36';
import { makeCabin } from './cabin.js?v=36';
import { makeLaunchSound } from './launch-sound.js?v=36';
import { makeRadio, combineRadios } from './radio.js?v=36';
import { makeClock } from './clock.js?v=36';
import { makeChatter } from './chatter.js?v=36';
import { handOnCrew } from './crew-life.js?v=36';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=36';
import { createHand, makeControllers } from './hands.js?v=36';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=36';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=36';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=36';
import { makeFloaters } from './floaters.js?v=36';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);

const START = GETH.start, END = GETH.end, EVA_T = GETH.eva, NEXT = 'index.html';          /* (re-entry is the next chapter to be built) */
const WARPS = [1, 10, 100, 1000, 10000];
/* never warp across the walk outside */
const HOLD_ALL = [[EVA_T - 100, EVA_T + 20, 'the walk outside']];
const holds = () => holdsFor(HOLD_ALL, HOLD_ALL);

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
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, roll: 0, ended: false, snap: true, cues: {} };

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
const wrist = makeWrist(['ff', 'turn', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
const EVENTS = [[EVA_T, 'the walk outside'], [END, 'end of the chapter, 25,000 km from home']];
function info() {
  const out = EVA.state === 'out';
  return { chapter: 'Chapter 10 \u00b7 The coast home', now: phase(S.t), next: S.ended ? null : EVA.state === 'wait' ? ['go out of the hatch (the clock waits)', NaN] : out ? [EVA.has ? 'back to the hatch with the cassette' : 'along the rail to the instrument bay', NaN] : nextOf(EVENTS, S.t), after: 're-entry', get: S.t, warp: S.warp,
    rows: [['To the Earth', fmtDist(w.alt)], ['From the Moon', fmtDist(w.dMoon)], ['Speed', fmtSpeed(w.speed)], ['Fast forward', settings.text('ff')]],
    alert: S.ended ? 'End of chapter 10.  A: back to the chapters' : null,
    keys: out || EVA.state === 'wait' ? [['grip', 'pull yourself along the rail'], ['L stick', 'forward and back along it'], ['A', 'go by itself'], ['grip', 'at the bay: take the cassette'], ['L stick', 'press: panel, settings']]
      : [['A', 'slower (hold: real time)'], ['B', 'faster'], ['X', 'cheat window'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['grip', 'take things'], ['R stick', 'press: re-seat']], fps: S.fps };
}
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
const km = m => m > 1e6 ? (m / 1e6).toFixed(m > 1e8 ? 0 : 1) + 'k km' : (m / 1000).toFixed(0) + ' km';
function phase(t) {
  if (EVA.state === 'out') return EVA.has ? 'outside, the cassette in hand' : 'outside, on the rail';
  if (EVA.state === 'wait') return 'the hatch is open';
  if (t < EVA_T) return 'coasting home';
  return 'coasting home, the film aboard';
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
  if (e.code === 'KeyN' && S.ended) nextChapter(NEXT);
  if (e.code === 'KeyF') evaGo();
  if (e.code === 'KeyG') evaTake();
  const jump = { Digit1: START, Digit2: EVA_T - 60, Digit3: EVA_T + 3600, Digit4: END - 7200, Digit5: END - 300 }[e.code];
  if (jump !== undefined) setTime(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const clock = makeClock({ holds, lines: () => radio ? radio.lines : [], warps: WARPS, cap: 10, toast });
const held = t => !!clock.inHold(t);
function cycleWarp() { if (EVA.state !== 'in' && EVA.state !== 'done') { S.warp = 1; toast.say('Real time while you are outside', 3, true); return; } clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
function setTime(t) { S.t = t; prevT = t; S.warp = 1; S.ended = false; S.snap = true; S.cues = {}; evaReset(t > EVA_T + 30); clock.reset(); }
/* the chapter's end: A (or N on the screen) goes on; a page change must leave VR first */
/* on to the next chapter: the page changes while still in VR, and the next page enters VR by itself where the browser
 * allows it (progress.js autoEnter); a break: out of VR, to this page (Continue will find the place) */
function nextChapter(href) { location.href = href; }
function takeBreak() { const s = renderer.xr.getSession(); if (s) s.end(); card.hide(); }
const STICK = { x: 0, y: 0, ry: 0 };
const pad = { prev: {}, grip: {}, aDown: -1 };
/* In the couch: B (right) one step faster; A one step slower, held: real time at once; X (left) the cheat window;
 * Y outside (both stay till pressed again). When the hatch is open and outside: a grip takes hold (pull yourself
 * along the rail; at the bay it takes the cassette), the left stick goes along the rail, A goes by itself. Left
 * stick press: the panel, then the settings; right stick press: re-seat */
function readControllers(now) {
  const s = renderer.xr.getSession(); if (!s) return;
  const walk = EVA.state === 'wait' || EVA.state === 'out';
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed, g = !!gp.buttons[1]?.pressed || (gp.buttons[0]?.value || 0) > 0.6;
    if (g && !pad.grip[k] && EVA.state === 'wait') evaGo();
    if (g && !pad.grip[k] && EVA.state === 'out') evaTake(k);
    pad.grip[k] = g;
    if (k === 'right') {
      if (a && !pad.prev.ra) { if (S.ended) nextChapter(NEXT); else if (walk) evaGo(); else pad.aDown = now; }
      if (a && pad.aDown >= 0 && now - pad.aDown > 600) { realTime(); pad.aDown = -1; }
      if (!a && pad.prev.ra && pad.aDown >= 0) { if (!S.ended) clock.slower(S); pad.aDown = -1; }
      if (b && !pad.prev.rb && S.ended) takeBreak();
      if (b && !pad.prev.rb && !S.ended) cycleWarp();
      if (st && !pad.prev.rs) SEAT.pending = 1;
    } else if (k === 'left') {
      if (a && !pad.prev.la && !walk) cabin.setCheat(!cabin.cheat);       /* X: the cheat window, Y: outside (both stay) */
      if (b && !pad.prev.lb && !walk) OUT.want = !OUT.want;
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

/* ---------------------------------------------------------------- attitude */
const _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion();
const V = a => new THREE.Vector3().fromArray(a);
function fromNoseHead(nose, headV, out) {
  _y.copy(headV).addScaledVector(nose, -headV.dot(nose)).normalize(); _z.copy(nose).negate(); _x.crossVectors(_y, _z).normalize();
  return out.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
const SUNV = V(SUN), NRM = V(NORMAL);
/* the attitude that puts cabin direction `c` on world direction `d`, with your head as near `h` as that allows */
const _ma = new THREE.Matrix4(), _mb = new THREE.Matrix4(), _p = new THREE.Vector3(), _r = new THREE.Vector3();
function aim(c, d, h, out) {
  _p.set(0, 1, 0).addScaledVector(c, -c.y).normalize(); _r.crossVectors(c, _p);
  _ma.makeBasis(c, _p, _r);
  _y.copy(h).addScaledVector(d, -h.dot(d)).normalize(); _x.crossVectors(d, _y);
  _mb.makeBasis(d, _y, _x);
  return out.setFromRotationMatrix(_mb.multiply(_ma.transpose()));
}
const HATCH_RIG = new THREE.Vector3();          /* the hatch's side of the ship, in your couch's frame (set at the start) */
function attitudeAt(t, w, out) {
  /* for the walk outside: the ship held still, the hatch's side and the instrument bay in the sunlight */
  if (t > EVA_T - 300 && EVA.state !== 'done') return aim(HATCH_RIG, SUNV, NRM, out);
  /* passive thermal control: the nose across the sunlight, rolling slowly */
  const nose = new THREE.Vector3().crossVectors(SUNV, NRM).normalize();
  const h0 = new THREE.Vector3().crossVectors(NRM, nose).normalize().applyAxisAngle(nose, S.roll);
  return fromNoseHead(nose, h0, out);
}

/* the vehicle: the Command and Service Modules alone (Challenger was cast off in chapter 9) */
const _mt = new THREE.Matrix4().makeTranslation(0, -1e7, 0);
function configure() { const td = { sivb: null, lm: _mt, open: 1, gone: 1e6, sps: 0, j2: 0 }; vehicle.space(td); vehicle.setProbe(0); return td; }

/* ---------------------------------------------------------------- the walk outside
 * The CSM's frame: +y the nose, the hatch on the -x side of the Command Module's cone, the Service Module a
 * cylinder 1.96 m in radius from y -1 down to y -5. The rail runs from the hatch down the cone and along the
 * Service Module to the instrument bay, a twelfth of the way round from the hatch (clear of the thruster quad that
 * stands under it; the real bay was a sixth of the way round). You float 0.8 m off the hull, an arm's length from the rail, facing it, your head towards the nose. EVA.s:
 * metres along the rail from the hatch. */
const BAY_AZ = Math.PI / 6, _ay = new THREE.Vector3(0, 1, 0);
const RAIL = [new THREE.Vector3(-1.62, 0.9, 0), new THREE.Vector3(-2.75, -0.95, 0).applyAxisAngle(_ay, BAY_AZ * 0.6), new THREE.Vector3(-2.75, -3.0, 0.12).applyAxisAngle(_ay, BAY_AZ)];
if (Q.has('off')) for (const p of RAIL) { const r = Math.hypot(p.x, p.z), k = (1.96 + num('off', 0.8)) / r; p.x *= k; p.z *= k; }          /* test aid: further off, for pictures */
const RAIL_L = [RAIL[0].distanceTo(RAIL[1]), RAIL[1].distanceTo(RAIL[2])], RAIL_LEN = RAIL_L[0] + RAIL_L[1];
const railAt = (sv, out) => sv < RAIL_L[0] ? out.copy(RAIL[0]).lerp(RAIL[1], sv / RAIL_L[0]) : out.copy(RAIL[1]).lerp(RAIL[2], Math.min(1, (sv - RAIL_L[0]) / RAIL_L[1]));
const EVA = { state: 'in', s: 0, v: 0, auto: false, has: false, hand: null, hintT: 0, grab: {}, k: 0 };
const bay = new THREE.Group(), cassette = new THREE.Group(), CASS_HOME = new THREE.Vector3(-2.31, -3.0, 0.3);
{ const M = (c, m = 0.3, r = 0.6) => new THREE.MeshStandardMaterial({ color: c, metalness: m, roughness: r });
  const box = (w, h, d, x, y, z, mat, to = bay) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat); b.position.set(x, y, z); to.add(b); return b; };
  const rod = (a, b, mat, r = 0.014) => { const l = a.distanceTo(b), m = new THREE.Mesh(new THREE.CylinderGeometry(r, r, l, 8), mat); m.position.copy(a).lerp(b, 0.5); m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize()); bay.add(m); };
  const dark = M(0x4b4e52, 0.4, 0.7), alu = M(0x9a9da1, 0.5, 0.6), gold = M(0xc79a3a, 0.9, 0.35), white = M(0xe9e8e2, 0.1, 0.7), black = M(0x0c0c0d, 0.2, 0.8);
  /* the bay: a door's width of the Service Module's side laid open, the instruments in it */
  box(0.06, 2.3, 1.5, -1.955, -3.0, 0, dark);
  box(0.3, 0.7, 0.55, -2.05, -2.45, -0.38, alu); box(0.26, 0.5, 0.5, -2.06, -3.55, -0.35, gold);       /* the panoramic camera, the mass spectrometer's housing */
  box(0.2, 0.75, 0.5, -2.07, -3.1, 0.3, alu);                                                         /* the mapping camera, its cassette standing out of it */
  for (const z of [-0.75, 0.75]) box(0.05, 2.3, 0.04, -1.99, -3.0, z, alu);
  for (const y of [-1.86, -4.14]) box(0.05, 0.04, 1.5, -1.99, y, 0, alu);
  /* the rail: down the cone from the hatch, along the Service Module, standing off on short posts; a foot restraint
   * by the bay */
  const pts = [new THREE.Vector3(-0.98, 0.78, -0.42), new THREE.Vector3(-1.99, -0.92, 0.15), new THREE.Vector3(-2.02, -1.9, 0.42)];
  rod(pts[0], pts[1], gold); rod(pts[1], pts[2], gold);
  for (const [p, q] of [[pts[0], new THREE.Vector3(-0.9, 0.78, -0.39)], [pts[1], new THREE.Vector3(-1.93, -0.98, 0.15)], [pts[2], new THREE.Vector3(-1.96, -1.9, 0.42)]]) rod(p, q, gold, 0.01);
  rod(new THREE.Vector3(-2.02, -2.0, -0.55), new THREE.Vector3(-2.02, -4.0, -0.55), gold);
  box(0.03, 0.3, 0.36, -2.0, -4.3, 0, gold);
  /* the cassette: a white drum with a handle and a tether ring, as big as a hat box */
  const drum = new THREE.Mesh(new THREE.CylinderGeometry(0.13, 0.13, 0.3, 20), white); drum.rotation.z = Math.PI / 2; cassette.add(drum);
  const band = new THREE.Mesh(new THREE.CylinderGeometry(0.133, 0.133, 0.05, 20), M(0xb3202a, 0.1, 0.7)); band.rotation.z = Math.PI / 2; cassette.add(band);
  const hd = new THREE.Mesh(new THREE.TorusGeometry(0.07, 0.012, 6, 16, Math.PI), alu); hd.position.set(-0.15, 0, 0); hd.rotation.z = Math.PI / 2; cassette.add(hd);
  cassette.position.copy(CASS_HOME); bay.add(cassette); }
bay.rotation.y = BAY_AZ; vehicle.parts.csm.add(bay);
function evaReset(done) { EVA.state = done ? 'done' : 'in'; EVA.s = 0; EVA.v = 0; EVA.auto = false; EVA.has = done; EVA.hand = null; EVA.hintT = 0; EVA.grab = {}; bay.add(cassette); cassette.position.copy(CASS_HOME); cassette.rotation.set(0, 0, 0); cassette.visible = !done; FADE.target = 0; }
/* A, a grip or the stick: out of the hatch; outside, A goes by itself (to the bay, and back when you have the film) */
function evaGo() { if (EVA.state === 'wait') { EVA.state = 'leaving'; FADE.target = 1; card.hide(); } else if (EVA.state === 'out') EVA.auto = !EVA.auto; }
/* a grip by the cassette takes it (k: which hand; none: the right) */
const _cw = new THREE.Vector3(), _hw2 = new THREE.Vector3();
function evaTake(k) {
  if (EVA.state !== 'out' || EVA.has || EVA.s < RAIL_LEN - 0.9) return false;
  const hand = k || 'right', g = ctl.grips[hand];
  if (k && renderer.xr.isPresenting) { cassette.getWorldPosition(_cw); g.getWorldPosition(_hw2); if (_cw.distanceTo(_hw2) > 0.75) return false; }
  EVA.has = true; EVA.hand = hand; cue('got'); sound.thump(0.2, 0.1, 1200);
  if (renderer.xr.isPresenting) { g.add(cassette); cassette.position.set(0, -0.02, -0.16); cassette.rotation.set(0, 0, 0); } else cassette.visible = false;
  return true;
}
const _rp = new THREE.Vector3(), _rq = new THREE.Quaternion(), _rz = new THREE.Vector3(), _rx = new THREE.Vector3(), _rm = new THREE.Matrix4();
/* facing the hull (your back to the axis's outward line), your head towards the nose */
function placeEVA() {
  railAt(EVA.s, _rp); _rz.set(_rp.x, 0, _rp.z).normalize(); _rx.crossVectors(_ay, _rz);
  _rq.setFromRotationMatrix(_rm.makeBasis(_rx, _ay, _rz));
  rig.position.copy(_rp).applyQuaternion(attRel); rig.quaternion.copy(attRel).multiply(_rq);
}
function updateEVA(dt, xr) {
  /* the time has come: the cabin is let down, the hatch opened; the clock waits for you */
  if (EVA.state === 'in' && S.t >= EVA_T && S.t < EVA_T + 30 && S.running) { EVA.state = 'wait'; EVA.hintT = 0; cue('hatch'); sound.thump(0.3, 0.3, 500); if (OUT.want) OUT.want = false; }
  if (EVA.state === 'wait') {
    EVA.hintT += dt;
    if (xr && STICK.y < -0.4) evaGo();
    if (EVA.hintT > 5) { EVA.hintT = -15; EVA.hints = (EVA.hints || 0) + 1; toast.say('The walk outside: A, a grip or the left stick forward takes you out of the hatch', 8, true);
      if (EVA.hints >= 2) card.show({ title: 'Out of the hatch, to the instrument bay', sub: 'A  \u00b7  or a grip  \u00b7  or the left stick forward' }, 12); }
  }
  if (EVA.state === 'leaving' && FADE.k > 0.92) { EVA.state = 'out'; EVA.s = 0; EVA.v = 0; FADE.target = 0; cabin.setCheat(false); cue('out'); if (xr) seat(); }
  if (EVA.state === 'entering' && FADE.k > 0.92) { EVA.state = 'done'; FADE.target = 0; cassette.visible = false; cue('in'); sound.thump(0.4, 0.3, 500); if (xr) SEAT.pending = 1; S.snap = true; }
  if (EVA.state !== 'out') return;
  /* along the rail: a gripping hand stays where it took hold (you move the other way); the stick; or by itself */
  let moved = false;
  if (xr) for (const k of ['left', 'right']) {
    const g = ctl.grips[k]; if (!ctl.src[k]) continue;
    const along = -g.position.y;                                                    /* (the rail runs down your own upright) */
    if (pad.grip[k] && !(EVA.has && EVA.hand === k)) { if (EVA.grab[k] === undefined) EVA.grab[k] = along + EVA.s; else { EVA.s = EVA.grab[k] - along; moved = true; EVA.auto = false; } } else EVA.grab[k] = undefined;
  }
  const push = xr ? -STICK.y : (keys.ArrowUp ? 1 : 0) - (keys.ArrowDown ? 1 : 0);
  if (!moved) {
    if (Math.abs(push) > 0.15) { EVA.v += (push * 0.45 - EVA.v) * Math.min(1, dt * 3); EVA.auto = false; }
    else if (EVA.auto) { const to = EVA.has ? 0 : RAIL_LEN, d = to - EVA.s; EVA.v += (Math.sign(d) * Math.min(0.3, Math.abs(d) + 0.04) - EVA.v) * Math.min(1, dt * 2); if (!EVA.has && Math.abs(d) < 0.05) { EVA.v = 0; evaTake(); } }
    else EVA.v *= Math.exp(-1.5 * dt);
    EVA.s += EVA.v * dt;
  }
  EVA.s = Math.max(0, Math.min(RAIL_LEN, EVA.s));
  if (EVA.s > RAIL_LEN - 0.6) { cue('bay'); if (!EVA.has && !EVA.auto) { EVA.hintT += dt; if (EVA.hintT > 4) { EVA.hintT = -12; toast.say('The cassette: a grip by it takes it (A does it for you)', 7, true); } } }
  /* back at the hatch with the film: in. Without it: you are told */
  if (EVA.s < 0.04 && EVA.k > 0.5) { if (EVA.has) { EVA.state = 'entering'; FADE.target = 1; } else if (!EVA.told) { EVA.told = true; toast.say('The film cassette is still in the bay: down the rail to the Service Module', 7, true); } }
  if (EVA.s > 0.5) { EVA.k = 1; EVA.told = false; }
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
    const gp = src.gamepad, b = k => gp.buttons[k] || null, outside1 = EVA.state === 'out';
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
    const onCrew = !OUT.on && !outside1 && handOnCrew(m, [cabin.life], gp, who => chat && chat.touch(who));
    m.userData.tip.getWorldPosition(_tip);
    let hit = false;
    if (!OUT.on && !outside1) for (const pnl of cabin.panels) { _loc.copy(_tip); pnl.mesh.worldToLocal(_loc); if (Math.abs(_loc.x) < pnl.w / 2 && Math.abs(_loc.y) < pnl.h / 2 && Math.abs(_loc.z) < 0.03) hit = true; }
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
    if (recentred() && SEAT.done && !SEAT.pending && EVA.state !== 'out') SEAT.pending = 1;     /* the headset's own "reset view": seated again where you belong */
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  outside.dome.position.setFromMatrixPosition(camera.matrixWorld);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const out1 = EVA.state === 'out';
  const show = (c, f, n) => { cabin.group.visible = c && !OUT.on && !out1; for (const h of ['left', 'right']) hands[h].visible = hands[h].visible && (!out1 || n || c); wrist.mesh.visible = c && wrist.on; toast.mesh.visible = c && toast.k > 0.01; badge.mesh.visible = c && badge.on; card.mesh.visible = c && card.k > 0.01; outside.far.visible = f; outside.near.visible = n; sign.visible = c && SIGN.k > 0.01; };
  const nearest = Math.min(w.alt, w.dMoon - R_MOON), farthest = Math.max(w.alt + 2 * RE, w.dMoon + R_MOON);
  show(false, true, false); setPlanes(Math.max(20, Math.min(nearest * 0.2, OUT.on || out1 ? 2000 : 1e9)), Math.max(6e7, farthest * 1.5)); renderer.render(scene, camera);
  renderer.clearDepth();
  show(false, false, true); setPlanes(out1 ? 0.05 : 0.5, out1 ? 8000 : 80000); renderer.render(scene, camera);
  renderer.clearDepth();
  show(true, false, false); setPlanes(0.02, 12); renderer.render(scene, camera);
  if (FADE.k > 0.001) { veil.material.opacity = FADE.k; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t;
const cue = k => { if (S.cues[k]) return; S.cues[k] = true; if (radio) radio.cue(k); };
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock: warp stops short of the walk outside and drops to real time; while the hatch stands open it waits */
  if (S.running && !S.still && !S.ended) {
    if (EVA.state !== 'in' && EVA.state !== 'done') S.warp = 1;
    if (EVA.state !== 'wait') S.t = Math.min(END, clock.advance(S, dt));
    if (EVA.state === 'in' && prevT < EVA_T && S.t > EVA_T) S.t = EVA_T;          /* (not past it) */
    if (S.t >= END) { S.ended = true; S.running = false; wrist.show(true); card.show({ title: 'End of chapter 10', sub: 'The Earth fills the window. Re-entry comes next', a: 'Back to the chapters', b: 'Take a break' }); saveProgress('index.html', null, 0, true); }
  }
  updateEVA(dt, xr);
  w = where(S.t);
  /* the roll of passive thermal control: three turns an hour, but never faster than 1.4 deg/s to look at */
  S.roll += Math.min(2 * Math.PI * 3 / 3600 * dt * S.warp, 0.025 * dt);
  /* comfort: the attitude as you see it never turns faster than RATE */
  attitudeAt(S.t, w, attWant);
  if (S.snap) { att.copy(attWant); S.snap = false; } else att.rotateTowards(attWant, RATE() * dt);
  attRel.copy(att).multiply(_q.copy(attPad).invert());
  vehicle.group.quaternion.copy(attRel);
  if (EVA.state === 'out') placeEVA(); else if (OUT.on) placeOutside(w); else { rig.quaternion.copy(att); rig.position.copy(EYE_OFF).applyQuaternion(attRel); }
  outside.update({ pos: w.pos, up: w.up, alt: w.alt, nose: [0, 1, 0], head: [0, 0, -1] }, S.t, { towerJett: -1e9 });
  outside.setSpin(W_EARTH * S.t);
  _x.fromArray(w.moon).sub(_y.fromArray(w.rE));
  outside.setMoon(_x, _y.fromArray(w.moon).negate().normalize(), NRM);
  const td = configure();
  vehicle.update(-1e9, OUT.on || EVA.state === 'out' ? 'outside' : 'inside', { alt: w.alt, up: w.up });
  vehicle.space(td);                                   /* again: update() resets the launch configuration */
  vehicle.lm.visible = false;
  /* orbital night: in the Earth's shadow the Sun is gone, outside and in the cabin (a soft edge for the penumbra) */
  _z.fromArray(w.rE);
  const along = _z.dot(SUNV), perp = Math.sqrt(Math.max(0, _z.lengthSq() - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - RE) / 60000 + 0.5));
  vehicle.lights.children.forEach(l => { if (l.isDirectionalLight) l.intensity = 2.6 * lit; });
  _y.copy(SUNV).applyQuaternion(_q.copy(rig.quaternion).invert());
  cabin.setDaylight(lit, _y);
  outside.stars.setDark(1 - lit);
  sound.drive(0, S.warp);
  prevT = S.t;
  /* floating things drift, and can be taken */
  floaters.update(dt, { ctl, head, hands, enabled: xr && !OUT.on && EVA.state !== 'out' });
  /* instruments */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  if (now - lastDsky > 500) {
    const d = cabin.dsky;
    d.draw('00', '16', '44', d.f(Math.min(99999, w.alt / 1852 / 10)), d.f(0), d.f(w.speed * 3.281));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: ['CMP', 'LMP'], where: ['any', 'space', 'earth'], quiet: S.running && !S.ended && !OUT.on && (EVA.state === 'in' || EVA.state === 'done') && !clock.inHold(S.t) && !(radio && radio.current(S.t)) });
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio) });
  cabin.setBody(OPT.body);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 10. A (N on this screen): back to the chapters.' : EVA.state === 'wait' ? 'The hatch is open: F to go out. Outside: arrow keys along the rail, F by itself, G takes the cassette.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress('homeward.html', EVA.state === 'in' || EVA.state === 'done' ? S.t : EVA_T - 30, now);
  render();
  S.frames++;
  if (window.__home.afterFrame) { const f = window.__home.afterFrame; window.__home.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/homeward.json', import.meta.url).href);
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
  card.show({ title: 'Chapter 10', sub: 'The coast home' }, 6);
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
offerContinue('homeward.html', $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = ascentState(-60); fromNoseHead(V(s0.nose), V(s0.head), attPad); HATCH_RIG.set(-1, 0, 0).applyQuaternion(attPad).normalize(); }
  if (Q.has('eva')) { EVA.state = 'out'; EVA.s = num('eva', 0); S.t = Math.max(S.t, EVA_T); }
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

window.__home = { S, OUT, START, END, EVA_T, EVA, RAIL_LEN, bay, cassette, renderer, camera, rig, head, cabin, outside, vehicle, hands, SEAT, afterFrame: null, setTime, get radio() { return radio; }, where, FADE, evaGo, evaTake, att,
  ctl, wrist, toast, badge, clock, get chat() { return chat; }, floaters, settings, info };
