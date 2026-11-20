/*
 * Bordbyen — build a little town on your real table, then shrink down into it and race through its streets.
 *
 *  - Design: the town lies on the table (found in the headset's room scan, or marked by hand). The left hand holds a palette (roads, houses, town
 *    blocks, towers, a town hall, a church, parks, squares, the start/finish and gates, an eraser); the right hand points and draws. Roads are drawn
 *    cell by cell as you sweep; everything else is dragged out as a rectangle.
 *  - Pick a car: four toy cars stand on the table's edge; point at one and pull the trigger.
 *  - Shrink: the town grows round you until it is full size (you shrink, as far as you can tell) and the room gives way to sky and fields. Then the
 *    countdown, and three laps through the gates in order against three opponents.
 *  - Grow: Y takes you back up to the table to build on.
 *
 * Both modes show the same city meshes; only the city's root matrix changes. In design it is the table's pose at 1:160. Driving, it puts the camera's
 * place in the city (the driver's eyes, or a point behind and above the car) where your head was when you went down, so the town moves round you and
 * not you through the room. Between the two, the matrix is blended so that the camera's place travels from the table to your head while the scale
 * grows from 1:160 to 1:1.
 *
 * Flat preview (?flat, or the button): a wooden table in an empty room; the mouse draws (right drag turns the view, the wheel zooms), keys choose.
 */
import * as THREE from './vendor/three.module.min.js';
import { createCity, TOOLS, CELL, SCALE } from './city.js?v=8';
import { buildCar, CARS, CAR_SCALE } from './cars.js?v=8';
import { createRace, LAPS } from './race.js?v=8';
import { createAudio } from './audio.js?v=8';
import { INK, NOIR } from './toon.js?v=8';

const params = new URLSearchParams(location.search);
const $ = id => document.getElementById(id);
const V = THREE.Vector3, clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const SAVE_KEY = 'bordbyen.by';

/* ---------------- renderer, scene ---------------- */
const canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
renderer.setPixelRatio(Math.min(2, devicePixelRatio)); renderer.setSize(innerWidth, innerHeight, false);
renderer.xr.enabled = true; renderer.xr.setReferenceSpaceType('local-floor');
renderer.outputColorSpace = THREE.SRGBColorSpace;
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.04, 3200);      // a near plane not too near: the town on the table is thin
const hemi = new THREE.HemisphereLight(0xe6f2ff, 0x8a7a60, 1.7); scene.add(hemi);
const sun = new THREE.DirectionalLight(0xfff4e0, 2.0); sun.position.set(0.5, 1, 0.35); scene.add(sun);

/* the sky, for driving: a gradient dome round the camera that fades in as you go down (in design the real room shows instead) */
const skyMat = new THREE.ShaderMaterial({ side: THREE.BackSide, depthWrite: false, transparent: true, uniforms: { uA: { value: 0 }, uNoir: { value: 0 }, uFlash: { value: 0 }, uT: { value: 0 } },
  vertexShader: 'varying vec3 vD; void main() { vD = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
  fragmentShader: `precision highp float; varying vec3 vD; uniform float uA; uniform float uNoir; uniform float uFlash; uniform float uT;
    void main() { float h = vD.y; vec3 top = vec3(0.38, 0.62, 0.95), hor = vec3(0.86, 0.92, 0.97), low = vec3(0.62, 0.74, 0.62);
      vec3 c = h > 0.0 ? mix(hor, top, pow(h, 0.6)) : mix(hor, low, min(1.0, -h * 4.0));
      vec3 s = normalize(vec3(0.5, 0.75, 0.35)); c += vec3(1.0, 0.9, 0.7) * pow(max(dot(vD, s), 0.0), 400.0) * 2.0 + vec3(0.25, 0.2, 0.1) * pow(max(dot(vD, s), 0.0), 8.0);
      if (uNoir > 0.5) {                                                         /* the night: a low storm sky, a hazy moon behind it, lightning */
        vec3 nt = vec3(0.05, 0.06, 0.09), nh = vec3(0.17, 0.19, 0.25), nl = vec3(0.06, 0.07, 0.08);
        c = h > 0.0 ? mix(nh, nt, pow(h, 0.5)) : mix(nh, nl, min(1.0, -h * 4.0));
        float cl = sin(vD.x * 9.0 + uT * 0.05) * sin(vD.z * 7.0 - uT * 0.04) * sin(vD.y * 13.0 + vD.x * 5.0); c *= 0.85 + 0.25 * smoothstep(-0.2, 0.6, cl) * step(0.0, h);
        vec3 mo = normalize(vec3(-0.4, 0.55, -0.7)); float md = dot(vD, mo); c += vec3(0.75, 0.8, 0.9) * smoothstep(0.9993, 0.9997, md) * 0.8 + vec3(0.2, 0.22, 0.28) * pow(max(md, 0.0), 30.0);
        c += vec3(0.75, 0.8, 1.0) * uFlash * (0.4 + 0.6 * max(h, 0.0));
      }
      gl_FragColor = vec4(c, uA); }` });
const sky = new THREE.Mesh(new THREE.SphereGeometry(3000, 32, 16), skyMat); sky.renderOrder = -10; sky.frustumCulled = false; sky.visible = false; scene.add(sky);
const FOG = new THREE.Fog(0xdbe9f3, 250, 1400);
/** the ink lines fade into the fog like everything else (and not at all on the table) */
function inkFog() { INK.uNear.value = scene.fog ? FOG.near : 1e6; INK.uFar.value = scene.fog ? FOG.far : 2e6; INK.uFog.value.copy(FOG.color); }

/* ---------------- the style: a bright cartoon by day, or a dark graphic novel by night in the rain ---------------- */
/* rain: streaks that fall through a box (round the camera when driving, over the whole board on the table); their fall is done in the shader */
const RAIN_N = 5000;
const rainMat = new THREE.ShaderMaterial({ transparent: true, depthWrite: false, uniforms: { uT: { value: 0 }, uC: { value: new V() }, uBox: { value: new V(60, 30, 60) }, uSpeed: { value: 16 }, uLen: { value: 1.1 }, uA: { value: 0.35 } },
  vertexShader: `attribute float aEnd; uniform float uT; uniform vec3 uC; uniform vec3 uBox; uniform float uSpeed; uniform float uLen;
    void main() { vec3 o = position; float y = fract(o.y - uT * uSpeed / uBox.y);
      vec3 p = uC + vec3((o.x - 0.5) * uBox.x, (y - 0.5) * uBox.y, (o.z - 0.5) * uBox.z); p.y += aEnd * uLen; p.x += aEnd * uLen * 0.18;
      gl_Position = projectionMatrix * modelViewMatrix * vec4(p, 1.0); }`,
  fragmentShader: 'precision highp float; uniform float uA; void main() { gl_FragColor = vec4(0.72, 0.78, 0.9, uA); }' });
const rain = (() => { const pos = new Float32Array(RAIN_N * 6), end = new Float32Array(RAIN_N * 2);
  for (let k = 0; k < RAIN_N; k++) { const x = Math.random(), y = Math.random(), z = Math.random(); pos.set([x, y, z, x, y, z], k * 6); end[k * 2 + 1] = 1; }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(pos, 3)); g.setAttribute('aEnd', new THREE.BufferAttribute(end, 1));
  const r = new THREE.LineSegments(g, rainMat); r.frustumCulled = false; r.visible = false; r.renderOrder = 30; return r; })();
const LIGHT = { flash: 0, next: 8 };
function applyStyle() {
  if (!rain.parent) holder.add(rain);
  const n = S.noir; NOIR.uNoir.value = n ? 1 : 0; skyMat.uniforms.uNoir.value = n ? 1 : 0; rain.visible = n;
  FOG.color.set(n ? 0x1d2330 : 0xdbe9f3); FOG.near = n ? 40 : 250; FOG.far = n ? 520 : 1400; inkFog();
  hemi.color.set(n ? 0x7d8db8 : 0xe6f2ff); hemi.groundColor.set(n ? 0x2a2a33 : 0x8a7a60); hemi.intensity = n ? 1.3 : 1.7;
  sun.color.set(n ? 0xaebde0 : 0xfff4e0); sun.intensity = n ? 1.3 : 2.0; sun.position.set(n ? -0.4 : 0.5, 1, n ? -0.6 : 0.35);
  INK.uW.value = n ? 0.004 : 0.0032;
  if (!S.xr && flatTable) scene.background = new THREE.Color(n ? 0x101318 : 0x2a2f38);
  audio.noir(n);
}
function setStyle(noir) { S.noir = noir; try { localStorage.setItem('bordbyen.style', noir ? 'noir' : 'day'); } catch (_) { /* no storage */ } applyStyle(); toast(noir ? 'Graphic novel: nat og regn' : 'Tegneserie: dag'); }
/** every frame: the hatching follows the city's matrix, the rain falls where you are, and now and then the sky lights up */
function styleStep(dt) {
  NOIR.uCityInv.value.copy(holder.matrix).invert(); NOIR.uTime.value = S.time; skyMat.uniforms.uT.value = S.time;
  if (!S.noir) return;
  const driving = S.mode === 'drive' || S.mode === 'shrink' || S.mode === 'watchDown' || S.mode === 'grow' || S.mode === 'watchUp';
  rainMat.uniforms.uT.value = S.time;
  rain.visible = driving && sky.visible && skyMat.uniforms.uA.value > 0.5;                // rain only down in the streets, not over the town on the table
  if (rain.visible) { rainMat.uniforms.uC.value.set(camC.p.x, camC.p.y, camC.p.z); rainMat.uniforms.uBox.value.set(50, 26, 50); rainMat.uniforms.uSpeed.value = 17; rainMat.uniforms.uLen.value = 1.1; rainMat.uniforms.uA.value = 0.32; rain.geometry.setDrawRange(0, RAIN_N * 2); }
  else { const W = city.st.W * CELL, H = city.st.H * CELL; rainMat.uniforms.uC.value.set(0, 16, 0); rainMat.uniforms.uBox.value.set(W, 32, H); rainMat.uniforms.uSpeed.value = 90; rainMat.uniforms.uLen.value = 2.6; rainMat.uniforms.uA.value = 0.16; rain.geometry.setDrawRange(0, RAIN_N * 0.5); }
  /* lightning when you are down in the town: a double flicker, then thunder after a moment */
  LIGHT.next -= dt;
  if (driving && LIGHT.next <= 0) { LIGHT.next = 10 + Math.random() * 18; LIGHT.flash = 1; LIGHT.t = 0; audio.thunder(Math.random() * 1.5); }
  if (LIGHT.flash > 0) { LIGHT.t += dt; LIGHT.flash = LIGHT.t < 0.08 ? 1 : LIGHT.t < 0.16 ? 0.2 : LIGHT.t < 0.26 ? 0.8 : Math.max(0, 0.8 - (LIGHT.t - 0.26) * 3); }
  skyMat.uniforms.uFlash.value = LIGHT.flash; hemi.intensity = 1.3 + LIGHT.flash * 1.6;
}

/* ---------------- the city, the cars, the race, sound ---------------- */
const city = createCity();
const holder = new THREE.Group(); holder.matrixAutoUpdate = false; scene.add(holder); holder.add(city.root);
let scenery = null;
const audio = createAudio();
const race = createRace({ city,
  onBump: (c, k, other, n) => { if (c.ai) return;
    audio.bump(other ? k * 1.6 : k); buzz(clamp(0.25 + k * (other ? 0.09 : 0.05), 0, 1), 70 + Math.min(220, k * (other ? 30 : 10)));
    /* a jolt: the view is knocked the way the car was pushed, and springs back */
    const dir = n ? new V(n.x, 0, n.y) : new V(-Math.sin(c.yaw), 0, -Math.cos(c.yaw)).multiplyScalar(-1);
    JOLT.v.addScaledVector(dir, Math.min(1.6, k * (other ? 0.12 : 0.05))); },
  onGate: (c, k, n, total) => { if (c.ai) return; const g = routeInfo.gates[k]; city.markGate(g.i, g.j, true); audio.chime(n === total ? 4 : 2, 880); toast(n === total ? 'Alle porte! Mod mål!' : 'Port ' + n + ' af ' + total); },
  onLap: (c, lap) => { if (!c.ai) { audio.chime(3, 660); toast(lap === LAPS ? 'Sidste omgang!' : 'Omgang ' + lap); } },
  onFinish: (c, place) => { if (!c.ai) { audio.fanfare(); toast(place === 1 ? 'Du vandt!' : 'Du blev nr. ' + place); } } });

const S = { mode: 'menu', xr: false, tool: 0, table: null, steerMode: 'pad', view: 'cockpit', carKey: 'lyn', msg: '', toastT: 0, toast: '', time: 0, frames: 0 };
const table = { pos: new V(0, 0.74, -0.75), yaw: 0, w: 1.2, d: 0.8, found: false };

/* ---------------- saving, undo ---------------- */
const undo = [];
function store() { try { localStorage.setItem(SAVE_KEY, city.save()); } catch (_) { /* no storage */ } }
function before() { undo.push(city.save()); if (undo.length > 40) undo.shift(); }
function doUndo() { const s = undo.pop(); if (!s) return; city.load(s); store(); audio.click(); checkRoute(); }
function boardFor(w, d) { return [clamp(Math.floor(w / (CELL * SCALE)), 6, 40), clamp(Math.floor(d / (CELL * SCALE)), 5, 40)]; }
function loadOrStart() {
  let ok = false; try { const s = localStorage.getItem(SAVE_KEY); if (s && !params.has('new')) ok = city.load(s); } catch (_) { /* no storage */ }
  if (!ok) { const [W, H] = boardFor(table.w, table.d); city.reset(W, H); city.starter(); store(); }
  checkRoute();
}
function newCity(empty) { before(); const [W, H] = boardFor(table.w, table.d); city.reset(W, H); if (!empty) city.starter(); store(); checkRoute(); }
let routeInfo = null;
function checkRoute() { routeInfo = city.route(); S.msg = routeInfo.error ? routeInfo.error : 'Banen er klar: start/mål og ' + (routeInfo.gates.length - 1) + ' port' + (routeInfo.gates.length > 2 ? 'e' : '') + '. Tryk A for at køre.'; paletteDirty = true; flatUI(); }

/* ---------------- the matrices: design (on the table) and driving (round your head) ---------------- */
const _m1 = new THREE.Matrix4(), _m2 = new THREE.Matrix4(), _q = new THREE.Quaternion(), YAX = new V(0, 1, 0);
const BOARD_TOP = 0.6 * SCALE + 0.002;                                        // the board's grass sits on its 4 mm slab
/** city → world for a point F of the city put at world point w, turned by yaw, at scale s */
function cityMatrix(out, w, yaw, s, F) { return out.compose(w, _q.setFromAxisAngle(YAX, yaw), new V(s, s, s)).multiply(_m2.makeTranslation(-F.x, -F.y, -F.z)); }
function designMatrix(out, F = new V()) {
  const w = F.clone().multiplyScalar(SCALE).applyAxisAngle(YAX, table.yaw).add(table.pos); w.y += BOARD_TOP;
  return cityMatrix(out, w, table.yaw, SCALE, F);
}
const head = new V(0, 1.6, 0), headQ = new THREE.Quaternion();
const anchor = { p: new V(0, 1.2, 0), yaw: 0 };
const camC = { p: new V(), yaw: 0 };                                        // the camera's place and heading in the city
function headYaw() { const f = new V(0, 0, -1).applyQuaternion(headQ); return Math.atan2(-f.x, -f.z); }
function setAnchor() { anchor.p.copy(head); anchor.yaw = headYaw(); }
/** Keep your eyes at the driver's eyes: the seat follows your head's height all the time (sit or stand, you look out at the same place), and if you
 *  lean more than 12 cm away it slides after you, so your head cannot end up through the roof or out of the side. */
function alignSeat(dt) {
  if (!S.xr) return;
  anchor.p.y += (head.y - anchor.p.y) * Math.min(1, dt * 2.5);
  const dx = head.x - anchor.p.x, dz = head.z - anchor.p.z, d = Math.hypot(dx, dz);
  if (d > 0.12) { const k = (1 - 0.12 / d) * Math.min(1, dt * 1.5); anchor.p.x += dx * k; anchor.p.z += dz * k; }
}
function updateCamC(dt, snap) {
  const car = player(); if (!car) return;
  if (S.view === 'cockpit') {
    const e = car.obj.userData.eye; camC.p.set(e.x, e.y, e.z).applyAxisAngle(YAX, car.yaw).add(new V(car.p.x, city.heightAt(car.p.x, car.p.y), car.p.y)); camC.yaw = car.yaw;
  } else {
    let d = car.yaw - camC.yaw; d = Math.atan2(Math.sin(d), Math.cos(d)); camC.yaw += snap ? d : d * Math.min(1, dt * 3.2);
    /* behind and above the car; nearer if a building is in the way (never inside one) */
    const back = new V(0, 3.0, 8.0).applyAxisAngle(YAX, camC.yaw); let t = 1;
    for (let k = 1; k <= 12; k++) { const u = k / 12; if (city.inSolid(car.p.x + back.x * u, car.p.y + back.z * u)) { t = Math.max(0.15, (k - 1) / 12); break; } }
    const want = new V(back.x * t, 3.0 * Math.max(0.6, t), back.z * t).add(new V(car.p.x, city.heightAt(car.p.x, car.p.y), car.p.y));
    if (snap) camC.p.copy(want); else camC.p.lerp(want, Math.min(1, dt * 6));
  }
}
/** the knock you feel when hit: a little spring the view rides on (city metres) */
const JOLT = { p: new V(), v: new V() };
function joltStep(dt) { const k = 90, damp = 11; JOLT.v.addScaledVector(JOLT.p, -k * dt).multiplyScalar(Math.exp(-damp * dt)); JOLT.p.addScaledVector(JOLT.v, dt); if (JOLT.p.length() > 0.12) JOLT.p.setLength(0.12); }
function driveMatrix(out) { return cityMatrix(out, anchor.p, anchor.yaw - camC.yaw, 1, camC.p.clone().sub(JOLT.p)); }
/** In the driver's seat you must see out: your own car loses its ink (its black inside would fill the view) and its roof; its body and glass are
 *  drawn from outside only, so from within they vanish and the view is open, with the dashboard and the wheel in front. */
function insideView(car, inside) { if (!car) return; car.traverse(o => { if (o.name === 'ink') o.visible = !inside; }); if (car.userData.roof) car.userData.roof.visible = !inside; car.userData.cockpit.visible = inside; }
const TR = { t: 0, dur: 2.6, from: null, F: new V(), w0: new V(), yaw0: 0, s0: SCALE, up: false };
const ease = t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;

/* ---------------- controllers ---------------- */
const ctrl = [0, 1].map(i => { const ray = renderer.xr.getController(i), grip = renderer.xr.getControllerGrip(i); scene.add(ray); scene.add(grip);
  const c = { i, ray, grip, hand: null, gp: null, prev: {} };
  ray.addEventListener('connected', e => { c.hand = e.data.handedness; c.gp = e.data.gamepad; });
  ray.addEventListener('disconnected', () => { c.hand = null; c.gp = null; });
  /* a little toy controller: a rounded handle and a ray when pointing */
  const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.018, 0.07, 4, 10), new THREE.MeshStandardMaterial({ color: 0x2c3038, roughness: 0.6 })); body.rotation.x = Math.PI / 2 - 0.5; grip.add(body);
  const line = new THREE.Line(new THREE.BufferGeometry().setFromPoints([new V(), new V(0, 0, -1)]), new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.6 }));
  line.visible = false; ray.add(line); c.line = line; return c; });
const hand = h => ctrl.find(c => c.hand === h) || null;
const btn = (c, k) => !!(c && c.gp && c.gp.buttons[k] && c.gp.buttons[k].pressed);
const val = (c, k) => (c && c.gp && c.gp.buttons[k] ? c.gp.buttons[k].value : 0);
const axes = c => (c && c.gp && c.gp.axes.length >= 4 ? [c.gp.axes[2], c.gp.axes[3]] : c && c.gp && c.gp.axes.length >= 2 ? [c.gp.axes[0], c.gp.axes[1]] : [0, 0]);
function edge(c, k) { if (!c) return false; const now = btn(c, k), was = !!c.prev[k]; c.prev[k] = now; return now && !was; }
function buzz(v, ms) { for (const c of ctrl) { try { c.gp?.hapticActuators?.[0]?.pulse?.(v, ms); } catch (_) { /* none */ } } }

/* ---------------- the palette (on the left hand in the headset; buttons on screen in the flat preview) ---------------- */
const PAL = { cols: 4, cell: 120, pad: 8, rows: 4 };
const palCanvas = document.createElement('canvas'); palCanvas.width = 512; palCanvas.height = 740;
const palTex = new THREE.CanvasTexture(palCanvas); palTex.colorSpace = THREE.SRGBColorSpace;
const palette = new THREE.Mesh(new THREE.PlaneGeometry(0.17, 0.2457), new THREE.MeshBasicMaterial({ map: palTex, transparent: true, depthTest: false }));
palette.renderOrder = 50; palette.visible = false;
let paletteDirty = true;
const PAL_ITEMS = [...TOOLS.map((t, i) => ({ tool: i })), { act: 'undo', name: 'Fortryd', color: '#9aa6b2' }, { act: 'go', name: 'Kør!', color: '#3ecf6a', wide: true }];
function palRect(k) { if (PAL_ITEMS[k].wide) return [PAL.pad, 4 + Math.ceil((PAL_ITEMS.length - 1) / 4) * 124, 512 - 2 * PAL.pad, 112]; const c = k % 4, r = Math.floor(k / 4); return [PAL.pad + c * 124, 4 + r * 124, 116, 116]; }
function drawPalette() {
  paletteDirty = false; const x = palCanvas.getContext('2d'); x.clearRect(0, 0, 512, 740);
  x.fillStyle = 'rgba(10,14,22,.86)'; x.beginPath(); x.roundRect(0, 0, 512, 740, 26); x.fill();
  PAL_ITEMS.forEach((it, k) => {
    const [px, py, w, h] = palRect(k), t = it.tool !== undefined ? TOOLS[it.tool] : it, sel = it.tool === S.tool;
    x.fillStyle = sel ? '#ffffff' : 'rgba(255,255,255,.08)'; x.beginPath(); x.roundRect(px, py, w, h, 16); x.fill();
    x.fillStyle = t.color; x.beginPath(); x.roundRect(px + 10, py + 10, w - 20, h - 52, 10); x.fill();
    if (it.tool !== undefined) icon(x, t.key, px + w / 2, py + 10 + (h - 52) / 2);
    x.fillStyle = sel ? '#111' : '#e8eef6'; x.font = `bold ${it.wide ? 34 : 21}px Arial, sans-serif`; x.textAlign = 'center'; x.fillText(t.name, px + w / 2, py + h - 14);
  });
  x.fillStyle = routeInfo && !routeInfo.error ? '#9ff0b4' : '#ffc27a'; x.font = '20px Arial, sans-serif'; x.textAlign = 'left';
  wrap(x, S.msg || '', 20, 652, 472, 24);
  palTex.needsUpdate = true;
}
function wrap(x, text, px, py, w, lh) { const words = text.split(' '); let line = ''; for (const wd of words) { const t = line ? line + ' ' + wd : wd; if (x.measureText(t).width > w) { x.fillText(line, px, py); py += lh; line = wd; } else line = t; } x.fillText(line, px, py); }
function icon(x, key, cx, cy) {
  x.save(); x.translate(cx, cy); x.fillStyle = 'rgba(0,0,0,.35)'; x.strokeStyle = 'rgba(0,0,0,.45)'; x.lineWidth = 4;
  if (key === 'vej') { x.fillStyle = '#f4f1e6'; for (let k = -2; k <= 2; k++) x.fillRect(-4, k * 12 - 4, 8, 7); }
  else if (key === 'huse') { for (const o of [-22, 6]) { x.fillRect(o - 2, -6, 18, 20); x.beginPath(); x.moveTo(o - 5, -6); x.lineTo(o + 7, -18); x.lineTo(o + 19, -6); x.fill(); } }
  else if (key === 'byhus') { x.fillRect(-26, -16, 22, 32); x.fillRect(2, -24, 24, 40); }
  else if (key === 'hoejhus') x.fillRect(-10, -26, 20, 44);
  else if (key === 'raadhus') { x.fillRect(-28, -2, 56, 18); x.fillRect(-6, -26, 12, 26); x.beginPath(); x.moveTo(-8, -26); x.lineTo(0, -34); x.lineTo(8, -26); x.fill(); }
  else if (key === 'kirke') { x.fillRect(-20, -4, 34, 20); x.fillRect(14, -20, 10, 36); x.beginPath(); x.moveTo(12, -20); x.lineTo(19, -34); x.lineTo(26, -20); x.fill(); }
  else if (key === 'park') { for (const [ox, oy] of [[-14, -2], [12, -8], [2, 10]]) { x.beginPath(); x.arc(ox, oy, 10, 0, 7); x.fill(); } }
  else if (key === 'torv') { x.beginPath(); x.arc(0, 0, 14, 0, 7); x.stroke(); x.beginPath(); x.arc(0, 0, 5, 0, 7); x.fill(); }
  else if (key === 'start') { for (let i = 0; i < 4; i++) for (let j = 0; j < 3; j++) { x.fillStyle = (i + j) % 2 ? '#111' : '#bbb'; x.fillRect(-24 + i * 12, -18 + j * 12, 12, 12); } }
  else if (key === 'port') { x.fillStyle = 'rgba(0,0,0,.45)'; x.fillRect(-20, -18, 6, 34); x.fillRect(14, -18, 6, 34); x.fillRect(-20, -20, 40, 10); }
  else if (key === 'slet') { x.rotate(-0.5); x.fillStyle = 'rgba(255,255,255,.8)'; x.fillRect(-20, -9, 40, 18); x.fillStyle = 'rgba(0,0,0,.3)'; x.fillRect(4, -9, 16, 18); }
  x.restore();
}
function choose(k) {
  const it = PAL_ITEMS[k]; if (!it) return;
  if (it.tool !== undefined) { S.tool = it.tool; paletteDirty = true; audio.click(); flatUI(); }
  else if (it.act === 'undo') doUndo();
  else if (it.act === 'go') toPick();
}

/* ---------------- design: pointing at the board ---------------- */
const plane = new THREE.Plane(), ray = new THREE.Ray(), _hit = new V();
const cursor = new THREE.Mesh(new THREE.BoxGeometry(1, 1, 1), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.45, depthWrite: false }));
cursor.renderOrder = 20; cursor.visible = false; city.root.add(cursor);
const D = { down: false, a: null, b: null, lastCell: null, dirty: false, rebuildT: 0 };
/** the cell under a world ray, or null */
function cellUnder(o, d) {
  const n = new V(0, 1, 0), top = table.pos.y + BOARD_TOP; plane.set(n, -top); ray.set(o, d);
  if (!ray.intersectPlane(plane, _hit)) return null;
  const inv = _m1.copy(holder.matrix).invert(), c = _hit.clone().applyMatrix4(inv); const [i, j] = city.cellAt(c.x, c.z);
  return city.inside(i, j) ? { i, j, world: _hit.clone() } : { i: clamp(i, 0, city.st.W - 1), j: clamp(j, 0, city.st.H - 1), out: true, world: _hit.clone() };
}
function showCursor(a, b, ok) {
  const t = TOOLS[S.tool], x0 = Math.min(a.i, b.i), x1 = Math.max(a.i, b.i), z0 = Math.min(a.j, b.j), z1 = Math.max(a.j, b.j);
  const w = (x1 - x0 + 1) * CELL, d = (z1 - z0 + 1) * CELL, h = t.kind === 'lot' && t.solid ? 10 : t.kind === 'terrain' ? 4 : 1.5;
  { const qx = (city.cx(x0) + city.cx(x1)) / 2, qz = (city.cz(z0) + city.cz(z1)) / 2; cursor.position.set(qx, h / 2 + city.heightAt(qx, qz), qz); } cursor.scale.set(w + 0.4, h, d + 0.4);
  cursor.material.color.set(ok ? t.color : '#ff3030'); cursor.visible = true;
}
function designInput(o, d, down, dt) {
  const t = TOOLS[S.tool], c = cellUnder(o, d);
  if (!c) { cursor.visible = false; if (!down && D.down) endStroke(); return null; }
  if (down && !D.down) { D.down = true; D.a = c; D.lastCell = null; D.pushed = !c.out; if (D.pushed) before(); }
  if (!down && D.down) { D.b = c; endStroke(); return c; }
  if (D.down) {
    D.b = c;
    if (t.kind === 'road' && !D.a.out) {
      /* sweep: paint every cell between the last and this one (no gaps when the hand moves fast) */
      const from = D.lastCell || c; let i = from.i, j = from.j; let painted = false;
      const step = () => { if (city.paintRoad(i, j)) painted = true; };
      step(); while (i !== c.i || j !== c.j) { if (i !== c.i) i += Math.sign(c.i - i); else j += Math.sign(c.j - j); step(); }
      D.lastCell = c; if (painted) { D.dirty = true; audio.click(); }
      D.rebuildT -= dt; if (D.dirty && D.rebuildT <= 0) { city.commit(); D.dirty = false; D.rebuildT = 0.12; }
      showCursor(c, c, true);
    } else if (t.kind === 'lot' || t.kind === 'erase' || t.kind === 'terrain') showCursor(D.a, c, t.kind === 'erase' || t.kind === 'terrain' || city.lotOK(t.key, D.a.i, D.a.j, c.i, c.j));
    else showCursor(c, c, city.isRoad(c.i, c.j));
  } else showCursor(c, c, t.kind !== 'gate' || city.isRoad(c.i, c.j));
  return c;
}
function endStroke() {
  const t = TOOLS[S.tool], a = D.a, b = D.b || a; D.down = false; cursor.visible = false;
  if (!a || a.out) return;
  let changed = false;
  if (t.kind === 'road') { changed = true; city.commit(); }
  else if (t.kind === 'lot') { changed = city.placeLot(t.key, a.i, a.j, b.i, b.j); if (changed) { city.commit(); audio.pop(); } else toast(t.name + ' skal være mindst ' + t.min + ' × ' + t.min + ' felter'); }
  else if (t.kind === 'terrain') { changed = city.shapeTerrain(a.i, a.j, b.i, b.j, t.dir); if (changed) { city.reshape(); audio.pop(); } }
  else if (t.kind === 'erase') { changed = city.eraseRect(a.i, a.j, b.i, b.j); if (changed) { city.commit(); audio.click(); } }
  else if (t.kind === 'gate') { changed = city.setGate(b.i, b.j, t.key === 'start'); if (changed) { city.commit(); audio.chime(2, 990); } else toast('Start/mål og porte skal stå på en vej'); }
  if (!changed) { if (D.pushed) undo.pop(); } else { store(); checkRoute(); buzz(0.3, 30); }
}

/* ---------------- picking a car: four toy cars on the table's near edge ---------------- */
const showroom = new THREE.Group(); showroom.visible = false; scene.add(showroom);
const showCars = CARS.map((c, k) => { const g = new THREE.Group(), car = buildCar(c.key); car.scale.setScalar(0.028); g.add(car);
  const plinth = new THREE.Mesh(new THREE.CylinderGeometry(0.07, 0.075, 0.012, 32), new THREE.MeshStandardMaterial({ color: 0xf4f1e6, roughness: 0.4 })); plinth.position.y = -0.006; g.add(plinth);
  const lc = document.createElement('canvas'); lc.width = 256; lc.height = 96; const x = lc.getContext('2d'); x.fillStyle = 'rgba(10,14,22,.85)'; x.beginPath(); x.roundRect(0, 0, 256, 96, 18); x.fill();
  x.fillStyle = c.color; x.font = 'bold 40px Arial, sans-serif'; x.textAlign = 'center'; x.fillText(c.name, 128, 44); x.fillStyle = '#d8e2ee'; x.font = '19px Arial, sans-serif'; x.fillText(c.blurb, 128, 78);
  const lt = new THREE.CanvasTexture(lc); lt.colorSpace = THREE.SRGBColorSpace;
  const label = new THREE.Mesh(new THREE.PlaneGeometry(0.13, 0.049), new THREE.MeshBasicMaterial({ map: lt, transparent: true })); label.position.set(0, 0.1, 0); g.add(label);
  g.userData = { key: c.key, car, label }; showroom.add(g); return g; });
const pickSign = (() => { const c = document.createElement('canvas'); c.width = 512; c.height = 96; const x = c.getContext('2d'); x.fillStyle = 'rgba(10,14,22,.85)'; x.beginPath(); x.roundRect(0, 0, 512, 96, 20); x.fill();
  x.fillStyle = '#fff'; x.font = 'bold 40px Arial, sans-serif'; x.textAlign = 'center'; x.fillText('Vælg din bil', 256, 48); x.font = '22px Arial, sans-serif'; x.fillStyle = '#b9c6d6'; x.fillText('peg og tryk  ·  B: tilbage', 256, 82);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; const m = new THREE.Mesh(new THREE.PlaneGeometry(0.3, 0.056), new THREE.MeshBasicMaterial({ map: t, transparent: true })); showroom.add(m); return m; })();
function placeShowroom() {
  /* along the near edge of the table, towards you */
  const toward = new V().subVectors(head, table.pos); toward.y = 0; const yawT = Math.atan2(toward.x, toward.z);
  const r = new V(Math.cos(yawT), 0, -Math.sin(yawT)), f = new V(Math.sin(yawT), 0, Math.cos(yawT)), edgeD = Math.min(table.d, table.w) * 0.5 * 0.8;
  showCars.forEach((g, k) => { g.position.copy(table.pos).addScaledVector(f, edgeD).addScaledVector(r, (k - 1.5) * 0.17); g.position.y = table.pos.y + 0.12; g.rotation.set(0, yawT, 0); });
  pickSign.position.copy(table.pos).addScaledVector(f, edgeD); pickSign.position.y = table.pos.y + 0.3; pickSign.rotation.set(0, yawT, 0);
}
let hoverCar = null;
function toPick() {
  checkRoute(); if (routeInfo.error) { toast(routeInfo.error); audio.beep(false); return; }
  S.mode = 'pick'; placeShowroom(); showroom.visible = true; cursor.visible = false; audio.chime(2, 700); flatUI();
}
const raycaster = new THREE.Raycaster();
function pickInput(o, d, click) {
  raycaster.set(o, d); hoverCar = null; let best = Infinity;
  for (const g of showCars) { const box = new THREE.Box3().setFromObject(g.userData.car).expandByScalar(0.02); const p = raycaster.ray.intersectBox(box, _hit); if (p) { const dd = p.distanceTo(o); if (dd < best) { best = dd; hoverCar = g; } } }
  if (click && hoverCar) { S.carKey = hoverCar.userData.key; startDrive(); }
}

/* ---------------- going down, driving, coming back up ---------------- */
let playerCar = null;
const player = () => race.cars.find(c => !c.ai);
function startDrive() {
  audio.init();
  race.clear();
  const others = CARS.filter(c => c.key !== S.carKey), names = ['Rød Rasmus', 'Gule Gerda', 'Grønne Gustav', 'Blå Bente'];
  const skills = [0.86, 0.9, 0.94];
  others.forEach((c, k) => { const obj = buildCar(c.key); holder.add(obj); race.addCar(obj, c.key, names[CARS.indexOf(c)], true, skills[k]); });
  playerCar = buildCar(S.carKey); holder.add(playerCar); race.addCar(playerCar, S.carKey, 'Dig', false);
  insideView(playerCar, S.view === 'cockpit'); JOLT.p.set(0, 0, 0); JOLT.v.set(0, 0, 0);
  race.grid(routeInfo); resetMarks();
  if (!scenery) { scenery = city.scenery(); city.root.add(scenery); } scenery.visible = true;
  showroom.visible = false; palette.visible = false; city.gridLines().visible = false;
  setAnchor(); S.view = S.view || 'cockpit'; updateCamC(0, true); playerCar.userData.cockpit.visible = S.view === 'cockpit';
  /* the blend: the camera's place in the city travels from the table to your head */
  TR.t = 0; TR.up = false; TR.F.copy(camC.p); TR.w0.copy(TR.F).multiplyScalar(SCALE).applyAxisAngle(YAX, table.yaw).add(table.pos); TR.w0.y += BOARD_TOP; TR.yaw0 = table.yaw; TR.s0 = SCALE;
  S.mode = 'shrink'; audio.whoosh(false); hud.visible = true; S.lastCount = 4; flatUI();
}
function growUp() {
  if (S.mode !== 'drive') return;
  TR.t = 0; TR.up = true; TR.F.copy(camC.p); S.mode = 'grow'; audio.whoosh(true); audio.silence(); hud.visible = false; flatUI();
}
function endGrow() {
  carTags(false);
  race.clear(); playerCar = null; if (scenery) scenery.visible = false; city.gridLines().visible = true; S.mode = 'design'; scene.fog = null; inkFog(); sky.visible = false; skyMat.uniforms.uA.value = 0; flatUI();
}
function blend(out, t) {
  const e = ease(t), w = new V().lerpVectors(TR.w0, anchor.p, e);
  let dy = (anchor.yaw - camC.yaw) - TR.yaw0; dy = Math.atan2(Math.sin(dy), Math.cos(dy));
  const s = Math.exp(Math.log(TR.s0) * (1 - e));
  return cityMatrix(out, w, TR.yaw0 + dy * e, s, TR.F);
}

/* ---------------- the board in the car: lap, place, time, speed, the countdown ---------------- */
const hudCanvas = document.createElement('canvas'); hudCanvas.width = 512; hudCanvas.height = 330;
const hudTex = new THREE.CanvasTexture(hudCanvas); hudTex.colorSpace = THREE.SRGBColorSpace;
const hud = new THREE.Mesh(new THREE.PlaneGeometry(1, 0.6445), new THREE.MeshBasicMaterial({ map: hudTex, transparent: true, depthTest: false })); hud.renderOrder = 60; hud.visible = false; holder.add(hud);
let hudSig = '';
const fmt = t => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(1); };
function drawHud() {
  const c = player(); if (!c) return;
  const ord = race.order(), place = ord.indexOf(c) + 1, total = routeInfo.gates.length - 1, lap = c.nCleared, kmh = Math.round(Math.abs(c.v) * 3.6);
  let big = '';
  if (race.state === 'count') big = race.count > 3 ? '' : String(Math.ceil(race.count)); else if (race.state === 'run' && race.t < 1) big = 'KØR!';
  const sig = [place, lap, Math.floor(race.t * 10), kmh, big, race.state, S.toastT > 0 ? S.toast : '', ord.map(o => o.nCleared + (o.done ? 'd' : '')).join()].join('|'); if (sig === hudSig) return; hudSig = sig;
  const x = hudCanvas.getContext('2d'); x.clearRect(0, 0, 512, 330);
  x.fillStyle = 'rgba(10,14,22,.45)'; x.beginPath(); x.roundRect(0, 0, 512, 330, 26); x.fill();
  x.textAlign = 'left'; x.fillStyle = '#ffd34d'; x.font = 'bold 50px Arial, sans-serif'; x.fillText(place + '/' + race.cars.length, 24, 62);
  x.fillStyle = '#e8eef6'; x.font = 'bold 30px Arial, sans-serif'; x.fillText(lap >= total ? 'Mod mål!' : 'Porte ' + lap + '/' + total, 170, 52);
  x.font = '28px Arial, sans-serif'; x.fillStyle = '#b9c6d6'; x.fillText(fmt(race.state === 'count' ? 0 : c.done ? c.time : race.t), 170, 92);
  x.textAlign = 'right'; x.fillStyle = '#9ff0b4'; x.font = 'bold 46px Arial, sans-serif'; x.fillText(kmh, 470, 62); x.font = '20px Arial, sans-serif'; x.fillText('km/t', 492, 88);
  x.textAlign = 'center';
  if (big) { x.fillStyle = big === 'KØR!' ? '#3ecf6a' : '#ff5a4a'; x.font = 'bold 80px Arial, sans-serif'; x.fillText(big, 256, 178); }
  else if (race.state === 'done' || c.done) { x.fillStyle = '#ffd34d'; x.font = 'bold 30px Arial, sans-serif'; x.fillText(place === 1 ? 'Du vandt! ' : 'Du blev nr. ' + place + '. ', 256, 140); x.fillStyle = '#d8e2ee'; x.font = '22px Arial, sans-serif'; x.fillText('Hold A: kør igen   ·   Y: tilbage til bordet', 256, 178); }
  else if (S.toastT > 0) { x.fillStyle = '#ffffff'; x.font = 'bold 40px Arial, sans-serif'; x.fillText(S.toast, 256, 160); }
  /* the standings: every car, how many gates it has cleared */
  x.textAlign = 'left'; x.font = 'bold 22px Arial, sans-serif';
  ord.forEach((o, k) => { const y = 214 + k * 29, col = (CARS.find(q => q.key === o.key) || {}).color || '#fff';
    x.fillStyle = col; x.beginPath(); x.arc(34, y - 8, 9, 0, 7); x.fill(); x.fillStyle = o.ai ? '#d8e2ee' : '#ffd34d'; x.fillText((k + 1) + '. ' + o.name, 52, y);
    for (let g = 0; g < total; g++) { x.fillStyle = g < o.nCleared ? '#5be37d' : 'rgba(255,255,255,.22)'; x.beginPath(); x.arc(330 + g * 22, y - 8, 7, 0, 7); x.fill(); }
    if (o.done) { x.fillStyle = '#ffd34d'; x.fillText('MÅL', 330 + total * 22 + 6, y); } });
  hudTex.needsUpdate = true;
}
function placeHud() {
  const c = player(); if (!c) return; const e = c.obj.userData.eye;
  /* in the seat it sits low on the windscreen, like a head-up display, under the line you look along; behind the car it floats high over it */
  if (S.view === 'cockpit') { hud.scale.setScalar(0.3); const p = new V(0, e.y - 0.36, e.z - 0.82).applyAxisAngle(YAX, c.yaw); hud.position.set(c.p.x + p.x, p.y + city.heightAt(c.p.x, c.p.y), c.p.y + p.z); hud.rotation.set(-0.55, c.yaw, 0, 'YXZ'); }
  else { hud.scale.setScalar(1.5); const p = new V(0, 4.8, -1).applyAxisAngle(YAX, camC.yaw); hud.position.set(c.p.x + p.x, p.y + city.heightAt(c.p.x, c.p.y), c.p.y + p.z); hud.rotation.set(-0.15, camC.yaw, 0, 'YXZ'); }
}
function toast(t) { S.toast = t; S.toastT = 2.2; if (S.mode === 'design' || S.mode === 'pick') { S.msg = t; paletteDirty = true; flatUI(); } }

/* ---------------- driving input ---------------- */
const wheelGrip = { held: false, a0: 0, base: 0, angle: 0 };
function driveInput(dt) {
  if (!S.xr) {
    const k = keys; const gas = k.KeyW || k.ArrowUp ? 1 : 0, brake = k.KeyS || k.ArrowDown ? 1 : 0, st = (k.KeyA || k.ArrowLeft ? 1 : 0) - (k.KeyD || k.ArrowRight ? 1 : 0);
    return { gas, brake, steer: st };
  }
  const L = hand('left'), Rr = hand('right');
  const gas = val(Rr, 0), brake = Math.max(val(Rr, 1), val(L, 0));                 // right trigger forwards, right grip (or left trigger) brakes and backs
  if (S.steerMode === 'wheel' && L && Rr) {
    /* the wheel is where your hands are: its angle is the angle of the line between them, seen from where you sit (no need to grip) */
    const lp = new V(), rp = new V(); L.grip.getWorldPosition(lp); Rr.grip.getWorldPosition(rp);
    const right = new V(1, 0, 0).applyAxisAngle(YAX, anchor.yaw), d = rp.clone().sub(lp), a = Math.atan2(d.y, d.dot(right));
    wheelGrip.angle += (clamp(a, -2.2, 2.2) - wheelGrip.angle) * Math.min(1, dt * 18); wheelGrip.lp = lp; wheelGrip.rp = rp;
    return { gas, brake, steer: clamp(wheelGrip.angle / 1.4, -1, 1) };
  }
  const [lx] = axes(L), [rx] = axes(Rr); const sx = Math.abs(lx) > Math.abs(rx) ? lx : rx;
  return { gas, brake, steer: -Math.sign(sx) * Math.max(0, Math.abs(sx) - 0.08) / 0.92 };
}
/** in wheel mode the steering wheel sits between your hands, sized to them, and turns with them */
function wheelToHands(car) {
  const u = car.userData; if (!(S.xr && S.steerMode === 'wheel' && wheelGrip.lp && S.view === 'cockpit')) { u.wheel.rotation.z = -car.userData.steerAngle || u.wheel.rotation.z; return; }
  car.updateMatrixWorld(true); const inv = new THREE.Matrix4().copy(car.matrixWorld).invert();
  const mid = wheelGrip.lp.clone().add(wheelGrip.rp).multiplyScalar(0.5).applyMatrix4(inv), span = wheelGrip.lp.distanceTo(wheelGrip.rp);
  u.wheelBase.position.lerp(mid, 0.5); u.wheelBase.scale.setScalar(clamp(span / 2 / 0.19 / CAR_SCALE, 0.8, 2.2)); u.wheel.rotation.z = wheelGrip.angle;
}

/* ---------------- the room scan: find the table ---------------- */
const SCAN = { next: 0, tried: 0, manual: null, cands: [], sel: 0 };
function findTable(frame, ref) {
  const cands = [];
  try {
    const add = (pts, top) => {
      if (pts.length < 3) return;
      let mx = 0, mz = 0; for (const p of pts) { mx += p.x; mz += p.z; } mx /= pts.length; mz /= pts.length;
      let sxx = 0, szz = 0, sxz = 0; for (const p of pts) { const dx = p.x - mx, dz = p.z - mz; sxx += dx * dx; szz += dz * dz; sxz += dx * dz; }
      const ang = 0.5 * Math.atan2(2 * sxz, sxx - szz), ux = Math.cos(ang), uz = Math.sin(ang);
      let a0 = Infinity, a1 = -Infinity, b0 = Infinity, b1 = -Infinity; for (const p of pts) { const dx = p.x - mx, dz = p.z - mz, a = dx * ux + dz * uz, b = -dx * uz + dz * ux; a0 = Math.min(a0, a); a1 = Math.max(a1, a); b0 = Math.min(b0, b); b1 = Math.max(b1, b); }
      const ca = (a0 + a1) / 2, cb = (b0 + b1) / 2, w = a1 - a0, d = b1 - b0; if (w < 0.3 || d < 0.25 || top < 0.3 || top > 1.3) return;
      cands.push({ x: mx + ca * ux - cb * uz, z: mz + ca * uz + cb * ux, y: top, w, d, yaw: -ang });
    };
    const OK = ['table', 'desk', 'other', ''];
    if (frame.detectedPlanes) for (const p of frame.detectedPlanes) {
      if (!OK.includes(p.semanticLabel || '') || (p.orientation && p.orientation !== 'horizontal')) continue; const pose = frame.getPose(p.planeSpace, ref); if (!pose) continue;
      const m = new THREE.Matrix4().fromArray(pose.transform.matrix), pts = Array.from(p.polygon, q => new V(q.x, q.y, q.z).applyMatrix4(m));
      add(pts, pts.reduce((s, q) => s + q.y, 0) / pts.length);
    }
    if (frame.detectedMeshes) for (const mesh of frame.detectedMeshes) {
      if (!['table', 'desk'].includes(mesh.semanticLabel || '')) continue; const pose = frame.getPose(mesh.meshSpace, ref); if (!pose) continue;
      const m = new THREE.Matrix4().fromArray(pose.transform.matrix), v = mesh.vertices, pts = [];
      for (let k = 0; k < v.length; k += 3) pts.push(new V(v[k], v[k + 1], v[k + 2]).applyMatrix4(m));
      const top = Math.max(...pts.map(p => p.y)); add(pts.filter(p => p.y > top - 0.03), top);
    }
  } catch (e) { console.warn('table scan failed', e); }
  /* the same table found as a plane and as a mesh: keep the larger; then the ones in front of you first, then the nearer */
  const out = [];
  for (const c of cands.sort((a, b) => b.w * b.d - a.w * a.d)) if (!out.some(o => Math.hypot(o.x - c.x, o.z - c.z) < 0.35 && Math.abs(o.y - c.y) < 0.08)) out.push(c);
  const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; f.normalize();
  const score = c => { const dx = c.x - head.x, dz = c.z - head.z, d = Math.hypot(dx, dz); return d + 1.2 * (1 - (dx * f.x + dz * f.z) / Math.max(d, 1e-3)); };
  return out.sort((a, b) => score(a) - score(b));
}
/** where the board would lie on a found table: its long side across your view */
function boardPose(t) {
  let yaw = t.yaw, w = t.w, d = t.d; if (d > w) { yaw += Math.PI / 2; [w, d] = [d, w]; }
  const toHead = Math.atan2(head.x - t.x, head.z - t.z); let dd = toHead - yaw; dd = Math.atan2(Math.sin(dd), Math.cos(dd)); if (Math.abs(dd) > Math.PI / 2) yaw += Math.PI;
  return { x: t.x, y: t.y, z: t.z, w, d, yaw };
}
function setTable(t) { const b = boardPose(t); table.pos.set(b.x, b.y, b.z); table.w = b.w; table.d = b.d; table.yaw = b.yaw; table.found = true; }
/* a preview of the board on the table on offer: a green outline and a faint fill */
const preview = (() => { const g = new THREE.Group();
  const fill = new THREE.Mesh(new THREE.PlaneGeometry(1, 1).rotateX(-Math.PI / 2), new THREE.MeshBasicMaterial({ color: 0x5be37d, transparent: true, opacity: 0.28, depthWrite: false })); g.add(fill);
  const line = new THREE.LineLoop(new THREE.BufferGeometry().setFromPoints([new V(-0.5, 0, -0.5), new V(0.5, 0, -0.5), new V(0.5, 0, 0.5), new V(-0.5, 0, 0.5)]), new THREE.LineBasicMaterial({ color: 0x5be37d })); g.add(line);
  g.visible = false; scene.add(g); return g; })();
function showPreview(t) { if (!t) { preview.visible = false; return; } const b = boardPose(t); preview.position.set(b.x, b.y + 0.004, b.z); preview.rotation.set(0, b.yaw, 0); preview.scale.set(b.w, 1, b.d); preview.visible = true; }

/* ---------------- the loop ---------------- */
const keys = {}; let mouse = { x: 0, y: 0, down: false, right: false, click: false }; const orbit = { yaw: 0, pitch: 0.75, dist: 1.25 };
const clock = new THREE.Clock(); let setupMsg = null;
function loop(_, frame) {
  const dt = Math.min(0.05, clock.getDelta()); S.time += dt; S.frames++;
  if (S.xr && frame) {
    const ref = renderer.xr.getReferenceSpace(), vp = frame.getViewerPose(ref);
    if (vp) { const p = vp.transform.position, o = vp.transform.orientation; head.set(p.x, p.y, p.z); headQ.set(o.x, o.y, o.z, o.w); }
    if (S.mode === 'setup') setupStep(frame, ref, dt);
  } else if (!S.xr) flatCamera(dt);
  styleStep(dt);
  if (S.toastT > 0) { S.toastT -= dt; if (S.toastT <= 0 && (S.mode === 'design' || S.mode === 'pick')) { S.msg = ''; checkRoute(); } }

  /* pointing: the right controller's ray, or the mouse */
  let o = null, d = null, down = false, click = false;
  if (S.xr) { const Rr = hand('right') || ctrl[1]; if (Rr) { o = new V(); Rr.ray.getWorldPosition(o); d = new V(0, 0, -1).applyQuaternion(Rr.ray.getWorldQuaternion(new THREE.Quaternion())); down = val(Rr, 0) > 0.5; click = edge(Rr, 0); } }
  else if (S.mode !== 'menu') { raycaster.setFromCamera(new THREE.Vector2(mouse.x, mouse.y), camera); o = raycaster.ray.origin.clone(); d = raycaster.ray.direction.clone(); down = mouse.down; click = mouse.click; mouse.click = false; }

  if (S.mode === 'design') {
    designButtons();
    holder.matrix.copy(designMatrix(_m1)); holder.matrixWorldNeedsUpdate = true;
    /* the palette on the left hand; pointing at it chooses */
    let onPal = false;
    if (S.xr) {
      const L = hand('left'); palette.visible = !!L;
      if (L) { if (palette.parent !== L.grip) L.grip.add(palette); palette.position.set(0.02, 0.07, -0.05); palette.rotation.set(-0.9, 0, 0); }
      if (o && palette.visible) { raycaster.set(o, d); const h = raycaster.intersectObject(palette)[0]; if (h) { onPal = true; if (click) { const u = h.uv.x * 512, v = (1 - h.uv.y) * 740; const k = PAL_ITEMS.findIndex((_, i) => { const [px, py, w, hh] = palRect(i); return u >= px && u <= px + w && v >= py && v <= py + hh; }); if (k >= 0) choose(k); } } }
    }
    const Rr = hand('right'); if (Rr) { Rr.line.visible = true; Rr.line.scale.z = 3; }
    if (o && (!onPal || D.down)) designInput(o, d, down, dt);
    if (paletteDirty) drawPalette();
  } else if (S.mode === 'pick') {
    holder.matrix.copy(designMatrix(_m1)); holder.matrixWorldNeedsUpdate = true;
    if (o) pickInput(o, d, click);
    showCars.forEach((g, k) => { g.userData.car.rotation.y += dt * (g === hoverCar ? 1.6 : 0.4); g.userData.car.position.y = g === hoverCar ? 0.01 + 0.006 * Math.sin(S.time * 6) : 0; });
    if (S.xr && edge(hand('right'), 5)) { S.mode = 'design'; showroom.visible = false; }
  } else if (S.mode === 'shrink' || S.mode === 'grow') {
    if (S.mode === 'shrink') { updateCamC(dt, true); race.update(0.0001, { gas: 0, brake: 1, steer: 0 }); }
    TR.t = Math.min(1, TR.t + dt / TR.dur);
    const k = S.mode === 'shrink' ? TR.t : 1 - TR.t;
    if (S.mode === 'grow') { holder.matrix.copy(blendDown(_m1, k)); } else holder.matrix.copy(blend(_m1, TR.t)); holder.matrixWorldNeedsUpdate = true;
    const a = clamp((k - 0.55) / 0.35, 0, 1); skyMat.uniforms.uA.value = a; sky.visible = a > 0; scene.fog = k > 0.7 ? FOG : null; inkFog();
    if (TR.t >= 1) { if (S.mode === 'shrink') { S.mode = 'drive'; } else endGrow(); }
  } else if (S.mode === 'watchUp' || S.mode === 'watch' || S.mode === 'watchDown') {
    /* the overview: up at the table while the race goes on; your own car waits where you left it */
    if (S.mode === 'watch') watchButtons();
    if (!S.paused) race.update(dt, S.mode === 'watch' ? driveInput(dt) : { gas: 0, brake: 0, steer: 0 }); audio.silence();
    if (signDirty) drawWatchSign();
    const pc = player(); if (pc) { insideView(pc.obj, false); carMark.visible = S.mode === 'watch'; carMark.position.set(pc.p.x, city.heightAt(pc.p.x, pc.p.y) + 22 + Math.sin(S.time * 4) * 1.5, pc.p.y); carMark.rotation.y += dt * 2; }
    carTags(S.mode === 'watch');
    if (S.mode === 'watch') { holder.matrix.copy(designMatrix(_m1)); watchSign.visible = true; }
    else {
      TR.t = Math.min(1, TR.t + dt / 1.8); const k = S.mode === 'watchUp' ? 1 - TR.t : TR.t;
      if (S.mode === 'watchUp') holder.matrix.copy(blendDown(_m1, k)); else { updateCamC(dt, true); holder.matrix.copy(blend(_m1, TR.t)); }
      const a = clamp((k - 0.55) / 0.35, 0, 1); skyMat.uniforms.uA.value = a; sky.visible = a > 0; scene.fog = k > 0.7 ? FOG : null; inkFog();
      if (TR.t >= 1) { if (S.mode === 'watchUp') { S.mode = 'watch'; placeAtEdge(watchSign, 0.3); if (scenery) scenery.visible = false; } else { S.mode = 'drive'; hud.visible = true; } flatUI(); }
    }
    holder.matrixWorldNeedsUpdate = true;
  } else if (S.mode === 'drive') {
    driveButtons(); watchSign.visible = false; carTags(false);
    const inp = driveInput(dt);
    if (race.state === 'count') { const n = Math.ceil(race.count); if (n !== S.lastCount && n <= 3) { audio.beep(false); S.lastCount = n; } if (race.count - dt <= 0) audio.beep(true); }
    race.update(dt, inp);
    updateCamC(dt, false); joltStep(dt);
    holder.matrix.copy(driveMatrix(_m1)); holder.matrixWorldNeedsUpdate = true;
    placeHud(); drawHud(); carMark.visible = false;
    const pc = player(); if (pc) { insideView(pc.obj, S.view === 'cockpit'); pc.noTilt = S.view === 'cockpit'; }
    if (pc) wheelToHands(pc.obj);
    alignSeat(dt);
    /* off the road it bumps hard: the view shakes, the controllers rumble, and the car rattles */
    if (pc && pc.rough && Math.abs(pc.v) > 1.5 && race.state !== 'count') { S.roughT = (S.roughT || 0) - dt; const k = Math.min(1, Math.abs(pc.v) / 12);
      if (S.roughT <= 0) { S.roughT = 0.07 + Math.random() * 0.06; buzz(0.25 + 0.5 * k, 50); JOLT.v.y += (Math.random() * 0.9 - 0.3) * k; JOLT.v.x += (Math.random() - 0.5) * 0.3 * k; if (Math.random() < 0.25) audio.bump(0.6 + k); } }
    /* engines: yours, and the others where they are in the room (the listener is your head) */
    const toWorld = p => new V(p.x, 0.6, p.y).applyMatrix4(holder.matrix);
    audio.engines([player(), ...race.cars.filter(c => c.ai)].map(c => ({ v: c.v, gas: c.ai ? 1 : inp.gas, pos: toWorld(c.p), off: false })),
      { p: head, f: new V(0, 0, -1).applyQuaternion(headQ), u: new V(0, 1, 0).applyQuaternion(headQ) });
  }
  if (S.mode === 'drive' || S.mode === 'shrink' || S.mode === 'grow' || S.mode === 'watchUp' || S.mode === 'watchDown') sky.position.copy(S.xr ? head : camera.position);
  if (S.mode !== 'watch') watchSign.visible = false;
  if (flatTable) flatTable.visible = S.mode === 'design' || S.mode === 'pick' || S.mode === 'watch' || (S.mode !== 'drive' && skyMat.uniforms.uA.value < 0.5);
  for (const c of ctrl) if (S.mode !== 'design' && c.line) c.line.visible = S.mode === 'pick';
  renderer.render(scene, camera);
}
/** the way back up: from the driving matrix to the table, the camera's place in the city travelling from your head to the table */
function blendDown(out, k) {
  /* k goes 1 → 0 */
  const e = ease(k), w0 = TR.F.clone().multiplyScalar(SCALE).applyAxisAngle(YAX, table.yaw).add(table.pos); w0.y += BOARD_TOP;
  const w = new V().lerpVectors(w0, anchor.p, e); let dy = (anchor.yaw - camC.yaw) - table.yaw; dy = Math.atan2(Math.sin(dy), Math.cos(dy));
  return cityMatrix(out, w, table.yaw + dy * e, Math.exp(Math.log(SCALE) * (1 - e)), TR.F);
}
function designButtons() {
  if (!S.xr) return;
  const L = hand('left'), Rr = hand('right');
  if (edge(Rr, 4)) toPick();                                                   // A: drive
  if (edge(Rr, 5)) doUndo();                                                    // B: undo
  const [rx] = axes(Rr); if (Math.abs(rx) > 0.7 && !S.stickHeld) { S.stickHeld = true; S.tool = (S.tool + (rx > 0 ? 1 : TOOLS.length - 1)) % TOOLS.length; paletteDirty = true; audio.click(); } else if (Math.abs(rx) < 0.4) S.stickHeld = false;
  if (edge(L, 3)) { table.found = false; S.mode = 'setup'; SCAN.tried = 0; SCAN.manual = null; palette.visible = false; }           // left stick press: find the table again
  if (btn(L, 4)) { S.clearT = (S.clearT || 0) + 1 / 72; if (S.clearT > 2) { S.clearT = -99; newCity(true); toast('Byen er ryddet. B fortryder.'); } } else S.clearT = 0;      // hold X: an empty board
  if (edge(L, 5)) setStyle(!S.noir);                                            // Y: day or night
}
function driveButtons() {
  if (!S.xr) return;
  const L = hand('left'), Rr = hand('right');
  aButton(Rr);                                                                  // A: seat or behind; hold it: start again
  if (edge(Rr, 5)) toWatch(false);                                              // B: the city view, the race going on
  if (edge(L, 4)) toWatch(true);                                                // X: the city view, paused
  if (edge(L, 5)) growUp();                                                     // Y: stop the race, back to building
  if (edge(Rr, 3)) setAnchor();                                                 // right stick press: line up again
}
function aButton(Rr) {
  const now = btn(Rr, 4);
  if (now) { S.aHold = (S.aHold || 0) + 1 / 72; if (S.aHold > 1 && !S.aDone) { S.aDone = true; restartRace(); toast('Forfra'); } }
  else { if (S.aHold > 0 && !S.aDone) { S.view = S.view === 'cockpit' ? 'chase' : 'cockpit'; updateCamC(0, true); } S.aHold = 0; S.aDone = false; }
}
/* ---------------- the city view: B (on) or X (paused) takes you up to the table while the race goes on; you can still drive your car from there ---------------- */
function toWatch(pause) { if (S.mode !== 'drive') return; S.paused = !!pause; TR.t = 0; TR.F.copy(camC.p); S.mode = 'watchUp'; hud.visible = false; audio.whoosh(true); signDirty = true; flatUI(); }
function fromWatch() {
  if (S.mode !== 'watch') return;
  S.paused = false; setAnchor(); updateCamC(0, true);
  TR.t = 0; TR.F.copy(camC.p); TR.w0.copy(TR.F).multiplyScalar(SCALE).applyAxisAngle(YAX, table.yaw).add(table.pos); TR.w0.y += BOARD_TOP; TR.yaw0 = table.yaw; TR.s0 = SCALE;
  S.mode = 'watchDown'; carMark.visible = false; if (scenery) scenery.visible = true; audio.whoosh(false); flatUI();
}
function stopRace() { race.clear(); playerCar = null; carMark.visible = false; S.paused = false; endGrow(); }
function watchButtons() {
  if (!S.xr) return; const L = hand('left'), Rr = hand('right');
  if (edge(Rr, 5)) fromWatch();                                                 // B: back down into the street
  if (edge(L, 4)) { if (S.paused) fromWatch(); else { S.paused = true; signDirty = true; } }      // X: pause; again: on with the race, down in the car
  if (edge(L, 5)) stopRace();                                                   // Y: stop the race
  aButton(Rr);
}
/** in the city view, a name tag over every car with the gates it has cleared, so the standings can be read off the table */
const tags = new Map();
function carTags(on) {
  for (const c of race.cars) {
    let t = tags.get(c); if (!t) { const cv = document.createElement('canvas'); cv.width = 256; cv.height = 64; const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), depthTest: false, transparent: true }));
      sp.renderOrder = 70; sp.scale.set(24, 6, 1); holder.add(sp); t = { sp, cv, sig: '' }; tags.set(c, t); }
    t.sp.visible = on; if (!on) continue;
    const total = routeInfo.gates.length - 1, sig = c.nCleared + '/' + c.done; t.sp.position.set(c.p.x, city.heightAt(c.p.x, c.p.y) + 12, c.p.y);
    if (sig !== t.sig) { t.sig = sig; const x = t.cv.getContext('2d'), col = (CARS.find(q => q.key === c.key) || {}).color || '#fff'; x.clearRect(0, 0, 256, 64);
      x.fillStyle = 'rgba(10,14,22,.85)'; x.beginPath(); x.roundRect(0, 0, 256, 64, 14); x.fill(); x.fillStyle = col; x.fillRect(0, 0, 10, 64);
      x.fillStyle = c.ai ? '#fff' : '#ffd34d'; x.font = 'bold 24px Arial, sans-serif'; x.fillText(c.name, 20, 28); x.fillStyle = '#5be37d'; x.font = 'bold 22px Arial, sans-serif'; x.fillText(c.done ? 'I MÅL' : 'Porte ' + c.nCleared + '/' + total, 20, 55);
      t.sp.material.map.needsUpdate = true; }
  }
  for (const [c, t] of tags) if (!race.cars.includes(c)) { t.sp.removeFromParent(); t.sp.material.map.dispose(); tags.delete(c); }
}
/** a sign on the table's near edge */
function placeAtEdge(m, h) { const toward = new V().subVectors(head, table.pos); toward.y = 0; const yawT = Math.atan2(toward.x, toward.z), f = new V(Math.sin(yawT), 0, Math.cos(yawT));
  m.position.copy(table.pos).addScaledVector(f, Math.min(table.d, table.w) * 0.42); m.position.y = table.pos.y + h; m.rotation.set(0, yawT, 0); }
const carMark = new THREE.Mesh(new THREE.ConeGeometry(3, 7, 4).rotateX(Math.PI), new THREE.MeshBasicMaterial({ color: 0xffd34d })); carMark.visible = false; holder.add(carMark);
const signCanvas = document.createElement('canvas'); signCanvas.width = 512; signCanvas.height = 96; const signTex = new THREE.CanvasTexture(signCanvas); signTex.colorSpace = THREE.SRGBColorSpace;
const watchSign = new THREE.Mesh(new THREE.PlaneGeometry(0.3, 0.056), new THREE.MeshBasicMaterial({ map: signTex, transparent: true })); watchSign.visible = false; scene.add(watchSign);
let signDirty = true;
function drawWatchSign() {
  signDirty = false; const x = signCanvas.getContext('2d'); x.clearRect(0, 0, 512, 96); x.fillStyle = S.paused ? 'rgba(60,20,10,.9)' : 'rgba(10,14,22,.85)'; x.beginPath(); x.roundRect(0, 0, 512, 96, 20); x.fill();
  x.fillStyle = '#fff'; x.font = 'bold 36px Arial, sans-serif'; x.textAlign = 'center'; x.fillText(S.paused ? 'PAUSE' : 'Byoverblik: løbet kører', 256, 44);
  x.font = '21px Arial, sans-serif'; x.fillStyle = '#c9d5e4'; x.fillText(S.paused ? 'X: fortsæt, ned i bilen  ·  Y: stop løbet' : 'B: ned i bilen  ·  X: pause  ·  Y: stop løbet', 256, 80); signTex.needsUpdate = true;
}
function restartRace() { race.grid(routeInfo); resetMarks(); S.lastCount = 4; updateCamC(0, true); }
function resetMarks() { if (routeInfo && routeInfo.gates) for (const g of routeInfo.gates) city.markGate(g.i, g.j, false); }

/* ---------------- headset: finding the table, by scan or by hand ---------------- */
const setupCanvas = document.createElement('canvas'); setupCanvas.width = 512; setupCanvas.height = 220;
const setupTex = new THREE.CanvasTexture(setupCanvas); setupTex.colorSpace = THREE.SRGBColorSpace;
const setupPanel = new THREE.Mesh(new THREE.PlaneGeometry(0.5, 0.215), new THREE.MeshBasicMaterial({ map: setupTex, transparent: true, depthTest: false })); setupPanel.renderOrder = 70; setupPanel.visible = false; scene.add(setupPanel);
const marker = new THREE.Mesh(new THREE.SphereGeometry(0.012, 12, 8), new THREE.MeshBasicMaterial({ color: 0xffd34d })); marker.visible = false; scene.add(marker);
function setupText(lines) {
  const key = lines.join('|'); if (key === setupMsg) return; setupMsg = key;
  const x = setupCanvas.getContext('2d'); x.clearRect(0, 0, 512, 220); x.fillStyle = 'rgba(10,14,22,.88)'; x.beginPath(); x.roundRect(0, 0, 512, 220, 24); x.fill();
  x.fillStyle = '#fff'; x.font = 'bold 30px Arial, sans-serif'; x.textAlign = 'center'; x.fillText(lines[0], 256, 50);
  x.fillStyle = '#c8d4e2'; x.font = '22px Arial, sans-serif'; let y = 92; for (const l of lines.slice(1)) { wrap(x, l, 30, y, 452, 28); y += 36; } setupTex.needsUpdate = true;
}
function setupStep(frame, ref, dt) {
  const f = new V(0, 0, -1).applyQuaternion(headQ); setupPanel.position.copy(head).addScaledVector(f, 0.8); setupPanel.position.y -= 0.1; setupPanel.lookAt(head); setupPanel.visible = true;
  holder.visible = false;
  if (!SCAN.manual) {
    /* every surface that could be a table, the one in front of you first; you pick which one, or mark it by hand */
    SCAN.tried += dt; SCAN.next -= dt;
    if (SCAN.next <= 0) { SCAN.next = 0.6; const prev = SCAN.cands && SCAN.cands[SCAN.sel]; SCAN.cands = findTable(frame, ref);
      if (prev && SCAN.cands.length) { let best = 0, bd = Infinity; SCAN.cands.forEach((c, k) => { const d = Math.hypot(c.x - prev.x, c.z - prev.z) + Math.abs(c.y - prev.y); if (d < bd) { bd = d; best = k; } }); SCAN.sel = bd < 0.3 ? best : 0; } else SCAN.sel = 0; }
    const Rr = hand('right'), L = hand('left'), cands = SCAN.cands || [];
    if (cands.length) {
      const [rx] = axes(Rr); const flick = Math.abs(rx) > 0.7 && !S.stickHeld; if (Math.abs(rx) > 0.7) S.stickHeld = true; else if (Math.abs(rx) < 0.4) S.stickHeld = false;
      if (edge(Rr, 5) || flick) { SCAN.sel = (SCAN.sel + (flick && rx < 0 ? cands.length - 1 : 1)) % cands.length; audio.click(); }
      const t = cands[SCAN.sel]; showPreview(t);
      setupText(['Bord ' + (SCAN.sel + 1) + ' af ' + cands.length, 'Byen kommer til at ligge på det grønne felt.', 'Aftrækker: brug dette bord · B eller højre stick: næste · X: mærk bordet selv']);
      if (edge(Rr, 0)) { setTable(t); preview.visible = false; return tableDone(); }
    } else { showPreview(null); setupText(['Leder efter et bord …', 'Bordet skal være med i headsettets rumscanning (Space Setup).', SCAN.tried > 3 ? 'Intet fundet. Tryk på aftrækkeren for at markere bordet selv.' : '']);
      if (SCAN.tried > 3 && edge(Rr, 0)) { SCAN.manual = { pts: [] }; return; } }
    if (edge(L, 4)) { SCAN.manual = { pts: [] }; preview.visible = false; }
    return;
  }
  /* by hand: touch two opposite corners of the table with the right controller's tip */
  const Rr = hand('right'), tip = new V(); if (Rr) { Rr.ray.getWorldPosition(tip); marker.position.copy(tip); marker.visible = true; }
  setupText([SCAN.manual.pts.length ? 'Nu det modsatte hjørne' : 'Mærk bordet selv', 'Sæt spidsen af højre controller på et hjørne af bordpladen og tryk på aftrækkeren. Derefter det modsatte hjørne.']);
  if (Rr && edge(Rr, 0)) { SCAN.manual.pts.push(tip.clone()); audio.click(); buzz(0.4, 40);
    if (SCAN.manual.pts.length === 2) { const [a, b] = SCAN.manual.pts, yaw = headYaw();
      const ca = Math.cos(yaw), sa = Math.sin(yaw), dx = b.x - a.x, dz = b.z - a.z, lx = dx * ca - dz * sa, lz = dx * sa + dz * ca;
      setTable({ x: (a.x + b.x) / 2, z: (a.z + b.z) / 2, y: (a.y + b.y) / 2, w: Math.max(0.3, Math.abs(lx)), d: Math.max(0.25, Math.abs(lz)), yaw });
      marker.visible = false; tableDone(); } }
}
function tableDone() {
  setupPanel.visible = false; holder.visible = true; S.mode = 'design'; audio.chime(3, 700); buzz(0.5, 80);
  if (!city.st.cells || !S.loaded) { loadOrStart(); S.loaded = true; }
  toast('Bordet er fundet. Tegn med højre hånd, vælg på paletten i venstre.');
}

/* ---------------- flat preview ---------------- */
let flatTable = null;
function enterFlat() {
  S.xr = false; document.body.classList.add('playing', 'flat'); audio.init();
  scene.background = new THREE.Color(0x2a2f38);
  /* a wooden table on a floor */
  flatTable = new THREE.Group(); const wood = new THREE.MeshStandardMaterial({ color: 0x9a6b43, roughness: 0.7 });
  const top = new THREE.Mesh(new THREE.BoxGeometry(table.w + 0.1, 0.04, table.d + 0.1), wood); top.position.set(0, -0.02, 0); flatTable.add(top);
  for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const leg = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.72, 0.05), wood); leg.position.set(sx * (table.w / 2 - 0.05), -0.4, sz * (table.d / 2 - 0.05)); flatTable.add(leg); }
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(8, 8), new THREE.MeshStandardMaterial({ color: 0x5b5048, roughness: 0.9 })); floor.rotation.x = -Math.PI / 2; floor.position.y = -table.pos.y; flatTable.add(floor);
  flatTable.position.copy(table.pos); scene.add(flatTable);
  head.set(0, 1.45, 0.1); loadOrStart(); S.loaded = true; S.mode = 'design'; flatUI();
  S.steerMode = $('optSteer') ? $('optSteer').value : 'pad';
  S.noir = readStyle(); applyStyle();
}
function flatCamera(dt) {
  if (S.mode === 'menu') return;
  if (S.mode === 'design' || S.mode === 'pick' || S.mode === 'watch') {
    const t = table.pos, cp = Math.cos(orbit.pitch); camera.position.set(t.x + Math.sin(orbit.yaw) * cp * orbit.dist, t.y + Math.sin(orbit.pitch) * orbit.dist, t.z + Math.cos(orbit.yaw) * cp * orbit.dist);
    camera.lookAt(t); head.copy(camera.position); headQ.copy(camera.quaternion);
  } else {
    /* driving: the camera is your head, at the anchor, looking where the car looks; drag to look round */
    head.copy(anchor.p); camera.position.copy(anchor.p); camera.rotation.set(S.lookPitch || 0, anchor.yaw + (S.lookYaw || 0), 0, 'YXZ'); headQ.copy(camera.quaternion);
  }
}
function flatUI() {
  const bar = $('flatbar'); if (!bar || S.xr) return;
  bar.style.display = S.mode === 'menu' ? 'none' : 'flex';
  const design = S.mode === 'design', pick = S.mode === 'pick';
  bar.innerHTML = '';
  if (design) PAL_ITEMS.forEach((it, k) => { const b = document.createElement('button'); const t = it.tool !== undefined ? TOOLS[it.tool] : it; b.textContent = (k < 12 ? ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='][k] + ' ' : '') + t.name; b.className = it.tool === S.tool ? 'sel' : ''; b.style.borderColor = t.color; b.onclick = () => choose(k); bar.appendChild(b); });
  if (pick) CARS.forEach((c, k) => { const b = document.createElement('button'); b.textContent = (k + 1) + ' ' + c.name; b.style.borderColor = c.color; b.onclick = () => { S.carKey = c.key; startDrive(); }; bar.appendChild(b); });
  const msg = document.createElement('div'); msg.className = 'msg';
  msg.textContent = design ? (S.msg || '') : pick ? 'Klik på en bil (eller tast 1–4). Esc: tilbage.' : S.mode === 'drive' ? 'W/S: gas og bremse · A/D: styr · C: kamera · V: byoverblik · P: pause · R: forfra · Esc: tilbage til bordet · træk med musen: kig rundt' : S.mode === 'watch' ? (S.paused ? 'PAUSE. P: fortsæt (ned i bilen) · Esc: stop løbet' : 'Byoverblik: løbet kører, og du kan stadig køre (W/A/S/D). V: ned i bilen · P: pause · Esc: stop løbet') : '';
  bar.appendChild(msg);
}
addEventListener('keydown', e => {
  keys[e.code] = true; if (S.xr || S.mode === 'menu') return;
  if (S.mode === 'design') {
    const k = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='].indexOf(e.key); if (k >= 0) choose(k);
    if (e.code === 'KeyZ' || e.code === 'Backspace') doUndo(); if (e.code === 'Enter') toPick(); if (e.code === 'KeyN') setStyle(!S.noir);
  } else if (S.mode === 'pick') { if (/^[1-4]$/.test(e.key)) { S.carKey = CARS[+e.key - 1].key; startDrive(); } if (e.code === 'Escape') { S.mode = 'design'; showroom.visible = false; flatUI(); } }
  else if (S.mode === 'watch') { if (e.code === 'KeyV') fromWatch(); if (e.code === 'KeyP') { if (S.paused) fromWatch(); else { S.paused = true; signDirty = true; flatUI(); } } if (e.code === 'Escape') stopRace(); }
  else if (S.mode === 'drive') { if (e.code === 'KeyV') toWatch(false); if (e.code === 'KeyP') toWatch(true); if (e.code === 'KeyC') { S.view = S.view === 'cockpit' ? 'chase' : 'cockpit'; updateCamC(0, true); } if (e.code === 'KeyR') restartRace(); if (e.code === 'Escape' || e.code === 'KeyG') growUp(); }
});
addEventListener('keyup', e => { keys[e.code] = false; });
canvas.addEventListener('contextmenu', e => e.preventDefault());
let drag = null;
canvas.addEventListener('pointerdown', e => { audio.init(); canvas.setPointerCapture(e.pointerId); if (e.button === 2 || S.mode === 'drive') drag = { x: e.clientX, y: e.clientY }; else { mouse.down = true; mouse.click = true; } setMouse(e); });
canvas.addEventListener('pointermove', e => { setMouse(e); if (drag) { const dx = e.clientX - drag.x, dy = e.clientY - drag.y; drag = { x: e.clientX, y: e.clientY };
  if (S.mode === 'drive') { S.lookYaw = (S.lookYaw || 0) - dx * 0.004; S.lookPitch = clamp((S.lookPitch || 0) - dy * 0.004, -1.2, 1.2); } else { orbit.yaw -= dx * 0.006; orbit.pitch = clamp(orbit.pitch + dy * 0.005, 0.15, 1.5); } } });
canvas.addEventListener('pointerup', () => { mouse.down = false; drag = null; });
canvas.addEventListener('wheel', e => { orbit.dist = clamp(orbit.dist * Math.exp(e.deltaY * 0.001), 0.35, 3); }, { passive: true });
function setMouse(e) { mouse.x = e.clientX / innerWidth * 2 - 1; mouse.y = -(e.clientY / innerHeight) * 2 + 1; }
addEventListener('resize', () => { if (S.xr) return; renderer.setSize(innerWidth, innerHeight, false); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); });

/* ---------------- the headset session ---------------- */
function readStyle() { const o = $('optStyle'); return params.has('noir') || (o ? o.value === 'noir' : false); }
async function enterXR() {
  audio.init();
  try {
    const session = await navigator.xr.requestSession('immersive-ar', { requiredFeatures: ['local-floor'], optionalFeatures: ['plane-detection', 'mesh-detection'] });
    session.addEventListener('end', () => location.reload());
    await renderer.xr.setSession(session); try { renderer.xr.setFoveation(1); } catch (_) { /* fine */ }
    S.xr = true; document.body.classList.add('playing'); scene.background = null;
    S.steerMode = $('optSteer') ? $('optSteer').value : 'pad'; S.noir = readStyle(); applyStyle();
    S.mode = 'setup'; SCAN.tried = 0;
  } catch (e) { $('status').textContent = 'Kunne ikke starte headset-sessionen: ' + (e.message || e); console.error(e); }
}

renderer.setAnimationLoop(loop);
(async function init() {
  const optStyle = $('optStyle');
  try { if (optStyle) { optStyle.value = localStorage.getItem('bordbyen.style') || 'day'; optStyle.addEventListener('change', () => localStorage.setItem('bordbyen.style', optStyle.value)); } } catch (_) { /* no storage */ }
  const optSteer = $('optSteer');
  try { if (optSteer) { optSteer.value = localStorage.getItem('bordbyen.steer') || 'pad'; optSteer.addEventListener('change', () => localStorage.setItem('bordbyen.steer', optSteer.value)); } } catch (_) { /* no storage */ }
  $('btnFlat').addEventListener('click', enterFlat); $('btnAR').addEventListener('click', enterXR);
  let ok = false; try { ok = !!(navigator.xr && await navigator.xr.isSessionSupported('immersive-ar')); } catch (_) { /* no WebXR */ }
  $('btnAR').disabled = !ok; $('status').textContent = ok ? 'Klar. Stil dig ved et bord og tryk på Start.' : 'Headset-tilstand findes ikke her. Prøv det fladt på computeren.';
  if (params.has('flat')) enterFlat();
})();

window.__bb = { S, orbit, setStyle, LIGHT, findTable, boardPose, stopRace, toWatch, fromWatch, JOLT, city, race, table, head, camC, anchor, keys, mouse, toPick, startDrive, growUp, choose, holder, restartRace, setView: v => { S.view = v; updateCamC(0, true); }, TR };
