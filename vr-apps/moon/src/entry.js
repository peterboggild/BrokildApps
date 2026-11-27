/* Journey to the Moon, chapters 11 and 12: the entry, and the parachutes and the splashdown.
 *
 * One module for two pages (document.body.dataset.chapter). Chapter 11, entry.html: from the casting off of the
 * Service Module, a quarter of an hour out, through the entry interface at 11 km/s to the moment before the
 * parachutes: the glow that begins as a pale light in the windows and grows to a fire you fly inside, the load
 * that comes on to six and a half g, the radio gone, then back. Chapter 12, splash.html: the drogues at 7 km, the
 * three main parachutes at 3 km, the long ride down under them, the sea, and the capsule riding the swell. The end.
 *
 * The flight is entry-space.js (the capsule alone against the air, worked out step by step). The cabin, the
 * outside (earth.js), the vehicle, the hands and the outside view are chapter 2's, which this was made from. New
 * here: the fire (a shell of light round the capsule, streaming back, and sparks of the heat shield's char), the
 * load (the cabin shakes, you are pressed back, your sight narrows), the parachutes, the sea.
 *
 * URL: ?t= (GET, seconds)  ?yaw= &pitch=  ?still  ?shot  ?out (&od= &oa= &oe=)  ?cheat */
import * as THREE from 'three';
import { state as ascentState, SUN, RE } from './ascent.js?v=41';
import { NORMAL, W_EARTH, R_MOON } from './space.js?v=41';
import { where as coastWhere, GETH } from './home-space.js?v=41';
import { makeEntry, GETE } from './entry-space.js?v=41';
import { makeOutside } from './earth.js?v=41';
import { makeCabin } from './cabin.js?v=41';
import { makeLaunchSound } from './launch-sound.js?v=41';
import { makeRadio, combineRadios } from './radio.js?v=41';
import { makeClock } from './clock.js?v=41';
import { makeChatter } from './chatter.js?v=41';
import { handOnCrew } from './crew-life.js?v=41';
import { makeVehicle, EYE_OFF } from './vehicle.js?v=41';
import { createHand, makeControllers } from './hands.js?v=41';
import { makeWrist, makeToast, makeWarpBadge, makeCard, nextOf, fmtDist, fmtSpeed } from './wrist.js?v=41';
import { makeKeysBand, seatKeys } from './keysband.js?v=41';
import { saveProgress, offerContinue, autoEnter } from './progress.js?v=41';
import { settings, applyCommon, holdsFor, watchRecentre } from './settings.js?v=41';
import { makeFloaters } from './floaters.js?v=41';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);

const CH12 = document.body.dataset.chapter === '12';
const FLIGHT = makeEntry(), MOON_AT = coastWhere(GETH.entry).moon;
/* (with the Moon where the coast left it, 380,000 km off: the far pass sets its depths by it) */
const where = t => { const w = FLIGHT.where(t); w.moon = MOON_AT; w.dMoon = Math.hypot(MOON_AT[0] - w.rE[0], MOON_AT[1] - w.rE[1], MOON_AT[2] - w.rE[2]); return w; };
/* chapter 11: from a minute and a half before the Service Module goes, to two seconds before the drogues; chapter 12:
 * from 25 s before the drogues to a minute and a quarter on the sea */
const START = CH12 ? GETE.drogue - 25 : GETE.sep - 90, END = CH12 ? GETE.splash + 75 : GETE.drogue - 2;
const PAGE = CH12 ? 'splash.html' : 'entry.html', NEXT = CH12 ? 'index.html' : 'splash.html';
const WARPS = [1, 10];
/* real time (ten times at most) round the casting off, and from a minute before the air to the chapter's end; under
 * the parachutes the long ride down may be run faster, but not their opening nor the last of it */
const HOLD_ALL = CH12 ? [[START, GETE.main + 40, 'the parachutes'], [GETE.splash - 45, END, 'the splashdown']] : [[GETE.sep - 25, GETE.sep + 30, 'the Service Module is cast off'], [GETE.ei - 60, END, 'the entry']];
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
  fps: 0, calls: 0, tris: 0, frames: 0, ready: false, roll: 0, ended: false, snap: true, cues: {}, temp: 22 };

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
const band = makeKeysBand({ settings: ['ff', 'turn', 'hz', 'fov', 'voices', 'sound', 'text', 'body', 'hand'] });          /* on a computer screen: the keys along the top, the status and settings on P */
ctl.grips.left.add(wrist.mesh);
const toast = makeToast(); head.add(toast.mesh);
const card = makeCard(); head.add(card.mesh); card.flat = () => band.flat;                 /* the chapter's title, and at its end the way on */
const badge = makeWarpBadge(); head.add(badge.mesh);          /* the small clock while time runs fast */
const EVENTS = CH12 ? [[GETE.drogue, 'the drogue parachutes'], [GETE.main, 'the three main parachutes'], [GETE.splash, 'splashdown']]
  : [[GETE.sep, 'the Service Module is cast off'], [GETE.ei, 'the entry interface, 122 km up'], [GETE.blackout, 'the radio is lost'], [GETE.peak, 'the heaviest of the load'], [GETE.blackoutEnd, 'the radio comes back'], [END, 'the parachutes (chapter 12)']];
function info() {
  const hot = Math.round(w.heat * 2700);
  return { chapter: CH12 ? 'Chapter 12 \u00b7 The splashdown' : 'Chapter 11 \u00b7 The entry', now: phase(S.t), next: S.ended ? null : nextOf(EVENTS, S.t), after: CH12 ? 'home' : 'the parachutes', get: S.t, warp: S.warp,
    rows: [['Height', w.alt > 2000 ? fmtDist(w.alt) : Math.max(0, w.alt).toFixed(0) + ' m'], ['Speed', fmtSpeed(w.speed)], ['Load', w.g.toFixed(1) + ' g'],
      CH12 ? ['Coming down', Math.max(0, -w.climb).toFixed(0) + ' m/s'] : ['Heat shield', hot > 150 ? hot.toLocaleString('en-GB') + ' \u00b0C' : 'cold'], ['Cabin', S.temp.toFixed(0) + ' \u00b0C'], ['Radio', blackout(S.t) ? 'lost in the fire (blackout)' : 'in touch with Houston']],
    alert: S.ended ? (CH12 ? 'The end. Apollo 17 is home.  A: back to the chapters' : 'End of chapter 11.  A: on to chapter 12') : null,
    keys: [['A', 'slower (hold: real time)'], ['B', 'faster'], ['X', 'cheat window'], ['Y', 'outside view'], ['L stick', 'press: panel, settings'], ['R stick', 'press: re-seat']], fps: S.fps };
}
const blackout = t => t > GETE.blackout && t < GETE.blackoutEnd;
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = Math.floor(s % 60); return h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
const km = m => m > 1e6 ? (m / 1e6).toFixed(m > 1e8 ? 0 : 1) + 'k km' : (m / 1000).toFixed(0) + ' km';
function phase(t) {
  if (t >= GETE.splash) return 'on the sea';
  if (t >= GETE.main) return 'under the three main parachutes';
  if (t >= GETE.drogue) return 'under the drogue parachutes';
  if (t < GETE.sep) return 'the last minutes of the coast';
  if (t < GETE.ei) return 'the Command Module alone, heat shield forward';
  if (blackout(t)) return 'in the fire';
  return t < GETE.blackoutEnd ? 'the first of the air' : 'falling, the fire behind you';
}

/* ---------------------------------------------------------------- on a computer screen: the keys that do something now, and what to do */
function pcKeys() { return seatKeys(S, { win: true, out: OUT, jump: CH12 ? '1–3' : '1–5', next: CH12 ? 'back to the chapters' : 'on to chapter 12, the splashdown' }); }
function pcHint() { return S.ended ? (CH12 ? 'Apollo 17 is home. N: back to the chapters.' : 'End of chapter 11. N goes on to chapter 12, the splashdown.') : S.running ? null : 'Paused. Space goes on.'; }

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startSound(); S.running = !S.running; }
  if (e.code === 'KeyW') cycleWarp();
  if (e.code === 'KeyS') clock.slower(S);
  if (e.code === 'KeyP') { if (band.flat) band.toggle(); else wrist.press(); }
  if (e.code === 'KeyC') cabin.setCheat(!cabin.cheat);
  if (e.code === 'KeyO') OUT.want = !OUT.want;
  if (e.code === 'KeyN' && S.ended) nextChapter(NEXT);
  const jump = (CH12 ? { Digit1: START, Digit2: GETE.main - 12, Digit3: GETE.splash - 40 } : { Digit1: START, Digit2: GETE.ei - 30, Digit3: GETE.ei + 45, Digit4: GETE.peak - 12, Digit5: GETE.blackoutEnd - 15 })[e.code];
  if (jump !== undefined) setTime(jump);
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const clock = makeClock({ key: () => (band.flat ? 'W' : 'B'), holds, lines: () => radio ? radio.lines : [], warps: WARPS, cap: 10, toast });
const held = t => !!clock.inHold(t);
function cycleWarp() { clock.faster(S); }
function realTime() { if (S.warp > 1) { S.warp = 1; toast.say('Real time', 1.5, true); } }
function setTime(t) { S.t = t; prevT = t; S.warp = 1; S.ended = false; S.snap = true; S.cues = {}; S.temp = 22; clock.reset(); }
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
      if (a && !pad.prev.ra) { pad.aDown = now; if (S.ended) nextChapter(NEXT); }
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

/* ---------------------------------------------------------------- attitude */
const _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion();
const V = a => new THREE.Vector3().fromArray(a);
function fromNoseHead(nose, headV, out) {
  _y.copy(headV).addScaledVector(nose, -headV.dot(nose)).normalize(); _z.copy(nose).negate(); _x.crossVectors(_y, _z).normalize();
  return out.setFromRotationMatrix(_m.makeBasis(_x, _y, _z));
}
const SUNV = V(SUN), NRM = V(NORMAL);
/* The attitude. Coasting in: the nose along the fall. For the entry: the heat shield forward and a little up (the
 * capsule flies tilted, which is its lift), so the nose and your windows look back up the way you came, your head
 * towards the Earth (heads down, as they flew it; the horizon is in the windows until the fire hides it). Under the
 * parachutes the capsule hangs with its nose up and swings a little; on the sea it rides the swell. */
function attitudeAt(t, w, out) {
  const up = V(w.up), vel = V(w.v).normalize();
  if (t < GETE.sep + 20) return fromNoseHead(vel, up.clone().negate(), out);
  if (t < GETE.drogue) { _x.copy(vel).negate().addScaledVector(up, 0.3).normalize(); return fromNoseHead(_x, up.clone().negate(), out); }
  const fw = _y.copy(vel).addScaledVector(up, -vel.dot(up)); if (fw.lengthSq() < 1e-6) fw.set(1, 0, 0).addScaledVector(up, -up.x); fw.normalize();
  const swing = t < GETE.splash ? 0.09 * Math.sin(t * 0.9) * Math.exp(-Math.max(0, t - GETE.main) / 60) + 0.035 * Math.sin(t * 0.47) : 0.07 * Math.sin(t * 1.15) + 0.03 * Math.sin(t * 0.63 + 1);
  const hang = t < GETE.splash ? 0.47 : 0.12;                                   /* (27 degrees off the upright on the lines) */
  _x.copy(up).multiplyScalar(Math.cos(hang + swing)).addScaledVector(fw, Math.sin(hang + swing));
  return fromNoseHead(_x, fw.negate(), out);
}

/* the vehicle: the Command and Service Modules; after the casting off the Command Module alone */
const _mt = new THREE.Matrix4().makeTranslation(0, -1e7, 0);
function configure() { const td = { sivb: null, lm: _mt, open: 1, gone: 1e6, sps: 0, j2: 0 }; vehicle.space(td); vehicle.setProbe(0); return td; }

/* ---------------------------------------------------------------- the fire
 * The air ahead of the heat shield is squeezed until it glows, and streams back round the capsule: from your
 * couch, looking back up the path, you are inside a tube of light that narrows into the wake. It begins as a faint
 * rosy light, turns orange, and at the height of it is nearly white. A shell round the capsule, drawn before
 * everything near (so the hull and the cabin are in front of it), drawn out into a tail down the wake, soft at its
 * edge, brightest towards the shield and streaming towards the nose (its own +y); and sparks: glowing flakes of the shield's char going by. uHeat: 0 to 1 (entry-space.js). */
const fireU = { uT: { value: 0 }, uHeat: { value: 0 } };
const fire = new THREE.Mesh(new THREE.SphereGeometry(9, 48, 32), new THREE.ShaderMaterial({
  uniforms: fireU, side: THREE.BackSide, transparent: true, depthTest: false, depthWrite: false, blending: THREE.AdditiveBlending,
  vertexShader: 'varying vec3 vP; varying float vEdge; void main() { vP = normalize(position); vec4 mv = modelViewMatrix * vec4(position, 1.0); vEdge = abs(dot(normalize(normalMatrix * normal), normalize(mv.xyz))); gl_Position = projectionMatrix * mv; }',
  fragmentShader: `
    uniform float uT, uHeat; varying vec3 vP; varying float vEdge;
    float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
    float noise(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
      return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y); }
    void main() {
      if (uHeat < 0.004) discard;
      vec3 v = normalize(vP), a = vec3(0.0, 1.0, 0.0);
      float c = dot(v, a), sAng = acos(clamp(c, -1.0, 1.0));                 /* 0: straight back down the wake; pi: the shield */
      vec3 e1 = normalize(cross(a, vec3(0.31, 0.87, 0.38))), e2 = cross(a, e1);
      float phi = atan(dot(v, e2), dot(v, e1));
      /* streamers running from the shield's rim back into the wake, of two widths, hurrying; they wrap round (phi) */
      float px = phi * 3.0, flow = uT * (3.0 + 5.0 * uHeat);
      float n1 = noise(vec2(px * 3.0, sAng * 2.2 + flow)) * 0.6 + noise(vec2(px * 9.0 + 4.0, sAng * 5.0 + flow * 1.7)) * 0.4;
      float n2 = noise(vec2(px * 22.0, sAng * 9.0 + flow * 2.6));
      float streak = pow(n1, 1.6) * 0.9 + n2 * n2 * 0.35;
      /* brightest round the shield's rim, thinning into the wake, a dark core far down it */
      float rim = smoothstep(0.15, 2.2, sAng), core = smoothstep(0.0, 0.5, sAng);
      float body = (0.42 + 0.58 * rim) * (0.45 + 0.75 * streak) * (0.72 + 0.28 * core);
      float flick = 0.9 + 0.06 * sin(uT * 37.0) + 0.04 * sin(uT * 91.0 + phi * 3.0);
      float h = uHeat;
      vec3 rose = vec3(0.85, 0.42, 0.55), orange = vec3(1.0, 0.36, 0.1), white = vec3(1.0, 0.86, 0.72);
      vec3 col = mix(rose, orange, smoothstep(0.03, 0.3, h)); col = mix(col, white, smoothstep(0.45, 1.0, h) * (0.45 + 0.55 * rim));
      float I = pow(h, 0.75) * body * flick * (0.95 + 0.55 * h) * smoothstep(0.0, 0.45, vEdge);          /* (not so bright that the streamers are lost in white; soft where you look along its skin) */
      gl_FragColor = vec4(col * I, 1.0);
    }` }));
fire.frustumCulled = false; fire.renderOrder = -10; fire.visible = false; fire.scale.set(1, 2.7, 1);
const SPARKS = 260, sparkU = { uT: { value: 0 }, uHeat: { value: 0 }, uPx: { value: 900 } };
const sparkGeo = new THREE.BufferGeometry();
{ const p = new Float32Array(SPARKS * 3), sd = new Float32Array(SPARKS * 3);
  for (let i = 0; i < SPARKS; i++) { const a = Math.random() * Math.PI * 2, r = 2.3 + Math.random() * 2.6; p[i * 3] = Math.cos(a) * r; p[i * 3 + 1] = 0; p[i * 3 + 2] = Math.sin(a) * r; sd[i * 3] = Math.random(); sd[i * 3 + 1] = 0.6 + Math.random(); sd[i * 3 + 2] = Math.random(); }
  sparkGeo.setAttribute('position', new THREE.BufferAttribute(p, 3)); sparkGeo.setAttribute('aSeed', new THREE.BufferAttribute(sd, 3)); }
/* (in their own frame: +y the way the fire streams, towards the nose) */
const sparks = new THREE.Points(sparkGeo, new THREE.ShaderMaterial({
  uniforms: sparkU, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
  vertexShader: `uniform float uT, uHeat, uPx; attribute vec3 aSeed; varying float vA;
    void main() { float k = fract(aSeed.x + uT * aSeed.y * (0.5 + 0.9 * uHeat)); vec3 p = position; p.y = -7.0 + 22.0 * k; p.xz *= 1.0 - 0.25 * k;
      vA = uHeat * smoothstep(0.0, 0.08, k) * (1.0 - smoothstep(0.6, 1.0, k)) * step(aSeed.z, 0.25 + uHeat);
      vec4 mv = modelViewMatrix * vec4(p, 1.0); gl_Position = projectionMatrix * mv; gl_PointSize = clamp(uPx * (0.012 + 0.02 * aSeed.z) / max(0.6, -mv.z), 1.0, 9.0); }`,
  fragmentShader: `varying float vA; void main() { vec2 c = gl_PointCoord - 0.5; float d = dot(c, c); if (vA < 0.01 || d > 0.25) discard; gl_FragColor = vec4(vec3(1.0, 0.72, 0.4) * vA * (1.0 - d * 4.0) * 1.6, 1.0); }` }));
sparks.frustumCulled = false; sparks.renderOrder = -9; sparks.visible = false;
/* the fire's light in the cabin: through the windows, flickering, from the colour of embers to nearly white */
const fireLight = new THREE.PointLight(0xff7a30, 0, 6, 1); fireLight.position.set(0.6, 0.6, -0.25);
const fireLight2 = new THREE.PointLight(0xff9a50, 0, 6, 1); fireLight2.position.set(-0.6, 0.35, 0.05);

/* ---------------------------------------------------------------- the parachutes
 * Two drogues (white, 5 m across, on long lines) steady and slow the capsule; then they are cut away and the three
 * mains come out reefed (small, so as not to burst), and open in two steps to 25 m each, orange and white. They are
 * seen through the forward windows, over your head as you lie. In the CSM's frame: +y the nose, where they fly. */
const chutes = new THREE.Group(); chutes.visible = false;
function canopy(R, gores, colA, colB) {
  const g = new THREE.SphereGeometry(R, gores * 2, 10, 0, Math.PI * 2, 0, Math.PI * 0.46), col = new Float32Array(g.attributes.position.count * 3), A = new THREE.Color(colA), B = new THREE.Color(colB), p = g.attributes.position;
  for (let i = 0; i < p.count; i++) { const a = Math.atan2(p.getZ(i), p.getX(i)), k = Math.floor((a / (Math.PI * 2) + 1) * gores) % 2, ring = p.getY(i) > R * 0.97 ? 1 : 0; (k || ring ? A : B).toArray(col, i * 3); }
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  const m = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ vertexColors: true, side: THREE.DoubleSide })); m.frustumCulled = false; return m;
}
function riser(top, R, n, mat) {
  const pts = []; for (let k = 0; k < n; k++) { const a = k / n * Math.PI * 2; pts.push(0, 0, 0, top.x + Math.cos(a) * R, top.y, top.z + Math.sin(a) * R); }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3)); const l = new THREE.LineSegments(g, mat); l.frustumCulled = false; return l;
}
const lineMat = new THREE.LineBasicMaterial({ color: 0xd9d6cc });
const drogues = [], mains = [];
for (const sx of [-1, 1]) { const g = new THREE.Group(), c = canopy(2.5, 8, 0xf2f0ea, 0xf2f0ea); c.position.set(0, 19, 0); g.add(c, riser(new THREE.Vector3(0, 19.4, 0), 2.45, 12, lineMat)); g.rotation.z = sx * 0.13; chutes.add(g); drogues.push(g); }
for (let k = 0; k < 3; k++) { const g = new THREE.Group(), c = canopy(12.5, 12, 0xe8641c, 0xf4f1ea); c.position.set(0, 38, 0); g.add(c, riser(new THREE.Vector3(0, 40, 0), 12.3, 24, lineMat));
  g.userData = { c, az: k * Math.PI * 2 / 3 + 0.4 }; chutes.add(g); mains.push(g); }
chutes.position.set(0, 2.9, 0);                       /* the top of the Command Module */
function setChutes(t) {
  chutes.visible = t >= GETE.drogue && t < GETE.splash + 6;
  if (!chutes.visible) return;
  const dk = Math.min(1, (t - GETE.drogue) / 1.6), dOn = t < GETE.main + 0.4;
  drogues.forEach((g, i) => { g.visible = dOn; g.scale.set(0.25 + 0.75 * dk, 1, 0.25 + 0.75 * dk); g.rotation.x = 0.05 * Math.sin(t * 2.1 + i * 2); });
  /* the mains: out, reefed to a fifth for six seconds, to a half for four more, then full; on the sea they fall away */
  const m = t - GETE.main, open = m < 0 ? 0 : m < 1.5 ? 0.2 * m / 1.5 : m < 6 ? 0.2 : m < 7 ? 0.2 + 0.3 * (m - 6) : m < 10 ? 0.5 : m < 11.5 ? 0.5 + 0.5 * (m - 10) / 1.5 : 1;
  const fall = Math.max(0, t - GETE.splash);
  mains.forEach((g, i) => { g.visible = m > 0; const spread = 0.34 * open, a = g.userData.az;
    g.rotation.set(Math.cos(a) * spread + 0.02 * Math.sin(t * 0.7 + i) + fall * 0.22 * Math.cos(a), 0, Math.sin(a) * spread + 0.02 * Math.sin(t * 0.83 + i * 2) + fall * 0.22 * Math.sin(a));
    g.scale.set(0.05 + 0.95 * open, Math.max(0.2, 1 - fall * 0.14), 0.05 + 0.95 * open); g.userData.c.scale.y = 0.75 + 0.25 * open; });
}

/* ---------------------------------------------------------------- the sea
 * Below 40 km the Pacific under you: a great disc of water at the sea's level, lit by the Sun and the sky, with a
 * swell that shows when you are near (from high up it only shines). It joins the Earth's own picture at its rim. */
const seaU = { uT: { value: 0 }, uSun: { value: new THREE.Vector3(0, 1, 0) }, uAlt: { value: 1000 }, uK: { value: 0 } };
const sea = new THREE.Mesh(new THREE.CircleGeometry(1, 96), new THREE.ShaderMaterial({
  uniforms: seaU, transparent: true, depthWrite: false,
  vertexShader: 'varying vec2 vP; varying vec3 vW; void main() { vP = position.xy; vec4 wp = modelMatrix * vec4(position, 1.0); vW = wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
  fragmentShader: `
    uniform float uT, uAlt, uK; uniform vec3 uSun; varying vec2 vP; varying vec3 vW;
    float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
    float noise(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
      return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y); }
    void main() {
      float r = length(vP); if (r > 1.0) discard;
      /* the water's own frame: the disc's plane; metres across it */
      vec2 m = vP * (uAlt * 14.0 + 900.0);
      float near = clamp(1.0 - uAlt / 2500.0, 0.0, 1.0);
      /* the swell and the chop: slopes from moving noise, coarser from higher up */
      float e = 1.0, sc = mix(0.004, 0.09, near);
      vec2 q = m * sc + vec2(uT * 0.25, uT * 0.17);
      float h0 = noise(q) + 0.5 * noise(q * 2.3 - uT * 0.3), hx = noise(q + vec2(0.35, 0.0)) + 0.5 * noise((q + vec2(0.35, 0.0)) * 2.3 - uT * 0.3), hy = noise(q + vec2(0.0, 0.35)) + 0.5 * noise((q + vec2(0.0, 0.35)) * 2.3 - uT * 0.3);
      vec3 n = normalize(vec3((h0 - hx) * (0.25 + 0.9 * near), (h0 - hy) * (0.25 + 0.9 * near), 1.0));
      vec3 view = normalize(cameraPosition - vW);
      /* colours: deep blue water, the sky's light on it at a low angle, the Sun's path of light */
      vec3 sunL = normalize(uSun), up = vec3(0.0, 0.0, 1.0);
      float fres = pow(1.0 - clamp(dot(viewLocal(view), n), 0.0, 1.0), 4.0);
      vec3 deep = vec3(0.012, 0.07, 0.16), sky = vec3(0.42, 0.6, 0.82);
      vec3 col = mix(deep * (0.5 + 0.9 * max(0.0, sunL.z)), sky, 0.12 + 0.7 * fres);
      vec3 hv = normalize(sunL + viewLocal(view)); col += vec3(1.0, 0.95, 0.82) * pow(max(0.0, dot(n, hv)), mix(60.0, 260.0, near)) * 1.3;
      col += vec3(0.9) * smoothstep(1.25, 1.42, h0) * near * 0.5;                         /* white caps, near */
      gl_FragColor = vec4(col, uK * (1.0 - smoothstep(0.8, 1.0, r)));
    }`.replace(/viewLocal\(view\)/g, 'vl') .replace('vec3 sunL = normalize(uSun)', 'vec3 vl = vec3(dot(view, uX), dot(view, uY), dot(view, uZ)); vec3 sunL = normalize(uSun)').replace('uniform vec3 uSun;', 'uniform vec3 uSun, uX, uY, uZ;') }));
seaU.uX = { value: new THREE.Vector3(1, 0, 0) }; seaU.uY = { value: new THREE.Vector3(0, 1, 0) }; seaU.uZ = { value: new THREE.Vector3(0, 0, 1) };
sea.frustumCulled = false; sea.renderOrder = -20; sea.visible = false;
const _sx = new THREE.Vector3(), _sy = new THREE.Vector3(), _sz = new THREE.Vector3(), _sm = new THREE.Matrix4();
/* laid under the capsule (the world here is the capsule's: it is at the origin, the sea w.alt below along w.up) */
function setSea(w, now) {
  const on = w.alt < 40000; sea.visible = on; if (!on) return;
  _sz.fromArray(w.up); _sx.set(1, 0, 0).addScaledVector(_sz, -_sz.x).normalize(); _sy.crossVectors(_sz, _sx);
  sea.quaternion.setFromRotationMatrix(_sm.makeBasis(_sx, _sy, _sz));
  sea.position.copy(_sz).multiplyScalar(-Math.max(0, w.alt) - 0.5);          /* (the waterline a little over the heat shield's rim) */ sea.scale.setScalar(Math.max(4000, w.alt * 30));
  seaU.uT.value = now / 1000; seaU.uAlt.value = Math.max(0, w.alt); seaU.uK.value = Math.min(1, (40000 - w.alt) / 15000);
  seaU.uX.value.copy(_sx); seaU.uY.value.copy(_sy); seaU.uZ.value.copy(_sz);
  seaU.uSun.value.set(SUNV.dot(_sx), SUNV.dot(_sy), SUNV.dot(_sz));
}

/* ---------------------------------------------------------------- outside view, hands (as in launch.js) */
const _h1 = new THREE.Vector3(), _h2 = new THREE.Vector3(), _up = new THREE.Vector3(), _tg = new THREE.Vector3();
function placeOutside(w) {
  _up.fromArray(w.up); _h1.fromArray(w.v).addScaledVector(_up, -_h1.dot(_up)); if (_h1.lengthSq() < 25) _h1.crossVectors(_up, NRM);          /* (falling straight down, or on the sea: no way ahead to go by) */
  _h1.normalize(); _h2.crossVectors(_up, _h1).normalize();
  _tg.set(0, 0.4, 0).applyQuaternion(attRel);
  rig.position.copy(_h1).multiplyScalar(Math.cos(OUT.az)).addScaledVector(_h2, Math.sin(OUT.az)).multiplyScalar(Math.cos(OUT.el)).addScaledVector(_up, Math.sin(OUT.el)).multiplyScalar(OUT.dist).add(_tg);
  /* never under the sea: a metre and a half over it at the least */
  { const low = -Math.max(0, w.alt) + 1.0 - rig.position.dot(_up); if (low > 0) rig.position.addScaledVector(_up, low); }
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
    if (recentred() && SEAT.done && !SEAT.pending) SEAT.pending = 1;     /* the headset's own "reset view": seated again where you belong */
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
  const veilK = Math.max(FADE.k, GREY);
  if (veilK > 0.001) { veil.material.opacity = veilK; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); renderer.render(fadeScene, camera); }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, prevT = S.t, GREY = 0, splashT = -1;
const cue = k => { if (S.cues[k]) return; S.cues[k] = true; if (radio) radio.cue(k); };
const _press = new THREE.Vector3(), _v2 = new THREE.Vector2();
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  ctl.update();
  if (xr) readControllers(now);
  /* the clock: real time (ten times at most) through all that matters */
  if (S.running && !S.still && !S.ended) {
    S.t = Math.min(END, clock.advance(S, dt));
    if (S.t >= END) { S.ended = true; S.running = false; wrist.show(true);
      if (CH12) { card.show({ title: 'Apollo 17 is home', sub: 'Twelve days, to the Moon and back. Thank you for flying', a: 'Back to the chapters', b: 'Take a break' }); saveProgress('index.html', null, 0, true); }
      else { card.show({ title: 'End of chapter 11', sub: 'Through the fire', a: 'Continue: chapter 12, The splashdown', b: 'Take a break' }); saveProgress('splash.html', null, 0, true); } }
  }
  w = where(S.t);
  /* the attitude: turned to gently before the air (never faster than RATE as you see it); in the air and under the
   * parachutes it is the capsule's own, at once */
  attitudeAt(S.t, w, attWant);
  /* (on to the parachutes the capsule swings up nose first in a few seconds: a swing, not a jump, for your comfort) */
  const swing = S.t >= GETE.drogue && S.t < GETE.drogue + 9;
  if (S.snap) { att.copy(attWant); S.snap = false; }
  else if (swing) att.rotateTowards(attWant, 0.6 * dt)
  else if (S.t > GETE.ei) att.copy(attWant);
  else att.rotateTowards(attWant, (S.t > GETE.ei - 5 ? 4 : 1) * RATE() * dt);
  attRel.copy(att).multiply(_q.copy(attPad).invert());
  vehicle.group.quaternion.copy(attRel);
  /* the load: you are pressed back into your couch along the pull of it, a centimetre and a half for every g */
  _press.fromArray(w.a); const gl = w.g; if (gl > 0.05) _press.multiplyScalar(-0.014 * Math.min(gl, 7) / (gl * 9.80665)); else _press.set(0, 0, 0);
  if (OUT.on) placeOutside(w); else { rig.quaternion.copy(att); rig.position.copy(EYE_OFF).applyQuaternion(attRel).add(_press); }
  outside.update({ pos: w.pos, up: w.up, alt: w.alt, nose: [0, 1, 0], head: [0, 0, -1] }, S.t, { towerJett: -1e9 });
  outside.setSpin(W_EARTH * S.t);
  _x.fromArray(MOON_AT).sub(_y.fromArray(w.rE));
  outside.setMoon(_x, _y.fromArray(MOON_AT).negate().normalize(), NRM);
  const td = configure();
  vehicle.update(-1e9, OUT.on ? 'outside' : 'inside', { alt: w.alt, up: w.up });
  vehicle.space(td);                                   /* again: update() resets the launch configuration */
  vehicle.lm.visible = false;
  /* the Service Module is gone after the casting off: the Command Module alone */
  { const gone = S.t >= GETE.sep; for (const c of vehicle.parts.csm.children) if (c !== vehicle.parts.cm) c.visible = c.visible && !gone; }
  /* the fire: how hot, which way it streams (towards the nose, in the capsule's frame: the shell and the sparks are
   * the vehicle's children); its light in the cabin; the cabin warming */
  { const h = S.t < GETE.drogue ? w.heat : 0, on = h > 0.004;
    fire.visible = sparks.visible = on; fireU.uHeat.value = sparkU.uHeat.value = h; fireU.uT.value = sparkU.uT.value = now / 1000;
    if (on) { _x.fromArray(w.v).normalize().negate().applyQuaternion(_q.copy(attRel).invert()); sparks.quaternion.setFromUnitVectors(_y.set(0, 1, 0), _x); fire.quaternion.copy(sparks.quaternion); fire.position.copy(_x).multiplyScalar(15); sparkU.uPx.value = renderer.getDrawingBufferSize(_v2).y; }
    const fl = 0.85 + 0.15 * Math.sin(now * 0.037) + 0.1 * Math.sin(now * 0.011);
    fireLight.intensity = (OUT.on ? 0 : 1) * 2.6 * Math.pow(h, 0.8) * fl; fireLight2.intensity = fireLight.intensity * 0.7;
    fireLight.color.setRGB(1, 0.4 + 0.32 * h, 0.14 + 0.3 * h * h); fireLight2.color.copy(fireLight.color);
    /* the cabin's air: warmed by the fire through the heat shield and the walls (it rose a few degrees) */
    const want = 22 + 9 * Math.min(1, Math.max(0, (S.t - GETE.ei - 40) / 260)) * (S.t < GETE.splash ? 1 : 0.6);
    S.temp += (want - S.temp) * Math.min(1, dt * S.warp * 0.05); }
  setChutes(S.t);
  setSea(w, now);
  /* your sight under the load: it greys in from the edges above four g (the veil, a little) */
  GREY = OUT.on ? 0 : Math.max(0, Math.min(0.42, (w.g - 4) * 0.16));
  /* what happens, said once each */
  if (S.running) {
    const dtE = S.t - GETE.ei;
    if (prevT < GETE.sep && S.t >= GETE.sep) { cue('sep'); sound.thump(0.7, 0.4, 500); }
    if (!CH12) {
      if (dtE > -45) cue('ready');
      if (dtE > 0) cue('ei');
      if (w.heat > 0.05) cue('glow');
      if (w.g > 0.05 && dtE > 0) cue('point05');
      if (w.heat > 0.3) cue('fire');
      if (w.g > 2) cue('g2');
      if (w.heat > 0.7) cue('hot');
      if (w.g > 4) cue('g4');
      if (w.g > 6) cue('g6');
      if (S.t > GETE.peak + 6 && w.g < 5.2) cue('ease');
      if (S.temp > 27.5) cue('warm');
      if (S.t > GETE.blackoutEnd) cue('aos');
      if (S.t > GETE.blackoutEnd + 9) cue('aos2');
      if (S.t > GETE.drogue - 60) cue('fall');
    } else {
      if (prevT < GETE.drogue - 2 && S.t >= GETE.drogue - 2) { cue('cover'); sound.thump(0.5, 0.2, 800); }
      if (prevT < GETE.drogue && S.t >= GETE.drogue) { cue('drogues'); sound.thump(0.9, 0.5, 300); }
      if (prevT < GETE.main && S.t >= GETE.main) { cue('mains'); sound.thump(0.9, 0.6, 260); }
      if (S.t > GETE.main + 12) cue('three');
      if (S.t > GETE.main + 30) cue('recovery');
      if (w.alt < 300 && S.t < GETE.splash) cue('brace');
      if (prevT < GETE.splash && S.t >= GETE.splash) { cue('splash'); sound.thump(1, 0.7, 200); splashT = now; }
      if (S.t > GETE.splash + 8) cue('stable');
      if (S.t > GETE.splash + 22) cue('home');
    }
  }
  /* orbital night: in the Earth's shadow the Sun is gone, outside and in the cabin (a soft edge for the penumbra) */
  _z.fromArray(w.rE);
  const along = _z.dot(SUNV), perp = Math.sqrt(Math.max(0, _z.lengthSq() - along * along));
  const lit = along > 0 ? 1 : Math.min(1, Math.max(0, (perp - RE) / 60000 + 0.5));
  vehicle.lights.children.forEach(l => { if (l.isDirectionalLight) l.intensity = 2.6 * lit; });
  _y.copy(SUNV).applyQuaternion(_q.copy(rig.quaternion).invert());
  cabin.setDaylight(lit, _y);
  outside.stars.setDark(1 - lit);
  /* the cabin shakes with the air's roar (the load and the fire), and bumps under the parachutes; the roar itself */
  const rough = Math.min(1, w.g / 6.5) * (S.t < GETE.drogue ? 1 : 0.25) + (splashT > 0 && now - splashT < 1500 ? 0.8 : 0);
  cabin.group.position.set((Math.random() - 0.5) * 0.004 * rough, (Math.random() - 0.5) * 0.004 * rough, (Math.random() - 0.5) * 0.002 * rough);
  sound.drive(S.t < GETE.drogue ? Math.min(1, w.g / 5.5 + w.heat * 0.25) : S.t < GETE.splash ? 0.1 : 0.02, S.warp);
  prevT = S.t;
  /* floating things drift, and can be taken */
  floaters.update(dt, { ctl, head, hands, enabled: xr && !OUT.on });
  /* (under load nothing floats: the loose things lie where the pull puts them) */
  floaters.group.visible = w.g < 0.3 && S.t < GETE.ei;
  /* instruments */
  cabin.timer.draw(S.t);
  cabin.ball.quaternion.copy(rig.quaternion).invert();
  if (now - lastDsky > 500) {
    const d = cabin.dsky;
    d.draw(S.t < GETE.ei ? '63' : S.t < GETE.drogue ? '64' : '67', '06', '74', d.f(w.g * 100), d.f(w.speed * 3.281), d.f(Math.max(0, w.alt) / 1852 * 10));
    lastDsky = now;
  }
  if (radio && S.running) radio.update(S.t, S.warp);
  if (chat) chat.update(dt, { crew: ['CMP', 'LMP'], where: ['any', 'space', 'earth'], quiet: false });
  updateOutside(dt);
  updateHands(dt);
  cabin.update(now / 1000, { viewer: camera, radio: combineRadios(radio, chat && chat.radio) });
  cabin.setBody(OPT.body);
  FADE.k += (FADE.target - FADE.k) * Math.min(1, dt * 4);
  if (!xr) { head.position.set(0, 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : (S.ended ? (CH12 ? 'The end. Apollo 17 is home. A (N on this screen): back to the chapters.' : 'End of chapter 11. A (N on this screen) goes on to chapter 12, the splashdown.') : ''); }
  if ($('clock')) $('clock').textContent = 'GET ' + fmt(S.t) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  { const i = info(); wrist.draw(now, i); band.update(now, i, pcKeys(), pcHint()); }
  toast.update(dt, camera);
  badge.update(dt, camera, S.warp); card.update(dt, camera);
  if (S.running && !S.ended) saveProgress(PAGE, S.t, now);
  render();
  S.frames++;
  if (window.__entry.afterFrame) { const f = window.__entry.afterFrame; window.__entry.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx) sound.setLevels({ voices: settings.get('voices'), sound: settings.get('sound') });
  if (ctx && !radio) radio = await makeRadio(ctx, sound.voice, new URL('../assets/script/' + (CH12 ? 'splash' : 'entry') + '.json', import.meta.url).href);
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
  card.show(CH12 ? { title: 'Chapter 12', sub: 'The splashdown' } : { title: 'Chapter 11', sub: 'The entry' }, 6);
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
offerContinue(PAGE, $('start'));          /* after a crash or a break: on from where you were */
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  { const s0 = ascentState(-60); fromNoseHead(V(s0.nose), V(s0.head), attPad); }
  vehicle.group.add(fire, sparks, chutes); outside.near.add(sea); cabin.group.add(fireLight, fireLight2);
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

window.__entry = { S, OUT, START, END, GETE, CH12, FLIGHT, fire, sparks, fireU, fireLight, chutes, drogues, mains, sea, seaU, renderer, camera, rig, head, cabin, outside, vehicle, hands, SEAT, afterFrame: null, setTime, get radio() { return radio; }, where, FADE, att, blackout,
  get w() { return w; }, get grey() { return GREY; }, ctl, wrist, toast, badge, clock, get chat() { return chat; }, floaters, settings, info };
