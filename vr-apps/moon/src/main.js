/* Journey to the Moon, stage 0: the Moon test.
 *
 * Three views of the same baked Moon, to judge realism and frame rate in the headset before anything else is
 * built: lunar orbit (110 km), the low point of the descent orbit (15 km), and standing in Taurus-Littrow.
 *
 * URL: ?view=orbit|low|surface  &theta=deg (position along the orbit)  &yaw=&pitch= (flat preview)
 *      &sun=deg (Sun later/earlier, 0.51 deg per hour)  &scale=0.8..1.4 (VR resolution)  &still (no motion)
 */
import * as THREE from 'three';
import { loadMoon } from './moon.js?v=2';
import { makeSky, makeRocks, makeLMStandIn, makeRail } from './extras.js?v=2';
import { makePrints } from './prints.js?v=2';
import { makeSuitSound } from './sound.js?v=2';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;
const FAR = 3.0e6;

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;          /* we update the XR cameras ourselves, to set near and far per pass */
renderer.xr.setReferenceSpaceType('local-floor');
$('stage').appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.05, 1000);
const rig = new THREE.Group();                 /* the viewer's floor: position and orientation in the site frame */
const head = new THREE.Group();                /* flat preview only: eye height and mouse look */
rig.add(head); head.add(camera); scene.add(rig);
addEventListener('resize', () => {
  if (renderer.xr.isPresenting) return;
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight);
});

/* ---------------------------------------------------------------- state */
const S = {
  view: Q.get('view') || 'surface',
  theta: null,                 /* degrees along the ground track, positive = before the site (east of it) */
  still: Q.has('still'),
  yaw: num('yaw', 0), pitch: num('pitch', 0),
  sunOffset: num('sun', 0),
  pos: new THREE.Vector3(6, 0, 14), heading: 0,      /* on the surface: metres from the LM, heading in radians */
  fps: 0, ms: 0, calls: 0, tris: 0, foveation: 1, ready: false, frames: 0
};
const VIEWS = {
  orbit: { alt: 110e3, from: 13, to: -7, tilt: 34, label: 'Lunar orbit, 110 km' },
  low: { alt: 15e3, from: 6, to: -3.5, tilt: 24, label: 'Low pass, 15 km' },
  surface: { label: 'Taurus-Littrow, on the surface' }
};
const EXPOSURE = num('exposure', 3.4);
const HEADING = -82 * Math.PI / 180;       /* ground track: travelling west, a little north (Apollo 17's approach) */
const GM = 4.9028e12;

let moon, rocks, lm, rail, sky, hud, prints;
const suit = makeSuitSound();
const OPT = { teleport: true, fan: true };

/* A point above the Moon at `theta` degrees along the track from the site, in the site frame, with its axes. */
const _c = new THREE.Vector3(), _u = new THREE.Vector3(), _f = new THREE.Vector3(), _r = new THREE.Vector3();
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion();
function orbitFrame(theta, alt, tiltDeg) {
  const man = moon.manifest, R = man.radius;
  /* travel direction on the ground at the site (unit, horizontal): heading measured from north towards east */
  const dir = new THREE.Vector3(Math.sin(HEADING), 0, -Math.cos(HEADING));
  const a = -theta * Math.PI / 180;                     /* angle travelled past the site */
  _u.set(0, 1, 0).multiplyScalar(Math.cos(a)).addScaledVector(dir, Math.sin(a));          /* local up there */
  _f.copy(dir).multiplyScalar(Math.cos(a)).addScaledVector(new THREE.Vector3(0, 1, 0), -Math.sin(a));
  _c.set(0, -man.r0, 0).addScaledVector(_u, R + alt);
  /* tilt the viewer's floor forward so the ground ahead fills the view, as through a forward-down window */
  const t = tiltDeg * Math.PI / 180;
  const up2 = _u.clone().multiplyScalar(Math.cos(t)).addScaledVector(_f, Math.sin(t));
  const fw2 = _f.clone().multiplyScalar(Math.cos(t)).addScaledVector(_u, -Math.sin(t));
  _r.crossVectors(fw2, up2).normalize();
  _m.makeBasis(_r, up2, fw2.clone().negate());
  rig.position.copy(_c); rig.quaternion.setFromRotationMatrix(_m);
}

function setView(name, theta) {
  if (!VIEWS[name]) name = 'surface';
  S.view = name;
  const v = VIEWS[name];
  S.theta = name === 'surface' ? 0 : (theta ?? v.from);
  if (rail) rail.visible = name !== 'surface';
  if ($('viewname')) $('viewname').textContent = v.label;
}

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Digit1') setView('orbit'); if (e.code === 'Digit2') setView('low'); if (e.code === 'Digit3') setView('surface');
  if (e.code === 'KeyP') S.still = !S.still;
  if (e.code === 'BracketRight') { S.sunOffset += 1; moon.setSunOffset(S.sunOffset); }
  if (e.code === 'BracketLeft') { S.sunOffset -= 1; moon.setSunOffset(S.sunOffset); }
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

const pad = { prev: {}, snap: 0 };
const ctrl = {};                                     /* handedness -> target-ray space (a Group in the rig) */
function readControllers(dt) {
  const s = renderer.xr.getSession();
  if (!s) return;
  let mx = 0, mz = 0, turn = 0, next = false, toggle = false, trig = false;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    if (src.handedness === 'right') trig = !!gp.buttons[0]?.pressed;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (src.handedness === 'left') { mx = ax[0]; mz = ax[1]; } else turn = ax[0];
    const k = src.handedness;
    const a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed;
    if (a && !pad.prev[k + 'a']) next = true;
    if (b && !pad.prev[k + 'b']) toggle = true;
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b;
  }
  if (next) { const names = Object.keys(VIEWS); setView(names[(names.indexOf(S.view) + 1) % names.length]); }
  aimTeleport(trig && OPT.teleport && S.view === 'surface' ? ctrl.right : null);
  if (toggle && hud) hud.on = !hud.on;
  /* snap turn, 30 degrees */
  if (Math.abs(turn) > 0.7 && !pad.snap) { S.heading -= Math.sign(turn) * Math.PI / 6; pad.snap = 1; }
  if (Math.abs(turn) < 0.3) pad.snap = 0;
  if (S.view === 'surface' && (Math.abs(mx) > 0.15 || Math.abs(mz) > 0.15)) {
    /* walk where the head looks */
    const d = new THREE.Vector3(); camera.getWorldDirection(d); d.y = 0; d.normalize();
    const r = new THREE.Vector3(-d.z, 0, d.x), sp = 1.5 * dt;
    S.pos.addScaledVector(d, -mz * sp).addScaledVector(r, mx * sp);
  }
}

/* ---------------------------------------------------------------- teleport
 * Hold the right trigger: an arc from the hand shows where you would stand. Let go: the view blinks out and in
 * and you stand there. Only on the detailed ground round the landing site. */
const TP = { aiming: false, valid: false, target: new THREE.Vector3(), blink: 0, pending: null };
const ARC_N = 48;
const arcLine = new THREE.Line(new THREE.BufferGeometry().setAttribute('position', new THREE.BufferAttribute(new Float32Array(ARC_N * 3), 3)),
  new THREE.LineBasicMaterial({ color: 0xd8d4ca, transparent: true, opacity: 0.55, depthTest: false }));
arcLine.frustumCulled = false; arcLine.visible = false; arcLine.renderOrder = 30;
const marker = new THREE.Mesh(new THREE.RingGeometry(0.24, 0.27, 48).rotateX(-Math.PI / 2),
  new THREE.MeshBasicMaterial({ color: 0xe8e4da, transparent: true, opacity: 0.7, depthWrite: false }));
marker.visible = false; marker.renderOrder = 31;
const _o = new THREE.Vector3(), _d = new THREE.Vector3(), _p = new THREE.Vector3(), _v = new THREE.Vector3();
function walkLimit() { return moon.manifest.walk ? moon.manifest.walk.half - 3 : 100; }
function aimTeleport(hand) {
  if (TP.aiming && !hand) {                          /* released */
    TP.aiming = false; marker.visible = false;
    if (TP.valid) { TP.pending = TP.target.clone(); TP.blink = 0.0001; }
    return;
  }
  if (!hand) return;
  TP.aiming = true;
  hand.getWorldPosition(_o); _d.set(0, 0, -1).transformDirection(hand.matrixWorld);
  _v.copy(_d).multiplyScalar(14);                    /* a thrown arc: about 20 m at 45 degrees */
  const pos = arcLine.geometry.attributes.position.array, lim = walkLimit();
  let hit = -1;
  _p.copy(_o);
  for (let i = 0; i < ARC_N; i++) {
    pos[i * 3] = _p.x; pos[i * 3 + 1] = _p.y; pos[i * 3 + 2] = _p.z;
    if (hit < 0 && _p.y < moon.heightAt(_p.x, _p.z)) hit = i;
    if (hit < 0) { _p.addScaledVector(_v, 0.06); _v.y -= 9.8 * 0.06; }
  }
  arcLine.geometry.attributes.position.needsUpdate = true;
  TP.valid = hit > 0 && Math.abs(_p.x) < lim && Math.abs(_p.z) < lim;
  if (TP.valid) {
    TP.target.set(_p.x, moon.heightAt(_p.x, _p.z), _p.z);
    marker.position.copy(TP.target); marker.position.y += 0.01; marker.visible = true;
    arcLine.material.color.setHex(0xd8d4ca);
  } else { marker.visible = false; arcLine.material.color.setHex(0x8a5a50); }
}
function stepBlink(dt) {
  if (!TP.blink) return;
  TP.blink += dt;
  const T = 0.12;                                     /* out, move, in */
  if (TP.pending && TP.blink >= T) { teleportTo(TP.pending.x, TP.pending.z); TP.pending = null; }
  const f = TP.blink < T ? 1 - TP.blink / T : Math.min(1, (TP.blink - T) / T);
  moon.shared.uExposure.value = EXPOSURE * Math.max(0, f);
  sky.visible = f > 0.5;
  if (TP.blink >= 2 * T) { TP.blink = 0; moon.shared.uExposure.value = EXPOSURE; sky.visible = true; }
}
/* stand at (x, z): the rig moves so that your head, not the middle of your play area, ends up over the spot */
function teleportTo(x, z) {
  const head = headGround(new THREE.Vector3());
  S.pos.x += x - head.x; S.pos.z += z - head.z;
  const lim = walkLimit();
  S.pos.x = Math.max(-lim, Math.min(lim, S.pos.x)); S.pos.z = Math.max(-lim, Math.min(lim, S.pos.z));
  STEP.last = null;
  /* where you land you stand: two prints side by side */
  const f = new THREE.Vector3(); camera.getWorldDirection(f); f.y = 0;
  if (f.lengthSq() < 1e-6) f.set(0, 0, -1);
  f.normalize();
  prints.add(x - f.z * 0.12, z + f.x * 0.12, f.x, f.z, 1); prints.add(x + f.z * 0.12, z - f.x * 0.12, f.x, f.z, -1);
  suit.step(0.7);
}

/* ---------------------------------------------------------------- steps: a print and a sound every stride */
const STEP = { last: null, side: 1, stride: 0.82 };
const _h = new THREE.Vector3();
/* the head in the site frame. In XR, renderer.xr.updateCamera has written the headset pose into `camera`;
 * the XR array camera itself has no meaningful world position to read. */
function headGround(out) {
  out.setFromMatrixPosition(camera.matrixWorld);
  out.y = 0;
  return out;
}
function trackSteps() {
  if (S.view !== 'surface') { STEP.last = null; return; }
  headGround(_h);
  if (!STEP.last) { STEP.last = _h.clone(); return; }
  const dx = _h.x - STEP.last.x, dz = _h.z - STEP.last.z, d = Math.hypot(dx, dz);
  if (d > 3) { STEP.last.copy(_h); return; }
  if (d < STEP.stride) return;
  const ux = dx / d, uz = dz / d;
  STEP.side = -STEP.side;
  prints.add(_h.x - uz * STEP.side * 0.11, _h.z + ux * STEP.side * 0.11, ux, uz, STEP.side);
  suit.step();
  STEP.last.copy(_h);
}

/* ---------------------------------------------------------------- HUD (a test instrument, not part of the experience) */
function makeHud() {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.08), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false }));
  mesh.renderOrder = 999;
  let last = 0;
  return {
    mesh, on: true,
    draw(now) {
      if (now - last < 500) return; last = now;
      g.clearRect(0, 0, 512, 256); g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(0, 0, 512, 256);
      g.fillStyle = S.fps > 68 ? '#9fe6a0' : S.fps > 55 ? '#f0d070' : '#ff8a70';
      g.font = 'bold 76px monospace'; g.fillText(S.fps.toFixed(0) + ' fps', 20, 84);
      g.fillStyle = '#d8d8d8'; g.font = '30px monospace';
      g.fillText(S.ms.toFixed(1) + ' ms   ' + S.calls + ' calls', 20, 132);
      g.fillText((S.tris / 1000).toFixed(0) + 'k triangles', 20, 172);
      g.fillText(VIEWS[S.view].label, 20, 212);
      g.fillStyle = '#8a8a8a'; g.font = '22px monospace'; g.fillText('A/X next view   B/Y hide', 20, 244);
      tex.needsUpdate = true;
    }
  };
}

/* ---------------------------------------------------------------- rendering */
function setPlanes(n, f) {
  const fix = c => {
    const e = c.projectionMatrix.elements;
    e[10] = -(f + n) / (f - n); e[14] = -2 * f * n / (f - n);
    c.projectionMatrixInverse.copy(c.projectionMatrix).invert();
  };
  if (renderer.xr.isPresenting) { const xc = renderer.xr.getCamera(); fix(xc); xc.cameras.forEach(fix); }
  else { camera.near = n; camera.far = f; camera.updateProjectionMatrix(); }
}
const _cam = new THREE.Vector3();
function render() {
  const xr = renderer.xr.isPresenting;
  scene.updateMatrixWorld(true);
  if (xr) renderer.xr.updateCamera(camera);
  _cam.setFromMatrixPosition(camera.matrixWorld);
  rocks.update(_cam);
  moon.setDetail(_cam, S.view === 'surface');
  renderer.info.reset();
  renderer.clear(true, true, false);
  const close = S.view === 'surface';
  const show = (farOn, nearOn, cabinOn) => {
    moon.far.visible = farOn; sky.visible = farOn;
    moon.near.visible = nearOn; rocks.group.visible = nearOn && close; lm.visible = nearOn;
    prints.mesh.visible = nearOn && close; arcLine.visible = TP.aiming && cabinOn && close; marker.visible = TP.aiming && TP.valid && nearOn;
    rail.visible = cabinOn && S.view !== 'surface'; if (hud) hud.mesh.visible = hud.on && cabinOn;
  };
  if (close) {
    /* standing on the Moon: distant terrain first, then everything within a few kilometres over a fresh depth buffer */
    show(true, false, false); setPlanes(800, FAR); renderer.render(scene, camera);
    renderer.clearDepth();
    show(false, true, true); setPlanes(0.05, 40e3); renderer.render(scene, camera);
  } else {
    const alt = VIEWS[S.view].alt;
    show(true, true, false); setPlanes(Math.max(50, alt * 0.25), FAR); renderer.render(scene, camera);
    renderer.clearDepth();
    show(false, false, true); setPlanes(0.05, 100); renderer.render(scene, camera);
  }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

let lastT = 0, acc = 0, accN = 0, slow = 0;
function frame(t) {
  const dt = Math.min(0.1, (t - lastT) / 1000 || 0.016); lastT = t;
  acc += dt; accN++;
  if (acc > 0.5) {
    S.fps = accN / acc; S.ms = acc / accN * 1000; acc = 0; accN = 0;
    /* frame-rate guard: stronger foveation when the headset falls behind (resolution cannot change mid-session) */
    if (renderer.xr.isPresenting) {
      slow = S.fps < 66 ? slow + 1 : 0;
      if (slow >= 4 && S.foveation < 1) { S.foveation = Math.min(1, S.foveation + 0.25); renderer.xr.setFoveation(S.foveation); slow = 0; }
    }
    if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`;
  }
  const xr = renderer.xr.isPresenting;
  if (xr) readControllers(dt);
  if (S.view === 'surface') {
    if (!xr) {
      const sp = (keys.ShiftLeft ? 6 : 1.5) * dt, h = (S.heading + S.yaw * Math.PI / 180);
      const fx = -Math.sin(h), fz = -Math.cos(h);
      if (keys.KeyW) { S.pos.x += fx * sp; S.pos.z += fz * sp; } if (keys.KeyS) { S.pos.x -= fx * sp; S.pos.z -= fz * sp; }
      if (keys.KeyA) { S.pos.x += fz * sp; S.pos.z -= fx * sp; } if (keys.KeyD) { S.pos.x -= fz * sp; S.pos.z += fx * sp; }
    }
    const lim = moon.manifest.walk ? moon.manifest.walk.half - 3 : 100;
    S.pos.x = Math.max(-lim, Math.min(lim, S.pos.x)); S.pos.z = Math.max(-lim, Math.min(lim, S.pos.z));
    rig.position.set(S.pos.x, moon.heightAt(S.pos.x, S.pos.z), S.pos.z);
    rig.quaternion.setFromAxisAngle(new THREE.Vector3(0, 1, 0), S.heading);
  } else {
    const v = VIEWS[S.view];
    if (!S.still) {
      S.theta -= Math.sqrt(GM / (moon.manifest.radius + v.alt)) / (moon.manifest.radius + v.alt) * 180 / Math.PI * dt;
      if (S.theta < v.to) S.theta = v.from;
    }
    orbitFrame(S.theta, v.alt, v.tilt);
    if (xr) rig.quaternion.multiply(_q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), S.heading));
  }
  if (xr) { head.position.set(0, 0, 0); head.rotation.set(0, 0, 0); }
  else { head.position.set(0, 1.65, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if (hud) hud.draw(t);
  stepBlink(dt);
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) renderer.xr.updateCamera(camera);
  trackSteps();
  render();
  S.frames++;
  if (window.__moon.afterFrame) { const f = window.__moon.afterFrame; window.__moon.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function enterVR() {
  if (!navigator.xr) return;
  readOptions();
  suit.init();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  S.foveation = 0.5; renderer.xr.setFoveation(S.foveation);
  const hz = +($('hz')?.value || 72);
  if (session.updateTargetFrameRate && session.supportedFrameRates?.includes(hz)) session.updateTargetFrameRate(hz).catch(() => {});
  S.heading = 0; S.yaw = 0; S.pitch = 0;
  /* the fps panel rides on the left controller */
  for (const i of [0, 1]) {
    const grip = renderer.xr.getControllerGrip(i), ray = renderer.xr.getController(i);
    rig.add(grip); rig.add(ray);
    grip.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') grip.add(hud.mesh); });
    ray.addEventListener('connected', e => { if (e.data) ctrl[e.data.handedness] = ray; });
  }
  hud.mesh.position.set(0, 0.09, -0.06); hud.mesh.rotation.set(-0.9, 0, 0);
  document.body.classList.add('invr');
  session.addEventListener('end', () => { document.body.classList.remove('invr'); renderer.setSize(innerWidth, innerHeight); });
}

async function start() {
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.setSunOffset(S.sunOffset);
  moon.shared.uExposure.value = EXPOSURE;
  rocks = await makeRocks(moon, BASE);
  lm = makeLMStandIn(moon);
  sky = makeSky(moon);
  rail = makeRail(); rig.add(rail);
  hud = makeHud();
  prints = makePrints(moon);
  prints.trail(0.4, 5.3, 6, 13.2);                   /* your own way here from the ladder */
  scene.add(moon.far, moon.near, rocks.group, lm, sky, prints.mesh, arcLine, marker);
  setView(S.view, Q.has('theta') ? num('theta', 0) : undefined);
  if (Q.has('x')) S.pos.set(num('x', 6), 0, num('z', 14));
  S.heading = num('heading', S.view === 'surface' ? 25 : 0) * Math.PI / 180;
  renderer.setAnimationLoop(frame);
  document.body.classList.add('loaded');
  S.ready = true;
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
}
function readOptions() {
  OPT.teleport = $('opt-teleport') ? $('opt-teleport').checked : true;
  OPT.fan = $('opt-fan') ? $('opt-fan').checked : true;
  suit.setFan(OPT.fan);
}
$('opt-fan')?.addEventListener('change', readOptions);
/* on the computer, sound starts with the first click or key (browsers do not allow it earlier) */
const wake = () => { if (Q.has('shot')) return; readOptions(); suit.init(); };
addEventListener('pointerdown', wake); addEventListener('keydown', wake);
if (Q.has('shot')) document.body.classList.add('invr');
$('vr')?.addEventListener('click', enterVR);
document.querySelectorAll('[data-view]').forEach(b => b.addEventListener('click', () => setView(b.dataset.view)));
start().catch(e => { console.error(e); if ($('err')) { $('err').textContent = 'Could not start: ' + e.message; } });

/* test hooks */
window.__moon = { S, setView, afterFrame: null, get moon() { return moon; }, renderer, camera, rig,
  setSun(deg) { S.sunOffset = deg; moon.setSunOffset(deg); },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; },
  stand(x, z, headingDeg) { S.pos.set(x, 0, z); if (headingDeg !== undefined) S.heading = headingDeg * Math.PI / 180; },
  teleport(x, z) { TP.pending = new THREE.Vector3(x, 0, z); TP.blink = 0.0001; },
  get prints() { return prints; }, suit };
