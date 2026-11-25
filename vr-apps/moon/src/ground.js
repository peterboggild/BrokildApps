/* The generated ground round the walker: the page's side of ground-worker.js.
 *
 * One mesh, one map texture and a few instanced rock batches, all reused: when the walker has moved away from the
 * centre of the square, a new square is asked for and swapped in when it arrives. The levels leave the square out
 * (uPatch); the square leaves L6 out. What the worker makes is a pure function of position, so a swap changes
 * nothing in the part both squares cover. */
import * as THREE from 'three';
import { gridHeight } from './geo.js?v=3';
import { rockGeometries, groundRockMaterial } from './extras.js?v=5';

const RECENTRE = 22;        /* ask for a new square when the walker is this far from the centre, metres */
const SNAP = 8;             /* centres on a grid, so standing still never asks twice */

export function makeGround(moon, base) {
  const man = moon.manifest, group = new THREE.Group();
  if (!man.valley || !man.craters) return { group, ok: false, update() {}, request() { return Promise.resolve(false); }, covers() { return false; } };
  const worker = new Worker(new URL('./ground-worker.js?v=3', import.meta.url), { type: 'module' });
  const lv = n => moon.levels.find(l => l.name === n);
  const cut = l => l.P.slice(0, (l.nx + 1) * (l.ny + 1) * 3);
  worker.postMessage({ type: 'init', man, base, P4: cut(lv('L4')), P5: cut(lv('L5')) });

  const u = moon.groundUniforms, half6 = moon.l6.half, limit = lv('L4').half - 200;
  const mat = moon.groundMaterial(), rockMat = groundRockMaterial(moon);
  let mesh = null, tex = null, hAt = null, rect = null, centre = null, rockMeshes = [];
  let pending = null, nextId = 1, lastMs = 0, count = 0;
  const waiters = [];

  worker.onmessage = e => {
    const m = e.data;
    if (m.type !== 'ground') return;
    if (pending && m.id === pending.id) { install(m); const p = pending; pending = null; p.resolve(true); }
  };
  worker.onerror = e => { console.error('ground worker: ' + (e.message || e)); if (pending) { pending.resolve(false); pending = null; } };

  function install(m) {
    const n = m.n, nv = (n + 1) * (n + 1);
    if (!mesh) {
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(nv * 3), 3));
      g.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(nv * 2), 2));
      g.setAttribute('uvA', new THREE.BufferAttribute(new Float32Array(nv * 2), 2));
      g.setAttribute('uvB', new THREE.BufferAttribute(new Float32Array(nv * 2), 2));
      const idx = new Uint32Array(n * n * 6);
      let q = 0;
      for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
        const a = j * (n + 1) + i, b = a + n + 1;
        idx[q++] = a; idx[q++] = b; idx[q++] = a + 1; idx[q++] = b; idx[q++] = b + 1; idx[q++] = a + 1;
      }
      g.setIndex(new THREE.BufferAttribute(idx, 1));
      mesh = new THREE.Mesh(g, mat);
      mesh.frustumCulled = false; mesh.matrixAutoUpdate = false; mesh.renderOrder = 8;
      group.add(mesh);
      tex = new THREE.DataTexture(new Uint8Array(m.t * m.t * 4), m.t, m.t, THREE.RGBAFormat);
      tex.generateMipmaps = true; tex.minFilter = THREE.LinearMipmapLinearFilter; tex.magFilter = THREE.LinearFilter;
      tex.anisotropy = 4; tex.wrapS = tex.wrapT = THREE.ClampToEdgeWrapping;
      u.uMap.value = tex;
    }
    const g = mesh.geometry, x0 = m.cx - m.half, z0 = m.cz - m.half, uv = g.attributes.uv.array;
    g.attributes.position.array.set(m.pos); g.attributes.uvA.array.set(m.uvA); g.attributes.uvB.array.set(m.uvB);
    for (let k = 0; k < nv; k++) { uv[k * 2] = (m.pos[k * 3] - x0) / (2 * m.half); uv[k * 2 + 1] = (m.pos[k * 3 + 2] - z0) / (2 * m.half); }
    for (const a of ['position', 'uv', 'uvA', 'uvB']) g.attributes[a].needsUpdate = true;
    tex.image.data.set(m.map); tex.needsUpdate = true;
    hAt = gridHeight(m.pos, n, n);
    centre = { x: m.cx, z: m.cz };
    rect = { x0, z0, x1: x0 + 2 * m.half, z1: z0 + 2 * m.half };
    u.uGround.value.set(rect.x0, rect.z0, rect.x1, rect.z1);
    moon.shared.uPatch.value.set(rect.x0 + 1, rect.z0 + 1, rect.x1 - 1, rect.z1 - 1);
    setRocks(m.rocks);
    lastMs = m.ms; count++;
  }

  /* rocks: one instanced batch per size class and shape */
  const m4 = new THREE.Matrix4(), q4 = new THREE.Quaternion(), e3 = new THREE.Euler(), s3 = new THREE.Vector3(), p3 = new THREE.Vector3();
  function setRocks(d) {
    for (const r of rockMeshes) { group.remove(r); r.geometry.dispose(); r.dispose(); }
    rockMeshes = [];
    const geos = rockGeometries(), buckets = new Map(), n = d.length / 8;
    for (let i = 0; i < n; i++) {
      const size = d[i * 8 + 3], cls = size < 0.22 ? 0 : size < 0.55 ? 1 : 2, key = cls * 3 + (d[i * 8 + 5] | 0) % 3;
      let b = buckets.get(key); if (!b) buckets.set(key, b = []); b.push(i);
    }
    for (const [key, list] of buckets) {
      const im = new THREE.InstancedMesh(geos[key % 3][(key / 3) | 0], rockMat, list.length), uvA = new Float32Array(list.length * 2);
      list.forEach((i, k) => {
        const yaw = d[i * 8 + 4];
        e3.set(Math.sin(yaw * 7) * 0.25, yaw, Math.cos(yaw * 5) * 0.25);
        m4.compose(p3.set(d[i * 8], d[i * 8 + 1], d[i * 8 + 2]), q4.setFromEuler(e3), s3.setScalar(d[i * 8 + 3]));
        im.setMatrixAt(k, m4);
        uvA[k * 2] = d[i * 8 + 6]; uvA[k * 2 + 1] = d[i * 8 + 7];
      });
      im.geometry = im.geometry.clone();          /* the instanced attribute belongs to this batch */
      im.geometry.setAttribute('aUvA', new THREE.InstancedBufferAttribute(uvA, 2));
      im.frustumCulled = false; im.userData.small = key < 3;
      group.add(im); rockMeshes.push(im);
    }
  }

  const inL6 = (x, z, margin) => Math.abs(x) < half6 - margin && Math.abs(z) < half6 - margin;
  const covers = (x, z, margin = 0) => !!rect && x > rect.x0 + margin && x < rect.x1 - margin && z > rect.z0 + margin && z < rect.z1 - margin;
  /* is the ground detailed at (x, z) right now? (L6, or the square) */
  const detailed = (x, z) => inL6(x, z, 2) || covers(x, z, 14);

  moon.groundHeight = (x, z) => (hAt && covers(x, z) ? hAt(x, z) : null);

  /* ask for the square round (x, z); resolves true when it is in place (false if it cannot be made) */
  function request(x, z) {
    x = Math.max(-limit, Math.min(limit, Math.round(x / SNAP) * SNAP)); z = Math.max(-limit, Math.min(limit, Math.round(z / SNAP) * SNAP));
    if (centre && centre.x === x && centre.z === z && !pending) return Promise.resolve(true);
    if (pending && pending.x === x && pending.z === z) return pending.promise;
    if (pending) pending.resolve(false);           /* overtaken */
    let resolve;
    const promise = new Promise(r => { resolve = r; });
    pending = { id: nextId++, x, z, resolve, promise };
    worker.postMessage({ type: 'gen', id: pending.id, cx: x, cz: z });
    return promise;
  }

  /* every frame on the surface: keep the square round the walker */
  function update(x, z) {
    if (inL6(x, z, 100)) return;                    /* deep inside L6 nothing of the square would show */
    if (pending) return;
    if (!centre || Math.abs(x - centre.x) > RECENTRE || Math.abs(z - centre.z) > RECENTRE) request(x, z);
  }

  return { group, ok: true, request, update, covers, detailed, limit,
    get rect() { return rect; }, get ms() { return lastMs; }, get count() { return count; }, get busy() { return !!pending; },
    get rocks() { return rockMeshes.reduce((a, r) => a + r.count, 0); } };
}
