/*
 * Portalkniven — the spaceship you fly through the fractal dimensions (the third-person mode).
 *
 * Built from the owner's drawings (Spaceship A to D: nine views of one ship): a broad hull with a shovel-shaped bow whose lower lip runs out ahead of it, a
 * tall tower amidships that flares out of the hull and ends in a bulb and a mast, a long aerial hanging below, an upright tail fin with an engine pod on
 * top, an engine pod on a stub wing each side of the tail, a large pod and a small one on each flank, two splayed fins under the tail, recessed bays full
 * of machinery along the flanks, and the band's sigil on the flank pods and the tail fin. Pale blue plating in three shades, orange grilles, drawn outlines.
 *
 * There is no model file: the ship is made here, in code. An earlier version followed models/spaceship_model.py (a first reconstruction for Blender, kept
 * for reference); this one follows the drawings. Design coordinates are that script's: x along the ship (nose at -x), y across, z up, about 19 units long;
 * three.js gets (y, z, x), so the nose points along -Z and up is +Y. All painted parts are one mesh with vertex colours, plus one mesh for the outlines.
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';

const V = THREE.Vector3;
const T = (x, y, z) => new V(y, z, x);                              // design -> three
const col = (r, g, b) => new THREE.Color().setRGB(r / 255, g / 255, b / 255, THREE.SRGBColorSpace);
const C = {
  hull: [col(118, 172, 222), col(96, 148, 200), col(150, 198, 240)], pod: col(132, 190, 238), light: col(160, 206, 246), shade: col(100, 150, 198),
  orange: col(246, 178, 120), orangeD: col(214, 136, 74), mech: col(140, 166, 190), mechD: col(96, 120, 146), bay: col(52, 70, 92), line: col(22, 34, 52),
};

export function buildShip() {
  let seed = 7; const rnd = (a = 0, b = 1) => { seed = (seed * 16807) % 2147483647; return a + (seed / 2147483647) * (b - a); };
  const paint = [], lines = [];
  const tint = (g, c) => { const n = g.attributes.position.count, a = new Float32Array(n * 3); for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; } g.setAttribute('color', new THREE.BufferAttribute(a, 3)); return g; };
  const put = (g, c, outline = true) => { g.deleteAttribute('uv'); if (g.index) g = g.toNonIndexed(); if (!g.attributes.color) tint(g, c); paint.push(g); if (outline) lines.push(g); return g; };
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), UP = new V(0, 1, 0);
  const se = (v, n) => Math.sign(v) * Math.pow(Math.abs(v), 2 / n);                       // superellipse: n = 2 is round, larger is boxier
  const cr = (a, b, c, d, t) => 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t * t * t);
  const at = (keys, u) => { const k = Math.min(keys.length - 2, Math.floor(u)), t = u - k, p = [keys[Math.max(0, k - 1)], keys[k], keys[k + 1], keys[Math.min(keys.length - 1, k + 2)]];
    return keys[0].map((_, c) => (c === 0 ? p[1][0] + (p[2][0] - p[1][0]) * t : cr(p[0][c], p[1][c], p[2][c], p[3][c], t))); };

  /* A body lofted along the ship: keys are [x, half width, top z, bottom z, boxiness]; offY moves it sideways. Returns a function giving a point on its skin. */
  function loftX(keys, o = {}) {
    const nx = o.nx || (keys.length - 1) * 4, nj = o.nj || 32, offY = o.offY || 0, S = [];
    for (let i = 0; i <= nx; i++) S.push(at(keys, i / nx * (keys.length - 1)));
    const pt = (s, a) => T(s[0], offY + s[1] * se(Math.cos(a), s[4]), (s[2] + s[3]) / 2 + (s[2] - s[3]) / 2 * se(Math.sin(a), s[4]));
    const pos = [], idx = [];
    for (const s of S) for (let j = 0; j < nj; j++) { const p = pt(s, 2 * Math.PI * j / nj); pos.push(p.x, p.y, p.z); }
    for (let i = 0; i < nx; i++) for (let j = 0; j < nj; j++) { const a = i * nj + j, b = i * nj + (j + 1) % nj; idx.push(a, b, a + nj, b, b + nj, a + nj); }
    const c0 = pos.length / 3; for (const s of [S[0], S[nx]]) { const p = T(s[0], offY, (s[2] + s[3]) / 2); pos.push(p.x, p.y, p.z); }
    for (let j = 0; j < nj; j++) { idx.push(c0, (j + 1) % nj, j); const e = nx * nj; idx.push(c0 + 1, e + j, e + (j + 1) % nj); }
    let g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals(); g = g.toNonIndexed();
    if (o.panels) {                                                 // plating: blocks of the skin in three shades of blue, like the drawings
      const n = g.attributes.position.count, a = new Float32Array(n * 3), pw = o.panels[0], ph = o.panels[1];
      for (let v = 0; v < n; v++) { const quad = Math.floor(v / 6), i = Math.floor(quad / nj), j = quad % nj; let c = o.col || C.hull[0];
        if (quad < nx * nj) { const pi = Math.floor(i / pw), pj = Math.floor((j + (pi % 2) * Math.floor(ph / 2)) / ph), hh = Math.abs(Math.sin(pi * 12.9898 + pj * 78.233) * 43758.5453) % 1; c = C.hull[Math.floor(hh * 3)]; }
        a[v * 3] = c.r; a[v * 3 + 1] = c.g; a[v * 3 + 2] = c.b; }
      g.setAttribute('color', new THREE.BufferAttribute(a, 3));
    }
    put(g, o.col || C.hull[0]);
    return (x, a, lift = 0) => { let u = 0; for (let k = 0; k < keys.length - 1; k++) if (x >= keys[k][0]) u = k + (x - keys[k][0]) / (keys[k + 1][0] - keys[k][0]); const s = at(keys, Math.min(keys.length - 1.001, Math.max(0, u)));
      const p = pt(s, a); if (lift) { const c = T(s[0], offY, (s[2] + s[3]) / 2); p.add(new V().subVectors(p, c).setZ(0).normalize().multiplyScalar(lift)); } return p; };
  }
  /* A body lofted upwards: keys are [z, half length along the ship, half width, x centre]. */
  function loftZ(keys, c, nj = 28, sub = 4) {
    const n = (keys.length - 1) * sub, pos = [], idx = [];
    for (let i = 0; i <= n; i++) { const s = at(keys, i / n * (keys.length - 1)); for (let j = 0; j < nj; j++) { const a = 2 * Math.PI * j / nj, p = T(s[3] + Math.max(0.01, s[1]) * Math.cos(a), Math.max(0.01, s[2]) * Math.sin(a), s[0]); pos.push(p.x, p.y, p.z); } }
    const up = keys[keys.length - 1][0] > keys[0][0];
    for (let i = 0; i < n; i++) for (let j = 0; j < nj; j++) { const a = i * nj + j, b = i * nj + (j + 1) % nj; if (up) idx.push(a, b, a + nj, b, b + nj, a + nj); else idx.push(a, a + nj, b, b, a + nj, b + nj); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals(); return put(g, c);
  }
  const cube = (c, loc, dim, outline = true) => { const g = new THREE.BoxGeometry(dim[1], dim[2], dim[0]); g.translate(loc[1], loc[2], loc[0]); return put(g, c, outline); };
  const ball = (c, loc, sc, seg = 14, rings = 10) => { const g = new THREE.SphereGeometry(1, seg, rings); g.scale(sc[1], sc[2], sc[0]); g.translate(loc[1], loc[2], loc[0]); return put(g, c); };
  const beam = (c, a, b, r, r2 = r, sides = 8) => { const A = T(...a), B = T(...b), d = new V().subVectors(B, A), g = new THREE.CylinderGeometry(r2, r, d.length(), sides);
    g.applyMatrix4(m4.compose(new V().addVectors(A, B).multiplyScalar(0.5), q.setFromUnitVectors(UP, d.clone().normalize()), new V(1, 1, 1))); return put(g, c); };
  /* a flat plate with four corners and a little thickness */
  const plate = (c, vs, th = 0.14) => {
    const P = vs.map(v => T(...v)), nrm = new V().subVectors(P[1], P[0]).cross(new V().subVectors(P[3], P[0])).normalize().multiplyScalar(th / 2), pos = [], idx = [];
    for (const s of [1, -1]) for (const p of P) pos.push(p.x + nrm.x * s, p.y + nrm.y * s, p.z + nrm.z * s);
    idx.push(0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6); for (let k = 0; k < 4; k++) { const a = k, b = (k + 1) % 4; idx.push(a, a + 4, b, b, a + 4, b + 4); }
    let g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g = g.toNonIndexed(); g.computeVertexNormals(); return put(g, c);
  };
  const seam = (pts, r = 0.028, closed = false) => { const g = new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts, closed), pts.length * 2, r, 4, closed); return put(g, C.line, false); };

  /* ---- the hull: broad and deep forward, the lower lip of the bow running out ahead, tapering to a round tail ---- */
  const HULL = [[-9.55, 0.9, -1.25, -1.75, 2.0], [-9.3, 2.0, -0.7, -2.0, 2.3], [-8.8, 2.75, 0.2, -2.25, 2.5], [-8.0, 3.15, 0.95, -2.4, 2.6], [-6.5, 3.35, 1.4, -2.45, 2.6], [-4.0, 3.4, 1.65, -2.4, 2.5],
    [-1.0, 3.3, 1.8, -2.3, 2.4], [2.0, 2.9, 1.65, -2.05, 2.3], [4.5, 2.2, 1.35, -1.65, 2.2], [6.5, 1.5, 1.05, -1.25, 2.1], [8.3, 1.0, 0.8, -0.95, 2.0], [9.5, 0.85, 0.7, -0.8, 2.0]];
  const skin = loftX(HULL, { nx: 66, nj: 48, panels: [5, 6] });
  /* plating seams: rings round the hull and lines along it */
  for (const x of [-8.6, -7.2, -5.6, -4.0, -2.4, -0.6, 1.2, 3.0, 4.8, 6.6, 8.2]) { const pts = []; for (let j = 0; j < 40; j++) pts.push(skin(x, 2 * Math.PI * j / 40, 0.012)); seam(pts, 0.024, true); }
  for (const a of [0.0, 0.42, 0.95, -0.5, -1.05, 1.5708, -1.5708]) for (const s of (Math.abs(Math.abs(a) - 1.5708) < 0.01 ? [1] : [1, -1])) { const pts = []; for (let k = 0; k <= 30; k++) pts.push(skin(-9.1 + k * 18.4 / 30, s > 0 ? a : Math.PI - a, 0.012)); seam(pts, 0.022); }
  /* the tail's end: two engine openings, a large one over a small one */
  cube(C.orange, [9.53, 0, 0.18], [0.08, 1.0, 0.62]); cube(C.orangeD, [9.53, 0, -0.5], [0.08, 0.7, 0.3]);
  for (let k = 0; k < 4; k++) cube(C.line, [9.58, 0, -0.02 + k * 0.14], [0.03, 0.86, 0.025], false);
  /* a raised spine along the top of the bow, with a small window at its head */
  loftX([[-8.75, 0.3, 0.75, 0.2, 3], [-8.4, 0.5, 1.2, 0.3, 3], [-6.5, 0.48, 1.75, 0.9, 3], [-4.2, 0.4, 1.95, 1.2, 3], [-3.0, 0.2, 1.9, 1.4, 3]], { nj: 16, col: C.light });
  cube(C.bay, [-8.72, 0, 0.88], [0.06, 0.34, 0.26]);

  /* ---- the tower: flaring out of the hull, a waist, a bulb, stepped decks, the mast ---- */
  const TX = -1.5;
  loftZ([[1.25, 3.0, 2.1, TX + 0.3], [1.9, 1.9, 1.35, TX + 0.2], [2.7, 1.2, 0.95, TX], [3.5, 0.98, 0.86, TX], [4.0, 1.12, 1.02, TX], [4.6, 1.18, 1.06, TX], [5.1, 0.95, 0.86, TX], [5.45, 0.6, 0.56, TX], [5.6, 0.3, 0.3, TX]], C.pod, 32);
  for (const z of [3.52, 4.3, 5.02]) { const pts = [], s = z < 4 ? [0.99, 0.87] : z < 5 ? [1.19, 1.07] : [1.0, 0.9]; for (let j = 0; j < 28; j++) { const a = 2 * Math.PI * j / 28; pts.push(T(TX + s[0] * Math.cos(a), s[1] * Math.sin(a), z)); } seam(pts, 0.022, true); }
  for (const [z, r] of [[5.75, 0.36], [6.0, 0.27], [6.22, 0.2]]) { const g = new THREE.CylinderGeometry(r * 0.9, r, 0.26, 14); g.translate(0, z, TX); put(g, C.light); }
  beam(C.hull[0], [TX, 0, 6.3], [TX, 0, 8.3], 0.06, 0.03); beam(C.hull[0], [TX, -0.42, 7.5], [TX, 0.42, 7.5], 0.035); beam(C.hull[0], [TX, -0.2, 7.05], [TX, 0.2, 7.05], 0.03); cube(C.hull[0], [TX, 0, 8.36], [0.12, 0.05, 0.2]);
  beam(C.hull[0], [TX + 0.5, 0.25, 5.3], [TX + 0.5, 0.25, 6.4], 0.03); for (const s of [-1, 1]) cube(C.light, [TX, s * 1.15, 4.75], [0.5, 0.42, 0.07]);
  /* the lower dome behind it, and the ridge that runs down from the tower to the tail, crowded with machinery */
  loftZ([[2.3, 1.2, 0.8, 0.2], [3.0, 0.8, 0.62, 0.15], [3.6, 0.72, 0.6, 0.1], [4.0, 0.5, 0.45, 0.1], [4.15, 0.2, 0.2, 0.1]], C.hull[2], 24);
  loftX([[-0.2, 0.5, 2.6, 1.5, 3], [1.5, 0.5, 2.35, 1.4, 3], [4.0, 0.4, 1.9, 1.1, 3], [6.2, 0.28, 1.45, 0.9, 3], [7.2, 0.12, 1.15, 0.8, 3]], { nj: 14, col: C.hull[1] });
  for (let k = 0; k < 26; k++) { const x = 0.6 + k * 0.22 + rnd(-0.05, 0.05), z = 2.55 - (x + 0.2) * 0.19; (rnd() < 0.5 ? ball : cube)(rnd() < 0.5 ? C.mech : C.mechD, [x, rnd(-0.32, 0.32), z + rnd(0, 0.12)], [rnd(0.1, 0.2), rnd(0.08, 0.16), rnd(0.08, 0.2)]); }
  beam(C.hull[0], [3.6, 0.1, 1.9], [3.6, 0.1, 2.75], 0.03);

  /* ---- engine pods: rounded boxes with an orange grille at each end ---- */
  function pod(cx, cy, cz, len, w, h, c = C.pod) {
    const x0 = cx - len / 2, x1 = cx + len / 2, T2 = cz + h / 2, B2 = cz - h / 2;
    loftX([[x0, w * 0.43, cz + h * 0.43, cz - h * 0.43, 5], [x0 + 0.22, w / 2, T2, B2, 5], [cx, w / 2, T2, B2, 5], [x1 - 0.22, w / 2, T2, B2, 5], [x1, w * 0.43, cz + h * 0.43, cz - h * 0.43, 5]], { offY: cy, nj: 24, nx: 12, col: c });
    for (const [xe, d] of [[x0, -1], [x1, 1]]) {
      cube(C.orange, [xe + d * 0.02, cy, cz], [0.06, w * 0.7, h * 0.72]);
      for (let k = 0; k < 5; k++) cube(C.line, [xe + d * 0.055, cy, cz - h * 0.3 + k * h * 0.066], [0.02, w * 0.62, 0.02], false);      // slats below
      for (let k = 0; k < 3; k++) cube(C.orangeD, [xe + d * 0.06, cy - w * 0.2 + k * w * 0.2, cz + h * 0.16], [0.04, w * 0.14, h * 0.2]);      // blocks above
    }
  }
  /* on top of the tail fin; the fin itself rises out of the tail with a hollow leading edge */
  { const sh = new THREE.Shape(); sh.moveTo(4.4, 0.95); sh.quadraticCurveTo(7.0, 1.3, 7.35, 3.2); sh.lineTo(7.35, 4.45); sh.lineTo(9.05, 4.45); sh.lineTo(9.05, 0.55); sh.lineTo(4.4, 0.75);
    const g = new THREE.ExtrudeGeometry(sh, { depth: 0.5, bevelEnabled: true, bevelThickness: 0.06, bevelSize: 0.06, bevelSegments: 2, curveSegments: 14 }); g.translate(0, 0, -0.25); g.rotateY(-Math.PI / 2); put(g, C.light); }
  pod(8.15, 0, 5.15, 3.0, 1.9, 1.5, C.light);
  /* one each side of the tail, on a stub wing, with a spike trailing behind */
  for (const s of [-1, 1]) {
    pod(8.5, s * 2.75, -0.15, 2.3, 1.5, 1.3);
    plate(C.hull[0], [[7.7, s * 0.75, -0.1], [9.1, s * 0.75, -0.1], [9.1, s * 2.1, -0.15], [8.0, s * 2.1, -0.15]], 0.16);
    beam(C.hull[0], [9.6, s * 3.25, -0.5], [10.7, s * 3.3, -0.5], 0.1, 0.01);
    /* the large pod on the flank, and the small one below and behind it, with the machinery between */
    pod(0.1, s * 3.85, -0.2, 2.5, 1.3, 2.0);
    pod(2.9, s * 3.2, -1.05, 1.5, 0.95, 1.1);
    for (let k = 0; k < 16; k++) (rnd() < 0.5 ? ball : cube)(rnd() < 0.5 ? C.mech : C.mechD, [rnd(1.4, 2.6), s * rnd(3.0, 3.75), rnd(-0.9, 0.5)], [rnd(0.14, 0.3), rnd(0.12, 0.26), rnd(0.12, 0.26)]);
    beam(C.mechD, [1.3, s * 3.6, -0.1], [2.5, s * 3.3, -0.5], 0.3, 0.22, 12);
    /* two small nacelles low on the bow, each with a dark mouth */
    for (const dz of [0, -0.34]) { loftX([[-6.6, 0.2, -0.75 + dz, -1.11 + dz, 2], [-6.3, 0.24, -0.7 + dz, -1.16 + dz, 2], [-5.4, 0.2, -0.75 + dz, -1.1 + dz, 2], [-4.4, 0.03, -0.9 + dz, -0.96 + dz, 2]], { offY: s * (3.42 + dz * 0.25), nj: 12, nx: 9, col: C.pod });
      ball(C.bay, [-6.6, s * (3.42 + dz * 0.25), -0.93 + dz], [0.05, 0.11, 0.11]); }
    /* two fins splayed out and down under the tail, each on a strut */
    plate(C.hull[0], [[6.0, s * 1.55, -2.0], [7.3, s * 1.5, -1.9], [8.3, s * 2.55, -3.5], [7.4, s * 2.6, -3.6]], 0.12);
    beam(C.hull[1], [6.6, s * 0.8, -1.05], [6.9, s * 1.75, -2.35], 0.1, 0.07);
  }
  /* ---- the long aerial under the hull, and a dish on the port shoulder ---- */
  loftZ([[-2.05, 0.75, 0.3, -2.3], [-2.6, 0.42, 0.18, -2.3], [-3.4, 0.24, 0.13, -2.3], [-4.6, 0.14, 0.09, -2.3], [-5.8, 0.09, 0.06, -2.3], [-6.9, 0.03, 0.03, -2.3]], C.hull[1], 14);
  beam(C.hull[1], [-2.3, -0.3, -5.5], [-2.3, 0.3, -5.5], 0.03); beam(C.hull[1], [-2.3, -0.16, -6.05], [-2.3, 0.16, -6.05], 0.025);
  { const p = skin(-4.6, 1.02, 0); beam(C.hull[0], [p.z, p.x, p.y], [p.z, p.x + 0.25, p.y + 0.45], 0.035); const g = new THREE.CylinderGeometry(0.3, 0.3, 0.05, 16); g.rotateZ(1.1); g.translate(p.x + 0.3, p.y + 0.5, p.z); put(g, C.shade); }

  /* ---- bays let into the flanks, full of machinery ---- */
  function bay(xa, xb, aa, ab, n, taper = 0) {
    for (const s of [1, -1]) {
      const A = a => (s > 0 ? a : Math.PI - a), nx = 8, na = 4, pos = [], idx = [];
      const span = u => { const m = taper ? 0.5 * (1 - taper * u) : 0.5, c = (aa + ab) / 2, h = (ab - aa) * m; return [c - h, c + h]; };      // a tapered bay narrows towards its far end
      for (let i = 0; i <= nx; i++) { const u = i / nx, [a0, a1] = span(u); for (let j = 0; j <= na; j++) { const p = skin(xa + (xb - xa) * u, A(a0 + (a1 - a0) * j / na), 0.02); pos.push(p.x, p.y, p.z); } }
      for (let i = 0; i < nx; i++) for (let j = 0; j < na; j++) { const a = i * (na + 1) + j, b = a + 1, c = a + na + 1, d = c + 1; if (s > 0) idx.push(a, b, c, b, d, c); else idx.push(a, c, b, b, c, d); }
      const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals(); put(g, C.bay, false);
      for (let k = 0; k < n; k++) { const u = rnd(0.04, 0.96), [a0, a1] = span(u), x = xa + (xb - xa) * u, a = A(a0 + (a1 - a0) * rnd(0.12, 0.88)), p = skin(x, a, 0.06), p2 = skin(x, a, 0.3);
        const g2 = rnd() < 0.55 ? new THREE.SphereGeometry(1, 8, 6) : new THREE.BoxGeometry(1.6, 1.2, 1.6); g2.scale(rnd(0.07, 0.15), rnd(0.05, 0.09), rnd(0.07, 0.17));
        g2.applyMatrix4(m4.compose(p, q.setFromUnitVectors(UP, p2.sub(p).normalize()), new V(1, 1, 1))); put(g2, rnd() < 0.6 ? C.mech : C.mechD); }
      const rim = []; for (let i = 0; i <= nx; i++) rim.push(skin(xa + (xb - xa) * i / nx, A(span(i / nx)[0]), 0.03)); for (let i = nx; i >= 0; i--) rim.push(skin(xa + (xb - xa) * i / nx, A(span(i / nx)[1]), 0.03)); seam(rim, 0.026, true);
    }
  }
  bay(-7.3, -3.7, 0.52, 0.98, 46, 0.75);                             // the long tapering bay on the shoulder of the bow
  bay(1.9, 4.9, 0.3, 0.62, 36, 0.4); bay(-3.2, -1.0, 0.86, 1.12, 22); bay(-5.6, -4.2, -0.62, -0.42, 12); bay(0.4, 1.7, -0.72, -0.5, 12); bay(3.4, 4.6, 0.86, 1.08, 12);
  /* rows of rivets along the shoulder plates */
  for (const s of [1, -1]) for (let k = 0; k < 9; k++) { const p = skin(-6.6 + k * 0.42, s > 0 ? 1.06 : Math.PI - 1.06, 0.02); const g = new THREE.SphereGeometry(0.045, 6, 4); g.translate(p.x, p.y, p.z); put(g, C.line, false); }

  /* ---- assemble: the painted ship, and its drawn outline (the same shapes a little larger, black, seen from inside) ---- */
  const ship = new THREE.Group();
  const body = new THREE.Mesh(mergeGeometries(paint, false), new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0, roughness: 0.9, envMapIntensity: 0.3 })); body.renderOrder = 46; ship.add(body);
  const og = mergeGeometries(lines.map(g => { const c = g.clone(), p = c.attributes.position, n = c.attributes.normal; for (let i = 0; i < p.count; i++) p.setXYZ(i, p.getX(i) + n.getX(i) * 0.07, p.getY(i) + n.getY(i) * 0.07, p.getZ(i) + n.getZ(i) * 0.07); c.deleteAttribute('color'); return c; }), false);
  const outline = new THREE.Mesh(og, new THREE.MeshBasicMaterial({ color: 0x0b1420, side: THREE.BackSide })); outline.renderOrder = 46; ship.add(outline);
  /* the band's sigil, on the flank pods and on both sides of the tail fin */
  const tex = new THREE.TextureLoader().load('img/segl.png'); tex.anisotropy = 4;
  const sigil = new THREE.MeshBasicMaterial({ color: col(226, 242, 252), alphaMap: tex, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2 });
  const mark = (x, y, z, w, h, side) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), sigil); m.position.copy(T(x, y, z)); m.rotation.y = side > 0 ? Math.PI / 2 : -Math.PI / 2; m.renderOrder = 47; ship.add(m); };
  for (const s of [-1, 1]) { mark(0.1, s * 4.515, -0.2, 0.92, 1.4, s); mark(8.2, s * 0.325, 2.75, 0.95, 1.45, s); }
  /* the drive's glow behind the tail, to show how hard it is pushing */
  const cv = document.createElement('canvas'); cv.width = cv.height = 64; const x2 = cv.getContext('2d'), gr = x2.createRadialGradient(32, 32, 0, 32, 32, 32);
  gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.3, 'rgba(255,200,140,.6)'); gr.addColorStop(1, 'rgba(255,140,60,0)'); x2.fillStyle = gr; x2.fillRect(0, 0, 64, 64);
  const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0 }));
  glow.position.set(0, 0, 10.1); glow.scale.setScalar(3); glow.renderOrder = 48; ship.add(glow); ship.userData.glow = glow;
  return ship;
}
