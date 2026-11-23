/* The planet's terrain: one height function on the unit sphere, in "units" (1 unit = 4.5 km of relief on Earth,
 * scaled by 1/g), shared with the GPU (scene/glsl.js, function terrainH). Here it is used for the hypsometry:
 * the distribution of heights, from which the sea level follows from the ocean volume, and the land fraction
 * from the sea level (spec §4.7). Pure functions; the same seed gives the same planet everywhere. */
import { snoise, GRAD } from './noise.js?v=8';

/* Seed -> offsets into noise space (passed to the GPU as uniforms, so both sides use the same numbers). */
export function seedOffsets(seed) {
  const r = (k) => { const x = Math.sin(seed * 12.9898 + k * 78.233) * 43758.5453; return (x - Math.floor(x)) * 200 - 100; };
  const v = (k) => [r(k), r(k + 1), r(k + 2)];
  return { a: v(1), b: v(4), c: v(7), d: v(10), e: v(13), f: v(16) };
}

const smoothstep = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };

function fbm(x, y, z, oct) {
  let sum = 0, amp = 0.5, f = 1;
  for (let i = 0; i < oct; i++) { sum += amp * snoise(x * f, y * f, z * f); f *= 2.03; amp *= 0.5; }
  return sum;
}
function ridged(x, y, z, oct) {
  let sum = 0, amp = 0.5, f = 1, w = 1;
  for (let i = 0; i < oct; i++) {
    let n = 1 - Math.abs(snoise(x * f, y * f, z * f));
    n *= n; n *= w;
    w = Math.min(1, Math.max(0, n * 2));
    sum += n * amp; f *= 2.1; amp *= 0.5;
  }
  return sum;
}
/* "Eroded" fbm: each octave is damped where the terrain built so far is steep (accumulated gradient), so
 * detail gathers in valleys and flats and slopes stay clean, in the manner of I. Quilez's derivative fbm. */
function erodedFbm(x, y, z, oct) {
  let sum = 0, amp = 0.5, f = 1, dx = 0, dy = 0, dz = 0;
  for (let i = 0; i < oct; i++) {
    const n = snoise(x * f, y * f, z * f);
    dx += GRAD[0] * amp; dy += GRAD[1] * amp; dz += GRAD[2] * amp;
    sum += amp * n / (1 + dx * dx + dy * dy + dz * dz);
    f *= 2.0; amp *= 0.5;
  }
  return sum;
}

/* Height in units at unit direction (nx, ny, nz). detailOct: octaves of the finest layer (GPU uses more). */
export function terrainH(nx, ny, nz, O, detailOct = 3) {
  /* continents: domain-warped fbm */
  const wx = nx * 0.9 + O.a[0], wy = ny * 0.9 + O.a[1], wz = nz * 0.9 + O.a[2];
  const qx = wx + 0.4 * snoise(wx * 1.6 + O.b[0], wy * 1.6 + O.b[1], wz * 1.6 + O.b[2]);
  const qy = wy + 0.4 * snoise(wx * 1.6 + O.b[0] + 17.1, wy * 1.6 + O.b[1], wz * 1.6 + O.b[2]);
  const qz = wz + 0.4 * snoise(wx * 1.6 + O.b[0], wy * 1.6 + O.b[1] + 31.7, wz * 1.6 + O.b[2]);
  const c = fbm(qx, qy, qz, 5);
  const land = smoothstep(-0.08, 0.18, c);
  let e = -1.0 + 1.1 * land + 0.45 * c;
  /* mountain belts: ridged multifractal inside a belt mask, mostly on land */
  const mm = smoothstep(0.25, 0.7, snoise(nx * 1.4 + O.c[0], ny * 1.4 + O.c[1], nz * 1.4 + O.c[2])) * smoothstep(0.0, 0.6, land);
  e += 2.0 * mm * ridged(nx * 4.2 + O.d[0], ny * 4.2 + O.d[1], nz * 4.2 + O.d[2], 6);
  /* mid-ocean ridges */
  const r2 = ridged(nx * 1.8 + O.e[0], ny * 1.8 + O.e[1], nz * 1.8 + O.e[2], 3);
  e += 0.3 * (1 - land) * r2 * r2;
  /* hills and valleys, eroded */
  e += 0.24 * (0.3 + land + mm) * erodedFbm(nx * 9 + O.f[0], ny * 9 + O.f[1], nz * 9 + O.f[2], detailOct);
  return e;
}

/* Relief scale: 4.5 km per unit on Earth. Weaker gravity supports taller mountains, but not in proportion:
 * Mars (0.38 g) has about 1.5 times Earth's full range of heights (about 30 km against 20 km), which a
 * (1/g)^0.45 scaling reproduces; capped at 2.5x. */
export function reliefMeters(gEarth) { return 4500 * Math.min(2.5, Math.max(gEarth, 0.1) ** -0.45); }

/* Hypsometry: heights at N equal-area points (Fibonacci sphere), sorted. Cached per seed. */
const cache = new Map();
export function hypsometry(seed, N = 12000) {
  const key = seed + ':' + N;
  if (cache.has(key)) return cache.get(key);
  const O = seedOffsets(seed);
  const h = new Float64Array(N);
  const ga = Math.PI * (3 - Math.sqrt(5));
  for (let i = 0; i < N; i++) {
    const y = 1 - 2 * (i + 0.5) / N, r = Math.sqrt(1 - y * y), th = ga * i;
    h[i] = terrainH(r * Math.cos(th), y, r * Math.sin(th), O, 3);
  }
  h.sort();
  if (cache.size > 8) cache.delete(cache.keys().next().value);
  cache.set(key, h);
  return h;
}

/* Sea level for an ocean volume (m^3) on a planet of radius R (m) with relief scale amp (m per unit).
 * Returns the sea level in units, the land fraction, the mean ocean depth (m), the highest point above the sea (m). */
export function seaLevel(seed, oceanVolume, R, amp) {
  const h = hypsometry(seed);
  const N = h.length;
  const cellArea = 4 * Math.PI * R * R / N;
  const volAt = (s) => { let v = 0; for (let i = 0; i < N && h[i] < s; i++) v += (s - h[i]); return v * amp * cellArea; };
  if (oceanVolume <= 0) return { level: h[0] - 0.01, landFrac: 1, meanDepth: 0, maxAbove: (h[N - 1] - h[0]) * amp, minH: h[0], maxH: h[N - 1] };
  let lo = h[0], hi = h[N - 1];
  if (volAt(hi) < oceanVolume) {
    /* everything is under water: the extra water is a uniform layer above the highest peak */
    const extra = (oceanVolume - volAt(hi)) / (4 * Math.PI * R * R) / amp;
    hi = h[N - 1] + extra;
    return { level: hi, landFrac: 0, meanDepth: oceanVolume / (4 * Math.PI * R * R), maxAbove: 0, minH: h[0], maxH: h[N - 1] };
  }
  for (let k = 0; k < 50; k++) { const m = (lo + hi) / 2; if (volAt(m) < oceanVolume) lo = m; else hi = m; }
  const level = (lo + hi) / 2;
  let under = 0;
  while (under < N && h[under] < level) under++;
  const landFrac = 1 - under / N;
  const meanDepth = under ? oceanVolume / (under * cellArea) : 0;
  return { level, landFrac, meanDepth, maxAbove: Math.max(0, (h[N - 1] - level) * amp), minH: h[0], maxH: h[N - 1] };
}
