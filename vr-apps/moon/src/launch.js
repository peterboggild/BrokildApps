/* Journey to the Moon, chapter 1: the launch, from the Commander's couch.
 *
 * T-3:30 to orbit insertion (T+11:53) and a minute beyond. The cabin is drawn first and writes the stencil; the
 * sky, the Earth and the pad are drawn only where it did not (the window openings), each with its own near and far
 * planes. Until the escape tower is jettisoned at T+3:18 the boost protective cover blinds every window except a
 * small one over the hatch window.
 *
 * URL: ?t=-210 (start time, s from liftoff)  ?yaw= &pitch= (flat preview)  ?still (clock stopped)  ?shot
 *      ?scale= (VR resolution)  ?shake=0..1 */
import * as THREE from 'three';
import { state, EVENTS, phase, SUN } from './ascent.js?v=4';
import { makeOutside } from './earth.js?v=4';
import { makeCabin } from './cabin.js?v=4';
import { makeLaunchSound, makeHaptics, intensity } from './launch-sound.js?v=4';
import { makeRadio } from './radio.js?v=4';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const END = 800;
const NOCABIN = Q.has('nocabin');                    /* test aid: the world outside without the spacecraft round it */

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;
renderer.xr.setReferenceSpaceType('local');          /* seated: the origin is where your head is when you start */
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
  yaw: num('yaw', 0), pitch: num('pitch', 0), fps: 0, calls: 0, tris: 0, frames: 0, ready: false, dsky: 0
};

const outside = makeOutside(renderer);
const cabin = makeCabin();
rig.add(cabin.group);
scene.add(outside.far, outside.near);
const sound = makeLaunchSound(), haptics = makeHaptics();
let radio = null;

/* ---------------------------------------------------------------- the HUD (a test instrument on the left controller) */
const hud = (() => {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.16, 0.08), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false,
    stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp }));
  mesh.renderOrder = 999;
  let last = 0;
  return { mesh, on: true, draw(now) {
    if (now - last < 500) return; last = now;
    g.clearRect(0, 0, 512, 256); g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(0, 0, 512, 256);
    g.fillStyle = S.fps > 68 ? '#9fe6a0' : S.fps > 55 ? '#f0d070' : '#ff8a70'; g.font = 'bold 70px monospace'; g.fillText(S.fps.toFixed(0) + ' fps', 20, 80);
    g.fillStyle = '#d8d8d8'; g.font = '28px monospace';
    g.fillText(S.calls + ' calls  ' + (S.tris / 1000).toFixed(0) + 'k tris', 20, 126);
    g.fillText(phase(S.t) + (S.warp > 1 ? '   x' + S.warp : ''), 20, 168);
    g.fillStyle = '#8a8a8a'; g.font = '22px monospace'; g.fillText('A/X time x1/x4/x16 (after T+3:20)  B/Y hide', 20, 240);
    tex.needsUpdate = true;
  } };
})();

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (e.code === 'Space') { startCount(); S.running = !S.running || !S.started; S.started = true; }
  if (e.code === 'KeyW') cycleWarp();
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
function warpAllowed(t) { return t > EVENTS.towerJett + 20 && !(t > EVENTS.siiCutoff - 15 && t < EVENTS.sivbIgnition + 8) && t < EVENTS.seco - 12; }
function cycleWarp() { S.warp = !warpAllowed(S.t) ? 1 : S.warp === 1 ? 4 : S.warp === 4 ? 16 : 1; }
function setTime(t) { S.t = t; S.warp = 1; }
const pad = { prev: {} };
function readControllers() {
  const s = renderer.xr.getSession(); if (!s) return;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    const k = src.handedness, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed;
    if (a && !pad.prev[k + 'a']) cycleWarp();
    if (b && !pad.prev[k + 'b']) hud.on = !hud.on;
    pad.prev[k + 'a'] = a; pad.prev[k + 'b'] = b;
  }
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
  if (renderer.xr.isPresenting) renderer.xr.updateCamera(camera);
  outside.dome.position.setFromMatrixPosition(camera.matrixWorld);
  renderer.info.reset();
  renderer.clear(true, true, true);
  const show = (c, f, n) => { cabin.group.visible = c && !NOCABIN; hud.mesh.visible = c && hud.on; outside.far.visible = f; outside.near.visible = n; };
  /* 1: the cabin, writing the stencil */
  show(true, false, false); setPlanes(0.02, 12); renderer.render(scene, camera);
  /* 2: sky and Earth through the windows */
  renderer.clearDepth();
  show(false, true, false); setPlanes(Math.max(20, st.alt * 0.2), 6e7); renderer.render(scene, camera);
  /* 3: things near the vehicle: the pad, the tower, the escape tower leaving */
  renderer.clearDepth();
  show(false, false, true); setPlanes(0.5, 80000); renderer.render(scene, camera);
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}

const _m = new THREE.Matrix4(), _x = new THREE.Vector3(), _y = new THREE.Vector3(), _z = new THREE.Vector3(), _q0 = new THREE.Quaternion(), _sun = new THREE.Vector3();
let jolt = 0, prevT = S.t;
function placeRig() {
  _y.fromArray(st.head); _z.fromArray(st.nose).negate(); _x.crossVectors(_y, _z).normalize();
  _m.makeBasis(_x, _y, _z);
  rig.quaternion.setFromRotationMatrix(_m);
}

let lastT = 0, acc = 0, accN = 0, lastDsky = 0, lastAlt = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - lastT) / 1000 || 0.016); lastT = now;
  acc += dt; accN++;
  if (acc > 0.5) { S.fps = accN / acc; acc = 0; accN = 0; if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`; }
  const xr = renderer.xr.isPresenting;
  if (xr) readControllers();
  if (S.running && !S.still) {
    if (!warpAllowed(S.t)) S.warp = 1;
    S.t = Math.min(END, S.t + dt * S.warp);
  }
  st = state(S.t, st);
  placeRig();
  outside.update(st, S.t, EVENTS);
  /* the cover and the daylight */
  const open = S.t >= EVENTS.towerJett;
  cabin.setCovers(!open);
  _sun.fromArray(SUN).applyQuaternion(_q0.copy(rig.quaternion).invert());
  cabin.setDaylight(open ? 1 : 0.15, _sun);
  /* shaking: the cabin round you, never your view */
  const k = intensity(S.t);
  if (prevT < EVENTS.sicCutoff + 0.6 && S.t >= EVENTS.sicCutoff + 0.6) jolt = 1;
  if (prevT < EVENTS.siiCutoff + 0.8 && S.t >= EVENTS.siiCutoff + 0.8) jolt = 0.6;
  prevT = S.t;
  jolt = Math.max(0, jolt - dt * 3);
  const a = 0.0035 * k.shake * S.shake;
  cabin.group.position.set((Math.random() - 0.5) * a, (Math.random() - 0.5) * a, (Math.random() - 0.5) * a * 0.6 + 0.03 * jolt * Math.sin(jolt * Math.PI) * S.shake);
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
  if (radio && S.running) radio.update(S.t, S.warp);
  if (xr) haptics(renderer.xr.getSession(), S.t, S.shake, now / 1000);
  if (!xr) { head.position.set(0, 0, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  else head.position.set(0, 0, 0);
  if ($('sub') && radio) { const l = radio.current(S.t); $('sub').textContent = l ? l.who + ': ' + l.text : ''; }
  if ($('clock')) $('clock').textContent = (S.t < 0 ? 'T−' : 'T+') + fmt(Math.abs(S.t)) + '  ' + phase(S.t) + (S.warp > 1 ? '  ×' + S.warp : '');
  hud.draw(now);
  render();
  S.frames++;
  if (window.__launch.afterFrame) { const f = window.__launch.afterFrame; window.__launch.afterFrame = null; f(); }
}
const fmt = s => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');

/* ---------------------------------------------------------------- start */
async function startSound() {
  const ctx = sound.init();
  if (ctx && !radio) radio = await makeRadio(ctx, sound.out, new URL('../assets/script/launch.json', import.meta.url).href);
}
function startCount() { startSound(); }
async function enterVR() {
  if (!navigator.xr) return;
  readOptions();
  await startSound();
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(0.5);
  const hz = +($('hz')?.value || 72);
  if (session.updateTargetFrameRate && session.supportedFrameRates?.includes(hz)) session.updateTargetFrameRate(hz).catch(() => {});
  for (const i of [0, 1]) {
    const grip = renderer.xr.getControllerGrip(i); rig.add(grip);
    grip.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') grip.add(hud.mesh); });
  }
  hud.mesh.position.set(0, 0.09, -0.06); hud.mesh.rotation.set(-0.9, 0, 0);
  S.yaw = 0; S.pitch = 0;
  document.body.classList.add('invr');
  setTimeout(() => { S.running = true; S.started = true; }, 2500);     /* a moment to settle in before the count resumes */
  session.addEventListener('end', () => { document.body.classList.remove('invr'); S.running = false; renderer.setSize(innerWidth, innerHeight); });
}
function readOptions() {
  if ($('start')) S.t = +$('start').value;
  if ($('shake')) S.shake = +$('shake').value;
}
$('vr')?.addEventListener('click', enterVR);
$('flat')?.addEventListener('click', async () => { readOptions(); await startSound(); S.running = true; S.started = true; document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');

(async () => {
  outside.update(state(S.t), S.t, EVENTS);          /* asks for the first sky table */
  await outside.ready;
  st = state(S.t); placeRig();
  renderer.setAnimationLoop(frame);
  S.ready = true;
  document.body.classList.add('loaded');
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
})().catch(e => { console.error(e); if ($('err')) $('err').textContent = 'Could not start: ' + e.message; });

window.__launch = { S, renderer, camera, rig, cabin, outside, afterFrame: null, setTime, get radio() { return radio; },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; } };
