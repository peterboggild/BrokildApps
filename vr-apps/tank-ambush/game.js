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
} from './world.js';
import { createFX, createAudio } from './fx.js';
import { buildTank, buildLauncher, buildRocket, setTrack, makePanel, tankAssets, canvasTex } from './models.js';

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
};

const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setClearColor(0x000000, 0);
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
renderer.domElement.id = 'view';
document.body.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.04, 40);
camera.position.set(0, 1.6, 0);
scene.add(camera);

scene.add(new THREE.HemisphereLight(0xe6edff, 0x4a4036, 1.5));
const sun = new THREE.DirectionalLight(0xfff2de, 2.3);
sun.position.set(1.5, 3.5, 1.0);
scene.add(sun);

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight; camera.updateProjectionMatrix();
  if (G.mode !== 'xr') renderer.setSize(window.innerWidth, window.innerHeight);
});

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

function spawnTank(x, z, yaw) {
  const w = G.wave;
  const model = buildTank();
  scene.add(model.group);
  const t = {
    model, x, z, yaw: yaw ?? Math.atan2(head.x - x, head.z - z), v: 0, yawRate: 0, tyaw: 0, gp: 0,
    hp: 100, dead: false, state: 'advance', stateT: 0, hideT: 0, stun: 0, pushX: 0, pushZ: 0, spin: 0,
    reload: rand(1.5, 3.2), shots: 0, burst: w >= 3 ? 2 : 1, reloadTime: Math.max(2.0, 4.3 - 0.25 * w),
    shellSpeed: Math.min(4.2, 2.6 + 0.12 * w), aimErr: Math.max(0.012, 0.05 - 0.004 * w),
    vmax: Math.min(0.85, 0.5 + 0.025 * w), dPref: rand(1.7, 3.2),
    sees: false, seeT: rand(0, 0.12), goalT: 0, gx: x, gz: z, cf: null, spawnT: 0, wreckT: 0, kick: 0,
    eng: audio.engine(), age: 0,
  };
  model.group.scale.setScalar(0.3);
  tanks.push(t);
  fx.puff(new V(x, 0.05, z), 0.5);
  return t;
}

function moveTo(t, nx, nz) {
  if (!navBlockedAt(nx, nz)) { t.x = nx; t.z = nz; return true; }
  if (!navBlockedAt(nx, t.z)) { t.x = nx; return true; }
  if (!navBlockedAt(t.x, nz)) { t.z = nz; return true; }
  return false;
}

function drive(t, gx, gz, dt, speedMul) {
  const sf = 0.4 + 0.6 * (t.hp / 100);          // a damaged tank is slower
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
  if (t.spawnT < 1) { t.spawnT = Math.min(1, t.spawnT + dt * 2.2); t.model.group.scale.setScalar(0.3 + 0.7 * t.spawnT); }
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
        if ((t.sees && dist < 5.6 && t.reload < 0.45) || (t.sees && dist < t.dPref) || dist < 1.0) { speedMul = 0; gx = px; gz = pz; }
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
  const sf = 0.6 + 0.4 * (t.hp / 100);
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
    const tr = m.tracks[s], sp = t.v - t.yawRate * tr.x, d = sp * dt;
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
  const core = new THREE.Mesh(shellGeo, shellMat);
  core.position.copy(mp);
  scene.add(core);
  shells.push({ obj: core, pos: mp.clone(), vel: aim.clone().multiplyScalar(t.shellSpeed), age: 0 });
  fx.muzzleFlash(mp, aim);
  audio.tankShot(mp.clone());
  t.kick = 1;
}

const shellGeo = new THREE.SphereGeometry(0.017, 10, 8);
const shellMat = new THREE.MeshBasicMaterial({ color: 0xffd27a });

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
      if (boom) shellBoom(s.pos);
      continue;
    }
    s.obj.position.copy(s.pos);
    // glowing head and a little tail
    fx.fire.add(s.pos.x, s.pos.y, s.pos.z, 0, 0, 0, 0.07, 0.11, 0.08, [1, 0.8, 0.35], [1, 0.4, 0.1], 0.9);
    fx.fire.add(s.pos.x - s.vel.x * 0.02, s.pos.y - s.vel.y * 0.02, s.pos.z - s.vel.z * 0.02, rand(-0.03, 0.03), rand(-0.03, 0.03), rand(-0.03, 0.03), 0.3, 0.05, 0.015, [1, 0.6, 0.2], [0.5, 0.1, 0], 0.7);
  }
}

/** Distance from a point to the player's head or chest. */
function playerDist(p) {
  chest.set(head.x, head.y - 0.38, head.z);
  return Math.min(p.distanceTo(head), p.distanceTo(chest) * 0.85);
}

function shellBoom(p) {
  fx.explosion(p, 0.5);
  audio.explosion(p.clone(), 0.55);
  if (G.phase !== 'play') return;
  const d = playerDist(p);
  const dmg = Math.round(22 * clamp(1 - d / 0.75, 0, 1));
  if (dmg >= 2) hurtPlayer(dmg);
}

function hurtPlayer(d) {
  G.hp = Math.max(0, G.hp - d);
  G.hurt = 1;
  audio.hurt();
  for (const h of hands) haptic(h, 1, 160);
  if (G.hp <= 0) gameOver();
}

/* --------------------------------------------------------------- damage */

function insideTank(t, p) {
  const dx = p.x - t.x, dz = p.z - t.z, s = Math.sin(t.yaw), c = Math.cos(t.yaw);
  const lz = dx * s + dz * c, lx = dx * c - dz * s;
  return Math.abs(lx) < 0.13 && Math.abs(lz) < 0.19 && p.y < 0.17 && p.y > -0.02;
}

function damageTank(t, dmg, from, push) {
  if (t.dead) return;
  t.hp -= dmg;
  if (t.hp <= 0) { killTank(t); return; }
  // hit but alive: shoved sideways, shaken, and slower from now on
  const dx = t.x - from.x, dz = t.z - from.z, l = Math.hypot(dx, dz) || 1;
  t.pushX += (dx / l) * push; t.pushZ += (dz / l) * push;
  t.spin += rand(-1, 1) * push * 2.2;
  t.stun = Math.max(t.stun, 0.35 + push * 0.3);
  t.kick = 1;
  if (t.state === 'advance' && Math.random() < 0.5) pickCover(t);
}

function killTank(t) {
  t.dead = true; t.wreckT = 5;
  G.score += 100; G.kills++;
  const p = tmpV.set(t.x, 0.07, t.z);
  fx.explosion(p, 1.0);
  audio.explosion(p.clone(), 0.9);
  t.eng.stop();
  const m = t.model, a = tankAssets();
  m.group.updateMatrixWorld(true);
  fx.flingObject(m.turret, new V(rand(-1, 1), rand(2.6, 3.8), rand(-1, 1)));
  for (let i = 0; i < 10; i++) fx.debrisBox(new V(t.x, 0.08, t.z), new V(rand(-2.2, 2.2), rand(1.5, 3.8), rand(-2.2, 2.2)), rand(0.012, 0.03), i < 3);
  m.group.traverse(o => { if (o.isMesh && !o.isInstancedMesh && o !== m.shadow) o.material = a.char; });
  m.turret.traverse(o => { if (o.isMesh) o.material = a.char; });
  m.body.rotation.z = rand(-0.1, 0.1);
}

function blast(p, direct) {
  fx.explosion(p, 1.15);
  audio.explosion(p.clone(), 1.1);
  const R = 0.8;
  for (const t of tanks) {
    if (t.dead) continue;
    if (t === direct) { damageTank(t, 999, p, 0); continue; }
    const d = Math.hypot(p.x - t.x, p.y - 0.06, p.z - t.z);
    if (d > R) continue;
    const k = 1 - d / R;
    damageTank(t, Math.round(95 * k), p, 3.4 * k + 0.6);
  }
  for (const h of hands) { const wp = h.ctrl.getWorldPosition(tmpV2); if (wp.distanceTo(p) < 2.5) haptic(h, 0.5, 80); }
}

/* -------------------------------------------------------------- rockets */

function lockCandidate(origin, dir, maxAng, maxD, ignore) {
  let best = null, ba = maxAng;
  for (const t of tanks) {
    if (t.dead || t === ignore) continue;
    tmpV.set(t.x - origin.x, 0.07 - origin.y, t.z - origin.z);
    const d = tmpV.length();
    if (d > maxD || d < 0.2) continue;
    const ang = Math.acos(clamp(tmpV.dot(dir) / d, -1, 1));
    if (ang >= ba) continue;
    if (!los(origin.x, origin.y, origin.z, t.x, 0.08, t.z, 0.08, 0.1)) continue;
    ba = ang; best = t;
  }
  return best;
}

function launch(h) {
  if (G.phase !== 'play' || h.cool > 0) return;
  h.cool = 1.15;
  h.ctrl.updateMatrixWorld(true);
  h.L.muzzle.getWorldPosition(mp);
  const dir = new V(0, 0, -1).transformDirection(h.ctrl.matrixWorld);
  const obj = buildRocket();
  obj.scale.setScalar(0.72);
  scene.add(obj);
  const r = { obj, pos: mp.clone(), dir: dir.clone(), speed: 3.2, age: 0, target: h.lock, boomed: false };
  rockets.push(r);
  h.L.rocket.visible = false;
  h.L.ledMat.color.setHex(0xff3322);
  // back-blast
  h.L.rear.getWorldPosition(tmpV);
  for (let i = 0; i < 14; i++) fx.smoke.add(tmpV.x, tmpV.y, tmpV.z, -dir.x * rand(0.5, 2) + rand(-0.3, 0.3), -dir.y * rand(0.5, 2) + rand(-0.2, 0.3), -dir.z * rand(0.5, 2) + rand(-0.3, 0.3), rand(0.6, 1.2), 0.05, 0.22, [0.55, 0.53, 0.5], [0.2, 0.2, 0.2], 0.5, 1.5, 0);
  for (let i = 0; i < 8; i++) fx.fire.add(tmpV.x, tmpV.y, tmpV.z, -dir.x * rand(1, 4), -dir.y * rand(1, 4) + rand(-0.3, 0.3), -dir.z * rand(1, 4), rand(0.1, 0.25), 0.07, 0.02, [1, 0.85, 0.5], [1, 0.3, 0.05], 1, 3, 0);
  audio.launch(mp.clone());
  haptic(h, 0.9, 120);
}

const _q = new THREE.Quaternion(), _z = new V(0, 0, 1), _tgt = new V();
function updateRockets(dt) {
  for (let i = rockets.length - 1; i >= 0; i--) {
    const r = rockets[i];
    r.age += dt;
    r.speed = Math.min(8, 3.2 + r.age * 9);

    // moderate homing: after a short delay, bend toward the locked tank
    if (r.age > 0.1) {
      if (r.target && (r.target.dead || !los(r.pos.x, r.pos.y, r.pos.z, r.target.x, 0.08, r.target.z, 0.1, 0.1))) r.target = null;
      if (!r.target) r.target = lockCandidate(r.pos, r.dir, 0.42, 7, null);
      if (r.target) {
        _tgt.set(r.target.x, 0.07, r.target.z).sub(r.pos);
        const d = _tgt.length();
        if (d > 0.01) {
          _tgt.divideScalar(d);
          const ang = Math.acos(clamp(r.dir.dot(_tgt), -1, 1));
          const turn = 2.3 * dt;                        // rad/s — enough to curve, not enough to chase
          if (ang > 1e-4) r.dir.lerp(_tgt, Math.min(1, turn / ang)).normalize();
        }
      }
    }

    const step = r.speed * dt, n = Math.max(1, Math.ceil(step / 0.04));
    let hit = null, boom = false;
    for (let k = 0; k < n && !boom; k++) {
      r.pos.addScaledVector(r.dir, step / n);
      if (r.pos.y <= 0.01 || solidAt(r.pos.x, r.pos.y, r.pos.z)) { boom = true; break; }
      for (const t of tanks) {
        if (t.dead) continue;
        if (insideTank(t, r.pos)) { hit = t; boom = true; break; }
        // proximity fuse: goes off at the closest point of a near miss
        const d = Math.hypot(r.pos.x - t.x, r.pos.y - 0.07, r.pos.z - t.z);
        if (d < 0.45) {
          if (r.near === t && d > r.nearD + 0.004) { boom = true; break; }
          if (r.near !== t || d < r.nearD) { r.near = t; r.nearD = d; }
        }
      }
    }
    if (boom || r.age > 5 || r.pos.length() > 20) {
      scene.remove(r.obj); rockets.splice(i, 1);
      if (boom) blast(r.pos.clone(), hit);
      continue;
    }
    r.obj.position.copy(r.pos);
    r.obj.quaternion.setFromUnitVectors(_z, r.dir);
    // exhaust flame and smoke trail
    const tx = r.pos.x - r.dir.x * 0.17, ty = r.pos.y - r.dir.y * 0.17, tz = r.pos.z - r.dir.z * 0.17;
    fx.fire.add(tx, ty, tz, 0, 0, 0, 0.08, 0.085, 0.03, [1, 0.9, 0.55], [1, 0.35, 0.05], 1);
    fx.fire.add(tx, ty, tz, -r.dir.x * 1.2, -r.dir.y * 1.2, -r.dir.z * 1.2, 0.16, 0.05, 0.015, [1, 0.8, 0.3], [0.8, 0.2, 0], 0.9);
    fx.smoke.add(tx, ty, tz, rand(-0.04, 0.04), rand(0.0, 0.08), rand(-0.04, 0.04), 1.2, 0.035, 0.17, [0.75, 0.75, 0.74], [0.35, 0.35, 0.35], 0.5, 0.6, 0);
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
  const beam = new THREE.Mesh(beamGeo, beamMat);
  beam.position.copy(L.muzzle.position);
  L.group.add(beam);
  const dot = new THREE.Mesh(new THREE.SphereGeometry(0.01, 8, 6), dotMat);
  scene.add(dot);
  const ring = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({ color: 0xff4a33, transparent: true, opacity: 0.95, side: THREE.DoubleSide, depthWrite: false, blending: THREE.AdditiveBlending }));
  ring.visible = false; scene.add(ring);
  return { i, L, ctrl: null, cool: 0, lock: null, beam, dot, ring, gamepad: null, lastHud: '' };
}
hands.push(makeHand(0), makeHand(1));

function haptic(h, strength, ms) {
  try { h.gamepad?.hapticActuators?.[0]?.pulse(strength, ms); } catch (_) { /* no haptics */ }
}

function drawHud(h) {
  const key = `${G.hp}|${G.score}|${G.wave}|${h.cool > 0}|${G.phase}`;
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
  ctx.fillStyle = '#fff'; ctx.font = 'bold 28px Arial'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(`HEALTH ${Math.ceil(G.hp)}`, 150, 38);
  ctx.fillStyle = '#ffb454'; ctx.font = 'bold 30px Arial'; ctx.textAlign = 'left';
  ctx.fillText(`WAVE ${G.wave}`, 16, 92);
  ctx.textAlign = 'right'; ctx.fillText(`${G.score}`, 284, 92);
  ctx.textAlign = 'center'; ctx.font = 'bold 26px Arial';
  ctx.fillStyle = h.cool > 0 ? '#ff6a55' : '#6dff8c';
  ctx.fillText(h.cool > 0 ? 'RELOADING' : 'ROCKET READY', 150, 140);
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
    { t: 'Dodge the shells, or duck behind something solid.', s: 32, b: false },
    { t: 'Trigger: fire a homing rocket · Grip: show room mesh', s: 28, c: '#9fb3c8', b: false, gap: 8 },
    scanLine(),
    { t: 'PULL A TRIGGER TO START', s: 46, c: '#6dff8c', gap: 6 },
  ].map(l => ({ s: 30, ...l })));
}

/* ================================================================= flow */

function resetEntities() {
  for (const t of tanks) { scene.remove(t.model.group); t.eng.stop(); }
  tanks.length = 0;
  for (const r of rockets) scene.remove(r.obj); rockets.length = 0;
  for (const s of shells) scene.remove(s.obj); shells.length = 0;
  fx.clear();
}

function startGame() {
  resetEntities();
  hideMsg();
  G.hp = 100; G.score = 0; G.kills = 0; G.wave = 0; G.waveClear = false;
  const wantVirtual = params.has('cover') || scan.sources.size === 0;
  if (wantVirtual && !virt.made) addVirtualCover();
  else if (!wantVirtual && virt.made) { removeVirtualCover(); rebuildNow([...scan.sources.values()]); }
  G.haveRoom = !wantVirtual || scan.sources.size > 0;
  updatePlayerField(0, true);
  G.phase = 'play';
  for (const h of hands) { h.cool = 0; h.L.rocket.visible = true; h.L.ledMat.color.setHex(0x44ff66); }
  startWave(1);
}

function startWave(n) {
  G.wave = n;
  G.toSpawn = Math.min(2 + n, 9);
  G.maxAlive = Math.min(2 + Math.floor((n + 1) / 2), 5);
  G.spawnT = 1.8;
  G.waveClear = false;
  audio.alert('wave');
  showMsg([{ t: `WAVE ${n}`, s: 120, c: '#ffb454' }, { t: n === 1 ? 'Tanks inbound' : 'They are getting smarter', s: 40, c: '#e6edf5', b: false }], 2.4, 0.7);
}

function gameOver() {
  if (G.phase !== 'play') return;
  G.phase = 'over';
  G.inputLock = 1.8;
  audio.alert('over');
  for (const r of rockets) scene.remove(r.obj); rockets.length = 0;
  setTimeout(() => {
    if (G.phase !== 'over') return;
    showMsg([
      { t: 'GAME OVER', s: 110, c: '#ff5a44' },
      { t: `Wave ${G.wave}  ·  ${G.kills} tanks destroyed  ·  score ${G.score}`, s: 38 },
      { t: 'PULL A TRIGGER TO PLAY AGAIN', s: 44, c: '#6dff8c', gap: 20 },
    ]);
  }, 700);
}

function updateGame(dt) {
  if (G.phase !== 'play') return;
  const alive = tanks.reduce((n, t) => n + (t.dead ? 0 : 1), 0);
  if (!G.waveClear) {
    if (G.toSpawn > 0) {
      if (alive < G.maxAlive) {
        G.spawnT -= dt;
        if (G.spawnT <= 0) {
          const p = pickSpawn();
          if (p) { spawnTank(p.x, p.z); G.toSpawn--; G.spawnT = rand(2, 4); } else G.spawnT = 1;
        }
      }
    } else if (alive === 0) {
      G.waveClear = true; G.nextWaveT = 4;
      G.hp = Math.min(100, G.hp + 25);
      audio.alert('wave');
      showMsg([{ t: 'WAVE CLEARED', s: 90, c: '#6dff8c' }, { t: '+25 health', s: 40, b: false }], 2.6, 0.7);
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
    c.addEventListener('selectstart', () => onTrigger(i));
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
let yaw = 0, pitch = 0;
function enterFlat() {
  audio.init();
  G.mode = 'flat';
  document.body.classList.add('playing', 'flat');
  scene.background = new THREE.Color(0x141a22);
  renderer.setClearColor(0x141a22, 1);
  const grid = new THREE.GridHelper(12, 24, 0x3b4b60, 0x232d3a);
  grid.position.y = 0.001; scene.add(grid);
  hands.forEach((h, i) => {
    const c = new THREE.Group();
    c.position.set(i ? 0.3 : -0.3, -0.3, -0.5);
    c.rotation.set(0.03, i ? 0.09 : -0.09, 0);
    c.add(h.L.group);
    camera.add(c);
    h.ctrl = c;
  });
  addVirtualCover();
  let flip = 0;
  renderer.domElement.addEventListener('click', () => {
    if (document.pointerLockElement !== renderer.domElement) { renderer.domElement.requestPointerLock?.(); }
    onTrigger(flip ^= 1);
  });
  document.addEventListener('mousemove', e => {
    if (document.pointerLockElement !== renderer.domElement) return;
    yaw -= e.movementX * 0.0022; pitch = clamp(pitch - e.movementY * 0.0022, -1.3, 1.3);
  });
  window.addEventListener('keydown', e => { keys[e.code] = true; if (e.code === 'KeyG') onSqueeze(); });
  window.addEventListener('keyup', e => { keys[e.code] = false; });
  G.menuPending = true;
}

function flatMove(dt) {
  const sp = 1.7 * dt;
  const f = (keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0), s = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0);
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
  if (msgTimer > 0 && G.phase === 'play') { msgTimer -= dt; if (msgTimer <= 0) hideMsg(); }

  updatePlayerField(dt, false);
  updateGame(dt);

  // --- hands
  for (const h of hands) {
    if (!h.ctrl) continue;
    h.ctrl.updateMatrixWorld(true);
    if (h.cool > 0) {
      h.cool -= dt;
      if (h.cool <= 0) { h.L.rocket.visible = true; h.L.ledMat.color.setHex(0x44ff66); audio.reload(h.ctrl.getWorldPosition(tmpV2).clone()); }
    }
    h.L.muzzle.getWorldPosition(mp);
    md.set(0, 0, -1).transformDirection(h.ctrl.matrixWorld);
    // laser sight
    const hitD = rayHit(mp.x, mp.y, mp.z, md.x, md.y, md.z, 8);
    h.beam.scale.z = hitD;
    h.dot.position.copy(mp).addScaledVector(md, hitD);
    h.dot.visible = hitD < 8 && h.ctrl.visible !== false;
    // lock-on: closest tank within ~25° of where the launcher points, that it can see
    h.lock = G.phase === 'play' ? lockCandidate(mp, md, 0.44, 8, null) : null;
    if (h.lock) {
      h.ring.visible = true;
      h.ring.position.set(h.lock.x, 0.1, h.lock.z);
      h.ring.lookAt(head);
      h.ring.scale.setScalar(1 + 0.12 * Math.sin(G.time * 10));
    } else h.ring.visible = false;
    if (Math.floor(G.time * 6) !== h.hudTick) { h.hudTick = Math.floor(G.time * 6); drawHud(h); }
  }

  // --- the fight
  for (let i = tanks.length - 1; i >= 0; i--) {
    const t = tanks[i];
    if (t.dead) {
      t.wreckT -= dt;
      if (t.wreckT > 0 && Math.random() < dt * 14) fx.smoke.add(t.x, 0.09, t.z, rand(-0.05, 0.05), rand(0.15, 0.3), rand(-0.05, 0.05), rand(1.2, 2), 0.04, 0.2, [0.3, 0.3, 0.3], [0.08, 0.08, 0.08], 0.55, 0.3, 0);
      if (t.wreckT < 0.8) t.model.group.scale.setScalar(Math.max(0.001, t.wreckT / 0.8));
      if (t.wreckT <= 0) { scene.remove(t.model.group); tanks.splice(i, 1); }
      continue;
    }
    updateTank(t, dt);
  }
  // tanks do not park on top of each other
  for (let i = 0; i < tanks.length; i++) for (let j = i + 1; j < tanks.length; j++) {
    const a = tanks[i], b = tanks[j];
    if (a.dead || b.dead) continue;
    const dx = b.x - a.x, dz = b.z - a.z, d = Math.hypot(dx, dz);
    if (d < 0.28 && d > 1e-4) { const k = (0.28 - d) * 0.5 / d; moveTo(a, a.x - dx * k, a.z - dz * k); moveTo(b, b.x + dx * k, b.z + dz * k); }
  }
  updateShells(dt);
  updateRockets(dt);
  fx.update(dt);

  // hurt flash
  if (G.hurt > 0) G.hurt = Math.max(0, G.hurt - dt * 1.6);
  vignette.material.opacity = Math.min(1, G.hurt * 1.2 + (G.hp < 30 && G.phase === 'play' ? 0.18 + 0.12 * Math.sin(G.time * 6) : 0));
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
window.__tank = { scanRoom, pumpRebuild, killTank, blast, updatePlayerField, view(x, y, z, yw, pt) { camera.position.set(x, y, z); yaw = yw; pitch = pt; flatMove.lx = x; flatMove.lz = z; }, G, tanks, rockets, shells, hands, head, W, scan, spawnTank, launch, startGame, camera, scene, renderer, fx, audio, onTrigger, pickSpawn, showMenu };
