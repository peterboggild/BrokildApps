/*
 * Bordbyen — the city: a grid of cells on the table, what is drawn on them, and the meshes.
 *
 * Everything here is in city metres. The same meshes are shown in both modes: shrunk onto the real table (design) and at full size around the player
 * (driving); only the city's root matrix changes. One cell is CELL metres (one road's width); the board has W × H cells.
 *
 * What a cell can hold:
 *   - a road (cells[k] = 1): asphalt, kerbs and pavements where the neighbour is not a road, lane dashes on straight pieces, zebra crossings at junctions;
 *   - a part of a lot (lot[k] = id): a rectangle of cells with one kind of thing on it (houses, a town block, a tower, a town hall, a church, a park,
 *     a square). Lots are built in a toy-town style from boxes, prisms and cones, deterministic from the lot's seed, one merged mesh each;
 *   - nothing: grass.
 * Gates sit on road cells: the start/finish and numbered gates, raced through in the order they were placed.
 *
 * Driving: the solid lots are boxes the cars bounce off (the lot less its pavement); parks have trees, squares a fountain (circles); parks and grass are
 * slow, roads and squares fast. The opponents' route is found through roads and squares only (breadth first, then rounded).
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';

export const CELL = 8;
export const SCALE = 1 / 160;                         // city metres to table metres: a cell is 5 cm on the table, a car 2.5 cm
export const TOOLS = [
  { key: 'vej', name: 'Vej', color: '#5b6270', kind: 'road' },
  { key: 'huse', name: 'Huse', color: '#f3b6a5', kind: 'lot', solid: true },
  { key: 'byhus', name: 'Byhuse', color: '#f7e48b', kind: 'lot', solid: true },
  { key: 'hoejhus', name: 'Højhus', color: '#8fc3ea', kind: 'lot', solid: true },
  { key: 'raadhus', name: 'Rådhus', color: '#c8664a', kind: 'lot', solid: true, min: 2 },
  { key: 'kirke', name: 'Kirke', color: '#f2efe6', kind: 'lot', solid: true, min: 2 },
  { key: 'park', name: 'Park', color: '#7cc46a', kind: 'lot' },
  { key: 'torv', name: 'Torv', color: '#d9c9a8', kind: 'lot' },
  { key: 'start', name: 'Start/mål', color: '#ffffff', kind: 'gate' },
  { key: 'port', name: 'Port', color: '#ff9a3c', kind: 'gate' },
  { key: 'slet', name: 'Viskelæder', color: '#ff6a6a', kind: 'erase' },
];
const TOOL = Object.fromEntries(TOOLS.map(t => [t.key, t]));

/* a small deterministic random generator per lot */
function rng(seed) { let s = (seed >>> 0) || 1; return () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; }
const pick = (r, a) => a[Math.floor(r() * a.length) % a.length];

const WALLS = ['#f6d6a8', '#f3b6a5', '#a8d8f0', '#c9e7a4', '#f7e48b', '#e8c4f0', '#ffd1dc', '#bfe3d0', '#fbe3c4'];
const ROOFS = ['#d9534f', '#b5523b', '#4a6fa5', '#5d8a4a', '#8a5a44', '#c0603a'];
const AWNINGS = ['#e85d5d', '#3c8dd8', '#43a85b', '#f0a530', '#9b59b6'];

/* ---- geometry builder: coloured, non-indexed pieces merged into one mesh ---- */
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _e = new THREE.Euler(), _c = new THREE.Color();
class Builder {
  constructor() { this.parts = []; }
  add(geo, color, x = 0, y = 0, z = 0, ry = 0, sx = 1, sy = 1, sz = 1) {
    let g = geo.index ? geo.toNonIndexed() : geo.clone();
    g.deleteAttribute('uv'); if (g.getAttribute('uv1')) g.deleteAttribute('uv1');
    g.applyMatrix4(_m.compose(new THREE.Vector3(x, y, z), _q.setFromEuler(_e.set(0, ry, 0)), new THREE.Vector3(sx, sy, sz)));
    _c.set(color); const n = g.getAttribute('position').count, col = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) { col[i * 3] = _c.r; col[i * 3 + 1] = _c.g; col[i * 3 + 2] = _c.b; }
    g.setAttribute('color', new THREE.BufferAttribute(col, 3)); this.parts.push(g); return this;
  }
  /** a box standing on y0 (its bottom), centred on x, z */
  box(x, y0, z, w, h, d, color, ry = 0) { return this.add(G.box, color, x, y0 + h / 2, z, ry, w, h, d); }
  /** a pitched roof: a prism along local x, standing on y0 */
  roof(x, y0, z, w, h, d, color, ry = 0) { return this.add(G.prism, color, x, y0, z, ry, w, h, d); }
  cyl(x, y0, z, r, h, color, seg = 'cyl') { return this.add(G[seg], color, x, y0 + h / 2, z, 0, r, h, r); }
  cone(x, y0, z, r, h, color, four = false) { return this.add(four ? G.pyr : G.cone, color, x, y0 + h / 2, z, four ? Math.PI / 4 : 0, r, h, r); }
  ball(x, y, z, r, color, sy = 1) { return this.add(G.ball, color, x, y, z, 0, r, r * sy, r); }
  build() { if (!this.parts.length) return null; const g = mergeGeometries(this.parts, false); g.computeBoundingSphere(); return g; }
}
const G = {
  box: new THREE.BoxGeometry(1, 1, 1),
  cyl: new THREE.CylinderGeometry(1, 1, 1, 10),
  cyl6: new THREE.CylinderGeometry(1, 1, 1, 6),
  cone: new THREE.ConeGeometry(1, 1, 10),
  pyr: new THREE.ConeGeometry(1.4142, 1, 4),
  ball: new THREE.IcosahedronGeometry(1, 1),
  prism: (() => { const s = new THREE.Shape(); s.moveTo(-0.5, 0); s.lineTo(0.5, 0); s.lineTo(0, 1); s.closePath();
    const g = new THREE.ExtrudeGeometry(s, { depth: 1, bevelEnabled: false }); g.translate(0, 0, -0.5); g.rotateY(Math.PI / 2); return g; })(),      // ridge along x, gables at ±x
};
G.prism.computeVertexNormals();

/* ---- trees, windows: shared bits ---- */
function tree(b, x, z, r, s = 1) {
  const kind = r(), h = (3 + r() * 2.5) * s;
  b.cyl(x, 0, z, 0.25 * s, h * 0.45, '#7a5232', 'cyl6');
  if (kind < 0.55) { b.ball(x, h * 0.62, z, 1.5 * s, pick(r, ['#4f9e3f', '#5cae45', '#3f8c3a']), 1.05); b.ball(x + 0.6 * s, h * 0.72, z - 0.3 * s, 1.0 * s, '#6cbf52'); }
  else b.cone(x, h * 0.3, z, 1.4 * s, h * 0.95, pick(r, ['#2f7a45', '#3a8a4e']));
}
/** windows round a box: rows per floor, a few per side */
function windows(b, x, z, w, d, y0, floors, fh, color, ry = 0, skipGround = false) {
  const c = Math.cos(ry), s = Math.sin(ry);
  const put = (lx, y, lz, ww, hh, dd) => b.box(x + lx * c + lz * s, y, z - lx * s + lz * c, ww, hh, dd, color, ry);
  for (let f = skipGround ? 1 : 0; f < floors; f++) {
    const y = y0 + f * fh + fh * 0.3, wh = fh * 0.45;
    const nx = Math.max(1, Math.floor(w / 2.4)), nz = Math.max(1, Math.floor(d / 2.4));
    for (let k = 0; k < nx; k++) { const lx = -w / 2 + (k + 0.5) * w / nx; put(lx, y, d / 2 + 0.04, 1.1, wh, 0.12); put(lx, y, -d / 2 - 0.04, 1.1, wh, 0.12); }
    for (let k = 0; k < nz; k++) { const lz = -d / 2 + (k + 0.5) * d / nz; put(w / 2 + 0.04, y, lz, 0.12, wh, 1.1); put(-w / 2 - 0.04, y, lz, 0.12, wh, 1.1); }
  }
}

export function createCity() {
  const root = new THREE.Group(); root.name = 'city';
  const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.85, metalness: 0.0 });
  const st = { W: 20, H: 14, cells: null, lot: null, lots: new Map(), gates: [], nextId: 1 };
  const meshes = new Map();                            // lot id -> mesh
  let roadMesh = null, groundMesh = null, gridLines = null, gateGroup = new THREE.Group(); root.add(gateGroup);
  const colliders = { rects: [], circles: [] };

  const idx = (i, j) => j * st.W + i;
  const inside = (i, j) => i >= 0 && j >= 0 && i < st.W && j < st.H;
  const cx = i => (i + 0.5 - st.W / 2) * CELL, cz = j => (j + 0.5 - st.H / 2) * CELL;
  const isRoad = (i, j) => inside(i, j) && st.cells[idx(i, j)] === 1;
  const cellAt = (x, z) => [Math.floor(x / CELL + st.W / 2), Math.floor(z / CELL + st.H / 2)];

  function reset(W, H) {
    st.W = W; st.H = H; st.cells = new Uint8Array(W * H); st.lot = new Int32Array(W * H).fill(0); st.lots.clear(); st.gates = []; st.nextId = 1;
    for (const m of meshes.values()) { root.remove(m); m.geometry.dispose(); } meshes.clear();
    rebuildGround(); rebuildRoads(); rebuildGates(); rebuildColliders();
  }

  /* ---------- editing ---------- */
  function removeLot(id) {
    const L = st.lots.get(id); if (!L) return; st.lots.delete(id);
    for (let j = L.z0; j <= L.z1; j++) for (let i = L.x0; i <= L.x1; i++) if (st.lot[idx(i, j)] === id) st.lot[idx(i, j)] = 0;
    const m = meshes.get(id); if (m) { root.remove(m); m.geometry.dispose(); meshes.delete(id); }
  }
  function clearRect(x0, z0, x1, z1) {
    let changed = false;
    for (let j = z0; j <= z1; j++) for (let i = x0; i <= x1; i++) {
      if (!inside(i, j)) continue; const k = idx(i, j);
      if (st.lot[k]) { removeLot(st.lot[k]); changed = true; }
      if (st.cells[k]) { st.cells[k] = 0; changed = true; }
    }
    const before = st.gates.length; st.gates = st.gates.filter(g => !(g.i >= x0 && g.i <= x1 && g.j >= z0 && g.j <= z1)); if (st.gates.length !== before) changed = true;
    return changed;
  }
  const norm = (a, b, c, d) => [Math.max(0, Math.min(a, c)), Math.max(0, Math.min(b, d)), Math.min(st.W - 1, Math.max(a, c)), Math.min(st.H - 1, Math.max(b, d))];
  /** paint road on one cell (a lot under it goes) */
  function paintRoad(i, j) {
    if (!inside(i, j) || isRoad(i, j)) return false;
    const k = idx(i, j); if (st.lot[k]) removeLot(st.lot[k]);
    st.cells[k] = 1; return true;
  }
  /** can a lot of this type go here? (size) */
  function lotOK(type, x0, z0, x1, z1) { const t = TOOL[type], w = x1 - x0 + 1, d = z1 - z0 + 1; return !(t.min && (w < t.min || d < t.min)); }
  function placeLot(type, a, b, c, d) {
    const [x0, z0, x1, z1] = norm(a, b, c, d); if (!lotOK(type, x0, z0, x1, z1)) return false;
    clearRect(x0, z0, x1, z1);
    const id = st.nextId++, L = { id, type, x0, z0, x1, z1, seed: (Math.random() * 1e9) | 0 };
    st.lots.set(id, L); for (let j = z0; j <= z1; j++) for (let i = x0; i <= x1; i++) st.lot[idx(i, j)] = id;
    buildLot(L); return true;
  }
  function eraseRect(a, b, c, d) { const [x0, z0, x1, z1] = norm(a, b, c, d); return clearRect(x0, z0, x1, z1); }
  /** a gate on a road cell: start/finish (only one), or the next numbered gate; on an existing gate it is removed */
  function setGate(i, j, start) {
    if (!isRoad(i, j)) return false;
    const at = st.gates.findIndex(g => g.i === i && g.j === j);
    if (at >= 0) { st.gates.splice(at, 1); return true; }
    if (start) st.gates = st.gates.filter(g => !g.start);
    st.gates.push({ i, j, start: !!start }); return true;
  }
  /** after any edit: roads, gates and colliders follow */
  function commit() { rebuildRoads(); rebuildGates(); rebuildColliders(); }

  /* ---------- meshes ---------- */
  function rebuildGround() {
    if (groundMesh) { root.remove(groundMesh); groundMesh.geometry.dispose(); }
    const b = new Builder(), W = st.W * CELL, H = st.H * CELL;
    b.box(0, -0.6, 0, W + 1.2, 0.6, H + 1.2, '#6f5a43');                                 // the board's edge, seen from the side
    b.box(0, 0, 0, W, 0.015, H, '#8fcf6e');                                          // grass
    groundMesh = new THREE.Mesh(b.build(), mat); groundMesh.receiveShadow = false; root.add(groundMesh);
    /* the grid, for drawing */
    if (gridLines) { root.remove(gridLines); gridLines.geometry.dispose(); }
    const pts = [];
    for (let i = 0; i <= st.W; i++) { const x = (i - st.W / 2) * CELL; pts.push(x, 0.05, -H / 2, x, 0.05, H / 2); }
    for (let j = 0; j <= st.H; j++) { const z = (j - st.H / 2) * CELL; pts.push(-W / 2, 0.05, z, W / 2, 0.05, z); }
    const gg = new THREE.BufferGeometry(); gg.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    gridLines = new THREE.LineSegments(gg, new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.22 })); root.add(gridLines);
  }
  function rebuildRoads() {
    if (roadMesh) { root.remove(roadMesh); roadMesh.geometry.dispose(); roadMesh = null; }
    const b = new Builder(), h = CELL / 2, pw = 1.4;
    for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) {
      if (!isRoad(i, j)) continue;
      const x = cx(i), z = cz(j), N = isRoad(i, j - 1), S = isRoad(i, j + 1), E = isRoad(i + 1, j), Wd = isRoad(i - 1, j), n = N + S + E + Wd;
      b.box(x, 0, z, CELL, 0.03, CELL, '#4d535e');
      /* pavements and kerbs where the road ends at a side */
      if (!N) b.box(x, 0, z - h + pw / 2, CELL, 0.16, pw, '#c9c4ba'); if (!S) b.box(x, 0, z + h - pw / 2, CELL, 0.16, pw, '#c9c4ba');
      if (!E) b.box(x + h - pw / 2, 0, z, pw, 0.16, CELL, '#c9c4ba'); if (!Wd) b.box(x - h + pw / 2, 0, z, pw, 0.16, CELL, '#c9c4ba');
      /* corner posts of pavement where two roads meet at a corner but the diagonal is not road (rounds off junction corners visually) */
      for (const [dx, dz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) {
        const a = dx > 0 ? E : Wd, c = dz > 0 ? S : N;
        if (a && c && !isRoad(i + dx, j + dz)) b.box(x + dx * (h - pw / 2), 0, z + dz * (h - pw / 2), pw, 0.16, pw, '#c9c4ba');
      }
      const straightZ = (N || S) && !E && !Wd, straightX = (E || Wd) && !N && !S;
      if (straightZ) for (const o of [-2, 2]) b.box(x, 0.03, z + o, 0.22, 0.01, 2, '#f4f1e6');
      else if (straightX) for (const o of [-2, 2]) b.box(x + o, 0.03, z, 2, 0.01, 0.22, '#f4f1e6');
      else if (n >= 3) {                                                                // a junction: zebra crossings just outside it, on the roads leading in
        for (const [on, dx, dz] of [[N, 0, -1], [S, 0, 1], [E, 1, 0], [Wd, -1, 0]]) {
          if (!on) continue;
          for (let k = -2; k <= 2; k++) {
            if (dz) b.box(x + k * 1.25, 0.03, z + dz * (h - 0.9), 0.6, 0.01, 1.4, '#f4f1e6');
            else b.box(x + dx * (h - 0.9), 0.03, z + k * 1.25, 1.4, 0.01, 0.6, '#f4f1e6');
          }
        }
      } else if (n === 2) {                                                             // a bend: a dashed quarter circle round the inside
        const ox = E ? 1 : -1, oz = S ? 1 : -1;
        for (let k = 0; k < 3; k++) { const a = (k + 0.5) / 3 * Math.PI / 2, px = x + ox * h - ox * h * Math.cos(a) * 1, pz = z + oz * h - oz * h * Math.sin(a);
          b.box(px, 0.03, pz, 0.5, 0.01, 0.5, '#f4f1e6'); }
      }
    }
    const g = b.build(); if (g) { roadMesh = new THREE.Mesh(g, mat); root.add(roadMesh); }
  }

  /* lots, one generator per kind; (x, z) the lot's centre, w × d its size in metres */
  const GEN = {
    huse(b, L, r, x, z, w, d) {
      b.box(x, 0, z, w - 0.4, 0.08, d - 0.4, '#a9d98b');
      /* a row of little houses along the longer side, facing the nearest road */
      const along = w >= d, len = along ? w : d, n = Math.max(1, Math.round(len / 7.5)), step = len / n;
      for (let k = 0; k < n; k++) {
        const hw = step - 1.4, hd = Math.min((along ? d : w) - 3, 7 + r() * 2), wall = pick(r, WALLS), roofc = pick(r, ROOFS), floors = r() < 0.35 ? 2 : 1, hh = floors * 3 + 0.4;
        const off = -len / 2 + (k + 0.5) * step, hx = along ? x + off : x, hz = along ? z : z + off, ry = along ? 0 : Math.PI / 2;
        const W2 = along ? hw : hd, D2 = along ? hd : hw;
        b.box(hx, 0, hz, W2, hh, D2, wall);
        b.roof(hx, hh, hz, hw + 0.6, 2.6 + r(), hd + 0.8, roofc, along ? 0 : Math.PI / 2);
        windows(b, hx, hz, W2, D2, 0, floors, 3, '#3d5a80');
        if (r() < 0.7) b.box(hx + (r() - 0.5) * W2 * 0.5, hh + 1.2, hz + (r() - 0.5) * D2 * 0.3, 0.7, 1.8, 0.7, '#8b4a3c');      // chimney
        /* a door on each long face, and a tree in some gardens */
        if (along) { b.box(hx, 0, hz + hd / 2 + 0.05, 1.1, 2.1, 0.15, '#7a4a2a'); b.box(hx, 0, hz - hd / 2 - 0.05, 1.1, 2.1, 0.15, '#7a4a2a'); }
        else { b.box(hx + hd / 2 + 0.05, 0, hz, 0.15, 2.1, 1.1, '#7a4a2a'); b.box(hx - hd / 2 - 0.05, 0, hz, 0.15, 2.1, 1.1, '#7a4a2a'); }
      }
      if ((along ? d : w) > 12) for (let k = 0; k < Math.floor(len / 9); k++) { const o = -len / 2 + 4 + k * 9 + r() * 2, s = (along ? d : w) / 2 - 2;
        tree(b, along ? x + o : x + s * (r() < 0.5 ? 1 : -1), along ? z + s * (r() < 0.5 ? 1 : -1) : z + o, r, 0.8); }
    },
    byhus(b, L, r, x, z, w, d) {
      /* a town block: buildings of three to five floors round the edge, shop awnings at street level */
      b.box(x, 0, z, w, 0.12, d, '#d8d2c6');
      const along = w >= d, len = along ? w : d, dep = along ? d : w, n = Math.max(1, Math.round(len / 9)), step = len / n;
      for (let k = 0; k < n; k++) {
        const floors = 3 + Math.floor(r() * 3), fh = 3.1, hh = floors * fh, wall = pick(r, WALLS), off = -len / 2 + (k + 0.5) * step;
        const bw = step - 0.3, bd = dep - 2.4, hx = along ? x + off : x, hz = along ? z : z + off, W2 = along ? bw : bd, D2 = along ? bd : bw;
        b.box(hx, 0, hz, W2, hh, D2, wall);
        b.box(hx, hh, hz, W2 + 0.3, 0.5, D2 + 0.3, '#e9e4da');                                                    // cornice
        if (r() < 0.4) b.roof(hx, hh + 0.5, hz, bw, 2.2, bd, pick(r, ROOFS), along ? 0 : Math.PI / 2);
        windows(b, hx, hz, W2, D2, 0, floors, fh, '#2f4a6b', 0, true);
        const aw = pick(r, AWNINGS);                                                                           // shops below
        if (along) for (const sgn of [1, -1]) { b.box(hx, 2.6, hz + sgn * (bd / 2 + 0.6), bw * 0.9, 0.35, 1.2, aw); b.box(hx, 0, hz + sgn * (bd / 2 + 0.03), bw * 0.8, 2.3, 0.1, '#bfe0f2'); }
        else for (const sgn of [1, -1]) { b.box(hx + sgn * (bd / 2 + 0.6), 2.6, hz, 1.2, 0.35, bw * 0.9, aw); b.box(hx + sgn * (bd / 2 + 0.03), 0, hz, 0.1, 2.3, bw * 0.8, '#bfe0f2'); }
      }
    },
    hoejhus(b, L, r, x, z, w, d) {
      b.box(x, 0, z, w, 0.12, d, '#cfd4da');
      const tw = w - 3, td = d - 3, floors = 9 + Math.floor(r() * 10) + Math.floor((w * d) / 120), fh = 3.2, glass = pick(r, ['#5a8fc4', '#4f7fae', '#6aa3c9', '#3f6f9a']);
      const body = pick(r, ['#e8ecef', '#d7dde3', '#f2e6d0', '#cfd9e0']);
      const stepAt = r() < 0.5 ? Math.floor(floors * (0.6 + r() * 0.2)) : floors;
      b.box(x, 0, z, tw, stepAt * fh, td, body); windows(b, x, z, tw, td, 0, stepAt, fh, glass);
      if (stepAt < floors) { const k = 0.7; b.box(x, stepAt * fh, z, tw * k, (floors - stepAt) * fh, td * k, body); windows(b, x, z, tw * k, td * k, stepAt * fh, floors - stepAt, fh, glass); }
      const top = floors * fh, kk = stepAt < floors ? 0.7 : 1;
      b.box(x, top, z, tw * kk * 0.6, 2, td * kk * 0.6, '#9aa4ad'); b.cyl(x + tw * kk * 0.15, top + 2, z, 0.12, 6, '#d94f4f');      // plant room and a red mast
      b.box(x, 0, z + td / 2 + 0.06, 3, 3, 0.15, '#bfe0f2'); b.box(x, 3, z + td / 2 + 0.8, 4, 0.3, 1.6, '#7d8790');                  // the entrance
    },
    raadhus(b, L, r, x, z, w, d) {
      b.box(x, 0, z, w, 0.12, d, '#d8cbb3');
      const bw = Math.min(w - 4, 30), bd = Math.min(d - 4, 18), fh = 4, brick = '#c8664a', trim = '#f1e7d6';
      b.box(x, 0, z, bw, fh * 2, bd, brick); b.box(x, fh * 2, z, bw + 0.4, 0.6, bd + 0.4, trim);
      b.roof(x, fh * 2 + 0.6, z, bw + 0.4, 4.5, bd + 0.4, '#5f6b74');
      windows(b, x, z, bw, bd, 0, 2, fh, '#f5e9c8');
      /* the tower, with a clock on each face and a copper roof */
      const th = 26, tw = 5;
      b.box(x, 0, z, tw, th, tw, brick); b.box(x, th, z, tw + 0.5, 0.6, tw + 0.5, trim);
      for (const [ox, oz, ry] of [[0, tw / 2 + 0.05, 0], [0, -tw / 2 - 0.05, 0], [tw / 2 + 0.05, 0, Math.PI / 2], [-tw / 2 - 0.05, 0, Math.PI / 2]]) {
        b.add(G.cyl, '#ffffff', x + ox, th - 3.5, z + oz, 0, 1.6, 0.12, 1.6); }
      for (const [ox, oz] of [[0, 1], [0, -1], [1, 0], [-1, 0]]) { b.box(x + ox * (tw / 2 + 0.12), th - 3.5, z + oz * (tw / 2 + 0.12), ox ? 0.1 : 0.25, 1.3, oz ? 0.1 : 0.25, '#222'); }
      b.cone(x, th + 0.6, z, tw * 0.62, 8, '#5fa38f', true); b.cyl(x, th + 8.6, z, 0.1, 3, '#444'); b.box(x + 0.9, th + 10.2, z, 1.6, 1, 0.08, '#d9534f');
      /* the steps and a door at the front */
      b.box(x, 0, z + bd / 2 + 1.2, 8, 0.6, 2.4, '#e7dfd0'); b.box(x, 0.6, z + bd / 2 + 0.06, 2.4, 3.4, 0.2, '#6a3e22');
    },
    kirke(b, L, r, x, z, w, d) {
      b.box(x, 0, z, w, 0.08, d, '#a9d98b');
      const along = w >= d, nl = Math.min((along ? w : d) - 4, 22), nw = Math.min((along ? d : w) - 5, 9), ry = along ? 0 : Math.PI / 2, white = '#f2efe6';
      const W2 = along ? nl : nw, D2 = along ? nw : nl;
      b.box(x, 0, z, W2, 8, D2, white); b.roof(x, 8, z, nl + 0.6, 6, nw + 0.8, '#c0503a', along ? 0 : Math.PI / 2);
      windows(b, x, z, W2, D2, 1, 1, 5, '#6f8fbf');
      const tx = along ? x - nl / 2 - 2 : x, tz = along ? z : z - nl / 2 - 2;
      b.box(tx, 0, tz, 4.5, 16, 4.5, white); b.cone(tx, 16, tz, 3.2, 11, '#c0503a', true); b.box(tx, 27, tz, 0.25, 2.4, 0.25, '#e8c35a'); b.box(tx, 28, tz, 1.2, 0.25, 0.25, '#e8c35a', ry);
      for (let k = 0; k < 3; k++) tree(b, x + (r() - 0.5) * (w - 4), z + (along ? 1 : -1) * (d / 2 - 2) * (r() < 0.5 ? 1 : -1), r, 0.8);
    },
    park(b, L, r, x, z, w, d, circles) {
      b.box(x, 0, z, w, 0.06, d, '#7cc46a');
      b.box(x, 0.06, z, w - 0.5, 0.02, 2.2, '#e3cf9e'); b.box(x, 0.06, z, 2.2, 0.02, d - 0.5, '#e3cf9e');                     // paths
      if (w >= 16 && d >= 16 && r() < 0.8) { const px = x + w / 4, pz = z - d / 4, pr = Math.min(w, d) / 6; b.cyl(px, 0.06, pz, pr, 0.05, '#5fb4e0'); b.cyl(px, 0.02, pz, pr + 0.5, 0.06, '#c9c4ba'); circles.push([px, pz, pr]); }
      const n = Math.round((w * d) / 40);
      for (let k = 0; k < n; k++) {
        const tx = x + (r() - 0.5) * (w - 3), tz = z + (r() - 0.5) * (d - 3);
        if (Math.abs(tx - x) < 2 || Math.abs(tz - z) < 2) continue;                                       // not on the paths
        if (circles.some(c => Math.hypot(c[0] - tx, c[1] - tz) < c[2] + 2)) continue;
        tree(b, tx, tz, r); circles.push([tx, tz, 0.5]);
      }
      for (const s of [-1, 1]) { b.box(x + s * 3, 0, z + 2, 1.8, 0.45, 0.5, '#8a5a3a'); b.box(x + s * 3, 0.45, z + 2.3, 1.8, 0.5, 0.12, '#8a5a3a'); }      // benches
    },
    torv(b, L, r, x, z, w, d, circles) {
      const tiles = Math.max(2, Math.round(w / 2)), tz = Math.max(2, Math.round(d / 2));
      b.box(x, 0, z, w, 0.06, d, '#d9c9a8');
      for (let i = 0; i < tiles; i++) for (let j = 0; j < tz; j++) if ((i + j) % 2) b.box(x - w / 2 + (i + 0.5) * w / tiles, 0.06, z - d / 2 + (j + 0.5) * d / tz, w / tiles, 0.01, d / tz, '#cbb995');
      /* the fountain: a basin, a column and a bowl */
      b.cyl(x, 0, z, 3, 0.7, '#bcb6aa'); b.cyl(x, 0.5, z, 2.6, 0.25, '#5fb4e0'); b.cyl(x, 0.7, z, 0.35, 2.2, '#bcb6aa'); b.cyl(x, 2.9, z, 1.1, 0.3, '#bcb6aa'); b.ball(x, 3.3, z, 0.5, '#9fd6f2', 1.6);
      circles.push([x, z, 3]);
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const lx = x + sx * (w / 2 - 1.2), lz = z + sz * (d / 2 - 1.2); b.cyl(lx, 0, lz, 0.1, 4, '#3c3c3c'); b.ball(lx, 4.2, lz, 0.35, '#fff4c2'); }
    },
  };
  const lotCircles = new Map();
  function buildLot(L) {
    const old = meshes.get(L.id); if (old) { root.remove(old); old.geometry.dispose(); }
    const b = new Builder(), r = rng(L.seed), w = (L.x1 - L.x0 + 1) * CELL, d = (L.z1 - L.z0 + 1) * CELL, x = (cx(L.x0) + cx(L.x1)) / 2, z = (cz(L.z0) + cz(L.z1)) / 2, circles = [];
    GEN[L.type](b, L, r, x, z, w, d, circles);
    lotCircles.set(L.id, circles);
    const g = b.build(); if (!g) return; const m = new THREE.Mesh(g, mat); m.userData.lot = L.id; meshes.set(L.id, m); root.add(m);
  }

  /* ---------- gates ---------- */
  const gateTex = new Map();
  function numberTex(label, bg) {
    const key = label + bg; if (gateTex.has(key)) return gateTex.get(key);
    const c = document.createElement('canvas'); c.width = 256; c.height = 64; const x = c.getContext('2d');
    if (label === 'START') { for (let i = 0; i < 16; i++) for (let j = 0; j < 4; j++) { x.fillStyle = (i + j) % 2 ? '#111' : '#fff'; x.fillRect(i * 16, j * 16, 16, 16); }
      x.fillStyle = 'rgba(255,255,255,.92)'; x.fillRect(70, 10, 116, 44); x.fillStyle = '#111'; x.font = 'bold 34px Arial, sans-serif'; x.textAlign = 'center'; x.fillText('MÅL', 128, 45); }
    else { x.fillStyle = bg; x.fillRect(0, 0, 256, 64); x.fillStyle = '#fff'; x.font = 'bold 44px Arial, sans-serif'; x.textAlign = 'center'; x.fillText('PORT ' + label, 128, 48); }
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; gateTex.set(key, t); return t;
  }
  /** the direction a gate faces (the road's axis): 'x' or 'z' */
  function gateAxis(g) { const ns = isRoad(g.i, g.j - 1) || isRoad(g.i, g.j + 1), ew = isRoad(g.i - 1, g.j) || isRoad(g.i + 1, g.j); return ew && !ns ? 'x' : 'z'; }
  function rebuildGates() {
    for (const c of [...gateGroup.children]) { gateGroup.remove(c); c.traverse(o => { if (o.geometry) o.geometry.dispose(); }); }
    st.gates = st.gates.filter(g => isRoad(g.i, g.j));
    let n = 0;
    for (const g of st.gates) {
      const grp = new THREE.Group(), b = new Builder(), col = g.start ? '#f4f1e6' : '#ff9a3c', H = 6.5;
      for (const s of [-1, 1]) { b.box(s * (CELL / 2 - 0.5), 0, 0, 0.6, H, 0.6, col); b.ball(s * (CELL / 2 - 0.5), H + 0.3, 0, 0.45, col); }
      grp.add(new THREE.Mesh(b.build(), mat));
      const label = g.start ? 'START' : String(++n);
      for (const back of [0, 1]) {                                                    /* two faces, so it reads the right way from both sides */
        const banner = new THREE.Mesh(new THREE.PlaneGeometry(CELL - 1.2, 1.8), new THREE.MeshBasicMaterial({ map: numberTex(label, '#e8742a') }));
        banner.position.set(0, H - 1.1, back ? -0.03 : 0.03); banner.rotation.y = back ? Math.PI : 0; grp.add(banner); }
      grp.position.set(cx(g.i), 0, cz(g.j)); grp.rotation.y = gateAxis(g) === 'x' ? Math.PI / 2 : 0;
      g.n = g.start ? 0 : n; gateGroup.add(grp);
    }
  }

  /* ---------- driving: colliders, surfaces, routes ---------- */
  function rebuildColliders() {
    colliders.rects.length = 0; colliders.circles.length = 0;
    for (const L of st.lots.values()) {
      if (TOOL[L.type].solid) colliders.rects.push([cx(L.x0) - CELL / 2 + 1.1, cz(L.z0) - CELL / 2 + 1.1, cx(L.x1) + CELL / 2 - 1.1, cz(L.z1) + CELL / 2 - 1.1]);
      for (const c of lotCircles.get(L.id) || []) colliders.circles.push(c);
    }
    for (const g of st.gates) { const ax = gateAxis(g), x = cx(g.i), z = cz(g.j); for (const s of [-1, 1]) colliders.circles.push(ax === 'x' ? [x, z + s * (CELL / 2 - 0.5), 0.4] : [x + s * (CELL / 2 - 0.5), z, 0.4]); }
  }
  /** what is under a point: 'road', 'torv', 'park', 'grass', 'lot' (a solid lot's pavement), or 'out' */
  function surfaceAt(x, z) {
    const [i, j] = cellAt(x, z); if (!inside(i, j)) return 'out';
    const k = idx(i, j); if (st.cells[k]) return 'road';
    const L = st.lot[k] && st.lots.get(st.lot[k]); if (!L) return 'grass';
    return L.type === 'torv' ? 'torv' : L.type === 'park' ? 'park' : 'lot';
  }
  /** push a circle (x, z, r) out of everything solid; returns the push normal (or null) */
  const _n = new THREE.Vector2();
  function collide(p, r) {
    let hit = null;
    const W = st.W * CELL / 2 - r, H = st.H * CELL / 2 - r;
    if (p.x > W) { p.x = W; hit = _n.set(-1, 0); } if (p.x < -W) { p.x = -W; hit = _n.set(1, 0); }
    if (p.y > H) { p.y = H; hit = _n.set(0, -1); } if (p.y < -H) { p.y = -H; hit = _n.set(0, 1); }
    for (const [x0, z0, x1, z1] of colliders.rects) {
      const qx = Math.max(x0, Math.min(x1, p.x)), qz = Math.max(z0, Math.min(z1, p.y)), dx = p.x - qx, dz = p.y - qz, d2 = dx * dx + dz * dz;
      if (d2 >= r * r) continue;
      if (d2 > 1e-9) { const d = Math.sqrt(d2); p.x = qx + dx / d * r; p.y = qz + dz / d * r; hit = _n.set(dx / d, dz / d); }
      else { const o = [p.x - x0, x1 - p.x, p.y - z0, z1 - p.y], m = Math.min(...o), k = o.indexOf(m);       // inside: out by the nearest side
        if (k === 0) { p.x = x0 - r; hit = _n.set(-1, 0); } else if (k === 1) { p.x = x1 + r; hit = _n.set(1, 0); } else if (k === 2) { p.y = z0 - r; hit = _n.set(0, -1); } else { p.y = z1 + r; hit = _n.set(0, 1); } }
    }
    for (const [x, z, cr] of colliders.circles) {
      const dx = p.x - x, dz = p.y - z, d = Math.hypot(dx, dz), rr = r + cr;
      if (d < rr && d > 1e-6) { p.x = x + dx / d * rr; p.y = z + dz / d * rr; hit = _n.set(dx / d, dz / d); }
    }
    return hit;
  }
  const drivable = (i, j) => { if (!inside(i, j)) return false; const k = idx(i, j); if (st.cells[k]) return true; const L = st.lot[k] && st.lots.get(st.lot[k]); return !!L && L.type === 'torv'; };
  function bfs(a, b) {
    const prev = new Int32Array(st.W * st.H).fill(-2), q = [idx(a.i, a.j)]; prev[q[0]] = -1;
    for (let h = 0; h < q.length; h++) {
      const k = q[h]; if (k === idx(b.i, b.j)) break; const i = k % st.W, j = (k / st.W) | 0;
      for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const ni = i + di, nj = j + dj; if (!drivable(ni, nj)) continue; const nk = idx(ni, nj); if (prev[nk] !== -2) continue; prev[nk] = k; q.push(nk); }
    }
    const end = idx(b.i, b.j); if (prev[end] === -2) return null;
    const out = []; for (let k = end; k !== -1; k = prev[k]) out.push(k); out.reverse(); return out;
  }
  /** The race: the start, then the numbered gates in order, then the start again. Returns { gates, line } where line is a closed, rounded list of
   *  points through the gates (for the opponents), or { error } in Danish. */
  function route() {
    const start = st.gates.find(g => g.start); if (!start) return { error: 'Sæt en start/mål på en vej.' };
    const seq = [start, ...st.gates.filter(g => !g.start)];
    if (seq.length < 2) return { error: 'Sæt mindst én port på vejene (efter start/mål).' };
    let cells = [];
    for (let k = 0; k < seq.length; k++) {
      const p = bfs(seq[k], seq[(k + 1) % seq.length]);
      if (!p) return { error: k + 1 < seq.length ? `Port ${k + 1} kan ikke nås ad vejene.` : 'Der er ingen vej fra den sidste port tilbage til mål.' };
      cells = cells.concat(k ? p.slice(1) : p);
    }
    cells.pop();                                                                       // closed: the last is the start again
    if (cells.length < 4) return { error: 'Banen er for kort: læg portene længere fra hinanden.' };
    let pts = cells.map(k => new THREE.Vector2(cx(k % st.W), cz((k / st.W) | 0)));
    for (let it = 0; it < 2; it++) {                                                   // round the corners (Chaikin), closed
      const o = []; for (let k = 0; k < pts.length; k++) { const a = pts[k], b2 = pts[(k + 1) % pts.length]; o.push(a.clone().lerp(b2, 0.25), a.clone().lerp(b2, 0.75)); } pts = o; }
    /* where each gate falls on the line */
    const gateAt = seq.map(g => { const gx = cx(g.i), gz = cz(g.j); let best = 0, bd = 1e9; pts.forEach((q, k) => { const dd = Math.hypot(q.x - gx, q.y - gz); if (dd < bd) { bd = dd; best = k; } }); return best; });
    return { gates: seq.map(g => ({ i: g.i, j: g.j, x: cx(g.i), z: cz(g.j), start: g.start, axis: gateAxis(g) })), line: pts, gateAt, startCells: cells };
  }

  /* ---------- saving ---------- */
  function save() { return JSON.stringify({ v: 1, W: st.W, H: st.H, cells: Array.from(st.cells), lots: [...st.lots.values()], gates: st.gates.map(g => ({ i: g.i, j: g.j, start: g.start })), nextId: st.nextId }); }
  function load(json) {
    let o; try { o = typeof json === 'string' ? JSON.parse(json) : json; } catch (_) { return false; }
    if (!o || o.v !== 1 || !o.W || !o.H) return false;
    reset(o.W, o.H); st.cells.set(o.cells.slice(0, o.W * o.H)); st.nextId = o.nextId || 1;
    for (const L of o.lots) { if (!GEN[L.type]) continue; st.lots.set(L.id, L); for (let j = L.z0; j <= L.z1; j++) for (let i = L.x0; i <= L.x1; i++) if (inside(i, j)) st.lot[idx(i, j)] = L.id; buildLot(L); st.nextId = Math.max(st.nextId, L.id + 1); }
    st.gates = (o.gates || []).map(g => ({ i: g.i, j: g.j, start: !!g.start })); commit(); return true;
  }
  /** a first town, so there is something to race in at once: a ring road with a cross street, houses, a park, a square, a town hall and a tower */
  function starter() {
    const W = st.W, H = st.H, m = 1, x1 = W - 2, z1 = H - 2, mid = Math.floor(W / 2), midz = Math.floor(H / 2);
    for (let i = m; i <= x1; i++) { paintRoad(i, m); paintRoad(i, z1); } for (let j = m; j <= z1; j++) { paintRoad(m, j); paintRoad(x1, j); }
    for (let j = m; j <= z1; j++) paintRoad(mid, j);
    const inner = (a, b, c, d, t) => { if (c >= a && d >= b) placeLot(t, a, b, c, d); };
    inner(m + 1, m + 1, mid - 1, midz - 1, 'huse'); inner(mid + 1, m + 1, x1 - 1, m + 2, 'byhus'); inner(mid + 1, m + 3, mid + 2, midz, 'raadhus');
    inner(mid + 3, m + 3, x1 - 1, midz, 'torv'); inner(m + 1, midz, mid - 1, z1 - 1, 'park'); inner(mid + 1, midz + 1, mid + 1, z1 - 1, 'hoejhus');
    inner(mid + 2, midz + 1, x1 - 1, z1 - 1, 'byhus');
    /* the outer edge: a few houses and a church, so the city does not stop at a road */
    setGate(mid - 2, z1, true); setGate(x1, midz, false); setGate(mid, m, false); setGate(m, midz, false);      // round the ring, anticlockwise seen from above
    commit();
  }

  /** The land round the city, for driving: fields to the horizon, a hedge along the board's edge, a ring of trees and far hills. */
  function scenery() {
    const b = new Builder(), r = rng(7), W = st.W * CELL / 2, H = st.H * CELL / 2;
    b.add(new THREE.CircleGeometry(1, 48).rotateX(-Math.PI / 2), '#86c467', 0, -0.07, 0, 0, 2500, 1, 2500);
    for (const [x, z, w, d] of [[0, -H - 0.8, 2 * W + 3.2, 1.2], [0, H + 0.8, 2 * W + 3.2, 1.2], [-W - 0.8, 0, 1.2, 2 * H + 3.2], [W + 0.8, 0, 1.2, 2 * H + 3.2]]) b.box(x, -0.07, z, w, 1.3, d, '#4f9440');
    for (let k = 0; k < 220; k++) {
      const a = r() * Math.PI * 2, rad = 8 + r() * 70, x = Math.cos(a) * (W + rad), z = Math.sin(a) * (H + rad);
      tree(b, x, z, r, 1 + r() * 0.8);
    }
    for (let k = 0; k < 16; k++) { const a = k / 16 * Math.PI * 2 + r() * 0.3, d = 520 + r() * 380, h = 60 + r() * 120;
      b.cone(Math.cos(a) * d, -1, Math.sin(a) * d, 160 + r() * 140, h, pick(r, ['#7fb26a', '#6fa363', '#8fbf77', '#7aa7a0'])); }
    const m = new THREE.Mesh(b.build(), mat); m.name = 'scenery'; return m;
  }

  reset(20, 14);
  return { scenery,
    root, st, reset, paintRoad, placeLot, eraseRect, setGate, commit, save, load, starter, route, collide, surfaceAt, cellAt, inside, isRoad, lotOK,
    cx, cz, colliders, gridLines: () => gridLines, ground: () => groundMesh, mat,
  };
}
