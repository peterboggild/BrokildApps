/*
 * Tank Ambush — a mixed-reality tank game for the Meta Quest 3 browser (WebXR).
 *
 * Small tanks drive in through your own room, hide behind your own furniture,
 * peek round the corners and shoot at your headset. You answer with a
 * homing-rocket launcher in each hand.
 *
 *  - The room scan (Quest "Space Setup") arrives as the WebXR `mesh-detection`
 *    meshes. They are drawn depth-only, so a tank behind your sofa is cut off
 *    by the real sofa; and they are rasterised into a voxel grid (world.js)
 *    that decides line of sight, collisions and where tanks can drive.
 *  - Without a scan (or on a laptop) a few virtual barriers stand in for the
 *    furniture and everything else works the same.
 *
 * Run flat in any desktop browser for a preview: WASD + mouse + click.
 */
import * as THREE from './vendor/three.module.min.js';
import {
  W, los, solidAt, rayHit, rebuildGen, rebuildNow, navBlockedAt, navLine, newField, computeField, nearestFree,
  fieldAt, fieldMetres, pathTarget, FAR, cellX, cellZ, GX, computeFieldGen,
} from './world.js?v=20261114a';
import { createFX, createAudio } from './fx.js?v=20261114a';
import { buildTank, buildHeli, heliShadow, buildLauncher, buildRocket, setTrack, makePanel, tankAssets, canvasTex } from './models.js?v=20261114a';

/* ================================================================ basics */

const TAU = Math.PI * 2;
const rand = (a, b) => a + Math.random() * (b - a);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const wrapPi = a => { a %= TAU; if (a > Math.PI) a -= TAU; if (a < -Math.PI) a += TAU; return a; };
const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const V = THREE.Vector3;

const G = {
  mode: 'none',          // 'none' | 'xr' | 'flat'
  phase: 'idle',         // 'idle' | 'menu' | 'play' | 'over'
  hp: 100, score: 0, wave: 0, kills: 0,
  toSpawn: 0, maxAlive: 0, spawnT: 0, waveClear: false, nextWaveT: 0,
  inputLock: 0, hurt: 0, showMesh: false, fieldDirty: true, haveRoom: false, virtual: false,
  menuPending: false, time: 0,
  ammo: { std: 6, heavy: 0 }, toast: '', toastT: 0, fade: { t: -1, hold: false }, invuln: 0, queue: [], pickT: 0,
};

const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setClearColor(0x000000, 0);
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
renderer.domElement.id = 'view';
renderer.domElement.style.display = 'none';        // shown only once a session starts, or it covers the start buttons
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.04, 40);
camera.position.set(0, 1.6, 0);
scene.add(camera);

scene.add(new THREE.HemisphereLight(0xfff1dc, 0x6a5846, 1.0));      // warm, like a lit living room
const sun = new THREE.DirectionalLight(0xfff2de, 1.9);
sun.position.set(0.8, 3.5, 1.6);
scene.add(sun);

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight; camera.updateProjectionMatrix();
  if (G.mode !== 'xr') renderer.setSize(window.innerWidth, window.innerHeight);
});

/* a tiny made-up room (window, ceiling lamps, warm walls, dark floor) turned into an
 * environment map once, so metal and paint get something to reflect */
{
  const eq = canvasTex(256, 128, (c, w, h) => {
    const gr = c.createLinearGradient(0, 0, 0, h);
    gr.addColorStop(0, '#f4ecdc'); gr.addColorStop(0.45, '#b8a58c'); gr.addColorStop(0.55, '#8c7a66'); gr.addColorStop(1, '#4a3d31');
    c.fillStyle = gr; c.fillRect(0, 0, w, h);
    c.fillStyle = '#dfeaff'; c.fillRect(30, 40, 46, 38);                   // window
    c.fillStyle = '#fffbe8'; c.fillRect(150, 6, 40, 12); c.fillRect(200, 8, 30, 10);   // lamps
  });
  eq.mapping = THREE.EquirectangularReflectionMapping;
  const pm = new THREE.PMREMGenerator(renderer);
  scene.environment = pm.fromEquirectangular(eq).texture;
  scene.environmentIntensity = 0.65;
  pm.dispose();
}
const fx = createFX(scene);
const audio = createAudio();
tankAssets();

/* head pose (updated every frame, from the XR viewer pose or the flat camera) */
const head = new V(0, 1.6, 0), headQ = new THREE.Quaternion(), headFwd = new V(0, 0, -1), headUp = new V(0, 1, 0);
const chest = new V();

/* ====================================================== room: occluders */

const occRoot = new THREE.Group();
scene.add(occRoot);
const occMat = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: true, side: THREE.DoubleSide, polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 2 });
const dbgMat = new THREE.MeshBasicMaterial({ color: 0x33ffe0, wireframe: true, transparent: true, opacity: 0.4, side: THREE.DoubleSide });
const scan = { known: new Map(), sources: new Map(), next: 0, gen: null, dirty: false, nextBuild: 0, meshCount: 0, planeCount: 0, feature: '' };

const _m4 = new THREE.Matrix4(), _v3 = new V();

/** World-space copy of a mesh with floor/near-floor horizontal triangles dropped:
 *  tanks drive on the floor and must not be sliced by its z-noise. */
function snapshotTris(verts, indices, matrixArr) {
  _m4.fromArray(matrixArr);
  const n = verts.length / 3, pos = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) {
    _v3.set(verts[i * 3], verts[i * 3 + 1], verts[i * 3 + 2]).applyMatrix4(_m4);
    pos[i * 3] = _v3.x; pos[i * 3 + 1] = _v3.y; pos[i * 3 + 2] = _v3.z;
  }
  const keep = [];
  for (let t = 0; t < indices.length; t += 3) {
    const a = indices[t] * 3, b = indices[t + 1] * 3, c = indices[t + 2] * 3;
    const ux = pos[b] - pos[a], uy = pos[b + 1] - pos[a + 1], uz = pos[b + 2] - pos[a + 2];
    const vx = pos[c] - pos[a], vy = pos[c + 1] - pos[a + 1], vz = pos[c + 2] - pos[a + 2];
    const nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const nl = Math.hypot(nx, ny, nz) || 1;
    const cy = (pos[a + 1] + pos[b + 1] + pos[c + 1]) / 3;
    if (Math.abs(ny / nl) > 0.72 && cy < 0.18) continue;                 // floor (and rugs, door sills...)
    if (pos[a + 1] < 0.03 && pos[b + 1] < 0.03 && pos[c + 1] < 0.03) continue;
    keep.push(indices[t], indices[t + 1], indices[t + 2]);
  }
  return { pos, idx: Uint32Array.from(keep) };
}

function setSource(key, snap) {
  const old = scan.sources.get(key);
  if (old?.mesh) { occRoot.remove(old.mesh); old.mesh.geometry.dispose(); }
  if (!snap.idx.length) { scan.sources.delete(key); scan.dirty = true; return; }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(snap.pos, 3));
  geo.setIndex(new THREE.BufferAttribute(snap.idx, 1));
  const mesh = new THREE.Mesh(geo, G.showMesh ? dbgMat : occMat);
  mesh.renderOrder = -10; mesh.frustumCulled = false; mesh.matrixAutoUpdate = false;
  occRoot.add(mesh);
  scan.sources.set(key, { pos: snap.pos, idx: snap.idx, mesh });
  scan.dirty = true;
}

function scanRoom(frame, refSpace) {
  if (params.get('mesh') === '0') return;                  // ?mesh=0 ignores the headset's room scan
  const now = performance.now();
  if (now < scan.next) return;
  scan.next = now + 700;
  let did = 0;
  const seen = new Set();
  try {
    if (frame.detectedMeshes) {
      scan.feature = 'mesh';
      for (const m of frame.detectedMeshes) {
        seen.add(m);
        const rec = scan.known.get(m);
        if (rec && m.lastChangedTime <= rec.t) continue;
        if (did >= 1) { scan.next = 0; continue; }          // one mesh per frame: spread the cost
        const pose = frame.getPose(m.meshSpace, refSpace);
        if (!pose) continue;
        setSource(m, snapshotTris(m.vertices, m.indices, pose.transform.matrix));
        scan.known.set(m, { t: m.lastChangedTime });
        did++;
      }
    } else if (frame.detectedPlanes) {
      scan.feature = 'plane';
      for (const p of frame.detectedPlanes) {
        seen.add(p);
        const rec = scan.known.get(p);
        if (rec && p.lastChangedTime <= rec.t) continue;
        if (did >= 1) { scan.next = 0; continue; }
        const label = p.semanticLabel || '';
        const pose = frame.getPose(p.planeSpace, refSpace);
        if (!pose) continue;
        scan.known.set(p, { t: p.lastChangedTime });
        did++;
        if (label === 'floor' || label === 'ceiling') { setSource(p, { pos: new Float32Array(0), idx: new Uint32Array(0) }); continue; }
        const pts = Array.from(p.polygon, q => new THREE.Vector2(q.x, q.z));
        const tris = THREE.ShapeUtils.triangulateShape(pts, []);
        const verts = new Float32Array(pts.length * 3);
        pts.forEach((q, i) => { verts[i * 3] = q.x; verts[i * 3 + 1] = 0; verts[i * 3 + 2] = q.y; });
        setSource(p, snapshotTris(verts, Uint32Array.from(tris.flat()), pose.transform.matrix));
      }
    }
  } catch (e) { console.warn('room scan update failed', e); }
  for (const [k] of scan.known) if (!seen.has(k) && scan.feature) { scan.known.delete(k); setSource(k, { pos: new Float32Array(0), idx: new Uint32Array(0) }); }
  scan.meshCount = scan.known.size;
}

/** Spend a few milliseconds a frame turning the scan into the voxel grid. */
function pumpRebuild() {
  const now = performance.now();
  if (!scan.gen && scan.dirty && now > scan.nextBuild) {
    scan.dirty = false;
    scan.gen = rebuildGen([...scan.sources.values()]);
  }
  if (scan.gen) {
    const t0 = performance.now();
    while (performance.now() - t0 < 3) {
      if (scan.gen.next().done) { scan.gen = null; scan.nextBuild = now + 2500; G.fieldDirty = true; break; }
    }
  }
}

function applyMeshVisibility() {
  for (const s of scan.sources.values()) if (s.mesh) s.mesh.material = G.showMesh ? dbgMat : occMat;
}

/* ================================================== virtual cover (fallback) */

const virt = { group: new THREE.Group(), made: false };
scene.add(virt.group);

function addVirtualCover() {
  if (virt.made) return;
  virt.made = true;
  const crate = canvasTex(128, 128, (c, w, h) => {
    c.fillStyle = '#7b5a35'; c.fillRect(0, 0, w, h);
    for (let i = 0; i < 6; i++) { c.fillStyle = i % 2 ? '#6d4f2d' : '#86653c'; c.fillRect(0, i * 22, w, 20); }
    c.strokeStyle = '#3b2a17'; c.lineWidth = 6; c.strokeRect(3, 3, w - 6, h - 6);
    c.beginPath(); c.moveTo(0, 0); c.lineTo(w, h); c.moveTo(w, 0); c.lineTo(0, h); c.stroke();
  });
  const conc = canvasTex(128, 128, (c, w, h) => {
    c.fillStyle = '#8b8d8c'; c.fillRect(0, 0, w, h);
    for (let i = 0; i < 1400; i++) { c.fillStyle = `rgba(${Math.random() < 0.5 ? '40,40,40' : '200,200,200'},0.18)`; c.fillRect(Math.random() * w, Math.random() * h, 2, 2); }
    c.fillStyle = '#d9a21f'; for (let i = 0; i < 4; i++) c.fillRect(0, h - 22 + 0, w, 0);
    c.fillStyle = '#c99a1c'; c.fillRect(0, h - 18, w, 8);
  });
  const panel = canvasTex(128, 128, (c, w, h) => {
    c.fillStyle = '#5f6a6f'; c.fillRect(0, 0, w, h);
    for (let x = 0; x < w; x += 8) { c.fillStyle = (x / 8) % 2 ? '#6b777c' : '#525c61'; c.fillRect(x, 0, 8, h); }
  });
  const mats = {
    crate: new THREE.MeshStandardMaterial({ map: crate, roughness: 0.85 }),
    conc: new THREE.MeshStandardMaterial({ map: conc, roughness: 0.95 }),
    panel: new THREE.MeshStandardMaterial({ map: panel, roughness: 0.6, metalness: 0.4 }),
  };
  const defs = [
    ['panel', 1.3, 1.7, 0.1], ['panel', 1.1, 1.6, 0.1], ['conc', 1.1, 0.8, 0.32], ['conc', 1.0, 0.75, 0.3],
    ['crate', 0.55, 0.55, 0.55], ['crate', 0.6, 1.0, 0.6], ['crate', 0.5, 0.5, 0.5], ['conc', 0.9, 0.7, 0.3],
  ];
  const base = Math.random() * TAU;
  defs.forEach(([k, w, h, d], i) => {
    const a = base + (i / defs.length) * TAU + rand(-0.15, 0.15), r = rand(1.9, 2.9);
    const x = Math.sin(a) * r, z = -Math.cos(a) * r;
    const rot = Math.atan2(-x, -z) + rand(-0.4, 0.4);
    const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mats[k]);
    m.position.set(x, h / 2, z); m.rotation.y = rot;
    virt.group.add(m);
    W.boxes.push({ x, z, w, d, h, rot, y0: 0 });
  });
  W.arenaR = 3.7;
  G.virtual = true;
  rebuildNow([...scan.sources.values()]);
}

function removeVirtualCover() {
  virt.group.clear(); virt.made = false; W.boxes.length = 0; W.arenaR = 0; G.virtual = false;
}

/* ============================================================== navigation */

const playerField = newField();
const pf = { x: 1e9, z: 1e9, t: 0, job: null };
/** The player's flow field is rebuilt in slices of ~2 ms so a walk across the room never hitches. */
function updatePlayerField(dt, force) {
  pf.t -= dt;
  if (force) { pf.job = null; computeField(playerField, nearestFree(head.x, head.z)); pf.x = head.x; pf.z = head.z; pf.t = 0.3; G.fieldDirty = false; return; }
  if (!pf.job) {
    const moved = Math.hypot(head.x - pf.x, head.z - pf.z) > 0.12;
    if (pf.t > 0 || !(moved || G.fieldDirty)) return;
    pf.x = head.x; pf.z = head.z; G.fieldDirty = false;
    pf.job = computeFieldGen(playerField, nearestFree(head.x, head.z));
  }
  const t0 = performance.now();
  while (performance.now() - t0 < 2) {
    if (pf.job.next().done) { pf.job = null; pf.t = 0.3; break; }
  }
}

/* scorch marks left on the floor by blasts, pooled and faded out */
const scorch = (() => {
  const tex = canvasTex(128, 128, (c, w, h) => {
    const gr = c.createRadialGradient(64, 64, 4, 64, 64, 62);
    gr.addColorStop(0, 'rgba(8,6,4,0.95)'); gr.addColorStop(0.5, 'rgba(14,10,8,0.6)'); gr.addColorStop(1, 'rgba(14,10,8,0)');
    c.fillStyle = gr; c.fillRect(0, 0, w, h);
    c.strokeStyle = 'rgba(10,8,6,0.5)'; c.lineWidth = 2;
    for (let i = 0; i < 22; i++) { const a = Math.random() * 6.283, r0 = 12 + Math.random() * 10, r1 = 30 + Math.random() * 30; c.beginPath(); c.moveTo(64 + Math.cos(a) * r0, 64 + Math.sin(a) * r0); c.lineTo(64 + Math.cos(a) * r1, 64 + Math.sin(a) * r1); c.stroke(); }
  });
  const pool = [];
  for (let i = 0; i < 10; i++) {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(1, 1).rotateX(-Math.PI / 2), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2 }));
    m.visible = false; m.userData.t = 0; m.renderOrder = 2; scene.add(m); pool.push(m);
  }
  let k = 0;
  return {
    add(x, z, size) { const m = pool[k++ % pool.length]; m.position.set(x, 0.003, z); m.scale.setScalar(size); m.rotation.y = rand(0, 6.28); m.userData.t = 16; m.material.opacity = 0.85; m.visible = true; },
    update(dt) { for (const m of pool) if (m.visible) { m.userData.t -= dt; m.material.opacity = Math.min(0.85, m.userData.t / 5 * 0.85); if (m.userData.t <= 0) m.visible = false; } },
    clear() { for (const m of pool) m.visible = false; },
  };
})();

/* =========================================================== tanks & co. */

const tanks = [], rockets = [], shells = [];
const mp = new V(), md = new V(), tmpV = new V(), tmpV2 = new V();

function pickSpawn() {
  let best = null, bs = -1;
  for (let i = 0; i < 200; i++) {
    const x = rand(-5.6, 5.6), z = rand(-5.6, 5.6);
    if (navBlockedAt(x, z)) continue;
    const f = fieldAt(playerField, x, z);
    if (f >= FAR) continue;
    const ed = Math.hypot(x - head.x, z - head.z);
    if (ed < 2.3 || fieldMetres(f) > 10) continue;
    if (tanks.some(t => !t.dead && Math.hypot(t.x - x, t.z - z) < 0.6)) continue;
    const hidden = !los(x, 0.12, z, head.x, head.y - 0.05, head.z);
    const s = (hidden ? 60 : 0) + Math.min(ed, 5) * 6 + rand(0, 25);
    if (s > bs) { bs = s; best = { x, z }; }
  }
  return best;
}

/* tier 0: the green tanks. tier 1 (from wave 6): black, a little larger, much tougher. tier 2 (from wave 10): bigger and tougher still. */
const TIER = [
  { hp: 100, size: 1.0, score: 100, dmg: 15, vmul: 1.0, reload: 1.0, burst: 0, sh: 0 },
  { hp: 320, size: 1.3, score: 300, dmg: 22, vmul: 0.85, reload: 0.8, burst: 1, sh: 0.4 },
  { hp: 800, size: 1.75, score: 700, dmg: 32, vmul: 0.7, reload: 0.7, burst: 1, sh: 0.6 },
];

function spawnTank(x, z, yaw, tier = 0) {
  const w = G.wave, T = TIER[tier];
  const model = buildTank(tier);
  scene.add(model.group);
  const t = {
    model, x, z, yaw: yaw ?? Math.atan2(head.x - x, head.z - z), v: 0, yawRate: 0, tyaw: 0, gp: 0,
    tier, size: T.size, maxhp: T.hp, hp: T.hp, scoreVal: T.score, dmg: T.dmg, dead: false, state: 'advance', stateT: 0, hideT: 0, stun: 0, pushX: 0, pushZ: 0, spin: 0,
    reload: rand(3.5, 6) - Math.min(2, w * 0.25), shots: 0, burst: Math.min(3, (w >= 3 ? 2 : 1) + T.burst), reloadTime: Math.max(2.4, 5.0 - 0.25 * w) * T.reload,
    shellSpeed: Math.min(4.6, 2.1 + 0.12 * w + T.sh), aimErr: Math.max(0.012, 0.07 - 0.005 * w) * (tier ? 0.75 : 1),
    vmax: Math.min(0.85, 0.5 + 0.025 * w) * T.vmul, dPref: rand(1.7, 3.2) + tier * 0.4,
    sees: false, seeT: rand(0, 0.12), goalT: 0, gx: x, gz: z, cf: null, spawnT: 0, wreckT: 0, kick: 0,
    eng: audio.engine(), age: 0,
  };
  model.group.scale.setScalar(0.3 * t.size);
  tanks.push(t);
  fx.puff(new V(x, 0.05, z), 0.5 * t.size);
  return t;
}

function moveTo(t, nx, nz) {
  if (!navBlockedAt(nx, nz)) { t.x = nx; t.z = nz; return true; }
  if (!navBlockedAt(nx, t.z)) { t.x = nx; return true; }
  if (!navBlockedAt(t.x, nz)) { t.z = nz; return true; }
  return false;
}

function drive(t, gx, gz, dt, speedMul) {
  const sf = 0.4 + 0.6 * (t.hp / t.maxhp);      // a damaged tank is slower
  let vt = 0, yr = 0;
  if (gx != null && t.stun <= 0) {
    const dx = gx - t.x, dz = gz - t.z, d = Math.hypot(dx, dz);
    if (d > 0.02) {
      const err = wrapPi(Math.atan2(dx, dz) - t.yaw);
      yr = clamp(err * 5, -1.7 * sf, 1.7 * sf);
      vt = t.vmax * sf * speedMul * Math.max(0, 1 - Math.abs(err) / 1.0) * Math.min(1, d / 0.12);
    }
  }
  t.v += (vt - t.v) * Math.min(1, dt * 3.5);
  t.yawRate += (yr - t.yawRate) * Math.min(1, dt * 8);
  t.yaw = wrapPi(t.yaw + t.yawRate * dt);
  if (Math.abs(t.v) > 1e-4 && !moveTo(t, t.x + Math.sin(t.yaw) * t.v * dt, t.z + Math.cos(t.yaw) * t.v * dt)) t.v *= 0.5;
}

function pickCover(t) {
  let best = null, bd = 1e9;
  for (let i = 0; i < 48; i++) {
    const a = rand(0, TAU), r = rand(0.5, 2.4);
    const cx = t.x + Math.sin(a) * r, cz = t.z + Math.cos(a) * r;
    if (navBlockedAt(cx, cz)) continue;
    if (Math.hypot(cx - head.x, cz - head.z) < 1.3) continue;
    if (fieldAt(playerField, cx, cz) >= FAR) continue;
    if (los(cx, 0.12, cz, head.x, head.y - 0.05, head.z) || los(cx, 0.12, cz, head.x, head.y - 0.6, head.z)) continue;
    const d = Math.hypot(cx - t.x, cz - t.z);
    if (d < bd) { bd = d; best = { x: cx, z: cz }; }
  }
  if (!best) return false;
  t.cf ??= newField();
  computeField(t.cf, nearestFree(best.x, best.z, 3), 900);
  if (fieldAt(t.cf, t.x, t.z) >= FAR) return false;
  t.state = 'retreat'; t.stateT = 0;
  return true;
}

function updateTank(t, dt) {
  t.age += dt;
  if (t.spawnT < 1) { t.spawnT = Math.min(1, t.spawnT + dt * 2.2); t.model.group.scale.setScalar(t.size * (0.3 + 0.7 * t.spawnT)); }
  const playing = G.phase === 'play';

  // knocked about by blasts
  if (t.stun > 0) t.stun -= dt;
  if (Math.abs(t.pushX) + Math.abs(t.pushZ) > 0.02) {
    moveTo(t, t.x + t.pushX * dt, t.z + t.pushZ * dt);
    const k = Math.exp(-3.2 * dt); t.pushX *= k; t.pushZ *= k;
  }
  if (Math.abs(t.spin) > 0.01) { t.yaw = wrapPi(t.yaw + t.spin * dt); t.spin *= Math.exp(-3 * dt); }
  if (navBlockedAt(t.x, t.z)) {                         // shoved into a wall: ease back out
    const f = nearestFree(t.x, t.z, 8);
    if (f >= 0) { const cx = -6 + ((f % GX) + 0.5) * 0.06, cz = -6 + (((f / GX) | 0) + 0.5) * 0.06; t.x += clamp(cx - t.x, -0.2 * dt, 0.2 * dt); t.z += clamp(cz - t.z, -0.2 * dt, 0.2 * dt); }
  }

  const px = head.x, pz = head.z;
  const dist = Math.hypot(px - t.x, pz - t.z);

  // can it see the headset?
  t.seeT -= dt;
  if (t.seeT <= 0) {
    t.seeT = 0.12;
    t.model.group.updateMatrixWorld(true);
    t.model.muzzle.getWorldPosition(mp);
    t.sees = los(mp.x, mp.y, mp.z, head.x, head.y - 0.05, head.z);
  }

  let gx = null, gz = null, speedMul = 1;
  if (!playing) { drive(t, null, null, dt, 0); }
  else {
    switch (t.state) {
      case 'advance': {
        t.goalT -= dt;
        if (t.goalT <= 0) {
          t.goalT = 0.2;
          if (navLine(t.x, t.z, px, pz)) { t.gx = px; t.gz = pz; }
          else { const r = pathTarget(playerField, t.x, t.z); if (r.ok) { t.gx = r.x; t.gz = r.z; } else { t.gx = px; t.gz = pz; } }
        }
        gx = t.gx; gz = t.gz;
        // stop to take a shot, and stop once close enough
        if ((t.sees && dist < 5.6 && t.reload < 0.45) || (t.sees && dist < t.dPref) || dist < 1.4) { speedMul = 0; gx = px; gz = pz; }
        break;
      }
      case 'retreat': {
        t.stateT += dt;
        const r = pathTarget(t.cf, t.x, t.z);
        if (r.ok) { gx = r.x; gz = r.z; }
        speedMul = 1.1;
        if (fieldAt(t.cf, t.x, t.z) < 25 || t.stateT > 7) { t.state = 'hide'; t.hideT = rand(1.6, 3.8); gx = null; }
        break;
      }
      case 'hide': {
        t.hideT -= dt;
        speedMul = 0;
        if (t.hideT <= 0) { t.state = 'advance'; t.goalT = 0; }
        break;
      }
    }
    drive(t, gx, gz, dt, speedMul);
  }

  // turret and gun follow the headset
  t.model.group.updateMatrixWorld(true);
  t.model.muzzle.getWorldPosition(mp);
  const sf = 0.6 + 0.4 * (t.hp / t.maxhp);
  const wantYaw = wrapPi(Math.atan2(head.x - t.x, head.z - t.z) - t.yaw);
  const eY = wrapPi(wantYaw - t.tyaw);
  t.tyaw = wrapPi(t.tyaw + clamp(eY, -1.7 * sf * dt, 1.7 * sf * dt));
  const wantP = Math.atan2((head.y - 0.06) - mp.y, Math.max(0.1, Math.hypot(head.x - mp.x, head.z - mp.z)));
  const eP = clamp(wantP, -0.12, 1.15) - t.gp;
  t.gp += clamp(eP, -1.2 * dt, 1.2 * dt);

  // fire
  t.reload -= dt;
  if (playing && t.sees && dist < 6 && t.reload <= 0 && t.stun <= 0 && Math.abs(eY) < 0.05 && Math.abs(eP) < 0.07 && Math.abs(t.v) < 0.1) {
    fireShell(t);
    t.shots++;
    if (t.shots >= t.burst) {
      t.shots = 0; t.reload = t.reloadTime * rand(0.85, 1.2);
      if (Math.random() < 0.6) pickCover(t);
    } else t.reload = 0.8;
  }

  // visuals
  const m = t.model, g = m.group;
  t.kick *= Math.exp(-9 * dt);
  g.position.set(t.x, 0.004, t.z);
  g.rotation.y = t.yaw;
  m.turret.rotation.y = t.tyaw;
  m.gun.rotation.x = -t.gp;
  m.gun.position.z = 0.076 - 0.018 * t.kick;
  m.body.rotation.x = -0.03 * t.kick + clamp(t.v - 0.2, -0.3, 0.3) * 0.01;
  m.body.position.y = Math.sin(t.age * 31) * 0.0007 * Math.min(1, Math.abs(t.v) * 3);
  for (let s = 0; s < 2; s++) {
    const tr = m.tracks[s], sp = t.v - t.yawRate * tr.x * t.size, d = sp * dt / t.size;      // d in the model's own units
    if (Math.abs(d) > 1e-6) {
      tr.off += d; setTrack(tr.mesh, tr.x, tr.off);
      for (const w of m.wheels[s]) w.g.rotation.x += d / w.r;
    }
  }
  t.eng.set(tmpV.set(t.x, 0.1, t.z), clamp(Math.abs(t.v) / 0.7 + Math.abs(t.yawRate) * 0.15, 0, 1));
}

function fireShell(t) {
  const m = t.model;
  m.group.updateMatrixWorld(true);
  m.muzzle.getWorldPosition(mp);
  m.muzzle.getWorldDirection(md);
  // the head is the target; a little scatter that shrinks as the game goes on
  const aim = tmpV.set(head.x, head.y - 0.06, head.z).sub(mp).normalize();
  aim.x += rand(-1, 1) * t.aimErr; aim.y += rand(-1, 1) * t.aimErr; aim.z += rand(-1, 1) * t.aimErr; aim.normalize();
  const core = new THREE.Mesh(shellGeo, t.tier ? shellMatRed : shellMat);
  core.scale.setScalar(Math.min(1.6, t.size));
  core.position.copy(mp);
  scene.add(core);
  shells.push({ obj: core, pos: mp.clone(), vel: aim.clone().multiplyScalar(t.shellSpeed), age: 0, dmg: t.dmg, tier: t.tier });
  fx.muzzleFlash(mp, aim);
  audio.tankShot(mp.clone());
  t.kick = 1;
}

const shellGeo = new THREE.SphereGeometry(0.013, 12, 10);
const shellMat = new THREE.MeshBasicMaterial({ color: 0xfff0c0, toneMapped: false });
const shellMatRed = new THREE.MeshBasicMaterial({ color: 0xff7a66, toneMapped: false });

function updateShells(dt) {
  for (let i = shells.length - 1; i >= 0; i--) {
    const s = shells[i];
    s.age += dt;
    const step = s.vel.length() * dt, n = Math.max(1, Math.ceil(step / 0.03));
    let boom = false;
    for (let k = 0; k < n && !boom; k++) {
      s.pos.addScaledVector(s.vel, dt / n);
      if (playerDist(s.pos) < 0.2 && G.phase === 'play') boom = true;
      else if (s.pos.y <= 0.01 || solidAt(s.pos.x, s.pos.y, s.pos.z)) boom = true;
    }
    if (boom || s.age > 9 || s.pos.length() > 20) {
      scene.remove(s.obj); shells.splice(i, 1);
      if (boom) shellBoom(s.pos, s.dmg, s.small);
      continue;
    }
    s.obj.position.copy(s.pos);
    // glowing head and a little tail
    const c0 = s.tier ? [1, 0.35, 0.25] : [1, 0.8, 0.35], c1 = s.tier ? [0.9, 0.1, 0.05] : [1, 0.4, 0.1], z = s.small ? 0.45 : 1;
    fx.fire.add(s.pos.x, s.pos.y, s.pos.z, 0, 0, 0, 0.07, (s.tier ? 0.26 : 0.2) * z, 0.15 * z, c0, c1, 0.95);
    for (let k = 1; k <= 2; k++)
      fx.fire.add(s.pos.x - s.vel.x * 0.03 * k, s.pos.y - s.vel.y * 0.03 * k, s.pos.z - s.vel.z * 0.03 * k, rand(-0.03, 0.03), rand(-0.03, 0.03), rand(-0.03, 0.03), 0.45, 0.075 * z, 0.02, c0, [0.5, 0.05, 0], 0.8);
    if (s.rocket) fx.smoke.add(s.pos.x, s.pos.y, s.pos.z, rand(-0.03, 0.03), 0.02, rand(-0.03, 0.03), 1.0, 0.03, 0.14, [0.6, 0.6, 0.6], [0.3, 0.3, 0.3], 0.45, 0.6, 0);
  }
}

/** Distance from a point to the player's head or chest. */
function playerDist(p) {
  chest.set(head.x, head.y - 0.38, head.z);
  return Math.min(p.distanceTo(head), p.distanceTo(chest) * 0.85);
}

function shellBoom(p, dmgMax = 15, small = false) {
  if (small) {                                    // a cannon round: sparks, and damage only for a direct hit
    fx.spark(p, 1.3);
    if (G.phase === 'play' && playerDist(p) < 0.24) hurtPlayer(dmgMax);
    return;
  }
  fx.explosion(p, 0.5);
  if (p.y < 0.3) scorch.add(p.x, p.z, 0.25);
  audio.explosion(p.clone(), 0.55);
  if (G.phase !== 'play') return;
  const d = playerDist(p);
  const dmg = Math.round(dmgMax * clamp(1 - d / 0.75, 0, 1));
  if (dmg >= 2) hurtPlayer(dmg);
}

/* A hit takes the view to red, then black, then brings it back, over about two seconds.
 * You cannot see during the black, so the same two seconds are also a grace period. */
function hurtPlayer(d) {
  if (G.phase !== 'play' || G.invuln > 0) return;
  G.hp = Math.max(0, G.hp - d);
  G.hurt = 1;
  G.invuln = 2.0;
  G.fade.t = 0; G.fade.hold = false;
  audio.hurt();
  for (const h of hands) haptic(h, 1, 160);
  if (G.hp <= 0) { G.fade.hold = true; gameOver(); }
}

/* --------------------------------------------------------------- damage */

function insideTank(t, p) {
  if (t.air) {                                                   // a helicopter: fuselage ellipsoid, tail boom, stub wings
    const dx = p.x - t.x, dz = p.z - t.z, s = Math.sin(t.yaw), c = Math.cos(t.yaw), k = t.size;
    const lz = (dx * s + dz * c) / k, lx = (dx * c - dz * s) / k, ly = (p.y - t.y) / k;
    if ((lx / 0.085) ** 2 + (ly / 0.075) ** 2 + (lz / 0.19) ** 2 < 1) return true;
    if (lz < -0.12 && lz > -0.34 && Math.abs(lx) < 0.03 && Math.abs(ly) < 0.035) return true;
    return Math.abs(lx) < 0.12 && Math.abs(lz) < 0.03 && ly > -0.03 && ly < 0.02;
  }
  const dx = p.x - t.x, dz = p.z - t.z, s = Math.sin(t.yaw), c = Math.cos(t.yaw);
  const lz = dx * s + dz * c, lx = dx * c - dz * s;
  const k = t.size;
  return Math.abs(lx) < 0.13 * k && Math.abs(lz) < 0.19 * k && p.y < 0.17 * k && p.y > -0.02;
}

function damageTank(t, dmg, from, push, light = false) {
  if (t.dead) return;
  t.hp -= dmg;
  if (t.hp <= 0) { killTank(t); return; }
  if (light) return;                       // bullets chip away; they do not shove, stun or scare a tank
  // hit but alive: shoved sideways, shaken, and slower from now on (heavier tanks move less)
  push /= t.size * t.size;
  if (t.air) {                             // a helicopter is knocked through the air and swings round
    const dx = t.x - from.x, dy = t.y - from.y, dz = t.z - from.z, l = Math.hypot(dx, dy, dz) || 1;
    t.vx += dx / l * push * 0.9; t.vy += dy / l * push * 0.6; t.vz += dz / l * push * 0.9;
    t.yawKick += rand(-1, 1) * push * 2.2; t.stun = Math.max(t.stun, 0.4 + push * 0.15); t.wpT = 0;
    return;
  }
  const dx = t.x - from.x, dz = t.z - from.z, l = Math.hypot(dx, dz) || 1;
  t.pushX += (dx / l) * push; t.pushZ += (dz / l) * push;
  t.spin += rand(-1, 1) * push * 2.2;
  t.stun = Math.max(t.stun, 0.35 + push * 0.3);
  t.kick = 1;
  if (t.state === 'advance' && Math.random() < 0.5) pickCover(t);
}

function killTank(t) {
  if (t.air) { killHeli(t); return; }
  t.dead = true; t.wreckT = 5;
  G.score += t.scoreVal; G.kills++;
  const p = tmpV.set(t.x, 0.07, t.z);
  fx.explosion(p, Math.min(1.7, t.size));
  scorch.add(t.x, t.z, 0.7 * t.size);
  audio.explosion(p.clone(), 0.9);
  t.eng.stop();
  const m = t.model, a = tankAssets();
  m.group.updateMatrixWorld(true);
  fx.flingObject(m.turret, new V(rand(-1, 1), rand(2.6, 3.8), rand(-1, 1)));
  for (let i = 0; i < 10; i++) fx.debrisBox(new V(t.x, 0.08, t.z), new V(rand(-2.2, 2.2), rand(1.5, 3.8), rand(-2.2, 2.2)), rand(0.012, 0.03), i < 3);
  const burn = o => { if (o.isMesh && !o.isInstancedMesh && o !== m.shadow) o.material = o.material === a.camo ? a.wreckCamo : a.char; };
  m.group.traverse(burn);
  m.turret.traverse(burn);
  m.body.rotation.z = rand(-0.1, 0.1);
}

function blast(p, direct, heavy = false) {
  fx.explosion(p, heavy ? 1.6 : 1.15);
  if (p.y < 0.5) scorch.add(p.x, p.z, heavy ? 0.85 : 0.55);
  audio.explosion(p.clone(), heavy ? 1.4 : 1.1);
  const R = heavy ? 1.15 : 0.8, dmgMax = heavy ? 200 : 95, direct$ = heavy ? 260 : 110;
  for (const t of tanks) {
    if (t.dead) continue;
    if (t === direct) { damageTank(t, direct$, p, heavy ? 2 : 1); continue; }
    const d = Math.hypot(p.x - t.x, p.y - (t.air ? t.y : 0.06 * t.size), p.z - t.z) - 0.1 * (t.size - 1);
    if (d > R) continue;
    const k = 1 - Math.max(0, d) / R;
    damageTank(t, Math.round(dmgMax * k), p, (3.4 * k + 0.6) * (heavy ? 1.4 : 1));
  }
  for (const h of hands) { const wp = h.ctrl.getWorldPosition(tmpV2); if (wp.distanceTo(p) < 2.5) haptic(h, 0.5, 80); }
}

/* -------------------------------------------------------------- rockets */

/** Height of the middle of a target: floor level for a tank, its altitude for a helicopter. */
const cyOf = t => (t.air ? t.y : 0.07 * t.size);

function lockCandidate(origin, dir, maxAng, maxD, ignore) {
  let best = null, ba = maxAng;
  for (const t of tanks) {
    if (t.dead || t === ignore) continue;
    tmpV.set(t.x - origin.x, cyOf(t) - origin.y, t.z - origin.z);
    const d = tmpV.length();
    if (d > maxD || d < 0.2) continue;
    const ang = Math.acos(clamp(tmpV.dot(dir) / d, -1, 1));
    if (ang >= ba) continue;
    if (!los(origin.x, origin.y, origin.z, t.x, cyOf(t) + 0.01, t.z, 0.08, 0.1)) continue;
    ba = ang; best = t;
  }
  return best;
}

function launch(h) {
  if (G.phase !== 'play' || h.cool > 0) return;
  const A = G.ammo;
  if (A.std + A.heavy <= 0) return;                 // out of rockets: the machine gun takes over
  const heavy = A.heavy > 0;
  if (heavy) A.heavy--; else A.std--;
  h.cool = 1.15;
  h.ctrl.updateMatrixWorld(true);
  h.L.muzzle.getWorldPosition(mp);
  const dir = new V(0, 0, -1).transformDirection(h.ctrl.matrixWorld);
  const obj = buildRocket(heavy);
  obj.scale.setScalar(heavy ? 0.62 : 0.5);
  scene.add(obj);
  dir.x += rand(-0.006, 0.006); dir.y += rand(-0.006, 0.006); dir.normalize();     // a hair of launch scatter
  const r = { obj, pos: mp.clone(), dir: dir.clone(), vel: new V(), speed: ROCKET.v0, age: 0, target: h.lock, heavy };
  rockets.push(r);
  h.L.rocket.visible = false; h.L.rocketH.visible = false;
  h.L.ledMat.color.setHex(0xff3322);
  // back-blast
  h.L.rear.getWorldPosition(tmpV);
  for (let i = 0; i < 14; i++) fx.smoke.add(tmpV.x, tmpV.y, tmpV.z, -dir.x * rand(0.5, 2) + rand(-0.3, 0.3), -dir.y * rand(0.5, 2) + rand(-0.2, 0.3), -dir.z * rand(0.5, 2) + rand(-0.3, 0.3), rand(0.6, 1.2), 0.05, 0.22, [0.55, 0.53, 0.5], [0.2, 0.2, 0.2], 0.5, 1.5, 0);
  for (let i = 0; i < 8; i++) fx.fire.add(tmpV.x, tmpV.y, tmpV.z, -dir.x * rand(1, 4), -dir.y * rand(1, 4) + rand(-0.3, 0.3), -dir.z * rand(1, 4), rand(0.1, 0.25), 0.07, 0.02, [1, 0.85, 0.5], [1, 0.3, 0.05], 1, 3, 0);
  audio.launch(mp.clone());
  haptic(h, 0.9, 120);
}

const _q = new THREE.Quaternion(), _z = new V(0, 0, 1), _tgt = new V();
/*
 * Rocket flight. The launcher ejects it at 3 m/s, the motor burns for one second and
 * accelerates it to ~10 m/s, then it coasts: drag bleeds speed and gravity pulls the
 * nose down in a ballistic arc. The seeker can only nudge the heading (0.5 rad/s at full
 * speed, and only while the motor burns, inside an 11° cone), so it forgives a slightly
 * bad aim but never rescues a bad one — and over a long shot you must aim a bit high.
 */
const ROCKET = { burn: 1.0, v0: 3.0, vmax: 10, accel: 7, turn: 0.5, lockCone: 0.22, gBurn: 0.15, gCoast: 5, drag: 0.22 };
function updateRockets(dt) {
  for (let i = rockets.length - 1; i >= 0; i--) {
    const r = rockets[i];
    r.age += dt;
    const burning = r.age < ROCKET.burn;
    r.speed = burning ? Math.min(ROCKET.vmax, r.speed + ROCKET.accel * dt) : r.speed * Math.exp(-ROCKET.drag * dt);

    // weak seeker: after a short delay, nudge the heading toward the locked tank
    if (burning && r.age > 0.15) {
      if (r.target && (r.target.dead || !los(r.pos.x, r.pos.y, r.pos.z, r.target.x, cyOf(r.target) + 0.01, r.target.z, 0.1, 0.1))) r.target = null;
      if (!r.target) r.target = lockCandidate(r.pos, r.dir, ROCKET.lockCone * 0.75, 7, null);
      if (r.target) {
        _tgt.set(r.target.x, cyOf(r.target), r.target.z).sub(r.pos);
        const d = _tgt.length();
        if (d > 0.01) {
          _tgt.divideScalar(d);
          const ang = Math.acos(clamp(r.dir.dot(_tgt), -1, 1));
          const turn = ROCKET.turn * (r.speed / ROCKET.vmax) * dt;
          if (ang > 1e-4) r.dir.lerp(_tgt, Math.min(1, turn / ang)).normalize();
        }
      }
    }
    // velocity = heading * speed, then gravity bends the path
    r.vel.copy(r.dir).multiplyScalar(r.speed);
    r.vel.y -= (burning ? ROCKET.gBurn : ROCKET.gCoast) * dt;
    r.speed = r.vel.length();
    if (r.speed > 1e-3) r.dir.copy(r.vel).divideScalar(r.speed);
    if (!burning) r.speed = Math.max(r.speed, 0.1);

    const step = r.speed * dt, n = Math.max(1, Math.ceil(step / 0.04));
    let hit = null, boom = false;
    for (let k = 0; k < n && !boom; k++) {
      r.pos.addScaledVector(r.dir, step / n);
      if (r.pos.y <= 0.01 || solidAt(r.pos.x, r.pos.y, r.pos.z)) { boom = true; break; }
      for (const t of tanks) {
        if (t.dead) continue;
        if (insideTank(t, r.pos)) { hit = t; boom = true; break; }
        // proximity fuse: goes off at the closest point of a near miss
        const d = Math.hypot(r.pos.x - t.x, r.pos.y - cyOf(t), r.pos.z - t.z);
        if (d < 0.45 * t.size) {
          if (r.near === t && d > r.nearD + 0.004) { boom = true; break; }
          if (r.near !== t || d < r.nearD) { r.near = t; r.nearD = d; }
        }
      }
    }
    if (boom || r.age > 6 || r.pos.length() > 20) {
      scene.remove(r.obj); rockets.splice(i, 1);
      if (boom) blast(r.pos.clone(), hit, r.heavy);
      else if (r.age > 6) fx.puff(r.pos, 0.5);             // self-destruct
      continue;
    }
    r.obj.position.copy(r.pos);
    r.obj.quaternion.setFromUnitVectors(_z, r.dir);
    // motor flame and smoke while it burns; a thin wisp of smoke as it coasts
    const tx = r.pos.x - r.dir.x * 0.17, ty = r.pos.y - r.dir.y * 0.17, tz = r.pos.z - r.dir.z * 0.17;
    if (burning) {
      fx.fire.add(tx, ty, tz, 0, 0, 0, 0.08, 0.085, 0.03, [1, 0.9, 0.55], [1, 0.35, 0.05], 1);
      fx.fire.add(tx, ty, tz, -r.dir.x * 1.2, -r.dir.y * 1.2, -r.dir.z * 1.2, 0.16, 0.05, 0.015, [1, 0.8, 0.3], [0.8, 0.2, 0], 0.9);
      fx.smoke.add(tx, ty, tz, rand(-0.04, 0.04), rand(0.0, 0.08), rand(-0.04, 0.04), 1.4, 0.035, 0.17, [0.75, 0.75, 0.74], [0.35, 0.35, 0.35], 0.5, 0.6, 0);
    } else if (r.age < ROCKET.burn + 0.5) {
      fx.smoke.add(tx, ty, tz, rand(-0.03, 0.03), rand(0.0, 0.05), rand(-0.03, 0.03), 0.9, 0.025, 0.1, [0.7, 0.7, 0.7], [0.35, 0.35, 0.35], 0.28, 0.8, 0);
    }
  }
}

/* ================================================================ helicopters */

/* From wave 8 an attack helicopter joins in; from wave 12 a bigger black gunship. They fly in the room's
 * free air at 1-2 m, keep their nose on your head, strafe between waypoints that can see you, fire bursts of
 * tracer rounds (the gunship also fires rockets) and when shot down they spin out, burn and crash. */
const HELI = [
  { hp: 240, size: 1.0, score: 400, dmg: 5, rounds: 5, speed: 6.0, v: 1.0 },
  { hp: 520, size: 1.5, score: 900, dmg: 8, rounds: 8, speed: 6.5, v: 0.85 },
];
const tracerGeo = new THREE.SphereGeometry(0.008, 8, 6);
const tracerMat = new THREE.MeshBasicMaterial({ color: 0xffe0a0, toneMapped: false });

function ceilingAt(x, z) { for (let y = 1.0; y < 3.1; y += 0.1) if (solidAt(x, y, z)) return y; return 3.2; }
const clearAir = (x, y, z, r = 0.25) => !(solidAt(x, y, z) || solidAt(x, y + 0.3, z) || solidAt(x, y - 0.3, z) || solidAt(x + r, y, z) || solidAt(x - r, y, z) || solidAt(x, y, z + r) || solidAt(x, y, z - r));

function pickHeliSpawn() {
  let best = null, bs = -1;
  for (let i = 0; i < 60; i++) {
    const a = rand(0, TAU), r = W.arenaR > 0 ? rand(1.8, 3.0) : rand(2.6, 4.6);
    let x = head.x + Math.sin(a) * r, z = head.z + Math.cos(a) * r;
    if (W.arenaR > 0) { const rr = Math.hypot(x, z); if (rr > W.arenaR - 0.4) { x *= (W.arenaR - 0.4) / rr; z *= (W.arenaR - 0.4) / rr; } }
    const y = rand(1.0, 1.7);
    if (!clearAir(x, y, z) || Math.hypot(x - head.x, z - head.z) < 1.8) continue;
    if (tanks.some(t => !t.dead && t.air && Math.hypot(t.x - x, t.z - z) < 0.8)) continue;
    const s = (!los(x, y, z, head.x, head.y - 0.05, head.z) ? 50 : 0) + rand(0, 30);
    if (s > bs) { bs = s; best = { x, y, z }; }
  }
  return best;
}

function spawnHeli(x, y, z, htier = 0) {
  const T = HELI[htier], w = G.wave;
  const model = buildHeli(htier);
  scene.add(model.group);
  const shadow = heliShadow(); shadow.renderOrder = 1; scene.add(shadow);
  const t = {
    air: true, model, shadow, tier: 3 + htier, htier, size: T.size, maxhp: T.hp, hp: T.hp, scoreVal: T.score, dmg: T.dmg,
    x, y, z, vx: 0, vy: 0, vz: 0, yaw: Math.atan2(head.x - x, head.z - z), pitch: 0, roll: 0, yawKick: 0, spin: 0,
    dead: false, state: 'fly', wp: { x, y, z }, wpT: 0, unseenT: 0, sees: false, seeT: rand(0, 0.12),
    reload: rand(3, 5), burstLeft: 0, burstT: 0, missileT: rand(5, 8), stun: 0, spawnT: 0, wreckT: 0, landed: false,
    rounds: T.rounds, shotSpeed: T.speed + Math.min(1.5, 0.06 * w), vmax: (1.0 + Math.min(0.5, 0.04 * w)) * T.v,
    eng: audio.rotor(), age: 0,
  };
  model.group.scale.setScalar(0.3 * t.size);
  model.group.position.set(x, y, z);
  tanks.push(t);
  fx.puff(new V(x, y, z), 0.5);
  return t;
}

function pickHeliWaypoint(t) {
  let best = null, bs = -1e9;
  for (let i = 0; i < 28; i++) {
    const a = rand(0, TAU), r = rand(2.0, 4.0);
    let x = head.x + Math.sin(a) * r, z = head.z + Math.cos(a) * r;
    if (W.arenaR > 0) { const rr = Math.hypot(x, z); if (rr > W.arenaR - 0.35) { x *= (W.arenaR - 0.35) / rr; z *= (W.arenaR - 0.35) / rr; } }
    const top = Math.min(2.0, ceilingAt(x, z) - 0.25);
    if (top < 0.8) continue;
    const y = rand(0.8, top);
    if (!clearAir(x, y, z)) continue;
    if (!los(x, y, z, head.x, head.y - 0.05, head.z)) continue;                       // from there it can see you
    if (!los(t.x, t.y, t.z, x, y, z, 0.15, 0.15)) continue;                           // and the way there is clear
    const d = Math.hypot(x - t.x, z - t.z);
    const s = 10 - Math.abs(d - 1.6) * 2 + rand(0, 4);
    if (s > bs) { bs = s; best = { x, y, z }; }
  }
  if (best) { t.wp = best; t.wpT = rand(2.2, 4.0); }
  else { t.wp = { x: head.x + Math.sin(t.age) * 2.2, y: Math.max(1.0, t.y), z: head.z + Math.cos(t.age) * 2.2 }; t.wpT = 0.7; }
}

const AVD = [[1, 0, 0], [-1, 0, 0], [0, 0, 1], [0, 0, -1], [0, 1, 0], [0, -1, 0]];
function updateHeli(t, dt) {
  t.age += dt;
  const playing = G.phase === 'play', m = t.model, hpf = t.hp / t.maxhp;
  if (t.spawnT < 1) { t.spawnT = Math.min(1, t.spawnT + dt * 1.8); m.group.scale.setScalar(t.size * (0.3 + 0.7 * t.spawnT)); }
  if (t.stun > 0) t.stun -= dt;

  // can it see the headset?
  t.seeT -= dt;
  if (t.seeT <= 0) {
    t.seeT = 0.12;
    m.group.updateMatrixWorld(true); m.muzzle.getWorldPosition(mp);
    t.sees = los(mp.x, mp.y, mp.z, head.x, head.y - 0.05, head.z);
    t.unseenT = t.sees ? 0 : t.unseenT + 0.12;
  }

  // where to fly: a waypoint that can see you, renewed every few seconds or when it loses sight of you
  t.wpT -= dt;
  if (playing && (t.wpT <= 0 || t.unseenT > 1.4)) { pickHeliWaypoint(t); t.unseenT = 0; }
  const ex = t.wp.x - t.x, ey = t.wp.y - t.y, ez = t.wp.z - t.z, ed = Math.hypot(ex, ey, ez) || 1;
  const vmax = t.vmax * (0.45 + 0.55 * hpf) * (t.stun > 0 ? 0.3 : 1);
  const sp = playing ? Math.min(vmax, ed * 1.3) : 0;
  let ax = (ex / ed * sp - t.vx) * 2.6, ay = (ey / ed * sp - t.vy) * 2.6, az = (ez / ed * sp - t.vz) * 2.6;
  for (const d of AVD)                                                        // keep off walls, furniture and the ceiling
    if (solidAt(t.x + d[0] * 0.24 * t.size, t.y + d[1] * 0.2 * t.size, t.z + d[2] * 0.24 * t.size)) { ax -= d[0] * 3.4; ay -= d[1] * 3.4; az -= d[2] * 3.4; }
  const hx = t.x - head.x, hy = t.y - head.y, hz = t.z - head.z, hd = Math.hypot(hx, hy, hz);
  if (hd < 1.4 && hd > 1e-3) { const k = (1.4 - hd) * 5 / hd; ax += hx * k; ay += hy * k; az += hz * k; }   // never right on top of you
  if (t.y < 0.5) ay += (0.5 - t.y) * 8;
  ax += Math.sin(t.age * 1.3) * 0.25; az += Math.cos(t.age * 1.1) * 0.25; ay += Math.sin(t.age * 1.9) * 0.2;   // hover wobble
  const al = Math.hypot(ax, ay, az);
  if (al > 3.6) { ax *= 3.6 / al; ay *= 3.6 / al; az *= 3.6 / al; }
  t.vx += ax * dt; t.vy += ay * dt; t.vz += az * dt;
  const drag = Math.exp(-0.35 * dt); t.vx *= drag; t.vy *= drag; t.vz *= drag;
  const nx = t.x + t.vx * dt, ny = t.y + t.vy * dt, nz = t.z + t.vz * dt;
  if (solidAt(nx, ny, nz)) { t.vx *= -0.25; t.vy *= -0.25; t.vz *= -0.25; } else { t.x = nx; t.y = ny; t.z = nz; }
  if (W.arenaR > 0) { const rr = Math.hypot(t.x, t.z); if (rr > W.arenaR - 0.2) { t.x *= (W.arenaR - 0.2) / rr; t.z *= (W.arenaR - 0.2) / rr; t.vx *= 0.5; t.vz *= 0.5; } }

  // nose on the player; bank into the acceleration like a real one
  const want = Math.atan2(head.x - t.x, head.z - t.z), rel = wrapPi(want - t.yaw);
  t.yaw = wrapPi(t.yaw + clamp(rel, -1.6 * dt * (t.stun > 0 ? 0.3 : 1), 1.6 * dt) + t.yawKick * dt);
  t.yawKick *= Math.exp(-3 * dt);
  const fwdAcc = ax * Math.sin(t.yaw) + az * Math.cos(t.yaw), latAcc = ax * Math.cos(t.yaw) - az * Math.sin(t.yaw);
  t.pitch += (clamp(fwdAcc * 0.2, -0.35, 0.35) - t.pitch) * Math.min(1, dt * 5);
  t.roll += (-clamp(latAcc * 0.22, -0.4, 0.4) - t.roll) * Math.min(1, dt * 5);
  const shake = hpf < 0.4 ? Math.sin(t.age * 37) * 0.05 * (1 - hpf) : 0;
  m.group.position.set(t.x, t.y, t.z);
  m.group.rotation.y = t.yaw;
  m.body.rotation.x = t.pitch + shake; m.body.rotation.z = t.roll;
  m.rotor.rotation.y += 36 * dt * (0.5 + 0.5 * hpf);
  m.tailRotor.rotation.x += 70 * dt;
  m.strobe.visible = (t.age % 1.2) < 0.08;
  // the chin gun follows the headset
  const dyg = (head.y - 0.1) - (t.y - 0.03 * t.size), d2 = Math.max(0.2, Math.hypot(head.x - t.x, head.z - t.z));
  m.chin.rotation.y = clamp(rel, -1.1, 1.1);
  m.chin.rotation.x = clamp(-Math.atan2(dyg, d2), -0.4, 1.2);

  // fire: a burst of tracers, and on the gunship now and then a rocket
  t.reload -= dt;
  if (playing && t.sees && hd < 7 && t.reload <= 0 && t.burstLeft <= 0 && t.stun <= 0 && Math.abs(rel) < 0.25) {
    t.burstLeft = t.rounds; t.burstT = 0;
    t.reload = rand(2.6, 4.2) * (t.htier ? 0.8 : 1) + t.rounds * 0.11;
  }
  if (t.burstLeft > 0) {
    if (!t.sees) t.burstLeft = 0;
    else { t.burstT -= dt; if (t.burstT <= 0) { fireHeliShot(t); t.burstLeft--; t.burstT = 0.11; } }
  }
  if (t.htier && playing && t.sees && hd > 1.8) { t.missileT -= dt; if (t.missileT <= 0) { t.missileT = rand(5, 8); fireHeliMissile(t); } }

  // rotor wash kicks up dust when it is low; a damaged one smokes and then burns
  if (t.y < 1.5 && Math.random() < (1.5 - t.y) * 40 * dt) {
    const a = rand(0, TAU), sp2 = rand(0.5, 1.0);
    fx.smoke.add(t.x + Math.cos(a) * 0.12, 0.02, t.z + Math.sin(a) * 0.12, Math.cos(a) * sp2, 0.03, Math.sin(a) * sp2, 0.9, 0.05, 0.22, [0.5, 0.46, 0.4], [0.42, 0.38, 0.34], 0.22, 1.2, 0);
  }
  if (hpf < 0.5 && Math.random() < dt * 25)
    fx.smoke.add(t.x - Math.sin(t.yaw) * 0.06 * t.size, t.y + 0.04 * t.size, t.z - Math.cos(t.yaw) * 0.06 * t.size, rand(-0.03, 0.03), 0.08, rand(-0.03, 0.03), 1.3, 0.04 * t.size, 0.2 * t.size, [0.22, 0.22, 0.22], [0.07, 0.07, 0.07], 0.6, 0.4, 0);
  if (hpf < 0.25 && Math.random() < dt * 14)
    fx.fire.add(t.x - Math.sin(t.yaw) * 0.06 * t.size, t.y + 0.03 * t.size, t.z - Math.cos(t.yaw) * 0.06 * t.size, 0, 0.1, 0, 0.3, 0.05 * t.size, 0.015, [1, 0.7, 0.25], [0.7, 0.1, 0], 0.9, 0.5, 0);

  t.shadow.position.set(t.x, 0.002, t.z);
  t.shadow.rotation.y = t.yaw;
  t.shadow.scale.set(0.5 * t.size * (1 + t.y * 0.2), 1, 0.7 * t.size * (1 + t.y * 0.2));
  t.eng.set(tmpV.set(t.x, t.y, t.z), clamp(Math.hypot(t.vx, t.vz) / 1.2, 0, 1));
}

function fireHeliShot(t) {
  const m = t.model;
  m.group.updateMatrixWorld(true);
  m.muzzle.getWorldPosition(mp);
  const aim = tmpV.set(head.x, head.y - 0.08, head.z).sub(mp).normalize();
  const sc = 0.035 + Math.hypot(t.vx, t.vz) * 0.02;                          // moving makes it less accurate
  aim.x += rand(-1, 1) * sc; aim.y += rand(-1, 1) * sc; aim.z += rand(-1, 1) * sc; aim.normalize();
  const core = new THREE.Mesh(tracerGeo, t.htier ? shellMatRed : tracerMat);
  core.position.copy(mp); scene.add(core);
  shells.push({ obj: core, pos: mp.clone(), vel: aim.clone().multiplyScalar(t.shotSpeed), age: 0, dmg: t.dmg, tier: t.htier ? 1 : 0, small: true });
  fx.fire.add(mp.x, mp.y, mp.z, 0, 0, 0, 0.05, 0.05, 0.1, [1, 0.95, 0.7], [1, 0.5, 0.1], 0.9);
  audio.mg(mp.clone());
}

function fireHeliMissile(t) {
  const m = t.model;
  m.group.updateMatrixWorld(true);
  const side = Math.random() < 0.5 ? -1 : 1;
  mp.set(side * 0.12, -0.008, 0.06); m.body.localToWorld(mp);
  const aim = tmpV.set(head.x, head.y - 0.1, head.z).sub(mp).normalize();
  aim.x += rand(-1, 1) * 0.03; aim.y += rand(-1, 1) * 0.03; aim.z += rand(-1, 1) * 0.03; aim.normalize();
  const core = new THREE.Mesh(shellGeo, shellMatRed);
  core.scale.setScalar(1.5); core.position.copy(mp); scene.add(core);
  shells.push({ obj: core, pos: mp.clone(), vel: aim.clone().multiplyScalar(3.4), age: 0, dmg: 20, tier: 1, small: false, rocket: true });
  audio.launch(mp.clone());
}

function killHeli(t) {
  t.dead = true; t.landed = false;
  G.score += t.scoreVal; G.kills++;
  const m = t.model, p = new V(t.x, t.y, t.z);
  fx.explosion(p, 0.9 * t.size);
  audio.explosion(p.clone(), 0.9);
  t.eng.stop();
  m.group.updateMatrixWorld(true);
  fx.flingObject(m.rotor, new V(rand(-1, 1), rand(1.5, 2.5), rand(-1, 1)));                // the rotors fly off
  fx.flingObject(m.tailRotor, new V(rand(-1, 1), rand(1, 2), rand(-1, 1)));
  m.disc.visible = false;
  for (let i = 0; i < 8; i++) fx.debrisBox(p.clone(), new V(rand(-2, 2), rand(0.5, 2.5), rand(-2, 2)), rand(0.012, 0.03), i < 3);
  t.spin = (Math.random() < 0.5 ? -1 : 1) * rand(4, 8);
  t.vx *= 0.6; t.vz *= 0.6; t.vy = Math.max(t.vy, 0.3);
}

/** A shot-down helicopter spins, burns and falls until it hits the floor or the furniture. */
function updateCrash(t, dt) {
  const m = t.model, a = tankAssets();
  t.vy -= 7 * dt;
  t.x += t.vx * dt; t.y += t.vy * dt; t.z += t.vz * dt;
  t.yaw += t.spin * dt; t.roll += dt * 1.6; t.pitch += dt * 0.6;
  m.group.position.set(t.x, t.y, t.z); m.group.rotation.y = t.yaw;
  m.body.rotation.z = t.roll; m.body.rotation.x = t.pitch;
  t.shadow.position.set(t.x, 0.002, t.z);
  fx.fire.add(t.x, t.y, t.z, rand(-0.1, 0.1), rand(0, 0.2), rand(-0.1, 0.1), 0.3, 0.08 * t.size, 0.02, [1, 0.7, 0.25], [0.7, 0.1, 0], 0.9, 0.5, 0);
  fx.smoke.add(t.x, t.y + 0.03, t.z, rand(-0.05, 0.05), 0.1, rand(-0.05, 0.05), 1.4, 0.06 * t.size, 0.25 * t.size, [0.25, 0.25, 0.25], [0.08, 0.08, 0.08], 0.6, 0.4, 0);
  if (t.y <= 0.07 * t.size || solidAt(t.x, t.y, t.z)) {
    const p = new V(t.x, Math.max(t.y, 0.1), t.z);
    fx.explosion(p, 1.5 * t.size);
    audio.explosion(p.clone(), 1.3);
    scorch.add(t.x, t.z, 0.9 * t.size);
    for (let i = 0; i < 12; i++) fx.debrisBox(p.clone(), new V(rand(-2.2, 2.2), rand(1, 3), rand(-2.2, 2.2)), rand(0.012, 0.03), i < 3);
    m.group.traverse(o => { if (o.isMesh && !o.isInstancedMesh) o.material = a.char; });
    t.landed = true; t.wreckT = 6;
    t.y = 0.05 * t.size; m.group.position.y = t.y;
    m.body.rotation.set(rand(-0.3, 0.3), 0, rand(-0.5, 0.5));
    t.shadow.visible = false;
  }
}

/* ------------------------------------------------------- machine gun */

/* With the rockets gone the launchers fall back to bullets: hold the trigger and they fire on
 * their own, fast and cheap — a tank soaks about nine of these where one rocket would do. */
const MG = { rate: 0.15, speed: 16, spread: 0.03, dmg: 12 };
const bullets = [];
const bulletGeo = new THREE.SphereGeometry(0.0065, 8, 6);
const bulletMat = new THREE.MeshBasicMaterial({ color: 0xfff2b0, toneMapped: false });

function fireBullet(h) {
  h.ctrl.updateMatrixWorld(true);
  h.L.muzzle.getWorldPosition(mp);
  md.set(0, 0, -1).transformDirection(h.ctrl.matrixWorld);
  md.x += rand(-1, 1) * MG.spread; md.y += rand(-1, 1) * MG.spread; md.z += rand(-1, 1) * MG.spread; md.normalize();
  const obj = new THREE.Mesh(bulletGeo, bulletMat);
  obj.position.copy(mp); scene.add(obj);
  bullets.push({ obj, pos: mp.clone(), vel: md.clone().multiplyScalar(MG.speed), age: 0 });
  fx.fire.add(mp.x + md.x * 0.03, mp.y + md.y * 0.03, mp.z + md.z * 0.03, 0, 0, 0, 0.05, 0.05, 0.1, [1, 0.95, 0.7], [1, 0.5, 0.1], 0.9);
  audio.mg(mp.clone());
  haptic(h, 0.25, 25);
}

function updateBullets(dt) {
  for (let i = bullets.length - 1; i >= 0; i--) {
    const b = bullets[i];
    b.age += dt;
    const step = MG.speed * dt, n = Math.max(1, Math.ceil(step / 0.05));
    let end = false;
    for (let k = 0; k < n && !end; k++) {
      b.pos.addScaledVector(b.vel, dt / n);
      if (b.pos.y <= 0.005 || solidAt(b.pos.x, b.pos.y, b.pos.z)) { end = true; fx.spark(b.pos); break; }
      for (const t of tanks) {
        if (t.dead) continue;
        if (insideTank(t, b.pos)) { damageTank(t, MG.dmg, b.pos, 0, true); fx.spark(b.pos, 1.3); end = true; break; }
      }
    }
    if (end || b.age > 1.4) { scene.remove(b.obj); bullets.splice(i, 1); continue; }
    b.obj.position.copy(b.pos);
    fx.fire.add(b.pos.x, b.pos.y, b.pos.z, 0, 0, 0, 0.05, 0.045, 0.015, [1, 0.9, 0.5], [1, 0.5, 0.1], 0.85);
    fx.fire.add(b.pos.x - b.vel.x * 0.012, b.pos.y - b.vel.y * 0.012, b.pos.z - b.vel.z * 0.012, 0, 0, 0, 0.07, 0.03, 0.01, [1, 0.7, 0.3], [0.8, 0.2, 0], 0.6);
  }
}

/* ------------------------------------------------------------ ammo on the floor */

const pickups = [];
const PK = {
  std: { box: 0x56603a, band: 0xe8c040, ring: 0xffe27a, label: '+3 ROCKETS' },
  heavy: { box: 0x8a2a1c, band: 0xffa020, ring: 0xff6a2a, label: '+2 HEAVY ROCKETS' },
};
const pkBox = new THREE.BoxGeometry(0.26, 0.12, 0.15), pkBand = new THREE.BoxGeometry(0.04, 0.125, 0.155);
const pkRing = new THREE.RingGeometry(0.2, 0.25, 40).rotateX(-Math.PI / 2);
for (const k in PK) {
  PK[k].boxMat = new THREE.MeshStandardMaterial({ color: PK[k].box, roughness: 0.7, metalness: 0.25 });
  PK[k].bandMat = new THREE.MeshStandardMaterial({ color: PK[k].band, roughness: 0.5, metalness: 0.2 });
}

function pickPickupSpot() {
  for (let i = 0; i < 40; i++) {
    const a = rand(0, TAU), r = rand(0.9, 3.2);
    const x = head.x + Math.sin(a) * r, z = head.z + Math.cos(a) * r;
    if (navBlockedAt(x, z) || fieldAt(playerField, x, z) >= FAR) continue;
    if (tanks.some(t => !t.dead && Math.hypot(t.x - x, t.z - z) < 0.8)) continue;
    if (pickups.some(p => Math.hypot(p.x - x, p.z - z) < 0.9)) continue;
    return { x, z };
  }
  return null;
}

function spawnPickup(kind) {
  const spot = pickPickupSpot();
  if (!spot) return false;
  const P = PK[kind], group = new THREE.Group();
  const box = new THREE.Mesh(pkBox, P.boxMat); box.position.y = 0.06; group.add(box);
  for (const dz of [-0.05, 0.05]) { const b = new THREE.Mesh(pkBand, P.bandMat); b.position.set(dz * 2, 0.06, 0); group.add(b); }
  const icon = buildRocket(kind === 'heavy'); icon.scale.setScalar(0.5); icon.position.y = 0.3;
  const iconPivot = new THREE.Group(); iconPivot.add(icon); iconPivot.position.y = 0.3; group.add(iconPivot);
  const ring = new THREE.Mesh(pkRing, new THREE.MeshBasicMaterial({ color: P.ring, transparent: true, opacity: 0.8, depthWrite: false, blending: THREE.AdditiveBlending }));
  ring.position.y = 0.006; group.add(ring);
  group.position.set(spot.x, 0, spot.z); group.rotation.y = rand(0, 6.28);
  scene.add(group);
  pickups.push({ kind, group, iconPivot, ring, x: spot.x, z: spot.z, age: 0 });
  fx.spark(new V(spot.x, 0.15, spot.z), 1.5);
  return true;
}

function toast(text) { G.toast = text; G.toastT = 1.8; }

function updatePickups(dt) {
  for (let i = pickups.length - 1; i >= 0; i--) {
    const p = pickups[i];
    p.age += dt;
    p.iconPivot.rotation.y += dt * 2.2;
    p.iconPivot.position.y = 0.3 + 0.025 * Math.sin(p.age * 3);
    p.ring.scale.setScalar(1 + 0.12 * Math.sin(p.age * 4));
    p.ring.material.opacity = 0.55 + 0.3 * Math.sin(p.age * 4);
    p.group.visible = p.age < 28 || Math.floor(p.age * 6) % 2 === 0;          // blinks for its last few seconds
    let got = false;
    if (G.phase === 'play') {
      if (Math.hypot(head.x - p.x, head.z - p.z) < 0.55) got = true;          // walk over it
      else for (const h of hands) if (h.ctrl && h.ctrl.getWorldPosition(tmpV2).distanceTo(tmpV.set(p.x, 0.12, p.z)) < 0.32) got = true;   // or reach down to it
    }
    if (got) {
      if (p.kind === 'heavy') G.ammo.heavy = Math.min(6, G.ammo.heavy + 2); else G.ammo.std = Math.min(12, G.ammo.std + 3);
      toast(PK[p.kind].label);
      audio.pickup(p.kind === 'heavy');
      fx.spark(new V(p.x, 0.2, p.z), 1.6);
      for (const h of hands) haptic(h, 0.5, 70);
    }
    if (got || p.age > 34) { scene.remove(p.group); pickups.splice(i, 1); }
  }
}

/* ============================================================ hands/HUD */

const hands = [];
const beamMat = new THREE.MeshBasicMaterial({ color: 0xff3322, transparent: true, opacity: 0.55, depthWrite: false, blending: THREE.AdditiveBlending });
const beamGeo = (() => { const g = new THREE.CylinderGeometry(0.0012, 0.0012, 1, 5, 1, true); g.rotateX(Math.PI / 2); g.translate(0, 0, -0.5); return g; })();
const dotMat = new THREE.MeshBasicMaterial({ color: 0xff5544, transparent: true, opacity: 0.9 });
const ringGeo = new THREE.RingGeometry(0.13, 0.15, 36);

function makeHand(i) {
  const L = buildLauncher();
  L.group.scale.setScalar(0.5);                      // half size: you hold it, you do not carry it
  const beam = new THREE.Mesh(beamGeo, beamMat);
  beam.position.copy(L.muzzle.position);
  L.group.add(beam);
  const dot = new THREE.Mesh(new THREE.SphereGeometry(0.01, 8, 6), dotMat);
  scene.add(dot);
  const ring = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({ color: 0xff4a33, transparent: true, opacity: 0.95, side: THREE.DoubleSide, depthWrite: false, blending: THREE.AdditiveBlending }));
  ring.visible = false; scene.add(ring);
  return { i, L, ctrl: null, cool: 0, mg: 0, trig: false, lock: null, beam, dot, ring, gamepad: null, lastHud: '' };
}
hands.push(makeHand(0), makeHand(1));

function haptic(h, strength, ms) {
  try { h.gamepad?.hapticActuators?.[0]?.pulse(strength, ms); } catch (_) { /* no haptics */ }
}

function drawHud(h) {
  const have = G.ammo.std + G.ammo.heavy;
  const key = `${G.hp}|${G.score}|${G.wave}|${h.cool > 0}|${G.phase}|${G.ammo.std}|${G.ammo.heavy}|${G.toast}`;
  if (key === h.lastHud) return;
  h.lastHud = key;
  const { ctx, w, hgt = 170 } = h.L.panel;
  ctx.clearRect(0, 0, 300, 170);
  ctx.fillStyle = 'rgba(4,8,12,0.78)'; ctx.fillRect(0, 0, 300, 170);
  ctx.strokeStyle = '#ffb454'; ctx.lineWidth = 4; ctx.strokeRect(2, 2, 296, 166);
  const f = G.hp / 100;
  ctx.fillStyle = '#1b2430'; ctx.fillRect(16, 18, 268, 38);
  ctx.fillStyle = f > 0.5 ? '#46d06a' : f > 0.25 ? '#e8b030' : '#e23b2c';
  ctx.fillRect(16, 18, 268 * f, 38);
  ctx.fillStyle = '#fff'; ctx.font = 'bold 34px Arial'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(`HEALTH ${Math.ceil(G.hp)}`, 150, 38);
  ctx.fillStyle = '#ffb454'; ctx.font = 'bold 30px Arial'; ctx.textAlign = 'left';
  ctx.fillText(`WAVE ${G.wave}`, 16, 92);
  ctx.textAlign = 'right'; ctx.fillText(`${G.score}`, 284, 92);
  ctx.textAlign = 'center'; ctx.font = 'bold 26px Arial';
  if (G.toast) { ctx.fillStyle = '#ffe27a'; ctx.font = 'bold 28px Arial'; ctx.fillText(G.toast, 150, 140); }
  else if (have === 0) { ctx.fillStyle = '#ffcc33'; ctx.fillText('MACHINE GUN  \u221e', 150, 140); }
  else {
    ctx.fillStyle = h.cool > 0 ? '#ff6a55' : G.ammo.heavy > 0 ? '#ff9a4a' : '#6dff8c';
    ctx.fillText(`ROCKETS ${G.ammo.std}${G.ammo.heavy ? `  \u00b7  HVY ${G.ammo.heavy}` : ''}`, 150, 140);
  }
  h.L.panel.refresh();
}

/* the full-screen red flash when you are hit, glued to the head */
const vignette = (() => {
  const tex = canvasTex(128, 128, (c, w, h) => {
    const g = c.createRadialGradient(w / 2, h / 2, w * 0.18, w / 2, h / 2, w * 0.72);
    g.addColorStop(0, 'rgba(200,0,0,0)'); g.addColorStop(1, 'rgba(200,0,0,0.95)');
    c.fillStyle = g; c.fillRect(0, 0, w, h);
  });
  const m = new THREE.Mesh(new THREE.PlaneGeometry(1.5, 1.5), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false, depthWrite: false, opacity: 0, toneMapped: false }));
  m.position.set(0, 0, -0.4); m.renderOrder = 999; m.frustumCulled = false;
  camera.add(m);
  return m;
})();

/* the hit fade: red, then black, then back — all over about two seconds, glued to the head */
const fadeQuad = (() => {
  const m = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, depthTest: false, depthWrite: false, opacity: 0, toneMapped: false }));
  m.position.set(0, 0, -0.25); m.renderOrder = 400; m.frustumCulled = false; m.visible = false;
  camera.add(m);
  return m;
})();
function updateFade(dt) {
  const f = G.fade;
  if (f.t < 0) { fadeQuad.visible = false; return; }
  f.t += dt;
  const t = f.t;
  let a, r;
  if (t < 0.2) { a = 0.85 * (t / 0.2); r = 0.78; }                         // flash to red
  else if (t < 0.9) { const k = (t - 0.2) / 0.7; a = 0.85 + 0.15 * k; r = 0.78 * (1 - k); }   // red goes to black
  else if (f.hold) { a = 0.92; r = 0; }                                    // dead: stay dark behind the score board
  else if (t < 1.3) { a = 1; r = 0; }                                      // black
  else if (t < 2.0) { a = 1 - (t - 1.3) / 0.7; r = 0; }                    // and back
  else { f.t = -1; fadeQuad.visible = false; return; }
  fadeQuad.material.color.setRGB(r, 0, 0);
  fadeQuad.material.opacity = a;
  fadeQuad.visible = true;
}

/* floating message board (menu, wave banners, game over) */
const msg = makePanel(1.2, 0.6, 1024, 512, { depthTest: false });
msg.mesh.renderOrder = 500; msg.mesh.visible = false; msg.mesh.frustumCulled = false;
scene.add(msg.mesh);
let msgTimer = 0;

function showMsg(lines, secs = 0, scale = 1) {
  const { ctx } = msg;
  ctx.clearRect(0, 0, 1024, 512);
  ctx.fillStyle = 'rgba(5,9,14,0.86)';
  ctx.beginPath(); ctx.roundRect(8, 8, 1008, 496, 28); ctx.fill();
  ctx.strokeStyle = '#ffb454'; ctx.lineWidth = 6; ctx.stroke();
  let y = 30;
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  for (const l of lines) {
    ctx.fillStyle = l.c || '#e6edf5';
    ctx.font = `${l.b === false ? '' : 'bold '}${l.s || 40}px Arial, sans-serif`;
    ctx.fillText(l.t, 512, y);
    y += (l.s || 40) * 1.32 + (l.gap || 0);
  }
  msg.refresh();
  msg.mesh.scale.setScalar(scale);
  placeMsg();
  msg.mesh.visible = true;
  msgTimer = secs;
}
function placeMsg() {
  tmpV.set(0, 0, -1).applyQuaternion(headQ); tmpV.y = 0;
  if (tmpV.lengthSq() < 1e-4) tmpV.set(0, 0, -1);
  tmpV.normalize();
  msg.mesh.position.set(head.x + tmpV.x * 1.5, Math.max(1.0, head.y - 0.1), head.z + tmpV.z * 1.5);
  msg.mesh.rotation.set(0, Math.atan2(head.x - msg.mesh.position.x, head.z - msg.mesh.position.z), 0);
}
const hideMsg = () => { msg.mesh.visible = false; };

function scanLine() {
  if (G.mode === 'flat') return { t: 'Flat preview: virtual barriers stand in for your furniture', c: '#9fb3c8' };
  const n = scan.sources.size;
  if (n > 0) return { t: `Room scan found (${n} surface ${n === 1 ? 'mesh' : 'meshes'}) — hide behind real furniture!`, c: '#6dff8c' };
  return { t: 'No room scan found yet — virtual barriers will be used', c: '#ffb454' };
}

function showMenu() {
  G.phase = 'menu';
  G.inputLock = 0.8;
  showMsg([
    { t: 'TANK AMBUSH', s: 84, c: '#ffb454' },
    { t: 'Tanks are rolling into your room.', s: 36, b: false },
    { t: 'They hide behind your furniture and aim at your head.', s: 32, b: false },
    { t: 'Dodge the shells, duck behind something solid, and grab ammo crates.', s: 30, b: false },
    G.mode === 'flat'
      ? { t: 'Click: rocket (hold when empty: machine gun) · WASD · C crouch · G mesh', s: 26, c: '#9fb3c8', b: false, gap: 8 }
      : { t: 'Trigger: rocket (hold when empty: machine gun) · Grip: room mesh', s: 26, c: '#9fb3c8', b: false, gap: 8 },
    scanLine(),
    { t: G.mode === 'flat' ? 'CLICK TO START' : 'PULL A TRIGGER TO START', s: 46, c: '#6dff8c', gap: 6 },
  ].map(l => ({ s: 30, ...l })));
}

/* ================================================================= flow */

function resetEntities() {
  for (const t of tanks) { scene.remove(t.model.group); if (t.shadow) scene.remove(t.shadow); t.eng.stop(); }
  tanks.length = 0;
  for (const r of rockets) scene.remove(r.obj); rockets.length = 0;
  for (const s of shells) scene.remove(s.obj); shells.length = 0;
  for (const b of bullets) scene.remove(b.obj); bullets.length = 0;
  for (const p of pickups) scene.remove(p.group); pickups.length = 0;
  fx.clear(); scorch.clear();
}

function startGame() {
  resetEntities();
  hideMsg();
  G.hp = 100; G.score = 0; G.kills = 0; G.wave = 0; G.waveClear = false;
  G.ammo = { std: 6, heavy: 0 }; G.toast = ''; G.invuln = 0; G.fade = { t: -1, hold: false }; G.pickT = 9;
  const wantVirtual = params.has('cover') || scan.sources.size === 0;
  if (wantVirtual && !virt.made) addVirtualCover();
  else if (!wantVirtual && virt.made) { removeVirtualCover(); rebuildNow([...scan.sources.values()]); }
  G.haveRoom = !wantVirtual || scan.sources.size > 0;
  updatePlayerField(0, true);
  G.phase = 'play';
  for (const h of hands) { h.cool = 0; h.L.rocket.visible = true; h.L.ledMat.color.setHex(0x44ff66); }
  startWave(1);
}

/** Who is in this wave: green tanks, plus black ones from wave 6 and the biggest from wave 10. */
function buildQueue(n) {
  const total = Math.min(2 + n, 9);
  const t2 = n >= 10 ? Math.min(3, 1 + Math.floor((n - 10) / 2)) : 0;
  const t1 = n >= 10 ? Math.min(2, 1 + Math.floor((n - 10) / 3)) : n >= 6 ? Math.min(3, 1 + Math.floor((n - 6) / 2)) : 0;
  const h4 = n >= 12 ? Math.min(2, 1 + Math.floor((n - 12) / 3)) : 0;      // black gunships (value 4)
  const h3 = n >= 12 ? 1 : n >= 10 ? 2 : n >= 8 ? 1 : 0;                    // attack helicopters (value 3)
  const q = [];
  for (let i = 0; i < h4; i++) q.push(4);
  for (let i = 0; i < h3; i++) q.push(3);
  for (let i = 0; i < t2; i++) q.push(2);
  for (let i = 0; i < t1; i++) q.push(1);
  while (q.length < Math.max(total, t1 + t2 + h3 + h4 + 2)) q.push(0);
  for (let i = q.length - 1; i > 0; i--) { const j = (Math.random() * (i + 1)) | 0; [q[i], q[j]] = [q[j], q[i]]; }
  const first = q.findIndex(v => v === 0);                                  // never open a wave with the big ones
  if (first > 0) [q[0], q[first]] = [q[first], q[0]];
  return q;
}

function startWave(n) {
  G.wave = n;
  G.queue = buildQueue(n);
  G.toSpawn = G.queue.length;
  G.maxAlive = Math.min(1 + Math.ceil(n / 2), 5);
  G.spawnT = 1.8;
  G.waveClear = false;
  audio.alert('wave');
  const sub2 = n === 1 ? { t: 'Tanks inbound', c: '#e6edf5' } : n === 6 ? { t: 'HEAVY ARMOUR INBOUND', c: '#ff5a44' } : n === 8 ? { t: 'ATTACK HELICOPTER INBOUND', c: '#ff5a44' } : n === 10 ? { t: 'SUPER-HEAVY TANKS', c: '#ff5a44' } : n === 12 ? { t: 'BLACK GUNSHIP INBOUND', c: '#ff5a44' }
    : n === 5 ? { t: 'Heavy rocket crates now drop', c: '#ffa020' } : { t: 'They are getting smarter', c: '#e6edf5' };
  showMsg([{ t: `WAVE ${n}`, s: 120, c: '#ffb454' }, { t: sub2.t, s: 40, c: sub2.c, b: n === 6 || n === 8 || n === 10 || n === 12 }], 2.6, 0.7);
  if (n > 1) { spawnPickup('std'); if (n >= 5 && !pickups.some(p => p.kind === 'heavy')) spawnPickup('heavy'); }
}

function gameOver() {
  if (G.phase !== 'play') return;
  G.phase = 'over';
  G.inputLock = 1.8;
  audio.alert('over');
  for (const r of rockets) scene.remove(r.obj); rockets.length = 0;
  for (const b of bullets) scene.remove(b.obj); bullets.length = 0;
  setTimeout(() => {
    if (G.phase !== 'over') return;
    let best = 0;
    try { best = +localStorage.getItem('tankAmbushBest') || 0; if (G.score > best) localStorage.setItem('tankAmbushBest', String(G.score)); } catch (_) { /* no storage */ }
    const record = G.score > best;
    showMsg([
      { t: 'WELL DONE!', s: 96, c: '#ffd45a' },
      { t: 'Game over', s: 38, c: '#ff7a66' },
      { t: `SCORE ${G.score}`, s: 70, c: '#ffffff' },
      { t: `Wave ${G.wave}  ·  ${G.kills} tank${G.kills === 1 ? '' : 's'} destroyed`, s: 34 },
      record ? { t: 'NEW BEST!', s: 34, c: '#ffd45a' } : { t: `Best: ${best}`, s: 32, c: '#9fb3c8', b: false },
      { t: G.mode === 'flat' ? 'CLICK TO PLAY AGAIN' : 'PULL A TRIGGER TO PLAY AGAIN', s: 40, c: '#6dff8c', gap: 10 },
    ]);
  }, 700);
}

function updateGame(dt) {
  if (G.phase !== 'play') return;
  // ammo crates keep turning up; sooner when you are dry and nothing is lying about
  G.pickT -= dt;
  if (G.ammo.std + G.ammo.heavy === 0 && pickups.length === 0) G.pickT = Math.min(G.pickT, 2.5);
  if (G.pickT <= 0) {
    G.pickT = rand(11, 16);
    const hv = pickups.some(p => p.kind === 'heavy');
    if (G.wave >= 5 && !hv && Math.random() < 0.45) spawnPickup('heavy');
    else if (pickups.length < 3) spawnPickup('std');
  }
  const alive = tanks.reduce((n, t) => n + (t.dead ? 0 : 1), 0);
  if (!G.waveClear) {
    if (G.toSpawn > 0) {
      if (alive < G.maxAlive) {
        G.spawnT -= dt;
        if (G.spawnT <= 0) {
          const next = G.queue[0] || 0;
          const p = next >= 3 ? pickHeliSpawn() : pickSpawn();
          if (p) {
            G.queue.shift();
            if (next >= 3) spawnHeli(p.x, p.y, p.z, next - 3); else spawnTank(p.x, p.z, undefined, next);
            G.toSpawn--; G.spawnT = rand(2, 4);
          } else G.spawnT = 1;
        }
      }
    } else if (alive === 0) {
      G.waveClear = true; G.nextWaveT = 4;
      G.hp = Math.min(100, G.hp + 40);
      audio.alert('wave');
      showMsg([{ t: 'WAVE CLEARED', s: 90, c: '#6dff8c' }, { t: '+40 health', s: 40, b: false }], 2.6, 0.7);
    }
  } else {
    G.nextWaveT -= dt;
    if (G.nextWaveT <= 0) startWave(G.wave + 1);
  }
}

/* ============================================================ input/XR */

function onTrigger(i) {
  audio.init();
  if (G.phase === 'menu' || G.phase === 'over') { if (G.inputLock <= 0) startGame(); return; }
  launch(hands[i]);
}
function onSqueeze() {
  G.showMesh = !G.showMesh;
  applyMeshVisibility();
}

function attachXRControllers() {
  hands.forEach((h, i) => {
    const c = renderer.xr.getController(i);
    c.add(h.L.group);
    scene.add(c);
    h.ctrl = c;
    c.addEventListener('connected', e => { h.gamepad = e.data?.gamepad || null; });
    c.addEventListener('selectstart', () => { h.trig = true; onTrigger(i); });
    c.addEventListener('selectend', () => { h.trig = false; });
    c.addEventListener('squeezestart', onSqueeze);
  });
}

async function enterXR() {
  const status = $('status');
  try {
    audio.init();
    const session = await navigator.xr.requestSession('immersive-ar', {
      requiredFeatures: ['local-floor'],
      optionalFeatures: ['mesh-detection', 'plane-detection', 'bounded-floor'],
    });
    session.addEventListener('end', () => location.reload());
    await renderer.xr.setSession(session);
    G.mode = 'xr';
    renderer.domElement.style.display = 'block';
    document.body.classList.add('playing');
    attachXRControllers();
    G.menuPending = true;
  } catch (e) {
    status.textContent = 'Could not start the headset session: ' + (e.message || e);
    console.error(e);
  }
}

/* flat preview: WASD + mouse look + click */
const keys = {};
const flatHold = { down: false, t: 0, dragged: 0 };      // mouse held without dragging = trigger held
let yaw = 0, pitch = 0;
function enterFlat() {
  audio.init();
  G.mode = 'flat';
  renderer.domElement.style.display = 'block';
  document.body.classList.add('playing', 'flat');
  scene.background = new THREE.Color(0x141a22);
  renderer.setClearColor(0x141a22, 1);
  const grid = new THREE.GridHelper(12, 24, 0x3b4b60, 0x232d3a);
  grid.position.y = 0.001; scene.add(grid);
  hands.forEach((h, i) => {
    const c = new THREE.Group();
    c.position.set(i ? 0.28 : -0.28, -0.24, -0.42);
    c.rotation.set(0.03, i ? 0.09 : -0.09, 0);
    c.add(h.L.group);
    camera.add(c);
    h.ctrl = c;
  });
  addVirtualCover();
  let flip = 0, down = null, dragged = 0;
  const dom = renderer.domElement;
  dom.addEventListener('pointerdown', e => { down = { x: e.clientX, y: e.clientY }; dragged = 0; flatHold.down = true; flatHold.t = 0; flatHold.dragged = 0; });
  dom.addEventListener('pointerup', () => { down = null; flatHold.down = false; });
  dom.addEventListener('click', () => {
    if (dragged > 6) return;                                   // that was a look-around drag, not a shot
    if (document.pointerLockElement !== dom) dom.requestPointerLock?.()?.catch?.(() => {});
    onTrigger(flip ^= 1);
  });
  // mouse look: captured pointer, or simply hold the button and drag
  document.addEventListener('mousemove', e => {
    const locked = document.pointerLockElement === dom;
    if (!locked && !(down && (e.buttons & 1))) return;
    if (!locked) { dragged += Math.abs(e.movementX) + Math.abs(e.movementY); flatHold.dragged = dragged; }
    yaw -= e.movementX * 0.0022; pitch = clamp(pitch - e.movementY * 0.0022, -1.3, 1.3);
  });
  window.addEventListener('keydown', e => { keys[e.code] = true; if (e.code === 'KeyG') onSqueeze(); });
  window.addEventListener('keyup', e => { keys[e.code] = false; });
  const releaseKeys = () => { for (const k in keys) keys[k] = false; };
  window.addEventListener('blur', releaseKeys);
  document.addEventListener('visibilitychange', releaseKeys);
  G.menuPending = true;
}

function flatMove(dt) {
  const sp = 1.7 * dt;
  const f = (keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0), s = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0);
  const eye = flatMove.hold ?? ((keys.KeyC || keys.ControlLeft) ? 1.0 : 1.6);                 // C or Ctrl: crouch
  camera.position.y += (eye - camera.position.y) * Math.min(1, dt * 9);
  camera.position.x += (-Math.sin(yaw) * f + Math.cos(yaw) * s) * sp;
  camera.position.z += (-Math.cos(yaw) * f - Math.sin(yaw) * s) * sp;
  const r = Math.hypot(camera.position.x, camera.position.z);
  if (r > 3.4) { camera.position.x *= 3.4 / r; camera.position.z *= 3.4 / r; }
  if (solidAt(camera.position.x, 1.0, camera.position.z) || solidAt(camera.position.x, 0.4, camera.position.z)) {
    camera.position.x = flatMove.lx; camera.position.z = flatMove.lz;
  }
  flatMove.lx = camera.position.x; flatMove.lz = camera.position.z;
  camera.quaternion.setFromEuler(new THREE.Euler(pitch, yaw, 0, 'YXZ'));
}
flatMove.lx = 0; flatMove.lz = 0;

/* ================================================================ loop */

let last = performance.now();
const _fwd = new V();

function loop(_, frame) {
  const now = performance.now();
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now;
  G.time += dt;
  if (G.mode === 'none') { return; }

  // --- where is the head?
  if (G.mode === 'xr') {
    const ref = renderer.xr.getReferenceSpace();
    if (frame && ref) {
      const vp = frame.getViewerPose(ref);
      if (vp) {
        const p = vp.transform.position, o = vp.transform.orientation;
        head.set(p.x, p.y, p.z); headQ.set(o.x, o.y, o.z, o.w);
      }
      scanRoom(frame, ref);
    }
  } else {
    flatMove(dt);
    head.copy(camera.position); headQ.copy(camera.quaternion);
    if (flatHold.down) flatHold.t += dt;
    const hold = flatHold.down && flatHold.t > 0.18 && flatHold.dragged <= 6;
    hands[0].trig = hands[1].trig = hold;
  }
  headFwd.set(0, 0, -1).applyQuaternion(headQ);
  headUp.set(0, 1, 0).applyQuaternion(headQ);
  audio.listener(head, headFwd, headUp);

  pumpRebuild();

  if (G.menuPending && (G.mode === 'flat' || frame)) {
    G.menuPending = false;
    if (G.mode === 'flat') rebuildNow([]);
    showMenu();
  }
  if (G.phase === 'menu') {      // keep the scan line fresh while the player looks around
    scan.menuT = (scan.menuT || 0) - dt;
    if (scan.menuT <= 0) { scan.menuT = 1.2; const n = scan.sources.size; if (n !== scan.menuN) { scan.menuN = n; showMenu(); G.inputLock = Math.max(G.inputLock, 0); } }
  }
  if (G.inputLock > 0) G.inputLock -= dt;
  if (G.invuln > 0) G.invuln -= dt;
  if (G.toastT > 0) { G.toastT -= dt; if (G.toastT <= 0) G.toast = ''; }
  if (msgTimer > 0 && G.phase === 'play') { msgTimer -= dt; if (msgTimer <= 0) hideMsg(); }

  updatePlayerField(dt, false);
  updateGame(dt);

  // --- hands
  for (const h of hands) {
    if (!h.ctrl) continue;
    h.ctrl.updateMatrixWorld(true);
    const have = G.ammo.std + G.ammo.heavy;
    if (h.cool > 0) {
      h.cool -= dt;
      if (h.cool <= 0 && have > 0) audio.reload(h.ctrl.getWorldPosition(tmpV2).clone());
    }
    // what is loaded: a standard or heavy rocket, or nothing (machine gun)
    const ready = h.cool <= 0 && have > 0;
    h.L.rocket.visible = ready && G.ammo.heavy === 0;
    h.L.rocketH.visible = ready && G.ammo.heavy > 0;
    h.L.ledMat.color.setHex(h.cool > 0 ? 0xff3322 : have === 0 ? 0xffcc33 : G.ammo.heavy > 0 ? 0xff7a1a : 0x44ff66);
    h.mg = Math.max(0, h.mg - dt);
    if (G.phase === 'play' && h.trig && have === 0 && h.mg <= 0) { h.mg = MG.rate; fireBullet(h); }
    h.L.muzzle.getWorldPosition(mp);
    md.set(0, 0, -1).transformDirection(h.ctrl.matrixWorld);
    // laser sight
    const hitD = rayHit(mp.x, mp.y, mp.z, md.x, md.y, md.z, 8);
    const gs = h.L.group.scale.x;
    h.beam.scale.set(1 / gs, 1 / gs, hitD / gs);           // the launcher is scaled, the laser should not be
    h.dot.position.copy(mp).addScaledVector(md, hitD);
    h.dot.visible = hitD < 8 && h.ctrl.visible !== false;
    // lock-on: closest tank within ~25° of where the launcher points, that it can see
    h.lock = G.phase === 'play' ? lockCandidate(mp, md, ROCKET.lockCone, 8, null) : null;
    if (h.lock) {
      h.ring.visible = true;
      h.ring.position.set(h.lock.x, cyOf(h.lock) + 0.03, h.lock.z);
      h.ring.lookAt(head);
      h.ring.scale.setScalar(1 + 0.12 * Math.sin(G.time * 10));
    } else h.ring.visible = false;
    if (Math.floor(G.time * 6) !== h.hudTick) { h.hudTick = Math.floor(G.time * 6); drawHud(h); }
  }

  // --- the fight
  for (let i = tanks.length - 1; i >= 0; i--) {
    const t = tanks[i];
    if (t.dead) {
      if (t.air && !t.landed) { updateCrash(t, dt); continue; }
      t.wreckT -= dt;
      if (t.wreckT > 0 && Math.random() < dt * 5) fx.fire.add(t.x + rand(-0.06, 0.06), 0.07, t.z + rand(-0.08, 0.08), 0, rand(0.1, 0.3), 0, rand(0.2, 0.5), 0.05, 0.015, [1, 0.6, 0.2], [0.6, 0.1, 0], 0.8, 0.5, 0);     // embers
      if (t.wreckT > 0 && Math.random() < dt * 14) fx.smoke.add(t.x, 0.09, t.z, rand(-0.05, 0.05), rand(0.15, 0.3), rand(-0.05, 0.05), rand(1.2, 2), 0.04, 0.2, [0.3, 0.3, 0.3], [0.08, 0.08, 0.08], 0.55, 0.3, 0);
      if (t.wreckT < 0.8) t.model.group.scale.setScalar(t.size * Math.max(0.001, t.wreckT / 0.8));
      if (t.wreckT <= 0) { scene.remove(t.model.group); if (t.shadow) scene.remove(t.shadow); tanks.splice(i, 1); }
      continue;
    }
    if (t.air) updateHeli(t, dt); else updateTank(t, dt);
  }
  // tanks do not park on top of each other
  for (let i = 0; i < tanks.length; i++) for (let j = i + 1; j < tanks.length; j++) {
    const a = tanks[i], b = tanks[j];
    if (a.dead || b.dead || a.air || b.air) continue;
    const dx = b.x - a.x, dz = b.z - a.z, d = Math.hypot(dx, dz);
    const min = 0.14 * (a.size + b.size);
    if (d < min && d > 1e-4) { const k = (min - d) * 0.5 / d; moveTo(a, a.x - dx * k, a.z - dz * k); moveTo(b, b.x + dx * k, b.z + dz * k); }
  }
  updateShells(dt);
  updateRockets(dt);
  updateBullets(dt);
  updatePickups(dt);
  fx.update(dt); scorch.update(dt);

  // hurt flash
  if (G.hurt > 0) G.hurt = Math.max(0, G.hurt - dt * 1.6);
  updateFade(dt);
  vignette.material.opacity = Math.min(1, G.hurt * 0.5 + (G.hp < 30 && G.phase === 'play' ? 0.18 + 0.12 * Math.sin(G.time * 6) : 0));
  vignette.visible = vignette.material.opacity > 0.01;

  renderer.render(scene, camera);
}

/* ============================================================ start UI */

(async function init() {
  const status = $('status'), btnAR = $('enterAR'), btnFlat = $('enterFlat');
  let xrOK = false;
  try { xrOK = !!navigator.xr && await navigator.xr.isSessionSupported('immersive-ar'); } catch (_) { /* no WebXR */ }
  if (xrOK) { btnAR.disabled = false; status.textContent = 'Mixed reality is available. Put the headset on, then press the button.'; }
  else status.textContent = navigator.xr
    ? 'Immersive AR is not available here. Open this page in the Meta Quest 3 browser, or try the flat preview.'
    : 'WebXR is not available in this browser. Use the Meta Quest 3 browser for the real thing — or try the flat preview.';
  btnAR.addEventListener('click', enterXR);
  btnFlat.addEventListener('click', enterFlat);
  renderer.setAnimationLoop(loop);
  if (params.has('flat')) enterFlat();
})();

// handy for poking at the game from the console
window.__tank = { spawnHeli, pickHeliSpawn, updateCrash, pickups, bullets, spawnPickup, fireBullet, hurtPlayer, buildQueue, TIER, scanRoom, pumpRebuild, killTank, blast, updatePlayerField, view(x, y, z, yw, pt) { flatMove.hold = y; camera.position.set(x, y, z); yaw = yw; pitch = pt; flatMove.lx = x; flatMove.lz = z; }, G, tanks, rockets, shells, hands, head, W, scan, spawnTank, launch, startGame, camera, scene, renderer, fx, audio, onTrigger, pickSpawn, showMenu };
