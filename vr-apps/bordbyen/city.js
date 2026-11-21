/*
 * Ræs i Bordbyen — the city: a grid of cells on the table, what is drawn on them, and the meshes.
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
 * Hills: the ground has a height at every cell corner (none at the board's edge). Raising or lowering an area adds a soft bump. Roads, grass, parks
 * and squares lie on the ground (their vertices are lifted by the height under them); buildings stay upright: each solid lot stands at the highest
 * ground under it, on a stone plinth that reaches down to the lowest.
 *
 * Tunnels: an underground layer, two-lane roads only, at one depth (TUN_Y). A ramp (two cells) leads down from a road to a tunnel: an open trench
 * with concrete walls and a portal at the bottom. Nothing else can be built down there; tunnels may run under anything. From the table they show as a
 * faint brown band over the town. Cars have a level: 0 on the surface, -1 in a tunnel, 'r' on a ramp; they only meet cars on their own level.
 *
 * Driving: the solid lots are boxes the cars bounce off (the lot less its pavement); parks have trees, squares a fountain (circles); parks and grass are
 * slow, roads and squares fast. The opponents' route is found through roads and squares only (breadth first, then rounded).
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';
import { toonMat, ink } from './toon.js?v=11';

export const CELL = 8;
export const SCALE = 1 / 160;
export const TUN_Y = -4.5, TUN_TOP = -0.6;                // the tunnel floor and ceiling, in city metres below the board's surface
const DIRV = [null, [0, -1], [1, 0], [0, 1], [-1, 0]];     // ramp directions (downhill): 1 N, 2 E, 3 S, 4 W                         // city metres to table metres: a cell is 5 cm on the table, a car 2.5 cm
export const TOOLS = [
  { key: 'vej', name: 'Vej', color: '#5b6270', kind: 'road' },
  { key: 'tunnel', name: 'Tunnel', color: '#8a5a2a', kind: 'tunnel' },
  { key: 'rampe', name: 'Nedkørsel', color: '#c79a5a', kind: 'ramp' },
  { key: 'huse', name: 'Huse', color: '#f3b6a5', kind: 'lot', solid: true },
  { key: 'byhus', name: 'Byhuse', color: '#f7e48b', kind: 'lot', solid: true },
  { key: 'hoejhus', name: 'Højhus', color: '#8fc3ea', kind: 'lot', solid: true },
  { key: 'raadhus', name: 'Rådhus', color: '#c8664a', kind: 'lot', solid: true, min: 2 },
  { key: 'kirke', name: 'Kirke', color: '#f2efe6', kind: 'lot', solid: true, min: 2 },
  { key: 'park', name: 'Park', color: '#7cc46a', kind: 'lot' },
  { key: 'torv', name: 'Torv', color: '#d9c9a8', kind: 'lot' },
  { key: 'bakke', name: 'Bakke', color: '#a6d97f', kind: 'terrain', dir: 1 },
  { key: 'dal', name: 'Sænk', color: '#6f8a62', kind: 'terrain', dir: -1 },
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

/* ---- the tunnel sign: blue, an arrow pointing down, and TUNNEL (one texture for them all) ---- */
const SIGN_GEO = new THREE.PlaneGeometry(5.4, 2.1);
let _signMat = null;
function signMat() {
  if (_signMat) return _signMat;
  const c = document.createElement('canvas'); c.width = 512; c.height = 200; const x = c.getContext('2d');
  x.fillStyle = '#1f5fae'; x.beginPath(); x.roundRect(0, 0, 512, 200, 26); x.fill(); x.strokeStyle = '#ffffff'; x.lineWidth = 10; x.beginPath(); x.roundRect(12, 12, 488, 176, 18); x.stroke();
  x.fillStyle = '#ffffff'; x.beginPath(); x.moveTo(62, 40); x.lineTo(102, 40); x.lineTo(102, 100); x.lineTo(132, 100); x.lineTo(82, 165); x.lineTo(32, 100); x.lineTo(62, 100); x.closePath(); x.fill();
  x.font = 'bold 92px Arial, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText('TUNNEL', 318, 104);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4;
  return (_signMat = new THREE.MeshBasicMaterial({ map: t }));
}

/* ---- trees, windows: shared bits ---- */
function tree(b, x, z, r, s = 1, circles = null) {
  const kind = r(), h = (3 + r() * 2.5) * s; if (circles) circles.push([x, z, 0.45 * s]);      // a trunk you can run into
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
  const st = { W: 20, H: 14, cells: null, lot: null, hts: null, tun: null, rdir: null, rpart: null, lots: new Map(), gates: [], nextId: 1 };
  const meshes = new Map();                            // lot id -> mesh
  let roadMesh = null, groundMesh = null, gridLines = null, gateGroup = new THREE.Group(); root.add(gateGroup);
  const colliders = { rects: [], circles: [] };

  const idx = (i, j) => j * st.W + i;
  const inside = (i, j) => i >= 0 && j >= 0 && i < st.W && j < st.H;
  const cx = i => (i + 0.5 - st.W / 2) * CELL, cz = j => (j + 0.5 - st.H / 2) * CELL;
  const isRoad = (i, j) => inside(i, j) && st.cells[idx(i, j)] === 1;
  const roadish = (i, j) => inside(i, j) && st.cells[idx(i, j)] >= 1;                 // a road or a ramp
  const isTun = (i, j) => inside(i, j) && st.tun[idx(i, j)] === 1;
  const rampAt = (i, j) => { if (!inside(i, j)) return null; const k = idx(i, j), d = st.rdir[k]; return d ? { d, v: DIRV[d], part: st.rpart[k] } : null; };
  const cellAt = (x, z) => [Math.floor(x / CELL + st.W / 2), Math.floor(z / CELL + st.H / 2)];

  function reset(W, H) {
    st.W = W; st.H = H; st.hts = new Float32Array((W + 1) * (H + 1)); st.tun = new Uint8Array(W * H); st.rdir = new Uint8Array(W * H); st.rpart = new Uint8Array(W * H); st.cells = new Uint8Array(W * H); st.lot = new Int32Array(W * H).fill(0); st.lots.clear(); st.gates = []; st.nextId = 1;
    for (const m of meshes.values()) { root.remove(m); m.geometry.dispose(); } meshes.clear();
    rebuildGround(); rebuildRoads(); rebuildUnder(); rebuildGates(); rebuildColliders();
  }

  /* ---------- the ground's height ---------- */
  const hIdx = (ci, cj) => cj * (st.W + 1) + ci;
  /** the ground's height at a city point (bilinear between the cell corners) */
  function heightAt(x, z) {
    const fx = Math.max(0, Math.min(st.W - 1e-6, x / CELL + st.W / 2)), fz = Math.max(0, Math.min(st.H - 1e-6, z / CELL + st.H / 2)), i = Math.floor(fx), j = Math.floor(fz), u = fx - i, v = fz - j;
    const h = st.hts; return (h[hIdx(i, j)] * (1 - u) + h[hIdx(i + 1, j)] * u) * (1 - v) + (h[hIdx(i, j + 1)] * (1 - u) + h[hIdx(i + 1, j + 1)] * u) * v;
  }
  /** lift (dir 1) or lower (dir -1) the ground softly over a rectangle of cells, fading out one cell beyond it */
  function shapeTerrain(a, b, c, d, dir) {
    const [x0, z0, x1, z1] = norm(a, b, c, d), u0 = x0 - 1, u1 = x1 + 2, v0 = z0 - 1, v1 = z1 + 2;
    for (let cj = 1; cj < st.H; cj++) for (let ci = 1; ci < st.W; ci++) {
      if (ci <= u0 || ci >= u1 || cj <= v0 || cj >= v1) continue;
      const w = Math.sin(Math.PI * (ci - u0) / (u1 - u0)) * Math.sin(Math.PI * (cj - v0) / (v1 - v0)), k = hIdx(ci, cj);
      st.hts[k] = Math.max(0, Math.min(16, st.hts[k] + dir * 3.5 * w));      // never below the board (it would sink into the board and into your table)
    }
    return true;
  }
  /** lay a geometry on the ground: every vertex lifted by the height under it */
  function drape(g, lift = 0) { const p = g.getAttribute('position'); for (let k = 0; k < p.count; k++) p.setY(k, p.getY(k) + heightAt(p.getX(k), p.getZ(k)) + lift); p.needsUpdate = true; g.computeBoundingSphere(); return g; }
  /** a whole new shape of the ground: everything that stands on it is rebuilt */
  function reshape() { rebuildGround(); for (const L of st.lots.values()) buildLot(L); commit(); }

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
      if (st.rdir[k]) { removeRamp(i, j); changed = true; }
      if (st.cells[k]) { st.cells[k] = 0; changed = true; }
    }
    const before = st.gates.length; st.gates = st.gates.filter(g => !(g.i >= x0 && g.i <= x1 && g.j >= z0 && g.j <= z1)); if (st.gates.length !== before) changed = true;
    return changed;
  }
  const norm = (a, b, c, d) => [Math.max(0, Math.min(a, c)), Math.max(0, Math.min(b, d)), Math.min(st.W - 1, Math.max(a, c)), Math.min(st.H - 1, Math.max(b, d))];
  /** take a ramp away, both its cells */
  function removeRamp(i, j) {
    const r = rampAt(i, j); if (!r) return; const [di, dj] = r.v, oi = r.part ? i - di : i + di, oj = r.part ? j - dj : j + dj;
    for (const [a, b] of [[i, j], [oi, oj]]) if (inside(a, b)) { const k = idx(a, b); if (st.rdir[k]) { st.rdir[k] = 0; st.rpart[k] = 0; st.cells[k] = 0; } }
  }
  /** a ramp down from cell a, two cells long towards b (its direction is the larger step from a to b); the tunnel starts beyond it */
  function placeRamp(ai, aj, bi, bj) {
    const dx = bi - ai, dz = bj - aj; if (!dx && !dz) return false;
    let d = Math.abs(dx) >= Math.abs(dz) ? (dx > 0 ? 2 : 4) : (dz > 0 ? 3 : 1), [di, dj] = DIRV[d], ci = ai + di, cj = aj + dj;
    if (!inside(ai, aj) || !inside(ci, cj)) return false;
    /* dragged from the tunnel towards the road (or with a road at the far end and none at the near end): turn it round, top at the road */
    const roadAt = (i, j) => inside(i, j) && !isTun(i, j) && (st.cells[idx(i, j)] === 1 || (st.lot[idx(i, j)] && st.lots.get(st.lot[idx(i, j)]).type === 'torv'));
    const fit = (ti, tj, fi, fj, ddi, ddj) => (roadAt(ti - ddi, tj - ddj) ? 1 : 0) + (isTun(fi + ddi, fj + ddj) ? 1 : 0);
    if (fit(ci, cj, ai, aj, -di, -dj) > fit(ai, aj, ci, cj, di, dj)) { [ai, aj, ci, cj] = [ci, cj, ai, aj]; d = [0, 3, 4, 1, 2][d]; [di, dj] = DIRV[d]; }
    clearRect(Math.min(ai, ci), Math.min(aj, cj), Math.max(ai, ci), Math.max(aj, cj));
    for (const [i, j, part] of [[ai, aj, 0], [ci, cj, 1]]) { const k = idx(i, j); st.cells[k] = 2; st.rdir[k] = d; st.rpart[k] = part; st.tun[k] = 0;
      for (const [a, b] of [[0, 0], [1, 0], [0, 1], [1, 1]]) st.hts[hIdx(i + a, j + b)] = 0; }           // the ground round a ramp is flat
    if (inside(ci + di, cj + dj)) st.tun[idx(ci + di, cj + dj)] = 1;
    return true;
  }
  /** a tunnel cell, or none (on = false) */
  function paintTunnel(i, j, on) { if (!inside(i, j)) return false; const k = idx(i, j); if (st.rdir[k]) return false; const v = on ? 1 : 0; if (st.tun[k] === v) return false; st.tun[k] = v; return true; }
  /** paint road on one cell (a lot under it goes) */
  function paintRoad(i, j) {
    if (!inside(i, j) || isRoad(i, j)) return false;
    const k = idx(i, j); if (st.lot[k]) removeLot(st.lot[k]); if (st.rdir[k]) removeRamp(i, j);
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
  function commit() { footTunnels(); rebuildRoads(); rebuildUnder(); rebuildGates(); rebuildColliders(); }
  /** the cell beyond every ramp's foot is tunnel, always (a tunnel stroke that started there used to take it away, and the ramp then ran into earth) */
  function footTunnels() {
    for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) { const r = rampAt(i, j); if (!r || r.part !== 1) continue;
      const ni = i + r.v[0], nj = j + r.v[1]; if (inside(ni, nj) && !st.rdir[idx(ni, nj)]) st.tun[idx(ni, nj)] = 1; }
  }

  /* ---------- meshes ---------- */
  function rebuildGround() {
    if (groundMesh) { root.remove(groundMesh); groundMesh.geometry.dispose(); }
    const b = new Builder(), W = st.W * CELL, H = st.H * CELL;
    /* the board is a thick block (the tunnels live inside it): four sides and a bottom, open on top, where the grass is */
    const T = 6.4; for (const [x, z, w, d] of [[0, -H / 2 - 0.3, W + 1.2, 0.6], [0, H / 2 + 0.3, W + 1.2, 0.6], [-W / 2 - 0.3, 0, 0.6, H], [W / 2 + 0.3, 0, 0.6, H]]) b.box(x, -T, z, w, T, d, '#6f5a43');
    b.box(0, -T - 0.3, 0, W + 1.2, 0.3, H + 1.2, '#5a4836');
    groundMesh = ink(new THREE.Mesh(b.build(), mat)); root.add(groundMesh);
    const gb = new Builder(); for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) if (!st.rdir[idx(i, j)]) gb.add(G.plane, '#8fcf6e', cx(i), 0.015, cz(j), 0, CELL, 1, CELL);      // grass on the hills (open over the ramps): too thin to ink
    const gg0 = drape(gb.build()); gg0.computeVertexNormals(); groundMesh.add(new THREE.Mesh(gg0, mat));
    /* the grid, for drawing */
    if (gridLines) { root.remove(gridLines); gridLines.geometry.dispose(); }
    const pts = [];
    const hc = (ci, cj) => st.hts[hIdx(ci, cj)] + 0.08;
    for (let i = 0; i <= st.W; i++) for (let j = 0; j < st.H; j++) { const x = (i - st.W / 2) * CELL; pts.push(x, hc(i, j), (j - st.H / 2) * CELL, x, hc(i, j + 1), (j + 1 - st.H / 2) * CELL); }
    for (let j = 0; j <= st.H; j++) for (let i = 0; i < st.W; i++) { const z = (j - st.H / 2) * CELL; pts.push((i - st.W / 2) * CELL, hc(i, j), z, (i + 1 - st.W / 2) * CELL, hc(i + 1, j), z); }
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
      const x = cx(i), z = cz(j), N = roadish(i, j - 1), S = roadish(i, j + 1), E = roadish(i + 1, j), Wd = roadish(i - 1, j), n = N + S + E + Wd;
      f.add(G.plane, '#4b515c', x, 0.02, z, 0, CELL, 1, CELL);                                   // asphalt (no ink)
      /* pavements with a kerb where the road ends at a side; a lamp on every other one */
      const sides = [[!N, 0, -1], [!S, 0, 1], [!E, 1, 0], [!Wd, -1, 0]];
      for (const [on, dx, dz] of sides) {
        if (!on) continue;
        if (dz) { b.box(x, 0, z + dz * (h - pw / 2), CELL, 0.18, pw, PAVE); b.box(x, 0, z + dz * (h - pw - 0.08), CELL, 0.2, 0.18, KERB); }
        else { b.box(x + dx * (h - pw / 2), 0, z, pw, 0.18, CELL, PAVE); b.box(x + dx * (h - pw - 0.08), 0, z, 0.18, 0.2, CELL, KERB); }
      }
      const lampSide = sides.find(sd => sd[0]);
      if (lampSide && (i * 7 + j * 13) % 9 === 0) { const [, dx, dz] = lampSide, hx = x + dx * (h - 0.7) + (dz ? 2.5 : 0), hz = z + dz * (h - 0.7) + (dx ? 2.5 : 0); b.cyl(hx, 0.18, hz, 0.2, 0.7, '#e2382f', 'cyl6'); b.ball(hx, 0.92, hz, 0.2, '#e2382f'); }      // a fire hydrant
      for (const [dx, dz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) {
        const a = dx > 0 ? E : Wd, c = dz > 0 ? S : N;
        if (a && c && !roadish(i + dx, j + dz)) {
          const px = x + dx * (h - pw / 2), pz = z + dz * (h - pw / 2); b.box(px, 0, pz, pw, 0.18, pw, PAVE);
          if (n >= 3) {                                                                           // a traffic light, out at the very corner (cars pass it)
            const qx = x + dx * (h - 0.25), qz = z + dz * (h - 0.25);
            b.cyl(qx, 0.18, qz, 0.1, 3.4, '#2a2a2e', 'cyl6'); b.box(qx, 3.2, qz, 0.45, 1.3, 0.45, '#1f1f24');
            for (const [k, col] of [[0, '#ff4a3d'], [1, '#ffcf3d'], [2, '#4ade6b']]) b.ball(qx - dx * 0.24, 4.25 - k * 0.4, qz - dz * 0.24, 0.13, col);
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
    const g = b.build(); if (g) { roadMesh = ink(new THREE.Mesh(drape(g), mat)); root.add(roadMesh); }
    const g2 = f.build(); if (g2) { flatRoad = new THREE.Mesh(drape(g2), mat); root.add(flatRoad); }
  }

  /* ---------- the underground: ramps, tunnels, and their map on the table ---------- */
  let rampMesh = null, tunMesh = null, tunFlat = null, tunMap = null; const signs = new THREE.Group(); root.add(signs);
  const tunMapMat = new THREE.MeshBasicMaterial({ color: 0x7a4a1e, transparent: true, opacity: 0.45, depthTest: false, depthWrite: false });
  /** a quad from four corners (in order round it), for sloping floors */
  function quad(b, p, color) { const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute([...p[0], ...p[1], ...p[2], ...p[0], ...p[2], ...p[3]], 3)); g.computeVertexNormals(); b.add(g, color); }
  /** is a tunnel cell open towards a neighbour (another tunnel cell, or the bottom of a ramp that leads into it)? */
  function tunOpen(i, j, ni, nj) { if (isTun(ni, nj)) return true; const r = rampAt(ni, nj); return !!r && r.part === 1 && ni + r.v[0] === i && nj + r.v[1] === j; }
  function rebuildUnder() {
    signs.clear();
    for (const m of [rampMesh, tunMesh, tunFlat, tunMap]) if (m) { root.remove(m); m.geometry.dispose(); }
    rampMesh = tunMesh = tunFlat = tunMap = null;
    const rb = new Builder(), tb = new Builder(), tf = new Builder(), mb = new Builder(), h = CELL / 2, CON = '#bdb7ab', DARK = '#8f8a80';
    for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) {
      const x = cx(i), z = cz(j), r = rampAt(i, j);
      if (r) {
        /* a ramp: a sloping floor, walls either side, and at its foot the tunnel's portal */
        const [di, dj] = r.v, sx = -dj, sz = di, y0 = r.part ? TUN_Y / 2 : 0, y1 = r.part ? TUN_Y : TUN_Y / 2;
        const P = (a, s, y) => [x + di * a + sx * s, y, z + dj * a + sz * s];
        quad(rb, [P(-h, -h, y0), P(-h, h, y0), P(h, h, y1), P(h, -h, y1)].map(q => q), '#4b515c');
        if (r.part === 0) for (let k = -1; k <= 1; k++) quad(tf, [P(-h + 0.5, k * 2 - 0.3, y0 + 0.03), P(-h + 0.5, k * 2 + 0.3, y0 + 0.03), P(-h + 2.0, k * 2 + 0.3, y0 + 0.03 + (y1 - y0) * 1.5 / CELL), P(-h + 2.0, k * 2 - 0.3, y0 + 0.03 + (y1 - y0) * 1.5 / CELL)], '#f2c230');
        for (const sd of [-1, 1]) rb.add(G.box, CON, x + sx * sd * (h - 0.3), (TUN_Y - 0.3 + 1.0) / 2, z + sz * sd * (h - 0.3), Math.atan2(di, dj), 0.6, 1.0 - TUN_Y + 0.3, CELL);
        if (r.part === 1) { const fx = x + di * (h - 0.3), fz = z + dj * (h - 0.3); rb.add(G.box, CON, fx, (TUN_TOP + 1.0) / 2, fz, Math.atan2(di, dj) + Math.PI / 2, 0.6, 1.0 - TUN_TOP, CELL);
          for (let k = 0; k < 4; k++) rb.add(G.box, k % 2 ? '#222' : '#f2c230', fx + di * 0.32 + sx * (k - 1.5) * 1.9, TUN_TOP + 0.25, fz + dj * 0.32 + sz * (k - 1.5) * 1.9, Math.atan2(di, dj) + Math.PI / 2, 0.05, 0.5, 1.9); }
        /* over the top of the ramp, a gantry with a blue sign: an arrow down and TUNNEL, facing the cars that come to it */
        if (r.part === 0) { const tx = x - di * (h - 0.4), tz = z - dj * (h - 0.4), ry = Math.atan2(di, dj);
          for (const sd of [-1, 1]) rb.box(tx + sx * sd * (h - 0.3), 1.0, tz + sz * sd * (h - 0.3), 0.35, 5.0, 0.35, '#8d949c');
          rb.add(G.box, '#8d949c', tx, 5.85, tz, ry + Math.PI / 2, 0.3, 0.3, CELL - 0.2);
          rb.add(G.box, '#2b3440', tx + di * 0.08, 4.55, tz + dj * 0.08, ry + Math.PI / 2, 0.12, 2.3, 5.6);
          const sg = new THREE.Mesh(SIGN_GEO, signMat()); sg.position.set(tx - di * 0.01, 4.55, tz - dj * 0.01); sg.rotation.y = Math.atan2(-di, -dj); signs.add(sg); }
        continue;
      }
      if (!isTun(i, j)) continue;
      /* a tunnel cell: floor, ceiling, walls where it is closed, lamps under the ceiling, lane dashes on straight pieces */
      const N = tunOpen(i, j, i, j - 1), S = tunOpen(i, j, i, j + 1), E = tunOpen(i, j, i + 1, j), Wd = tunOpen(i, j, i - 1, j);
      tf.add(G.plane, '#3f444d', x, TUN_Y + 0.02, z, 0, CELL, 1, CELL);
      tf.box(x, TUN_TOP, z, CELL, 0.35, CELL, DARK);
      for (const [on, dx, dz] of [[N, 0, -1], [S, 0, 1], [E, 1, 0], [Wd, -1, 0]]) { if (on) continue;
        if (dz) tb.box(x, TUN_Y, z + dz * (h - 0.3), CELL, TUN_TOP - TUN_Y, 0.6, CON); else tb.box(x + dx * (h - 0.3), TUN_Y, z, 0.6, TUN_TOP - TUN_Y, CELL, CON); }
      for (const [dx, dz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const a = dx > 0 ? E : Wd, c = dz > 0 ? S : N; if (a && c && !isTun(i + dx, j + dz)) tb.box(x + dx * (h - 0.3), TUN_Y, z + dz * (h - 0.3), 0.6, TUN_TOP - TUN_Y, 0.6, CON); }
      const along = (N || S) && !E && !Wd ? 'z' : (E || Wd) && !N && !S ? 'x' : null;
      if (along === 'z') { for (const o of [-2, 2]) tf.add(G.plane, '#f4f1e6', x, TUN_Y + 0.04, z + o, 0, 0.22, 1, 2); tb.box(x, TUN_TOP - 0.12, z, 0.35, 0.12, 5, '#fff2b0'); }
      else if (along === 'x') { for (const o of [-2, 2]) tf.add(G.plane, '#f4f1e6', x + o, TUN_Y + 0.04, z, 0, 2, 1, 0.22); tb.box(x, TUN_TOP - 0.12, z, 5, 0.12, 0.35, '#fff2b0'); }
      else tb.box(x, TUN_TOP - 0.12, z, 1.2, 0.12, 1.2, '#fff2b0');
      /* the map on the table: a brown band along the tunnel */
      mb.add(G.plane, '#7a4a1e', x, 0, z, 0, 3.6, 1, 3.6);
      for (const [on, dx, dz] of [[N, 0, -1], [S, 0, 1], [E, 1, 0], [Wd, -1, 0]]) if (on) mb.add(G.plane, '#7a4a1e', x + dx * h / 2, 0, z + dz * h / 2, 0, dx ? h : 3.6, 1, dz ? h : 3.6);
    }
    const g1 = rb.build(); if (g1) { rampMesh = ink(new THREE.Mesh(g1, mat)); root.add(rampMesh); }
    const g2 = tb.build(); if (g2) { tunMesh = ink(new THREE.Mesh(g2, mat)); root.add(tunMesh); }
    const g3 = tf.build(); if (g3) { tunFlat = new THREE.Mesh(g3, mat); root.add(tunFlat); }
    const g4 = mb.build(); if (g4) { tunMap = new THREE.Mesh(drape(g4, 0.4), tunMapMat); tunMap.renderOrder = 15; tunMap.visible = mapOn; root.add(tunMap); }
  }
  let mapOn = true;
  /** the tunnel map on the table: shown while building and in the city view, hidden down in the streets */
  function showTunnelMap(on) { mapOn = on; if (tunMap) tunMap.visible = on; }
  /** the floor's height on a level: the ground (0), the tunnel floor (-1), or along a ramp ('r') */
  function groundY(level, x, z) {
    if (level === -1) return TUN_Y;
    if (level === 'r') { const [i, j] = cellAt(x, z), r = rampAt(i, j); if (r) return rampT(i, j, r, x, z) * TUN_Y; return heightAt(x, z); }
    return heightAt(x, z);
  }
  /** how far down a ramp a point is: 0 at its top, 1 at its foot */
  function rampT(i, j, r, x, z) { const [di, dj] = r.v, top0 = r.part ? 1.5 * CELL : 0.5 * CELL; return Math.max(0, Math.min(1, ((x - cx(i)) * di + (z - cz(j)) * dj + top0) / (2 * CELL))); }
  /** a car's level after it has moved to (x, z), coming from level `lv` (with how far down a ramp it was, t) */
  function levelAt(lv, t, x, z) { const [i, j] = cellAt(x, z), r = rampAt(i, j); if (r) return { level: 'r', t: rampT(i, j, r, x, z) }; if (lv === 'r') return { level: t > 0.5 ? -1 : 0, t: 0 }; return { level: lv, t: 0 }; }

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
      if (dep > 14) for (let k = 0; k < Math.floor((len - 6) / 9); k++) { const o = -len / 2 + 5 + k * 9 + r() * 2, sd = (dep / 2 - 3) * (r() < 0.5 ? 1 : -1);
        const tx = along ? x + o : x + sd, tz = along ? z + sd : z + o; tree(b, tx, tz, r, 0.8, circles); }
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
    kirke(b, L, r, x, z, w, d, circles) {
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
      for (let k = 0; k < 3; k++) tree(b, x + (r() - 0.5) * (w - 7), z + (along ? 1 : -1) * (d / 2 - 3) * (r() < 0.5 ? 1 : -1), r, 0.8, circles);
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
        const tx = x + (r() - 0.5) * (w - 7), tz = z + (r() - 0.5) * (d - 7);                 // well inside the park: no crowns over the road
        if (Math.abs(tx - x) < 4 || Math.abs(tz - z) < 4) continue;
        if (circles.some(c => Math.hypot(c[0] - tx, c[1] - tz) < c[2] + 2)) continue;
        tree(b, tx, tz, r); circles.push([tx, tz, 0.5]);
      }
      bench(b, x + 3.5, z - 1.6, 0); bench(b, x - 3.5, z + 1.6, Math.PI);
      for (const [sx, sz] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const lx = x + sx * (w / 2 - 2.5) + (sz ? 1.6 : 0), lz = z + sz * (d / 2 - 2.5) + (sx ? 1.6 : 0); lamp(b, lx, lz); }
    },
    torv(b, L, r, x, z, w, d, circles) {
      const tiles = Math.max(2, Math.round(w / 2)), tz = Math.max(2, Math.round(d / 2));
      b.flat.box(x, 0, z, w, 0.06, d, '#e2d2b0');
      for (let i = 0; i < tiles; i++) for (let j = 0; j < tz; j++) if ((i + j) % 2) b.flat.add(G.plane, '#d2bf98', x - w / 2 + (i + 0.5) * w / tiles, 0.065, z - d / 2 + (j + 0.5) * d / tz, 0, w / tiles, 1, d / tz);
      /* the fountain: a basin, a column, two bowls and a spout */
      b.cyl(x, 0, z, 3, 0.8, '#c6c0b4'); b.flat.cyl(x, 0.6, z, 2.6, 0.22, '#5fc0ea'); b.cyl(x, 0.8, z, 0.35, 2.2, '#c6c0b4'); b.cyl(x, 3.0, z, 1.3, 0.3, '#c6c0b4'); b.cyl(x, 3.3, z, 0.25, 0.9, '#c6c0b4'); b.cyl(x, 4.2, z, 0.7, 0.22, '#c6c0b4'); b.ball(x, 4.6, z, 0.35, '#bfe9ff', 1.8);
      circles.push([x, z, 3]);
      for (const [sx, sz] of [[1, 1], [1, -1], [-1, 1], [-1, -1]]) { const lx = x + sx * (w / 2 - 0.6), lz = z + sz * (d / 2 - 0.6); lamp(b, lx, lz); }
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
    const g = b.build(); if (!g) return; const gf = f.build(); const solid = !!TOOL[L.type].solid;
    let top = -Infinity, low = Infinity; for (let cj = L.z0; cj <= L.z1 + 1; cj++) for (let ci = L.x0; ci <= L.x1 + 1; ci++) { const hh = st.hts[hIdx(ci, cj)]; top = Math.max(top, hh); low = Math.min(low, hh); }
    if (!solid) { drape(g); if (gf) drape(gf); }
    const m = ink(new THREE.Mesh(g, mat)); m.userData.lot = L.id;
    if (gf) m.add(new THREE.Mesh(gf, mat));
    if (solid) { m.position.y = top; if (top - low > 0.05) { const pb = new Builder(); pb.box(x, low - top - 0.3, z, w - 0.2, top - low + 0.3, d - 0.2, '#a39b8e'); m.add(ink(new THREE.Mesh(pb.build(), mat))); } }      // a stone plinth down to the lowest ground
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
      /* a beacon over each gate, so it can be found from anywhere (and seen on the table): a faint pillar of light and a floating diamond.
       * The diamonds of the gates you have cleared turn green. */
      const pillar = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 30, 10, 1, true), new THREE.MeshBasicMaterial({ color: g.start ? 0xffffff : 0xffa040, transparent: true, opacity: 0.22, depthWrite: false, side: THREE.DoubleSide }));
      pillar.position.y = H + 15; grp.add(pillar);
      const dia = ink(new THREE.Mesh(new THREE.OctahedronGeometry(2.4, 0), new THREE.MeshBasicMaterial({ color: g.start ? 0xffffff : 0xff8a1e }))); dia.position.y = 30; dia.scale.y = 1.5; grp.add(dia);
      grp.position.set(cx(g.i), heightAt(cx(g.i), cz(g.j)), cz(g.j)); grp.rotation.y = gateAxis(g) === 'x' ? Math.PI / 2 : 0;
      g.n = g.start ? 0 : n; g.dia = dia; g.pillar = pillar; gateGroup.add(grp);
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
    rampWalls();
    for (const g of st.gates) { const ax = gateAxis(g), x = cx(g.i), z = cz(g.j); for (const s of [-1, 1]) colliders.circles.push(ax === 'x' ? [x, z + s * (CELL / 2 - 0.5), 0.4] : [x + s * (CELL / 2 - 0.5), z, 0.4]); }
  }
  /* the ramps' walls: both sides of every ramp cell (for everyone), and across the foot of a ramp (for those up on the surface) */
  const rampSides = [], rampCaps = [];
  function rampWalls() {
    rampSides.length = 0; rampCaps.length = 0; const h = CELL / 2;
    for (let j = 0; j < st.H; j++) for (let i = 0; i < st.W; i++) { const r = rampAt(i, j); if (!r) continue; const [di, dj] = r.v, x = cx(i), z = cz(j);
      for (const sd of [-1, 1]) { const wx = x - dj * sd * (h - 0.3), wz = z + di * sd * (h - 0.3); rampSides.push(dj ? [wx - 0.3, wz - h, wx + 0.3, wz + h] : [wx - h, wz - 0.3, wx + h, wz + 0.3]); }
      if (r.part === 1) { const fx = x + di * (h - 0.3), fz = z + dj * (h - 0.3); rampCaps.push(di ? [fx - 0.3, fz - h, fx + 0.3, fz + h] : [fx - h, fz - 0.3, fx + h, fz + 0.3]); } }
  }
  function pushRect(p, r, [x0, z0, x1, z1]) {
    const qx = Math.max(x0, Math.min(x1, p.x)), qz = Math.max(z0, Math.min(z1, p.y)), dx = p.x - qx, dz = p.y - qz, d2 = dx * dx + dz * dz;
    if (d2 >= r * r) return null;
    if (d2 > 1e-9) { const d = Math.sqrt(d2); p.x = qx + dx / d * r; p.y = qz + dz / d * r; return _n.set(dx / d, dz / d); }
    const o = [p.x - x0, x1 - p.x, p.y - z0, z1 - p.y], m = Math.min(...o), k = o.indexOf(m);
    if (k === 0) { p.x = x0 - r; return _n.set(-1, 0); } if (k === 1) { p.x = x1 + r; return _n.set(1, 0); } if (k === 2) { p.y = z0 - r; return _n.set(0, -1); } p.y = z1 + r; return _n.set(0, 1);
  }
  /** push a circle out of everything solid on a level: the surface (0), a ramp ('r') or the tunnels (-1); returns the last push normal */
  function collideL(p, r, level) {
    if (level === 0 || level === undefined) { let hit = collide(p, r); for (const q of rampSides) hit = pushRect(p, r, q) || hit; for (const q of rampCaps) hit = pushRect(p, r, q) || hit; return hit; }
    let hit = null; const W = st.W * CELL / 2 - r, H = st.H * CELL / 2 - r;
    if (p.x > W) { p.x = W; hit = _n.set(-1, 0); } if (p.x < -W) { p.x = -W; hit = _n.set(1, 0); } if (p.y > H) { p.y = H; hit = _n.set(0, -1); } if (p.y < -H) { p.y = -H; hit = _n.set(0, 1); }
    for (const q of rampSides) hit = pushRect(p, r, q) || hit;
    if (level === -1) { const [ci, cj] = cellAt(p.x, p.y);
      for (let j = cj - 1; j <= cj + 1; j++) for (let i = ci - 1; i <= ci + 1; i++) { if (!inside(i, j)) continue; const k = idx(i, j); if (st.tun[k] || st.rdir[k]) continue;
        hit = pushRect(p, r, cellRect(i, j)) || hit; } }
    return hit;
  }
  /** a solid cell down in the tunnels, grown by the tunnel walls' thickness (they stand inside the tunnel's own cell) */
  const cellRect = (i, j) => [cx(i) - CELL / 2 - 0.6, cz(j) - CELL / 2 - 0.6, cx(i) + CELL / 2 + 0.6, cz(j) + CELL / 2 + 0.6];
  /** Everything solid near (x, z) for a car on a level ('r' with how far down the ramp, t): axis-aligned rectangles [x0, z0, x1, z1] and
   *  circles [x, z, r] within `reach` metres, and the board's half size. The cars' boxes are kept out of these (race.js). */
  function solidsNear(level, t, x, z, reach) {
    const rects = [], circs = [], near = q => q[0] - reach < x && q[2] + reach > x && q[1] - reach < z && q[3] + reach > z;
    for (const q of rampSides) if (near(q)) rects.push(q);
    if (level === -1 || (level === 'r' && t > 0.5)) {
      const [ci, cj] = cellAt(x, z);
      for (let j = cj - 1; j <= cj + 1; j++) for (let i = ci - 1; i <= ci + 1; i++) { if (!inside(i, j)) continue; const k = idx(i, j); if (st.tun[k] || st.rdir[k]) continue; const q = cellRect(i, j); if (near(q)) rects.push(q); }
    } else {
      for (const q of colliders.rects) if (near(q)) rects.push(q);
      if (level === 0) for (const q of rampCaps) if (near(q)) rects.push(q);
      for (const c of colliders.circles) if (Math.abs(c[0] - x) < reach + c[2] && Math.abs(c[1] - z) < reach + c[2]) circs.push(c);
    }
    return { rects, circs, W: st.W * CELL / 2, H: st.H * CELL / 2 };
  }
  /** the room a camera has at (x, z) when following a car on a level: the floor under it and the ceiling over it (Infinity in the open) */
  function spaceAt(level, x, z) {
    const [i, j] = cellAt(x, z), r = rampAt(i, j);
    if (r) { const t = rampT(i, j, r, x, z), [di, dj] = r.v, toFoot = r.part ? h2 - ((x - cx(i)) * di + (z - cz(j)) * dj) : 99;
      return { floor: t * TUN_Y, ceil: toFoot < 1.2 ? TUN_TOP - 0.45 : Infinity }; }          // under the lintel at the tunnel's mouth
    if (level !== 0 && isTun(i, j)) return { floor: TUN_Y, ceil: TUN_TOP - 0.45 };
    return { floor: heightAt(x, z), ceil: Infinity };
  }
  const h2 = CELL / 2;
  /** is a point inside a building (with a margin, in metres)? */
  function inSolid(x, z, m = 0.6) { for (const [x0, z0, x1, z1] of colliders.rects) if (x > x0 - m && x < x1 + m && z > z0 - m && z < z1 + m) return true; return false; }
  /** what is under a point: 'road', 'torv', 'park', 'grass', 'lot' (a solid lot's pavement), or 'out' */
  function surfaceAt(x, z, level = 0) {
    if (level === -1 || level === 'r') return 'road';
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
  const drivable = (i, j) => { if (!inside(i, j)) return false; const k = idx(i, j); if (st.cells[k] === 1) return true; const L = st.lot[k] && st.lots.get(st.lot[k]); return !!L && L.type === 'torv'; };
  function bfs(a, b) {
    const prev = new Int32Array(st.W * st.H).fill(-2), q = [idx(a.i, a.j)]; prev[q[0]] = -1;
    for (let h = 0; h < q.length; h++) {
      const k = q[h]; if (k === idx(b.i, b.j)) break; const i = k % st.W, j = (k / st.W) | 0;
      for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const ni = i + di, nj = j + dj; if (!drivable(ni, nj)) continue; const nk = idx(ni, nj); if (prev[nk] !== -2) continue; prev[nk] = k; q.push(nk); }
    }
    const end = idx(b.i, b.j); if (prev[end] === -2) return null;
    const out = []; for (let k = end; k !== -1; k = prev[k]) out.push(k); out.reverse(); return out;
  }
  /** the way through both levels: nodes are surface cells (0..N-1, roads, squares and ramps) and tunnel cells (N..2N-1) */
  function bfs2(start, end) {
    const N = st.W * st.H, prev = new Int32Array(2 * N).fill(-2), q = [start]; prev[start] = -1;
    const nb = (node, out) => {
      out.length = 0; const under = node >= N, k = node % N, i = k % st.W, j = (k / st.W) | 0;
      if (under) { for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const ni = i + di, nj = j + dj; if (isTun(ni, nj)) out.push(N + idx(ni, nj)); const r = rampAt(ni, nj); if (r && r.part === 1 && ni + r.v[0] === i && nj + r.v[1] === j) out.push(idx(ni, nj)); } return; }
      const r = rampAt(i, j);
      if (r) { const [di, dj] = r.v; if (r.part === 0) { if (drivable(i - di, j - dj)) out.push(idx(i - di, j - dj)); out.push(idx(i + di, j + dj)); } else { out.push(idx(i - di, j - dj)); if (isTun(i + di, j + dj)) out.push(N + idx(i + di, j + dj)); } return; }
      for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const ni = i + di, nj = j + dj; if (drivable(ni, nj)) out.push(idx(ni, nj)); const rr = rampAt(ni, nj); if (rr && rr.part === 0 && ni - rr.v[0] === i && nj - rr.v[1] === j) out.push(idx(ni, nj)); }
    };
    const tmp = [];
    for (let h = 0; h < q.length; h++) { const n = q[h]; if (n === end) break; nb(n, tmp); for (const m of tmp) { if (prev[m] !== -2) continue; prev[m] = n; q.push(m); } }
    if (prev[end] === -2) return null; const out = []; for (let n = end; n !== -1; n = prev[n]) out.push(n % N); out.reverse(); return out;
  }
  /** The way by road from a point (x, z) on a level to a cell, rounded, as points; null if there is none. From off the road it starts at the nearest drivable cell. */
  function pathTo(x, z, ti, tj, level = 0) {
    let [i, j] = cellAt(x, z); i = Math.max(0, Math.min(st.W - 1, i)); j = Math.max(0, Math.min(st.H - 1, j));
    const N = st.W * st.H;
    if (level === -1 || level === 'r' || rampAt(i, j)) {
      const start = rampAt(i, j) ? idx(i, j) : isTun(i, j) ? N + idx(i, j) : null;
      if (start !== null) { const cells = bfs2(start, idx(ti, tj)); if (!cells) return null; return smoothPath(x, z, cells); }
    }
    if (!drivable(i, j)) { let best = null, bd = Infinity; for (let r = 1; r < 6 && !best; r++) for (let a = -r; a <= r; a++) for (let b = -r; b <= r; b++) { if (!drivable(i + a, j + b)) continue; const d = a * a + b * b; if (d < bd) { bd = d; best = [i + a, j + b]; } } if (!best) return null; [i, j] = best; }
    const cells = bfs2(idx(i, j), idx(ti, tj)); if (!cells) return null;
    return smoothPath(x, z, cells);
  }
  function smoothPath(x, z, cells) {
    let pts = [new THREE.Vector2(x, z), ...cells.map(k => new THREE.Vector2(cx(k % st.W), cz((k / st.W) | 0)))];
    for (let it = 0; it < 2; it++) { const o = [pts[0]]; for (let k = 0; k + 1 < pts.length; k++) { const a = pts[k], b2 = pts[k + 1]; o.push(a.clone().lerp(b2, 0.25), a.clone().lerp(b2, 0.75)); } o.push(pts[pts.length - 1]); pts = o; }
    let len = 0; for (let k = 1; k < pts.length; k++) len += pts[k].distanceTo(pts[k - 1]);
    return { pts, len };
  }
  /** show a gate's beacon as cleared (green) or not */
  function markGate(i, j, cleared) { const g = st.gates.find(q => q.i === i && q.j === j); if (!g || !g.dia || g.start) return; g.dia.material.color.set(cleared ? 0x44dd66 : 0xff8a1e); g.pillar.material.color.set(cleared ? 0x44dd66 : 0xffa040); }

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
  function save() { return JSON.stringify({ v: 1, W: st.W, H: st.H, cells: Array.from(st.cells), lots: [...st.lots.values()], gates: st.gates.map(g => ({ i: g.i, j: g.j, start: g.start })), nextId: st.nextId, hts: Array.from(st.hts, v => Math.round(v * 100) / 100), tun: Array.from(st.tun), rdir: Array.from(st.rdir), rpart: Array.from(st.rpart) }); }
  function load(json) {
    /* (older towns may have ground below the board: it is lifted back up to it) */
    let o; try { o = typeof json === 'string' ? JSON.parse(json) : json; } catch (_) { return false; }
    if (!o || o.v !== 1 || !o.W || !o.H) return false;
    reset(o.W, o.H); st.cells.set(o.cells.slice(0, o.W * o.H)); st.nextId = o.nextId || 1;
    if (o.tun && o.tun.length === st.tun.length) st.tun.set(o.tun);
    if (o.rdir && o.rdir.length === st.rdir.length) { st.rdir.set(o.rdir); st.rpart.set(o.rpart || []); }
    if (o.hts && o.hts.length === st.hts.length) st.hts.set(o.hts.map(v => Math.max(0, v)));
    rebuildGround();
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
    /* the fields: a wide disc with a hole where the board is. A whole disc lay just under the town and closed the ramps and tunnels off from the
       sky (from below it showed as a black lid); flat, so it needs no ink and goes in a mesh of its own */
    const fs = new THREE.Shape(); for (let k = 0; k <= 64; k++) { const a = k / 64 * Math.PI * 2; fs[k ? 'lineTo' : 'moveTo'](Math.cos(a) * 2500, Math.sin(a) * 2500); }
    const hole = new THREE.Path(), hx = W + 0.2, hz = H + 0.2; hole.moveTo(-hx, -hz); hole.lineTo(-hx, hz); hole.lineTo(hx, hz); hole.lineTo(hx, -hz); hole.closePath(); fs.holes.push(hole);
    const fields = new Builder(); fields.add(new THREE.ShapeGeometry(fs).rotateX(Math.PI / 2), '#86c467', 0, -0.07, 0);
    for (const [x, z, w, d] of [[0, -H - 0.8, 2 * W + 3.2, 1.2], [0, H + 0.8, 2 * W + 3.2, 1.2], [-W - 0.8, 0, 1.2, 2 * H + 3.2], [W + 0.8, 0, 1.2, 2 * H + 3.2]]) b.box(x, -0.07, z, w, 1.3, d, '#4f9440');
    for (let k = 0; k < 220; k++) {
      const a = r() * Math.PI * 2, rad = 8 + r() * 70, x = Math.cos(a) * (W + rad), z = Math.sin(a) * (H + rad);
      if (Math.abs(x) < W + 6 && Math.abs(z) < H + 6) continue;                    // the ring is an ellipse: near the corners it reached onto the board (trees on the roads)
      tree(b, x, z, r, 1 + r() * 0.8);
    }
    for (let k = 0; k < 16; k++) { const a = k / 16 * Math.PI * 2 + r() * 0.3, d = 520 + r() * 380, h = 60 + r() * 120;
      b.cone(Math.cos(a) * d, -1, Math.sin(a) * d, 160 + r() * 140, h, pick(r, ['#7fb26a', '#6fa363', '#8fbf77', '#7aa7a0'])); }
    /* cartoon clouds: a few puffy heaps of white balls high over the fields */
    for (let k = 0; k < 14; k++) { const a = r() * Math.PI * 2, dd = 160 + r() * 380, cx0 = Math.cos(a) * dd, cz0 = Math.sin(a) * dd, cy = 70 + r() * 70, s = 8 + r() * 10;
      for (let q = 0; q < 6; q++) b.ball(cx0 + (q - 2.5) * s * 0.75, cy + (q % 2 ? s * 0.35 : 0) + (q === 2 || q === 3 ? s * 0.4 : 0), cz0 + (r() - 0.5) * s, s * (q === 2 || q === 3 ? 1.1 : 0.8), '#ffffff', 0.8); }
    const m = ink(new THREE.Mesh(b.build(), mat)); m.name = 'scenery'; m.add(new THREE.Mesh(fields.build(), mat)); return m;
  }

  reset(20, 14);
  return { scenery, inSolid, solidsNear, spaceAt, pathTo, markGate, heightAt, shapeTerrain, reshape, paintTunnel, placeRamp, isTun, rampAt, groundY, levelAt, collideL, showTunnelMap,
    root, st, reset, paintRoad, placeLot, eraseRect, setGate, commit, save, load, starter, route, collide, surfaceAt, cellAt, inside, isRoad, lotOK,
    cx, cz, colliders, gridLines: () => gridLines, ground: () => groundMesh, mat,
  };
}
