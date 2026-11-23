/* 3D simplex noise, a line-by-line JavaScript port of the GLSL in scene/glsl.js (Ian McEwan / Stefan Gustavson,
 * "webgl-noise", MIT licence: github.com/stegu/webgl-noise). The two must stay identical: the planet's sea level and
 * land fraction are computed here from the same height function the GPU draws. Change one, change the other. */

const mod289 = (x) => x - Math.floor(x * (1 / 289)) * 289;
const permute = (x) => mod289(((x * 34) + 10) * x);
const taylorInvSqrt = (r) => 1.79284291400159 - 0.85373472095314 * r;

export function snoise(vx, vy, vz) {
  const Cx = 1 / 6, Cy = 1 / 3;
  /* first corner */
  const s = (vx + vy + vz) * Cy;
  let ix = Math.floor(vx + s), iy = Math.floor(vy + s), iz = Math.floor(vz + s);
  const t = (ix + iy + iz) * Cx;
  const x0x = vx - ix + t, x0y = vy - iy + t, x0z = vz - iz + t;
  /* other corners: g = step(x0.yzx, x0.xyz) */
  const gx = x0x >= x0y ? 1 : 0, gy = x0y >= x0z ? 1 : 0, gz = x0z >= x0x ? 1 : 0;
  const lx = 1 - gx, ly = 1 - gy, lz = 1 - gz;
  /* i1 = min(g.xyz, l.zxy), i2 = max(g.xyz, l.zxy) */
  const i1x = Math.min(gx, lz), i1y = Math.min(gy, lx), i1z = Math.min(gz, ly);
  const i2x = Math.max(gx, lz), i2y = Math.max(gy, lx), i2z = Math.max(gz, ly);
  const x1x = x0x - i1x + Cx, x1y = x0y - i1y + Cx, x1z = x0z - i1z + Cx;
  const x2x = x0x - i2x + Cy, x2y = x0y - i2y + Cy, x2z = x0z - i2z + Cy;
  const x3x = x0x - 0.5, x3y = x0y - 0.5, x3z = x0z - 0.5;
  /* permutations */
  ix = mod289(ix); iy = mod289(iy); iz = mod289(iz);
  const pz = [iz, iz + i1z, iz + i2z, iz + 1].map(permute);
  const py = [pz[0] + iy, pz[1] + iy + i1y, pz[2] + iy + i2y, pz[3] + iy + 1].map(permute);
  const p = [py[0] + ix, py[1] + ix + i1x, py[2] + ix + i2x, py[3] + ix + 1].map(permute);
  /* gradients: 7x7 points over a square, mapped onto an octahedron */
  /* 1/7 as the GPU sees it: in float32 the constant rounds slightly above 1/7, so floor(7 * n_) is 1. In float64 it
   * rounds below and floor gives 0, which picks gradients outside the 7x7 grid. Math.fround keeps the two identical. */
  const n_ = Math.fround(0.142857142857);
  const nsx = n_ * 2 - 0, nsy = n_ * 0.5 - 1, nsz = n_ * 1 - 0;   // ns = n_ * D.wyz - D.xzx
  const gxv = [], gyv = [], hv = [];
  for (let k = 0; k < 4; k++) {
    const j = p[k] - 49 * Math.floor(p[k] * nsz * nsz);
    const x_ = Math.floor(j * nsz);
    const y_ = Math.floor(j - 7 * x_);
    const x = x_ * nsx + nsy, y = y_ * nsx + nsy;
    gxv.push(x); gyv.push(y); hv.push(1 - Math.abs(x) - Math.abs(y));
  }
  /* b0 = (x.xy, y.xy), b1 = (x.zw, y.zw); s = floor(b)*2+1; sh = -step(h, 0) */
  const sh = hv.map((h) => (h <= 0 ? -1 : 0));
  const s0 = [Math.floor(gxv[0]) * 2 + 1, Math.floor(gxv[1]) * 2 + 1, Math.floor(gyv[0]) * 2 + 1, Math.floor(gyv[1]) * 2 + 1];
  const s1 = [Math.floor(gxv[2]) * 2 + 1, Math.floor(gxv[3]) * 2 + 1, Math.floor(gyv[2]) * 2 + 1, Math.floor(gyv[3]) * 2 + 1];
  /* a0 = b0.xzyw + s0.xzyw * sh.xxyy ; a1 = b1.xzyw + s1.xzyw * sh.zzww */
  const a0 = [gxv[0] + s0[0] * sh[0], gyv[0] + s0[2] * sh[0], gxv[1] + s0[1] * sh[1], gyv[1] + s0[3] * sh[1]];
  const a1 = [gxv[2] + s1[0] * sh[2], gyv[2] + s1[2] * sh[2], gxv[3] + s1[1] * sh[3], gyv[3] + s1[3] * sh[3]];
  const P = [[a0[0], a0[1], hv[0]], [a0[2], a0[3], hv[1]], [a1[0], a1[1], hv[2]], [a1[2], a1[3], hv[3]]];
  const X = [[x0x, x0y, x0z], [x1x, x1y, x1z], [x2x, x2y, x2z], [x3x, x3y, x3z]];
  let sum = 0, gx_ = 0, gy_ = 0, gz_ = 0;
  for (let k = 0; k < 4; k++) {
    const q = P[k];
    const norm = taylorInvSqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    const qx = q[0] * norm, qy = q[1] * norm, qz = q[2] * norm;
    const xx = X[k];
    const m = Math.max(0.5 - (xx[0] * xx[0] + xx[1] * xx[1] + xx[2] * xx[2]), 0);
    const m2 = m * m, m4 = m2 * m2;
    const pdotx = qx * xx[0] + qy * xx[1] + qz * xx[2];
    sum += m4 * pdotx;
    const temp = m2 * m * pdotx;
    gx_ += -8 * temp * xx[0] + m4 * qx;
    gy_ += -8 * temp * xx[1] + m4 * qy;
    gz_ += -8 * temp * xx[2] + m4 * qz;
  }
  GRAD[0] = 105 * gx_; GRAD[1] = 105 * gy_; GRAD[2] = 105 * gz_;
  return 105 * sum;
}
/* The gradient of the last snoise() call (avoids allocating). */
export const GRAD = new Float64Array(3);
