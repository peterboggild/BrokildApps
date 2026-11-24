/* Geometry shared by the page and the ground worker: the site frame against lunar latitude and longitude, where a
 * level's mesh surface is, and seeded random numbers. Plain numbers (float64), no three.js. */

const D = Math.PI / 180;

export function makeGeo(man) {
  const la = man.site.lat * D, lo = man.site.lon * D, r0 = man.r0;
  const E = [-Math.sin(lo), Math.cos(lo), 0];
  const N = [-Math.sin(la) * Math.cos(lo), -Math.sin(la) * Math.sin(lo), Math.cos(la)];
  const U = [Math.cos(la) * Math.cos(lo), Math.cos(la) * Math.sin(lo), Math.sin(la)];

  /* site frame (x east, y up, z south, metres) -> lat, lon in degrees and the distance from the Moon's centre */
  function latlon(x, y, z, out) {
    const a = y + r0;
    const px = E[0] * x + U[0] * a - N[0] * z, py = E[1] * x + U[1] * a - N[1] * z, pz = E[2] * x + U[2] * a - N[2] * z;
    const r = Math.hypot(px, py, pz);
    out.lat = Math.asin(pz / r) / D; out.lon = Math.atan2(py, px) / D; out.r = r;
    return out;
  }
  /* position in a level's map: u east, v south, 0..1 */
  function levelUV(lv, ll, out) {
    out.u = (ll.lon - (lv.lon0 - lv.dlon)) / (2 * lv.dlon);
    out.v = ((lv.lat0 + lv.dlat) - ll.lat) / (2 * lv.dlat);
    return out;
  }
  /* Height (site y) of a level's MESH under a point, on the same two triangles per cell as it is drawn.
   * P: the level's vertex positions. Unwarped levels only. null outside the level. */
  function surfaceY(lv, P, uv) {
    const fi = uv.u * lv.nx, fj = uv.v * lv.ny;
    if (!(fi >= 0 && fi <= lv.nx && fj >= 0 && fj <= lv.ny)) return null;
    const i = Math.min(lv.nx - 1, Math.floor(fi)), j = Math.min(lv.ny - 1, Math.floor(fj)), fx = fi - i, fy = fj - j;
    const w = lv.nx + 1, a = (j * w + i) * 3 + 1;
    const A = P[a], C = P[a + 3], B = P[a + w * 3], Dd = P[a + w * 3 + 3];
    return fx + fy <= 1 ? A + fx * (C - A) + fy * (B - A) : Dd + (1 - fx) * (B - Dd) + (1 - fy) * (C - Dd);
  }
  return { latlon, levelUV, surfaceY, r0 };
}

/* Height of an axis-aligned grid mesh (rows along x, columns along z; spacing may vary) at (x, z), on the two
 * triangles per cell as drawn. null outside. */
export function gridHeight(P, nx, ny) {
  const xs = new Float64Array(nx + 1), zs = new Float64Array(ny + 1), mi = ny >> 1, mj = nx >> 1;
  for (let i = 0; i <= nx; i++) xs[i] = P[(mi * (nx + 1) + i) * 3];
  for (let j = 0; j <= ny; j++) zs[j] = P[(j * (nx + 1) + mj) * 3 + 2];
  const find = (a, v) => { let lo = 0, hi = a.length - 1; while (hi - lo > 1) { const m = (lo + hi) >> 1; if (a[m] <= v) lo = m; else hi = m; } return lo; };
  const y = (i, j) => P[(j * (nx + 1) + i) * 3 + 1];
  return (x, z) => {
    if (x < xs[0] || x > xs[nx] || z < zs[0] || z > zs[ny]) return null;
    const i = find(xs, x), j = find(zs, z);
    const fx = Math.min(1, Math.max(0, (x - xs[i]) / (xs[i + 1] - xs[i]))), fy = Math.min(1, Math.max(0, (z - zs[j]) / (zs[j + 1] - zs[j])));
    const A = y(i, j), B = y(i, j + 1), C = y(i + 1, j), Dd = y(i + 1, j + 1);
    return fx + fy <= 1 ? A + fx * (C - A) + fy * (B - A) : Dd + (1 - fx) * (B - Dd) + (1 - fy) * (C - Dd);
  };
}

/* a small seeded generator (mulberry32) and a hash of two integers to seed it with */
export function rng(seed) {
  let a = seed >>> 0;
  return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}
export function hash2(i, j, k = 0) {
  let h = Math.imul(i | 0, 374761393) ^ Math.imul(j | 0, 668265263) ^ Math.imul(k | 0, 1274126177);
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return (h ^ (h >>> 16)) >>> 0;
}

export const smooth = t => { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); };
