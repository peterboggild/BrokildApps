/* Journey to the Moon: the mission briefing (optional, and not authentic).
 *
 * A dark room with one large curved screen and a 1:60 model of the Apollo-Saturn V on a pedestal. Twelve boards
 * (briefing-slides.js) walk through the mission, each with a narration; on the boards about the vehicle and the
 * launch the model demonstrates: it turns, and runs through the staging. B or the trigger: next board; A or X: back; Y: leave the briefing.
 *
 * URL: ?slide=1..12  ?shot  ?yaw= &pitch= */
import * as THREE from 'three';
import { SLIDES, W, H } from './briefing-slides.js?v=35';
import { makeVehicle } from './vehicle.js?v=35';
import { createHand, makeControllers } from './hands.js?v=35';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
$('stage').appendChild(renderer.domElement);
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x05070b);
const camera = new THREE.PerspectiveCamera(65, innerWidth / innerHeight, 0.02, 200);
const rig = new THREE.Group(); rig.add(camera); scene.add(rig);
camera.position.set(0, 1.6, 0);
addEventListener('resize', () => { if (renderer.xr.isPresenting) return; camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); });

const S = { slide: Math.min(12, Math.max(1, num('slide', 1))) - 1, yaw: num('yaw', 0), pitch: num('pitch', 0), frames: 0, ready: false, t0: 0 };

/* ---------------- the room */
const L = o => new THREE.MeshLambertMaterial(o);
const floor = new THREE.Mesh(new THREE.CircleGeometry(9, 64), L({ color: 0x15171b })); floor.rotation.x = -Math.PI / 2; scene.add(floor);
const ring = new THREE.Mesh(new THREE.RingGeometry(1.6, 1.64, 64), new THREE.MeshBasicMaterial({ color: 0x3a3220 })); ring.rotation.x = -Math.PI / 2; ring.position.y = 0.002; scene.add(ring);
const walls = new THREE.Mesh(new THREE.CylinderGeometry(9, 9, 6, 64, 1, true), L({ color: 0x0d0f13, side: THREE.BackSide })); walls.position.y = 3; scene.add(walls);
scene.add(new THREE.HemisphereLight(0x8090a8, 0x101010, 0.6));
const spot = new THREE.SpotLight(0xfff0d8, 30, 12, 0.5, 0.6, 1.5); spot.position.set(-2.2, 5, -1.2); spot.target.position.set(-2.4, 0.8, -2.6); scene.add(spot, spot.target);

/* the screen: a section of a cylinder round you, 16:9 */
const canvas = document.createElement('canvas'); canvas.width = W; canvas.height = H;
const ctx = canvas.getContext('2d');
const tex = new THREE.CanvasTexture(canvas); tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 8;
const R = 3.4, arc = 4.6 / R;                                   /* 4.6 m wide */
const screenGeo = new THREE.CylinderGeometry(R, R, 4.6 * H / W, 64, 1, true, Math.PI - arc / 2, arc);
const screen = new THREE.Mesh(screenGeo, new THREE.MeshBasicMaterial({ map: tex, side: THREE.BackSide, toneMapped: false }));
screen.position.set(0, 1.62, 0); scene.add(screen);
const bezelM = L({ color: 0x22252b });
const bezel = new THREE.Mesh(new THREE.CylinderGeometry(R + 0.03, R + 0.03, 4.6 * H / W + 0.1, 64, 1, true, Math.PI - arc / 2 - 0.015, arc + 0.03), bezelM);
bezel.material.side = THREE.BackSide; bezel.position.copy(screen.position); scene.add(bezel);
/* the cylinder's texture runs the other way round when seen from inside */
tex.wrapS = THREE.RepeatWrapping; tex.repeat.x = -1;

/* ---------------- the model on its pedestal */
const pedestal = new THREE.Mesh(new THREE.CylinderGeometry(0.42, 0.5, 0.75, 32), L({ color: 0x2a2c31 })); pedestal.position.set(-2.4, 0.375, -2.5); scene.add(pedestal);
const plaque = new THREE.Mesh(new THREE.PlaneGeometry(0.5, 0.12), new THREE.MeshBasicMaterial({ map: (() => {
  const c = document.createElement('canvas'); c.width = 512; c.height = 120; const x = c.getContext('2d');
  x.fillStyle = '#1b1c20'; x.fillRect(0, 0, 512, 120); x.fillStyle = '#d8c27a'; x.font = '600 34px Arial'; x.textAlign = 'center';
  x.fillText('APOLLO-SATURN V', 256, 52); x.font = '24px Arial'; x.fillStyle = '#9a9a9a'; x.fillText('scale 1 : 60', 256, 92);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; })() }));
plaque.position.set(-2.4, 0.55, -2.0); plaque.lookAt(0, 0.55, 0); scene.add(plaque);
const vehicle = makeVehicle(renderer);
const model = new THREE.Group(); model.position.set(-2.4, 0.75, -2.5); model.scale.setScalar(1 / 60); scene.add(model);
model.add(vehicle.group); vehicle.group.position.y = 105.6 - 8.5 + 5.8;      /* stand it on its engine bells */
scene.add(vehicle.lights);

/* ---------------- narration */
let audio = null, voices = [], playing = null;
async function initAudio() {
  if (audio) { if (audio.state === 'suspended') audio.resume(); return; }
  audio = new (window.AudioContext || window.webkitAudioContext)();
  const url = new URL('../assets/script/briefing.json', import.meta.url).href, script = await (await fetch(url)).json();
  voices = await Promise.all(script.lines.map(async l => {
    try { return { ...l, buf: await audio.decodeAudioData(await (await fetch(new URL('voice/' + l.id + '.ogg', url).href)).arrayBuffer()) }; }
    catch (e) { return { ...l, buf: null }; }
  }));
  say();
}
function say() {
  if (!audio || !voices.length) return;
  if (playing) { try { playing.stop(); } catch (_) { /* done */ } playing = null; }
  const v = voices[S.slide]; if (!v || !v.buf) return;
  const s = audio.createBufferSource(); s.buffer = v.buf; s.connect(audio.destination); s.start(audio.currentTime + 0.4); playing = s;
  if ($('sub')) $('sub').textContent = v.text;
}

/* ---------------- boards */
function show(i) {
  S.slide = Math.max(0, Math.min(SLIDES.length - 1, i));
  SLIDES[S.slide](ctx); tex.needsUpdate = true;
  S.t0 = performance.now();
  say();
}
function next() { if (S.slide === SLIDES.length - 1) { leave(); return; } show(S.slide + 1); }
function back() { show(S.slide - 1); }
function leave() {
  location.href = 'launch.html';                    /* still in VR: chapter 1 enters VR by itself where the browser allows it */
}

/* ---------------- input */
addEventListener('keydown', e => { initAudio(); if (e.code === 'ArrowRight' || e.code === 'Space') next(); if (e.code === 'ArrowLeft') back(); });
$('next')?.addEventListener('click', () => { initAudio(); next(); });
$('back')?.addEventListener('click', () => { initAudio(); back(); });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; });
addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => { if (!drag) return; S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-80, Math.min(80, S.pitch - (e.clientY - drag[1]) * 0.2)); drag = [e.clientX, e.clientY]; });
const prev = {};
const hands = { left: createHand('left'), right: createHand('right') };
let ctl = null;                     /* the controllers by hand (hands.js), made with the session */
function controllers(dt) {
  const s = renderer.xr.getSession(); if (!s || !ctl) return;
  ctl.update();
  s.inputSources.forEach(src => {
    const gp = src.gamepad; if (!gp) return;
    const k = src.handedness, tr = gp.buttons[0]?.pressed, a = gp.buttons[4]?.pressed, b = gp.buttons[5]?.pressed;
    /* as time in the chapters: B (or the trigger) forward, A back; X back too; Y leaves the briefing for chapter 1 */
    if ((tr && !prev[k + 't']) || (k === 'right' && b && !prev[k + 'b'])) next();
    if (a && !prev[k + 'a']) back();
    if (k === 'left' && b && !prev[k + 'b']) leave();
    prev[k + 't'] = tr; prev[k + 'a'] = a; prev[k + 'b'] = b;
    const m = hands[k], grip = ctl.grips[k];
    if (m && m.parent !== grip) grip.add(m);
    if (m) m.userData.pose(gp.buttons[0]?.value || 0, gp.buttons[1]?.value || 0, [3, 4, 5].some(n => gp.buttons[n]?.touched), false, dt);
  });
}

/* ---------------- frame */
let last = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - last) / 1000 || 0.016); last = now;
  controllers(dt);
  if (!renderer.xr.isPresenting) { camera.position.set(0, 1.6, 0); camera.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  /* the model: turning slowly; on the launch board it runs through the staging, again and again */
  const show2 = S.slide === 1 || S.slide === 2;
  model.visible = show2;
  model.rotation.y = now / 1000 * 0.25;
  const tt = S.slide === 2 ? ((now - S.t0) / 1000 * 9) % 300 - 15 : -60;
  vehicle.update(tt, 'outside', { alt: Math.max(0, tt) * 300, up: [0, 1, 0] });
  if (S.slide === 2) {               /* the falling stages stay near the model (they are scaled with it) */
    for (const p of [vehicle.parts.sic, vehicle.parts.skirt, vehicle.parts.sii, vehicle.parts.les]) if (Math.abs(p.position.y) > 160) p.visible = false;
  }
  renderer.render(scene, camera);
  S.frames++;
  if (window.__brief.afterFrame) { const f = window.__brief.afterFrame; window.__brief.afterFrame = null; f(); }
}

async function enterVR() {
  if (!navigator.xr) return;
  await initAudio();
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  renderer.xr.setFoveation(0.5);
  if (!ctl) ctl = makeControllers(renderer, camera.parent);
  camera.position.set(0, 0, 0);
  document.body.classList.add('invr');
  say();
  session.addEventListener('end', () => document.body.classList.remove('invr'));
}
$('vr')?.addEventListener('click', enterVR);
$('flat')?.addEventListener('click', () => { initAudio(); document.body.classList.add('running'); });
if (Q.has('shot')) document.body.classList.add('invr');
show(S.slide);
renderer.setAnimationLoop(frame);
S.ready = true;
if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
window.__brief = { S, show, next, back, renderer, camera, afterFrame: null };
