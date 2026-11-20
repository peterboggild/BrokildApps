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
import { createCity, TOOLS, CELL, SCALE } from './city.js?v=1';
import { buildCar, CARS } from './cars.js?v=1';
import { createRace, LAPS } from './race.js?v=1';
import { createAudio } from './audio.js?v=1';

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
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.01, 4000);
scene.add(new THREE.HemisphereLight(0xe6f2ff, 0x8a7a60, 1.7));
const sun = new THREE.DirectionalLight(0xfff4e0, 2.0); sun.position.set(0.5, 1, 0.35); scene.add(sun);

/* the sky, for driving: a gradient dome round the camera that fades in as you go down (in design the real room shows instead) */
const skyMat = new THREE.ShaderMaterial({ side: THREE.BackSide, depthWrite: false, transparent: true, uniforms: { uA: { value: 0 } },
  vertexShader: 'varying vec3 vD; void main() { vD = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
  fragmentShader: `precision highp float; varying vec3 vD; uniform float uA;
    void main() { float h = vD.y; vec3 top = vec3(0.38, 0.62, 0.95), hor = vec3(0.86, 0.92, 0.97), low = vec3(0.62, 0.74, 0.62);
      vec3 c = h > 0.0 ? mix(hor, top, pow(h, 0.6)) : mix(hor, low, min(1.0, -h * 4.0));
      vec3 s = normalize(vec3(0.5, 0.75, 0.35)); c += vec3(1.0, 0.9, 0.7) * pow(max(dot(vD, s), 0.0), 400.0) * 2.0 + vec3(0.25, 0.2, 0.1) * pow(max(dot(vD, s), 0.0), 8.0);
      gl_FragColor = vec4(c, uA); }` });
const sky = new THREE.Mesh(new THREE.SphereGeometry(3000, 32, 16), skyMat); sky.renderOrder = -10; sky.frustumCulled = false; sky.visible = false; scene.add(sky);
const FOG = new THREE.Fog(0xdbe9f3, 250, 1400);

/* ---------------- the city, the cars, the race, sound ---------------- */
const city = createCity();
const holder = new THREE.Group(); holder.matrixAutoUpdate = false; scene.add(holder); holder.add(city.root);
let scenery = null;
const audio = createAudio();
const race = createRace({ city,
  onBump: (c, k) => { if (!c.ai) { audio.bump(k); buzz(clamp(0.2 + k * 0.05, 0, 1), 60 + k * 10); } },
  onGate: (c, n) => { if (!c.ai) { audio.chime(2, 880); toast('Port ' + n); } },
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
function updateCamC(dt, snap) {
  const car = player(); if (!car) return;
  if (S.view === 'cockpit') {
    const e = car.obj.userData.eye; camC.p.set(e.x, e.y, e.z).applyAxisAngle(YAX, car.yaw).add(new V(car.p.x, 0, car.p.y)); camC.yaw = car.yaw;
  } else {
    let d = car.yaw - camC.yaw; d = Math.atan2(Math.sin(d), Math.cos(d)); camC.yaw += snap ? d : d * Math.min(1, dt * 3.2);
    const want = new V(0, 3.0, 8.0).applyAxisAngle(YAX, camC.yaw).add(new V(car.p.x, 0, car.p.y));
    if (snap) camC.p.copy(want); else camC.p.lerp(want, Math.min(1, dt * 6));
  }
}
function driveMatrix(out) { return cityMatrix(out, anchor.p, anchor.yaw - camC.yaw, 1, camC.p); }
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
const palCanvas = document.createElement('canvas'); palCanvas.width = 512; palCanvas.height = 600;
const palTex = new THREE.CanvasTexture(palCanvas); palTex.colorSpace = THREE.SRGBColorSpace;
const palette = new THREE.Mesh(new THREE.PlaneGeometry(0.17, 0.2), new THREE.MeshBasicMaterial({ map: palTex, transparent: true, depthTest: false }));
palette.renderOrder = 50; palette.visible = false;
let paletteDirty = true;
const PAL_ITEMS = [...TOOLS.map((t, i) => ({ tool: i })), { act: 'undo', name: 'Fortryd', color: '#9aa6b2' }, { act: 'go', name: 'Kør!', color: '#3ecf6a', wide: true }];
function palRect(k) { if (PAL_ITEMS[k].wide) return [PAL.pad, 4 + 3 * 124, 512 - 2 * PAL.pad, 112]; const c = k % 4, r = Math.floor(k / 4); return [PAL.pad + c * 124, 4 + r * 124, 116, 116]; }
function drawPalette() {
  paletteDirty = false; const x = palCanvas.getContext('2d'); x.clearRect(0, 0, 512, 600);
  x.fillStyle = 'rgba(10,14,22,.86)'; x.beginPath(); x.roundRect(0, 0, 512, 600, 26); x.fill();
  PAL_ITEMS.forEach((it, k) => {
    const [px, py, w, h] = palRect(k), t = it.tool !== undefined ? TOOLS[it.tool] : it, sel = it.tool === S.tool;
    x.fillStyle = sel ? '#ffffff' : 'rgba(255,255,255,.08)'; x.beginPath(); x.roundRect(px, py, w, h, 16); x.fill();
    x.fillStyle = t.color; x.beginPath(); x.roundRect(px + 10, py + 10, w - 20, h - 52, 10); x.fill();
    if (it.tool !== undefined) icon(x, t.key, px + w / 2, py + 10 + (h - 52) / 2);
    x.fillStyle = sel ? '#111' : '#e8eef6'; x.font = `bold ${it.wide ? 34 : 21}px Arial, sans-serif`; x.textAlign = 'center'; x.fillText(t.name, px + w / 2, py + h - 14);
  });
  x.fillStyle = routeInfo && !routeInfo.error ? '#9ff0b4' : '#ffc27a'; x.font = '20px Arial, sans-serif'; x.textAlign = 'left';
  wrap(x, S.msg || '', 20, 520, 472, 24);
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
  const w = (x1 - x0 + 1) * CELL, d = (z1 - z0 + 1) * CELL, h = t.kind === 'lot' && t.solid ? 10 : 1.5;
  cursor.position.set((city.cx(x0) + city.cx(x1)) / 2, h / 2, (city.cz(z0) + city.cz(z1)) / 2); cursor.scale.set(w + 0.4, h, d + 0.4);
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
    } else if (t.kind === 'lot' || t.kind === 'erase') showCursor(D.a, c, t.kind === 'erase' || city.lotOK(t.key, D.a.i, D.a.j, c.i, c.j));
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
  race.grid(routeInfo);
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
  race.clear(); playerCar = null; if (scenery) scenery.visible = false; city.gridLines().visible = true; S.mode = 'design'; scene.fog = null; sky.visible = false; skyMat.uniforms.uA.value = 0; flatUI();
}
function blend(out, t) {
  const e = ease(t), w = new V().lerpVectors(TR.w0, anchor.p, e);
  let dy = (anchor.yaw - camC.yaw) - TR.yaw0; dy = Math.atan2(Math.sin(dy), Math.cos(dy));
  const s = Math.exp(Math.log(TR.s0) * (1 - e));
  return cityMatrix(out, w, TR.yaw0 + dy * e, s, TR.F);
}

/* ---------------- the board in the car: lap, place, time, speed, the countdown ---------------- */
const hudCanvas = document.createElement('canvas'); hudCanvas.width = 512; hudCanvas.height = 200;
const hudTex = new THREE.CanvasTexture(hudCanvas); hudTex.colorSpace = THREE.SRGBColorSpace;
const hud = new THREE.Mesh(new THREE.PlaneGeometry(1, 0.39), new THREE.MeshBasicMaterial({ map: hudTex, transparent: true, depthTest: false })); hud.renderOrder = 60; hud.visible = false; holder.add(hud);
let hudSig = '';
const fmt = t => { const m = Math.floor(t / 60), s = t - m * 60; return m + ':' + (s < 10 ? '0' : '') + s.toFixed(1); };
function drawHud() {
  const c = player(); if (!c) return;
  const ord = race.order(), place = ord.indexOf(c) + 1, lap = clamp(c.lap, 1, LAPS), kmh = Math.round(Math.abs(c.v) * 3.6);
  let big = '';
  if (race.state === 'count') big = race.count > 3 ? '' : String(Math.ceil(race.count)); else if (race.state === 'run' && race.t < 1) big = 'KØR!';
  const sig = [place, lap, Math.floor(race.t * 10), kmh, big, race.state, S.toastT > 0 ? S.toast : ''].join('|'); if (sig === hudSig) return; hudSig = sig;
  const x = hudCanvas.getContext('2d'); x.clearRect(0, 0, 512, 200);
  x.fillStyle = 'rgba(10,14,22,.78)'; x.beginPath(); x.roundRect(0, 0, 512, 200, 26); x.fill();
  x.textAlign = 'left'; x.fillStyle = '#ffd34d'; x.font = 'bold 50px Arial, sans-serif'; x.fillText(place + '/' + race.cars.length, 24, 62);
  x.fillStyle = '#e8eef6'; x.font = 'bold 30px Arial, sans-serif'; x.fillText('Omgang ' + lap + '/' + LAPS, 170, 52);
  x.font = '28px Arial, sans-serif'; x.fillStyle = '#b9c6d6'; x.fillText(fmt(race.state === 'count' ? 0 : c.done ? c.time : race.t), 170, 92);
  x.textAlign = 'right'; x.fillStyle = '#9ff0b4'; x.font = 'bold 46px Arial, sans-serif'; x.fillText(kmh, 470, 62); x.font = '20px Arial, sans-serif'; x.fillText('km/t', 492, 88);
  x.textAlign = 'center';
  if (big) { x.fillStyle = big === 'KØR!' ? '#3ecf6a' : '#ff5a4a'; x.font = 'bold 80px Arial, sans-serif'; x.fillText(big, 256, 178); }
  else if (race.state === 'done' || c.done) { x.fillStyle = '#ffd34d'; x.font = 'bold 30px Arial, sans-serif'; x.fillText(place === 1 ? 'Du vandt! ' : 'Du blev nr. ' + place + '. ', 256, 140); x.fillStyle = '#d8e2ee'; x.font = '22px Arial, sans-serif'; x.fillText('A: kør igen   ·   Y: tilbage til bordet', 256, 178); }
  else if (S.toastT > 0) { x.fillStyle = '#ffffff'; x.font = 'bold 40px Arial, sans-serif'; x.fillText(S.toast, 256, 160); }
  hudTex.needsUpdate = true;
}
function placeHud() {
  const c = player(); if (!c) return; const e = c.obj.userData.eye;
  if (S.view === 'cockpit') { hud.scale.setScalar(0.42); const p = new V(0, e.y - 0.12, e.z - 1.05).applyAxisAngle(YAX, c.yaw); hud.position.set(c.p.x + p.x, p.y, c.p.y + p.z); hud.rotation.set(-0.25, c.yaw, 0, 'YXZ'); }
  else { hud.scale.setScalar(2.2); const p = new V(0, 3.4, 0).applyAxisAngle(YAX, camC.yaw); hud.position.set(c.p.x + p.x, p.y, c.p.y + p.z); hud.rotation.set(-0.2, camC.yaw, 0, 'YXZ'); }
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
  const gas = val(Rr, 0), brake = val(L, 0);
  if (S.steerMode === 'wheel') {
    /* hold the wheel with both grips: its angle is the angle of the line between your hands, seen from where you sit */
    const both = btn(L, 1) && btn(Rr, 1);
    if (both) {
      const lp = new V(), rp = new V(); L.grip.getWorldPosition(lp); Rr.grip.getWorldPosition(rp);
      const right = new V(1, 0, 0).applyAxisAngle(YAX, anchor.yaw), d = rp.sub(lp), a = Math.atan2(d.y, d.dot(right));
      if (!wheelGrip.held) { wheelGrip.held = true; wheelGrip.a0 = a; wheelGrip.base = wheelGrip.angle; }
      wheelGrip.angle = clamp(wheelGrip.base + (a - wheelGrip.a0), -2.2, 2.2);
    } else { wheelGrip.held = false; wheelGrip.angle *= Math.exp(-dt * 4); }
    return { gas, brake, steer: clamp(wheelGrip.angle / 1.6, -1, 1) };
  }
  const [lx] = axes(L), [rx] = axes(Rr); const sx = Math.abs(lx) > Math.abs(rx) ? lx : rx;
  return { gas, brake, steer: -Math.sign(sx) * Math.max(0, Math.abs(sx) - 0.08) / 0.92 };
}

/* ---------------- the room scan: find the table ---------------- */
const SCAN = { next: 0, tried: 0, manual: null };
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
    if (frame.detectedPlanes) for (const p of frame.detectedPlanes) {
      if ((p.semanticLabel || '') !== 'table') continue; const pose = frame.getPose(p.planeSpace, ref); if (!pose) continue;
      const m = new THREE.Matrix4().fromArray(pose.transform.matrix), pts = Array.from(p.polygon, q => new V(q.x, q.y, q.z).applyMatrix4(m));
      add(pts, pts.reduce((s, q) => s + q.y, 0) / pts.length);
    }
    if (frame.detectedMeshes) for (const mesh of frame.detectedMeshes) {
      if ((mesh.semanticLabel || '') !== 'table') continue; const pose = frame.getPose(mesh.meshSpace, ref); if (!pose) continue;
      const m = new THREE.Matrix4().fromArray(pose.transform.matrix), v = mesh.vertices, pts = [];
      for (let k = 0; k < v.length; k += 3) pts.push(new V(v[k], v[k + 1], v[k + 2]).applyMatrix4(m));
      const top = Math.max(...pts.map(p => p.y)); add(pts.filter(p => p.y > top - 0.03), top);
    }
  } catch (e) { console.warn('table scan failed', e); }
  if (!cands.length) return null;
  cands.sort((a, b) => Math.hypot(a.x - head.x, a.z - head.z) - Math.hypot(b.x - head.x, b.z - head.z));
  return cands[0];
}
function setTable(t) {
  table.pos.set(t.x, t.y, t.z); table.w = t.w; table.d = t.d;
  /* turn the board so its long side runs across your view */
  let yaw = t.yaw; if (t.d > t.w) { yaw += Math.PI / 2; const w = table.w; table.w = table.d; table.d = w; }
  const toHead = Math.atan2(head.x - t.x, head.z - t.z); let dd = toHead - yaw; dd = Math.atan2(Math.sin(dd), Math.cos(dd)); if (Math.abs(dd) > Math.PI / 2) yaw += Math.PI;
  table.yaw = yaw; table.found = true;
}

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
      if (o && palette.visible) { raycaster.set(o, d); const h = raycaster.intersectObject(palette)[0]; if (h) { onPal = true; if (click) { const u = h.uv.x * 512, v = (1 - h.uv.y) * 600; const k = PAL_ITEMS.findIndex((_, i) => { const [px, py, w, hh] = palRect(i); return u >= px && u <= px + w && v >= py && v <= py + hh; }); if (k >= 0) choose(k); } } }
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
    const a = clamp((k - 0.55) / 0.35, 0, 1); skyMat.uniforms.uA.value = a; sky.visible = a > 0; scene.fog = k > 0.7 ? FOG : null;
    if (TR.t >= 1) { if (S.mode === 'shrink') { S.mode = 'drive'; } else endGrow(); }
  } else if (S.mode === 'drive') {
    driveButtons();
    const inp = driveInput(dt);
    if (race.state === 'count') { const n = Math.ceil(race.count); if (n !== S.lastCount && n <= 3) { audio.beep(false); S.lastCount = n; } if (race.count - dt <= 0) audio.beep(true); }
    race.update(dt, inp);
    updateCamC(dt, false);
    holder.matrix.copy(driveMatrix(_m1)); holder.matrixWorldNeedsUpdate = true;
    placeHud(); drawHud();
    const pc = player(); if (pc && pc.obj.userData.cockpit) { pc.obj.userData.cockpit.visible = S.view === 'cockpit'; if (pc.obj.userData.roof) pc.obj.userData.roof.visible = S.view !== 'cockpit'; }
    if (S.steerMode === 'wheel' && pc) pc.obj.userData.wheel.rotation.z = wheelGrip.angle;
    /* engines: yours, and the others where they are in the room (the listener is your head) */
    const toWorld = p => new V(p.x, 0.6, p.y).applyMatrix4(holder.matrix);
    audio.engines([player(), ...race.cars.filter(c => c.ai)].map(c => ({ v: c.v, gas: c.ai ? 1 : inp.gas, pos: toWorld(c.p), off: false })),
      { p: head, f: new V(0, 0, -1).applyQuaternion(headQ), u: new V(0, 1, 0).applyQuaternion(headQ) });
  }
  if (S.mode === 'drive' || S.mode === 'shrink' || S.mode === 'grow') sky.position.copy(S.xr ? head : camera.position);
  if (flatTable) flatTable.visible = S.mode === 'design' || S.mode === 'pick' || (S.mode !== 'drive' && skyMat.uniforms.uA.value < 0.5);
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
  edge(L, 5);
}
function driveButtons() {
  if (S.xr) {
    const L = hand('left'), Rr = hand('right');
    if (edge(Rr, 5)) { S.view = S.view === 'cockpit' ? 'chase' : 'cockpit'; updateCamC(0, true); }
    if (edge(Rr, 4)) restartRace();
    if (edge(L, 5)) growUp();
    if (edge(Rr, 3)) setAnchor();
    edge(L, 4);
  }
}
function restartRace() { race.grid(routeInfo); S.lastCount = 4; updateCamC(0, true); }

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
    SCAN.tried += dt; const t = findTable(frame, ref);
    if (t) { setTable(t); return tableDone(); }
    setupText(['Leder efter et bord …', 'Bordet skal være med i headsettets rumscanning (Space Setup).', SCAN.tried > 3 ? 'Intet fundet. Tryk på aftrækkeren for at markere bordet selv.' : '']);
    if (SCAN.tried > 3 && edge(hand('right'), 0)) SCAN.manual = { pts: [] };
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
}
function flatCamera(dt) {
  if (S.mode === 'menu') return;
  if (S.mode === 'design' || S.mode === 'pick') {
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
  if (design) PAL_ITEMS.forEach((it, k) => { const b = document.createElement('button'); const t = it.tool !== undefined ? TOOLS[it.tool] : it; b.textContent = (k < 9 ? (k + 1) + ' ' : k === 9 ? '0 ' : k === 10 ? '- ' : '') + t.name; b.className = it.tool === S.tool ? 'sel' : ''; b.style.borderColor = t.color; b.onclick = () => choose(k); bar.appendChild(b); });
  if (pick) CARS.forEach((c, k) => { const b = document.createElement('button'); b.textContent = (k + 1) + ' ' + c.name; b.style.borderColor = c.color; b.onclick = () => { S.carKey = c.key; startDrive(); }; bar.appendChild(b); });
  const msg = document.createElement('div'); msg.className = 'msg';
  msg.textContent = design ? (S.msg || '') : pick ? 'Klik på en bil (eller tast 1–4). Esc: tilbage.' : S.mode === 'drive' ? 'W/S: gas og bremse · A/D: styr · C: kamera · R: forfra · Esc: tilbage til bordet · træk med musen: kig rundt' : '';
  bar.appendChild(msg);
}
addEventListener('keydown', e => {
  keys[e.code] = true; if (S.xr || S.mode === 'menu') return;
  if (S.mode === 'design') {
    const k = e.key === '0' ? 9 : e.key === '-' ? 10 : /^[1-9]$/.test(e.key) ? +e.key - 1 : -1; if (k >= 0) choose(k);
    if (e.code === 'KeyZ' || e.code === 'Backspace') doUndo(); if (e.code === 'Enter') toPick();
  } else if (S.mode === 'pick') { if (/^[1-4]$/.test(e.key)) { S.carKey = CARS[+e.key - 1].key; startDrive(); } if (e.code === 'Escape') { S.mode = 'design'; showroom.visible = false; flatUI(); } }
  else if (S.mode === 'drive') { if (e.code === 'KeyC') { S.view = S.view === 'cockpit' ? 'chase' : 'cockpit'; updateCamC(0, true); } if (e.code === 'KeyR') restartRace(); if (e.code === 'Escape' || e.code === 'KeyG') growUp(); }
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
async function enterXR() {
  audio.init();
  try {
    const session = await navigator.xr.requestSession('immersive-ar', { requiredFeatures: ['local-floor'], optionalFeatures: ['plane-detection', 'mesh-detection'] });
    session.addEventListener('end', () => location.reload());
    await renderer.xr.setSession(session); try { renderer.xr.setFoveation(1); } catch (_) { /* fine */ }
    S.xr = true; document.body.classList.add('playing'); scene.background = null;
    S.steerMode = $('optSteer') ? $('optSteer').value : 'pad';
    S.mode = 'setup'; SCAN.tried = 0;
  } catch (e) { $('status').textContent = 'Kunne ikke starte headset-sessionen: ' + (e.message || e); console.error(e); }
}

renderer.setAnimationLoop(loop);
(async function init() {
  const optSteer = $('optSteer');
  try { if (optSteer) { optSteer.value = localStorage.getItem('bordbyen.steer') || 'pad'; optSteer.addEventListener('change', () => localStorage.setItem('bordbyen.steer', optSteer.value)); } } catch (_) { /* no storage */ }
  $('btnFlat').addEventListener('click', enterFlat); $('btnAR').addEventListener('click', enterXR);
  let ok = false; try { ok = !!(navigator.xr && await navigator.xr.isSessionSupported('immersive-ar')); } catch (_) { /* no WebXR */ }
  $('btnAR').disabled = !ok; $('status').textContent = ok ? 'Klar. Stil dig ved et bord og tryk på Start.' : 'Headset-tilstand findes ikke her. Prøv det fladt på computeren.';
  if (params.has('flat')) enterFlat();
})();

window.__bb = { S, orbit, city, race, table, head, camC, anchor, keys, mouse, toPick, startDrive, growUp, choose, holder, restartRace, setView: v => { S.view = v; updateCamC(0, true); }, TR };
