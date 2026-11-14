/*
 * world.js — what the game knows about the room.
 *
 * The headset hands us the scanned room as triangle meshes (and sometimes flat
 * planes). Raycasting against a 100k-triangle mesh every frame for every tank
 * and every shell would be far too slow, so the room is rasterised once into a
 * coarse voxel grid (6 cm cells) and everything else — line of sight, shell and
 * rocket collisions, tank navigation — reads that grid instead.
 *
 *   vox    solid / empty, 3D                    "can a shell fly through here"
 *   nav    blocked / free for a tank, 2D        floor-level footprint of the
 *                                               obstacles, grown by a tank's
 *                                               half-width
 *   fields flow fields (Dijkstra distance maps) tanks roll downhill on
 *
 * The grid is fixed in the reference space (local-floor): x and z run -6..+6 m
 * around where the session started, y runs from the floor up to ~3.2 m.
 */

export const CELL = 0.06, INV = 1 / CELL;
export const GX = 200, GY = 56, GZ = 200;
export const OX = -6, OY = -0.12, OZ = -6;
export const GXZ = GX * GZ;

export const W = {
  vox: new Uint8Array(GX * GY * GZ),
  build: new Uint8Array(GX * GY * GZ),
  nav: new Uint8Array(GXZ),
  arenaR: 0,          // >0: nothing outside this radius is drivable (no room scan)
  solidCount: 0,
  boxes: [],          // virtual cover, re-stamped into every rebuild
  version: 0,         // bumped whenever nav changes
};

export const cellX = x => Math.floor((x - OX) * INV);
export const cellZ = z => Math.floor((z - OZ) * INV);
export const cellCenterX = ix => OX + (ix + 0.5) * CELL;
export const cellCenterZ = iz => OZ + (iz + 0.5) * CELL;
export const inGrid = (ix, iz) => ix >= 0 && iz >= 0 && ix < GX && iz < GZ;

/* ---------------------------------------------------------------- 3D grid */

export function solidAt(x, y, z) {
  const fx = (x - OX) * INV, fz = (z - OZ) * INV, fy = (y - OY) * INV;
  if (fx < 0 || fz < 0 || fx >= GX || fz >= GZ) return true;   // the edge of the world is a wall
  if (fy < 0) return true;                                      // under the floor
  if (fy >= GY) return false;
  return W.vox[((fy | 0) * GZ + (fz | 0)) * GX + (fx | 0)] !== 0;
}

/** Clear line between two points? The ends are ignored by e0/e1 metres so a
 *  muzzle touching a wall, or a head next to a shelf, is not "hidden". */
export function los(ax, ay, az, bx, by, bz, e0 = 0.06, e1 = 0.12) {
  const dx = bx - ax, dy = by - ay, dz = bz - az;
  const len = Math.sqrt(dx * dx + dy * dy + dz * dz);
  if (len < 1e-4) return true;
  const n = Math.ceil(len / 0.03), t0 = e0 / len, t1 = 1 - e1 / len;
  for (let i = 1; i < n; i++) {
    const t = i / n;
    if (t < t0 || t > t1) continue;
    if (solidAt(ax + dx * t, ay + dy * t, az + dz * t)) return false;
  }
  return true;
}

/** Distance along a (unit) ray to the first solid voxel or the floor. */
export function rayHit(ox, oy, oz, dx, dy, dz, maxD) {
  const step = 0.03;
  for (let d = 0.05; d < maxD; d += step) {
    const x = ox + dx * d, y = oy + dy * d, z = oz + dz * d;
    if (y <= 0 || solidAt(x, y, z)) return d;
  }
  return maxD;
}

/* ------------------------------------------------------------- rasteriser */

const d2 = (ax, ay, az, bx, by, bz) => (ax - bx) * (ax - bx) + (ay - by) * (ay - by) + (az - bz) * (az - bz);

let built = 0;                                   // unique solid voxels stamped into the current build
function stamp(buf, x, y, z) {
  if (y < 0.05) return 0;                        // the floor itself is not an obstacle
  const fx = (x - OX) * INV, fz = (z - OZ) * INV, fy = (y - OY) * INV;
  if (fx < 0 || fz < 0 || fy < 0 || fx >= GX || fz >= GZ || fy >= GY) return 0;
  const i = ((fy | 0) * GZ + (fz | 0)) * GX + (fx | 0);
  if (!buf[i]) { buf[i] = 1; built++; }
  return 1;
}

/** Stamp a triangle into buf by sampling it finer than a cell. Returns sample count. */
export function rasterTri(buf, ax, ay, az, bx, by, bz, cx, cy, cz) {
  const e0 = d2(ax, ay, az, bx, by, bz), e1 = d2(bx, by, bz, cx, cy, cz), e2 = d2(cx, cy, cz, ax, ay, az);
  const m = Math.max(e0, e1, e2);
  if (m > 0.36) {                                 // longer than 60 cm: split the longest edge
    if (m === e0) { const mx = (ax + bx) / 2, my = (ay + by) / 2, mz = (az + bz) / 2;
      return rasterTri(buf, ax, ay, az, mx, my, mz, cx, cy, cz) + rasterTri(buf, mx, my, mz, bx, by, bz, cx, cy, cz); }
    if (m === e1) { const mx = (bx + cx) / 2, my = (by + cy) / 2, mz = (bz + cz) / 2;
      return rasterTri(buf, ax, ay, az, bx, by, bz, mx, my, mz) + rasterTri(buf, ax, ay, az, mx, my, mz, cx, cy, cz); }
    const mx = (cx + ax) / 2, my = (cy + ay) / 2, mz = (cz + az) / 2;
    return rasterTri(buf, ax, ay, az, bx, by, bz, mx, my, mz) + rasterTri(buf, mx, my, mz, bx, by, bz, cx, cy, cz);
  }
  const n = Math.max(1, Math.ceil(Math.sqrt(m) / 0.03));
  let c = 0;
  for (let i = 0; i <= n; i++) {
    for (let j = 0; j <= n - i; j++) {
      const u = i / n, v = j / n, w = 1 - u - v;
      c += stamp(buf, ax * w + bx * u + cx * v, ay * w + by * u + cy * v, az * w + bz * u + cz * v);
    }
  }
  return n * n;
}

/** Fill an (optionally rotated) box with solid voxels. */
export function stampBox(buf, b) {
  const cs = Math.cos(b.rot || 0), sn = Math.sin(b.rot || 0);
  const s = 0.04;
  for (let lx = -b.w / 2; lx <= b.w / 2 + 1e-6; lx += s)
    for (let lz = -b.d / 2; lz <= b.d / 2 + 1e-6; lz += s)
      for (let y = b.y0 ?? 0.06; y <= (b.y0 ?? 0) + b.h; y += s)
        stamp(buf, b.x + lx * cs + lz * sn, y, b.z - lx * sn + lz * cs);
}

/** Generator: rebuild the 3D grid from world-space triangle sources, a little
 *  at a time (yield every ~25k samples) so a big room scan never stalls a frame. */
export function* rebuildGen(sources) {
  const buf = W.build;
  buf.fill(0);
  built = 0;
  let budget = 0;
  for (const s of sources) {
    const P = s.pos, I = s.idx;
    for (let t = 0; t < I.length; t += 3) {
      const a = I[t] * 3, b = I[t + 1] * 3, c = I[t + 2] * 3;
      budget += rasterTri(buf, P[a], P[a + 1], P[a + 2], P[b], P[b + 1], P[b + 2], P[c], P[c + 1], P[c + 2]);
      if (budget > 25000) { budget = 0; yield; }
    }
  }
  for (const b of W.boxes) stampBox(buf, b);
  const tmp = W.vox; W.vox = W.build; W.build = tmp;
  W.solidCount = built;
  computeNav();
}

/** Synchronous version, for the flat preview and for tests. */
export function rebuildNow(sources) { for (const _ of rebuildGen(sources)); }

/* ------------------------------------------------------------- 2D nav map */

const raw = new Uint8Array(GXZ);

export function computeNav() {
  raw.fill(0);
  for (let iz = 0; iz < GZ; iz++)
    for (let ix = 0; ix < GX; ix++) {
      // a tank is ~11 cm tall: anything solid between 5 and 22 cm stops it
      for (let iy = 2; iy <= 5; iy++)
        if (W.vox[(iy * GZ + iz) * GX + ix]) { raw[iz * GX + ix] = 1; break; }
      if (W.arenaR > 0) {
        const x = cellCenterX(ix), z = cellCenterZ(iz);
        if (x * x + z * z > W.arenaR * W.arenaR) raw[iz * GX + ix] = 1;
      }
      if (ix < 2 || iz < 2 || ix >= GX - 2 || iz >= GZ - 2) raw[iz * GX + ix] = 1;
    }
  const nav = W.nav;
  nav.fill(0);
  for (let iz = 0; iz < GZ; iz++)
    for (let ix = 0; ix < GX; ix++) {
      if (!raw[iz * GX + ix]) continue;
      for (let dz = -2; dz <= 2; dz++)
        for (let dx = -2; dx <= 2; dx++) {
          if (dx * dx + dz * dz > 5) continue;            // a rounded footprint, not a square
          const x = ix + dx, z = iz + dz;
          if (inGrid(x, z)) nav[z * GX + x] = 1;
        }
    }
  W.version++;
}

export const navBlockedAt = (x, z) => {
  const ix = cellX(x), iz = cellZ(z);
  return !inGrid(ix, iz) || W.nav[iz * GX + ix] === 1;
};

/** Is the straight floor path between two points free? */
export function navLine(x0, z0, x1, z1) {
  const dx = x1 - x0, dz = z1 - z0, len = Math.hypot(dx, dz);
  const n = Math.ceil(len / 0.02);
  const from = navBlockedAt(x0, z0) ? 3 : 1;      // already wedged in a wall: do not count the way out
  for (let i = from; i <= n; i++) {
    const t = i / n;
    if (navBlockedAt(x0 + dx * t, z0 + dz * t)) return false;
  }
  return true;
}

/* ----------------------------------------------------------- flow fields */

const NDX = [1, -1, 0, 0, 1, 1, -1, -1], NDZ = [0, 0, 1, -1, 1, -1, 1, -1], NC = [10, 10, 10, 10, 14, 14, 14, 14];
export const FAR = 1e9;
export const newField = () => new Float32Array(GXZ).fill(FAR);

/** The nearest drivable cell to (x,z), or -1. */
export function nearestFree(x, z, maxR = 12) {
  const cx = cellX(x), cz = cellZ(z);
  for (let r = 0; r <= maxR; r++)
    for (let dz = -r; dz <= r; dz++)
      for (let dx = -r; dx <= r; dx++) {
        if (Math.max(Math.abs(dx), Math.abs(dz)) !== r) continue;
        const ix = cx + dx, iz = cz + dz;
        if (inGrid(ix, iz) && !W.nav[iz * GX + ix]) return iz * GX + ix;
      }
  return -1;
}

/**
 * Dijkstra over free nav cells from one seed cell index. Cost: 10 per cell.
 * A generator so the big player field can be spread over several frames (it
 * yields every ~1500 cells); the result is copied into `field` only when done,
 * so a half-finished field is never read. Each job has its own heap.
 */
export function* computeFieldGen(field, seed, maxCost = 1e8) {
  const out = new Float32Array(GXZ).fill(FAR);
  if (seed < 0) { field.set(out); return; }
  const hc = [], hi = [];
  const push = (c, i) => {
    let k = hc.length; hc.push(c); hi.push(i);
    while (k > 0) {
      const p = (k - 1) >> 1;
      if (hc[p] <= c) break;
      hc[k] = hc[p]; hi[k] = hi[p]; k = p;
    }
    hc[k] = c; hi[k] = i;
  };
  out[seed] = 0; push(0, seed);
  const nav = W.nav;
  let work = 0;
  while (hc.length) {
    const c = hc[0], i = hi[0];
    const lc = hc.pop(), li = hi.pop();
    const n = hc.length;
    if (n > 0) {                                  // sift the last element down from the root
      let k = 0;
      for (;;) {
        let l = 2 * k + 1;
        if (l >= n) break;
        if (l + 1 < n && hc[l + 1] < hc[l]) l++;
        if (hc[l] >= lc) break;
        hc[k] = hc[l]; hi[k] = hi[l]; k = l;
      }
      hc[k] = lc; hi[k] = li;
    }
    if (c > out[i]) continue;
    if (c > maxCost) break;
    const ix = i % GX, iz = (i / GX) | 0;
    for (let k = 0; k < 8; k++) {
      const x = ix + NDX[k], z = iz + NDZ[k];
      if (x < 0 || z < 0 || x >= GX || z >= GZ) continue;
      const j = z * GX + x;
      if (nav[j]) continue;
      if (k >= 4 && (nav[iz * GX + x] || nav[z * GX + ix])) continue;   // no cutting corners
      const nc = c + NC[k];
      if (nc < out[j]) { out[j] = nc; push(nc, j); }
    }
    if (++work % 1500 === 0) yield;
  }
  field.set(out);
}

/** Run a field job to completion right now (for the small cover fields). */
export function computeField(field, seed, maxCost = 1e8) { for (const _ of computeFieldGen(field, seed, maxCost)); }

export const fieldAt = (field, x, z) => {
  const ix = cellX(x), iz = cellZ(z);
  return inGrid(ix, iz) ? field[iz * GX + ix] : FAR;
};
/** Path length in metres from a field value. */
export const fieldMetres = c => c * CELL / 10;

const out = { x: 0, z: 0, ok: false };
/**
 * Follow the field downhill from (x,z) for up to ~1.5 m, then aim at the
 * furthest point of that walk that is in a straight free line — which pulls the
 * jagged grid path taut, so tanks drive straight where they can and only bend
 * round the corners that are really there.
 */
export function pathTarget(field, x, z) {
  out.ok = false;
  let ix = cellX(x), iz = cellZ(z);
  if (!inGrid(ix, iz) || field[iz * GX + ix] >= FAR) return out;
  const pts = [];
  let cur = iz * GX + ix;
  for (let s = 0; s < 26; s++) {
    const cx = cur % GX, cz = (cur / GX) | 0;
    let best = -1, bv = field[cur];
    for (let k = 0; k < 8; k++) {
      const nx = cx + NDX[k], nz = cz + NDZ[k];
      if (!inGrid(nx, nz)) continue;
      const j = nz * GX + nx;
      if (field[j] < bv) { bv = field[j]; best = j; }
    }
    if (best < 0) break;
    cur = best;
    pts.push(cur);
    if (bv <= 0) break;
  }
  if (!pts.length) { out.ok = true; out.x = x; out.z = z; return out; }
  let pick = pts[0];
  for (let k = pts.length - 1; k >= 0; k--) {
    const px = cellCenterX(pts[k] % GX), pz = cellCenterZ((pts[k] / GX) | 0);
    if (navLine(x, z, px, pz)) { pick = pts[k]; break; }
  }
  out.ok = true;
  out.x = cellCenterX(pick % GX); out.z = cellCenterZ((pick / GX) | 0);
  return out;
}
