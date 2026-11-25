/* Journey to the Moon, chapter 4: into the LM.
 *
 * GET 106:40 to 110:05, in the descent orbit. You start in your couch in the Command Module; the tunnel to the
 * Lunar Module is open (the Lunar Module Pilot has gone ahead). Pull yourself through it hand over hand (grip and
 * pull; or A to float through), down into the LM, where you stand at the Commander's station with the LMP beside
 * you. Undocking at GET 107:47:56: the Command Module backs away overhead (look up through the docking window),
 * comes round in front to look at your legs, then opens the distance. Then the wait for the go for powered
 * descent, where the chapter ends.
 *
 * Frames: the flight is chapter 3's (lunar-space.js, stage 0's site frame, stage 0's Moon). The LM flies upright
 * (its top, and the docked CSM, away from the Moon) with its windows ahead along the track; the CSM is placed
 * relative to it. The Command Module's cabin (cabin.js), the tunnel and the LM's cabin (lm-cabin.js) are drawn
 * last, as the occluder with windows, as in the other chapters.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)  ?lm (start standing in the LM)
 *      ?float=z (floating on the tunnel's axis at cabin z, for pictures) */
import * as THREE from 'three';
import { state as ascentState } from './ascent.js?v=13';
import { loadMoon } from './moon.js?v=3';
import { makeSky } from './extras.js?v=4';
import { makeLunarFlight, GETL } from './lunar-space.js?v=13';
import { makeCabin } from './cabin.js?v=13';
import { makeLMCabin, LMC } from './lm-cabin.js?v=13';
import { makeLaunchSound } from './launch-sound.js?v=13';
import { makeRadio } from './radio.js?v=13';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=13';
import { createHand } from './hands.js?v=13';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;

const START = 384000, UNDOCK = 107 * 3600 + 47 * 60 + 56, SEP = UNDOCK + 1800, END = 396300;
const WARPS = [1, 10, 100, 1000, 10000];
const HOLD = [[UNDOCK - 130, UNDOCK + 440]];
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
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, ended: false, mode: 'cm', auto: false, arrivedAt: -1, hatchShut: false, held: false };

/* ---------------------------------------------------------------- the two spacecraft and their insides */
const vehicle = makeVehicle(renderer);
const near = new THREE.Group(); scene.add(near);
near.add(vehicle.group, vehicle.lights);
vehicle.lm.userData.setLegs(true);
const vSun = vehicle.lights.children.find(l => l.isDirectionalLight), vHemi = vehicle.lights.children.find(l => l.isHemisphereLight);
const cabin = makeCabin();
cabin.setHelmets(false); cabin.setCovers(false); cabin.setTunnelOpen(true);
if (cabin.crewmen[1]) cabin.crewmen[1].visible = false;                 /* the LMP has gone ahead */
const cabinHolder = new THREE.Group(); cabinHolder.matrixAutoUpdate = false; scene.add(cabinHolder);
cabinHolder.add(cabin.group);
const lmc = makeLMCabin();
lmc.group.matrixAutoUpdate = false; scene.add(lmc.group);
lmc.setHatchOpen(true);
const tunnel = new THREE.Group(); cabinHolder.add(tunnel);

const attPad = new THREE.Quaternion();
const C = new THREE.Matrix4(), D = new THREE.Matrix4(), Dinv = new THREE.Matrix4();   /* cabin in the CSM; LM in the CSM */
let zH = -3;                                                                            /* the LM's ceiling, in cabin z */
const OUT = { want: false, on: false, az: -1.25, el: 0.25, dist: 26 };
const sound = makeLaunchSound();
let radio = null, moon = null, sky = null, L = null;
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const FADE = { k: 0, target: 0 };

/* ---------------------------------------------------------------- HUD */
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
function phase(t) {
  if (S.mode !== 'lm') return 'go through to the LM';
  if (t < UNDOCK) return 'in the LM, docked';
  if (t < SEP) return 'undocked, America alongside';
  return 'waiting for powered descent';
}
const hud = (() => {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.08), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false }));
  mesh.renderOrder = 999;
  let last = 0;
  return { mesh, on: true, draw(now, w) {
    if (now - last < 500 || !w) return; last = now;
    g.clearRect(0, 0, 512, 256); g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(0, 0, 512, 256);
    g.fillStyle = S.fps > 68 ? '#9fe6a0' : S.fps > 55 ? '#f0d070' : '#ff8a70'; g.font = 'bold 54px monospace'; g.fillText(S.fps.toFixed(0) + ' fps', 20, 64);
    g.fillStyle = '#d8d8d8'; g.font = '26px monospace';
    g.fillText('GET ' + fmt(S.t) + (S.warp > 1 ? '  x' + S.warp : '') + (S.held ? '  HOLD' : ''), 20, 108);
    g.fillText('above the Moon ' + (w.alt / 1000).toFixed(0) + ' km' + (w.hidden ? '  LOS' : ''), 20, 146);
    if (S.ended) { g.fillStyle = '#f0d070'; g.fillText('End of chapter 4.  A: the surface', 20, 184); }
    else g.fillText(phase(S.t), 20, 184);
    g.fillStyle = '#8a8a8a'; g.font = '19px monospace';
    g.fillText(S.mode === 'lm' ? 'A/X time  hold B outside' : 'grip: pull yourself  A: float through', 12, 222);
    g.fillText('stick press: L panel, R re-seat', 12, 246);
    tex.needsUpdate = true;
  } };
})();

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyF' && S.mode !== 'lm') { S.auto = !S.auto; if (S.mode === 'cm') beginCrawl(); }
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter('index.html');
  const jump = { Digit1: START, Digit2: UNDOCK - 300, Digit3: UNDOCK + 320, Digit4: END - 400 }[e.code];
  if (jump !== undefined) { setTime(jump); if (jump > START) enterLM(true); }
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const held = t => HOLD.some(([a, b]) => t > a && t < b);
function cycleWarp() { if (held(S.t)) { S.warp = S.warp === 1 ? 10 : 1; return; } S.warp = WARPS[(WARPS.indexOf(S.warp) + 1) % WARPS.length]; }
function setTime(t) { S.t = t; S.warp = 1; S.ended = false; }
function nextChapter(href) { const s = renderer.xr.getSession(), go = () => { location.href = href; }; if (s) s.end().then(go, go); else go(); }
const STICK = { x: 0, y: 0, ry: 0 };
const pad = { prev: {}, grip: {} };
function readControllers() {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed, st = gp.buttons[3]?.pressed;
    pad.grip[k] = !!gp.buttons[1]?.pressed;
    if (pad.grip[k] && S.mode === 'cm') beginCrawl();                     /* taking hold: you leave the couch */
    if (a && !pad.prev[k + 'a']) {
      if (S.ended) nextChapter('index.html');
      else if (S.mode !== 'lm') { S.auto = !S.auto; if (S.mode === 'cm') beginCrawl(); }
      else cycleWarp();
    }
    if (k === 'right') OUT.want = !!b && S.mode === 'lm';
    if (st && !pad.prev[k + 's']) { if (k === 'left') hud.on = !hud.on; else SEAT.pending = 1; }
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b; pad.prev[k + 's'] = st;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (k === 'left') { STICK.x = ax[0]; STICK.y = ax[1]; } else STICK.ry = ax[1];
  }
}

/* seated in the couch (the couch brought to you, as launch.js), or standing in the LM (only turned and moved
 * across the floor: your own height stays) */
const SEAT = { pending: 0, done: false };
const _e = new THREE.Euler();
function seat() {
  _e.setFromQuaternion(camera.quaternion, 'YXZ');
  const yaw = _e.y, c = Math.cos(-yaw), s = Math.sin(-yaw), p = camera.position;
  head.rotation.set(0, -yaw, 0);
  head.position.set(-(c * p.x + s * p.z), S.mode === 'lm' ? 0 : -p.y, -(-s * p.x + c * p.z));
  SEAT.done = true;
}

/* ---------------------------------------------------------------- floating through the tunnel */
const body = new THREE.Vector3(), vel = new THREE.Vector3(), grab = {}, _hw = new THREE.Vector3(), _hc = new THREE.Vector3();
const cabinWorld = new THREE.Matrix4(), cabinInv = new THREE.Matrix4(), _hr = new THREE.Vector3(), _d = new THREE.Vector3();
function beginCrawl() { if (S.mode !== 'cm') return; S.mode = 'crawl'; cabin.setBody(false); vel.set(0, 0, 0); }
function headRel(out) { return out.copy(camera.position).applyQuaternion(head.quaternion).add(head.position); }
function clampHead(hp) {
  const dx = hp.x - AX, dy = hp.y - AY, rho = Math.hypot(dx, dy);
  const maxR = hp.z >= Z_APEX ? Math.max(0.2, rAt(hp.z) - 0.18) : 0.24;
  if (rho > maxR) { const k = maxR / rho; hp.x = AX + dx * k; hp.y = AY + dy * k; }
  hp.z = Math.min(0.4, Math.max(zH - 0.6, hp.z));
  return hp;
}
function updateFloat(dt) {
  if (S.mode !== 'crawl') return;
  const xr = renderer.xr.isPresenting;
  /* hand over hand: a gripping hand stays where it took hold, so you move the other way */
  let n = 0; _d.set(0, 0, 0);
  if (xr) for (let i = 0; i < 2; i++) {
    const src = renderer.xr.getSession()?.inputSources[i]; if (!src) continue;
    const k = src.handedness, gp = renderer.xr.getControllerGrip(i);
    /* the hand in the cabin's frame, from its pose in yours (not the world: the cabin moves 1.6 km/s through it) */
    _hc.copy(gp.position).applyQuaternion(head.quaternion).add(head.position).add(body);
    if (pad.grip[k]) { if (!grab[k]) grab[k] = _hc.clone(); else { _d.add(_hc.clone().sub(grab[k])); n++; } }
    else grab[k] = null;
  }
  if (n) { _d.divideScalar(n); body.sub(_d); vel.copy(_d).multiplyScalar(-1 / Math.max(dt, 0.011)).clampLength(0, 1.2); S.auto = false; }
  else {
    /* drifting, slowing gently; or the stick; or the automatic float along the tunnel */
    const fwd = _hr.set(0, 0, -1).applyQuaternion(camera.quaternion).applyQuaternion(head.quaternion);
    let push = 0;
    if (xr) push = -STICK.y; else push = (keys.ArrowUp ? 1 : 0) - (keys.ArrowDown ? 1 : 0);
    if (Math.abs(push) > 0.15) { vel.addScaledVector(fwd, push * 0.8 * dt); S.auto = false; }
    if (S.auto) {
      headRel(_d).add(body);
      const target = _d.z > Z_APEX + 0.25 ? new THREE.Vector3(AX, AY, Z_APEX + 0.1) : new THREE.Vector3(AX, AY, zH - 0.6);
      const to = target.sub(_d), dist = to.length();
      vel.lerp(to.normalize().multiplyScalar(Math.min(0.32, dist + 0.05)), Math.min(1, dt * 2));
    } else vel.multiplyScalar(Math.exp(-0.7 * dt));
    vel.clampLength(0, 0.8);
    body.addScaledVector(vel, dt);
  }
  /* stay inside: the cone, the tunnel */
  headRel(_d); const hp = _d.clone().add(body), before = hp.clone();
  clampHead(hp); body.add(hp.sub(before));
  for (const k in grab) if (grab[k]) grab[k].add(hp);                 /* a clamped hand slides along the wall */
  /* through the LM's hatch and down into its cabin: arrived */
  if (before.z < zH - 0.35) arrive();
}
function arrive() {
  if (S.mode !== 'crawl') return;
  S.mode = 'arriving'; FADE.target = 1;
}
function enterLM(now = false) {
  S.mode = 'lm'; S.auto = false; S.arrivedAt = S.t; body.set(0, 0, 0); vel.set(0, 0, 0);
  cabin.group.visible = false;
  if (renderer.xr.isPresenting) SEAT.pending = 1;
  if (now) { S.hatchShut = S.t > START + 600; }
  else if (radio) radio.cue('arrive');
}

/* ---------------------------------------------------------------- the two ships' places */
const _p = new THREE.Vector3(), _up = new THREE.Vector3(), _fw = new THREE.Vector3(), _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3();
const _m = new THREE.Matrix4(), _m2 = new THREE.Matrix4(), _q = new THREE.Quaternion(), _s = new THREE.Vector3();
const LMworld = new THREE.Matrix4(), CSMworld = new THREE.Matrix4(), lmAtt = new THREE.Quaternion(), lmWant = new THREE.Quaternion();
const smooth = (a, b, x) => { const k = Math.min(1, Math.max(0, (x - a) / (b - a))); return k * k * (3 - 2 * k); };
const PIVOT = new THREE.Vector3(), _fp = new THREE.Vector3();
/* where the CSM is from the LM (LM coordinates), and how far it has turned to face it */
function csmRel(t, out) {
  const d = t - UNDOCK;
  if (d <= 0) return [out.set(0, 0, 0), 0];
  if (d < 60) return [out.set(0, 0.2 * d, 0), 0];
  /* to the formation point: ahead, to the right and below, where both triangular windows look (their use was the
   * view forward and down); the CSM's centre goes there, turned nose towards you */
  const k = smooth(60, 330, d);
  out.set(0, 12, 0).lerp(_fp.set(-9, -3, 30).sub(PIVOT), k);
  if (t > SEP) { const s = t - SEP; out.x -= 0.25 * s; out.y -= 0.1 * s; out.z += 0.8 * s; }
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
    .multiply(_m.makeRotationX(turn)).multiply(_m.makeTranslation(-PIVOT.x, -PIVOT.y, -PIVOT.z)).multiply(Dinv);
  CSMworld.decompose(vehicle.group.position, vehicle.group.quaternion, _s);
  vehicle.lights.position.copy(_p);
  cabinWorld.multiplyMatrices(CSMworld, C); cabinInv.copy(cabinWorld).invert();
  cabinHolder.matrix.copy(cabinWorld);
  lmc.group.matrix.copy(LMworld);
  return rel.length();
}
const standM = new THREE.Matrix4();
function placeRig() {
  if (OUT.on) return placeOutside();
  if (S.mode === 'lm') {
    standM.copy(LMworld).multiply(_m.makeTranslation(LMC.CDR.x, LMC.FLOOR, LMC.CDR.z)).multiply(_m2.makeRotationY(Math.PI));
    standM.decompose(rig.position, rig.quaternion, _s);
  } else {
    _m.copy(cabinWorld).multiply(_m2.makeTranslation(body.x, body.y, body.z));
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
  else if (FADE.target === 1 && FADE.k > 0.9 && S.mode !== 'arriving') FADE.target = 0;
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
  const panels = S.mode === 'lm' ? lmc.panels : cabin.panels;
  for (let i = 0; i < 2; i++) {
    const src = s.inputSources[i]; if (!src || !src.gamepad) continue;
    const m = hands[src.handedness]; if (!m) continue;
    const grip = renderer.xr.getControllerGrip(i); if (m.parent !== grip) grip.add(m);
    m.visible = !OUT.on;
    const gp = src.gamepad, b = k => gp.buttons[k] || null;
    m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed)), false, dt);
    m.userData.tip.getWorldPosition(_tip);
    let hit = false;
    if (!OUT.on && S.mode !== 'crawl') for (const pnl of panels) { _loc.copy(_tip); pnl.mesh.worldToLocal(_loc); if (Math.abs(_loc.x) < pnl.w / 2 && Math.abs(_loc.y) < pnl.h / 2 && Math.abs(_loc.z) < 0.03) hit = true; }
    m.userData.warn = hit ? 0.4 : Math.max(0, (m.userData.warn || 0) - dt); m.userData.setTint(m.userData.warn > 0);
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
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) {
    renderer.xr.updateCamera(camera);
    if (SEAT.pending && ++SEAT.pending > 4) { seat(); SEAT.pending = 0; scene.updateMatrixWorld(true); renderer.xr.updateCamera(camera); }
  }
  _cam.setFromMatrixPosition(camera.matrixWorld);
  moon.setDetail(_cam, false);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const undocked = S.t >= UNDOCK;
  const inside = !OUT.on;
  const show = (c, s, m, n) => {
    cabinHolder.visible = c && inside && !undocked;
    cabin.group.visible = c && inside && S.mode !== 'lm';
    tunnel.visible = c && inside && !S.hatchShut;
    lmc.group.visible = c && inside;
    hud.mesh.visible = c && hud.on; sign.visible = c && SIGN.k > 0.01;
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

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t, first = true;
const _sun = new THREE.Vector3(), _a = new THREE.Vector3(), _iq = new THREE.Quaternion();
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  if (xr) readControllers();
  /* the clock; it holds ten minutes before undocking until you are in the LM */
  S.held = false;
  if (S.running && !S.still && !S.ended) {
    let nt = S.t + dt * S.warp;
    if (S.warp > 10) for (const [a] of HOLD) if (S.t < a && nt >= a) { nt = a; S.warp = 1; }
    if (held(nt) && S.warp > 10) S.warp = 1;
    if (S.mode !== 'lm' && nt > UNDOCK - 600) { nt = Math.max(S.t, UNDOCK - 600); S.warp = 1; S.held = true; }
    S.t = Math.min(END, nt);
    if (S.t >= END) { S.ended = true; S.running = false; hud.on = true; }
  }
  /* the arrival: through black, standing at your station */
  if (S.mode === 'arriving' && FADE.k > 0.92) { enterLM(); FADE.target = 0; }
  /* the LM's hatch shut behind you, the tunnel closed off (the CMP refits the drogue and his hatch) */
  if (S.mode === 'lm' && !S.hatchShut && S.t > S.arrivedAt + 25) { S.hatchShut = true; sound.thump(0.3, 0.2, 900); }
  lmc.setHatchOpen(!S.hatchShut);
  w = L.where(S.t);
  moon.setSunOffset(L.sunOffset(S.t));
  _sun.copy(moon.shared.uSun.value);
  sky.userData.setSpin(W_EARTH * (S.t - GETL.land) + 2.1);
  const dist = place(w, dt, first); first = false;
  /* the vehicle: the CSM with its cabin hidden while you are in it; the LM outside hidden while you are in it */
  vehicle.update(-1e9, (S.mode === 'cm' || S.mode === 'crawl' || S.mode === 'arriving') && !OUT.on ? 'inside' : 'outside', { alt: 1e9, up: w.up });
  _m.copy(CSMworld).invert().multiply(LMworld);
  vehicle.space({ sivb: null, lm: _m, open: 1, gone: 1e6, sps: 0, j2: 0 });
  vehicle.lm.visible = OUT.on || S.mode === 'cm';
  vehicle.parts.csm.visible = dist < 30000;
  updateFloat(dt);
  placeRig();
  /* sunlight or the Moon's shadow */
  const along = _a.fromArray(w.r).dot(_sun), perp = Math.sqrt(Math.max(0, w.rC * w.rC - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - L.R) / 30000 + 0.5));
  vSun.position.copy(_sun).multiplyScalar(100); vSun.intensity = 2.6 * lit;
  vHemi.position.fromArray(w.up); vHemi.color.setRGB(0.05, 0.05, 0.06); vHemi.groundColor.setRGB(0.55, 0.53, 0.5);
  vHemi.intensity = 0.15 + 0.6 * Math.max(0, _a.fromArray(w.up).dot(_sun) + 0.2);
  cabinWorld.decompose(_x, _q, _s); cabin.setDaylight(lit, _y.copy(_sun).applyQuaternion(_iq.copy(_q).invert()));
  lmc.setDaylight(lit, _y.copy(_sun).applyQuaternion(_iq.copy(lmAtt).invert()));
  /* undocking: the latches let go */
  if (prevT < UNDOCK && S.t >= UNDOCK && S.warp <= 10) sound.thump(0.6, 0.35, 650);
  if (prevT < SEP && S.t >= SEP && S.warp <= 10) sound.thump(0.2, 0.6, 300);
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
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000); lmc.update(now / 1000);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, S.mode === 'lm' && !OUT.on ? 1.62 : 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? 'End of chapter 4. The descent is still to come; A (N on this screen) takes you to the surface now.' : S.held ? 'Go through to the LM first: grip and pull yourself along, or A (F on this screen) to float through.' : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  hud.draw(now, w);
  render();
  S.frames++;
  if (window.__lm.afterFrame) { const f = window.__lm.afterFrame; window.__lm.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx && !radio) radio = await makeRadio(ctx, sound.out, new URL('../assets/script/lm.json', import.meta.url).href);
}
function readOptions() {
  const v = $('start') ? +$('start').value : S.t;
  setTime(v);
  if (v > START) enterLM(true); else { S.mode = 'cm'; cabin.group.visible = true; cabin.setBody(true); body.set(0, 0, 0); }
}
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
  { const s0 = ascentState(-60); _x.fromArray(s0.nose); _y.fromArray(s0.head);
    const nose = _x.clone(), hd = _y.clone().addScaledVector(nose, -_y.dot(nose)).normalize(), zz = nose.clone().negate(), xx = new THREE.Vector3().crossVectors(hd, zz).normalize();
    attPad.setFromRotationMatrix(_m.makeBasis(xx, hd, zz)); }
  C.compose(EYE_OFF, attPad, _s.set(1, 1, 1));
  { const md = new THREE.Matrix4(); vehicle.csmInSivb(vehicle.DOCK_DIST, Math.PI, md); D.copy(md).invert().multiply(vehicle.LM_REST); Dinv.copy(D).invert(); }
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
  if (Q.has('lm') || S.t > START + 1) enterLM(true);
  if (Q.has('float')) { S.mode = 'crawl'; cabin.setBody(false); body.set(AX, AY, num('float', -2)); }       /* test aid: floating on the tunnel's axis at cabin z */
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

window.__lm = { S, OUT, END, UNDOCK, SEP, renderer, camera, rig, head, cabin, lmc, vehicle, hands, SEAT, body, afterFrame: null, setTime,
  get zH() { return zH; }, get radio() { return radio; }, get L() { return L; }, beginCrawl, enterLM, tunnel };
