/* The Lunar Module, outside (stage 5's detailed model, built in code): Challenger as it flew on Apollo 17.
 *
 * Frame (unchanged from the stand-in, so every chapter and the cabin inside still fit): y up from the bottom of the
 * descent engine's bell, +z forward (the windows, the hatch, the porch and the ladder), +x the Commander's side (left
 * as you stand in it). The descent stage's deck is at 2.35, the cabin floor inside at 2.45 (lm-cabin.js), the top of
 * the docking tunnel at 4.75 (userData.top: the docking distances depend on it), the soles of the footpads 0.86 below
 * the origin (PAD_DROP in descent-space.js).
 *
 * Descent stage: an octagon 4.2 m across the flats in crinkled gold and black foil, the engine's bell below. Four
 * legs on the axes (front, back, left, right), each a primary strut from the apex of its outrigger truss down to a
 * footpad 4.3 m out, two secondary struts from the stage's lower corners to its middle, every rod ending on a shared
 * joint; a contact probe under three footpads (not the ladder's). The front leg carries the ladder (nine rungs), and
 * over its outrigger the porch runs from the forward hatch, with a handrail each side.
 * Ascent stage: the cabin, a faceted drum 2.3 m across with its two triangular windows and the square forward
 * hatch; the midsection and the aft equipment bay behind it; a propellant tank in its shroud each side (the
 * Commander's further out, as on the real one); four thruster quads on their booms with plume deflectors under
 * them; the docking tunnel and drogue; the rendezvous radar, the S-band dish, two VHF aerials, the docking target and
 * the small docking window in the roof. The flag and UNITED STATES on the descent stage, as on the real LMs; no
 * NASA insignia.
 *
 * Everything is merged into one mesh per material (a dozen draw calls). userData: setLegs(on) (stowed as in the
 * adapter / deployed), setLanded(on) (the probes gone), top, hatch (the outer hatch, its own mesh), porch and
 * ladder (where they are, for climbing down: stage 5's egress). */
import * as THREE from 'three';
import { mergeGeometries } from '../vendor/BufferGeometryUtils.js';

export const LM = {
  DS_Y0: 0.75, DS_TOP: 2.35, AS_Y: 2.4, APOTHEM: 2.1, TOP: 4.75, FOOT_R: 4.3, FOOT_Y: -0.70,
  DRUM_R: 1.15, DRUM_Y: 3.55, FRONT: 1.25,
  HATCH: { y0: 2.47, y1: 3.2, half: 0.41 },          /* the forward hatch (under the instrument panels inside) */
  PORCH: { y: 2.37, z0: 1.3, z1: 3.02, half: 0.43 }
};

/* crinkled foil: a height field of random flat facets (each cell of a jittered grid its own tilted plane), as a
 * tiling normal map */
function crinkle(n = 256, cells = 14, strength = 2.2, seed = 11) {
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const h = new Float32Array(n * n);
  const layer = (cells, weight) => {
  const C = [];
  for (let j = 0; j < cells; j++) for (let i = 0; i < cells; i++) C.push([(i + rnd()) / cells, (j + rnd()) / cells, (rnd() - 0.5) * 2, (rnd() - 0.5) * 2, rnd()]);
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const u = x / n, v = y / n, ci = Math.floor(u * cells), cj = Math.floor(v * cells);
    let best = 9, val = 0;
    for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
      const c = C[((cj + dj + cells) % cells) * cells + ((ci + di + cells) % cells)];
      let dx = u - c[0], dy = v - c[1]; dx -= Math.round(dx); dy -= Math.round(dy);
      const d = dx * dx + dy * dy;
      if (d < best) { best = d; val = (dx * c[2] + dy * c[3]) * cells * 0.5 + c[4] * 0.15; }
    }
    h[y * n + x] += val * weight;
  }
  };
  layer(cells, 1); layer(cells * 3, 0.22);            /* big folds, fine wrinkles on them */
  const d = new Uint8Array(n * n * 4);
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const hx = h[y * n + (x + 1) % n] - h[y * n + (x + n - 1) % n], hy = h[((y + 1) % n) * n + x] - h[((y + n - 1) % n) * n + x];
    let nx = -hx * strength * 8, ny = -hy * strength * 8, nz = 1; const l = Math.hypot(nx, ny, nz); nx /= l; ny /= l; nz /= l;
    const k = (y * n + x) * 4; d[k] = (nx * 0.5 + 0.5) * 255; d[k + 1] = (ny * 0.5 + 0.5) * 255; d[k + 2] = (nz * 0.5 + 0.5) * 255; d[k + 3] = 255;
  }
  const t = new THREE.DataTexture(d, n, n, THREE.RGBAFormat);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.magFilter = THREE.LinearFilter; t.minFilter = THREE.LinearMipmapLinearFilter; t.generateMipmaps = true; t.anisotropy = 4; t.needsUpdate = true;
  return t;
}
function decal(w, h, draw) {
  const c = document.createElement('canvas'); c.width = w; c.height = h; draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; return t;
}

/* geometry gathered per material, each piece with its place baked in, merged at the end */
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _e = new THREE.Euler(), _p = new THREE.Vector3(), _s = new THREE.Vector3(), _Y = new THREE.Vector3(0, 1, 0), _d = new THREE.Vector3();
class Parts {
  constructor() { this.map = new Map(); }
  addM(geo, mat, matrix) {
    const g = geo.index ? geo.toNonIndexed() : geo.clone();
    for (const k of Object.keys(g.attributes)) if (k !== 'position' && k !== 'normal' && k !== 'uv') g.deleteAttribute(k);
    if (!g.attributes.uv) g.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(g.attributes.position.count * 2), 2));
    g.applyMatrix4(matrix);
    if (!this.map.has(mat)) this.map.set(mat, []);
    this.map.get(mat).push(g);
  }
  add(geo, mat, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1) {
    this.addM(geo, mat, _m.compose(_p.set(x, y, z), _q.setFromEuler(_e.set(rx, ry, rz)), _s.set(sx, sy, sz)));
  }
  box(mat, w, h, d, x, y, z, rx = 0, ry = 0, rz = 0) { this.add(new THREE.BoxGeometry(w, h, d), mat, x, y, z, rx, ry, rz); }
  /* a rod from a to b; r2: the radius at b (a taper) */
  rod(mat, a, b, r, seg = 8, r2 = r) {
    _d.subVectors(b, a); const len = _d.length();
    this.addM(new THREE.CylinderGeometry(r2, r, len, seg), mat, _m.compose(_p.addVectors(a, b).multiplyScalar(0.5), _q.setFromUnitVectors(_Y, _d.normalize()), _s.set(1, 1, 1)));
  }
  ball(mat, p, r) { this.add(new THREE.SphereGeometry(r, 10, 8), mat, p.x, p.y, p.z); }
  tube(mat, pts, r, seg = 24) { this.addM(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), seg, r, 6), mat, _m.identity()); }
  into(group) { for (const [mat, list] of this.map) { const mesh = new THREE.Mesh(mergeGeometries(list), mat); group.add(mesh); } return group; }
}
const V = (x, y, z) => new THREE.Vector3(x, y, z);

export function makeLM(env) {
  const g = new THREE.Group();
  const Std = o => new THREE.MeshStandardMaterial({ envMap: env, ...o });
  const foilN = crinkle(512, 6, 1.5, 11), softN = crinkle(256, 5, 0.6, 5);
  foilN.repeat.set(1.5, 1.5); softN.repeat.set(2, 2);
  const N = (s) => new THREE.Vector2(s, s);
  const gold = Std({ color: 0xcf9a36, metalness: 1.0, roughness: 0.36, envMapIntensity: 1.1, normalMap: foilN, normalScale: N(1.0) });
  const amber = Std({ color: 0xa8651c, metalness: 1.0, roughness: 0.36, envMapIntensity: 1.0, normalMap: foilN, normalScale: N(1.2) });
  const blackFoil = Std({ color: 0x141414, metalness: 0.75, roughness: 0.34, envMapIntensity: 0.9, normalMap: foilN, normalScale: N(0.9) });
  const alu = Std({ color: 0x9b9d9f, metalness: 0.8, roughness: 0.48, envMapIntensity: 0.8, normalMap: softN, normalScale: N(0.5), flatShading: true });
  const aluD = Std({ color: 0xa9abad, metalness: 0.8, roughness: 0.42, envMapIntensity: 0.8, side: THREE.DoubleSide });
  const dark = Std({ color: 0x232425, metalness: 0.5, roughness: 0.55, envMapIntensity: 0.5 });
  const darkD = Std({ color: 0x1b1b1c, metalness: 0.6, roughness: 0.5, envMapIntensity: 0.5, side: THREE.DoubleSide });
  const glass = Std({ color: 0x05070a, metalness: 0.9, roughness: 0.05, envMapIntensity: 1.4 });
  const white = Std({ color: 0xe9e9e4, metalness: 0.1, roughness: 0.7, envMapIntensity: 0.3 });
  const { DS_Y0, DS_TOP, AS_Y, APOTHEM: A, FOOT_R, FOOT_Y, DRUM_R, DRUM_Y, FRONT, HATCH, PORCH } = LM;
  const body = new Parts(), deployed = new Parts(), folded = new Parts(), probes = new Parts();

  /* ================================================================ descent stage */
  const FW = 2 * A * Math.tan(Math.PI / 8), DH = DS_TOP - DS_Y0, DM = (DS_TOP + DS_Y0) / 2;
  for (let k = 0; k < 8; k++) {
    const a = k * Math.PI / 4, sx = Math.sin(a), cz = Math.cos(a), diag = k % 2 === 1;
    body.box(diag ? gold : blackFoil, FW, DH, 0.04, sx * A, DM, cz * A, 0, a, 0);
    if (diag) {
      /* the quads' blankets: overlapping sheets of different foil, taped on a little askew */
      const rear = k === 3 || k === 5;
      body.box(amber, FW * 0.86, DH * 0.42, 0.012, sx * (A + 0.028), DS_Y0 + DH * 0.27, cz * (A + 0.028), 0, a, 0.012);
      body.box(rear ? blackFoil : gold, FW * 0.5, DH * 0.34, 0.012, sx * (A + 0.036) + cz * 0.3, DS_Y0 + DH * 0.75, cz * (A + 0.036) - sx * 0.3, 0, a, -0.02);
      if (rear) body.box(blackFoil, FW * 0.34, DH * 0.5, 0.012, sx * (A + 0.04) - cz * 0.52, DS_Y0 + DH * 0.5, cz * (A + 0.04) + sx * 0.52, 0, a, 0.03);
    } else {
      body.box(gold, FW * 0.3, DH * 0.9, 0.012, sx * (A + 0.028) + cz * 0.58, DM, cz * (A + 0.028) - sx * 0.58, 0, a, 0.01);
      body.box(amber, FW * 0.22, DH * 0.6, 0.012, sx * (A + 0.03) - cz * 0.62, DM - 0.2, cz * (A + 0.03) + sx * 0.62, 0, a, -0.015);
    }
  }
  const RO = A / Math.cos(Math.PI / 8);
  body.add(new THREE.CylinderGeometry(RO, RO, 0.05, 8), gold, 0, DS_TOP - 0.01, 0, 0, Math.PI / 8, 0);
  body.add(new THREE.CylinderGeometry(RO * 0.62, RO * 0.62, 0.02, 8), blackFoil, 0, DS_TOP + 0.02, 0, 0, Math.PI / 8, 0);
  body.add(new THREE.CylinderGeometry(RO, RO, 0.05, 8), blackFoil, 0, DS_Y0, 0, 0, Math.PI / 8, 0);
  /* the descent engine: the bell with its skirt, dark inside */
  body.add(new THREE.CylinderGeometry(0.4, 0.76, 0.75, 28, 1, true), darkD, 0, DS_Y0 - 0.375, 0);
  body.add(new THREE.TorusGeometry(0.76, 0.012, 6, 28), alu, 0, 0.012, 0, Math.PI / 2);
  body.add(new THREE.CylinderGeometry(0.42, 0.42, 0.04, 20), dark, 0, DS_Y0 - 0.03, 0);
  /* the landing radar under the rear, tipped forward and down */
  body.box(alu, 0.5, 0.08, 0.6, 0, DS_Y0 - 0.1, -1.45, 0.3, 0, 0);
  /* the flag (front left quad) and UNITED STATES (front right quad) */
  const flagM = Std({ map: decal(256, 160, (x, w, h) => {
    for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f4f2ec' : '#b22234'; x.fillRect(0, k * h / 13, w, h / 13 + 0.5); }
    x.fillStyle = '#3c3b6e'; x.fillRect(0, 0, w * 0.4, h * 7 / 13);
    x.fillStyle = '#f4f2ec'; for (let r = 0; r < 9; r++) for (let c = 0; c < (r % 2 ? 5 : 6); c++) { x.beginPath(); x.arc((c + (r % 2 ? 1 : 0.5)) * w * 0.4 / 6, (r + 0.6) * h * 7 / 13 / 9.4, 2.2, 0, 7); x.fill(); }
  }), metalness: 0.1, roughness: 0.6, envMapIntensity: 0.3 });
  const usM = Std({ map: decal(512, 80, (x, w, h) => {
    x.fillStyle = '#b7babd'; x.fillRect(0, 0, w, h); x.fillStyle = '#16171a'; x.font = 'bold 54px Arial, Helvetica, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle';
    x.fillText('UNITED STATES', w / 2, h / 2 + 3, w - 24);
  }), metalness: 0.3, roughness: 0.55, envMapIntensity: 0.4 });
  { const a = Math.PI / 4, r = A + 0.05;
    body.add(new THREE.PlaneGeometry(0.56, 0.35), flagM, Math.sin(a) * r, DS_Y0 + DH * 0.72, Math.cos(a) * r, 0, a, 0);
    body.add(new THREE.PlaneGeometry(1.1, 0.172), usM, Math.sin(-a) * r, DS_Y0 + DH * 0.72, Math.cos(-a) * r, 0, -a, 0); }

  /* ================================================================ the legs */
  const ladder = [];
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2, sa = Math.sin(a), ca = Math.cos(a);
    const P = (rho, y, tau = 0) => V(sa * rho + ca * tau, y, ca * rho - sa * tau);      /* radial, up, across */
    const apex = P(2.95, 2.2), foot = P(FOOT_R, FOOT_Y), joint = apex.clone().lerp(foot, 0.6), knee = apex.clone().lerp(foot, 0.56);
    /* the outrigger truss: from the face's four corners to the apex; the stubs the secondary struts hang from */
    for (const s of [-1, 1]) {
      const up = P(A, 2.25, s * 0.8), low = P(A, 0.88, s * 0.8), stub = P(2.72, 0.82, s * 1.02);
      deployed.rod(gold, up, apex, 0.045); deployed.rod(gold, low, apex, 0.04);
      deployed.rod(gold, low, stub, 0.04); deployed.rod(gold, up, stub, 0.03);
      deployed.rod(gold, stub, joint, 0.048); deployed.ball(alu, stub, 0.06);
    }
    deployed.rod(gold, P(A, 2.25, -0.8), P(A, 2.25, 0.8), 0.03);
    /* the primary strut: the outer cylinder in gold foil, the piston below it, the pad on its ball joint */
    deployed.rod(gold, apex, knee, 0.095, 10); deployed.rod(alu, knee, foot, 0.068, 10);
    deployed.ball(alu, apex, 0.11); deployed.ball(gold, joint, 0.1); deployed.ball(alu, foot, 0.085);
    deployed.add(new THREE.CylinderGeometry(0.47, 0.4, 0.16, 24), gold, foot.x, foot.y - 0.08, foot.z);
    deployed.add(new THREE.CylinderGeometry(0.44, 0.47, 0.03, 24, 1, true), aluD, foot.x, foot.y + 0.012, foot.z);
    if (k !== 0) probes.rod(alu, P(FOOT_R + 0.3, FOOT_Y - 0.16), P(FOOT_R + 0.3, FOOT_Y - 1.86), 0.012, 6);
    /* stowed, as it rode in the adapter: the strut up against the stage, the pad under its corner */
    const fa = P(2.22, 2.2), fb = P(2.6, 0.55);
    folded.rod(gold, fa, fb, 0.09, 10); folded.add(new THREE.CylinderGeometry(0.45, 0.4, 0.14, 20), gold, fb.x, fb.y - 0.09, fb.z);
    for (const s of [-1, 1]) folded.rod(gold, P(A, 0.88, s * 0.8), fa.clone().lerp(fb, 0.6), 0.04);

    if (k === 0) {
      /* the ladder on the front leg: two rails along the strut, nine rungs; the last one 0.75 m above the pad */
      const along = foot.clone().sub(apex), len = along.length(); along.normalize();
      const out = V(0, 1, 0).addScaledVector(along, -along.y).normalize();               /* square to the strut, up and out */
      const at = (u, tau) => apex.clone().addScaledVector(along, u * len).addScaledVector(out, 0.13).add(V(tau, 0, 0));
      for (const s of [-1, 1]) deployed.rod(alu, at(0.0, s * 0.27), at(0.78, s * 0.27), 0.02);
      for (let n = 0; n < 9; n++) {
        const u = 0.06 + n * (0.76 - 0.06) / 8, c = at(u, 0);
        deployed.rod(alu, at(u, -0.27), at(u, 0.27), 0.016, 6); ladder.push(c);
        if (n % 4 === 0) for (const s of [-1, 1]) deployed.rod(alu, at(u, s * 0.27), apex.clone().addScaledVector(along, u * len).add(V(s * 0.07, 0, 0)), 0.012, 5);
      }
    }
  }
  /* the porch over the front outrigger, from the hatch's sill out to the head of the ladder; a handrail each side */
  { const pz = (PORCH.z0 + PORCH.z1) / 2, pl = PORCH.z1 - PORCH.z0;
    deployed.box(dark, PORCH.half * 2, 0.04, pl, 0, PORCH.y - 0.02, pz);
    for (let n = 0; n < 9; n++) deployed.box(alu, PORCH.half * 2, 0.012, 0.03, 0, PORCH.y + 0.006, PORCH.z0 + 0.12 + n * (pl - 0.2) / 8);   /* its ribs */
    for (const s of [-1, 1]) {
      deployed.box(alu, 0.03, 0.06, pl, s * PORCH.half, PORCH.y - 0.02, pz);
      deployed.tube(alu, [V(s * 0.47, 3.12, FRONT + 0.1), V(s * 0.47, 3.05, 1.9), V(s * 0.47, 2.92, 2.6), V(s * 0.4, 2.72, 3.0), V(s * 0.3, 2.42, 3.12)], 0.019);
      for (const z of [1.9, 2.6]) deployed.rod(alu, V(s * 0.47, z < 2 ? 3.05 : 2.92, z), V(s * PORCH.half, PORCH.y, z), 0.014, 6);
      deployed.rod(alu, V(s * PORCH.half, PORCH.y - 0.03, 2.2), V(s * 0.8, 2.25, A), 0.02, 6);                 /* braces down to the stage */
    }
    deployed.rod(alu, V(0, PORCH.y - 0.04, 2.9), V(0, 2.2, 2.95), 0.03, 6); }

  /* ================================================================ ascent stage */
  /* the cabin: a faceted drum, its axis fore and aft */
  { const sh = new THREE.Shape(); const n = 18;
    for (let i = 0; i < n; i++) { const t = (i + 0.5) / n * Math.PI * 2, x = Math.sin(t) * DRUM_R, y = DRUM_Y - Math.cos(t) * DRUM_R; if (i) sh.lineTo(x, y); else sh.moveTo(x, y); }
    sh.closePath();
    body.add(new THREE.ExtrudeGeometry(sh, { depth: FRONT - 0.1, bevelEnabled: false }), alu, 0, 0, 0.1); }
  /* the front: the hatch's housing at the foot of the drum, dark thermal panels either side of it, the brow */
  body.box(alu, 1.12, 0.98, 0.3, 0, AS_Y + 0.5, FRONT - 0.1);
  for (const s of [-1, 1]) body.box(blackFoil, 0.42, 0.7, 0.03, s * 0.8, AS_Y + 0.78, FRONT + 0.012, 0, 0, s * 0.12);
  body.box(alu, 1.5, 0.1, 0.1, 0, 4.36, FRONT + 0.02, 0.3);
  /* the two triangular windows (where the cabin's are), dark glass in black frames, canted panes */
  for (const s of [-1, 1]) {
    const pts = [[s * 0.12, 3.57], [s * 0.66, 3.57], [s * 0.12, 4.29]];
    const tri = new THREE.Shape(); tri.moveTo(pts[0][0], pts[0][1]); if (s > 0) { tri.lineTo(pts[1][0], pts[1][1]); tri.lineTo(pts[2][0], pts[2][1]); } else { tri.lineTo(pts[2][0], pts[2][1]); tri.lineTo(pts[1][0], pts[1][1]); }
    tri.closePath();
    body.add(new THREE.ShapeGeometry(tri), glass, 0, 0, FRONT + 0.012);
    for (let i = 0; i < 3; i++) { const p = pts[i], q = pts[(i + 1) % 3]; body.rod(dark, V(p[0], p[1], FRONT + 0.015), V(q[0], q[1], FRONT + 0.015), 0.022, 6); }
  }
  /* the midsection, the aft equipment bay with its rack and two gas tanks */
  body.box(blackFoil, 1.56, 1.9, 0.87, 0, AS_Y + 0.95, -0.33);
  body.box(alu, 1.2, 0.04, 0.8, 0, 4.31, -0.33);
  body.box(blackFoil, 1.9, 1.3, 0.75, 0, 3.4, -1.13);
  body.box(gold, 1.7, 1.1, 0.03, 0, 3.4, -1.52);
  for (let i = 0; i < 4; i++) body.box(amber, 0.34, 0.95, 0.02, -0.63 + i * 0.42, 3.4, -1.545);
  for (const s of [-1, 1]) body.add(new THREE.SphereGeometry(0.2, 12, 8), alu, s * 0.6, 4.18, -1.2);
  /* the propellant tanks in their shrouds: the Commander's side further out */
  for (const [x, r] of [[1.42, 0.64], [-1.26, 0.6]]) {
    body.add(new THREE.SphereGeometry(r, 10, 7), alu, x, 3.22, -0.3, 0, 0.3, 0, 1, 1.08, 1);
    body.box(blackFoil, Math.abs(x) - 0.5, 0.9, 0.9, x / 2 + Math.sign(x) * 0.25, 3.2, -0.3);
  }
  /* four thruster quads on their booms, four nozzles each; a plume deflector under each, braced to the deck */
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    const q = V(sx * 1.32, 3.98, -0.12 + sz * 1.3), root = V(sx * 0.8, 3.9, -0.12 + sz * 0.75);
    body.rod(alu, root, q, 0.05); body.rod(alu, V(root.x, 3.4, root.z), q, 0.03, 6);
    body.box(dark, 0.22, 0.22, 0.22, q.x, q.y, q.z);
    for (const [dx, dy, dz, rx, rz] of [[0, 1, 0, 0, 0], [0, -1, 0, Math.PI, 0], [sx, 0, 0, 0, -sx * Math.PI / 2], [0, 0, sz, sz * Math.PI / 2, 0]])
      body.add(new THREE.CylinderGeometry(0.035, 0.075, 0.17, 12, 1, true), aluD, q.x + dx * 0.19, q.y + dy * 0.19, q.z + dz * 0.19, rx, 0, rz);
    const dfl = V(sx * 1.5, 2.86, -0.12 + sz * 1.48);
    body.box(amber, 0.5, 0.025, 0.5, dfl.x, dfl.y, dfl.z, -sz * 0.5, 0, sx * 0.5);
    body.rod(alu, dfl, V(sx * 1.3, DS_TOP, -0.12 + sz * 1.15), 0.018, 6); body.rod(alu, dfl, V(sx * 1.75, DS_TOP, -0.12 + sz * 1.0), 0.018, 6);
  }
  /* the docking tunnel and drogue (the top stays at 4.75) */
  body.add(new THREE.CylinderGeometry(0.42, 0.45, 0.45, 24), alu, 0, 4.525, 0);
  body.add(new THREE.ConeGeometry(0.42, 0.18, 24, 1, true), darkD, 0, LM.TOP - 0.05, 0, Math.PI, 0, 0);
  body.add(new THREE.TorusGeometry(0.42, 0.018, 6, 24), alu, 0, LM.TOP, 0, Math.PI / 2);
  /* the rendezvous radar over the front; the S-band dish on its boom; two VHF aerials; the docking target; the small
   * docking window in the roof over the Commander */
  body.rod(alu, V(0, 4.62, 0.95), V(0, 4.95, 1.0), 0.04); body.box(dark, 0.2, 0.16, 0.16, 0, 4.98, 1.0);
  body.add(new THREE.SphereGeometry(0.34, 20, 8, 0, Math.PI * 2, 0, 0.85), aluD, 0, 5.02, 0.86, 1.95, 0, 0);
  body.rod(alu, V(-0.75, 4.3, -0.35), V(-1.5, 4.95, -0.35), 0.03); body.rod(alu, V(-1.5, 4.95, -0.35), V(-1.5, 5.12, -0.35), 0.025);
  body.add(new THREE.SphereGeometry(0.4, 20, 8, 0, Math.PI * 2, 0, 0.8), aluD, -1.5, 5.5, -0.35, Math.PI, 0, 0);
  body.rod(dark, V(-1.5, 5.15, -0.35), V(-1.5, 5.42, -0.35), 0.012, 5);
  for (const s of [-1, 1]) body.rod(alu, V(s * 0.55, 4.3, -0.6), V(s * 0.62, 5.15, -0.66), 0.01, 5);
  body.rod(alu, V(-0.5, 4.55, 0.55), V(-0.62, 4.85, 0.55), 0.012, 5); body.add(new THREE.CircleGeometry(0.09, 16), white, -0.62, 4.86, 0.55, -Math.PI / 2, 0, 0);
  { const x = 0.45, y = DRUM_Y + Math.sqrt(DRUM_R * DRUM_R - x * x) + 0.012, tilt = Math.asin(x / DRUM_R);
    body.add(new THREE.PlaneGeometry(0.18, 0.24), glass, x, y, 0.9, -Math.PI / 2, 0, 0); void tilt; }
  body.into(g);

  /* the outer hatch, its own mesh (it opens inwards for the egress): a square door with its frame and handle */
  const hatch = new THREE.Group();
  { const hp = new Parts(), hw = HATCH.half * 2, hh = HATCH.y1 - HATCH.y0, hc = (HATCH.y0 + HATCH.y1) / 2;
    hp.box(alu, hw, hh, 0.025, 0, hc, FRONT + 0.065);
    for (const [w, h, x, y] of [[hw + 0.08, 0.04, 0, HATCH.y1 + 0.02], [hw + 0.08, 0.04, 0, HATCH.y0 - 0.02], [0.04, hh, -HATCH.half - 0.02, hc], [0.04, hh, HATCH.half + 0.02, hc]]) body2(hp, dark, w, h, x, y);
    hp.box(dark, 0.16, 0.05, 0.05, 0.2, hc, FRONT + 0.1); hp.box(white, 0.3, 0.012, 0.004, 0, hc + 0.22, FRONT + 0.08);
    hp.into(hatch); }
  function body2(parts, mat, w, h, x, y) { parts.box(mat, w, h, 0.04, x, y, FRONT + 0.07); }
  g.add(hatch);

  const dep = deployed.into(new THREE.Group()), fol = folded.into(new THREE.Group()), prb = probes.into(new THREE.Group());
  g.add(dep, fol, prb); dep.visible = false; prb.visible = false;
  let legs = false, landed = false;
  const apply = () => { fol.visible = !legs; dep.visible = legs; prb.visible = legs && !landed; };
  g.userData.setLegs = on => { legs = on; apply(); };
  g.userData.setLanded = on => { landed = on; apply(); };
  g.userData.top = LM.TOP;
  g.userData.hatch = hatch; g.userData.porch = PORCH; g.userData.ladder = ladder;
  return g;
}
