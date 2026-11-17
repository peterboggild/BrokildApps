/*
 * Portalkniven — the spaceship you fly through the fractal dimensions (the third-person mode).
 *
 * The owner's model is a Blender script (spaceship_model.py: "procedural exterior reconstruction of the supplied retro-futurist spacecraft", about 20 m long,
 * nose along -X, up +Z). There is no Blender where this is built and no exported file, so the same recipe is followed here in three.js, part for part and
 * with the script's own numbers: the lofted pressure hull, keel and dorsal spine, four engine pods with copper grilles, the tower and antenna, fins, panel
 * seams, service recesses, rivets and aft vents. Blender (x, y, z) becomes three (y, z, x): the nose points along -Z and up is +Y.
 * Everything is merged into one mesh per material (five draw calls).
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';

const V = THREE.Vector3;
const T = (x, y, z) => new V(y, z, x);                              // Blender -> three

export function buildShip() {
  const mats = {
    BLUE: new THREE.MeshStandardMaterial({ color: new THREE.Color(0.22, 0.58, 0.82), metalness: 0.28, roughness: 0.58 }),
    BLUE_DARK: new THREE.MeshStandardMaterial({ color: new THREE.Color(0.10, 0.34, 0.54), metalness: 0.38, roughness: 0.62 }),
    ORANGE: new THREE.MeshStandardMaterial({ color: new THREE.Color(0.96, 0.48, 0.20), metalness: 0.35, roughness: 0.48 }),
    DARK: new THREE.MeshStandardMaterial({ color: new THREE.Color(0.035, 0.065, 0.085), metalness: 0.55, roughness: 0.65 }),
    METAL: new THREE.MeshStandardMaterial({ color: new THREE.Color(0.36, 0.43, 0.47), metalness: 0.72, roughness: 0.42 }),
  };
  const bins = { BLUE: [], BLUE_DARK: [], ORANGE: [], DARK: [], METAL: [] };
  const put = (m, g) => { g.deleteAttribute('uv'); bins[m].push(g); return g; };
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), UP = new V(0, 1, 0);

  /* an elliptical loft along the ship's length: sections are (x, half width y, half height z, z centre), smoothed between (Blender's subdivision) */
  function loft(m, sections, n, offY = 0) {
    const cr = (a, b, c, d, t) => 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t * t * t);
    const S = []; const sub = 3;
    for (let i = 0; i < sections.length - 1; i++) for (let k = 0; k < sub; k++) {
      const t = k / sub, p = [sections[Math.max(0, i - 1)], sections[i], sections[i + 1], sections[Math.min(sections.length - 1, i + 2)]];
      S.push([0, 1, 2, 3].map(c => (c === 0 ? p[1][0] + (p[2][0] - p[1][0]) * t : c === 3 ? cr(p[0][3], p[1][3], p[2][3], p[3][3], t) : Math.max(0.02, cr(p[0][c], p[1][c], p[2][c], p[3][c], t)))));
    }
    S.push(sections[sections.length - 1]);
    const pos = [], idx = [];
    for (const [x, ry, rz, zc] of S) for (let j = 0; j < n; j++) { const a = 2 * Math.PI * j / n, p = T(x, offY + ry * Math.cos(a), zc + rz * Math.sin(a)); pos.push(p.x, p.y, p.z); }
    for (let i = 0; i < S.length - 1; i++) for (let j = 0; j < n; j++) { const a = i * n + j, b = i * n + (j + 1) % n; idx.push(a, a + n, b, b, a + n, b + n); }
    const c0 = pos.length / 3; { const p = T(S[0][0], offY, S[0][3]); pos.push(p.x, p.y, p.z); } const c1 = c0 + 1; { const L = S[S.length - 1], p = T(L[0], offY, L[3]); pos.push(p.x, p.y, p.z); }
    for (let j = 0; j < n; j++) { idx.push(c0, j, (j + 1) % n); const o = (S.length - 1) * n; idx.push(c1, o + (j + 1) % n, o + j); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals();
    return put(m, g);
  }
  const cube = (m, loc, dim) => { const g = new THREE.BoxGeometry(dim[1], dim[2], dim[0]); g.translate(loc[1], loc[2], loc[0]); return put(m, g); };
  const ball = (m, loc, sc, seg = 12, rings = 8) => { const g = new THREE.SphereGeometry(1, seg, rings); g.scale(sc[1], sc[2], sc[0]); g.translate(loc[1], loc[2], loc[0]); return put(m, g); };
  const beam = (m, a, b, r, sides = 8) => { const A = T(...a), B = T(...b), d = new V().subVectors(B, A), g = new THREE.CylinderGeometry(r, r, d.length(), sides);
    g.applyMatrix4(m4.compose(new V().addVectors(A, B).multiplyScalar(0.5), q.setFromUnitVectors(UP, d.clone().normalize()), new V(1, 1, 1))); return put(m, g); };
  const seam = (m, pts, r) => put(m, new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts.map(p => T(...p))), pts.length * 2, r, 4, false));
  /* a fin: a flat four-cornered plate with a little thickness */
  const fin = (m, vs, th = 0.12) => {
    const P = vs.map(v => T(...v)), nrm = new V().subVectors(P[1], P[0]).cross(new V().subVectors(P[3], P[0])).normalize().multiplyScalar(th / 2), pos = [], idx = [];
    for (const s of [1, -1]) for (const p of P) pos.push(p.x + nrm.x * s, p.y + nrm.y * s, p.z + nrm.z * s);
    idx.push(0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6); for (let k = 0; k < 4; k++) { const a = k, b = (k + 1) % 4; idx.push(a, a + 4, b, b, a + 4, b + 4); }
    const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); const ni = g.toNonIndexed(); ni.computeVertexNormals();
    ni.setIndex([...Array(ni.attributes.position.count).keys()]); return put(m, ni);
  };

  /* ---- primary hull, keel, dorsal spine ---- */
  loft('BLUE', [[-9.2, .08, .10, 0], [-8.8, 1.25, .85, 0], [-7.8, 2.45, 1.45, 0], [-5.8, 3.25, 1.9, 0], [-2.5, 3.65, 2.15, 0], [1.0, 3.55, 2.05, 0], [4.5, 3.0, 1.75, 0], [7.0, 2.15, 1.35, 0], [8.8, 1.15, .85, 0], [9.4, .12, .12, 0]], 48);
  loft('BLUE_DARK', [[-7.5, .35, .32, -1.35], [-5, .52, .42, -1.48], [0, .48, .40, -1.45], [5, .34, .30, -1.15], [7, .12, .12, -.95]], 20);
  loft('BLUE', [[-4, .18, .18, 1.55], [-1, .42, .38, 1.85], [2, .55, .52, 1.95], [5, .38, .36, 1.65], [7, .12, .12, 1.35]], 24);
  /* ---- four engine / radiator pods, their copper faces towards the nose ---- */
  for (const side of [-1, 1]) for (const z of [-1.25, 1.25]) {
    const y = side * 3.55;
    loft('BLUE', [[-4.6, .10, .12, z], [-4.25, .72, .72, z], [-3.45, .82, .78, z], [-1.0, .78, .72, z], [-.35, .55, .55, z], [0, .12, .14, z]], 28, y);
    cube('DARK', [-4.32, y, z], [0.10, 1.08, 1.02]); cube('ORANGE', [-4.40, y, z], [0.12, 1.02, .96]); cube('DARK', [-4.475, y, z], [0.035, .84, .76]);
    for (let k = 0; k < 6; k++) cube('ORANGE', [-4.51, y, z - .31 + k * .124], [.045, .78, .035]);
    for (const yy of [-.27, -.09, .09, .27]) for (const zz of [-.19, .19]) ball('METAL', [-4.54, y + yy, z + zz], [.055, .055, .055]);
    cube('BLUE_DARK', [-1.8, y + side * .79, z], [.95, .055, .46]);
    for (let k = 0; k < 3; k++) cube('METAL', [-2.1 + k * .28, y + side * .83, z], [.045, .025, .32]);
  }
  /* ---- central tower and antenna ---- */
  ball('BLUE', [1.8, 0, 2.05], [1.55, 1.35, 1.25], 24, 12);
  loft('BLUE', [[.9, .85, .65, 2.5], [1.5, 1.0, .9, 2.75], [2.3, .9, .82, 2.8], [2.9, .58, .55, 2.85]], 32);
  for (const [z, sx, sy, sz] of [[3.15, 1.25, 1.35, .52], [3.65, 1.0, 1.12, .48], [4.10, .72, .82, .38], [4.43, .48, .56, .28]]) cube('BLUE', [2.05, 0, z], [sx, sy, sz]);
  beam('BLUE', [2.05, 0, 4.55], [2.05, 0, 7.0], .055, 10); beam('METAL', [2.05, -.42, 6.35], [2.05, .42, 6.35], .035); ball('BLUE', [2.05, 0, 7.02], [.09, .09, .16]);
  for (const side of [-1, 1]) beam('METAL', [1.75, side * .72, 3.75], [1.75, side * 1.18, 3.9], .035);
  /* ---- fins, pylons and stabilizers ---- */
  for (const side of [-1, 1]) {
    fin('BLUE', [[-5.8, side * 1.1, -1.0], [-3.0, side * 1.5, -1.3], [-1.7, side * 2.0, -3.15], [-3.8, side * 1.6, -2.45]]);
    fin('BLUE', [[1.0, side * 2.7, .15], [4.6, side * 3.0, .35], [6.4, side * 3.6, 1.15], [3.2, side * 3.0, 1.0]]);
    beam('BLUE_DARK', [-3.3, side * 1.3, -1.1], [-2.7, side * 1.75, -2.0], .11, 10);
  }
  /* ---- panel seams over the upper hull ---- */
  for (const x of [-6.8, -5.1, -3.2, -1.0, 1.2, 3.4, 5.4, 7.0]) {
    const ry = x < 2 ? 3.55 : Math.max(1.3, 3.55 - (x - 2) * .45), rz = x < 2 ? 2.0 : Math.max(.8, 2.0 - (x - 2) * .22), pts = [];
    for (let j = 0; j < 17; j++) { const y = -ry + 2 * ry * j / 16; pts.push([x, y, rz * Math.sqrt(Math.max(.02, 1 - (y / ry) ** 2)) + .025]); }
    seam('BLUE_DARK', pts, .02);
  }
  for (const side of [-1, 1]) for (const zoff of [.55, 1.05]) {
    const pts = []; for (let i = 0; i < 30; i++) { const x = -6.8 + i * 13.2 / 29, ry = x < 1 ? 3.5 : Math.max(1.3, 3.5 - (x - 1) * .40), rz = x < 1 ? 2.0 : Math.max(.8, 2.0 - (x - 1) * .18);
      pts.push([x, side * ry * .72, rz * Math.sqrt(1 - .72 ** 2) + zoff * .10]); }
    seam('BLUE_DARK', pts, .025);
  }
  /* ---- service recesses with their blocks and fasteners, rivets along the shoulders, aft vents ---- */
  for (const side of [-1, 1]) {
    for (const x of [-2.8, .2, 3.0]) { const y = side * 2.72, z = .75; cube('DARK', [x, y, z], [1.25, .09, .55]);
      for (let k = 0; k < 5; k++) { cube('METAL', [x - .48 + k * .23, y + side * .07, z], [.14, .08, .30]); ball('ORANGE', [x - .48 + k * .23, y + side * .12, z + .20], [.035, .025, .035], 10, 6); } }
    for (const x of [-6, -4.5, -3, -1.5, 0, 1.5, 3, 4.5, 6]) ball('METAL', [x, side * 2.55, 1.18], [.045, .045, .045], 10, 6);
    for (const z of [-.55, 0, .55]) { cube('DARK', [7.9, side * 1.0, z], [.08, .72, .12]); cube('METAL', [7.84, side * 1.0, z + .09], [.12, .78, .035]); }
  }

  const ship = new THREE.Group();
  for (const k in bins) { const mesh = new THREE.Mesh(mergeGeometries(bins[k], false), mats[k]); mesh.renderOrder = 46; ship.add(mesh); }
  /* the drive: not in the drawings, so only a glow behind the tail, to show how hard it is pushing */
  const cv = document.createElement('canvas'); cv.width = cv.height = 64; const x = cv.getContext('2d'), gr = x.createRadialGradient(32, 32, 0, 32, 32, 32);
  gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.3, 'rgba(150,210,255,.6)'); gr.addColorStop(1, 'rgba(60,140,255,0)'); x.fillStyle = gr; x.fillRect(0, 0, 64, 64);
  const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0 }));
  glow.position.set(0, 0, 9.9); glow.scale.setScalar(3); glow.renderOrder = 47; ship.add(glow); ship.userData.glow = glow;
  return ship;
}
