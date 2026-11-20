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
 * The look is a cartoon: flat colours in three tones and black ink round everything that stands up (toon.js); flat things that lie on others (asphalt,
 * markings, paving patterns) go in a second, un-inked mesh so the town is not scribbled over.
 *
 * Driving: the solid lots are boxes the cars bounce off (the lot less its pavement); parks have trees, squares a fountain (circles); parks and grass are
 * slow, roads and squares fast. The opponents' route is found through roads and squares only (breadth first, then rounded).
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';
import { toonMat, ink } from './toon.js?v=2';

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
const darkenHex = (h, k) => '#' + new THREE.Color(h).lerp(new THREE.Color(0, 0, 0), k).getHexString();

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
  /** a box tilted about its local z (for a slide) */
  boxRot(x, y, z, w, h, d, color, ry, rz) { let g = G.box.toNonIndexed(); g.applyMatrix4(new THREE.Matrix4().makeScale(w, h, d)); g.applyMatrix4(new THREE.Matrix4().makeRotationZ(rz)); g.applyMatrix4(new THREE.Matrix4().makeRotationY(ry)); g.translate(x, y, z); return this.add(g, color); }
  build() { if (!this.parts.length) return null; const g = mergeGeometries(this.parts, false); g.computeBoundingSphere(); return g; }
}
const G = {
  box: new THREE.BoxGeometry(1, 1, 1),
  cyl: new THREE.CylinderGeometry(1, 1, 1, 10),
  cyl6: new THREE.CylinderGeometry(1, 1, 1, 6),
  cone: new THREE.ConeGeometry(1, 1, 10),
  pyr: new THREE.ConeGeometry(1.4142, 1, 4),
  ball: new THREE.IcosahedronGeometry(1, 1),
  plane: new THREE.PlaneGeometry(1, 1).rotateX(-Math.PI / 2),
  cylZ: new THREE.CylinderGeometry(1, 1, 1, 20).rotateX(Math.PI / 2),
  prism: (() => { const s = new THREE.Shape(); s.moveTo(-0.5, 0); s.lineTo(0.5, 0); s.lineTo(0, 1); s.closePath();
    const g = new THREE.ExtrudeGeometry(s, { depth: 1, bevelEnabled: false }); g.translate(0, 0, -0.5); g.rotateY(Math.PI / 2); return g; })(),      // ridge along x, gables at ±x
};
G.prism.computeVertexNormals();

/* ---- trees, windows: shared bits ---- */
function tree(b, x, z, r, s = 1) {
  const kind = r(), h = (3 + r() * 2.5) * s;
  b.cyl(x, 0, z, 0.28 * s, h * 0.5, '#8a5a34', 'cyl6');
  if (kind < 0.6) { const g = pick(r, ['#5cb84a', '#6cc552', '#4fa845']); b.ball(x, h * 0.66, z, 1.6 * s, g, 1.0); b.ball(x + 0.8 * s, h * 0.6, z - 0.3 * s, 1.0 * s, g); b.ball(x - 0.5 * s, h * 0.82, z + 0.5 * s, 1.05 * s, '#7fd462'); }
  else if (kind < 0.85) { b.cone(x, h * 0.25, z, 1.5 * s, h * 0.6, '#3d9a52'); b.cone(x, h * 0.55, z, 1.1 * s, h * 0.55, '#47ab5c'); }
  else { b.ball(x, h * 0.7, z, 1.5 * s, '#f2a7c3', 1.0); b.ball(x + 0.7 * s, h * 0.62, z + 0.4 * s, 0.9 * s, '#f7c2d6'); }      // a blossoming one
}
/** windows round a box: framed panes per floor, a few per side; flower boxes under some ground-floor windows */
function windows(b, x, z, w, d, y0, floors, fh, color, ry = 0, skipGround = false, r = null, frame = '#ffffff') {
  const c = Math.cos(ry), s = Math.sin(ry);
  const put = (lx, y, lz, ww, hh, dd, col) => b.box(x + lx * c + lz * s, y, z - lx * s + lz * c, ww, hh, dd, col, ry);
  for (let f = skipGround ? 1 : 0; f < floors; f++) {
    const y = y0 + f * fh + fh * 0.28, wh = fh * 0.46;
    const nx = Math.max(1, Math.floor(w / 2.5)), nz = Math.max(1, Math.floor(d / 2.5));
    const one = (lx, lz, ax) => {                                                    // ax: the window faces along x (true) or z
      const out = ax ? Math.sign(lx) : Math.sign(lz);
      if (ax) { put(lx + out * 0.03, y - 0.12, lz, 0.1, wh + 0.24, 1.4, frame); put(lx + out * 0.09, y, lz, 0.06, wh, 1.1, color); put(lx + out * 0.1, y, lz, 0.06, wh, 0.1, frame); }
      else { put(lx, y - 0.12, lz + out * 0.03, 1.4, wh + 0.24, 0.1, frame); put(lx, y, lz + out * 0.09, 1.1, wh, 0.06, color); put(lx, y, lz + out * 0.1, 0.1, wh, 0.06, frame); }
      if (r && f === 0 && r() < 0.45) { const fl = pick(r, ['#ff5d73', '#ffb13b', '#c86bff', '#ff7ab8']);
        if (ax) { put(lx + out * 0.3, y - 0.42, lz, 0.45, 0.3, 1.3, '#8a5a3a'); for (let k = -1; k <= 1; k++) b.ball(x + (lx + out * 0.3) * c + (lz + k * 0.4) * s, y - 0.02, z - (lx + out * 0.3) * s + (lz + k * 0.4) * c, 0.17, fl); }
        else { put(lx, y - 0.42, lz + out * 0.3, 1.3, 0.3, 0.45, '#8a5a3a'); for (let k = -1; k <= 1; k++) b.ball(x + (lx + k * 0.4) * c + (lz + out * 0.3) * s, y - 0.02, z - (lx + k * 0.4) * s + (lz + out * 0.3) * c, 0.17, fl); } }
    };
    for (let k = 0; k < nx; k++) { const lx = -w / 2 + (k + 0.5) * w / nx; one(lx, d / 2, false); one(lx, -d / 2, false); }
    for (let k = 0; k < nz; k++) { const lz = -d / 2 + (k + 0.5) * d / nz; one(w / 2, lz, true); one(-w / 2, lz, true); }
  }
}
/** a street lamp: a dark green post, a curved arm and a warm globe */
function lamp(b, x, z, ax = 0, az = 0) { b.cyl(x, 0, z, 0.12, 4.6, '#2f5d4a', 'cyl6'); b.box(x + ax * 0.5, 4.5, z + az * 0.5, ax ? 1.0 : 0.16, 0.16, az ? 1.0 : 0.16, '#2f5d4a'); b.ball(x + ax * 0.95, 4.35, z + az * 0.95, 0.32, '#fff2b0'); }
function bench(b, x, z, ry) { b.add(G.box, '#a8693f', x, 0.45, z, ry, 1.8, 0.1, 0.55); b.add(G.box, '#a8693f', x - Math.sin(ry) * 0.25, 0.75, z - Math.cos(ry) * 0.25, ry, 1.8, 0.45, 0.1); b.add(G.box, '#333', x, 0.2, z, ry, 1.6, 0.4, 0.4); }
/** a market stall: a table and a striped roof on posts */
function stall(b, x, z, ry, r) {
  const a = pick(r, AWNINGS); b.add(G.box, '#c9a27a', x, 0.45, z, ry, 2.4, 0.9, 1.3);
  for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const c = Math.cos(ry), s = Math.sin(ry), lx = sx * 1.15, lz = sz * 0.6; b.cyl(x + lx * c + lz * s, 0, z - lx * s + lz * c, 0.06, 2.4, '#6b4a2a', 'cyl6'); }
  for (let k = 0; k < 4; k++) b.add(G.prism, k % 2 ? '#ffffff' : a, x + Math.cos(ry) * (-0.9 + k * 0.6), 2.4, z - Math.sin(ry) * (-0.9 + k * 0.6), ry + Math.PI / 2, 1.6, 0.7, 0.6);
  for (let k = 0; k < 5; k++) b.ball(x + Math.cos(ry) * (-0.9 + k * 0.45), 1.02, z - Math.sin(ry) * (-0.9 + k * 0.45), 0.2, pick(r, ['#ff5d3b', '#ffd23b', '#7ccf4a', '#ff8a3b']));
}

export function createCity() {
  const root = new THREE.Group(); root.name = 'city';
  const mat = toonMat();
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
    groundMesh = ink(new THREE.Mesh(b.build(), mat)); root.add(groundMesh);
    const gb = new Builder(); gb.box(0, 0, 0, W, 0.015, H, '#8fcf6e');                         // grass: too thin to ink (its line would show through it)
    groundMesh.add(new THREE.Mesh(gb.build(), mat));
    /* the grid, for drawing */
    if (gridLines) { root.remove(gridLines); gridLines.geometry.dispose(); }
    const pts = [];
    for (let i = 0; i <= st.W; i++) { const x = (i - st.W / 2) * CELL; pts.push(x, 0.05, -H / 2, x, 0.05, H / 2); }
    for (let j = 0; j <= st.H; j++) { const z = (j - st.H / 2) * CELL; pts.push(-W / 2, 0.05, z, W / 2, 0.05, z); }
    const gg = new THREE.BufferGeometry(); gg.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    gridLines = new THREE.LineSegments(gg, new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.22 })); root.add(gridLines);
  }
  let flatRoad = null; const roadCircles = [];
  function rebuildRoads() {
    if (roadMesh) { root.remove(roadMesh); roadMesh.geometry.dispose(); roadMesh = null; }
    if (flatRoad) { root.remove(flatRoad); flatRoad.geometry.dispose(); flatRoad = null; }
    roadCircles.length = 0;
    const b = new Builder(), f = new Builder(), h = CELL / 2, pw = 1.4, PAVE = '#d8d2c6', KERB = '#bcb5a8';
    for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) {
      if (!isRoad(i, j)) continue;
      const x = cx(i), z = cz(j), N = isRoad(i, j - 1), S = isRoad(i, j + 1), E = isRoad(i + 1, j), Wd = isRoad(i - 1, j), n = N + S + E + Wd;
      f.add(G.plane, '#4b515c', x, 0.02, z, 0, CELL, 1, CELL);                                   // asphalt (no ink)
      /* pavements with a kerb where the road ends at a side; a lamp on every other one */
      const sides = [[!N, 0, -1], [!S, 0, 1], [!E, 1, 0], [!Wd, -1, 0]];
      for (const [on, dx, dz] of sides) {
        if (!on) continue;
        if (dz) { b.box(x, 0, z + dz * (h - pw / 2), CELL, 0.18, pw, PAVE); b.box(x, 0, z + dz * (h - pw - 0.08), CELL, 0.2, 0.18, KERB); }
        else { b.box(x + dx * (h - pw / 2), 0, z, pw, 0.18, CELL, PAVE); b.box(x + dx * (h - pw - 0.08), 0, z, 0.18, 0.2, CELL, KERB); }
      }
      const lampSide = sides.find(sd => sd[0]);
      if (lampSide && (i + j) % 2 === 0 && n <= 2) { const [, dx, dz] = lampSide, lx = x + dx * (h - 0.6) + (dz ? -2 : 0), lz = z + dz * (h - 0.6) + (dx ? -2 : 0); lamp(b, lx, lz, -dx, -dz); roadCircles.push([lx, lz, 0.25]); }
      else if (lampSide && (i * 7 + j * 13) % 9 === 0) { const [, dx, dz] = lampSide, hx = x + dx * (h - 0.7) + (dz ? 2.5 : 0), hz = z + dz * (h - 0.7) + (dx ? 2.5 : 0); b.cyl(hx, 0.18, hz, 0.2, 0.7, '#e2382f', 'cyl6'); b.ball(hx, 0.92, hz, 0.2, '#e2382f'); }      // a fire hydrant
      for (const [dx, dz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) {
        const a = dx > 0 ? E : Wd, c = dz > 0 ? S : N;
        if (a && c && !isRoad(i + dx, j + dz)) {
          const px = x + dx * (h - pw / 2), pz = z + dz * (h - pw / 2); b.box(px, 0, pz, pw, 0.18, pw, PAVE);
          if (n >= 3) {                                                                           // a traffic light on the corner
            b.cyl(px, 0.18, pz, 0.1, 3.4, '#2a2a2e', 'cyl6'); b.box(px, 3.2, pz, 0.45, 1.3, 0.45, '#1f1f24');
            for (const [k, col] of [[0, '#ff4a3d'], [1, '#ffcf3d'], [2, '#4ade6b']]) b.ball(px - dx * 0.24, 4.25 - k * 0.4, pz - dz * 0.24, 0.13, col);
            roadCircles.push([px, pz, 0.25]);
          }
        }
      }
      const straightZ = (N || S) && !E && !Wd, straightX = (E || Wd) && !N && !S;
      if (straightZ) for (const o of [-2, 2]) f.add(G.plane, '#f4f1e6', x, 0.035, z + o, 0, 0.22, 1, 2);
      else if (straightX) for (const o of [-2, 2]) f.add(G.plane, '#f4f1e6', x + o, 0.035, z, 0, 2, 1, 0.22);
      else if (n >= 3) {                                                                // a junction: zebra crossings just outside it, on the roads leading in
        for (const [on, dx, dz] of [[N, 0, -1], [S, 0, 1], [E, 1, 0], [Wd, -1, 0]]) {
          if (!on) continue;
          for (let k = -2; k <= 2; k++) {
            if (dz) f.add(G.plane, '#f4f1e6', x + k * 1.25, 0.035, z + dz * (h - 0.9), 0, 0.6, 1, 1.4);
            else f.add(G.plane, '#f4f1e6', x + dx * (h - 0.9), 0.035, z + k * 1.25, 0, 1.4, 1, 0.6);
          }
        }
        f.cyl(x, 0.025, z, 0.5, 0.02, '#3d424b');                                           // a manhole cover
      } else if (n === 2) {                                                             // a bend: a dashed quarter circle round the inside
        const ox = E ? 1 : -1, oz = S ? 1 : -1;
        for (let k = 0; k < 3; k++) { const a = (k + 0.5) / 3 * Math.PI / 2, px = x + ox * h - ox * h * Math.cos(a), pz = z + oz * h - oz * h * Math.sin(a);
          f.add(G.plane, '#f4f1e6', px, 0.035, pz, 0, 0.5, 1, 0.5); }
      }
    }
    const g = b.build(); if (g) { roadMesh = ink(new THREE.Mesh(g, mat)); root.add(roadMesh); }
    const g2 = f.build(); if (g2) { flatRoad = new THREE.Mesh(g2, mat); root.add(flatRoad); }
  }

  /* lots, one generator per kind; (x, z) the lot's centre, w × d its size in metres */
  const GEN = {
    huse(b, L, r, x, z, w, d, circles) {
      b.flat.box(x, 0, z, w - 0.4, 0.06, d - 0.4, '#9fd67e');
      const along = w >= d, len = along ? w : d, dep = along ? d : w, n = Math.max(1, Math.round(len / 7.5)), step = len / n;
      /* a low hedge round the gardens, with a gap at each house's path */
      for (const sgn of [-1, 1]) { if (along) b.box(x, 0, z + sgn * (d / 2 - 0.5), w - 1, 0.9, 0.6, '#4f9a3f'); else b.box(x + sgn * (w / 2 - 0.5), 0, z, 0.6, 0.9, d - 1, '#4f9a3f'); }
      for (let k = 0; k < n; k++) {
        const hw = step - 1.4, hd = Math.min(dep - 3.5, 7 + r() * 2), wall = pick(r, WALLS), roofc = pick(r, ROOFS), floors = r() < 0.4 ? 2 : 1, hh = floors * 3 + 0.4;
        const off = -len / 2 + (k + 0.5) * step, hx = along ? x + off : x, hz = along ? z : z + off, ry = along ? 0 : Math.PI / 2;
        const W2 = along ? hw : hd, D2 = along ? hd : hw, rh = 2.6 + r();
        b.box(hx, 0, hz, W2, 0.4, D2, '#b7aa98');                                                         // plinth
        b.box(hx, 0.4, hz, W2, hh - 0.4, D2, wall);
        b.roof(hx, hh, hz, hw + 0.7, rh, hd + 0.9, roofc, ry);
        b.add(G.box, darkenHex(roofc, 0.25), hx, hh + rh - 0.05, hz, ry, hw + 0.8, 0.22, 0.3);               // ridge
        windows(b, hx, hz, W2, D2, 0, floors, 3, '#7fb7e6', 0, false, r);
        if (r() < 0.75) { const cxp = hx + (r() - 0.5) * W2 * 0.5, czp = hz + (r() - 0.5) * D2 * 0.3; b.box(cxp, hh + 0.6, czp, 0.75, 2.2, 0.75, '#b1543f'); b.box(cxp, hh + 2.8, czp, 0.95, 0.2, 0.95, '#8b3e2e'); }
        /* a door with a frame and a step on each long face, and a path to the hedge */
        for (const sgn of [-1, 1]) {
          const dx = along ? 0 : sgn * (hd / 2 + 0.06), dz = along ? sgn * (hd / 2 + 0.06) : 0, ry2 = along ? 0 : Math.PI / 2;
          b.add(G.box, '#ffffff', hx + dx, 1.2, hz + dz, ry2, 1.5, 2.5, 0.12); b.add(G.box, pick(r, ['#c0392b', '#2e6fb7', '#2f8f4e', '#7a4a2a', '#e0a019']), hx + dx * 1.02, 1.15, hz + dz * 1.02, ry2, 1.1, 2.2, 0.14);
          b.ball(hx + dx * 1.05 + (along ? 0.35 : 0), 1.15, hz + dz * 1.05 + (along ? 0 : 0.35), 0.07, '#e8c35a');
          b.add(G.box, '#c9c4ba', hx + dx * 1.12, 0.12, hz + dz * 1.12, ry2, 1.8, 0.24, 0.6);
          const pathLen = dep / 2 - hd / 2 - 0.4; b.flat.add(G.plane, '#e3cf9e', hx + (along ? 0 : sgn * (hd / 2 + pathLen / 2)), 0.08, hz + (along ? sgn * (hd / 2 + pathLen / 2) : 0), 0, along ? 1.1 : pathLen, 1, along ? pathLen : 1.1);
        }
        if (r() < 0.5) { const mx = hx + (along ? 1.2 : (dep / 2 - 1.1) * (r() < 0.5 ? 1 : -1)), mz = hz + (along ? (dep / 2 - 1.1) * (r() < 0.5 ? 1 : -1) : 1.2); b.cyl(mx, 0, mz, 0.06, 1.0, '#555', 'cyl6'); b.box(mx, 1.0, mz, 0.4, 0.35, 0.55, '#d93a2b'); }      // a letterbox
      }
      if (dep > 12) for (let k = 0; k < Math.floor(len / 9); k++) { const o = -len / 2 + 4 + k * 9 + r() * 2, sd = (dep / 2 - 2) * (r() < 0.5 ? 1 : -1);
        const tx = along ? x + o : x + sd, tz = along ? z + sd : z + o; tree(b, tx, tz, r, 0.8); }
    },
    byhus(b, L, r, x, z, w, d) {
      b.flat.box(x, 0, z, w, 0.12, d, '#d8d2c6');
      const along = w >= d, len = along ? w : d, dep = along ? d : w, n = Math.max(1, Math.round(len / 9)), step = len / n;
      for (let k = 0; k < n; k++) {
        const floors = 3 + Math.floor(r() * 3), fh = 3.1, hh = floors * fh, wall = pick(r, WALLS), off = -len / 2 + (k + 0.5) * step;
        const bw = step - 0.3, bd = dep - 2.4, hx = along ? x + off : x, hz = along ? z : z + off, W2 = along ? bw : bd, D2 = along ? bd : bw, ry = along ? 0 : Math.PI / 2;
        b.box(hx, 0, hz, W2, hh, D2, wall);
        b.box(hx, fh - 0.25, hz, W2 + 0.2, 0.3, D2 + 0.2, '#efe9de');                                    // a band over the shops
        b.box(hx, hh, hz, W2 + 0.4, 0.55, D2 + 0.4, '#efe9de');                                           // cornice
        const top = r(); if (top < 0.35) b.roof(hx, hh + 0.55, hz, bw, 2.4, bd, pick(r, ROOFS), ry);
        else if (top < 0.7) { const tx = hx + (r() - 0.5) * W2 * 0.4, tz = hz + (r() - 0.5) * D2 * 0.4; for (const [a, c] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) b.cyl(tx + a * 0.7, hh + 0.55, tz + c * 0.7, 0.07, 1.2, '#5a4a3a', 'cyl6'); b.cyl(tx, hh + 1.75, tz, 1.1, 1.8, '#8a6a4a'); b.cone(tx, hh + 3.55, tz, 1.2, 0.8, '#6a4e36'); }      // a water tank on legs
        else for (let q = 0; q < 2; q++) b.box(hx + (r() - 0.5) * W2 * 0.5, hh + 0.55, hz + (r() - 0.5) * D2 * 0.5, 1.4, 0.9, 1.0, '#b9c0c6');      // air-conditioning boxes
        windows(b, hx, hz, W2, D2, 0, floors, fh, '#7fb7e6', 0, true, null, '#fffaf0');
        /* balconies on some upper floors, on the street faces */
        if (r() < 0.6) for (let f = 1; f < floors; f += 2) for (const sgn of [1, -1]) {
          const bx = along ? hx : hx + sgn * (bd / 2 + 0.55), bz = along ? hz + sgn * (bd / 2 + 0.55) : hz, lw = Math.min(3.2, bw * 0.4);
          b.add(G.box, '#e9e4da', bx, f * fh + 0.05, bz, ry, lw, 0.14, 1.1); b.add(G.box, '#3a3d44', bx + (along ? 0 : sgn * 0.5), f * fh + 0.55, bz + (along ? sgn * 0.5 : 0), ry, lw, 0.9, 0.06);
        }
        /* shops below: a striped awning, a window and a sign */
        const aw = pick(r, AWNINGS), sign = pick(r, ['#2e6fb7', '#c0392b', '#2f8f4e', '#7d3c98', '#e67e22']);
        for (const sgn of [1, -1]) {
          const ox = along ? 0 : sgn * (bd / 2), oz = along ? sgn * (bd / 2) : 0, ns = Math.max(2, Math.round(bw * 0.9 / 0.8));
          for (let q = 0; q < ns; q++) { const t = -bw * 0.45 + (q + 0.5) * bw * 0.9 / ns; b.add(G.prism, q % 2 ? '#ffffff' : aw, hx + ox * 1.0 + (along ? t : sgn * 0.6), 2.25, hz + oz * 1.0 + (along ? sgn * 0.6 : t), ry + Math.PI / 2, 1.3, 0.55, bw * 0.9 / ns); }
          b.add(G.box, '#bfe0f2', hx + ox * 1.01, 0.3, hz + oz * 1.01, ry, along ? bw * 0.8 : 0.1, 1.9, along ? 0.1 : bw * 0.8);
          b.add(G.box, sign, hx + ox * 1.02, 2.95, hz + oz * 1.02, ry, along ? bw * 0.5 : 0.14, 0.6, along ? 0.14 : bw * 0.5);
          b.add(G.box, '#ffffff', hx + ox * 1.03, 3.15, hz + oz * 1.03, ry, along ? bw * 0.35 : 0.15, 0.12, along ? 0.15 : bw * 0.35);
        }
      }
    },
    hoejhus(b, L, r, x, z, w, d, circles) {
      b.flat.box(x, 0, z, w, 0.12, d, '#cfd4da');
      const tw = w - 4, td = d - 4, floors = 9 + Math.floor(r() * 10) + Math.floor((w * d) / 120), fh = 3.2, glass = pick(r, ['#6fa8dc', '#5d9bd5', '#7cc3e8', '#4f8fcf']);
      const body = pick(r, ['#f2f4f6', '#e3e8ee', '#f6ead6', '#dfe9ef']), trim = pick(r, ['#ff7a59', '#4cb1a0', '#f2c230', '#7d6bd8']);
      const stepAt = r() < 0.5 ? Math.floor(floors * (0.6 + r() * 0.2)) : floors;
      const block = (bw, bd, y0, nf) => {
        b.box(x, y0, z, bw, nf * fh, bd, body);
        windows(b, x, z, bw, bd, y0, nf, fh, glass, 0, false, null, body);
        for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) b.box(x + sx * (bw / 2 - 0.25), y0, z + sz * (bd / 2 - 0.25), 0.7, nf * fh, 0.7, trim);      // coloured corner piers
        b.box(x, y0 + nf * fh, z, bw + 0.5, 0.5, bd + 0.5, trim);
      };
      block(tw, td, 0, stepAt); const kk = stepAt < floors ? 0.7 : 1; if (stepAt < floors) block(tw * kk, td * kk, stepAt * fh + 0.5, floors - stepAt);
      const top = floors * fh + (stepAt < floors ? 1 : 0.5);
      if (r() < 0.5) { b.cyl(x, top, z, Math.min(tw, td) * kk * 0.32, 0.25, '#4a4e57'); b.box(x - 0.9, top + 0.25, z, 0.35, 0.02, 2.4, '#f2c230'); b.box(x + 0.9, top + 0.25, z, 0.35, 0.02, 2.4, '#f2c230'); b.box(x, top + 0.25, z, 1.8, 0.02, 0.35, '#f2c230'); }      // a helipad
      else { b.box(x, top, z, tw * kk * 0.5, 2, td * kk * 0.5, '#aab3bc'); b.cyl(x + tw * kk * 0.15, top + 2, z, 0.14, 7, '#d94f4f'); b.ball(x + tw * kk * 0.15, top + 9.1, z, 0.3, '#ff3b30'); }
      b.box(x, 0, z + td / 2 + 0.06, 3.4, 3.2, 0.15, '#bfe0f2'); b.box(x, 3.2, z + td / 2 + 0.9, 5, 0.3, 1.8, trim);       // entrance and canopy
      for (const s of [-1, 1]) { const px = x + s * (tw / 2 - 1), pz = z + td / 2 + 1.3; b.box(px, 0, pz, 1.4, 0.7, 1.4, '#9aa3ad'); tree(b, px, pz, r, 0.5); circles.push([px, pz, 0.8]); }
    },
    raadhus(b, L, r, x, z, w, d, circles) {
      b.flat.box(x, 0, z, w, 0.12, d, '#d8cbb3');
      const bw = Math.min(w - 4, 30), bd = Math.min(d - 6, 18), fh = 4, brick = '#d0674a', trim = '#f4ecdc';
      b.box(x, 0, z, bw, fh * 2, bd, brick); b.box(x, fh * 2, z, bw + 0.5, 0.6, bd + 0.5, trim); b.box(x, 0, z, bw + 0.3, 0.6, bd + 0.3, trim);
      b.roof(x, fh * 2 + 0.6, z, bw + 0.4, 4.5, bd + 0.4, '#5e7f8c');
      for (let k = -2; k <= 2; k++) if (k) { b.box(x + k * bw / 6, fh * 2 + 1.0, z + bd / 2 - 0.4, 1.6, 2.0, 1.4, trim); b.roof(x + k * bw / 6, fh * 2 + 3.0, z + bd / 2 - 0.4, 1.8, 1.0, 1.6, '#5e7f8c', Math.PI / 2); }      // dormers
      windows(b, x, z, bw, bd, 0, 2, fh, '#fff1c2', 0, false, null, trim);
      /* the tower, a clock on each face, a copper roof and a flag */
      const th = 26, tw = 5;
      b.box(x, 0, z, tw, th, tw, brick); b.box(x, th, z, tw + 0.6, 0.7, tw + 0.6, trim); b.box(x, th - 6, z, tw + 0.3, 0.4, tw + 0.3, trim);
      for (const [ox, oz, ry] of [[0, 1, 0], [0, -1, 0], [1, 0, Math.PI / 2], [-1, 0, Math.PI / 2]]) {
        const cxp = x + ox * (tw / 2 + 0.06), czp = z + oz * (tw / 2 + 0.06);
        b.add(G.cylZ, '#ffffff', cxp, th - 3.5, czp, ry, 1.7, 1.7, 0.16); b.add(G.cylZ, '#1b1b1b', cxp + ox * 0.02, th - 3.5, czp + oz * 0.02, ry, 1.8, 1.8, 0.1);
        b.add(G.box, '#1b1b1b', cxp + ox * 0.1, th - 3.0, czp + oz * 0.1, ry, 0.14, 1.0, 0.06); b.add(G.box, '#1b1b1b', cxp + ox * 0.1 + oz * 0.35, th - 3.5, czp + oz * 0.1 + ox * 0.35, ry, 0.7, 0.12, 0.06);
      }
      b.cone(x, th + 0.7, z, tw * 0.62, 8, '#5fb39a', true); b.cyl(x, th + 8.7, z, 0.1, 3, '#444'); b.box(x + 0.9, th + 10.2, z, 1.6, 1, 0.08, '#e8392f');
      /* columns and steps at the front, and flags by them */
      const fz = z + bd / 2;
      b.box(x, 0, fz + 1.4, 10, 0.4, 2.8, '#ece4d4'); b.box(x, 0, fz + 0.9, 9, 0.8, 1.8, '#ece4d4');
      for (let k = -2; k <= 2; k++) b.cyl(x + k * 2, 0.8, fz + 1.0, 0.35, 6.4, trim);
      b.roof(x, 7.2, fz + 1.0, 10.5, 2.2, 2.4, trim, 0);
      b.box(x, 0.8, fz + 0.06, 2.4, 3.6, 0.2, '#6a3e22');
      for (const s of [-1, 1]) { const px = x + s * 7, pz = fz + 2.6; b.cyl(px, 0, pz, 0.09, 8, '#d0d0d0', 'cyl6'); b.box(px + 0.75, 6.7, pz, 1.4, 0.9, 0.05, s > 0 ? '#c8102e' : '#ffffff'); if (s < 0) b.box(px + 0.75, 6.7, pz + 0.03, 1.4, 0.18, 0.05, '#c8102e'); circles.push([px, pz, 0.3]); }
    },
    kirke(b, L, r, x, z, w, d) {
      b.flat.box(x, 0, z, w, 0.06, d, '#9fd67e');
      const along = w >= d, nl = Math.min((along ? w : d) - 5, 22), nw = Math.min((along ? d : w) - 5, 9), ry = along ? 0 : Math.PI / 2, white = '#f7f3ea';
      const W2 = along ? nl : nw, D2 = along ? nw : nl;
      b.box(x, 0, z, W2, 8, D2, white); b.roof(x, 8, z, nl + 0.6, 6, nw + 0.9, '#d1543b', ry);
      b.add(G.box, '#a8402c', x, 13.9, z, ry, nl + 0.7, 0.25, 0.35);
      windows(b, x, z, W2, D2, 1.5, 1, 5, '#8fb3e8', 0, false, null, '#d9cfb8');
      const tx = along ? x - nl / 2 - 2.2 : x, tz = along ? z : z - nl / 2 - 2.2;
      b.box(tx, 0, tz, 4.6, 16, 4.6, white); b.box(tx, 16, tz, 5.0, 0.5, 5.0, '#d9cfb8'); b.cone(tx, 16.5, tz, 3.2, 11, '#d1543b', true);
      b.box(tx, 27.4, tz, 0.25, 2.6, 0.25, '#e8c35a'); b.add(G.box, '#e8c35a', tx, 28.6, tz, ry, 1.3, 0.25, 0.25);
      const ox = along ? -2.33 : 0, oz = along ? 0 : -2.33;
      b.add(G.cylZ, '#8fb3e8', tx + ox, 11.5, tz + oz, ry + Math.PI / 2, 1.0, 1.0, 0.12); b.add(G.cylZ, '#d9cfb8', tx + ox * 0.99, 11.5, tz + oz * 0.99, ry + Math.PI / 2, 1.2, 1.2, 0.08);      // rose window
      b.add(G.box, '#6a3e22', tx + ox, 0, tz + oz, ry + Math.PI / 2, 1.8, 3.2, 0.12); b.add(G.cylZ, '#6a3e22', tx + ox, 3.2, tz + oz, ry + Math.PI / 2, 0.9, 0.9, 0.12);      // arched door
      for (let k = 0; k < 6; k++) { const gx = x + (r() - 0.5) * (w - 3), gz = z + (along ? 1 : -1) * (d / 2 - 1.5) * (r() < 0.5 ? 1 : -1); b.box(gx, 0, gz, 0.6, 0.9, 0.2, '#9aa0a6', ry); }      // gravestones
      for (let k = 0; k < 3; k++) tree(b, x + (r() - 0.5) * (w - 4), z + (along ? 1 : -1) * (d / 2 - 2) * (r() < 0.5 ? 1 : -1), r, 0.8);
    },
    park(b, L, r, x, z, w, d, circles) {
      b.flat.box(x, 0, z, w, 0.06, d, '#7fcc6a');
      b.flat.box(x, 0.06, z, w - 0.5, 0.02, 2.2, '#ead7a4'); b.flat.box(x, 0.06, z, 2.2, 0.02, d - 0.5, '#ead7a4');
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) b.box(x + sx * (w / 2 - 0.4), 0, z + sz * (d / 4 + 0.5), 0.6, 0.8, d / 2 - 2.4, '#4f9a3f');      // hedges along the sides, open at the paths
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) b.box(x + sx * (w / 4 + 0.5), 0, z + sz * (d / 2 - 0.4), w / 2 - 2.4, 0.8, 0.6, '#4f9a3f');
      if (w >= 16 && d >= 16) {                                                                       // a pond, with a duck
        const px = x + w / 4, pz = z - d / 4, pr = Math.min(w, d) / 6; b.cyl(px, 0.02, pz, pr + 0.6, 0.12, '#c9c4ba'); b.flat.cyl(px, 0.13, pz, pr, 0.04, '#5fc0ea'); circles.push([px, pz, pr]);
        const dx = px + pr * 0.3, dz = pz + pr * 0.2; b.ball(dx, 0.35, dz, 0.35, '#ffd23b', 0.8); b.ball(dx + 0.3, 0.65, dz, 0.2, '#ffd23b'); b.add(G.box, '#ff8a2b', dx + 0.52, 0.62, dz, 0, 0.18, 0.07, 0.12);
      }
      if (w >= 16 && d >= 16) {                                                                       // a playground: sand, a slide and swings
        const px = x - w / 4, pz = z + d / 4; b.flat.box(px, 0.06, pz, 6, 0.04, 5, '#f2dd9b');
        b.box(px - 1.2, 0, pz, 0.8, 2.2, 0.8, '#e8392f'); b.add(G.box, '#3b82d6', px + 0.4, 1.1, pz, 0, 2.6, 0.12, 0.8); b.add(G.box, '#3b82d6', px + 0.4, 1.1, pz, 0, 0.1, 0.1, 0.1);
        b.boxRot(px + 0.5, 1.1, pz, 2.8, 0.12, 0.9, '#3b82d6', 0, -0.62);
        for (const s of [-1, 1]) { b.cyl(px - 0.4 + s * 1.6, 0, pz + 1.8, 0.07, 2.6, '#f2c230', 'cyl6'); }
        b.box(px - 0.4, 2.55, pz + 1.8, 3.4, 0.14, 0.14, '#f2c230'); for (const s of [-1, 1]) { b.box(px - 0.4 + s * 0.7, 1.4, pz + 1.8, 0.04, 1.15, 0.04, '#333'); b.box(px - 0.4 + s * 0.7, 0.55, pz + 1.8, 0.6, 0.08, 0.35, '#e8392f'); }
        circles.push([px - 0.4, pz + 1, 2.2]);
      }
      /* flower beds round the middle */
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const fx = x + sx * 2.6, fz = z + sz * 2.6; b.cyl(fx, 0, fz, 1.0, 0.25, '#7a5132'); for (let k = 0; k < 6; k++) b.ball(fx + Math.cos(k) * 0.55, 0.42, fz + Math.sin(k) * 0.55, 0.22, pick(r, ['#ff5d73', '#ffd23b', '#c86bff', '#ff8a3b', '#ffffff'])); }
      const n = Math.round((w * d) / 45);
      for (let k = 0; k < n; k++) {
        const tx = x + (r() - 0.5) * (w - 3), tz = z + (r() - 0.5) * (d - 3);
        if (Math.abs(tx - x) < 4 || Math.abs(tz - z) < 4) continue;
        if (circles.some(c => Math.hypot(c[0] - tx, c[1] - tz) < c[2] + 2)) continue;
        tree(b, tx, tz, r); circles.push([tx, tz, 0.5]);
      }
      bench(b, x + 3.5, z - 1.6, 0); bench(b, x - 3.5, z + 1.6, Math.PI);
      for (const [sx, sz] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const lx = x + sx * (w / 2 - 2.5) + (sz ? 1.6 : 0), lz = z + sz * (d / 2 - 2.5) + (sx ? 1.6 : 0); lamp(b, lx, lz); circles.push([lx, lz, 0.25]); }
    },
    torv(b, L, r, x, z, w, d, circles) {
      const tiles = Math.max(2, Math.round(w / 2)), tz = Math.max(2, Math.round(d / 2));
      b.flat.box(x, 0, z, w, 0.06, d, '#e2d2b0');
      for (let i = 0; i < tiles; i++) for (let j = 0; j < tz; j++) if ((i + j) % 2) b.flat.add(G.plane, '#d2bf98', x - w / 2 + (i + 0.5) * w / tiles, 0.065, z - d / 2 + (j + 0.5) * d / tz, 0, w / tiles, 1, d / tz);
      /* the fountain: a basin, a column, two bowls and a spout */
      b.cyl(x, 0, z, 3, 0.8, '#c6c0b4'); b.flat.cyl(x, 0.6, z, 2.6, 0.22, '#5fc0ea'); b.cyl(x, 0.8, z, 0.35, 2.2, '#c6c0b4'); b.cyl(x, 3.0, z, 1.3, 0.3, '#c6c0b4'); b.cyl(x, 3.3, z, 0.25, 0.9, '#c6c0b4'); b.cyl(x, 4.2, z, 0.7, 0.22, '#c6c0b4'); b.ball(x, 4.6, z, 0.35, '#bfe9ff', 1.8);
      circles.push([x, z, 3]);
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const lx = x + sx * (w / 2 - 1.2), lz = z + sz * (d / 2 - 1.2); lamp(b, lx, lz); circles.push([lx, lz, 0.25]); }
      /* market stalls and benches round the fountain, where there is room */
      if (w >= 14 || d >= 14) { const spots = [[0, -1], [0, 1], [1, 0], [-1, 0]]; let k = 0;
        for (const [sx, sz] of spots) { const ox = sx * Math.min(w / 2 - 3, 7), oz = sz * Math.min(d / 2 - 3, 7); if (Math.abs(ox) < 5 && Math.abs(oz) < 5) continue;
          if (k++ % 2 === 0) { stall(b, x + ox, z + oz, sx ? Math.PI / 2 : 0, r); circles.push([x + ox, z + oz, 1.3]); } else bench(b, x + ox, z + oz, sz > 0 ? Math.PI : sx > 0 ? -Math.PI / 2 : sx < 0 ? Math.PI / 2 : 0); } }
      for (const [sx, sz] of [[1, 1], [-1, -1]]) { const px = x + sx * (w / 2 - 3), pz = z + sz * (d / 2 - 3); b.cyl(px, 0, pz, 0.7, 0.8, '#b5654a'); b.ball(px, 1.1, pz, 0.6, '#5cb84a'); for (let k = 0; k < 4; k++) b.ball(px + Math.cos(k * 1.6) * 0.4, 1.4, pz + Math.sin(k * 1.6) * 0.4, 0.15, '#ff5d73'); }
    },
  };
  const lotCircles = new Map();
  function buildLot(L) {
    const old = meshes.get(L.id); if (old) { root.remove(old); old.geometry.dispose(); }
    const b = new Builder(), r = rng(L.seed), w = (L.x1 - L.x0 + 1) * CELL, d = (L.z1 - L.z0 + 1) * CELL, x = (cx(L.x0) + cx(L.x1)) / 2, z = (cz(L.z0) + cz(L.z1)) / 2, circles = [];
    const f = new Builder(); b.flat = f; GEN[L.type](b, L, r, x, z, w, d, circles);
    lotCircles.set(L.id, circles);
    const g = b.build(); if (!g) return; const m = ink(new THREE.Mesh(g, mat)); m.userData.lot = L.id;
    const gf = f.build(); if (gf) m.add(new THREE.Mesh(gf, mat));
    meshes.set(L.id, m); root.add(m);
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
      grp.add(ink(new THREE.Mesh(b.build(), mat)));
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
    for (const c of roadCircles) colliders.circles.push(c);
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
    /* cartoon clouds: a few puffy heaps of white balls high over the fields */
    for (let k = 0; k < 14; k++) { const a = r() * Math.PI * 2, dd = 160 + r() * 380, cx0 = Math.cos(a) * dd, cz0 = Math.sin(a) * dd, cy = 70 + r() * 70, s = 8 + r() * 10;
      for (let q = 0; q < 6; q++) b.ball(cx0 + (q - 2.5) * s * 0.75, cy + (q % 2 ? s * 0.35 : 0) + (q === 2 || q === 3 ? s * 0.4 : 0), cz0 + (r() - 0.5) * s, s * (q === 2 || q === 3 ? 1.1 : 0.8), '#ffffff', 0.8); }
    const m = ink(new THREE.Mesh(b.build(), mat)); m.name = 'scenery'; return m;
  }

  reset(20, 14);
  return { scenery,
    root, st, reset, paintRoad, placeLot, eraseRect, setGate, commit, save, load, starter, route, collide, surfaceAt, cellAt, inside, isRoad, lotOK,
    cx, cz, colliders, gridLines: () => gridLines, ground: () => groundMesh, mat,
  };
}
