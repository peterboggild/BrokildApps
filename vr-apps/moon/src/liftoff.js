/* Journey to the Moon, chapter 7: the lift-off, back to lunar orbit.
 *
 * GET 185:21:07 to orbit insertion a little over seven minutes after the lift-off at 185:21:37, standing at the
 * Commander's station of the LM (lm-cabin.js), the samples aboard. The ascent engine lights and the ascent stage
 * leaves the descent stage on the ground: ten seconds straight up, then the pitch-over, face down, the valley and
 * then the Moon's whole face sliding by under the windows as the speed builds; cut-off 18 km up, in orbit. Nothing
 * is flown by hand: the real ascent was the computer's, the crew watching it.
 *
 * The flight: liftoff-space.js (stage 0's site frame). The Moon: as chapter 5, in its two surface passes near the
 * ground and in one pass from high up. Outside (Y): the ascent stage alone, the descent stage where it was left
 * (two copies of the model, each cut at the joint, as in chapter 6's film).
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=) */
import * as THREE from 'three';
import { loadMoon } from './moon.js?v=4';
import { makeSky, makeRocks } from './extras.js?v=5';
import { makeGround } from './ground.js?v=4';
import { GETL, HEADING } from './lunar-space.js?v=35';
import { makeAscent, GETA } from './liftoff-space.js?v=35';
import { makeLMCabin, LMC } from './lm-cabin.js?v=35';
import { settleLM, LM } from './lm-model.js?v=35';
import { makeLMShadow } from './lm-shadow.js?v=35';
import { makeLaunchSound } from './launch-sound.js?v=35';
import { makeRadio, combineRadios } from './radio.js?v=35';
import { makeClock } from './clock.js?v=35';
import { makeChatter } from './chatter.js?v=35';
import { handOnCrew } from './crew-life.js?v=35';
import { makeVehicle, makeLMModel } from './vehicle.js?v=35';
import { createHand, makeControllers } from './hands.js?v=35';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtSpeed } from './wrist.js?v=35';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=35';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=35';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;

const START = GETA.lift - 30;
const W_EARTH = 7.2921159e-5;
const RATE = 4.5 * Math.PI / 180;         /* how fast the LM turns: the pitch-over takes about twelve seconds */
const FT = 0.3048;

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;
renderer.xr.setReferenceSpaceType('local-floor');
renderer.localClippingEnabled = true;                     /* the two stages are one model each, cut at the joint */
$('stage').appendChild(renderer.domElement);
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, innerWidth / innerHeight, 0.02, 100);
const rig = new THREE.Group(), head = new THREE.Group();
rig.add(head); head.add(camera); scene.add(rig);
addEventListener('resize', () => { if (renderer.xr.isPresenting) return; camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); });

const S = { t: num('t', START), running: false, warp: 1, still: Q.has('still'), yaw: num('yaw', 0), pitch: num('pitch', 0),
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, ended: false, cues: {}, landed: -1 };

/* ---------------------------------------------------------------- the LM, inside and out */
const vehicle = makeVehicle(renderer);
for (const k of ['sic', 'skirt', 'sii', 'sivb', 'csm', 'les', 'cm']) if (vehicle.parts[k]) vehicle.parts[k].visible = false;
vehicle.lm.userData.setLegs(true);
/* The ascent stage is the flying model, cut off below the joint, without the legs, the porch and the deflectors
 * (the descent stage's), with its engine's bell whole. The descent stage is a second model that stays where the LM
 * stood, cut off above the joint, with what is its own whole. The cuts are world planes, set every frame from each
 * model's own place and lean (the LM stands a little tilted: chapter 6's lesson). */
const lmUp = vehicle.lm, lmDown = makeLMModel(renderer);
const clipUp = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), clipDown = new THREE.Plane(new THREE.Vector3(0, -1, 0), 0);
{ const whole = groups => { for (const g of groups) g.traverse(o => { if (o.isMesh) { o.material = o.material.clone(); o.userData.whole = true; } }); };
  whole([lmDown.userData.deployed, lmDown.userData.descentTop]); whole([lmUp.userData.ascentOnly]);
  lmDown.traverse(o => { if (o.isMesh && !o.userData.whole) { o.material = o.material.clone(); o.material.clippingPlanes = [clipDown]; } });
  lmUp.traverse(o => { if (o.isMesh && !o.userData.whole) o.material.clippingPlanes = [clipUp]; });
  lmUp.userData.deployed.visible = false; lmUp.userData.descentTop.visible = false; lmDown.userData.ascentOnly.visible = false;
  lmDown.rotation.order = 'YXZ'; }
const _cn = new THREE.Vector3(), _cp = new THREE.Vector3(), _cq = new THREE.Vector3();
function setClips() {
  lmDown.updateMatrixWorld(true); lmUp.updateMatrixWorld(true);
  _cn.set(0, 1, 0).transformDirection(lmDown.matrixWorld);
  clipDown.setFromNormalAndCoplanarPoint(_cq.copy(_cn).negate(), lmDown.localToWorld(_cp.set(0, LM.AS_Y - 0.01, 0)));
  _cn.set(0, 1, 0).transformDirection(lmUp.matrixWorld);
  clipUp.setFromNormalAndCoplanarPoint(_cn, lmUp.localToWorld(_cp.set(0, LM.AS_Y - 0.01, 0)));
}
const vSun = vehicle.lights.children.find(l => l.isDirectionalLight), vHemi = vehicle.lights.children.find(l => l.isHemisphereLight);
const lmc = makeLMCabin(); lmc.setHatchOpen(false);
lmc.group.matrixAutoUpdate = false; scene.add(lmc.group);
const nearG = new THREE.Group(); scene.add(nearG);
nearG.add(vehicle.group, vehicle.lights, lmDown);
const OUT = { want: false, on: false, az: 2.4, el: 0.25, dist: 26 };
const sound = makeLaunchSound();
let chat = null, radio = null, moon = null, sky = null, D = null, ground = null, rocks = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0, then: null };

/* ---------------------------------------------------------------- the LM's shadow on the ground, and what the engine throws up */
const lmShadow = makeLMShadow({ heightAt: (x, z) => moon.heightAt(x, z) }), shadow = lmShadow.mesh;
shadow.visible = false; nearG.add(shadow);
/* grains and scraps of foil thrown out as the ascent engine lights on top of the descent stage: with no air each flies its own arc and
 * falls back, slowly, in a sixth of the Earth's gravity; nothing hangs as a cloud (owner's wish: dust that falls
 * slowly). A ring buffer of points, each worked out in the shader from where and when it left. */
const GRAINS = 1400, grainU = { uT: { value: 0 }, uPx: { value: 900 } };
const grainGeo = new THREE.BufferGeometry();
for (const [n, k] of [['position', 3], ['aV', 3], ['aT0', 1]]) grainGeo.setAttribute(n, new THREE.BufferAttribute(new Float32Array(GRAINS * k).fill(n === 'aT0' ? -1e6 : 0), k));
const grains = new THREE.Points(grainGeo, new THREE.ShaderMaterial({
  uniforms: grainU, transparent: true, depthWrite: false,
  vertexShader: `uniform float uT; uniform float uPx; attribute vec3 aV; attribute float aT0; varying float vA;
    void main(){ float t = uT - aT0; vec3 p = position + aV * t; p.y -= 0.81 * t * t;
      float up = p.y - position.y; vA = (t > 0.0 && up > -0.25) ? smoothstep(0.0, 0.3, t) * (1.0 - smoothstep(4.0, 9.0, t)) : 0.0;
      vec4 mv = modelViewMatrix * vec4(p, 1.0); gl_Position = projectionMatrix * mv; gl_PointSize = clamp(uPx * 0.035 / max(0.5, -mv.z), 1.0, 14.0); }`,
  fragmentShader: `varying float vA; void main(){ vec2 c = gl_PointCoord - 0.5; float d = dot(c, c); if (vA < 0.01 || d > 0.25) discard;
    gl_FragColor = vec4(vec3(0.66, 0.63, 0.58), vA * 0.55 * (1.0 - d * 4.0)); }` }));
grains.frustumCulled = false; grains.renderOrder = 22; nearG.add(grains);
let grainAt = 0;
function throwDust(n, x, y, z, t, hard) {
  const P = grainGeo.attributes.position, Vv = grainGeo.attributes.aV, T = grainGeo.attributes.aT0;
  for (let k = 0; k < n; k++) {
    const i = grainAt; grainAt = (grainAt + 1) % GRAINS;
    const a = Math.random() * Math.PI * 2, r = 0.8 + Math.random() * 2.5, sp = (hard ? 5 : 8) + Math.random() * (hard ? 9 : 16), up = (hard ? 0.6 : 0.3) + Math.random() * (hard ? 4.2 : 2.4);
    P.setXYZ(i, x + Math.cos(a) * r, y + 0.1, z + Math.sin(a) * r); Vv.setXYZ(i, Math.cos(a) * sp, up, Math.sin(a) * sp); T.setX(i, t + Math.random() * 0.15);
  }
  P.needsUpdate = Vv.needsUpdate = T.needsUpdate = true;
}

/* ---------------------------------------------------------------- the panel's helpers */
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
function prog(t) {
  if (t < GETA.lift) return 'on the surface';
  if (t < GETA.pitch) return 'lift-off, straight up';
  if (t < GETA.insert) return 'P12 powered ascent';
  return 'in orbit';
}
let cur = null, padH = 0;
/* the wrist panel (X), the messages */
const ctl = makeControllers(renderer, head);
const common = applyCommon(renderer, sound);
const wrist = makeWrist(['ff', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand']);
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh);                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
function info() {
  if (!cur) return null;
  const up = D.upAt(cur.r), vu = cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2], vh = Math.sqrt(Math.max(0, cur.v[0] ** 2 + cur.v[1] ** 2 + cur.v[2] ** 2 - vu * vu));
  const h = Math.max(0, padH), ev = [[GETA.lift, 'lift-off'], [GETA.pitch, 'the pitch-over'], [GETA.insert, 'engine cut-off, in orbit']];
  const o = S.t >= GETA.insert ? D.orbit(cur) : null;
  return { chapter: 'Chapter 7 \u00b7 The lift-off', now: prog(S.t), next: S.ended ? null : nextOf(ev, S.t),
    after: 'the rendezvous with America', get: S.t, warp: S.warp,
    rows: [['Altitude', (h / FT).toFixed(0) + ' ft  (' + (h >= 2000 ? (h / 1000).toFixed(1) + ' km' : h.toFixed(0) + ' m') + ')'], ['Climbing', (vu / FT).toFixed(0) + ' ft/s'], ['Forward', (vh / FT).toFixed(0) + ' ft/s  (' + fmtSpeed(vh) + ')'],
      o ? ['Orbit', (o.peri / 1000).toFixed(0) + ' by ' + (o.apo / 1000).toFixed(0) + ' km'] : ['To orbit', Math.max(0, D.vI - vh).toFixed(0) + ' m/s to gain'], ['Fast forward', settings.text('ff')]],
    alert: S.ended ? 'End of chapter 7.  A: on to chapter 8' : null,
    keys: [['A', 'slower (hold: real time)'], ['B', 'faster'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['R stick', 'press: re-seat']], fps: S.fps };
}

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyS') clock.slower(S);
  if (e.code === 'KeyP') wrist.press();
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter(NEXT);
  const jump = { Digit1: START, Digit2: GETA.lift + 60, Digit3: GETA.insert - 60 }[e.code];
  if (jump !== undefined) restart(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
/* fast forward (clock.js): real time through the lift-off and the pitch-over, thirty seconds, and through the
 * cut-off; then you are told that B runs the long burn ten times as fast (owner: seven minutes is too long) */
const FF_AT = 30;                           /* seconds after lift-off */
const NEXT = 'rendezvous.html';
const HOLDS = () => [[GETA.lift - 20, GETA.lift + FF_AT, 'the lift-off'], [GETA.insert - 20, GETA.insert + 12, 'the cut-off']], HOLDS_MAJOR = () => [[GETA.lift - 10, GETA.lift + FF_AT, 'the lift-off']];
const clock = makeClock({ holds: () => holdsFor(HOLDS(), HOLDS_MAJOR()), lines: () => radio ? radio.lines : [], warps: [1, 10], cap: 10, toast });
function cycleWarp() { clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
/* on to the next chapter: the page changes while still in VR, and the next page enters VR by itself where the browser
 * allows it (progress.js autoEnter); a break: out of VR, to this page (Continue will find the place) */
function nextChapter(href) { location.href = href; }
function takeBreak() { const s = renderer.xr.getSession(); if (s) s.end(); card.hide(); }
const STICK = { lx: 0, ly: 0, rx: 0, ry: 0 };
const pad = { prev: {}, aDown: -1 };
/* to a point on the flight: everything reset */
function restart(t) { S.t = t; S.warp = 1; S.ended = false; S.cues = {}; snap = true; grainLift = false; clock.reset(); }
/* A (right): a tap makes time slower, holding it goes back to real time; at the end, on to the chapters. B: faster.
 * Left stick press: the panel, then the settings. Y (left): outside. Right stick press: re-seat. */
function readControllers(now) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed;
    if (k === 'right') {
      if (a && !pad.prev.ra) { if (S.ended) nextChapter(NEXT); else pad.aDown = now; }
      if (a && pad.aDown >= 0 && now - pad.aDown > 600) { realTime(); pad.aDown = -1; }
      if (!a && pad.prev.ra && pad.aDown >= 0) { clock.slower(S); pad.aDown = -1; }
      if (b && !pad.prev.rb && S.ended) takeBreak();
      if (b && !pad.prev.rb && !S.ended) cycleWarp();
      if (st && !pad.prev.rs) SEAT.pending = 1;
    } else if (k === 'left') {
      if (b && !pad.prev.lb) OUT.want = !OUT.want;       /* Y: outside (stays till pressed again) */
      if (st && !pad.prev.ls) wrist.press();
    }
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') wrist.stick(ax[0], ax[1]);
    if (k === 'left' && !wrist.capturesStick) { STICK.lx = ax[0]; STICK.ly = ax[1]; }
    else if (k === 'right') { STICK.rx = ax[0]; STICK.ry = ax[1]; }
    pad.prev[k[0] + 'a'] = a; pad.prev[k[0] + 'b'] = b; pad.prev[k[0] + 's'] = st;
  }
}
/* standing at your station (only turned and moved over the floor; your own height stays) */
const SEAT = { pending: 0, done: false }, recentred = watchRecentre(renderer, camera);
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), 1.6 - p.y, -(-s * p.x + c * p.z));          /* eyes at standing height, level with the LMP's */
  SEAT.done = true;
}

/* ---------------------------------------------------------------- the flight */
const V3 = a => new THREE.Vector3().fromArray(a);
const _y = new THREE.Vector3(), _z = new THREE.Vector3(), _x = new THREE.Vector3(), _u = new THREE.Vector3(), _f = new THREE.Vector3(), _n = new THREE.Vector3();
const _m = new THREE.Matrix4(), _m2 = new THREE.Matrix4(), _s = new THREE.Vector3(), _q = new THREE.Quaternion();
const lmAtt = new THREE.Quaternion(), attWant = new THREE.Quaternion(), LMworld = new THREE.Matrix4();
let snap = true, thrustY = new THREE.Vector3(0, 1, 0);
function step() { cur = D.state(S.t); }
/* The LM's attitude. On the ground: as it stands (settled on its pads, chapter 5 and 6). In flight: its +y along the
 * thrust; the windows forward while it rises straight up, and turned down to the Moon as it pitches over (the crews
 * flew the ascent face down, the ground sliding by under the windows). After the cut-off it stays as it was. */
const qStand = new THREE.Quaternion();
function attitude(dt) {
  const am = Math.hypot(cur.a[0], cur.a[1], cur.a[2]);
  _u.fromArray(D.upAt(cur.r)); _f.fromArray(D.fwdAt(cur.r));
  if (S.t < GETA.lift) attWant.copy(qStand);
  else {
    if (am > 0.3) thrustY.fromArray(cur.a).normalize();
    _y.copy(thrustY);
    _z.copy(_f).sub(_u); _z.addScaledVector(_y, -_z.dot(_y)).normalize();
    _x.crossVectors(_y, _z).normalize(); _z.crossVectors(_x, _y);
    attWant.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
  }
  if (snap) { lmAtt.copy(attWant); snap = false; } else lmAtt.rotateTowards(attWant, RATE * dt);
  _s.fromArray(cur.r); _q.copy(lmAtt);
  LMworld.compose(_s, _q, _x.set(1, 1, 1));
  lmc.group.matrix.copy(LMworld);
  vehicle.group.position.copy(_s); vehicle.group.quaternion.copy(_q);
  vehicle.lights.position.copy(_s);
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
  /* never under the ground: half a metre clear of the terrain wherever the camera goes (owner's headset test) */
  if (moon && padH < 3000 && Math.hypot(rig.position.x, rig.position.z) < 20000) { const gh = moon.heightAt(rig.position.x, rig.position.z); if (isFinite(gh)) rig.position.y = Math.max(rig.position.y, gh + 0.5); }
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
  OUT.az += dz(ax) * 1.1 * dt; OUT.dist = Math.min(600, Math.max(9, OUT.dist * Math.exp(dz(ay) * 1.4 * dt))); OUT.el = Math.min(1.45, Math.max(-1.2, OUT.el - dz(ey) * 0.9 * dt));
}
let grainLift = false, shBaked = false; const _v2 = new THREE.Vector2();
/* The shadows on the ground, each stage's own silhouette as the Sun sees it (lm-shadow.js bake, as in chapter 6):
 * together the whole LM's while it stands; then the descent stage's stays, and the ascent stage's runs off down-Sun
 * as it rises straight up (once it pitches over and goes, its shadow is far off and is not drawn). And the burst at
 * ignition. */
const shUp = makeLMShadow({ heightAt: (x, z) => moon.heightAt(x, z) }); shUp.mesh.visible = false; nearG.add(shUp.mesh);
const _sun = new THREE.Vector3(), _kp = new THREE.Vector3(), _kq = new THREE.Quaternion();
function updateShadow(now) {
  grainU.uT.value = now / 1000; grainU.uPx.value = renderer.getDrawingBufferSize(_v2).y;
  if (!moon) return;
  if (!shBaked) {
    /* (drawn once, both stages as they stand, wherever in the flight the chapter starts) */
    _kp.copy(vehicle.group.position); _kq.copy(vehicle.group.quaternion);
    vehicle.group.position.fromArray(D.LAND); vehicle.group.quaternion.copy(qStand); nearG.updateMatrixWorld(true); setClips();
    const vis = lmUp.visible; lmUp.visible = true;
    lmShadow.bake(renderer, lmDown, _sun, { clip: [clipDown], read: false });
    shUp.bake(renderer, vehicle.group, _sun, { clip: [clipUp], read: false, nx: 12, ny: 52 });
    lmUp.visible = vis; vehicle.group.position.copy(_kp); vehicle.group.quaternion.copy(_kq); nearG.updateMatrixWorld(true); setClips(); shBaked = true;
  }
  const near = padH < 3000 && Math.hypot(cur.r[0], cur.r[2]) < 20000, up = cur.r[1] - D.LAND[1], off = Math.hypot(cur.r[0] - D.LAND[0], cur.r[2] - D.LAND[2]);
  shadow.visible = near;
  shUp.mesh.visible = near && up < 260 && off < 4; if (shUp.mesh.visible) shUp.lift(Math.max(0, up));
  if (S.t >= GETA.lift && S.t < GETA.lift + 1.5 && !grainLift && S.running) { grainLift = true; throwDust(520, D.LAND[0], D.LAND[1] + 1.4, D.LAND[2], now / 1000, true); }
}

/* ---------------------------------------------------------------- hands */
const hands = { left: createHand('left'), right: createHand('right') };
function updateHands(dt) {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const h of ['left', 'right']) {
    const src = ctl.src[h]; if (!src || !src.gamepad) continue;
    const m = hands[h], grip = ctl.grips[h]; if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
    const onCrew = !OUT.on && handOnCrew(m, [lmc.life], gp, who => chat && chat.touch(who));
    m.userData.setTint(!!onCrew);
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
  scene.updateMatrixWorld(true); setClips();
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (recentred() && SEAT.done && !SEAT.pending && true) SEAT.pending = 1;     /* the headset's own "reset view": seated again where you belong */
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  _cam.setFromMatrixPosition(camera.matrixWorld);
  const low = padH < 3500 && Math.hypot(_cam.x, _cam.z) < 60000;
  /* low in the valley, as standing in it: the coarse whole-Moon levels beyond the massifs are not drawn (stage 0) */
  moon.setDetail(_cam, padH < 1500 && Math.hypot(_cam.x, _cam.z) < 20000);
  if (low) { rocks.update(_cam); }
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, s, mf, mn, n) => {
    lmc.group.visible = c && !OUT.on; wrist.mesh.visible = c && wrist.on; toast.mesh.visible = c && toast.k > 0.01; badge.mesh.visible = c && badge.on; card.mesh.visible = c && card.k > 0.01;
    sky.visible = s; moon.far.visible = mf; moon.near.visible = mn;
    ground.group.visible = n && low && padH < 2000; rocks.group.visible = n && low && padH < 1200;
    nearG.visible = n; vehicle.lm.visible = OUT.on;          /* (from inside, your own ascent stage is the cabin about you) */
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
const cue = k => { if (S.cues[k]) return; S.cues[k] = true; if (radio) radio.cue(k); };
let lastT = 0, acc = 0, accN = 0, lastDsky = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock */
  const was = S.t;
  if (S.running && !S.still && !S.ended) S.t = Math.min(clock.advance(S, dt), END);
  step();
  padH = D.height(cur) - H0;
  /* what happens: ignition, the pitch-over, the cut-off, the end */
  if (was < GETA.lift && S.t >= GETA.lift) { cue('ignition'); sound.thump(0.5, 0.5, 240); }
  if (S.t >= GETA.pitch) cue('pitch');
  if (S.t >= GETA.lift + FF_AT && S.t < GETA.insert - 30 && !S.cues.ff && S.running) { S.cues.ff = true; if (S.warp === 1) toast.say('Fast forward enabled (B)', 6, true); }
  if (was < GETA.insert && S.t >= GETA.insert) { cue('cutoff'); sound.thump(0.25, 0.3, 500); }
  if (S.t >= END && !S.ended) { S.ended = true; wrist.show(true); card.show({ title: 'End of chapter 7', sub: 'In lunar orbit', a: 'Continue: chapter 8, The rendezvous', b: 'Take a break' }); saveProgress('rendezvous.html', null, 0, true); }
  moon.setSunOffset(GETL ? (S.t - GETL.land) * 0.508 / 3600 : 0);
  _sun.copy(moon.shared.uSun.value);
  sky.userData.setSpin(W_EARTH * (S.t - GETL.land) + 2.1);
  attitude(dt);
  vehicle.update(-1e9, 'outside', { alt: 1e9, up: D.upAt(cur.r) });
  vehicle.space({ sivb: null, lm: _m.identity(), open: 1, gone: 1e6, sps: 0, j2: 0 });
  for (const k of ['sic', 'skirt', 'sii', 'sivb', 'csm', 'les', 'cm']) if (vehicle.parts[k]) vehicle.parts[k].visible = false;
  lmUp.userData.deployed.visible = false;
  placeRig();
  if (low()) { ground.update(cur.r[0], cur.r[2]); }
  updateShadow(now);
  /* light: the Sun, three days higher than at the landing; the ground's grey light from below */
  vSun.position.copy(_sun).multiplyScalar(100); vSun.intensity = 2.6;
  vHemi.position.fromArray(D.upAt(cur.r)); vHemi.color.setRGB(0.05, 0.05, 0.06); vHemi.groundColor.setRGB(0.55, 0.53, 0.5); vHemi.intensity = 0.55;
  lmc.setDaylight(1, _x.copy(_sun).applyQuaternion(_q.copy(lmAtt).invert()));
  /* the engine, right behind your knees: the cabin trembles; the sound (the ascent engine was heard as a hiss
   * and felt more than heard) */
  const thrust = Math.hypot(cur.a[0], cur.a[1], cur.a[2]) / 5.0;
  lmc.group.matrix.multiply(_m.makeTranslation((Math.random() - 0.5) * 0.0009 * thrust, 0, (Math.random() - 0.5) * 0.0009 * thrust));
  sound.drive(Math.min(1, thrust) * 0.45, S.warp);
  /* instruments: P12, verb 16 noun 94 (speed, climb rate, altitude) */
  for (const b of lmc.balls) b.quaternion.copy(lmAtt).invert();
  if (now - lastDsky > 400) {
    const d = lmc.dsky, up = D.upAt(cur.r), vu = cur.v[0] * up[0] + cur.v[1] * up[1] + cur.v[2] * up[2];
    d.draw(S.t >= GETA.insert ? '00' : '12', '16', '94', d.f(Math.hypot(cur.v[0], cur.v[1], cur.v[2]) / FT), d.f(vu / FT), d.f(Math.max(0, padH) / FT / 10));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: ['LMP'], where: ['lm', 'moon'], quiet: S.running && !OUT.on && S.t > GETA.insert + 30 && !(radio && radio.current(S.t)) });
  updateOutside(dt);
  updateHands(dt);
  lmc.setBody(settings.get('body'));
  lmc.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio), busy: S.t > GETA.lift - 30 && S.t < GETA.insert + 20 });
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, OUT.on ? 0 : 1.62, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 7. A (N on this screen) goes on to chapter 8, the rendezvous.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + prog(S.t) + '  ' + (padH >= 2000 ? (padH / 1000).toFixed(1) + ' km' : Math.max(0, padH).toFixed(0) + ' m') + (S.warp > 1 ? '  ×' + S.warp : '');
  wrist.draw(now, info());
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress('liftoff.html', S.t, now);
  render();
  S.frames++;
  if (window.__liftoff.afterFrame) { const f = window.__liftoff.afterFrame; window.__liftoff.afterFrame = null; f(); }
}
let END = 1e12, H0 = 0;
const low = () => padH < 2000 && cur && Math.hypot(cur.r[0], cur.r[2]) < 60000;

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/liftoff.json', import.meta.url).href);
  if (ctx && !chat) chat = await makeChatter(ctx, sound.voice);
}
function readOptions() { if ($('start')) restart(+$('start').value); }
async function enterVR() {
  if (!navigator.xr) return;
  readOptions(); await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  if ($('hz')) settings.set('hz', +$('hz').value);
  renderer.xr.setFoveation(settings.get('fov'));
  common.all();
  card.show({ title: 'Chapter 7', sub: 'The lift-off' }, 6);
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
offerContinue('liftoff.html', $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  if (Q.has('out')) { OUT.want = OUT.on = true; if (Q.has('od')) OUT.dist = num('od', 26); if (Q.has('oa')) OUT.az = num('oa', 0); if (Q.has('oe')) OUT.el = num('oe', 0); }
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.shared.uExposure.value = 3.4;
  sky = makeSky(moon);
  rocks = await makeRocks(moon, BASE);
  ground = makeGround(moon, BASE);
  scene.add(sky, moon.far, moon.near, ground.group, rocks.group);
  if (ground.ok) await ground.request(0, 0).catch(() => {});
  /* where the LM stands: as chapter 5 set it down and chapter 6 left it (pads on the ground, facing along the approach) */
  { const yaw = Math.atan2(Math.sin(HEADING), -Math.cos(HEADING)), st = settleLM((x, z) => moon.heightAt(x, z), 0, 0, yaw);
    lmDown.rotation.set(st.pitch, yaw, st.roll); lmDown.position.set(0, st.y, 0); qStand.setFromEuler(lmDown.rotation);
    D = makeAscent(moon.manifest, st.y, (x, z) => moon.heightAt(x, z)); }
  END = GETA.insert + 45; H0 = D.height({ r: D.LAND });          /* heights are over where it stood */
  cur = D.state(S.t); padH = D.height(cur) - H0;
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__liftoff = { S, OUT, GETA, nearG, lmShadow, lmUp, lmDown, clipUp, clipDown, ctl, wrist, toast, clock, get chat() { return chat; }, settings, info, renderer, camera, rig, head, lmc, vehicle, hands, SEAT, afterFrame: null, restart,
  get END() { return END; }, get D() { return D; }, get cur() { return cur; }, get padH() { return padH; }, get radio() { return radio; }, get moon() { return moon; } };
