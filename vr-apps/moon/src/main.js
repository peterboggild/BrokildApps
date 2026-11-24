/* Journey to the Moon, stage 0: the Moon test.
 *
 * Three views of the same baked Moon, to judge realism and frame rate in the headset before anything else is
 * built: lunar orbit (110 km), the low point of the descent orbit (15 km), and standing in Taurus-Littrow.
 *
 * URL: ?view=orbit|low|surface  &theta=deg (position along the orbit)  &yaw=&pitch= (flat preview)
 *      &sun=deg (Sun later/earlier, 0.51 deg per hour)  &scale=0.8..1.4 (VR resolution)  &still (no motion)
 */
import * as THREE from 'three';
import { loadMoon } from './moon.js';
import { makeSky, makeRocks, makeLMStandIn, makeRail } from './extras.js';

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
const HEADING = -82 * Math.PI / 180;       /* ground track: travelling west, a little north (Apollo 17's approach) */
const GM = 4.9028e12;

let moon, rocks, lm, rail, sky, hud;

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
function readControllers(dt) {
  const s = renderer.xr.getSession();
  if (!s) return;
  let mx = 0, mz = 0, turn = 0, next = false, toggle = false;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (src.handedness === 'left') { mx = ax[0]; mz = ax[1]; } else turn = ax[0];
    const k = src.handedness;
    const a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed;
    if (a && !pad.prev[k + 'a']) next = true;
    if (b && !pad.prev[k + 'b']) toggle = true;
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b;
  }
  if (next) { const names = Object.keys(VIEWS); setView(names[(names.indexOf(S.view) + 1) % names.length]); }
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
  (xr ? renderer.xr.getCamera() : camera).getWorldPosition(_cam);
  rocks.update(_cam);
  moon.setDetail(_cam, S.view === 'surface');
  renderer.info.reset();
  renderer.clear(true, true, false);
  const close = S.view === 'surface';
  const show = (farOn, nearOn, cabinOn) => {
    moon.far.visible = farOn; sky.visible = farOn;
    moon.near.visible = nearOn; rocks.group.visible = nearOn && close; lm.visible = nearOn;
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
  render();
  S.frames++;
  if (window.__moon.afterFrame) { const f = window.__moon.afterFrame; window.__moon.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function enterVR() {
  if (!navigator.xr) return;
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  S.foveation = 0.5; renderer.xr.setFoveation(S.foveation);
  const hz = +($('hz')?.value || 72);
  if (session.updateTargetFrameRate && session.supportedFrameRates?.includes(hz)) session.updateTargetFrameRate(hz).catch(() => {});
  S.heading = 0; S.yaw = 0; S.pitch = 0;
  /* the fps panel rides on the left controller */
  for (const i of [0, 1]) {
    const grip = renderer.xr.getControllerGrip(i);
    rig.add(grip);
    grip.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') grip.add(hud.mesh); });
  }
  hud.mesh.position.set(0, 0.09, -0.06); hud.mesh.rotation.set(-0.9, 0, 0);
  document.body.classList.add('invr');
  session.addEventListener('end', () => { document.body.classList.remove('invr'); renderer.setSize(innerWidth, innerHeight); });
}

async function start() {
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.setSunOffset(S.sunOffset);
  if (Q.has('exposure')) moon.shared.uExposure.value = num('exposure', 3.4);
  rocks = await makeRocks(moon, BASE);
  lm = makeLMStandIn(moon);
  sky = makeSky(moon);
  rail = makeRail(); rig.add(rail);
  hud = makeHud();
  scene.add(moon.far, moon.near, rocks.group, lm, sky);
  setView(S.view, Q.has('theta') ? num('theta', 0) : undefined);
  if (Q.has('x')) S.pos.set(num('x', 6), 0, num('z', 14));
  S.heading = num('heading', S.view === 'surface' ? 25 : 0) * Math.PI / 180;
  renderer.setAnimationLoop(frame);
  document.body.classList.add('loaded');
  S.ready = true;
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
}
if (Q.has('shot')) document.body.classList.add('invr');
$('vr')?.addEventListener('click', enterVR);
document.querySelectorAll('[data-view]').forEach(b => b.addEventListener('click', () => setView(b.dataset.view)));
start().catch(e => { console.error(e); if ($('err')) { $('err').textContent = 'Could not start: ' + e.message; } });

/* test hooks */
window.__moon = { S, setView, afterFrame: null, get moon() { return moon; }, renderer, camera, rig,
  setSun(deg) { S.sunOffset = deg; moon.setSunOffset(deg); },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; },
  stand(x, z, headingDeg) { S.pos.set(x, 0, z); if (headingDeg !== undefined) S.heading = headingDeg * Math.PI / 180; } };
