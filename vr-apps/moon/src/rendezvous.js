/* Journey to the Moon, chapter 8: the rendezvous and the docking.
 *
 * GET 185:30 to the docking at 187:37:15 and into the Command Module. You stand at the Commander's station of the
 * ascent stage, in orbit, the Lunar Module Pilot beside you. America is a point of light ahead, 220 km off; run the
 * clock forward and it grows to a ship, comes to a stop before your windows nose on, swings up over your head and
 * comes down on to the drogue: the latches fire. Then the tunnel is opened from above, and you go up through it,
 * hand over hand (grip and pull; or the left stick; or A to float), to your couch in the Command Module, the samples
 * following. It is chapter 4 run the other way, and is made from it.
 *
 * Frames: stage 0's site frame and Moon. The pair fly a circular orbit 113 km up in the plane of the earlier
 * chapters, the LM upright with its windows ahead; the CSM is placed relative to it (csmRel). The Command Module's
 * cabin, the tunnel and the LM's cabin are drawn last, as in chapter 4.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)
 *      ?float=z (floating on the tunnel's axis at cabin z, for pictures) */
import * as THREE from 'three';
import { state as ascentState } from './ascent.js?v=34';
import { loadMoon } from './moon.js?v=4';
import { makeSky } from './extras.js?v=5';
import { makeLunarFlight, GETL, MU_M } from './lunar-space.js?v=34';
import { makeCabin } from './cabin.js?v=34';
import { makeLMCabin, LMC } from './lm-cabin.js?v=34';
import { LM } from './lm-model.js?v=34';
const AS_Y = LM.AS_Y;
import { makeLaunchSound } from './launch-sound.js?v=34';
import { makeRadio, combineRadios } from './radio.js?v=34';
import { makeClock } from './clock.js?v=34';
import { makeChatter } from './chatter.js?v=34';
import { handOnCrew } from './crew-life.js?v=34';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=34';
import { createHand, makeControllers } from './hands.js?v=34';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=34';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=34';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=34';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;

const START = 185 * 3600 + 30 * 60, DOCK = 187 * 3600 + 37 * 60 + 15, OPEN = DOCK + 40;       /* the tunnel is opened from above */
const NEXT = 'index.html';                   /* (leaving the Moon is the next chapter to be built) */
const WARPS = [1, 10, 100, 1000];
const HOLD_ALL = [[DOCK - 420, DOCK + 45, 'the docking']];
const holds = () => holdsFor(HOLD_ALL, HOLD_ALL);
const W_EARTH = 7.2921159e-5;
const RATE = 1.5 * Math.PI / 180;
/* the cabin's cone (cabin.js): the axis at x 0.6, y -0.15 along -Z; the tunnel end at the apex */
const AX = 0.6, AY = -0.15, TANA = Math.tan(33 * Math.PI / 180), Z_APEX = 0.5 - (1.95 - 0.36) / TANA;
const rAt = z => 1.95 - (0.5 - z) * TANA;

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
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, ended: false, mode: 'lm', auto: false, seatedAt: -1, hardK: 1, hintT: 0, cues: {} };

/* ---------------------------------------------------------------- the two spacecraft and their insides */
const vehicle = makeVehicle(renderer);
const near = new THREE.Group(); scene.add(near);
near.add(vehicle.group, vehicle.lights);
vehicle.lm.userData.setLegs(true);
/* the ascent stage alone: the model cut at the joint (a world plane set every frame from where it is), without the
 * legs, the porch and the deflectors, its engine's bell whole (as chapter 7) */
renderer.localClippingEnabled = true;
const lmUp = vehicle.lm, clipUp = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), _cn = new THREE.Vector3(), _cp = new THREE.Vector3();
{ lmUp.userData.ascentOnly.traverse(o => { if (o.isMesh) { o.material = o.material.clone(); o.userData.whole = true; } });
  lmUp.traverse(o => { if (o.isMesh && !o.userData.whole) { o.material = o.material.clone(); o.material.clippingPlanes = [clipUp]; } });
  lmUp.userData.descentTop.visible = false; }
function setClips() {
  lmUp.userData.deployed.visible = false; lmUp.updateMatrixWorld(true);
  _cn.set(0, 1, 0).transformDirection(lmUp.matrixWorld);
  clipUp.setFromNormalAndCoplanarPoint(_cn, lmUp.localToWorld(_cp.set(0, AS_Y - 0.01, 0)));
}
/* America far off: a point of light, until the ship itself is more than a point */
const star = new THREE.Points(new THREE.BufferGeometry().setAttribute('position', new THREE.BufferAttribute(new Float32Array(3), 3)),
  new THREE.PointsMaterial({ color: 0xffffff, size: 6, sizeAttenuation: false, transparent: true, depthWrite: false, toneMapped: false }));          /* (bold: the window's glass and the eye's distance take most of it) */
star.frustumCulled = false; near.add(star);
const vSun = vehicle.lights.children.find(l => l.isDirectionalLight), vHemi = vehicle.lights.children.find(l => l.isHemisphereLight);
const cabin = makeCabin();
cabin.setHelmets(false); cabin.setCovers(false); cabin.setTunnelOpen(true); cabin.setBody(false);
if (cabin.crewmen[1]) cabin.crewmen[1].visible = false;                 /* the LMP comes up after you, with the samples */
const cabinHolder = new THREE.Group(); cabinHolder.matrixAutoUpdate = false; scene.add(cabinHolder);
cabinHolder.add(cabin.group);
const lmc = makeLMCabin();
lmc.group.matrixAutoUpdate = false; scene.add(lmc.group);
lmc.setHatchOpen(false);
const tunnel = new THREE.Group(); cabinHolder.add(tunnel);

const attPad = new THREE.Quaternion();
const C = new THREE.Matrix4(), D = new THREE.Matrix4(), Dinv = new THREE.Matrix4();   /* cabin in the CSM; LM in the CSM */
let zH = -3;                                                                            /* the LM's ceiling, in cabin z */
const OUT = { want: false, on: false, az: -1.9, el: 0.3, dist: 30 };
const sound = makeLaunchSound();
let chat = null, radio = null, moon = null, sky = null, L = null, where = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };

/* ---------------------------------------------------------------- helpers for the panel and the page */
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
function phase(t) {
  if (S.mode === 'cm') return 'in America, home again';
  if (S.mode !== 'lm') return 'in the tunnel';
  if (t >= OPEN) return 'docked: go up through the tunnel';
  if (t >= DOCK) return 'docked, the latches closed';
  if (t >= DOCK - 330) return 'America before the windows';
  return 'closing on America';
}
/* ---------------------------------------------------------------- the wrist panel (X) and messages */
const ctl = makeControllers(renderer, head);
const common = applyCommon(renderer, sound);
const wrist = makeWrist(['ff', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
function info() {
  if (!w) return null;
  const ev = [[DOCK - 330, 'America stops before the windows'], [DOCK, 'the docking'], [OPEN, 'the tunnel opens']];
  return { chapter: 'Chapter 8 \u00b7 The rendezvous', now: phase(S.t), next: S.ended ? null : S.t >= OPEN ? (S.mode === 'cm' ? null : ['go up to America (the clock waits)', NaN]) : nextOf(ev, S.t),
    after: 'leaving the Moon', get: S.t, warp: S.warp,
    rows: [['America', S.t >= DOCK ? 'docked' : fmtDist(RANGE) + ' ahead'], ['Above the Moon', fmtDist(w.alt)], ['Speed', fmtSpeed(w.speed)], ['Radio', w.hidden ? 'behind the Moon (LOS)' : 'in touch with Houston'], ['Fast forward', settings.text('ff')]],
    alert: S.ended ? 'End of chapter 8.  A: back to the chapters' : null,
    keys: S.mode === 'lm' && S.t < OPEN ? [['A', 'slower (hold: real time)'], ['B', 'faster'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['R stick', 'press: re-seat']]
      : S.mode === 'cm' ? [['L stick', 'press: panel, settings'], ['R stick', 'press: re-seat']]
      : [['grip', 'pull yourself along'], ['L stick', 'forward: float along'], ['A', 'float through by itself'], ['L stick', 'press: panel, settings']], fps: S.fps };
}
let RANGE = 0;

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyS') clock.slower(S);
  if (e.code === 'KeyP') wrist.press();
  if (e.code === 'KeyF') goUp();
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter(NEXT);
  const jump = { Digit1: START, Digit2: DOCK - 400, Digit3: DOCK - 70, Digit4: OPEN + 2 }[e.code];
  if (jump !== undefined) { setTime(jump); standInLM(); }
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const clock = makeClock({ holds, lines: () => radio ? radio.lines : [], warps: WARPS, cap: 10, toast });
const held = t => !!clock.inHold(t);
function cycleWarp() {
  if (S.t >= OPEN - 1 || S.mode !== 'lm') { S.warp = 1; if (!S.ended) toast.say('Real time from here', 3, true); return; }
  clock.faster(S);
}
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
function setTime(t) { S.t = t; prevT = t; S.warp = 1; S.ended = false; clock.reset(); }
/* on to the next chapter: the page changes while still in VR, and the next page enters VR by itself where the browser
 * allows it (progress.js autoEnter); a break: out of VR, to this page (Continue will find the place) */
function nextChapter(href) { location.href = href; }
function takeBreak() { const s = renderer.xr.getSession(); if (s) s.end(); card.hide(); }
const STICK = { x: 0, y: 0, ry: 0 };
const pad = { prev: {}, grip: {}, aDown: -1 };
/* Before the tunnel is open: A slower (held: real time), B faster, Y outside. Once it is open: a grip, the left
 * stick forward or A sets you off up the tunnel; in it, grip and pull, the stick, or A to float by itself. Left
 * stick press: the panel, then the settings. Right stick press: re-seat */
function readControllers(now) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed;
    pad.grip[k] = !!gp.buttons[1]?.pressed;
    if (pad.grip[k] && S.mode === 'lm' && S.t >= OPEN && !OUT.on) beginCrawl();              /* taking hold: up you go */
    if (k === 'right') {
      if (a && !pad.prev.ra) {
        if (S.ended) nextChapter(NEXT);
        else if (S.t >= OPEN && S.mode !== 'cm') goUp();
        else pad.aDown = now;
      }
      if (a && pad.aDown >= 0 && now - pad.aDown > 600) { realTime(); pad.aDown = -1; }
      if (!a && pad.prev.ra && pad.aDown >= 0) { clock.slower(S); pad.aDown = -1; }
      if (b && !pad.prev.rb && S.ended) takeBreak();
      if (b && !pad.prev.rb && S.mode === 'lm' && !S.ended) cycleWarp();
      if (st && !pad.prev.rs) SEAT.pending = 1;
    } else if (k === 'left') {
      if (b && !pad.prev.lb && S.mode === 'lm') OUT.want = !OUT.want;       /* Y: outside (stays till pressed again) */
      if (st && !pad.prev.ls) wrist.press();
    }
    pad.prev[k[0] + 'a'] = a; pad.prev[k[0] + 'b'] = b; pad.prev[k[0] + 's'] = st;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') { wrist.stick(ax[0], ax[1]); if (wrist.capturesStick) { STICK.x = 0; STICK.y = 0; } else { STICK.x = ax[0]; STICK.y = ax[1]; } } else STICK.ry = ax[1];
  }
}

/* seated in the couch (the couch brought to you, as launch.js), or standing in the LM (only turned and moved
 * across the floor: your own height stays) */
const SEAT = { pending: 0, done: false }, recentred = watchRecentre(renderer, camera);
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  /* (in the LM your eyes are put at standing eye height, level with the Lunar Module Pilot's, whatever your own height
   * or whether you sit: owner's headset test) */
  head.position.set(-(c * p.x + s * p.z), S.mode === 'lm' ? 1.6 - p.y : -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true; SEAT.n = (SEAT.n || 0) + 1;
}

/* ---------------------------------------------------------------- up through the tunnel */
/* As chapter 4's float, the other way: from the LM's hatch up the tunnel, out of the cabin's apex, over the
 * instrument panel and down to your couch. You are turned to face the way you go (half a turn about the couch's
 * upright against how you sit: FLIP), so positions and directions read in your own frame are turned back into the
 * cabin's before they are used. */
const body = new THREE.Vector3(), vel = new THREE.Vector3(), grab = {}, _hw = new THREE.Vector3(), _hc = new THREE.Vector3();
const cabinWorld = new THREE.Matrix4(), cabinInv = new THREE.Matrix4(), _hr = new THREE.Vector3(), _d = new THREE.Vector3();
const flip = v => { v.x = -v.x; v.z = -v.z; return v; };
const COUCH = new THREE.Vector3(0, 0, 0.05);
/* out of the LM: through black, into the foot of the tunnel */
function beginCrawl() { if (S.mode !== 'lm' || S.t < OPEN || OUT.on) return; S.mode = 'leaving'; FADE.target = 1; vel.set(0, 0, 0); card.hide(); }
function goUp() { if (S.mode === 'lm') { S.auto = true; beginCrawl(); } else if (S.mode === 'crawl') S.auto = !S.auto; }
function intoTunnel() {
  S.mode = 'crawl'; lmc.setBody(false);
  if (renderer.xr.isPresenting) seat();                               /* (as seated: your own height does not count here) */
  body.set(AX, AY, zH + 0.25); vel.set(0, 0, 0);
  if (radio) radio.cue('up');
}
function headRel(out) { return flip(out.copy(camera.position).applyQuaternion(head.quaternion).add(head.position)); }
function clampHead(hp) {
  const dx = hp.x - AX, dy = hp.y - AY, rho = Math.hypot(dx, dy);
  const maxR = hp.z >= Z_APEX ? Math.max(0.2, rAt(hp.z) - 0.18) : 0.24;
  if (rho > maxR) { const k = maxR / rho; hp.x = AX + dx * k; hp.y = AY + dy * k; }
  /* over the instrument panel, not through it: the way rises as you come to the panel, crosses above it, and comes
   * down again to the couches */
  { const sm = (a, b, x) => { const k = Math.min(1, Math.max(0, (x - a) / (b - a))); return k * k * (3 - 2 * k); };
    const k = sm(-0.08, -0.42, hp.z) * (1 - sm(-0.85, -1.3, hp.z)), need = -0.6 + k * 1.04;
    if (hp.y < need) hp.y = need; }
  hp.z = Math.min(0.4, Math.max(zH - 0.05, hp.z));
  return hp;
}
function updateFloat(dt) {
  if (S.mode !== 'crawl') return;
  const xr = renderer.xr.isPresenting;
  /* hand over hand: a gripping hand stays where it took hold, so you move the other way */
  let n = 0; _d.set(0, 0, 0);
  if (xr) for (const k of ['left', 'right']) {
    if (!ctl.src[k]) continue;
    const gp = ctl.grips[k];
    flip(_hc.copy(gp.position).applyQuaternion(head.quaternion).add(head.position)).add(body);
    if (pad.grip[k]) { if (!grab[k]) grab[k] = _hc.clone(); else { _d.add(_hc.clone().sub(grab[k])); n++; } }
    else grab[k] = null;
  }
  if (n) { _d.divideScalar(n); body.sub(_d); vel.copy(_d).multiplyScalar(-1 / Math.max(dt, 0.011)).clampLength(0, 1.2); S.auto = false; }
  else {
    /* drifting, slowing gently; or the stick; or the automatic float: up the tunnel, then to the couch */
    const fwd = flip(_hr.set(0, 0, -1).applyQuaternion(camera.quaternion).applyQuaternion(head.quaternion));
    let push = 0;
    if (xr) push = -STICK.y; else push = (keys.ArrowUp ? 1 : 0) - (keys.ArrowDown ? 1 : 0);
    if (Math.abs(push) > 0.15) { vel.addScaledVector(fwd, push * 0.8 * dt); S.auto = false; }
    if (S.auto) {
      headRel(_d).add(body);
      const target = _d.z < Z_APEX + 0.2 ? new THREE.Vector3(AX, AY, Z_APEX + 0.35) : COUCH.clone();
      const to = target.sub(_d), dist = to.length();
      vel.lerp(to.normalize().multiplyScalar(Math.min(0.32, dist + 0.05)), Math.min(1, dt * 2));
    } else vel.multiplyScalar(Math.exp(-0.7 * dt));
    vel.clampLength(0, 0.8);
    body.addScaledVector(vel, dt);
  }
  /* stay inside: the tunnel, the cone */
  headRel(_d); const hp = _d.clone().add(body), before = hp.clone();
  clampHead(hp); body.add(hp.sub(before));
  for (const k in grab) if (grab[k]) grab[k].add(hp);                 /* a clamped hand slides along the wall */
  /* by the couches: home */
  headRel(_d).add(body);
  if (_d.z > -0.55 && _d.distanceTo(COUCH) < 0.6) { S.mode = 'seating'; FADE.target = 1; }
}
/* in your couch again */
function sitDown() {
  S.mode = 'cm'; S.auto = false; S.seatedAt = S.t; body.set(0, 0, 0); vel.set(0, 0, 0); cabin.setBody(true);
  if (renderer.xr.isPresenting) SEAT.pending = 1;
  if (radio) radio.cue('home', radio.busyFor ? radio.busyFor() : 0);
}
/* standing in the LM, as at the start (a jump in time, a second start on the same page) */
function standInLM() {
  S.mode = 'lm'; S.auto = false; S.seatedAt = -1; S.hintT = 0; S.cues = {}; body.set(0, 0, 0); vel.set(0, 0, 0); cabin.setBody(false); card.hide();
  S.hardK = 1; spacing(1); OUT.want = OUT.on = false; FADE.target = 0;
  if (renderer.xr.isPresenting) SEAT.pending = 1;
}

/* ---------------------------------------------------------------- the two ships' places */
const _p = new THREE.Vector3(), _up = new THREE.Vector3(), _fw = new THREE.Vector3(), _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3();
const _m = new THREE.Matrix4(), _m2 = new THREE.Matrix4(), _q = new THREE.Quaternion(), _s = new THREE.Vector3();
const LMworld = new THREE.Matrix4(), CSMworld = new THREE.Matrix4(), lmAtt = new THREE.Quaternion(), lmWant = new THREE.Quaternion();
const smooth = (a, b, x) => { const k = Math.min(1, Math.max(0, (x - a) / (b - a))); return k * k * (3 - 2 * k); };
const PIVOT = new THREE.Vector3(), _fp = new THREE.Vector3();
/* Where America is from the LM (LM coordinates: y up, away from the Moon; z ahead, where the windows look), and how
 * far it has still to turn to come down on to the drogue. d: seconds to the docking. Far off it is ahead, where the windows look, and
 * closing more slowly as it nears (220 km at the start); for five minutes it stands before the
 * windows nose on, 30 m off; then it swings up over your head, turning, and for the last minute comes straight
 * down at 20 cm a second. */
const FAR = new THREE.Vector3(-0.38, -0.19, 1).normalize();          /* (where the windows look: it grows in the same place) */
function csmRel(t, out) {
  const d = DOCK - t;
  if (d <= 0) return [out.set(0, 0, 0), 0];
  if (d < 60) return [out.set(0, 0.2 * d, 0), 0];
  const k = smooth(60, 330, d);
  out.set(0, 12, 0).lerp(_fp.set(-9, -3, 30).sub(PIVOT), k);
  if (d > 330) { const s = d - 330; out.addScaledVector(FAR, 0.8 * s + 0.004 * s * s); }
  return [out, k * Math.PI / 2];
}
const _rel = new THREE.Vector3();
function place(w, dt, snap) {
  _p.fromArray(w.pos); _up.fromArray(w.up); _fw.fromArray(w.v).addScaledVector(_up, -_fw.fromArray(w.v).dot(_up)).normalize();
  _x.crossVectors(_up, _fw).normalize();
  lmWant.setFromRotationMatrix(_m.makeBasis(_x, _up, _fw));
  if (snap) lmAtt.copy(lmWant); else lmAtt.rotateTowards(lmWant, RATE * dt);
  LMworld.compose(_p, lmAtt, _s.set(1, 1, 1));
  const [rel, turn] = csmRel(S.t, _rel);
  /* CSM = LM . T(rel) . T(pivot) . Rx(turn) . T(-pivot) . D^-1 */
  CSMworld.copy(LMworld).multiply(_m.makeTranslation(rel.x, rel.y, rel.z)).multiply(_m.makeTranslation(PIVOT.x, PIVOT.y, PIVOT.z))
    .multiply(_m.makeRotationX(turn)).multiply(_m.makeTranslation(-PIVOT.x, -PIVOT.y, -PIVOT.z)).multiply(spaced ? DinvNow : Dinv);
  CSMworld.decompose(vehicle.group.position, vehicle.group.quaternion, _s);
  vehicle.lights.position.copy(_p);
  cabinWorld.multiplyMatrices(CSMworld, C); cabinInv.copy(cabinWorld).invert();
  cabinHolder.matrix.copy(cabinWorld);
  lmc.group.matrix.copy(LMworld);
  return rel.length();
}
/* how far apart the two ships are drawn while docked: 0 the probe's length apart (the tunnel's spacing), 1 hard
 * docked, ring on ring, the probe drawn in */
const DinvNow = new THREE.Matrix4(); let spaced = false;
function spacing(k) {
  const md = new THREE.Matrix4(); vehicle.csmInSivb(vehicle.DOCK_DIST - k * (vehicle.DOCK_DIST - vehicle.HARD_DIST), Math.PI, md);
  DinvNow.copy(md).invert().multiply(vehicle.LM_REST).invert(); spaced = k > 0;
  vehicle.setProbe(1 - k);
}
const standM = new THREE.Matrix4();
function placeRig() {
  if (OUT.on) return placeOutside();
  if (S.mode === 'lm' || S.mode === 'leaving') {
    standM.copy(LMworld).multiply(_m.makeTranslation(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z)).multiply(_m2.makeRotationY(Math.PI));
    standM.decompose(rig.position, rig.quaternion, _s);
  } else {
    _m.copy(cabinWorld).multiply(_m2.makeTranslation(body.x, body.y, body.z));
    if (S.mode === 'crawl' || S.mode === 'seating') _m.multiply(_m2.makeRotationY(Math.PI));          /* facing the way you go */
    _m.decompose(rig.position, rig.quaternion, _s);
  }
}
const _tg = new THREE.Vector3(), _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3();
function placeOutside() {
  _tg.set(0, 3.2, 0).applyMatrix4(LMworld);
  _up.set(0, 1, 0).applyQuaternion(lmAtt); _h1.set(0, 0, 1).applyQuaternion(lmAtt); _h2.crossVectors(_up, _h1);
  rig.position.copy(_h1).multiplyScalar(Math.cos(OUT.az)).addScaledVector(_h2, Math.sin(OUT.az)).multiplyScalar(Math.cos(OUT.el)).addScaledVector(_up, Math.sin(OUT.el)).multiplyScalar(OUT.dist).add(_tg);
  _z.copy(rig.position).sub(_tg).normalize(); _x.crossVectors(_up, _z).normalize(); _y.crossVectors(_z, _x);
  rig.quaternion.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
function updateOutside(dt) {
  if (OUT.want !== OUT.on) { FADE.target = 1; if (FADE.k > 0.9) { OUT.on = OUT.want; FADE.target = 0; } }
  else if (FADE.target === 1 && FADE.k > 0.9 && S.mode !== 'leaving' && S.mode !== 'seating') FADE.target = 0;
  if (!OUT.on) return;
  let ax = 0, ay = 0, ey = 0;
  if (renderer.xr.isPresenting) { ax = STICK.x; ay = STICK.y; ey = STICK.ry; }
  else { ax = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); ey = (keys.KeyI ? -1 : 0) + (keys.KeyK ? 1 : 0); ay = (keys.KeyM ? 1 : 0) - (keys.KeyU ? 1 : 0); }
  const dz = v => Math.abs(v) < 0.15 ? 0 : v;
  OUT.az += dz(ax) * 1.1 * dt; OUT.dist = Math.min(600, Math.max(9, OUT.dist * Math.exp(dz(ay) * 1.4 * dt))); OUT.el = Math.min(1.45, Math.max(-1.3, OUT.el - dz(ey) * 0.9 * dt));
}

/* ---------------------------------------------------------------- hands: shown always; red on a panel ("Not now") */
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
  const inLM = S.mode === 'lm' || S.mode === 'leaving', panels = inLM ? lmc.panels : cabin.panels;
  for (const h of ['left', 'right']) {
    const src = ctl.src[h]; if (!src || !src.gamepad) continue;
    const m = hands[h], grip = ctl.grips[h]; if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
    const onCrew = !OUT.on && handOnCrew(m, (inLM ? [lmc.life] : [cabin.life]), gp, who => chat && chat.touch(who));
    m.userData.tip.getWorldPosition(_tip);
    let hit = false;
    if (!OUT.on && S.mode !== 'crawl') for (const pnl of panels) { _loc.copy(_tip); pnl.mesh.worldToLocal(_loc); if (Math.abs(_loc.x) < pnl.w / 2 && Math.abs(_loc.y) < pnl.h / 2 && Math.abs(_loc.z) < 0.03) hit = true; }
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
const _cam = new THREE.Vector3(), _cc = new THREE.Vector3();
function render() {
  scene.updateMatrixWorld(true); setClips();
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (recentred() && SEAT.done && !SEAT.pending && (S.mode === 'lm' || S.mode === 'cm')) SEAT.pending = 1;     /* the headset's own "reset view": seated again where you belong */
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  _cam.setFromMatrixPosition(camera.matrixWorld);
  moon.setDetail(_cam, false);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const inside = !OUT.on;
  const show = (c, s, m, n) => {
    const inLM = S.mode === 'lm' || S.mode === 'leaving';
    cabinHolder.visible = c && inside && S.t >= OPEN;
    cabin.group.visible = c && inside && !inLM;
    tunnel.visible = c && inside && S.t >= OPEN;
    lmc.group.visible = c && inside && S.mode !== 'cm';
    wrist.mesh.visible = c && wrist.on; toast.mesh.visible = c && toast.k > 0.01; badge.mesh.visible = c && badge.on; card.mesh.visible = c && card.k > 0.01; sign.visible = c && SIGN.k > 0.01;
    sky.visible = s; moon.far.visible = m; moon.near.visible = m; near.visible = n;
  };
  show(false, true, false, false); setPlanes(1e5, 3e6); renderer.render(scene, camera);
  renderer.clearDepth();
  const d = _cam.distanceTo(_cc.fromArray(L.C));
  show(false, false, true, false); setPlanes(Math.max(50, (d - L.R - 6000) * 0.25), d + L.R); renderer.render(scene, camera);
  renderer.clearDepth();
  show(false, false, false, true); setPlanes(0.3, 40000); renderer.render(scene, camera);
  renderer.clearDepth();
  show(true, false, false, false); setPlanes(0.02, 14); renderer.render(scene, camera);
  if (FADE.k > 0.001) { veil.material.opacity = FADE.k; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t, first = true, lit0 = 1;
const _sun = new THREE.Vector3(), _a = new THREE.Vector3(), _iq = new THREE.Quaternion();
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock: free until the tunnel is open, then real time, and the chapter waits for you to go up */
  if (S.running && !S.still && !S.ended) {
    if (S.t >= OPEN) S.warp = 1;
    S.t = clock.advance(S, dt);
    if (S.mode === 'cm' && S.seatedAt >= 0 && S.t > S.seatedAt + 26) { S.ended = true; wrist.show(true); card.show({ title: 'End of chapter 8', sub: 'Docked, and home in America. Leaving the Moon comes next', a: 'Back to the chapters', b: 'Take a break' }); saveProgress('index.html', null, 0, true); }
  }
  /* the tunnel is open and you are still standing there: say what to do (and again every twenty seconds); the stick
   * sets you off too */
  if (S.mode === 'lm' && S.t >= OPEN && S.running && !OUT.on) {
    S.hintT += dt;
    if (xr && STICK.y < -0.4) beginCrawl();
    if (S.hintT > 6) {
      S.hintT = -14; S.hints = (S.hints || 0) + 1;
      toast.say('To America: up through the tunnel. Left stick forward, or grip and pull, or A', 8, true);
      if (S.hints >= 2) card.show({ title: 'To America: up through the tunnel overhead', sub: 'Left stick forward  \u00b7  or grip and pull  \u00b7  or A' }, 12);
    }
  }
  /* through black: into the tunnel's foot; and at its end into your couch */
  if (S.mode === 'leaving' && FADE.k > 0.92) { intoTunnel(); FADE.target = 0; }
  if (S.mode === 'seating' && FADE.k > 0.92) { sitDown(); FADE.target = 0; }
  /* The two ships hard docked, ring on ring, while the tunnel is shut or you look from outside; with the tunnel open
   * they are drawn the probe's length apart, the spacing its inside is made to (chapter 4) */
  { const want = S.mode === 'lm' && (S.t < OPEN || OUT.on) ? 1 : 0, k = OUT.on && want ? 1 : S.hardK + Math.max(-dt / 4, Math.min(dt / 4, want - S.hardK));
    if (k !== S.hardK) { S.hardK = k; spacing(k); } }
  lmc.setHatchOpen(S.t >= OPEN);
  lmc.setBody(S.mode === 'lm' && settings.get('body'));
  w = where(S.t);
  moon.setSunOffset(L.sunOffset(S.t));
  _sun.copy(moon.shared.uSun.value);
  sky.userData.setSpin(W_EARTH * (S.t - GETL.land) + 2.1);
  const dist = place(w, dt, first); first = false; RANGE = dist;
  /* the vehicle: the CSM with its cabin hidden while you are in it; the LM outside hidden while you are in it */
  vehicle.update(-1e9, (S.mode === 'cm' || S.mode === 'crawl' || S.mode === 'seating') && !OUT.on ? 'inside' : 'outside', { alt: 1e9, up: w.up });
  _m.copy(CSMworld).invert().multiply(LMworld);
  vehicle.space({ sivb: null, lm: _m, open: 1, gone: 1e6, sps: 0, j2: 0 });
  vehicle.lm.visible = OUT.on || S.mode === 'cm';
  vehicle.parts.csm.visible = dist < 30000;
  /* far off, a point of light where America is (drawn no further than the near pass reaches) */
  { const far = dist > 150; star.visible = far && S.mode === 'lm';
    if (far) { CSMworld.decompose(_x, _q, _s); _y.setFromMatrixPosition(LMworld); _x.sub(_y); const l = _x.length(); _x.multiplyScalar(Math.min(l, 25000) / l).add(_y);
      star.geometry.attributes.position.setXYZ(0, _x.x, _x.y, _x.z); star.geometry.attributes.position.needsUpdate = true; star.material.opacity = Math.min(1, (dist - 150) / 450) * (0.55 + 0.45 * lit0); } }
  updateFloat(dt);
  placeRig();
  /* sunlight or the Moon's shadow */
  const along = _a.fromArray(w.r).dot(_sun), perp = Math.sqrt(Math.max(0, w.rC * w.rC - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - L.R) / 30000 + 0.5)); lit0 = lit;
  vSun.position.copy(_sun).multiplyScalar(100); vSun.intensity = 2.6 * lit;
  vHemi.position.fromArray(w.up); vHemi.color.setRGB(0.05, 0.05, 0.06); vHemi.groundColor.setRGB(0.55, 0.53, 0.5);
  vHemi.intensity = 0.15 + 0.6 * Math.max(0, _a.fromArray(w.up).dot(_sun) + 0.2);
  cabinWorld.decompose(_x, _q, _s); cabin.setDaylight(lit, _y.copy(_sun).applyQuaternion(_iq.copy(_q).invert()));
  lmc.setDaylight(lit, _y.copy(_sun).applyQuaternion(_iq.copy(lmAtt).invert()));
  sky.userData.stars.setDark(1 - lit);
  /* the docking: contact and the twelve latches; then the hatch overhead */
  if (prevT < DOCK && S.t >= DOCK) { sound.thump(0.7, 0.4, 500); if (!S.cues.dock) { S.cues.dock = true; if (radio) radio.cue('dock'); } }
  if (prevT < OPEN && S.t >= OPEN) { sound.thump(0.3, 0.2, 900); if (!S.cues.open) { S.cues.open = true; if (radio) radio.cue('open'); } }
  prevT = S.t;
  sound.drive(0, S.warp);
  /* instruments */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  for (const b of lmc.balls) b.quaternion.copy(lmAtt).invert();
  if (now - lastDsky > 500) {
    for (const d of [cabin.dsky, lmc.dsky]) d.draw('00', '16', '44', d.f(Math.min(99999, w.alt / 1852 * 10)), d.f(0), d.f(w.speed * 3.281));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: S.mode === 'cm' ? ['CMP'] : ['LMP'], where: S.mode === 'cm' ? ['any', 'space'] : ['any', 'lm', 'moon'], quiet: S.running && !S.ended && !OUT.on && S.mode === 'lm' && S.t < DOCK - 500 && !(radio && radio.current(S.t)) && !(radio && radio.busyFor && radio.busyFor() > 0) });
  updateOutside(dt);
  updateHands(dt);
  { const r = combineRadios(radio, chat && chat.radio); cabin.update(now / 1000, { viewer: camera, radio: r }); lmc.update(now / 1000, { viewer: camera, radio: r }); }
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, (S.mode === 'lm' || S.mode === 'leaving') && !OUT.on ? 1.62 : 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 8. A (N on this screen): back to the chapters.' : S.mode === 'lm' && S.t >= OPEN ? 'Up through the tunnel to America: F on this screen (arrow keys to float yourself).' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress('rendezvous.html', Math.min(S.t, OPEN + 1), now);
  render();
  S.frames++;
  if (window.__rdv.afterFrame) { const f = window.__rdv.afterFrame; window.__rdv.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/rendezvous.json', import.meta.url).href);
  if (ctx && !chat) chat = await makeChatter(ctx, sound.voice);
}
function readOptions() {
  setTime($('start') ? +$('start').value : S.t); standInLM();
  if (radio) for (const l of radio.lines) l.off = false;
}
async function enterVR() {
  if (!navigator.xr) return;
  readOptions(); await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  if ($('hz')) settings.set('hz', +$('hz').value);
  renderer.xr.setFoveation(settings.get('fov'));
  common.all();
  card.show({ title: 'Chapter 8', sub: 'The rendezvous' }, 6);
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
offerContinue('rendezvous.html', $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = ascentState(-60); _x.fromArray(s0.nose); _y.fromArray(s0.head);
    const nose = _x.clone(), hd = _y.clone().addScaledVector(nose, -_y.dot(nose)).normalize(), zz = nose.clone().negate(), xx = new THREE.Vector3().crossVectors(hd, zz).normalize();
    attPad.setFromRotationMatrix(_m.makeBasis(xx, hd, zz)); }
  C.compose(EYE_OFF, attPad, _s.set(1, 1, 1));
  { const md = new THREE.Matrix4(); vehicle.csmInSivb(vehicle.DOCK_DIST, Math.PI, md);   /* the crawl's spacing: the inside of the CM and the outside do not match yet, and at hard dock the tunnel would be 40 cm */ D.copy(md).invert().multiply(vehicle.LM_REST); Dinv.copy(D).invert(); }
  /* the LM's hatch in the cabin's frame: the tunnel runs from the cabin's apex to it */
  const hc = LMC.HATCH.clone().applyMatrix4(D).applyMatrix4(_m.copy(C).invert());
  zH = hc.z;
  PIVOT.set(0, -3, 0).applyMatrix4(Dinv);
  {
    const len = Z_APEX + 0.05 - zH, tm = new THREE.MeshLambertMaterial({ color: 0xa3a5a7, side: THREE.BackSide });
    const tube = new THREE.Mesh(new THREE.CylinderGeometry(0.4, 0.4, len, 32, 1, true), tm);
    tube.rotation.x = Math.PI / 2; tube.position.set(AX, AY, zH + len / 2); tunnel.add(tube);
    for (const z of [Z_APEX - 0.05, zH + len * 0.45, zH + 0.08]) {
      const r = new THREE.Mesh(new THREE.TorusGeometry(0.39, 0.025, 8, 40), new THREE.MeshLambertMaterial({ color: 0x6d7073 })); r.position.set(AX, AY, z); tunnel.add(r);
    }
    for (let k = 0; k < 4; k++) {                                         /* the docking latches round the ring */
      const a = k * Math.PI / 2 + 0.4, l = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.06, 0.14), new THREE.MeshLambertMaterial({ color: 0x3a3b3c }));
      l.position.set(AX + Math.cos(a) * 0.36, AY + Math.sin(a) * 0.36, zH + len * 0.45); tunnel.add(l);
    }
    const tl = new THREE.PointLight(0xfff0dc, 0.7, 2.5, 1.5); tl.position.set(AX, AY, zH + len * 0.5); tunnel.add(tl);
  }
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 26); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  spacing(1);
  if (Q.has('fhatch')) lmc.setForwardHatch(num('fhatch', 1));          /* test aid: the forward hatch open */
  if (Q.has('float')) { S.mode = 'crawl'; cabin.setBody(false); body.set(AX, AY, num('float', -2)); }       /* test aid: floating on the tunnel's axis at cabin z */
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.shared.uExposure.value = 3.4;
  sky = makeSky(moon);
  scene.add(sky, moon.far, moon.near);
  L = makeLunarFlight(moon.manifest);
  /* the orbit: circular, 113 km up, in the earlier chapters' plane, coming over the landing site four minutes after
   * the docking */
  { const R = L.R, rO = R + 113e3, n = Math.sqrt(MU_M / (rO * rO * rO)), vO = n * rO, E = L.EARTH;
    where = t => { const a = -0.16 + n * (t - DOCK), up = L.dirAt(a), fw = L.dirAt(a + Math.PI / 2), r = up.map(x => x * rO);
      const along = r[0] * E[0] + r[1] * E[1] + r[2] * E[2], off = Math.hypot(r[0] - E[0] * along, r[1] - E[1] * along, r[2] - E[2] * along);
      return { r, v: fw.map(x => x * vO), rC: rO, alt: rO - R, speed: vO, up, pos: [L.C[0] + r[0], L.C[1] + r[1], L.C[2] + r[2]], angle: a, hidden: along < 0 && off < R }; }; }
  w = where(S.t);
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__rdv = { S, OUT, START, DOCK, OPEN, ctl, wrist, toast, clock, get chat() { return chat; }, settings, info, renderer, camera, rig, head, cabin, lmc, vehicle, hands, SEAT, body, afterFrame: null, setTime, star, lmUp, clipUp,
  get zH() { return zH; }, get radio() { return radio; }, get L() { return L; }, get w() { return w; }, get range() { return RANGE; }, beginCrawl, goUp, standInLM, tunnel, readOptions, csmRel, FADE };
