/* Bootprints: the Apollo overshoe's ribbed sole pressed into the regolith.
 *
 * One instanced mesh of small quads laid on the ground (one draw call for all of them). Each quad is shaded by the
 * terrain's own shader with the print's relief on top and the shadows its walls and ribs cast at the low Sun
 * (moon.printMaterial), so a print sits in the ground rather than on it. The relief is a height field made here
 * once: no image files. */
import * as THREE from 'three';

const W = 0.22, L = 0.40;                 /* the quad: metres across and along (the print plus its rim) */
const MAX = 1200;                         /* prints kept; the oldest is reused after that */

function printTexture() {
  const nx = 128, ny = 256, h = new Float32Array(nx * ny), a = new Float32Array(nx * ny);
  let seed = 11;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  /* a little low-frequency wobble so the outline and the depth are not machine-perfect */
  const wob = [rnd(), rnd(), rnd(), rnd()].map(v => v * 6.28);
  for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
    const x = ((i + 0.5) / nx - 0.5) * W, y = ((j + 0.5) / ny - 0.5) * L;     /* y towards the toe */
    const halfLen = 0.165, t = Math.min(1, Math.max(0, (y + halfLen) / (2 * halfLen)));
    const hw = 0.058 + 0.018 * Math.sin(Math.min(1, t * 1.25) * Math.PI * 0.5) - 0.006 * t * t;   /* heel narrower than the ball */
    const qx = Math.abs(x) / hw, qy = Math.abs(y + 0.004 * Math.sin(x * 60 + wob[0])) / halfLen;
    const r = Math.pow(Math.pow(qx, 4) + Math.pow(qy, 4), 0.25) * (1 + 0.025 * Math.sin(y * 45 + wob[1]) + 0.02 * Math.sin(x * 70 + wob[2]));
    const edge = (1 - r) * Math.min(hw, halfLen);                                 /* metres inside (+) or outside (-) */
    let z;
    if (edge > 0) {
      const depth = 0.0125 + 0.004 * (1 - t) + 0.0015 * Math.sin(x * 40 + y * 25 + wob[3]);   /* deeper at the heel */
      const wall = Math.min(1, edge / 0.007);
      /* the sole's transverse ribs leave soil standing between them */
      const s = ((y + 1) / 0.0128) % 1;
      const rib = Math.min(1, Math.max(0, (s - 0.18) / 0.12)) * Math.min(1, Math.max(0, (0.68 - s) / 0.12));
      const ribOn = Math.min(1, edge / 0.012);
      z = -depth * wall * wall * (3 - 2 * wall) + 0.0035 * rib * ribOn;
      a[j * nx + i] = 1;
    } else {
      const o = -edge;                                                             /* metres outside */
      z = 0.0045 * Math.exp(-Math.pow(o / 0.011, 2)) * (0.7 + 0.6 * rnd());        /* soil pushed up round the edge */
      const f = Math.min(1, o / 0.028);
      a[j * nx + i] = 1 - f * f * (3 - 2 * f);
    }
    h[j * nx + i] = z;
  }
  const d = new Uint8Array(nx * ny * 4), px = W / nx, py = L / ny;
  for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
    const hx = (h[j * nx + Math.min(nx - 1, i + 1)] - h[j * nx + Math.max(0, i - 1)]) / (2 * px);
    const hy = (h[Math.min(ny - 1, j + 1) * nx + i] - h[Math.max(0, j - 1) * nx + i]) / (2 * py);
    const n = Math.hypot(hx, hy, 1), k = (j * nx + i) * 4;
    d[k] = Math.round((-hx / n * 0.5 + 0.5) * 255);
    d[k + 1] = Math.round((-hy / n * 0.5 + 0.5) * 255);
    d[k + 2] = Math.round(Math.min(1, Math.max(0, (h[j * nx + i] + 0.025) / 0.037)) * 255);
    d[k + 3] = Math.round(a[j * nx + i] * 255);
  }
  const t = new THREE.DataTexture(d, nx, ny, THREE.RGBAFormat);
  t.generateMipmaps = true; t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter;
  t.wrapS = t.wrapT = THREE.ClampToEdgeWrapping; t.needsUpdate = true;
  return t;
}

export function makePrints(moon) {
  const geo = new THREE.PlaneGeometry(W, L);
  geo.rotateX(-Math.PI / 2);                       /* lies in x-z; the toe (uv.y = 1) towards -z */
  const mesh = new THREE.InstancedMesh(geo, moon.printMaterial(printTexture(), new THREE.Vector2(W, L)), MAX);
  mesh.count = 0; mesh.frustumCulled = false; mesh.renderOrder = 20;
  let next = 0;
  const m = new THREE.Matrix4(), up = new THREE.Vector3(), fw = new THREE.Vector3(), x = new THREE.Vector3(), z = new THREE.Vector3();
  /* one print at (px, pz), toe towards (dx, dz); side -1 left foot, +1 right foot */
  function add(px, pz, dx, dz, side) {
    const e = 0.15, h = moon.heightAt(px, pz);
    up.set(moon.heightAt(px - e, pz) - moon.heightAt(px + e, pz), 2 * e, moon.heightAt(px, pz - e) - moon.heightAt(px, pz + e)).normalize();
    fw.set(dx, 0, dz).normalize(); fw.addScaledVector(up, -fw.dot(up)).normalize();
    z.copy(fw).negate();
    x.crossVectors(up, z).multiplyScalar(side < 0 ? -1 : 1);   /* a left print is the right one mirrored */
    m.makeBasis(x, up, z).setPosition(px, h + 0.004, pz);
    mesh.setMatrixAt(next, m);
    next = (next + 1) % MAX;
    mesh.count = Math.max(mesh.count, next === 0 ? MAX : next);
    mesh.instanceMatrix.needsUpdate = true;
  }
  /* a walked line of prints from a to b (for the trail already there when you arrive) */
  function trail(ax, az, bx, bz, stride = 0.82) {
    const dx = bx - ax, dz = bz - az, d = Math.hypot(dx, dz), ux = dx / d, uz = dz / d;
    let side = 1;
    for (let s = 0.3; s < d; s += stride * (0.9 + 0.2 * Math.random())) {
      side = -side;
      const wob = (Math.random() - 0.5) * 0.06;
      add(ax + ux * s - uz * side * 0.11, az + uz * s + ux * side * 0.11, ux - uz * wob, uz + ux * wob, side);
    }
  }
  return { mesh, add, trail, get count() { return mesh.count; } };
}
